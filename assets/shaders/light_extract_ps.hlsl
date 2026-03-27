Texture2D sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gTime;
    float gParam1;
    float gParam2;
    float gParam3;
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
    float3 src = sceneTexture.Sample(sceneSampler, input.uv).rgb;
    float luma = dot(src, float3(0.299, 0.587, 0.114));
    float warmness = saturate((src.r - src.b) * 1.00 + (src.g - src.b) * 0.42);
    float sunsetness = saturate((src.r - src.g) * 0.30 + src.r * 0.18 + src.g * 0.10);
    float coolness = saturate((src.b - src.r) * 0.60 + (src.g - src.r) * 0.14);

    float warmGlow = saturate((luma - 0.12) * 1.45) * saturate(warmness * 0.92 + sunsetness * 0.44);
    float windowGlow = saturate((luma - 0.20) * 1.70) * saturate(warmness * 0.80 + 0.10);
    float softSkyGlow = saturate((luma - 0.32) * 1.20) * saturate((src.r + src.g) * 0.30);
    float gaugeGlow = saturate((luma - 0.34) * 1.20) * coolness * 0.10;
    float mask = saturate(warmGlow * 0.62 + windowGlow * 0.56 + softSkyGlow * 0.22 + gaugeGlow);

    float3 amberTint = float3(0.96, 0.72, 0.40);
    float3 sunsetTint = float3(1.00, 0.76, 0.46);
    float3 gaugeTint = float3(0.64, 0.74, 0.88);
    float3 warmTint = lerp(amberTint, sunsetTint, saturate(sunsetness * 1.3));
    float3 glowTint = lerp(warmTint, gaugeTint, gaugeGlow * 0.8);
    float3 extracted = src * glowTint * mask * 1.05;
    return float4(extracted, mask);
}
