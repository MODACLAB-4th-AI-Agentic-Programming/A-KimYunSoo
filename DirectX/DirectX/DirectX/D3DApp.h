#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include "Camera.h"
#include "Sphere.h"
#include "Quad.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

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

class D3DApp
{
public:
    D3DApp(HINSTANCE hInst);
    ~D3DApp();

    bool Init();
    int  Run();
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool InitWindow();
    bool InitD3D();
    void InitImGui();
    void BuildShaders();
    void BuildRenderState();
    void BuildConstantBuffer();
    void BuildShadowResources();
    void RenderShadowPass();
    void RenderMainPass();
    void LoadTextures();
    void OnResize();
    void Update(float dt);
    void Render();

    HRESULT LoadTexture(const wchar_t* path, ComPtr<ID3D11ShaderResourceView>& outSRV);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HINSTANCE mhInst;
    HWND      mhWnd;
    int       mWidth   = 800;
    int       mHeight  = 600;
    bool      mRunning = true;

    ComPtr<ID3D11Device>            mDevice;
    ComPtr<ID3D11DeviceContext>     mCtx;
    ComPtr<IDXGISwapChain>          mSwapChain;
    ComPtr<ID3D11RenderTargetView>  mRTV;
    ComPtr<ID3D11DepthStencilView>  mDSV;
    ComPtr<ID3D11Texture2D>         mDepthTex;

    ComPtr<ID3D11VertexShader>      mVS;
    ComPtr<ID3D11PixelShader>       mPS;
    ComPtr<ID3D11InputLayout>       mLayout;
    ComPtr<ID3D11RasterizerState>   mRSState;
    ComPtr<ID3D11DepthStencilState> mDSState;
    ComPtr<ID3D11Buffer>            mCBuf;

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

    ComPtr<ID3D11ShaderResourceView> mDiffuseSRV;
    ComPtr<ID3D11ShaderResourceView> mHeightSRV;
    ComPtr<ID3D11ShaderResourceView> mNormalSRV;
    ComPtr<ID3D11SamplerState>       mSampler;

    Sphere mSphere;
    Quad   mQuad;
    Camera mCamera;

    int   mMeshMode    = 0;  // 0 = Sphere, 1 = Quad

    bool  mUsePOM      = true;
    float mHeightScale = 0.05f;
    float mSpecPower   = 32.0f;
    float mRotation    = 0.0f;
    float mRotSpeed    = 1.0f;
    float mLightAngle  = 0.0f;
    bool  mLightPaused = false;
};
