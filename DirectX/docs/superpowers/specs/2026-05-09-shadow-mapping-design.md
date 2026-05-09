# Shadow Mapping 설계 문서

**날짜:** 2026-05-09  
**기능:** PCF Soft Shadow Mapping  
**대상 프로젝트:** DirectX 11 렌더링 학습 프로젝트

---

## 목표

- Sphere가 Quad(바닥)에 그림자를 드리움
- Sphere 표면에 자기 자신의 그림자(Self-shadow) 발생
- PCF(Percentage Closer Filtering)로 가장자리 부드럽게 처리
- ImGui로 Shadow Bias / PCF Kernel / Shadow Intensity 실시간 조절

---

## 아키텍처: 2-Pass 렌더링

### 매 프레임 렌더 순서

```
① Shadow Pass
   - 뷰포트: 1024×1024 (shadow map 해상도)
   - 투영 방식: 빛 방향 기준 직교 투영 (Orthographic)
   - 렌더 타겟: 없음 (Depth buffer만)
   - 출력: mShadowTex (깊이 텍스처)

② Main Pass (기존)
   - mShadowSRV를 픽셀 셰이더 t3 슬롯에 바인딩
   - 각 픽셀을 빛 공간으로 변환 후 PCF 샘플링
   - 그림자 계수를 최종 조명 값에 곱함
```

---

## 셰이더 설계

### Shadow_VS.hlsl (신규)

Shadow Pass는 깊이만 기록하면 되므로 최소 구성.  
Pixel Shader 없음 — DSV만 있으면 D3D11이 자동 기록.

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

### POM_PS.hlsl 수정 사항

#### cbPerObject에 추가 (16바이트 정렬 유지)

```hlsl
float4x4 gLightViewProj;   // 64 bytes
float    gShadowBias;      //  4 bytes
int      gPCFKernel;       //  4 bytes  (1 / 3 / 5)
float    gShadowIntensity; //  4 bytes
int      gShadowPad;       //  4 bytes  (패딩)
```

#### 새 리소스 바인딩

```hlsl
Texture2D              gShadowMap  : register(t3);
SamplerComparisonState gShadowSamp : register(s1);
```

`SamplerComparisonState`를 쓰는 이유: GPU가 비교와 bilinear 보간을 한 번에 처리.  
수동 샘플링+비교보다 빠르고, 1×1 커널에서도 bilinear PCF 효과가 기본 제공.

#### CalcShadowFactor 함수

```hlsl
float CalcShadowFactor(float3 posW)
{
    float4 posL = mul(float4(posW, 1.0f), gLightViewProj);
    posL.xyz /= posL.w;

    // NDC → UV (D3D는 Y축 반전)
    float2 uv = float2(posL.x * 0.5f + 0.5f, -posL.y * 0.5f + 0.5f);

    // 빛 프러스텀 밖은 그림자 없음으로 처리
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float depth      = posL.z - gShadowBias;
    float shadow     = 0.0f;
    float texelSize  = 1.0f / 1024.0f;
    int   radius     = gPCFKernel / 2;  // 'half'는 HLSL 예약 키워드이므로 사용 불가
    float total      = (float)(gPCFKernel * gPCFKernel);

    for (int y = -radius; y <= radius; ++y)
    for (int x = -radius; x <= radius; ++x)
        shadow += gShadowMap.SampleCmpLevelZero(
            gShadowSamp, uv + float2(x, y) * texelSize, depth);

    // shadowIntensity: 1.0 = 완전 검정, 0.0 = 그림자 없음
    return lerp(1.0f - gShadowIntensity, 1.0f, shadow / total);
}
```

`CalcShadowFactor` 반환값을 최종 조명 색상에 곱함:
```hlsl
float shadowFactor = CalcShadowFactor(input.posW);
float3 lit = (diffuse + specular) * shadowFactor;
```

---

## CPU 리소스 설계 (D3DApp)

### D3DApp.h 추가 멤버

```cpp
// Shadow map GPU 리소스
ComPtr<ID3D11Texture2D>          mShadowTex;
ComPtr<ID3D11DepthStencilView>   mShadowDSV;
ComPtr<ID3D11ShaderResourceView> mShadowSRV;
ComPtr<ID3D11SamplerState>       mShadowSampler;  // Comparison sampler
ComPtr<ID3D11VertexShader>       mShadowVS;
ComPtr<ID3D11Buffer>             mShadowCB;       // cbShadow (shadow pass용)
ComPtr<ID3D11RasterizerState>    mShadowRS;       // 하드웨어 depth bias 적용

// ImGui 제어 파라미터
float mShadowBias      = 0.002f;
int   mPCFKernel       = 3;      // 1 / 3 / 5
float mShadowIntensity = 0.75f;
XMMATRIX mLightViewProj;
```

### cbPerObject 구조체 수정

```cpp
// 기존 240 bytes + shadow 추가 = 304 bytes (16바이트 정렬 유지)
struct cbPerObject
{
    XMMATRIX world;           // 64
    XMMATRIX view;            // 64
    XMMATRIX proj;            // 64
    XMFLOAT3 cameraPos;       // 12
    float    heightScale;     //  4
    XMFLOAT3 lightDir;        // 12
    float    specPower;        //  4
    int      usePOM;          //  4
    int      pad[3];          // 12  → 여기까지 기존 240 bytes
    XMMATRIX lightViewProj;   // 64
    float    shadowBias;      //  4
    int      pcfKernel;       //  4
    float    shadowIntensity; //  4
    int      shadowPad;       //  4  → 총 320 bytes
};
```

---

## Light Camera 설계

### 직교 투영 행렬 계산

