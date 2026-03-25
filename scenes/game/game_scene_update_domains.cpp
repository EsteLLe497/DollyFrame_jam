#include "game_scene_internal.h"
#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPhotoFocusTimeScale = 0.22f;
    constexpr float kCaptureFocusDuration = 0.8f;
    constexpr float kPlacementFocusDuration = 1.2f;
    constexpr float kCaptureFinderScaleMin = 1.0f;
    constexpr float kCaptureFinderScaleMax = 2.0f;
    constexpr float kCaptureFinderScaleStep = 0.1f;
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

    const bool placementHeld = !m_flow.cameraMode && m_photo.capture.hasPhoto && Input_IsActionDown(InputAction::HoldPlacement);
    const bool previewActive = m_photo.pendingStore.active && m_flow.developedPhotoPreviewRemaining > 0.0f;
    const bool previewOrbAttached =
        previewActive &&
        m_flow.developedPhotoPreviewRemaining <= 0.34f;
    const bool showPhotoTray = (previewActive && !previewOrbAttached) || m_flow.cameraMode || placementHeld || m_photo.placement.active;
    const float trayTarget = showPhotoTray ? 1.0f : 0.0f;
    m_flow.photoTrayReveal += (trayTarget - m_flow.photoTrayReveal) * std::min(1.0f, deltaTime * 12.0f);
    if (showPhotoTray)
    {
        UpdatePhotoTraySelection();
    }
    if (Input_IsActionPressed(InputAction::HoldCamera))
    {
        m_flow.captureSlowRemaining = kCaptureFocusDuration;
    }
    if (!m_flow.cameraMode && m_photo.capture.hasPhoto && Input_IsActionPressed(InputAction::HoldPlacement))
    {
        m_flow.placementSlowRemaining = kPlacementFocusDuration;
    }

    m_flow.captureSlowRemaining = std::max(0.0f, m_flow.captureSlowRemaining - deltaTime);
    m_flow.placementSlowRemaining = std::max(0.0f, m_flow.placementSlowRemaining - deltaTime);
    const bool slowForCapture = m_flow.cameraMode && m_flow.captureSlowRemaining > 0.0f;
    const bool slowForPlacement = placementHeld && m_flow.placementSlowRemaining > 0.0f;
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

