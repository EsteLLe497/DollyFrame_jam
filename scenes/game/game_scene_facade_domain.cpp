#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"

#include <algorithm>
#include <cmath>

using namespace game_scene_detail;

namespace
{
    constexpr float kZoomTargetTilesX = 23.0f;
}

void GameScene::BeginFrameUpdate(float deltaTime)
{
    UpdateTuningHotReload(deltaTime);
}

bool GameScene::TryHandleModalUpdates(float deltaTime)
{
    if (m_debug.showEscapeMenu)
    {
        UpdateEscapeMenuInput();
        return true;
    }

    if (m_mapEditor.active)
    {
        UpdateMapEditorInput(deltaTime);
        return true;
    }

    HandleGlobalSceneShortcuts();
    ProcessFilterInput();

    UpdateTuningPanel();
    if (m_debug.showTuningPanel)
    {
        return true;
    }

    if (UpdatePitRestartFlow(deltaTime))
    {
        return true;
    }
    if (UpdateStageTransitionFlow(deltaTime))
    {
        return true;
    }

    return false;
}

float GameScene::PrepareGameplayDeltaTime(float deltaTime)
{
    const float gameplayDeltaTime = UpdatePhotoModes(deltaTime);
    m_flow.hitStopRemaining = std::max(0.0f, m_flow.hitStopRemaining - deltaTime);
    m_flow.screenShakeRemaining = std::max(0.0f, m_flow.screenShakeRemaining - deltaTime);
    const float effectiveGameplayDeltaTime = m_flow.hitStopRemaining > 0.0f ? 0.0f : gameplayDeltaTime;
    m_flow.lastDeltaTime = effectiveGameplayDeltaTime;
    UpdateFrameTimers(deltaTime, gameplayDeltaTime, effectiveGameplayDeltaTime);
    return effectiveGameplayDeltaTime;
}

void GameScene::TickEntities(float effectiveGameplayDeltaTime)
{
    for (const auto& entity : m_entities)
    {
        entity->Update(effectiveGameplayDeltaTime);
    }
}

void GameScene::FinalizeGameplayFrame(float effectiveGameplayDeltaTime)
{
    GameSession_SetTimeRemaining(m_flow.timeRemaining);
    RunGameplayFrame(effectiveGameplayDeltaTime);
    if (Entity* player = FindEntityByTag(kTagPlayer))
    {
        game_scene_player_visual_system::UpdateAnimation(m_player, m_flow, *player, m_player.dodgeRemaining > 0.0f);
    }
}

void GameScene::PrepareFrameRendering()
{
    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderZoomAnchorScreenCenter = false;
    gRenderZoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    gRenderZoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;

    float baseCameraZoomMultiplier = 1.0f;
    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize > 0.0f)
    {
        const float targetWorldWidth = tileSize * kZoomTargetTilesX;
        if (targetWorldWidth > 0.0f)
        {
            baseCameraZoomMultiplier = std::max(1.0f, static_cast<float>(SCREEN_WIDTH) / targetWorldWidth);
        }
    }
    gRenderViewScaleMultiplier = m_mapEditor.active ? 1.0f : baseCameraZoomMultiplier;

    if (m_flow.screenShakeRemaining > 0.0f && m_flow.screenShakeDuration > 0.0f && m_flow.screenShakeAmplitude > 0.0f)
    {
        const float elapsed = m_flow.screenShakeDuration - m_flow.screenShakeRemaining;
        const float intensity = Clamp01(m_flow.screenShakeRemaining / m_flow.screenShakeDuration);
        gRenderShakeOffsetX = std::sin(elapsed * 91.0f) * m_flow.screenShakeAmplitude * intensity;
        gRenderShakeOffsetY = std::cos(elapsed * 123.0f) * (m_flow.screenShakeAmplitude * 0.6f) * intensity;
    }

    const float zoomBlend =
        m_flow.captureModeZoomBlend * m_flow.captureModeZoomBlend * (3.0f - 2.0f * m_flow.captureModeZoomBlend);

    // Capture zoom uses the currently rendered camera-center as pivot.
    gRenderZoomAnchorX = GetViewOriginX() + GetViewWidth() * 0.5f;
    gRenderZoomAnchorY = GetViewOriginY() + GetViewHeight() * 0.5f;

    if (!m_mapEditor.active)
    {
        gRenderViewScaleMultiplier = baseCameraZoomMultiplier + zoomBlend * 0.08f;
        gRenderZoomAnchorScreenCenter = m_flow.cameraMode;
    }
}

void GameScene::DrawWorldAndUiLayers()
{
    DrawBackdrop();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Background);
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Shadow);
    DrawBossShockwavesUnderlay();
    for (const auto& entity : m_entities)
    {
        if (entity && (HasTag(*entity, kTagPhotoBox) ||
            entity->GetComponent<PhotoPasteOrderComponent>() ||
            HasTag(*entity, "BossShockwave")))
        {
            continue;
        }
        DrawEntity(*entity);
    }
    DrawEffects();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Foreground);
    DrawPastedEntitiesFront();
    DrawPhotoPlacementPreview();
    DrawStageDarknessOverlay();
    DrawSepiaFilmFilterOverlay();
    DrawMarkerLightOutlines();
    DrawCaptureOverlay();
    DrawPhotoStorageTray();
    DrawDevelopedPhotoPreview();
    DrawPitRestartOverlay();
    DrawEscapeMenuOverlay();
    DrawMapEditorOverlay();
    DrawTuningPanel();
    DrawBatterySwitchCounters();
    DrawPlayerHpBar();
    DrawEnemyAttackRects();
}

void GameScene::ResetFrameRendering()
{
    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderViewScaleMultiplier = 1.0f;
    gRenderZoomAnchorScreenCenter = false;
    gRenderZoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    gRenderZoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
}

