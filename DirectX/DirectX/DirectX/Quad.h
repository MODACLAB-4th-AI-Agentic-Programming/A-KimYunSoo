#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Sphere.h"  // Vertex struct

using Microsoft::WRL::ComPtr;

class Quad
{
public:
    void Build(ID3D11Device* device);
    void Draw(ID3D11DeviceContext* ctx);

private:
    ComPtr<ID3D11Buffer> mVB;
    ComPtr<ID3D11Buffer> mIB;
};