디렉셔널 라이트는 위치가 없지만 Shadow Pass에서는 카메라가 필요.  
씬 중심을 바라보는 직교 카메라를 빛 방향 반대편에 배치.

```cpp
XMVECTOR lightDir = XMLoadFloat3(&lightDirNormalized);
XMVECTOR lightPos = -20.0f * lightDir;   // 씬 중심에서 빛 방향 반대로
XMVECTOR up       = XMVectorSet(0, 1, 0, 0);

// 빛이 수직으로 내려올 때 up 벡터 충돌 방지
if (fabsf(XMVectorGetY(XMVector3Normalize(lightDir))) > 0.99f)
    up = XMVectorSet(1, 0, 0, 0);

XMMATRIX lightView = XMMatrixLookAtLH(lightPos, XMVectorZero(), up);
XMMATRIX lightProj = XMMatrixOrthographicLH(12.0f, 12.0f, 0.1f, 50.0f);
mLightViewProj = lightView * lightProj;
```

Orthographic 범위(12×12)는 Sphere(반지름 1) + Quad(10×10)를 충분히 감쌈.  
범위가 너무 넓으면 shadow map 해상도가 낭비되어 품질 저하.

---

## Rasterizer State (Shadow Pass 전용)

```cpp
D3D11_RASTERIZER_DESC rd = {};
rd.FillMode             = D3D11_FILL_SOLID;
rd.CullMode             = D3D11_CULL_BACK;
rd.DepthBias            = 100;      // 정수 단위, 하드웨어 포맷 의존
rd.SlopeScaledDepthBias = 2.0f;    // 기울어진 표면(구)에 자동으로 bias 가중
rd.DepthBiasClamp       = 0.01f;   // 최대 bias 상한
```

`SlopeScaledDepthBias`가 Self-shadow 처리의 핵심.  
표면이 빛에 대해 기울어질수록 acne가 심해지는데, 이 값이 기울기에 비례해 자동으로 보정.

---

## Comparison Sampler 설정

```cpp
D3D11_SAMPLER_DESC sd = {};
sd.Filter         = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
sd.AddressU       = D3D11_TEXTURE_ADDRESS_BORDER;
sd.AddressV       = D3D11_TEXTURE_ADDRESS_BORDER;
sd.BorderColor[0] = 1.0f;  // 경계 밖은 "빛이 닿음"으로 처리
sd.BorderColor[1] = 1.0f;
sd.BorderColor[2] = 1.0f;
sd.BorderColor[3] = 1.0f;
sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
```

`BORDER` 주소 모드 + BorderColor=1: shadow map 범위 밖 픽셀이 그림자로 잘못 처리되는 현상 방지.

---

## 렌더 루프 구조

```
Render()
├── RenderShadowPass()
│   ├── mShadowRS 바인딩
│   ├── mShadowDSV 클리어 (1.0f)
│   ├── RTV = nullptr, DSV = mShadowDSV 설정
│   ├── 뷰포트: 1024×1024
│   ├── mShadowVS + mShadowCB 바인딩
│   ├── PS = nullptr
│   └── Sphere.Draw() + Quad.Draw()
│
└── RenderMainPass()
    ├── 기존 RTV/DSV 복구
    ├── 기존 뷰포트 복구
    ├── mShadowSRV → t3 슬롯
    ├── mShadowSampler → s1 슬롯
    ├── cbPerObject 업데이트 (lightViewProj + shadow params 포함)
    └── 기존 드로우 콜 (PS = nullptr 해제 주의)
```

---

## ImGui 추가 항목

```
[Shadow]
 Shadow Bias       SliderFloat  [0.0001 ~ 0.01]   // acne ↔ peter panning 트레이드오프
 PCF Kernel        RadioButton  [1 | 3 | 5]
 Shadow Intensity  SliderFloat  [0.0 ~ 1.0]
```

---

## 주요 함정 & 대응

| 함정 | 현상 | 대응 |
|------|------|------|
| Shadow Acne | 표면에 줄무늬 패턴 | SlopeScaledDepthBias + shader bias 조합 |
| Peter Panning | 그림자가 물체에서 떠있음 | bias 과도하게 크지 않게, ImGui로 튜닝 |
| Up 벡터 충돌 | 빛이 수직일 때 LookAt NaN | lightDir.y > 0.99 조건 분기 |
| 프러스텀 밖 처리 | UV 범위 초과 시 잘못된 그림자 | uv 범위 체크 후 1.0 리턴 |
| cbPerObject 정렬 | HLSL 16바이트 패딩 오류 | 구조체 total 크기 16의 배수 확인 (320 bytes) |
| PS unbind 누락 | Shadow Pass 이후 PS가 null인 채 Main Pass 진행 | RenderMainPass 진입 시 mPS 재바인딩 명시 |

---

## 텍스처 포맷

Shadow map은 TYPELESS 포맷으로 생성해 DSV/SRV가 각각 다른 포맷으로 해석해야 함.

| 용도 | DXGI_FORMAT |
|------|------------|
| Texture2D 생성 | `DXGI_FORMAT_R32_TYPELESS` |
| DepthStencilView | `DXGI_FORMAT_D32_FLOAT` |
| ShaderResourceView | `DXGI_FORMAT_R32_FLOAT` |

---

## 파일 변경 목록

| 파일 | 변경 종류 |
|------|---------|
| `Shader/Shadow_VS.hlsl` | 신규 |
| `Shader/POM_PS.hlsl` | 수정 (shadow 리소스 + CalcShadowFactor 추가) |
| `D3DApp.h` | 수정 (멤버 추가, cbPerObject 확장) |
| `D3DApp.cpp` | 수정 (BuildShadowResources, RenderShadowPass, RenderMainPass 분리) |
