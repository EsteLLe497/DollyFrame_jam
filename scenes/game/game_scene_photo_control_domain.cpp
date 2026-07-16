#include "pch.h"

#include "game_scene_internal.h"
#include "imgui_layer.h"
#include "DxLib.h"

#include <algorithm>
#include <cmath>

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
            return GameSession_Get().hasSepiaFilter ? PhotoFilterTheme::Sepia : PhotoFilterTheme::None;
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
            return true;
        case PhotoFilterTheme::Sepia:
            return GameSession_Get().hasSepiaFilter;
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
        const bool hasSepiaFilter = GameSession_Get().hasSepiaFilter;
        switch (NormalizeSelectableFilterTheme(current))
        {
        case PhotoFilterTheme::None:
            if (hasRecoveryFilter)
            {
                return PhotoFilterTheme::Cold;
            }
            return hasSepiaFilter ? PhotoFilterTheme::Sepia : PhotoFilterTheme::None;
        case PhotoFilterTheme::Cold:
            return hasSepiaFilter ? PhotoFilterTheme::Sepia : PhotoFilterTheme::None;
        case PhotoFilterTheme::Sepia:
        default:
            return PhotoFilterTheme::None;
        }
    }

    PhotoFilterTheme GetPreviousSelectableFilterTheme(PhotoFilterTheme current)
    {
        const bool hasRecoveryFilter = GameSession_Get().hasRecoveryFilter;
        const bool hasSepiaFilter = GameSession_Get().hasSepiaFilter;
        switch (NormalizeSelectableFilterTheme(current))
        {
        case PhotoFilterTheme::None:
            if (hasSepiaFilter)
            {
                return PhotoFilterTheme::Sepia;
            }
            return hasRecoveryFilter ? PhotoFilterTheme::Cold : PhotoFilterTheme::None;
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
    UpdateCaptureFinderCursor();
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

void GameScene::UpdateCaptureFinderCursor()
{
    // ファインダーの仮想カーソルをスクリーン座標で毎フレーム1回だけ進める。
    // GetCaptureFrameRect は描画・撮影判定で1フレームに複数回呼ばれるため、
    // 積分（速度×dt）はここに集約し、GetCaptureFrameRect は結果を読むだけにする。
    // スクリーン座標保持なので、カメラ（プレイヤー）が動いても画面上の位置は保たれ、
    // 撮影対象は毎フレーム現在のカメラでワールド変換される＝プレイヤーに追従する。
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();

    if (!m_ui.finderCursorInitialized)
    {
        m_ui.finderCursorScreenX = static_cast<float>(mouseX);
        m_ui.finderCursorScreenY = static_cast<float>(mouseY);
        m_ui.finderCursorVelocityX = 0.0f;
        m_ui.finderCursorVelocityY = 0.0f;
        m_ui.finderCursorLastMouseX = mouseX;
        m_ui.finderCursorLastMouseY = mouseY;
        m_ui.finderCursorLastTimeMs = static_cast<unsigned int>(GetNowCount());
        m_ui.finderCursorInitialized = true;
        return;
    }

    const unsigned int nowMs = static_cast<unsigned int>(GetNowCount());
    const float dt = m_ui.finderCursorLastTimeMs
        ? (static_cast<float>(nowMs - m_ui.finderCursorLastTimeMs) / 1000.0f)
        : (1.0f / 60.0f);
    m_ui.finderCursorLastTimeMs = nowMs;

    const bool mouseMoved = mouseX != m_ui.finderCursorLastMouseX || mouseY != m_ui.finderCursorLastMouseY;
    m_ui.finderCursorLastMouseX = mouseX;
    m_ui.finderCursorLastMouseY = mouseY;

    // マウスが動いたら仮想カーソルをマウスに吸着させ、パッド速度をリセットする
    // （マウスとパッドの切替が自然になる）。以降は生マウス座標に完全追従（元の挙動）。
    if (mouseMoved)
    {
        m_ui.finderCursorScreenX = static_cast<float>(mouseX);
        m_ui.finderCursorScreenY = static_cast<float>(mouseY);
        m_ui.finderCursorVelocityX = 0.0f;
        m_ui.finderCursorVelocityY = 0.0f;
        m_ui.finderCursorPadDriving = false;
        return;
    }

    const GameScenePadCursorTuning& padTuning = m_ui.padCursor;
    const float rightX = Input_GetRightStickX();
    const float rightY = Input_GetRightStickY();
    const float magnitude = std::sqrt(rightX * rightX + rightY * rightY);
    const bool padActive = Input_IsGamepadConnected() && magnitude > padTuning.deadZone;
    if (padActive)
    {
        const float normalizedMagnitude = std::clamp((magnitude - padTuning.deadZone) / std::max(0.0001f, 1.0f - padTuning.deadZone), 0.0f, 1.0f);
        const float curvedMagnitude = normalizedMagnitude * normalizedMagnitude;
        const float scale = curvedMagnitude / magnitude;
        const float desiredVelocityX = rightX * scale * padTuning.maxSpeed;
        const float desiredVelocityY = rightY * scale * padTuning.maxSpeed;
        const float response = std::min(1.0f, dt * padTuning.response);
        m_ui.finderCursorVelocityX += (desiredVelocityX - m_ui.finderCursorVelocityX) * response;
        m_ui.finderCursorVelocityY += (desiredVelocityY - m_ui.finderCursorVelocityY) * response;
        m_ui.finderCursorPadDriving = true;
    }
    else
    {
        // スティックを離したらその場に留まる（マウス位置への引き戻しはしない）。
        // 速度だけ減衰させて滑らかに停止する。マウス操作へ戻すにはマウスを動かせばよい。
        const float damping = std::max(0.0f, 1.0f - dt * padTuning.damping);
        m_ui.finderCursorVelocityX *= damping;
        m_ui.finderCursorVelocityY *= damping;
    }

    m_ui.finderCursorScreenX += m_ui.finderCursorVelocityX * dt;
    m_ui.finderCursorScreenY += m_ui.finderCursorVelocityY * dt;

    // 画面外へ暴走しないよう画面内に収める（マウスは常に画面内なので影響なし）。
    m_ui.finderCursorScreenX = std::clamp(m_ui.finderCursorScreenX, 0.0f, static_cast<float>(SCREEN_WIDTH));
    m_ui.finderCursorScreenY = std::clamp(m_ui.finderCursorScreenY, 0.0f, static_cast<float>(SCREEN_HEIGHT));
}

void GameScene::GetActivePadCursorScreen(float& screenX, float& screenY) const
{
    // パッド操作中のみ共有仮想カーソル、それ以外は生マウス座標。
    if (m_ui.finderCursorInitialized && m_ui.finderCursorPadDriving)
    {
        screenX = m_ui.finderCursorScreenX;
        screenY = m_ui.finderCursorScreenY;
    }
    else
    {
        screenX = static_cast<float>(Input_GetMouseX());
        screenY = static_cast<float>(Input_GetMouseY());
    }
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
        if (IsSelectableFilterTheme(PhotoFilterTheme::Sepia))
        {
            m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
        }
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

