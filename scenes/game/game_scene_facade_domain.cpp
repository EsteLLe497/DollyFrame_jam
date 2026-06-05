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

float GameScene::GetViewScale() const
{
    const float marginX = std::clamp(static_cast<float>(SCREEN_WIDTH) * 0.04f, 48.0f, 96.0f);
    const float marginY = std::clamp(static_cast<float>(SCREEN_HEIGHT) * 0.04f, 36.0f, 72.0f);
    const float maxWidth = static_cast<float>(SCREEN_WIDTH) - marginX * 2.0f;
    const float maxHeight = static_cast<float>(SCREEN_HEIGHT) - marginY * 2.0f;
    return std::min(maxWidth / gCameraViewWidth, maxHeight / gCameraViewHeight) * m_render.viewScaleMultiplier;
}

float GameScene::GetViewWidth() const
{
    return gCameraViewWidth * GetViewScale();
}

float GameScene::GetViewHeight() const
{
    return gCameraViewHeight * GetViewScale();
}

float GameScene::GetViewOriginX() const
{
    if (GetViewWidth() >= static_cast<float>(SCREEN_WIDTH))
    {
        if (m_render.zoomAnchorScreenCenter)
        {
            return std::round(m_render.zoomAnchorX - GetViewWidth() * 0.5f) + m_render.shakeOffsetX;
        }
        return m_render.shakeOffsetX;
    }

    return std::round((static_cast<float>(SCREEN_WIDTH) - GetViewWidth()) * 0.5f) + m_render.shakeOffsetX;
}

float GameScene::GetViewOriginY() const
{
    if (GetViewHeight() >= static_cast<float>(SCREEN_HEIGHT))
    {
        if (m_render.zoomAnchorScreenCenter)
        {
            return std::round(m_render.zoomAnchorY - GetViewHeight() * 0.5f) + m_render.shakeOffsetY;
        }
        return m_render.shakeOffsetY;
    }

    return std::round((static_cast<float>(SCREEN_HEIGHT) - GetViewHeight()) * 0.5f) + m_render.shakeOffsetY;
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
    for (const auto& entity : m_world.Entities())
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
    m_render.shakeOffsetX = 0.0f;
    m_render.shakeOffsetY = 0.0f;
    m_render.zoomAnchorScreenCenter = false;
    m_render.zoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    m_render.zoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;

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
    m_render.viewScaleMultiplier = m_mapEditor.active ? 1.0f : baseCameraZoomMultiplier;

    if (m_flow.screenShakeRemaining > 0.0f && m_flow.screenShakeDuration > 0.0f && m_flow.screenShakeAmplitude > 0.0f)
    {
        const float elapsed = m_flow.screenShakeDuration - m_flow.screenShakeRemaining;
        const float intensity = Clamp01(m_flow.screenShakeRemaining / m_flow.screenShakeDuration);
        m_render.shakeOffsetX = std::sin(elapsed * 91.0f) * m_flow.screenShakeAmplitude * intensity;
        m_render.shakeOffsetY = std::cos(elapsed * 123.0f) * (m_flow.screenShakeAmplitude * 0.6f) * intensity;
    }

    const float zoomBlend =
        m_flow.captureModeZoomBlend * m_flow.captureModeZoomBlend * (3.0f - 2.0f * m_flow.captureModeZoomBlend);

    // Capture zoom uses the currently rendered camera-center as pivot.
    m_render.zoomAnchorX = GetViewOriginX() + GetViewWidth() * 0.5f;
    m_render.zoomAnchorY = GetViewOriginY() + GetViewHeight() * 0.5f;

    if (!m_mapEditor.active)
    {
        m_render.viewScaleMultiplier = baseCameraZoomMultiplier + zoomBlend * 0.08f;
        m_render.zoomAnchorScreenCenter = m_flow.cameraMode;
    }
}

void GameScene::DrawWorldAndUiLayers()
{
    DrawBackdrop();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Background);
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Shadow);
    DrawBossShockwavesUnderlay();
    for (const auto& entity : m_world.Entities())
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
    DrawMidBoss3HpBar();
    DrawAttackCaptureSlot();
    DrawEnemyAttackRects();
}

void GameScene::ResetFrameRendering()
{
    m_render.shakeOffsetX = 0.0f;
    m_render.shakeOffsetY = 0.0f;
    m_render.viewScaleMultiplier = 1.0f;
    m_render.zoomAnchorScreenCenter = false;
    m_render.zoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    m_render.zoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
}

