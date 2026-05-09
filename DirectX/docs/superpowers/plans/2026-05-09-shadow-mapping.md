# Shadow Mapping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sphere가 Quad(바닥)에 그림자를 드리우고 자기 자신에게도 그림자가 생기는 PCF Soft Shadow Mapping 구현

**Architecture:** Shadow Pass에서 빛 방향 직교 투영으로 깊이만 렌더링(Shadow_VS.hlsl, PS 없음)하고, Main Pass에서 SamplerComparisonState로 3×3 PCF 샘플링하여 그림자 계수를 Blinn-Phong 조명에 곱하는 2-Pass 구조. ImGui로 Bias/Kernel/Intensity 실시간 제어.

**Tech Stack:** DirectX 11, HLSL 5.0 (shader model 5), ImGui, ComPtr

---

## 파일 변경 목록

| 파일 | 변경 종류 | 역할 |
|------|---------|------|
| `DirectX/DirectX/D3DApp.h` | 수정 | cbShadow 구조체, cbPerObject 확장, 새 멤버 선언 |
| `DirectX/DirectX/D3DApp.cpp` | 수정 | BuildShadowResources(), BuildShaders() 확장, Update() 확장, Render() 분리 |
| `DirectX/DirectX/Shader/Shadow_VS.hlsl` | 신규 | Shadow Pass 전용 버텍스 셰이더 |
| `DirectX/DirectX/Shader/POM_PS.hlsl` | 수정 | shadow 리소스 선언, CalcShadowFactor 함수, lit 계산 수정 |

> **경로 기준:** git 리포지토리 루트 `D:\Git\A-KimYunSoo\DirectX`

---

## Task 1: D3DApp.h — cbPerObject 확장 및 Shadow 멤버 추가

**Files:**
- Modify: `DirectX/DirectX/D3DApp.h`

### 배경

`cbPerObject`에 shadow 파라미터를 추가한다. 기존 240 bytes → 320 bytes.
CPU 구조체와 HLSL 상수 버퍼는 레이아웃이 byte 단위로 일치해야 한다.
`cbShadow`는 Shadow VS 전용 별도 상수 버퍼 구조체다.

- [ ] **Step 1: cbShadow 구조체 추가 및 cbPerObject 확장**

`D3DApp.h`에서 기존 `cbPerObject` 구조체를 아래로 교체하고, 바로 위에 `cbShadow`를 추가한다.

```cpp
// Shadow VS 전용 상수 버퍼 (64 bytes)
struct cbShadow
{
    XMMATRIX lightViewProj;
};

// 16-byte aligned: 총 320 bytes
struct cbPerObject
{
    XMMATRIX world;           // 64  [0-63]
    XMMATRIX view;            // 64  [64-127]
    XMMATRIX proj;            // 64  [128-191]
    XMFLOAT3 cameraPos;       // 12  [192-203]
    float    heightScale;     //  4  [204-207]
    XMFLOAT3 lightDir;        // 12  [208-219]
    float    specPower;       //  4  [220-223]
    int      usePOM;          //  4  [224-227]
    int      pad[3];          // 12  [228-239]
    XMMATRIX lightViewProj;   // 64  [240-303]
    float    shadowBias;      //  4  [304-307]
    int      pcfKernel;       //  4  [308-311]
    float    shadowIntensity; //  4  [312-315]
    int      shadowPad;       //  4  [316-319]
};
```

- [ ] **Step 2: Shadow 전용 멤버 변수 추가**

`D3DApp` 클래스의 private 섹션에 아래를 추가한다.  
기존 `ComPtr<ID3D11Buffer> mCBuf;` 바로 아래에 삽입한다.

