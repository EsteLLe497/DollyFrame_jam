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
    float2 wave = float2(
        sin((texcoord.y + effectParams.z) * 18.0f) * effectParams.x,
        cos((texcoord.x + effectParams.z) * 14.0f) * effectParams.y);
    float4 c = tex.Sample(samplerstate, texcoord + wave) * tint;
    c.rgb = lerp(c.rgb, c.rgb * float3(0.55f, 0.78f, 1.0f), saturate(effectParams.w));
    return c;
}
