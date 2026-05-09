cbuffer cbShadow : register(b0)
{
    float4x4 gLightViewProj;
};

float4 main(float3 posL : POSITION) : SV_Position
{
    return mul(float4(posL, 1.0f), gLightViewProj);
}