```cpp
// Shadow map GPU 리소스
ComPtr<ID3D11Texture2D>          mShadowTex;
ComPtr<ID3D11DepthStencilView>   mShadowDSV;
ComPtr<ID3D11ShaderResourceView> mShadowSRV;
ComPtr<ID3D11SamplerState>       mShadowSampler;
ComPtr<ID3D11VertexShader>       mShadowVS;
ComPtr<ID3D11InputLayout>        mShadowLayout;
ComPtr<ID3D11Buffer>             mShadowCB;
ComPtr<ID3D11RasterizerState>    mShadowRS;

// Shadow 파라미터 (ImGui 제어)
float    mShadowBias      = 0.002f;
int      mPCFKernel       = 3;
float    mShadowIntensity = 0.75f;
XMMATRIX mLightViewProj   = XMMatrixIdentity();
```

- [ ] **Step 3: BuildShadowResources 선언 추가**

`D3DApp` 클래스의 private 함수 선언 목록에 추가한다.  
`void BuildConstantBuffer();` 바로 아래에 삽입한다.

```cpp
void BuildShadowResources();
void RenderShadowPass();
void RenderMainPass();
```

- [ ] **Step 4: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공 (cbPerObject 크기 변경으로 링크 에러 없어야 함).

- [ ] **Step 5: 커밋**

```bash
git add DirectX/DirectX/D3DApp.h
git commit -m "refactor: cbPerObject에 shadow 필드 추가 및 shadow 멤버 선언"
```

---

## Task 2: BuildShadowResources() 구현

**Files:**
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

Shadow map은 TYPELESS 포맷으로 텍스처를 만들고,
DSV는 D32_FLOAT, SRV는 R32_FLOAT로 각각 다르게 해석한다.
Comparison Sampler는 하드웨어 PCF를 위해 반드시 `COMPARISON_LESS_EQUAL` 사용.
Shadow RS는 `SlopeScaledDepthBias`로 곡면(구)의 self-shadow acne를 처리한다.

- [ ] **Step 1: BuildShadowResources() 구현 추가**

`D3DApp.cpp`에서 `BuildConstantBuffer()` 함수 정의 바로 아래에 추가한다.

```cpp
void D3DApp::BuildShadowResources()
{
    // 1. Shadow map 텍스처 (1024×1024, TYPELESS)
    D3D11_TEXTURE2D_DESC td = {};
    td.Width          = 1024;
    td.Height         = 1024;
    td.MipLevels      = 1;
    td.ArraySize      = 1;
    td.Format         = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc.Count = 1;
    td.Usage          = D3D11_USAGE_DEFAULT;
    td.BindFlags      = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    mDevice->CreateTexture2D(&td, nullptr, mShadowTex.GetAddressOf());

    // 2. DSV: D32_FLOAT
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format             = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    mDevice->CreateDepthStencilView(mShadowTex.Get(), &dsvDesc, mShadowDSV.GetAddressOf());

    // 3. SRV: R32_FLOAT (PS에서 읽기용)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    mDevice->CreateShaderResourceView(mShadowTex.Get(), &srvDesc, mShadowSRV.GetAddressOf());

    // 4. Comparison Sampler (하드웨어 PCF)
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.BorderColor[0] = 1.0f;
    sd.BorderColor[1] = 1.0f;
    sd.BorderColor[2] = 1.0f;
    sd.BorderColor[3] = 1.0f;
    sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    mDevice->CreateSamplerState(&sd, mShadowSampler.GetAddressOf());

    // 5. Shadow RS: SlopeScaledDepthBias로 self-shadow acne 처리
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode             = D3D11_FILL_SOLID;
    rd.CullMode             = D3D11_CULL_BACK;
    rd.DepthBias            = 100;
    rd.SlopeScaledDepthBias = 2.0f;
    rd.DepthBiasClamp       = 0.01f;
    rd.DepthClipEnable      = TRUE;
    mDevice->CreateRasterizerState(&rd, mShadowRS.GetAddressOf());

    // 6. Shadow CB (Shadow VS용, 64 bytes)
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = sizeof(cbShadow);
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mDevice->CreateBuffer(&cbd, nullptr, mShadowCB.GetAddressOf());
}
```

