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
    float noise = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
    float threshold = saturate(effectParams.x);
    float edgeWidth = max(effectParams.y, 0.0001f);

    if (noise < threshold)
    {
        discard;
    }

    float edgeMask = 1.0f - saturate((noise - threshold) / edgeWidth);
    c.rgb = lerp(c.rgb, outlineColor.rgb, edgeMask * saturate(outlineColor.a));
    return c;
}
