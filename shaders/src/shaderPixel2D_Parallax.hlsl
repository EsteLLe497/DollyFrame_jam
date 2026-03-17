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
    float2 backUV = texcoord + float2(effectParams.w * effectParams.x, 0.0f);
    float2 frontUV = texcoord + float2(effectParams.w * effectParams.y, effectParams.w * effectParams.y * 0.35f);
    float4 backColor = tex.Sample(samplerstate, backUV) * tint;
    float4 frontColor = tex.Sample(samplerstate, frontUV) * tint;
    return lerp(backColor, frontColor, saturate(effectParams.z));
}
