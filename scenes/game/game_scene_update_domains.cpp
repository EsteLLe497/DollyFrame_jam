#include "game_scene_internal.h"
#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPhotoFocusTimeScale = 0.22f;
    constexpr float kCaptureFocusDuration = 0.8f;
    constexpr float kPlacementFocusDuration = 1.2f;
    constexpr float kStageTransitionFadeInDuration = 1.10f;
    constexpr float kCaptureFinderScaleMin = 1.0f;
    constexpr float kCaptureFinderScaleMax = 2.0f;
    constexpr float kCaptureFinderScaleStep = 0.1f;
    constexpr float kCaptureModeZoomResponse = 7.0f;
}

void GameScene::UpdateCameraMode()
{
    const bool wasCameraMode = m_flow.cameraMode;
    m_flow.cameraMode = Input_IsActionDown(InputAction::HoldCamera);
    if (m_flow.cameraMode)
    {
        m_photo.placement.active = false;
        m_photo.placement.valid = false;
    }
    if (m_flow.cameraMode && !wasCameraMode)
    {
        ++m_flow.cameraModeSessionId;
    }
}

float GameScene::UpdatePhotoModes(float deltaTime)
{
    UpdateCameraMode();

    // 3状態（撮影/配置/現像プレビュー）から、トレイ表示とスロー演出を一元決定する。
    const bool placementHeld = !m_flow.cameraMode && m_photo.capture.hasPhoto && Input_IsActionDown(InputAction::HoldPlacement);
    const bool placementActive = placementHeld || m_photo.placement.active;
    const bool previewActive = m_photo.pendingStore.active && m_flow.developedPhotoPreviewRemaining > 0.0f;
    const bool previewOrbAttached =
        previewActive &&
        m_flow.developedPhotoPreviewRemaining <= 0.34f;
    const bool showPhotoTray = (previewActive && !previewOrbAttached) || m_flow.cameraMode || placementActive;
    const float trayTarget = showPhotoTray ? 1.0f : 0.0f;
    m_flow.photoTrayReveal += (trayTarget - m_flow.photoTrayReveal) * std::min(1.0f, deltaTime * 12.0f);
    if (showPhotoTray)
    {
        UpdatePhotoTraySelection();
    }
    const float captureZoomTarget = m_flow.cameraMode ? 1.0f : 0.0f;
    m_flow.captureModeZoomBlend += (captureZoomTarget - m_flow.captureModeZoomBlend) * std::min(1.0f, deltaTime * kCaptureModeZoomResponse);
    m_flow.captureSlowRemaining = m_flow.cameraMode ? kCaptureFocusDuration : 0.0f;
    m_flow.placementSlowRemaining = placementActive ? kPlacementFocusDuration : 0.0f;
    const bool slowForCapture = m_flow.cameraMode;
    const bool slowForPlacement = placementActive;
    // フォーカス中だけゲーム全体を減速させる。
    return (slowForCapture || slowForPlacement)
        ? deltaTime * kPhotoFocusTimeScale
        : deltaTime;
}

void GameScene::UpdateCaptureFinderZoomInput()
{
    if (!m_flow.cameraMode)
    {
        return;
    }

    int zoomDirection = 0;
    const int wheelDelta = GetMouseWheelRotVol();
    const bool dpadUpDown = Input_IsDpadUpDown();
    const bool dpadDownDown = Input_IsDpadDownDown();
    if (wheelDelta > 0 || dpadUpDown)
    {
        ++zoomDirection;
    }
    if (wheelDelta < 0 || dpadDownDown)
    {
        --zoomDirection;
    }

    // Reverse gamepad zoom mapping:
    // LB = zoom in, RB = zoom out.
    if (Input_IsLeftShoulderPressed())
    {
        ++zoomDirection;
    }
    else if (Input_IsRightShoulderPressed())
    {
        --zoomDirection;
    }

    if (zoomDirection != 0)
    {
        m_flow.captureFinderScale = std::clamp(
            m_flow.captureFinderScale + static_cast<float>(zoomDirection) * kCaptureFinderScaleStep,
            kCaptureFinderScaleMin,
            kCaptureFinderScaleMax);
    }
}

void GameScene::ProcessFilterInput()
{
    if (Input_IsActionPressed(InputAction::SelectFilterNone))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::None;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterHot))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Hot;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterCold))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Cold;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterInvert))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Invert;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterSepia))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
    }
    if (Input_IsActionPressed(InputAction::CycleFilter))
    {
        m_photo.capture.selectedTheme = GetNextPhotoFilterTheme(m_photo.capture.selectedTheme);
    }

    const bool blockFilterChange = m_photo.placement.active || m_flow.cameraMode;
    if (!blockFilterChange)
    {
        if (Input_IsRightShoulderPressed())
        {
            m_photo.capture.selectedTheme = GetNextPhotoFilterTheme(m_photo.capture.selectedTheme);
        }
        else if (Input_IsLeftShoulderPressed())
        {
            switch (m_photo.capture.selectedTheme)
            {
            case PhotoFilterTheme::None:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
                break;
            case PhotoFilterTheme::Hot:
                m_photo.capture.selectedTheme = PhotoFilterTheme::None;
                break;
            case PhotoFilterTheme::Cold:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Hot;
                break;
            case PhotoFilterTheme::Invert:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Cold;
                break;
            case PhotoFilterTheme::Sepia:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Invert;
                break;
            }
        }
    }
}

