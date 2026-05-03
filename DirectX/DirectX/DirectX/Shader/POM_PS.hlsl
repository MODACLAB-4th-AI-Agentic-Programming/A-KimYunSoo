cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float3   gCameraPos;
    float    gHeightScale;
    int      gUsePOM;
    float3   gLightDir;
};

Texture2D    gDiffuseTex : register(t0);
Texture2D    gHeightMap  : register(t1);
Texture2D    gNormalMap  : register(t2);
SamplerState gSampler    : register(s0);

struct PSInput
{
    float4 posCS       : SV_POSITION;
    float2 uv          : TEXCOORD0;
    float3 tangentWS   : TEXCOORD1;
    float3 bitangentWS : TEXCOORD2;
    float3 normalWS    : TEXCOORD3;
    float3 posWS       : TEXCOORD4;
};

float2 ParallaxOcclusionMapping(float2 uv, float3 viewDirTS)
{
    float numLayers  = lerp(32.0, 8.0, saturate(abs(viewDirTS.z)));
    float layerDepth = 1.0 / numLayers;
    float2 uvStep    = (viewDirTS.xy / viewDirTS.z) * gHeightScale * layerDepth;

    float  currentDepth = 0.0;
    float2 currentUV    = uv;
    float  h            = gHeightMap.Sample(gSampler, currentUV).r;

    [loop]
    for (int i = 0; i < 32; i++)
    {
        if (currentDepth >= h) break;
        currentUV    -= uvStep;
        h             = gHeightMap.Sample(gSampler, currentUV).r;
        currentDepth += layerDepth;
    }

    return currentUV;
}

float4 PS(PSInput input) : SV_TARGET
{
    float3 viewDirWS = normalize(gCameraPos - input.posWS);

    // TBN: world -> tangent space (rows = T, B, N)
    float3x3 TBN     = float3x3(input.tangentWS, input.bitangentWS, input.normalWS);
    float3 viewDirTS = normalize(mul(TBN, viewDirWS));

    // POM: UV 오프셋 계산
    float2 finalUV = input.uv;
    if (gUsePOM && viewDirTS.z > 0.001)
        finalUV = ParallaxOcclusionMapping(input.uv, viewDirTS);

    // Diffuse
    float4 diffuse = gDiffuseTex.Sample(gSampler, finalUV);

    // Normal map decode + OpenGL->DX Y flip
    float3 normalTS = gNormalMap.Sample(gSampler, finalUV).rgb * 2.0 - 1.0;
    normalTS.y = -normalTS.y;

    // 탄젠트 -> 월드 공간 변환: mul(v, TBN) = T*v.x + B*v.y + N*v.z
    float3 normalWS = normalize(mul(normalTS, TBN));

    // Lambert + ambient
    float  NdotL    = saturate(dot(normalWS, normalize(gLightDir)));
    float3 lit      = diffuse.rgb * (NdotL * 0.85 + 0.15);

    return float4(lit, diffuse.a);
}
