Texture2D sceneTexture : register(t0);
Texture2D lightTexture : register(t1);
SamplerState sceneSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gTime;
    float gVignetteStrength;
    float gGrainStrength;
    float gLightBlend;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 dif : COLOR0;
    float4 spc : COLOR1;
    float2 uv : TEXCOORD0;
    float2 suv : TEXCOORD1;
};

float Hash21(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453);
}

float3 ApplyEveningGrade(float3 color)
{
    float luma = dot(color, float3(0.299, 0.587, 0.114));
    float3 desaturated = lerp(color, luma.xxx, 0.08);
    float shadowFactor = saturate(1.0 - luma * 1.28);
    float highlightFactor = saturate((luma - 0.34) * 1.20);
    float3 duskShadow = float3(0.38, 0.34, 0.30);
    float3 coolShadow = float3(0.30, 0.32, 0.38);
    float3 sunsetLift = float3(1.06, 1.00, 0.90);

    float3 base = desaturated * lerp(float3(1.0, 1.0, 1.0), duskShadow, shadowFactor * 0.11);
    base = lerp(base, base * coolShadow, shadowFactor * 0.03);
    return lerp(base, base * sunsetLift, highlightFactor * 0.14);
}

float3 BuildFarFog(float2 uv, float3 light, float timeSeconds)
{
    float farMask = 1.0 - smoothstep(0.34, 0.90, uv.y);
    float sideFade = 1.0 - smoothstep(0.10, 1.00, abs(uv.x - 0.5) * 1.8);
    float steamNoise = Hash21(uv * 18.0 + float2(timeSeconds * 0.010, -timeSeconds * 0.007));
    float lightLift = saturate(dot(light, float3(0.64, 0.38, 0.16)) * 1.45);
    float fogMask = farMask * (0.66 + sideFade * 0.34) * (0.96 + (steamNoise - 0.5) * 0.08);
    float fogStrength = fogMask * (0.022 + lightLift * 0.030);
    return float3(0.62, 0.46, 0.30) * fogStrength;
}

float3 BuildSunsetRay(float2 uv, float3 light)
{
    float2 rayDir = normalize(float2(-0.84, -0.54));
    float3 streak = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        float stepT = (float)(i + 1) * 0.012;
        float weight = 1.0 - (float)i * 0.16;
        float2 sampleUv = uv + rayDir * stepT;
        float3 sampleLight = lightTexture.Sample(sceneSampler, sampleUv).rgb;
        streak += sampleLight * weight;
        totalWeight += weight;
    }

    float3 blurredLight = streak / max(totalWeight, 0.0001);
    float sourceMask = smoothstep(0.52, 0.98, uv.x) * (1.0 - smoothstep(0.34, 0.82, uv.y));
    float distanceFade = smoothstep(0.08, 0.78, uv.y) * (1.0 - smoothstep(0.78, 1.0, uv.y));
    return blurredLight * sourceMask * distanceFade * float3(0.92, 0.66, 0.34) * 0.09;
}

float3 BuildWarmBuildingHit(float2 uv, float3 sceneColor, float3 light)
{
    float texelX = 1.0 / 1280.0;
    float texelY = 1.0 / 720.0;
    float3 right = sceneTexture.Sample(sceneSampler, uv + float2(texelX, 0.0)).rgb;
    float3 left = sceneTexture.Sample(sceneSampler, uv - float2(texelX, 0.0)).rgb;
    float3 up = sceneTexture.Sample(sceneSampler, uv + float2(0.0, texelY)).rgb;
    float3 down = sceneTexture.Sample(sceneSampler, uv - float2(0.0, texelY)).rgb;

    float edge = length(right - left) + length(up - down);
    float luma = dot(sceneColor, float3(0.299, 0.587, 0.114));
    float warmMask = saturate((sceneColor.r - sceneColor.b) * 1.05 + (sceneColor.g - sceneColor.b) * 0.40);
    float buildingBand = smoothstep(0.16, 0.92, uv.y) * (1.0 - smoothstep(0.90, 1.0, uv.y));
    float wx = abs(frac(uv.x * 56.0) - 0.5);
    float wy = abs(frac(uv.y * 42.0) - 0.5);
    float windowGrid = (1.0 - smoothstep(0.42, 0.50, wx)) * (1.0 - smoothstep(0.40, 0.50, wy));
    float lightMask = saturate(dot(light, float3(0.66, 0.38, 0.14)) * 1.22);
    float edgeMask = saturate(edge * 0.66);
    float hit = saturate(edgeMask * 0.72 + windowGrid * 0.18) * warmMask * (0.35 + lightMask * 0.65) * buildingBand;
    hit *= saturate((luma - 0.12) * 1.2);
    return float3(0.90, 0.68, 0.40) * hit * 0.14;
}

float BuildForegroundShade(float2 uv)
{
    float nearMask = smoothstep(0.54, 1.0, uv.y);
    float sideMask = smoothstep(0.20, 1.0, abs(uv.x - 0.5) * 1.7);
    return nearMask * (0.04 + sideMask * 0.04);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float4 src = sceneTexture.Sample(sceneSampler, uv);
    float3 light = lightTexture.Sample(sceneSampler, uv).rgb;

    float2 center = uv - float2(0.5, 0.5);
    float vignette = saturate(1.0 - dot(center, center) * 1.55);
    float vignetteMix = lerp(1.0 - gVignetteStrength, 1.0, vignette);

    float grain = Hash21(uv * 720.0 + gTime * 8.0) - 0.5;
    float grainScale = gGrainStrength * (0.20 + (1.0 - vignette) * 0.16);

    float3 color = ApplyEveningGrade(src.rgb) * vignetteMix;
    color += light * gLightBlend;
    color += BuildFarFog(uv, light, gTime);
    color += BuildSunsetRay(uv, light);
    color += BuildWarmBuildingHit(uv, src.rgb, light);
    color *= (1.0 - BuildForegroundShade(uv));
    color += grain * grainScale;
    color *= 1.08;

    float luma = dot(color, float3(0.299, 0.587, 0.114));
    float3 sunsetTint = float3(1.06, 0.98, 0.90);
    color = lerp(color, color * sunsetTint, saturate((1.0 - luma) * 0.05));
    color = color / (1.0 + color * 0.28);

    return float4(saturate(color), src.a);
}
