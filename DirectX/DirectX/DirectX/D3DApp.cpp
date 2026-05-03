#include "D3DApp.h"
#include <wincodec.h>
#include <cassert>
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

    if (mSampler)    mSampler->Release();
    if (mNormalSRV)  mNormalSRV->Release();
    if (mHeightSRV)  mHeightSRV->Release();
    if (mDiffuseSRV) mDiffuseSRV->Release();
    if (mCBuf)       mCBuf->Release();
    if (mDSState)    mDSState->Release();
    if (mRSState)    mRSState->Release();
    if (mLayout)     mLayout->Release();
    if (mPS)         mPS->Release();
    if (mVS)         mVS->Release();
    if (mDepthTex)   mDepthTex->Release();
    if (mDSV)        mDSV->Release();
    if (mRTV)        mRTV->Release();
    if (mSwapChain)  mSwapChain->Release();
    if (mCtx)        mCtx->Release();
    if (mDevice)     mDevice->Release();
    CoUninitialize();
}

bool D3DApp::Init()
{
    CoInitialize(nullptr);
    if (!InitWindow()) return false;
    if (!InitD3D())    return false;
    InitImGui();
    mSphere.Build(mDevice);
    BuildShaders();
    BuildRenderState();
    BuildConstantBuffer();
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
        &scd, &mSwapChain, &mDevice, &featureLevel, &mCtx);

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
    ImGui_ImplDX11_Init(mDevice, mCtx);
}

void D3DApp::OnResize()
{
    if (mRTV)      { mRTV->Release();      mRTV = nullptr; }
    if (mDSV)      { mDSV->Release();      mDSV = nullptr; }
    if (mDepthTex) { mDepthTex->Release(); mDepthTex = nullptr; }

    mSwapChain->ResizeBuffers(0, mWidth, mHeight, DXGI_FORMAT_UNKNOWN, 0);

    ID3D11Texture2D* backBuffer = nullptr;
    mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRTV);
    backBuffer->Release();

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width                = mWidth;
    dd.Height               = mHeight;
    dd.MipLevels            = 1;
    dd.ArraySize            = 1;
    dd.Format               = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count     = 1;
    dd.BindFlags            = D3D11_BIND_DEPTH_STENCIL;
    mDevice->CreateTexture2D(&dd, nullptr, &mDepthTex);
    mDevice->CreateDepthStencilView(mDepthTex, nullptr, &mDSV);

    D3D11_VIEWPORT vp = {};
    vp.Width          = (float)mWidth;
    vp.Height         = (float)mHeight;
    vp.MaxDepth       = 1.0f;
    mCtx->RSSetViewports(1, &vp);
    mCtx->OMSetRenderTargets(1, &mRTV, mDSV);
}

void D3DApp::BuildShaders()
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* vsBlob  = nullptr;
    ID3DBlob* psBlob  = nullptr;
    ID3DBlob* errBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(L"Shader/POM_VS.hlsl", nullptr, nullptr,
        "VS", "vs_5_0", flags, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        return;
    }

    hr = D3DCompileFromFile(L"Shader/POM_PS.hlsl", nullptr, nullptr,
        "PS", "ps_5_0", flags, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            OutputDebugStringA((char*)errBlob->GetBufferPointer());
            errBlob->Release();
        }
        vsBlob->Release();
        return;
    }

    mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mVS);
    mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mPS);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    mDevice->CreateInputLayout(layout, 5,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &mLayout);

    vsBlob->Release();
    psBlob->Release();
}

void D3DApp::BuildRenderState()
{
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode              = D3D11_FILL_SOLID;
    rd.CullMode              = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable       = TRUE;
    mDevice->CreateRasterizerState(&rd, &mRSState);

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable              = TRUE;
    dsd.DepthWriteMask           = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc                = D3D11_COMPARISON_LESS;
    mDevice->CreateDepthStencilState(&dsd, &mDSState);

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter             = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy      = 1;
    sd.MaxLOD             = D3D11_FLOAT32_MAX;
    mDevice->CreateSamplerState(&sd, &mSampler);
}

