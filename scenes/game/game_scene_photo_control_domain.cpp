#include "pch.h"

#include "game_scene_internal.h"
#include "imgui_layer.h"
#include "DxLib.h"

#include <algorithm>

using namespace game_scene_detail;

namespace
{
    constexpr float kPhotoFocusTimeScale = 0.22f;
    constexpr float kPlacementFocusDuration = 1.2f;

    PhotoFilterTheme ResolveCameraFilterHudTheme(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Cold:
            return GameSession_Get().hasRecoveryFilter ? PhotoFilterTheme::Cold : PhotoFilterTheme::None;
        case PhotoFilterTheme::Sepia:
            return PhotoFilterTheme::Sepia;
        case PhotoFilterTheme::None:
        case PhotoFilterTheme::Hot:
        case PhotoFilterTheme::Invert:
        default:
            return PhotoFilterTheme::None;
        }
    }

    bool IsSelectableFilterTheme(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::None:
        case PhotoFilterTheme::Sepia:
            return true;
        case PhotoFilterTheme::Cold:
            return GameSession_Get().hasRecoveryFilter;
        case PhotoFilterTheme::Hot:
        case PhotoFilterTheme::Invert:
        default:
            return false;
        }
    }

    PhotoFilterTheme NormalizeSelectableFilterTheme(PhotoFilterTheme theme)
    {
        return IsSelectableFilterTheme(theme) ? theme : PhotoFilterTheme::None;
    }

    PhotoFilterTheme GetNextSelectableFilterTheme(PhotoFilterTheme current)
    {
        const bool hasRecoveryFilter = GameSession_Get().hasRecoveryFilter;
        switch (NormalizeSelectableFilterTheme(current))
        {
        case PhotoFilterTheme::None:
            return hasRecoveryFilter ? PhotoFilterTheme::Cold : PhotoFilterTheme::Sepia;
        case PhotoFilterTheme::Cold:
            return PhotoFilterTheme::Sepia;
        case PhotoFilterTheme::Sepia:
        default:
            return PhotoFilterTheme::None;
        }
    }

    PhotoFilterTheme GetPreviousSelectableFilterTheme(PhotoFilterTheme current)
    {
        const bool hasRecoveryFilter = GameSession_Get().hasRecoveryFilter;
        switch (NormalizeSelectableFilterTheme(current))
        {
        case PhotoFilterTheme::None:
            return PhotoFilterTheme::Sepia;
        case PhotoFilterTheme::Sepia:
            return hasRecoveryFilter ? PhotoFilterTheme::Cold : PhotoFilterTheme::None;
        case PhotoFilterTheme::Cold:
        default:
            return PhotoFilterTheme::None;
        }
    }
}

void GameScene::UpdateCameraMode()
{
    m_flow.cameraMode = false;
    m_player.captureAnimationActive = false;
    m_player.captureAnimationReleased = false;
}

float GameScene::UpdatePhotoModes(float deltaTime)
{
    UpdateCameraMode();
    UpdateCaptureFinderZoomInput();
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
    m_ui.photoTrayReveal += (trayTarget - m_ui.photoTrayReveal) * std::min(1.0f, deltaTime * m_ui.tuning.photoTray.revealSpeed);
    if (showPhotoTray)
    {
        UpdatePhotoTraySelection();
    }
    m_flow.captureModeZoomBlend += (0.0f - m_flow.captureModeZoomBlend) * std::min(1.0f, deltaTime * m_ui.tuning.captureFinder.zoomBlendResponse);
    m_flow.captureSlowRemaining = 0.0f;
    m_flow.placementSlowRemaining = placementActive ? kPlacementFocusDuration : 0.0f;
    const bool slowForPlacement = placementActive;
    // フォーカス中だけゲーム全体を減速させる。
    return slowForPlacement
        ? deltaTime * kPhotoFocusTimeScale
        : deltaTime;
}

