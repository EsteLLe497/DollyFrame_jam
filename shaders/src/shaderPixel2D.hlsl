
cbuffer PSColor : register(b1)
{
    float4 tint; // RGBA (0..1)
}

Texture2D tex : register(t0);
SamplerState samplerstate : register(s0);

float4 main(in float4 position : SV_Position,
            in float2 texcoord : TEXCOORD0) : SV_TARGET
{
    float4 c = tex.Sample(samplerstate, texcoord);
    return c * tint; // �� �����ŐF�itint�j���|����
}

//float4 main(in float4 position : SV_Position,
//            in float2 texcoord : TEXCOORD0) : SV_TARGET
//{
//    return tex.Sample(samplerstate, texcoord);
//}


