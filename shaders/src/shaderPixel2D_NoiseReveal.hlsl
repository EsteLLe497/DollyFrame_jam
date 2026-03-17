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

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(in float4 position : SV_Position,
            in float2 texcoord : TEXCOORD0) : SV_TARGET
{
    float4 c = tex.Sample(samplerstate, texcoord) * tint;
    float noise = Hash(floor(texcoord * 48.0f) + effectParams.z);
    float threshold = saturate(effectParams.x);
    if (noise < threshold)
    {
        discard;
    }

    float edge = 1.0f - saturate((noise - threshold) / max(effectParams.y, 0.0001f));
    c.rgb = lerp(c.rgb, outlineColor.rgb, edge * outlineColor.a);
    return c;
}
