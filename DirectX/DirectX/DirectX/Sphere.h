#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

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
    ~Sphere();

private:
    ID3D11Buffer* mVB         = nullptr;
    ID3D11Buffer* mIB         = nullptr;
    UINT          mIndexCount = 0;
};