void D3DApp::BuildConstantBuffer()
{
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth         = sizeof(cbPerObject);
    cbd.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
    cbd.Usage             = D3D11_USAGE_DYNAMIC;
    cbd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;
    mDevice->CreateBuffer(&cbd, nullptr, &mCBuf);
}

HRESULT D3DApp::LoadTexture(const wchar_t* path, ID3D11ShaderResourceView** ppSRV)
{
    *ppSRV = nullptr;

    IWICImagingFactory* wic = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
    if (FAILED(hr)) return hr;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { wic->Release(); return hr; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr)) { wic->Release(); return hr; }

    IWICFormatConverter* conv = nullptr;
    wic->CreateFormatConverter(&conv);
    conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    frame->Release();

    UINT w = 0, h = 0;
    conv->GetSize(&w, &h);
    std::vector<BYTE> pixels(w * h * 4);
    conv->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), pixels.data());
    conv->Release();
    wic->Release();

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

    ID3D11Texture2D* tex = nullptr;
    hr = mDevice->CreateTexture2D(&td, &initData, &tex);
    if (FAILED(hr)) return hr;

    hr = mDevice->CreateShaderResourceView(tex, nullptr, ppSRV);
    tex->Release();
    return hr;
}

void D3DApp::LoadTextures()
{
    LoadTexture(L"../Texture/diffuse.png",      &mDiffuseSRV);
    LoadTexture(L"../Texture/displacement.png", &mHeightSRV);
    LoadTexture(L"../Texture/normal.png",       &mNormalSRV);
}

void D3DApp::Update(float dt)
{
    mRotation += mRotSpeed * dt;

    D3D11_MAPPED_SUBRESOURCE mapped;
    mCtx->Map(mCBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    auto* cb = reinterpret_cast<cbPerObject*>(mapped.pData);

    cb->world       = XMMatrixTranspose(XMMatrixRotationY(mRotation));
    cb->view        = XMMatrixTranspose(mCamera.GetView());
    cb->proj        = XMMatrixTranspose(mCamera.GetProj((float)mWidth / mHeight));
    cb->cameraPos   = mCamera.position;
    cb->heightScale = mHeightScale;
    cb->usePOM      = mUsePOM ? 1 : 0;
    cb->pad[0] = cb->pad[1] = cb->pad[2] = 0.0f;

    mCtx->Unmap(mCBuf, 0);
}

void D3DApp::Render()
{
    const float clearColor[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
    mCtx->ClearRenderTargetView(mRTV, clearColor);
    mCtx->ClearDepthStencilView(mDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    mCtx->IASetInputLayout(mLayout);
    mCtx->VSSetShader(mVS, nullptr, 0);
    mCtx->PSSetShader(mPS, nullptr, 0);
    mCtx->RSSetState(mRSState);
    mCtx->OMSetDepthStencilState(mDSState, 0);

    mCtx->VSSetConstantBuffers(0, 1, &mCBuf);
    mCtx->PSSetConstantBuffers(0, 1, &mCBuf);

    ID3D11ShaderResourceView* srvs[3] = { mDiffuseSRV, mHeightSRV, mNormalSRV };
    mCtx->PSSetShaderResources(0, 3, srvs);
    mCtx->PSSetSamplers(0, 1, &mSampler);

    mSphere.Draw(mCtx);

    // ImGui
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("POM Controls");
    ImGui::Checkbox("Enable POM", &mUsePOM);
    ImGui::SliderFloat("Height Scale", &mHeightScale, 0.01f, 0.2f);
    ImGui::SliderFloat("Rotation Speed", &mRotSpeed, 0.0f, 5.0f);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    mSwapChain->Present(1, 0);
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
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            mRunning = false;
        }
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
