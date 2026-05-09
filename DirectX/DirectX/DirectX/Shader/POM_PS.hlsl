cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float3   gCameraPos;
    float    gHeightScale;
    float3   gLightDir;
    float    gSpecPower;
    int      gUsePOM;
    int      pad[3];
    float4x4 gLightViewProj;
    float    gShadowBias;
    int      gPCFKernel;
    float    gShadowIntensity;
    int      gShadowPad;
};

Texture2D    gDiffuseTex : register(t0);
Texture2D    gHeightMap  : register(t1);
Texture2D    gNormalMap  : register(t2);
SamplerState gSampler    : register(s0);

Texture2D              gShadowMap  : register(t3);
SamplerComparisonState gShadowSamp : register(s1);

struct PSInput
{
    float4 posCS       : SV_POSITION;
    float2 uv          : TEXCOORD0;
    float3 tangentWS   : TEXCOORD1;
    float3 bitangentWS : TEXCOORD2;
    float3 normalWS    : TEXCOORD3;
    float3 posWS       : TEXCOORD4;
};

float CalcShadowFactor(float3 posW)
{
    float4 posL = mul(float4(posW, 1.0f), gLightViewProj);
    posL.xyz   /= posL.w;

    float2 uv = float2(posL.x * 0.5f + 0.5f, -posL.y * 0.5f + 0.5f);

    // 빛 프러스텀 밖은 그림자 없음
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float  depth      = posL.z - gShadowBias;
    float  shadow     = 0.0f;
    float  texelSize  = 1.0f / 1024.0f;
    int    radius     = gPCFKernel / 2;
    float  total      = (float)(gPCFKernel * gPCFKernel);

    for (int y = -radius; y <= radius; ++y)
    for (int x = -radius; x <= radius; ++x)
        shadow += gShadowMap.SampleCmpLevelZero(
            gShadowSamp, uv + float2(x, y) * texelSize, depth);

    return lerp(1.0f - gShadowIntensity, 1.0f, shadow / total);
}

float2 ParallaxOcclusionMapping(float2 uv, float3 viewDirTS)
{
    float numLayers  = lerp(64.0, 16.0, saturate(viewDirTS.z));
    float layerDepth = 1.0 / numLayers;
    float2 uvStep    = (viewDirTS.xy / viewDirTS.z) * gHeightScale * layerDepth;

    float  currentDepth = 0.0;
    float2 currentUV    = uv;
    float  h            = gHeightMap.Sample(gSampler, currentUV).r;

    float2 prevUV    = currentUV;
    float  prevDepth = 0.0;

    // 1단계: 교차 구간 탐색
    [loop]
    for (int i = 0; i < 64; i++)
    {
        if (currentDepth >= h) break;
        prevUV       = currentUV;
        prevDepth    = currentDepth;
        currentUV   -= uvStep;
        currentDepth += layerDepth;
        h            = gHeightMap.Sample(gSampler, currentUV).r;
    }

    // 2단계: 이진 탐색으로 교차점 정밀화 (선형 보간은 급경사 홈에서 오차 큼)
    [unroll]
    for (int j = 0; j < 8; j++)
    {
        float2 midUV    = (prevUV + currentUV) * 0.5;
        float  midDepth = (prevDepth + currentDepth) * 0.5;
        float  midH     = gHeightMap.Sample(gSampler, midUV).r;

        if (midDepth < midH)
        { prevUV = midUV; prevDepth = midDepth; }
        else
        { currentUV = midUV; currentDepth = midDepth; }
    }

    return (prevUV + currentUV) * 0.5;
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

    float3 L = normalize(gLightDir);

    // Lambert
    float NdotL = saturate(dot(normalWS, L));

    // Blinn-Phong specular
    float3 H    = normalize(viewDirWS + L);
    float NdotH = saturate(dot(normalWS, H));
    float spec  = pow(NdotH, gSpecPower);

    float shadowFactor = CalcShadowFactor(input.posWS);

    float3 lit = (diffuse.rgb * (NdotL * 0.85 + 0.15)
               + float3(1.0, 1.0, 1.0) * spec * 0.6 * NdotL) * shadowFactor;

    return float4(lit, diffuse.a);
}
