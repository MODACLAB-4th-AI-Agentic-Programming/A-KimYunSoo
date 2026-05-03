#include "Sphere.h"
#include <vector>
#include <cmath>

static const float PI = 3.14159265f;

void Sphere::Build(ID3D11Device* device, int stacks, int slices)
{
    std::vector<Vertex> vertices;
    std::vector<UINT>   indices;

    for (int i = 0; i <= stacks; ++i)
    {
        float phi    = PI * i / stacks;
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);

        for (int j = 0; j <= slices; ++j)
        {
            float theta    = 2.0f * PI * j / slices;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            Vertex v;
            v.position  = { sinPhi * cosTheta, cosPhi, sinPhi * sinTheta };
            v.normal    = v.position;
            v.uv        = { (float)j / slices, (float)i / stacks };
            v.tangent   = { -sinTheta, 0.0f, cosTheta };

            XMVECTOR n = XMLoadFloat3(&v.normal);
            XMVECTOR t = XMLoadFloat3(&v.tangent);
            XMStoreFloat3(&v.bitangent, XMVector3Cross(n, t));

            vertices.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            UINT v0 =  i      * (slices + 1) + j;
            UINT v1 =  i      * (slices + 1) + (j + 1);
            UINT v2 = (i + 1) * (slices + 1) + j;
            UINT v3 = (i + 1) * (slices + 1) + (j + 1);

            // CCW winding (FrontCounterClockwise = TRUE 기준)
            indices.push_back(v0); indices.push_back(v1); indices.push_back(v2);
            indices.push_back(v1); indices.push_back(v3); indices.push_back(v2);
        }
    }

    mIndexCount = (UINT)indices.size();

    D3D11_BUFFER_DESC vbd     = {};
    vbd.ByteWidth             = (UINT)(sizeof(Vertex) * vertices.size());
    vbd.BindFlags             = D3D11_BIND_VERTEX_BUFFER;
    vbd.Usage                 = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA vd = { vertices.data() };
    device->CreateBuffer(&vbd, &vd, mVB.GetAddressOf());

    D3D11_BUFFER_DESC ibd     = {};
    ibd.ByteWidth             = (UINT)(sizeof(UINT) * indices.size());
    ibd.BindFlags             = D3D11_BIND_INDEX_BUFFER;
    ibd.Usage                 = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA id = { indices.data() };
    device->CreateBuffer(&ibd, &id, mIB.GetAddressOf());
}

void Sphere::Draw(ID3D11DeviceContext* ctx)
{
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, mVB.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->DrawIndexed(mIndexCount, 0, 0);
}

