#include "pch.h"

#include "game_scene_internal.h"
#include "DxLib.h"

#include <algorithm>

using namespace game_scene_detail;

namespace
{
    constexpr float kPhotoFocusTimeScale = 0.22f;
    constexpr float kCaptureFocusDuration = 0.8f;
    constexpr float kPlacementFocusDuration = 1.2f;
    constexpr float kCaptureFinderScaleMin = 1.0f;
    constexpr float kCaptureFinderScaleMax = 2.0f;
    constexpr float kCaptureFinderScaleStep = 0.1f;
    constexpr float kCaptureModeZoomResponse = 7.0f;
}

void GameScene::UpdateCameraMode()
{
    if (m_flow.cameraMode)
    {
        ++m_flow.cameraModeSessionId;
        m_flow.cameraMode = false;
        m_player.captureAnimationActive = false;
        m_player.captureAnimationReleased = false;
    }
}

float GameScene::UpdatePhotoModes(float deltaTime)
{
    UpdateCameraMode();
    m_ui.captureRapidTimer = std::max(0.0f, m_ui.captureRapidTimer - deltaTime);
    m_ui.captureLockoutRemaining = std::max(0.0f, m_ui.captureLockoutRemaining - deltaTime);
    if (m_ui.captureRapidTimer <= 0.0f)
    {
        m_ui.captureRapidCount = 0;
    }

    // 3状態（撮影/配置/現像プレビュー）から、トレイ表示とスロー演出を一元決定する。
    const bool placementActive = m_photo.placement.active;
    const bool previewActive = m_photo.pendingStore.active && m_ui.developedPhotoPreviewRemaining > 0.0f;
    const bool previewOrbAttached =
        previewActive &&
        m_ui.developedPhotoPreviewRemaining <= 0.34f;
    const bool showPhotoTray = true;
    const float trayTarget = 1.0f;
    m_ui.photoTrayReveal += (trayTarget - m_ui.photoTrayReveal) * std::min(1.0f, deltaTime * 12.0f);
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
    if (Input_IsKeyDown(VK_RBUTTON) || m_photo.placement.active)
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
        m_ui.captureFinderScale = std::clamp(
            m_ui.captureFinderScale + static_cast<float>(zoomDirection) * kCaptureFinderScaleStep,
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

