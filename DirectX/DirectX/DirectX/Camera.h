#pragma once
#include <DirectXMath.h>
using namespace DirectX;

struct Camera
{
    XMFLOAT3 position = { 0.0f, 0.0f, -3.0f };

    XMMATRIX GetView() const
    {
        XMVECTOR eye = XMLoadFloat3(&position);
        XMVECTOR at  = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMVECTOR up  = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return XMMatrixLookAtLH(eye, at, up);
    }

    XMMATRIX GetProj(float aspectRatio) const
    {
        return XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f), aspectRatio, 0.1f, 100.0f);
    }
};
