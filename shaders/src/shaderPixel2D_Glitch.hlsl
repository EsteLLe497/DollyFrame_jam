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
    float rowIndex = floor(texcoord.y * 48.0f + effectParams.y * 12.0f);
    float offset = (Hash(float2(rowIndex, 0.0f)) - 0.5f) * effectParams.x;
    float2 uv = texcoord + float2(offset, 0.0f);
    float4 c = tex.Sample(samplerstate, uv) * tint;
    float scan = 1.0f - frac(texcoord.y * 80.0f + effectParams.y * 10.0f) * effectParams.z;
    c.rgb *= saturate(1.0f - scan * 0.35f);
    return c;
}
