#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <vector>
#include "Camera.h"
#include "Sphere.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;

// 16-byte aligned: 64+64+64+16+16 = 224 bytes
struct cbPerObject
{
    XMMATRIX world;
    XMMATRIX view;
    XMMATRIX proj;
    XMFLOAT3 cameraPos;
    float    heightScale;
    int      usePOM;
    float    pad[3];
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

    HRESULT LoadTexture(const wchar_t* path, ID3D11ShaderResourceView** ppSRV);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HINSTANCE mhInst;
    HWND      mhWnd;
    int       mWidth   = 800;
    int       mHeight  = 600;
    bool      mRunning = true;

    ID3D11Device*            mDevice    = nullptr;
    ID3D11DeviceContext*     mCtx       = nullptr;
    IDXGISwapChain*          mSwapChain = nullptr;
    ID3D11RenderTargetView*  mRTV       = nullptr;
    ID3D11DepthStencilView*  mDSV       = nullptr;
    ID3D11Texture2D*         mDepthTex  = nullptr;

    ID3D11VertexShader*      mVS      = nullptr;
    ID3D11PixelShader*       mPS      = nullptr;
    ID3D11InputLayout*       mLayout  = nullptr;
    ID3D11RasterizerState*   mRSState = nullptr;
    ID3D11DepthStencilState* mDSState = nullptr;
    ID3D11Buffer*            mCBuf    = nullptr;

    ID3D11ShaderResourceView* mDiffuseSRV = nullptr;
    ID3D11ShaderResourceView* mHeightSRV  = nullptr;
    ID3D11SamplerState*       mSampler    = nullptr;

    Sphere mSphere;
    Camera mCamera;

    bool  mUsePOM      = true;
    float mHeightScale = 0.05f;
};
