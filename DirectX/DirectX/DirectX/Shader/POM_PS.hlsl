cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4x4 gView;
    float4x4 gProj;
    float3   gCameraPos;
    float    gHeightScale;
};

Texture2D    gDiffuseTex : register(t0);
Texture2D    gHeightMap  : register(t1);
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

// Ray marching POM
// viewDirTS: view direction in tangent space (z > 0 points toward surface)
// Returns offset UV coordinates
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

    // TBN rows are T, B, N -> mul(TBN, v) transforms world to tangent space
    float3x3 TBN     = float3x3(input.tangentWS, input.bitangentWS, input.normalWS);
    float3 viewDirTS = normalize(mul(TBN, viewDirWS));

    // Skip POM at grazing angles to avoid artifacts
    float2 finalUV = input.uv;
    if (viewDirTS.z > 0.001)
        finalUV = ParallaxOcclusionMapping(input.uv, viewDirTS);

    float4 diffuse = gDiffuseTex.Sample(gSampler, finalUV);

    // Lambert diffuse + ambient
    float3 lightDir = normalize(float3(1.0, 1.0, -1.0));
    float  NdotL    = saturate(dot(normalize(input.normalWS), lightDir));
    float3 lit      = diffuse.rgb * (NdotL * 0.8 + 0.2);

    return float4(lit, diffuse.a);
}
