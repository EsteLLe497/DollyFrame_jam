// =========================================================
// ファイルの情報[forest_fog_ps.hlsl]
//
// 制作者:Masatora Tanaka		日付：2026/07/04
// =========================================================

cbuffer ForestFogParams : register(b0)
{
    float2 gViewSize;
    float gTimeSeconds;
    float gDensity;

    float2 gCameraOffset;
    float gNoiseScale;
    float gDriftSpeed;

    float3 gFogColor;
    float gOpacity;

    float gVerticalStart;
    float gVerticalEnd;
    float gEdgeSoftness;
    float gRayLength;

    float2 gLightPosition;
    float gLightIntensity;
    float gRayDecay;

    float3 gLightColor;
    float gRayContrast;

    float gCoverage;
    float gVariation;
    float2 gPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 dif : COLOR0;
    float4 spc : COLOR1;
    float2 uv : TEXCOORD0;
    float2 suv : TEXCOORD1;
};

static const int kRaySampleCount = 8;
static const float kNoiseRepeat = 8.0;

// =========================================================
// 疑似乱数
// =========================================================
float hash21(float2 position)
{
    position = frac(position * float2(123.34, 456.21));
    position += dot(position, position + 45.32);
    return frac(position.x * position.y);
}

// =========================================================
// 周期座標へ変換
// =========================================================
float2 wrapNoiseCell(float2 cell, float repeatCount)
{
    return cell - floor(cell / repeatCount) * repeatCount;
}

// =========================================================
// 描画範囲内で循環する補間ノイズ
// =========================================================
float periodicValueNoise(float2 position, float repeatCount)
{
    float2 cell = floor(position);
    float2 local = frac(position);
    float2 blend = local * local * (3.0 - 2.0 * local);

    float bottomLeft = hash21(wrapNoiseCell(cell, repeatCount));
    float bottomRight = hash21(wrapNoiseCell(cell + float2(1.0, 0.0), repeatCount));
    float topLeft = hash21(wrapNoiseCell(cell + float2(0.0, 1.0), repeatCount));
    float topRight = hash21(wrapNoiseCell(cell + float2(1.0, 1.0), repeatCount));

    return lerp(
        lerp(bottomLeft, bottomRight, blend.x),
        lerp(topLeft, topRight, blend.x),
        blend.y);
}

// =========================================================
// 時間方向にも継ぎ目なく循環する軽量フラクタルノイズ
// =========================================================
float loopingFractalNoise(float2 position, float loopPhase)
{
    float2 firstLoopOffset = loopPhase * float2(kNoiseRepeat, -kNoiseRepeat);
    float2 secondLoopOffset = loopPhase * float2(kNoiseRepeat * 2.0, -kNoiseRepeat * 2.0);
    float value = periodicValueNoise(position + firstLoopOffset, kNoiseRepeat) * 0.68;
    value += periodicValueNoise(
        position * 2.0 + secondLoopOffset + 13.7,
        kNoiseRepeat * 2.0) * 0.32;
    return value;
}

// =========================================================
// レイ上の霧密度を取得
// =========================================================
float sampleDensity(float2 uv, float rayDepth)
{
    float aspect = gViewSize.x / max(gViewSize.y, 1.0);
    float loopPhase = frac(gTimeSeconds * gDriftSpeed);
    float2 volumePosition =
        float2(uv.x * aspect, uv.y) * gNoiseScale +
        gCameraOffset +
        rayDepth * float2(0.43, -0.29);

    // 周期ノイズを一周分移動させ、範囲内で霧の流れを継ぎ目なくループする。
    float broadShape = loopingFractalNoise(volumePosition, loopPhase);
    float detailShape = periodicValueNoise(
        volumePosition * 3.0 +
        loopPhase * float2(kNoiseRepeat * 3.0, -kNoiseRepeat * 3.0) -
        rayDepth * 1.91,
        kNoiseRepeat * 3.0);
    float fogShape = broadShape * 0.79 + detailShape * 0.21;
    float threshold = lerp(0.72, 0.30, saturate(gCoverage));
    float cloudDensity = smoothstep(threshold - 0.16, threshold + 0.18, fogShape);
    // 全面の薄霧を残しつつ、周期ノイズで濃い場所をまばらに作る。
    float density = lerp(0.58, cloudDensity, saturate(gVariation));
    float groundWeight = lerp(0.82, 1.20, smoothstep(0.08, 0.96, uv.y));
    return saturate(density * groundWeight * lerp(0.72, 1.15, saturate(gDensity)));
}

// =========================================================
// 上下端の霧フェードを取得
// =========================================================
float getVerticalShape(float y)
{
    float verticalIn = gVerticalStart <= 0.0
        ? 1.0
        : smoothstep(gVerticalStart - gEdgeSoftness, gVerticalStart + gEdgeSoftness, y);
    float verticalOut = gVerticalEnd >= 1.0
        ? 1.0
        : 1.0 - smoothstep(gVerticalEnd - gEdgeSoftness, gVerticalEnd + gEdgeSoftness, y);
    return saturate(verticalIn * verticalOut);
}

// =========================================================
// ボリューム霧ピクセル描画
// =========================================================
float4 main(PSInput input) : SV_TARGET
{
    float2 rayVector = (gLightPosition - input.uv) * gRayLength;
    float2 rayStep = rayVector / (float)kRaySampleCount;
    float jitter = hash21(input.position.xy) - 0.5;
    float2 sampleUv = input.uv + rayStep * jitter;
    float illumination = 1.0;
    float scattering = 0.0;

    // 光源方向へ固定8ステップ進み、霧密度と光の減衰を積分する。
    [unroll]
    for (int sampleIndex = 0; sampleIndex < kRaySampleCount; ++sampleIndex)
    {
        float rayDepth = ((float)sampleIndex + 0.5 + jitter * 0.35) / (float)kRaySampleCount;
        sampleUv += rayStep;
        scattering += sampleDensity(sampleUv, rayDepth) * illumination;
        illumination *= gRayDecay;
    }
    scattering /= (float)kRaySampleCount;

    float localDensity = sampleDensity(input.uv, 0.18);
    float verticalShape = getVerticalShape(input.uv.y);
    float lightDistance = length(gLightPosition - input.uv);
    float lightReach = 1.0 - smoothstep(0.15, 1.28, lightDistance);
    float shaftStrength =
        pow(saturate(scattering), gRayContrast) *
        lightReach *
        gLightIntensity;

    // 均一な薄霧を下地にし、密度塊と光条を加えて参考画像の空気遠近へ寄せる。
    float baseHaze = lerp(0.08, 0.22, saturate(gCoverage));
    float alpha = saturate(
        (baseHaze + localDensity * 0.44 + shaftStrength * 0.95) *
        gOpacity *
        verticalShape);
    float lightMix = saturate(shaftStrength * 1.45);
    float3 color = lerp(gFogColor, gLightColor, lightMix);

    return float4(saturate(color), alpha);
}
