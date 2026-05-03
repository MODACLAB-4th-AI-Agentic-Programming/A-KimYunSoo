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

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// 16-byte aligned: 64+64+64+16+16+16 = 240 bytes
struct cbPerObject
{
    XMMATRIX world;
    XMMATRIX view;
    XMMATRIX proj;
    XMFLOAT3 cameraPos;   // block [192-207]
    float    heightScale;
    XMFLOAT3 lightDir;    // block [208-223]
    float    specPower;
    int      usePOM;      // block [224-239]
    int      pad[3];
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

    ComPtr<ID3D11ShaderResourceView> mDiffuseSRV;
    ComPtr<ID3D11ShaderResourceView> mHeightSRV;
    ComPtr<ID3D11ShaderResourceView> mNormalSRV;
    ComPtr<ID3D11SamplerState>       mSampler;

    Sphere mSphere;
    Camera mCamera;

    bool  mUsePOM      = true;
    float mHeightScale = 0.05f;
    float mSpecPower   = 32.0f;
    float mRotation    = 0.0f;
    float mRotSpeed    = 1.0f;
    float mLightAngle  = 0.0f;
    bool  mLightPaused = false;
};
