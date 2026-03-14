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
    float2 texelSize = textureInfo.xy;
    float thickness = textureInfo.z;
    float baseAlpha = c.a;
    float outlineAlpha = 0.0f;
    outlineAlpha = max(outlineAlpha, tex.Sample(samplerstate, texcoord + float2(texelSize.x * thickness, 0.0f)).a);
    outlineAlpha = max(outlineAlpha, tex.Sample(samplerstate, texcoord + float2(-texelSize.x * thickness, 0.0f)).a);
    outlineAlpha = max(outlineAlpha, tex.Sample(samplerstate, texcoord + float2(0.0f, texelSize.y * thickness)).a);
    outlineAlpha = max(outlineAlpha, tex.Sample(samplerstate, texcoord + float2(0.0f, -texelSize.y * thickness)).a);
    float mask = saturate(outlineAlpha - baseAlpha);
    c.rgb = lerp(c.rgb, outlineColor.rgb, mask * outlineColor.a);
    c.a = max(c.a, mask * outlineColor.a);
    return c;
}
