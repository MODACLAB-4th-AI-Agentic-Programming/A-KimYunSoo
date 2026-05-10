#include "Quad.h"

void Quad::Build(ID3D11Device* device)
{
    // XY 평면, 노멀 -Z (카메라가 z=-3에서 +Z 방향을 바라봄)
    // UV: U = 왼→오(+X), V = 위→아래(-Y)
    // 탄젠트: (1,0,0), 바이탄젠트: (0,-1,0)  => N = T×B = (0,0,-1)
    Vertex verts[4] = {
        // position         normal          uv        tangent        bitangent
        { {-1.0f,  1.0f, 0.0f}, {0,0,-1}, {0.0f, 0.0f}, {1,0,0}, {0,-1,0} }, // 좌상
        { { 1.0f,  1.0f, 0.0f}, {0,0,-1}, {1.0f, 0.0f}, {1,0,0}, {0,-1,0} }, // 우상
        { {-1.0f, -1.0f, 0.0f}, {0,0,-1}, {0.0f, 1.0f}, {1,0,0}, {0,-1,0} }, // 좌하
        { { 1.0f, -1.0f, 0.0f}, {0,0,-1}, {1.0f, 1.0f}, {1,0,0}, {0,-1,0} }, // 우하
    };

    // CW 와인딩 (FrontCounterClockwise=FALSE, 카메라 -Z 방향에서 바라볼 때 시계방향)
    UINT indices[6] = { 0, 1, 2, 1, 3, 2 };

    D3D11_BUFFER_DESC vbd     = {};
    vbd.ByteWidth             = sizeof(verts);
    vbd.BindFlags             = D3D11_BIND_VERTEX_BUFFER;
    vbd.Usage                 = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA vd = { verts };
    device->CreateBuffer(&vbd, &vd, mVB.GetAddressOf());

    D3D11_BUFFER_DESC ibd     = {};
    ibd.ByteWidth             = sizeof(indices);
    ibd.BindFlags             = D3D11_BIND_INDEX_BUFFER;
    ibd.Usage                 = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA id = { indices };
    device->CreateBuffer(&ibd, &id, mIB.GetAddressOf());
}

void Quad::Draw(ID3D11DeviceContext* ctx)
{
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, mVB.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->DrawIndexed(6, 0, 0);
}
