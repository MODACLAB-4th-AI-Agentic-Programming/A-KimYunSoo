#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
    XMFLOAT3 tangent;
    XMFLOAT3 bitangent;
};

class Sphere
{
public:
    void Build(ID3D11Device* device, int stacks = 30, int slices = 30);
    void Draw(ID3D11DeviceContext* ctx);

private:
    ComPtr<ID3D11Buffer> mVB;
    ComPtr<ID3D11Buffer> mIB;
    UINT                 mIndexCount = 0;
};
