
//�萔�o�b�t�@
float4x4 mtx;

float4 main(in float4 position : POSITION0,
                in float2 texcood : TEXCOORD,
                out float2 outTexcoord : TEXCOORD) : SV_Position
{
    outTexcoord = texcood;
    return mul(position, mtx);
}
