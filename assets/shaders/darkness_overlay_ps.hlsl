cbuffer DarknessOverlayParams : register(b0)
{
    float2 gScreenSize;
    float2 gPadding0;
    float4 gViewRect;
    float gDarknessOpacity;
    float gLightCount;
    float2 gPadding1;
    float3 gDarknessColor;
    float gPadding2;
    float4 gLightParams[8];
    float4 gLightIntensityPack[2];
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 dif : COLOR0;
    float4 spc : COLOR1;
    float2 uv : TEXCOORD0;
    float2 suv : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET
{
    if (gLightCount < 0.5)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float2 screenPos = input.uv * gScreenSize;
    if (screenPos.x < gViewRect.x || screenPos.x > gViewRect.z ||
        screenPos.y < gViewRect.y || screenPos.y > gViewRect.w)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float visibility = 0.0;

    [unroll]
    for (int lightIndex = 0; lightIndex < 8; ++lightIndex)
    {
        if ((float)lightIndex >= gLightCount)
        {
            break;
        }

        float4 light = gLightParams[lightIndex];
        float intensity = lightIndex < 4
            ? gLightIntensityPack[0][lightIndex]
            : gLightIntensityPack[1][lightIndex - 4];
        float distanceToLight = distance(screenPos, light.xy);
        float edgeFade = smoothstep(light.z, max(light.w, light.z + 0.001), distanceToLight);
        edgeFade = edgeFade * edgeFade * (3.0 - 2.0 * edgeFade);
        visibility = max(visibility, (1.0 - edgeFade) * intensity);
    }

    float alpha = saturate((1.0 - visibility) * gDarknessOpacity);
    return float4(gDarknessColor, alpha);
}
