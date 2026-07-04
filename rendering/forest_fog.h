// =========================================================
// ファイルの情報[forest_fog.h]
//
// 制作者:Masatora Tanaka		日付：2026/07/04
// =========================================================
#pragma once

// =========================================================
// 森林ステージ用ボリューム霧の描画パラメータ
// =========================================================
struct ForestFogParams
{
    bool enabled = true;
    float viewX = 0.0f;
    float viewY = 0.0f;
    float viewWidth = 0.0f;
    float viewHeight = 0.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float timeSeconds = 0.0f;
    float density = 0.48f;
    float opacity = 0.62f;
    float coverage = 0.88f;
    float variation = 0.72f;
    float noiseScale = 1.30f;
    float driftSpeed = 0.035f;
    float parallax = 0.00018f;
    float verticalStart = 0.0f;
    float verticalEnd = 1.0f;
    float edgeSoftness = 0.10f;
    float fogColorR = 1.00f;
    float fogColorG = 1.00f;
    float fogColorB = 1.00f;
    float lightPositionX = 0.35f;
    float lightPositionY = -0.10f;
    float lightIntensity = 2.20f;
    float lightColorR = 1.00f;
    float lightColorG = 1.00f;
    float lightColorB = 1.00f;
    float rayLength = 0.95f;
    float rayDecay = 0.94f;
    float rayContrast = 1.70f;
};

// =========================================================
// ボリューム霧描画機能
// =========================================================
namespace forestFog
{
    bool initialize();
    void finalize();
    bool isReady();
    void draw(const ForestFogParams& params);
}