void GameScene::UpdateCaptureFinderZoomInput()
{
    if (m_photo.placement.active || m_player.pasteAnimationActive)
    {
        return;
    }

    if (ImGuiLayer_WantsCaptureMouse())
    {
        return;
    }

    int zoomDirection = 0;
    const int wheelDelta = Input_GetMouseWheelDelta();
    if (wheelDelta > 0)
    {
        ++zoomDirection;
    }
    if (wheelDelta < 0)
    {
        --zoomDirection;
    }

    if (zoomDirection != 0)
    {
        m_ui.captureFinderScale = std::clamp(
            m_ui.captureFinderScale + static_cast<float>(zoomDirection) * m_ui.tuning.captureFinder.scaleStep,
            m_ui.tuning.captureFinder.scaleMin,
            m_ui.tuning.captureFinder.scaleMax);
    }
}

void GameScene::ProcessFilterInput()
{
    m_photo.capture.selectedTheme = NormalizeSelectableFilterTheme(m_photo.capture.selectedTheme);

    if (!m_ui.cameraFilterHudInitialized)
    {
        const PhotoFilterTheme initialHudTheme = ResolveCameraFilterHudTheme(m_photo.capture.selectedTheme);
        m_ui.cameraFilterHudTheme = initialHudTheme;
        m_ui.cameraFilterLastSelectedTheme = initialHudTheme;
        m_ui.cameraFilterAnimationFrom = initialHudTheme;
        m_ui.cameraFilterAnimationTo = initialHudTheme;
        m_ui.cameraFilterAnimationElapsed = 1.0f;
        m_ui.cameraFilterHudInitialized = true;
    }

    const PhotoFilterTheme previousTheme = m_photo.capture.selectedTheme;

    if (Input_IsActionPressed(InputAction::SelectFilterNone))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::None;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterHot))
    {
        if (IsSelectableFilterTheme(PhotoFilterTheme::Hot))
        {
            m_photo.capture.selectedTheme = PhotoFilterTheme::Hot;
        }
    }
    if (Input_IsActionPressed(InputAction::SelectFilterCold))
    {
        if (IsSelectableFilterTheme(PhotoFilterTheme::Cold))
        {
            m_photo.capture.selectedTheme = PhotoFilterTheme::Cold;
        }
    }
    if (Input_IsActionPressed(InputAction::SelectFilterInvert))
    {
        if (IsSelectableFilterTheme(PhotoFilterTheme::Invert))
        {
            m_photo.capture.selectedTheme = PhotoFilterTheme::Invert;
        }
    }
    if (Input_IsActionPressed(InputAction::SelectFilterSepia))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
    }
    if (Input_IsActionPressed(InputAction::CycleFilter))
    {
        m_photo.capture.selectedTheme = GetNextSelectableFilterTheme(m_photo.capture.selectedTheme);
    }

    const bool blockFilterChange = m_photo.placement.active;
    if (!blockFilterChange)
    {
        if (Input_IsRightShoulderPressed())
        {
            m_photo.capture.selectedTheme = GetNextSelectableFilterTheme(m_photo.capture.selectedTheme);
        }
        else if (Input_IsLeftShoulderPressed())
        {
            m_photo.capture.selectedTheme = GetPreviousSelectableFilterTheme(m_photo.capture.selectedTheme);
        }
    }

    if (m_photo.capture.selectedTheme != previousTheme)
    {
        const PhotoFilterTheme targetHudTheme = ResolveCameraFilterHudTheme(m_photo.capture.selectedTheme);
        if (targetHudTheme != m_ui.cameraFilterHudTheme)
        {
            m_ui.cameraFilterAnimationFrom = m_ui.cameraFilterHudTheme;
            m_ui.cameraFilterAnimationTo = targetHudTheme;
            m_ui.cameraFilterAnimationElapsed = 0.0f;
        }
        else
        {
            m_ui.cameraFilterAnimationElapsed = 1.0f;
            m_ui.cameraFilterHudTheme = targetHudTheme;
        }
        m_ui.cameraFilterLastSelectedTheme = targetHudTheme;
    }
}

