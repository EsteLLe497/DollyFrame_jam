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
    float2 centered = abs(texcoord * 2.0f - 1.0f);
    float edge = 1.0f - saturate(1.0f - max(centered.x, centered.y));
    float rim = pow(edge, max(effectParams.x, 0.1f)) * outlineColor.a;
    c.rgb = lerp(c.rgb, outlineColor.rgb, rim);
    return c;
}
