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

float ComputeCircleContribution(float2 screenPos, float4 light, float intensity)
{
    float distanceToLight = distance(screenPos, light.xy);
    float edgeFade = smoothstep(light.z, max(light.w, light.z + 0.001), distanceToLight);
    return (1.0 - edgeFade) * intensity;
}

float ComputeBoxContribution(float2 screenPos, float4 light, float4 shapeData, float intensity)
{
    float2 extents = shapeData.yz;
    float feather = max(shapeData.w, 0.001);
    float2 delta = abs(screenPos - light.xy) - extents;
    float outsideDistance = length(max(delta, 0.0));
    float insideDistance = min(max(delta.x, delta.y), 0.0);
    float signedDistance = outsideDistance + insideDistance;
    float edgeFade = smoothstep(0.0, feather, signedDistance);
    return (1.0 - edgeFade) * intensity;
}

float ComputeBeamContribution(float2 screenPos, float4 light, float4 shapeData, float intensity)
{
    float topHalfWidth = max(light.z, 0.001);
    float bottomHalfWidth = max(shapeData.y, topHalfWidth);
    float halfLength = max(shapeData.z, 0.001);
    float feather = max(shapeData.w, 0.001);
    float topY = light.y - halfLength;
    float normalizedY = saturate((screenPos.y - topY) / (halfLength * 2.0));
    float yWeight = normalizedY * normalizedY * (3.0 - 2.0 * normalizedY);
    float halfWidth = lerp(topHalfWidth, bottomHalfWidth, yWeight);
    float outsideX = max(abs(screenPos.x - light.x) - halfWidth, 0.0);
    float outsideY = max(max(topY - screenPos.y, screenPos.y - (light.y + halfLength)), 0.0);
    float outsideDistance = length(float2(outsideX, outsideY));
    float edgeFade = smoothstep(0.0, feather, outsideDistance);
    return (1.0 - edgeFade) * intensity;
}

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
    float strongestContribution = 0.0;
    float3 strongestLightColor = gDarknessColor;

    [loop]
    for (int lightIndex = 0; lightIndex < 16; ++lightIndex)
    {
        if ((float)lightIndex >= gLightCount)
        {
            break;
        }

        float4 light = gLightParams[lightIndex];
        float4 lightColor = gLightColors[lightIndex];
        float4 shapeData = gLightShapeData[lightIndex];
        float intensity = lightColor.a;
        float contribution = 0.0;
        if (shapeData.x < 0.5)
        {
            contribution = ComputeCircleContribution(screenPos, light, intensity);
        }
        else if (shapeData.x < 1.5)
        {
            contribution = ComputeBoxContribution(screenPos, light, shapeData, intensity);
        }
        else
        {
            contribution = ComputeBeamContribution(screenPos, light, shapeData, intensity);
        }

        visibility = max(visibility, contribution);
        if (contribution > strongestContribution)
        {
            strongestContribution = contribution;
            strongestLightColor = lightColor.rgb;
        }
    }

    float alpha = saturate((1.0 - visibility) * gDarknessOpacity);
    float colorBlend = saturate(visibility * 0.75);
    float3 overlayColor = lerp(gDarknessColor, strongestLightColor, colorBlend);
    return float4(overlayColor, alpha);
}
