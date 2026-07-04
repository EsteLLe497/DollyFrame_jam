// =========================================================
// ファイルの情報[game_scene_volumetric_fog.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/04
// =========================================================
#include "pch.h"

#include "game_scene_internal.h"

#include "forest_fog.h"

#include "DxLib.h"

// =========================================================
// 森林ステージのボリューム霧描画
// =========================================================
void GameScene::drawForestVolumetricFog() const
{
    const auto& tuning = m_tuning.volumetricFog;
    if (!m_lifecycle.forestStageEnabled ||
        m_lifecycle.loadingActive ||
        m_mapEditor.active ||
        !tuning.enabled)
    {
        return;
    }

    ForestFogParams params;
    params.viewX = GetViewOriginX() + tuning.positionX;
    params.viewY = GetViewOriginY() + tuning.positionY;
    params.viewWidth = tuning.width;
    params.viewHeight = tuning.height;
    params.cameraX = m_flow.cameraX;
    params.cameraY = m_flow.cameraY;
    params.timeSeconds = static_cast<float>(GetNowCount()) * 0.001f;
    params.density = tuning.density;
    params.opacity = tuning.opacity;
    params.coverage = tuning.coverage;
    params.variation = tuning.variation;
    params.noiseScale = tuning.noiseScale;
    params.driftSpeed = tuning.driftSpeed;
    params.fogColorR = tuning.fogColorR;
    params.fogColorG = tuning.fogColorG;
    params.fogColorB = tuning.fogColorB;
    params.lightPositionX = tuning.lightPositionX;
    params.lightPositionY = tuning.lightPositionY;
    params.lightColorR = tuning.lightColorR;
    params.lightColorG = tuning.lightColorG;
    params.lightColorB = tuning.lightColorB;
    params.lightIntensity = tuning.godRayIntensity;
    params.rayLength = tuning.godRayLength;
    params.rayDecay = tuning.godRayDecay;
    params.rayContrast = tuning.godRayContrast;
    forestFog::draw(params);

    // ImGui調整中だけ対象範囲を可視化し、ゲーム本番の描画には残さない。
    if (tuning.showBounds)
    {
        DrawBoxAA(
            params.viewX,
            params.viewY,
            params.viewX + params.viewWidth,
            params.viewY + params.viewHeight,
            GetColor(255, 96, 220),
            FALSE,
            2.0f);
    }
}
