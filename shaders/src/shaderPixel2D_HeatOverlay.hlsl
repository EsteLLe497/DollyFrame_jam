cbuffer PSColor : register(b1)
{
    float4 tint;
}

cbuffer PSEffect : register(b2)
{
    int4 modeAndFlags;
    float4 outlineColor;
    float4 textureInfo;
    float4 effectParams;
    float4 secondaryColor;
    float4 tertiaryColor;
}

Texture2D tex : register(t0);
SamplerState samplerstate : register(s0);

float4 main(in float4 position : SV_Position,
            in float2 texcoord : TEXCOORD0) : SV_TARGET
{
    float4 c = tex.Sample(samplerstate, texcoord) * tint;
    float pulse = 0.5f + 0.5f * sin(effectParams.y * 6.0f);
    float heat = saturate(effectParams.x) * (0.55f + pulse * 0.45f);
    float3 heatColor = lerp(float3(1.0f, 0.78f, 0.15f), float3(1.0f, 0.16f, 0.06f), texcoord.y);
    c.rgb = lerp(c.rgb, heatColor, heat * 0.65f);
    return c;
}
