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
    float4 gLightParams[16];
    float4 gLightIntensityPack[4];
    float4 gLightColors[16];
    float4 gLightShapeData[16];
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
    float3 lightColorAccum = 0.0;
    float lightContributionSum = 0.0;

    [loop]
    for (int lightIndex = 0; lightIndex < 16; ++lightIndex)
    {
        if ((float)lightIndex >= gLightCount)
        {
            break;
        }

        float4 light = gLightParams[lightIndex];
        float4 lightColor = gLightColors[lightIndex];
        float intensity = lightColor.a;
        float contribution = 0.0;
        if (gLightShapeData[lightIndex].x < 0.5)
        {
            float distanceToLight = distance(screenPos, light.xy);
            float edgeFade = smoothstep(light.z, max(light.w, light.z + 0.001), distanceToLight);
            edgeFade = edgeFade * edgeFade * (3.0 - 2.0 * edgeFade);
            contribution = (1.0 - edgeFade) * intensity;
        }
        else
        {
            float2 extents = gLightShapeData[lightIndex].yz;
            float feather = max(gLightShapeData[lightIndex].w, 0.001);
            float2 delta = abs(screenPos - light.xy) - extents;
            float outsideDistance = length(max(delta, 0.0));
            float insideDistance = min(max(delta.x, delta.y), 0.0);
            float signedDistance = outsideDistance + insideDistance;
            float edgeFade = smoothstep(0.0, feather, signedDistance);
            edgeFade = edgeFade * edgeFade * (3.0 - 2.0 * edgeFade);
            contribution = (1.0 - edgeFade) * intensity;
        }
        visibility = max(visibility, contribution);
        lightColorAccum += lightColor.rgb * contribution;
        lightContributionSum += contribution;
    }

    float alpha = saturate((1.0 - visibility) * gDarknessOpacity);
    float3 averageLightColor = lightContributionSum > 0.001
        ? lightColorAccum / lightContributionSum
        : float3(1.0, 1.0, 1.0);
    float colorBlend = saturate(visibility * 0.75);
    float3 overlayColor = lerp(gDarknessColor, averageLightColor, colorBlend);
    return float4(overlayColor, alpha);
}