void GameScene::UpdateTuningHotReload(float deltaTime)
{
    m_debug.tuningReloadTimer = std::max(0.0f, m_debug.tuningReloadTimer - deltaTime);
    if (m_debug.tuningReloadTimer > 0.0f)
    {
        return;
    }

    m_debug.tuningReloadTimer = 0.25f;
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
    if (!ec && (!m_debug.hasTuningFileWriteTime || writeTime != m_debug.tuningFileWriteTime))
    {
        m_debug.tuningFileWriteTime = writeTime;
        m_debug.hasTuningFileWriteTime = true;
        LoadTuningJsonFile();
    }
}

void GameScene::HandleGlobalSceneShortcuts()
{
    if (Input_IsActionPressed(InputAction::ReturnToTitle))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::RestartScene))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::ToggleTuningPanel))
    {
        m_debug.showTuningPanel = !m_debug.showTuningPanel;
    }
    if (Input_IsActionPressed(InputAction::ToggleCollisionDebug))
    {
        m_debug.showCollisionDebug = !m_debug.showCollisionDebug;
    }
}

bool GameScene::UpdatePitRestartFlow(float deltaTime)
{
    if (!m_flow.pitRestartActive)
    {
        return false;
    }

    m_flow.pitRestartTimer = std::max(0.0f, m_flow.pitRestartTimer - deltaTime);
    if (m_flow.pitRestartTimer > 0.0f)
    {
        return true;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    if (player)
    {
        RespawnPlayer(*player);
    }
    else
    {
        m_flow.pitRestartActive = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    return true;
}

bool GameScene::UpdateStageTransitionFlow(float deltaTime)
{
    if (!m_flow.stageTransitionActive)
    {
        return false;
    }

    m_flow.stageTransitionTimer = std::max(0.0f, m_flow.stageTransitionTimer - deltaTime);
    if (m_flow.stageTransitionTimer > 0.0f)
    {
        return true;
    }

    const bool transitioned = m_hasPendingStageTransition &&
        ExecuteStageTransition(
            m_pendingStageTransitionMapCsv,
            m_pendingStageTransitionSpawnMarker,
            m_pendingStageTransitionMarker);
    m_hasPendingStageTransition = false;
    m_pendingStageTransitionMapCsv.clear();
    m_pendingStageTransitionSpawnMarker = '\0';
    m_pendingStageTransitionMarker = '\0';
    m_flow.stageTransitionActive = false;
    m_flow.stageTransitionTimer = 0.0f;
    m_flow.stageTransitionFadeInTimer = transitioned ? kStageTransitionFadeInDuration : 0.0f;
    return true;
}

void GameScene::UpdateFrameTimers(float deltaTime, float gameplayDeltaTime, float effectiveGameplayDeltaTime)
{
    m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - effectiveGameplayDeltaTime);
    m_flow.shutterFlashRemaining = std::max(0.0f, m_flow.shutterFlashRemaining - deltaTime);
    m_flow.pitRestartFadeInTimer = std::max(0.0f, m_flow.pitRestartFadeInTimer - deltaTime);
    m_flow.stageTransitionFadeInTimer = std::max(0.0f, m_flow.stageTransitionFadeInTimer - deltaTime);
    const bool previewWasActive = m_flow.developedPhotoPreviewRemaining > 0.0f;
    m_flow.developedPhotoPreviewRemaining = std::max(0.0f, m_flow.developedPhotoPreviewRemaining - deltaTime);
    if (previewWasActive && m_flow.developedPhotoPreviewRemaining <= 0.0f)
    {
        CommitPendingCapturedPhoto();
    }
    m_flow.pickupPulse += gameplayDeltaTime;
}

void GameScene::RunGameplayFrame(float gameplayDeltaTime)
{
    UpdatePlayer(gameplayDeltaTime);
    HandlePhotoCapture();
    HandlePhotoSpawn();
    UpdateBarrels(gameplayDeltaTime);
    UpdateEnemies();
    UpdateBullets();
    UpdateDropItems(); // Legacy update order: drop item step
    UpdateGoalVisual(gameplayDeltaTime);
    HandleWorldInteractions();
    RemoveDefeatedEnemies();
    UpdateEffects(gameplayDeltaTime);

    // Flush entities queued during gameplay update
    for (auto& entity : m_pendingEntities)
    {
        m_entities.push_back(std::move(entity));
    }
    m_pendingEntities.clear();
}

