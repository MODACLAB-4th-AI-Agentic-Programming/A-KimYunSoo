#include "D3DApp.h"
#include <wincodec.h>
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static D3DApp* gApp = nullptr;

LRESULT CALLBACK D3DApp::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
    if (gApp) return gApp->MsgProc(hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3DApp::D3DApp(HINSTANCE hInst) : mhInst(hInst), mhWnd(nullptr)
{
    gApp = this;
}

D3DApp::~D3DApp()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CoUninitialize();
    // ComPtr members release automatically
}

bool D3DApp::Init()
{
    CoInitialize(nullptr);
    if (!InitWindow()) return false;
    if (!InitD3D())    return false;
    InitImGui();
    mSphere.Build(mDevice.Get());
    mQuad.Build(mDevice.Get());
    BuildShaders();
    BuildRenderState();
    BuildConstantBuffer();
    BuildShadowResources();
    LoadTextures();
    return true;
}

int D3DApp::Run()
{
    MSG msg = {};
    while (mRunning)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) mRunning = false;
        }
        Update(0.016f);
        Render();
    }
    return (int)msg.wParam;
}

bool D3DApp::InitWindow()
{
    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = mhInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"D3DWindow";
    RegisterClassEx(&wc);

    RECT r = { 0, 0, mWidth, mHeight };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    mhWnd = CreateWindowEx(0, L"D3DWindow", L"Parallax Occlusion Mapping - DirectX 11",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left, r.bottom - r.top,
        nullptr, nullptr, mhInst, nullptr);

    if (!mhWnd) return false;
    ShowWindow(mhWnd, SW_SHOW);
    UpdateWindow(mhWnd);
    return true;
}

bool D3DApp::InitD3D()
{
    DXGI_SWAP_CHAIN_DESC scd  = {};
    scd.BufferCount           = 1;
    scd.BufferDesc.Width      = mWidth;
    scd.BufferDesc.Height     = mHeight;
    scd.BufferDesc.Format     = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow          = mhWnd;
    scd.SampleDesc.Count      = 1;
    scd.Windowed              = TRUE;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, mSwapChain.GetAddressOf(), mDevice.GetAddressOf(),
        &featureLevel, mCtx.GetAddressOf());

    if (FAILED(hr)) return false;
    OnResize();
    return true;
}

void D3DApp::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(mhWnd);
    ImGui_ImplDX11_Init(mDevice.Get(), mCtx.Get());
}

void D3DApp::OnResize()
{
    mRTV.Reset();
    mDSV.Reset();
    mDepthTex.Reset();

    mSwapChain->ResizeBuffers(0, mWidth, mHeight, DXGI_FORMAT_UNKNOWN, 0);

    ComPtr<ID3D11Texture2D> backBuffer;
    mSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    mDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, mRTV.GetAddressOf());

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width                = mWidth;
    dd.Height               = mHeight;
    dd.MipLevels            = 1;
    dd.ArraySize            = 1;
    dd.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count     = 1;
    dd.BindFlags            = D3D11_BIND_DEPTH_STENCIL;
    mDevice->CreateTexture2D(&dd, nullptr, mDepthTex.GetAddressOf());
    mDevice->CreateDepthStencilView(mDepthTex.Get(), nullptr, mDSV.GetAddressOf());

    D3D11_VIEWPORT vp = {};
    vp.Width          = (float)mWidth;
    vp.Height         = (float)mHeight;
    vp.MaxDepth       = 1.0f;
    mCtx->RSSetViewports(1, &vp);

    ID3D11RenderTargetView* rtvRaw = mRTV.Get();
    mCtx->OMSetRenderTargets(1, &rtvRaw, mDSV.Get());
}

void D3DApp::BuildShaders()
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    HRESULT hr = D3DCompileFromFile(L"Shader/POM_VS.hlsl", nullptr, nullptr,
        "VS", "vs_5_0", flags, 0, vsBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return;
    }

    hr = D3DCompileFromFile(L"Shader/POM_PS.hlsl", nullptr, nullptr,
        "PS", "ps_5_0", flags, 0, psBlob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return;
    }

    mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, mVS.GetAddressOf());
    mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, mPS.GetAddressOf());

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    mDevice->CreateInputLayout(layout, 5,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), mLayout.GetAddressOf());

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
}

void D3DApp::BuildRenderState()
{
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode              = D3D11_FILL_SOLID;
    rd.CullMode              = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable       = TRUE;
    mDevice->CreateRasterizerState(&rd, mRSState.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable              = TRUE;
    dsd.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc                = D3D11_COMPARISON_LESS;
    mDevice->CreateDepthStencilState(&dsd, mDSState.GetAddressOf());

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy      = 1;
    sd.MaxLOD             = D3D11_FLOAT32_MAX;
    mDevice->CreateSamplerState(&sd, mSampler.GetAddressOf());
}

void D3DApp::BuildConstantBuffer()
{
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth         = sizeof(cbPerObject);
    cbd.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage             = D3D11_USAGE_DYNAMIC;
    cbd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    mDevice->CreateBuffer(&cbd, nullptr, mCBuf.GetAddressOf());
}

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

HRESULT D3DApp::LoadTexture(const wchar_t* path, ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    ComPtr<IWICImagingFactory> wic;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wic.GetAddressOf()));
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return hr;

    ComPtr<IWICFormatConverter> conv;
    wic->CreateFormatConverter(conv.GetAddressOf());
    conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    std::vector<BYTE> pixels(w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());

    D3D11_TEXTURE2D_DESC td = {};
    td.Width                = w;
    td.Height               = h;
    td.MipLevels            = 1;
    td.ArraySize            = 1;
    td.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count     = 1;
    td.Usage                = D3D11_USAGE_IMMUTABLE;
    td.BindFlags            = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem                = pixels.data();
    initData.SysMemPitch            = w * 4;

    ComPtr<ID3D11Texture2D> tex;
    hr = mDevice->CreateTexture2D(&td, &initData, tex.GetAddressOf());
    if (FAILED(hr)) return hr;

    return mDevice->CreateShaderResourceView(tex.Get(), nullptr, outSRV.GetAddressOf());
}

void D3DApp::LoadTextures()
{
    LoadTexture(L"../Texture/diffuse.png",      mDiffuseSRV);
    LoadTexture(L"../Texture/displacement.png", mHeightSRV);
    LoadTexture(L"../Texture/normal.png",       mNormalSRV);
}

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
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    mSwapChain->Present(1, 0);
}

void D3DApp::Render()
{
    RenderShadowPass();
    RenderMainPass();
}

LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        mRunning = false;
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { PostQuitMessage(0); mRunning = false; }
        return 0;
    case WM_SIZE:
        if (mDevice && LOWORD(lParam) > 0 && HIWORD(lParam) > 0) {
            mWidth  = LOWORD(lParam);
            mHeight = HIWORD(lParam);
            OnResize();
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
