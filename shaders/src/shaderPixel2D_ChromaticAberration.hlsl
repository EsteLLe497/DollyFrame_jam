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
    float2 offset = float2(sin(effectParams.y * 2.7f), cos(effectParams.y * 2.1f)) * effectParams.x;
    float4 baseColor = tex.Sample(samplerstate, texcoord) * tint;
    float red = tex.Sample(samplerstate, texcoord + offset).r * tint.r;
    float green = tex.Sample(samplerstate, texcoord).g * tint.g;
    float blue = tex.Sample(samplerstate, texcoord - offset).b * tint.b;
    return float4(red, green, blue, baseColor.a);
}
