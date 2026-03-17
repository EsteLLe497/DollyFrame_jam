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
    float levels = max(effectParams.x, 2.0f);
    float contrast = max(effectParams.y, 0.1f);
    c.rgb = saturate((c.rgb - 0.5f) * contrast + 0.5f);
    c.rgb = floor(c.rgb * levels) / (levels - 1.0f);
    return c;
}
