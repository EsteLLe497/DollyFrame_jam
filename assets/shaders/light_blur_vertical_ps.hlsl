Texture2D lightTexture : register(t0);
SamplerState sceneSampler : register(s0);

cbuffer PostProcessParams : register(b0)
{
    float gTexelSizeY;
    float gIntensity;
    float gParam2;
    float gParam3;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 dif : COLOR0;
    float4 spc : COLOR1;
    float2 uv : TEXCOORD0;
    float2 suv : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv;
    float offsets[5] = { -6.0, -3.0, 0.0, 3.0, 6.0 };
    float weights[5] = { 0.08, 0.22, 0.40, 0.22, 0.08 };

    float3 sum = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int i = 0; i < 5; ++i)
    {
        float2 sampleUv = uv + float2(0.0, offsets[i] * gTexelSizeY);
        sum += lightTexture.Sample(sceneSampler, sampleUv).rgb * weights[i];
    }

    return float4(sum * gIntensity, 1.0);
}