- [ ] **Step 2: Init()에서 BuildShadowResources() 호출**

`Init()`에서 `BuildConstantBuffer();` 바로 아래에 추가한다.

```cpp
BuildShadowResources();
```

- [ ] **Step 3: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 4: 커밋**

```bash
git add DirectX/DirectX/D3DApp.cpp
git commit -m "feat: BuildShadowResources() - shadow map 텍스처/DSV/SRV/샘플러/RS/CB 생성"
```

---

## Task 3: Shadow_VS.hlsl 작성 및 BuildShaders() 확장

**Files:**
- Create: `DirectX/DirectX/Shader/Shadow_VS.hlsl`
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

Shadow VS는 깊이만 기록하면 되므로 위치 변환만 한다.
PS는 없음 — D3D11은 DSV만 바인딩되어 있으면 깊이를 자동 기록한다.
Shadow VS는 기존 POSITION 시멘틱만 읽으므로, 별도 InputLayout이 필요하다.
(기존 layout은 5개 원소를 서명으로 갖고 있어 Shadow VS blob과 맞지 않음)

- [ ] **Step 1: Shadow_VS.hlsl 작성**

`DirectX/DirectX/Shader/Shadow_VS.hlsl` 파일을 새로 만든다.

```hlsl
cbuffer cbShadow : register(b0)
{
    float4x4 gLightViewProj;
};

float4 main(float3 posL : POSITION) : SV_Position
{
    return mul(float4(posL, 1.0f), gLightViewProj);
}
```

- [ ] **Step 2: BuildShaders()에 Shadow VS 컴파일 추가**

`D3DApp.cpp`의 `BuildShaders()` 함수 끝(기존 `mDevice->CreateInputLayout(...)` 이후)에 추가한다.

```cpp
    // Shadow VS 컴파일
    ComPtr<ID3DBlob> shadowVsBlob;
    hr = D3DCompileFromFile(L"Shader/Shadow_VS.hlsl", nullptr, nullptr,
        "main", "vs_5_0", flags, 0, shadowVsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return;
    }
    mDevice->CreateVertexShader(shadowVsBlob->GetBufferPointer(), shadowVsBlob->GetBufferSize(),
        nullptr, mShadowVS.GetAddressOf());

    // Shadow 전용 InputLayout (POSITION만)
    D3D11_INPUT_ELEMENT_DESC shadowLayoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    mDevice->CreateInputLayout(shadowLayoutDesc, 1,
        shadowVsBlob->GetBufferPointer(), shadowVsBlob->GetBufferSize(),
        mShadowLayout.GetAddressOf());
```

- [ ] **Step 3: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 4: 커밋**

```bash
git add DirectX/DirectX/Shader/Shadow_VS.hlsl DirectX/DirectX/D3DApp.cpp
git commit -m "feat: Shadow_VS.hlsl 작성 및 BuildShaders()에 Shadow VS/Layout 컴파일 추가"
```

---

## Task 4: Update() — Light Camera 계산 및 Shadow CB 업데이트

**Files:**
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

디렉셔널 라이트는 위치가 없지만 Shadow Pass는 카메라가 필요하다.
빛 방향 반대편에 직교 카메라를 배치하고 씬 중심(0,0,0)을 바라보게 한다.
`up` 벡터가 `lightDir`와 평행이면 LookAt이 NaN을 반환하므로 예외처리 필수.
`mShadowCB`는 Shadow VS에서 쓰는 별도 버퍼다.
`mCBuf`(cbPerObject)에도 shadow params를 추가로 채운다.

- [ ] **Step 1: Update()에서 lightDir 계산을 Map 블록 바깥으로 이동하고 Shadow CB 업데이트 추가**

`D3DApp.cpp`의 `Update()` 전체를 아래로 교체한다.

