cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float3   gCameraPos;
    float    gHeightScale;
};

struct VSInput
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float2 uv        : TEXCOORD;
    float3 tangent   : TANGENT;
    float3 bitangent : BITANGENT;
};

struct VSOutput
{
    float4 posCS       : SV_POSITION;
    float2 uv          : TEXCOORD0;
    float3 tangentWS   : TEXCOORD1;
    float3 bitangentWS : TEXCOORD2;
    float3 normalWS    : TEXCOORD3;
    float3 posWS       : TEXCOORD4;
};

VSOutput VS(VSInput input)
{
    VSOutput output;

    float4 posWS   = mul(float4(input.position, 1.0), gWorld);
    output.posWS   = posWS.xyz;
    output.posCS   = mul(mul(posWS, gView), gProj);
    output.uv      = input.uv;

    // 법선/탄젠트는 월드 행렬 3x3으로 변환 (uniform scale 가정)
    float3x3 worldRot    = (float3x3)gWorld;
    output.normalWS      = normalize(mul(input.normal,    worldRot));
    output.tangentWS     = normalize(mul(input.tangent,   worldRot));
    output.bitangentWS   = normalize(mul(input.bitangent, worldRot));

    return output;
}