```cpp
void D3DApp::Update(float dt)
{
    mRotation += mRotSpeed * dt;
    if (!mLightPaused)
        mLightAngle += dt;

    // Light direction (기존과 동일)
    XMVECTOR ld = XMVector3Normalize(XMVectorSet(1.0f, sinf(mLightAngle), -1.0f, 0.0f));

    // Light camera 계산
    XMVECTOR lightPos = XMVectorScale(ld, -20.0f);
    XMVECTOR up       = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    if (fabsf(XMVectorGetY(ld)) > 0.99f)
        up = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, XMVectorZero(), up);
    XMMATRIX lightProj = XMMatrixOrthographicLH(12.0f, 12.0f, 0.1f, 50.0f);
    mLightViewProj = lightView * lightProj;

    // Shadow CB 업데이트 (Shadow VS용)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        mCtx->Map(mShadowCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* cb = reinterpret_cast<cbShadow*>(mapped.pData);
        cb->lightViewProj = XMMatrixTranspose(mLightViewProj);
        mCtx->Unmap(mShadowCB.Get(), 0);
    }

    // Main CB 업데이트 (기존 + shadow params)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        mCtx->Map(mCBuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        auto* cb = reinterpret_cast<cbPerObject*>(mapped.pData);

        cb->world       = XMMatrixTranspose(XMMatrixRotationY(mRotation));
        cb->view        = XMMatrixTranspose(mCamera.GetView());
        cb->proj        = XMMatrixTranspose(mCamera.GetProj((float)mWidth / mHeight));
        cb->cameraPos   = mCamera.position;
        cb->heightScale = mHeightScale;
        cb->specPower   = mSpecPower;
        cb->usePOM      = mUsePOM ? 1 : 0;
        XMStoreFloat3(&cb->lightDir, ld);

        cb->lightViewProj   = XMMatrixTranspose(mLightViewProj);
        cb->shadowBias      = mShadowBias;
        cb->pcfKernel       = mPCFKernel;
        cb->shadowIntensity = mShadowIntensity;
        cb->shadowPad       = 0;

        mCtx->Unmap(mCBuf.Get(), 0);
    }
}
```

- [ ] **Step 2: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 3: 커밋**

```bash
git add DirectX/DirectX/D3DApp.cpp
git commit -m "feat: Update()에 light camera 계산 및 shadow CB 업데이트 추가"
```

---

## Task 5: RenderShadowPass() 구현

**Files:**
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

Shadow Pass는 RTV 없이 DSV만 바인딩한다.
PS를 nullptr로 설정해 픽셀 처리를 건너뛴다 (깊이만 기록).
mShadowSRV를 unbind하고 나서 DSV로 써야 D3D 리소스 hazard가 없다.
Shadow Pass에서는 mShadowLayout + mShadowVS를 사용한다.
Sphere와 Quad 모두 그림자를 드리워야 하므로 mMeshMode 관계없이 둘 다 Draw한다.

- [ ] **Step 1: RenderShadowPass() 추가**

`D3DApp.cpp`에서 `Update()` 함수 바로 아래에 추가한다.

```cpp
void D3DApp::RenderShadowPass()
{
    // SRV → DSV로 전환 전에 SRV 언바인드 (resource hazard 방지)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    mCtx->PSSetShaderResources(3, 1, &nullSRV);

    // RTV 없음, Shadow DSV 바인딩
    mCtx->OMSetRenderTargets(0, nullptr, mShadowDSV.Get());
    mCtx->ClearDepthStencilView(mShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Shadow 전용 뷰포트 (1024×1024)
    D3D11_VIEWPORT vp = {};
    vp.Width    = 1024.0f;
    vp.Height   = 1024.0f;
    vp.MaxDepth = 1.0f;
    mCtx->RSSetViewports(1, &vp);

    // Shadow VS + Layout
    mCtx->IASetInputLayout(mShadowLayout.Get());
    mCtx->VSSetShader(mShadowVS.Get(), nullptr, 0);
    mCtx->PSSetShader(nullptr, nullptr, 0);

    // Shadow RS (depth bias)
    mCtx->RSSetState(mShadowRS.Get());

    // Shadow CB → VS b0
    ID3D11Buffer* scb = mShadowCB.Get();
    mCtx->VSSetConstantBuffers(0, 1, &scb);

    // Sphere, Quad 모두 그림자 드리움 (mMeshMode 무관)
    mSphere.Draw(mCtx.Get());
    mQuad.Draw(mCtx.Get());
}
```

- [ ] **Step 2: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 3: 커밋**

```bash
git add DirectX/DirectX/D3DApp.cpp
git commit -m "feat: RenderShadowPass() 구현 - 깊이 전용 패스"
```

---

## Task 6: POM_PS.hlsl — shadow 리소스 선언 및 CalcShadowFactor 추가

**Files:**
- Modify: `DirectX/DirectX/Shader/POM_PS.hlsl`

### 배경

`cbPerObject`에 shadow 필드를 추가한다. CPU 구조체와 byte 레이아웃이 일치해야 한다.
`SamplerComparisonState` + `SampleCmpLevelZero`로 하드웨어 PCF를 사용한다.
HLSL에서 `half`는 예약 키워드(16-bit float)이므로 변수명으로 사용 불가.
`CalcShadowFactor`는 0.0(그림자) ~ 1.0(빛)을 반환하고, lit에 곱한다.

- [ ] **Step 1: cbPerObject에 shadow 필드 추가**

`POM_PS.hlsl`의 `cbPerObject` 블록을 아래로 교체한다.

```hlsl
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float3   gCameraPos;
    float    gHeightScale;
    float3   gLightDir;
    float    gSpecPower;
    int      gUsePOM;
    int      pad[3];
    float4x4 gLightViewProj;
    float    gShadowBias;
    int      gPCFKernel;
    float    gShadowIntensity;
    int      gShadowPad;
};
```

- [ ] **Step 2: shadow 리소스 선언 추가**

기존 `SamplerState gSampler : register(s0);` 바로 아래에 추가한다.

```hlsl
Texture2D              gShadowMap  : register(t3);
SamplerComparisonState gShadowSamp : register(s1);
```

- [ ] **Step 3: CalcShadowFactor 함수 추가**

`ParallaxOcclusionMapping` 함수 정의 바로 위에 추가한다.

```hlsl
float CalcShadowFactor(float3 posW)
{
    float4 posL = mul(float4(posW, 1.0f), gLightViewProj);
    posL.xyz   /= posL.w;

    float2 uv = float2(posL.x * 0.5f + 0.5f, -posL.y * 0.5f + 0.5f);

    // 빛 프러스텀 밖은 그림자 없음
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float  depth      = posL.z - gShadowBias;
    float  shadow     = 0.0f;
    float  texelSize  = 1.0f / 1024.0f;
    int    radius     = gPCFKernel / 2;
    float  total      = (float)(gPCFKernel * gPCFKernel);

    for (int y = -radius; y <= radius; ++y)
    for (int x = -radius; x <= radius; ++x)
        shadow += gShadowMap.SampleCmpLevelZero(
            gShadowSamp, uv + float2(x, y) * texelSize, depth);

    return lerp(1.0f - gShadowIntensity, 1.0f, shadow / total);
}
```

- [ ] **Step 4: PS()에서 shadowFactor를 lit에 적용**

`POM_PS.hlsl`의 `PS()` 함수 끝부분을 아래로 교체한다.

변경 전:
```hlsl
    float3 lit = diffuse.rgb * (NdotL * 0.85 + 0.15)
               + float3(1.0, 1.0, 1.0) * spec * 0.6 * NdotL;

    return float4(lit, diffuse.a);
```

변경 후:
```hlsl
    float shadowFactor = CalcShadowFactor(input.posWS);

    float3 lit = (diffuse.rgb * (NdotL * 0.85 + 0.15)
               + float3(1.0, 1.0, 1.0) * spec * 0.6 * NdotL) * shadowFactor;

    return float4(lit, diffuse.a);
```

- [ ] **Step 5: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 6: 커밋**

```bash
git add DirectX/DirectX/Shader/POM_PS.hlsl
git commit -m "feat: POM_PS.hlsl에 CalcShadowFactor PCF 함수 및 shadow 리소스 추가"
```

---

## Task 7: RenderMainPass() 구현 및 Render() 재구성

**Files:**
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

기존 `Render()` 내용을 `RenderMainPass()`로 이동하고, Shadow SRV/Sampler 바인딩을 추가한다.
렌더 후 SRV t3 슬롯을 nullptr로 unbind해야 다음 프레임 Shadow Pass가 DSV로 쓸 수 있다.
`Render()`는 `RenderShadowPass()` → `RenderMainPass()` 순으로만 호출한다.
뷰포트와 RTV/DSV를 반드시 복구해야 한다 (Shadow Pass가 바꿨으므로).

- [ ] **Step 1: RenderMainPass() 추가**

`RenderShadowPass()` 바로 아래에 추가한다.

```cpp
void D3DApp::RenderMainPass()
{
    // 뷰포트 복구 (메인 렌더 해상도)
    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)mWidth;
    vp.Height   = (float)mHeight;
    vp.MaxDepth = 1.0f;
    mCtx->RSSetViewports(1, &vp);

    // RTV + DSV 복구
    ID3D11RenderTargetView* rtvRaw = mRTV.Get();
    mCtx->OMSetRenderTargets(1, &rtvRaw, mDSV.Get());

    const float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    mCtx->ClearRenderTargetView(mRTV.Get(), clearColor);
    mCtx->ClearDepthStencilView(mDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 메인 셰이더 + 상태
    mCtx->IASetInputLayout(mLayout.Get());
    mCtx->VSSetShader(mVS.Get(), nullptr, 0);
    mCtx->PSSetShader(mPS.Get(), nullptr, 0);
    mCtx->RSSetState(mRSState.Get());
    mCtx->OMSetDepthStencilState(mDSState.Get(), 0);

    // CB 바인딩
    ID3D11Buffer* cb = mCBuf.Get();
    mCtx->VSSetConstantBuffers(0, 1, &cb);
    mCtx->PSSetConstantBuffers(0, 1, &cb);

    // 텍스처 SRV (t0~t2: diffuse/height/normal, t3: shadow map)
    ID3D11ShaderResourceView* srvs[4] = {
        mDiffuseSRV.Get(), mHeightSRV.Get(), mNormalSRV.Get(), mShadowSRV.Get() };
    mCtx->PSSetShaderResources(0, 4, srvs);

    // 샘플러 (s0: 일반, s1: comparison)
    ID3D11SamplerState* samplers[2] = { mSampler.Get(), mShadowSampler.Get() };
    mCtx->PSSetSamplers(0, 2, samplers);

    // 메시 드로우
    if (mMeshMode == 0)
        mSphere.Draw(mCtx.Get());
    else
        mQuad.Draw(mCtx.Get());

    // Shadow SRV unbind (다음 프레임 Shadow Pass에서 DSV로 사용하기 위해)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    mCtx->PSSetShaderResources(3, 1, &nullSRV);

    // ImGui
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("POM Controls");
    const char* meshItems[] = { "Sphere", "Quad" };
    ImGui::Combo("Mesh", &mMeshMode, meshItems, 2);
    ImGui::Separator();
    ImGui::Checkbox("Enable POM", &mUsePOM);
    ImGui::SliderFloat("Height Scale", &mHeightScale, 0.01f, 0.2f);
    ImGui::SliderFloat("Rotation Speed", &mRotSpeed, 0.0f, 5.0f);
    ImGui::Separator();
    ImGui::SliderFloat("Specular Power", &mSpecPower, 1.0f, 256.0f);
    ImGui::Separator();
    if (mLightPaused) {
        if (ImGui::Button("Resume Light")) mLightPaused = false;
    } else {
        if (ImGui::Button("Stop Light"))   mLightPaused = true;
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    mSwapChain->Present(1, 0);
}
```

- [ ] **Step 2: Render()를 2-Pass 구조로 교체**

기존 `Render()` 전체를 아래로 교체한다.

```cpp
void D3DApp::Render()
{
    RenderShadowPass();
    RenderMainPass();
}
```

- [ ] **Step 3: 빌드 확인**

Visual Studio에서 Ctrl+Shift+B.  
Expected: 에러 없이 빌드 성공.

- [ ] **Step 4: 실행하여 그림자 확인**

F5로 실행.  
Expected:
- 구가 바닥에 그림자를 드리움
- 구 표면에 자기 그림자 발생
- D3D Debug 출력(Output 창)에 리소스 hazard 경고 없음

- [ ] **Step 5: 커밋**

```bash
git add DirectX/DirectX/D3DApp.cpp
git commit -m "feat: 2-Pass 렌더링 구조 완성 - RenderShadowPass/RenderMainPass 분리"
```

---

## Task 8: ImGui Shadow 컨트롤 추가

**Files:**
- Modify: `DirectX/DirectX/D3DApp.cpp`

### 배경

Bias가 너무 작으면 shadow acne(줄무늬), 너무 크면 peter panning(떠있는 그림자).
슬라이더로 직접 조절하면 각 파라미터의 효과를 체감으로 이해할 수 있다.
PCF Kernel은 라디오버튼으로 1(하드)/3(소프트)/5(더 소프트) 선택.

- [ ] **Step 1: ImGui Shadow 섹션 추가**

`RenderMainPass()`의 ImGui 블록에서 기존 Light 버튼 `ImGui::End();` 바로 앞에 추가한다.

```cpp
    ImGui::Separator();
    ImGui::Text("Shadow");
    ImGui::SliderFloat("Shadow Bias", &mShadowBias, 0.0001f, 0.01f, "%.4f");
    ImGui::SliderFloat("Shadow Intensity", &mShadowIntensity, 0.0f, 1.0f);
    ImGui::Text("PCF Kernel");
    ImGui::SameLine();
    if (ImGui::RadioButton("1x1", mPCFKernel == 1)) mPCFKernel = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("3x3", mPCFKernel == 3)) mPCFKernel = 3;
    ImGui::SameLine();
    if (ImGui::RadioButton("5x5", mPCFKernel == 5)) mPCFKernel = 5;
```

- [ ] **Step 2: 실행하여 ImGui 컨트롤 동작 확인**

F5로 실행.  
Expected:
- Shadow Bias 슬라이더: 낮추면 acne 발생, 높이면 그림자가 물체에서 떨어짐
- Shadow Intensity 슬라이더: 0이면 그림자 없음, 1이면 완전 검정
- PCF Kernel 라디오버튼: 1x1은 딱딱한 가장자리, 5x5는 부드러운 가장자리

- [ ] **Step 3: 커밋**

```bash
git add DirectX/DirectX/D3DApp.cpp
git commit -m "feat: ImGui에 Shadow Bias/Intensity/PCF Kernel 실시간 제어 추가"
```

---

## 완료 기준

- [ ] Sphere가 Quad(바닥)에 그림자를 드리움
- [ ] Sphere 표면에 self-shadow 발생 (acne 없음)
- [ ] PCF로 그림자 가장자리가 부드러움
- [ ] D3D Debug 출력에 리소스 hazard/오류 없음
- [ ] ImGui로 Bias/Intensity/Kernel 실시간 조절 가능
- [ ] 빛 애니메이션에 따라 그림자가 함께 움직임
