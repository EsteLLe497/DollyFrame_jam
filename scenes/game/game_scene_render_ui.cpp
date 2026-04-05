#include "game_scene_internal.h"
#include "photo_system.h"
#include "photo_shared.h"
#include "photo_filter_rules.h"

#include <cctype>

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPadDeadZone = 0.18f;
    constexpr float kPadCursorMaxSpeed = 920.0f;
    constexpr float kPadCursorResponse = 14.0f;
    constexpr float kPadCursorDamping = 10.0f;
    constexpr float kPadCursorMouseReturnDelay = 0.28f;
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kStageTransitionFadeOutDuration = 0.45f;
    constexpr float kStageTransitionFadeInDuration = 1.10f;

    std::string GetMapDisplayName(const std::string& path)
    {
        const size_t slashPos = path.find_last_of("/\\");
        const std::string fileName = slashPos == std::string::npos ? path : path.substr(slashPos + 1);
        const size_t dotPos = fileName.find_last_of('.');
        return dotPos == std::string::npos ? fileName : fileName.substr(0, dotPos);
    }

    void UpdatePadCursor(
        float mouseWorldX,
        float mouseWorldY,
        bool mouseMoved,
        float rightX,
        float rightY,
        float dt,
        float& cursorWorldX,
        float& cursorWorldY,
        float& velocityX,
        float& velocityY,
        float& lastPadInputSeconds,
        float nowSeconds)
    {
        if (mouseMoved)
        {
            cursorWorldX = mouseWorldX;
            cursorWorldY = mouseWorldY;
            velocityX = 0.0f;
            velocityY = 0.0f;
            lastPadInputSeconds = -1000.0f;
            return;
        }

        const float magnitude = std::sqrt(rightX * rightX + rightY * rightY);
        const bool padActive = Input_IsGamepadConnected() && magnitude > kPadDeadZone;
        if (padActive)
        {
            const float normalizedMagnitude = std::clamp((magnitude - kPadDeadZone) / (1.0f - kPadDeadZone), 0.0f, 1.0f);
            const float curvedMagnitude = normalizedMagnitude * normalizedMagnitude;
            const float scale = curvedMagnitude / magnitude;
            const float desiredVelocityX = rightX * scale * kPadCursorMaxSpeed;
            const float desiredVelocityY = rightY * scale * kPadCursorMaxSpeed;
            const float response = std::min(1.0f, dt * kPadCursorResponse);
            velocityX += (desiredVelocityX - velocityX) * response;
            velocityY += (desiredVelocityY - velocityY) * response;
            lastPadInputSeconds = nowSeconds;
        }
        else
        {
            const float damping = std::max(0.0f, 1.0f - dt * kPadCursorDamping);
            velocityX *= damping;
            velocityY *= damping;

            if (nowSeconds - lastPadInputSeconds >= kPadCursorMouseReturnDelay)
            {
                const float returnFactor = std::min(1.0f, dt * 6.0f);
                cursorWorldX += (mouseWorldX - cursorWorldX) * returnFactor;
                cursorWorldY += (mouseWorldY - cursorWorldY) * returnFactor;
            }
        }

        cursorWorldX += velocityX * dt;
        cursorWorldY += velocityY * dt;
    }

    constexpr int kPhotoTraySlotCount = 3;
    constexpr float kPhotoTraySlotWidth = 170.0f;
    constexpr float kPhotoTraySlotHeight = 92.0f;
    constexpr float kPhotoTraySlotGap = 18.0f;
    constexpr float kTuningPanelX = 24.0f;
    constexpr float kTuningPanelY = 24.0f;
    constexpr float kTuningPanelWidth = 460.0f;
    constexpr float kTuningPanelHeight = 620.0f;
    constexpr float kTuningRowStartY = 124.0f;
    constexpr float kTuningRowHeight = 22.0f;
    constexpr float kTuningSectionGap = 14.0f;
    constexpr float kTuningSectionHeaderHeight = 24.0f;
    constexpr float kTuningMinusButtonX = 314.0f;
    constexpr float kTuningPlusButtonX = 390.0f;
    constexpr float kTuningButtonWidth = 52.0f;
    constexpr float kTuningButtonHeight = 18.0f;

    void RotatePoint(float centerX, float centerY, float rotation, float& x, float& y)
    {
        if (std::fabs(rotation) <= 0.0001f)
        {
            return;
        }

        const float localX = x - centerX;
        const float localY = y - centerY;
        const float cosTheta = std::cos(rotation);
        const float sinTheta = std::sin(rotation);
        x = centerX + (localX * cosTheta - localY * sinTheta);
        y = centerY + (localX * sinTheta + localY * cosTheta);
    }

    void DrawTriangleItem(
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        bool risesRight,
        bool flipX,
        float rotation,
        int color)
    {
        const bool finalRisesRight = flipX ? !risesRight : risesRight;
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;

        if (finalRisesRight)
        {
            ax = drawX;
            ay = drawY + drawHeight;
            bx = drawX + drawWidth;
            by = drawY + drawHeight;
            cx = drawX + drawWidth;
            cy = drawY;
        }
        else
        {
            ax = drawX;
            ay = drawY;
            bx = drawX;
            by = drawY + drawHeight;
            cx = drawX + drawWidth;
            cy = drawY + drawHeight;
        }

        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
    }
    void DrawProjectileItem(
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        bool flipX,
        float rotation,
        int color)
    {
        float ax = flipX ? drawX + drawWidth : drawX;
        float ay = drawY;
        float bx = flipX ? drawX + drawWidth : drawX;
        float by = drawY + drawHeight;
        float cx = flipX ? drawX : drawX + drawWidth;
        float cy = drawY + drawHeight * 0.5f;

        const float centerX = drawX + drawWidth * 0.5f;
        const float centerY = drawY + drawHeight * 0.5f;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
    }

    const char* GetStageGuideText(float playerX)
    {
        static_cast<void>(playerX);
        return "Sandbox: choose filter 1-4, capture, then place up to three copy groups.";
    }

    void DrawCapturedPreviewItem(
        int fallbackTextureId,
        const CapturedPhotoItem& item,
        float drawX,
        float drawY,
        float drawWidth,
        float drawHeight,
        float alpha)
    {
        Shader_ResetStyle();
        Shader_SetTint(item.tintR, item.tintG, item.tintB, std::min(1.0f, item.tintA) * alpha);
        if (item.spawnArchetype == CapturedSpawnArchetype::Projectile)
        {
            const float projectileAngle = std::atan2(item.projectileVelocityY, item.projectileVelocityX);
            const int color = GetColor(
                static_cast<int>(std::round(item.tintR * 255.0f)),
                static_cast<int>(std::round(item.tintG * 255.0f)),
                static_cast<int>(std::round(item.tintB * 255.0f)));
            DrawProjectileItem(
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                item.flipX,
                projectileAngle,
                color);
            return;
        }

        if (photo_shared::DrawDamagePlatformItemPreview(
                item,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                std::min(1.0f, item.tintA) * alpha))
        {
            return;
        }

        if (photo_shared::DrawSpikeStripItemPreview(
                item,
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                std::min(1.0f, item.tintA) * alpha))
        {
            return;
        }

        const TileTriangleShape triangle = TileMap::GetTriangleShape(item.sourceTileValue);
        if (triangle.isTriangle)
        {
            const int color = GetColor(
                static_cast<int>(std::round(item.tintR * 255.0f)),
                static_cast<int>(std::round(item.tintG * 255.0f)),
                static_cast<int>(std::round(item.tintB * 255.0f)));
            DrawTriangleItem(
                drawX,
                drawY,
                drawWidth,
                drawHeight,
                triangle.risesRight,
                item.flipX,
                item.rotation,
                color);
            return;
        }

        SpriteDraw(
            item.textureId >= 0 ? item.textureId : fallbackTextureId,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.sourceX,
            item.sourceY,
            item.sourceWidth,
            item.sourceHeight,
            item.flipX,
            item.rotation);
    }

    float GetTuningRowY(int index)
    {
        float y = kTuningPanelY + kTuningRowStartY;
        if (index >= 2)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        if (index >= 12)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        return y + static_cast<float>(index) * kTuningRowHeight;
    }

    void DrawTuningButton(float x, float y, float width, float height, const char* label, bool highlighted)
    {
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + width)),
            static_cast<int>(std::round(y + height)),
            highlighted ? GetColor(228, 196, 120) : GetColor(48, 60, 78),
            TRUE);
        DrawBox(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(x + width)),
            static_cast<int>(std::round(y + height)),
            GetColor(220, 230, 244),
            FALSE);
        DrawString(
            static_cast<int>(std::round(x + 18.0f)),
            static_cast<int>(std::round(y + 2.0f)),
            label,
            highlighted ? GetColor(18, 18, 22) : GetColor(232, 238, 245));
    }

    float SmoothStep01(float t)
    {
        const float x = Clamp01(t);
        return x * x * (3.0f - 2.0f * x);
    }
}

void GameScene::DrawPitRestartOverlay() const
{
    const bool hasPitFade = m_flow.pitRestartActive || m_flow.pitRestartFadeInTimer > 0.0f;
    const bool hasStageTransitionFade = m_flow.stageTransitionActive || m_flow.stageTransitionFadeInTimer > 0.0f;
    if (!hasPitFade && !hasStageTransitionFade)
    {
        return;
    }

    float pitAlpha = 0.0f;
    if (m_flow.pitRestartActive)
    {
        const float progress = Clamp01(1.0f - (m_flow.pitRestartTimer / kPitRestartFadeDuration));
        pitAlpha = 0.35f + progress * 0.65f; // Fade-out to black before respawn.
    }
    else if (m_flow.pitRestartFadeInTimer > 0.0f)
    {
        const float progress = Clamp01(m_flow.pitRestartFadeInTimer / kPitRestartFadeDuration);
        pitAlpha = progress; // Fade-in from black after respawn.
    }

    float stageTransitionAlpha = 0.0f;
    if (m_flow.stageTransitionActive)
    {
        const float progress = Clamp01(1.0f - (m_flow.stageTransitionTimer / kStageTransitionFadeOutDuration));
        stageTransitionAlpha = 0.35f + progress * 0.65f;
    }
    else if (m_flow.stageTransitionFadeInTimer > 0.0f)
    {
        const float elapsed = Clamp01(1.0f - (m_flow.stageTransitionFadeInTimer / kStageTransitionFadeInDuration));
        stageTransitionAlpha = 1.0f - SmoothStep01(elapsed);
    }

    const float alpha = std::max(pitAlpha, stageTransitionAlpha);

    Shader_ResetStyle();
    Shader_SetTint(0.01f, 0.01f, 0.02f, alpha);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_ResetStyle();
}

void GameScene::DrawCaptureOverlay() const
{
    if (!m_flow.cameraMode && m_flow.shutterFlashRemaining <= 0.0f)
    {
        return;
    }

    const Entity* player = FindEntityByTag(kTagPlayer);
    if (!player)
    {
        return;
    }

    const auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();
    const float drawX = viewOriginX + (frameX - m_flow.cameraX) * viewScale;
    const float drawY = viewOriginY + (frameY - m_flow.cameraY) * viewScale;
    const float drawWidth = frameWidth * viewScale;
    const float drawHeight = frameHeight * viewScale;
    const int left = static_cast<int>(std::round(drawX));
    const int top = static_cast<int>(std::round(drawY));
    const int right = static_cast<int>(std::round(drawX + drawWidth));
    const int bottom = static_cast<int>(std::round(drawY + drawHeight));

    const float shutterT = Clamp01(m_flow.shutterFlashRemaining / gShutterFlashSeconds);
    const float frameInset = 10.0f * shutterT * viewScale;
    const float innerX = drawX + frameInset;
    const float innerY = drawY + frameInset;
    const float innerWidth = std::max(8.0f, drawWidth - frameInset * 2.0f);
    const float innerHeight = std::max(8.0f, drawHeight - frameInset * 2.0f);
    float overlayR = 1.0f;
    float overlayG = 1.0f;
    float overlayB = 1.0f;
    GetPhotoFilterThemeOverlayColor(m_photo.capture.selectedTheme, overlayR, overlayG, overlayB);
    const unsigned int frameColor = GetColor(
        static_cast<int>(std::round(overlayR * 255.0f)),
        static_cast<int>(std::round(overlayG * 255.0f)),
        static_cast<int>(std::round(overlayB * 255.0f)));
    const unsigned int guideColor = GetColor(
        static_cast<int>(std::round(overlayR * 200.0f)),
        static_cast<int>(std::round(overlayG * 200.0f)),
        static_cast<int>(std::round(overlayB * 200.0f)));

    const auto drawVignetteBand = [&](float x, float y, float width, float height, float alpha, float r, float g, float b)
    {
        Shader_ResetStyle();
        Shader_SetTint(r, g, b, alpha);
        SpriteDraw(m_whiteTexture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
    };

    const auto drawFrameBand = [&](float x, float y, float width, float height, float alpha)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, alpha);
        SpriteDraw(m_whiteTexture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f);
    };

    const auto drawCornerFrame = [&](int frameLeft, int frameTop, int frameRight, int frameBottom, int thickness, int cornerLength, unsigned int color)
    {
        for (int offset = 0; offset < thickness; ++offset)
        {
            DrawLine(frameLeft, frameTop + offset, frameLeft + cornerLength, frameTop + offset, color);
            DrawLine(frameLeft + offset, frameTop, frameLeft + offset, frameTop + cornerLength, color);
            DrawLine(frameRight - cornerLength, frameTop + offset, frameRight, frameTop + offset, color);
            DrawLine(frameRight - offset, frameTop, frameRight - offset, frameTop + cornerLength, color);
            DrawLine(frameLeft, frameBottom - offset, frameLeft + cornerLength, frameBottom - offset, color);
            DrawLine(frameLeft + offset, frameBottom - cornerLength, frameLeft + offset, frameBottom, color);
            DrawLine(frameRight - cornerLength, frameBottom - offset, frameRight, frameBottom - offset, color);
            DrawLine(frameRight - offset, frameBottom - cornerLength, frameRight - offset, frameBottom, color);
        }
    };

    const int cornerLength = std::max(18, static_cast<int>(std::round(34.0f * viewScale)));
    const int cornerThickness = std::max(2, static_cast<int>(std::round(3.0f + shutterT * 2.0f)));
    const int guideInset = std::max(12, static_cast<int>(std::round(24.0f * viewScale)));

    if (m_flow.cameraMode)
    {
        const float vignetteAlpha = 0.15f + shutterT * 0.08f;
        const float edge0 = 34.0f * viewScale;
        const float edge1 = 72.0f * viewScale;
        const float edge2 = 118.0f * viewScale;
        const float edge3 = 164.0f * viewScale;
        const float topBottomBoost = 1.22f;
        drawVignetteBand(viewOriginX, viewOriginY, viewWidth, edge3, vignetteAlpha * topBottomBoost, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(viewOriginX, viewOriginY + viewHeight - edge3, viewWidth, edge3, vignetteAlpha * topBottomBoost, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(viewOriginX, viewOriginY, edge3, viewHeight, vignetteAlpha, 0.01f, 0.015f, 0.025f);
        drawVignetteBand(viewOriginX + viewWidth - edge3, viewOriginY, edge3, viewHeight, vignetteAlpha, 0.01f, 0.015f, 0.025f);

        drawVignetteBand(viewOriginX, viewOriginY, viewWidth, edge2, vignetteAlpha * 0.82f * topBottomBoost, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(viewOriginX, viewOriginY + viewHeight - edge2, viewWidth, edge2, vignetteAlpha * 0.82f * topBottomBoost, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(viewOriginX, viewOriginY, edge2, viewHeight, vignetteAlpha * 0.82f, 0.02f, 0.02f, 0.035f);
        drawVignetteBand(viewOriginX + viewWidth - edge2, viewOriginY, edge2, viewHeight, vignetteAlpha * 0.82f, 0.02f, 0.02f, 0.035f);

        drawVignetteBand(viewOriginX, viewOriginY, viewWidth, edge1, vignetteAlpha * 0.60f * topBottomBoost, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(viewOriginX, viewOriginY + viewHeight - edge1, viewWidth, edge1, vignetteAlpha * 0.60f * topBottomBoost, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(viewOriginX, viewOriginY, edge1, viewHeight, vignetteAlpha * 0.60f, 0.03f, 0.028f, 0.05f);
        drawVignetteBand(viewOriginX + viewWidth - edge1, viewOriginY, edge1, viewHeight, vignetteAlpha * 0.60f, 0.03f, 0.028f, 0.05f);

        drawVignetteBand(viewOriginX, viewOriginY, viewWidth, edge0, vignetteAlpha * 0.38f * topBottomBoost, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(viewOriginX, viewOriginY + viewHeight - edge0, viewWidth, edge0, vignetteAlpha * 0.38f * topBottomBoost, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(viewOriginX, viewOriginY, edge0, viewHeight, vignetteAlpha * 0.38f, 0.04f, 0.035f, 0.06f);
        drawVignetteBand(viewOriginX + viewWidth - edge0, viewOriginY, edge0, viewHeight, vignetteAlpha * 0.38f, 0.04f, 0.035f, 0.06f);
    }

    drawFrameBand(innerX, innerY, innerWidth, innerHeight, 0.10f + shutterT * 0.18f);
    drawFrameBand(drawX, drawY, drawWidth, std::max(4.0f, 8.0f * viewScale), 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY + drawHeight - std::max(4.0f, 8.0f * viewScale), drawWidth, std::max(4.0f, 8.0f * viewScale), 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX, drawY, std::max(4.0f, 8.0f * viewScale), drawHeight, 0.30f + shutterT * 0.16f);
    drawFrameBand(drawX + drawWidth - std::max(4.0f, 8.0f * viewScale), drawY, std::max(4.0f, 8.0f * viewScale), drawHeight, 0.30f + shutterT * 0.16f);
    drawCornerFrame(left, top, right, bottom, cornerThickness, cornerLength, frameColor);

    const int centerX = (left + right) / 2;
    const int centerY = (top + bottom) / 2;
    DrawLine(centerX - guideInset, centerY, centerX + guideInset, centerY, guideColor);
    DrawLine(centerX, centerY - guideInset, centerX, centerY + guideInset, guideColor);
    DrawBox(left + guideInset, top + guideInset, right - guideInset, bottom - guideInset, guideColor, FALSE);

    if (Entity* target = FindCaptureTarget(*transform))
    {
        if (const auto* targetTransform = target->GetComponent<TransformComponent>())
        {
            const float targetDrawX = viewOriginX + (targetTransform->x - m_flow.cameraX) * viewScale;
            const float targetDrawY = viewOriginY + (targetTransform->y - m_flow.cameraY) * viewScale;
            const float targetDrawWidth = targetTransform->width * targetTransform->scale * viewScale;
            const float targetDrawHeight = targetTransform->height * targetTransform->scale * viewScale;
            Shader_SetOutline(0.34f, 1.0f, 0.48f, 1.0f, 1.8f);
            Shader_SetTint(0.10f, 0.30f, 0.14f, 0.12f);
            SpriteDraw(m_whiteTexture, targetDrawX, targetDrawY, targetDrawWidth, targetDrawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    if (m_flow.shutterFlashRemaining > 0.0f)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, GetViewOriginX(), GetViewOriginY(), GetViewWidth(), GetViewHeight(), 0.0f, 0.0f, 1.0f, 1.0f);

        const int pulseInset = static_cast<int>(std::round(20.0f * shutterT * viewScale));
        drawCornerFrame(
            left + pulseInset,
            top + pulseInset,
            right - pulseInset,
            bottom - pulseInset,
            std::max(2, cornerThickness + 1),
            std::max(12, cornerLength - pulseInset / 2),
            frameColor);
    }

    Shader_ResetStyle();
}

void GameScene::DrawTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    const auto entries = BuildGameSceneTuningEntries();
    DrawBox(
        static_cast<int>(std::round(kTuningPanelX)),
        static_cast<int>(std::round(kTuningPanelY)),
        static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth)),
        static_cast<int>(std::round(kTuningPanelY + kTuningPanelHeight)),
        GetColor(12, 16, 22),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(kTuningPanelX)),
        static_cast<int>(std::round(kTuningPanelY)),
        static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth)),
        static_cast<int>(std::round(kTuningPanelY + kTuningPanelHeight)),
        GetColor(220, 230, 244),
        FALSE);

    DrawString(
        static_cast<int>(kTuningPanelX + 16.0f),
        static_cast<int>(kTuningPanelY + 14.0f),
        "Game Tuning",
        GetColor(245, 248, 255));
    DrawString(
        static_cast<int>(kTuningPanelX + 16.0f),
        static_cast<int>(kTuningPanelY + 42.0f),
        "F1 close  Arrow keys adjust  Click +/- writes assets/tuning.json",
        GetColor(178, 198, 220));

    const auto drawSectionHeader = [](float y, const char* label)
    {
        DrawBox(
            static_cast<int>(std::round(kTuningPanelX + 14.0f)),
            static_cast<int>(std::round(y)),
            static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth - 14.0f)),
            static_cast<int>(std::round(y + kTuningSectionHeaderHeight - 6.0f)),
            GetColor(28, 36, 48),
            TRUE);
        DrawString(
            static_cast<int>(std::round(kTuningPanelX + 24.0f)),
            static_cast<int>(std::round(y + 2.0f)),
            label,
            GetColor(255, 228, 164));
    };

    drawSectionHeader(kTuningPanelY + 76.0f, "Camera");
    drawSectionHeader(kTuningPanelY + 76.0f + (2.0f * kTuningRowHeight) + kTuningSectionHeaderHeight + kTuningSectionGap, "Player");
    drawSectionHeader(kTuningPanelY + 76.0f + (12.0f * kTuningRowHeight) + (kTuningSectionHeaderHeight + kTuningSectionGap) * 2.0f, "Photo");

    for (int index = 0; index < static_cast<int>(entries.size()); ++index)
    {
        const float rowY = GetTuningRowY(index);
        const bool selected = index == m_debug.tuningSelection;
        const int labelColor = selected ? GetColor(255, 236, 178) : GetColor(228, 234, 242);
        const int valueColor = selected ? GetColor(255, 236, 178) : GetColor(172, 196, 220);

        if (selected)
        {
            DrawBox(
                static_cast<int>(std::round(kTuningPanelX + 12.0f)),
                static_cast<int>(std::round(rowY - 1.0f)),
                static_cast<int>(std::round(kTuningPanelX + kTuningPanelWidth - 12.0f)),
                static_cast<int>(std::round(rowY + kTuningButtonHeight + 1.0f)),
                GetColor(38, 48, 62),
                TRUE);
        }

        DrawString(
            static_cast<int>(std::round(kTuningPanelX + 20.0f)),
            static_cast<int>(std::round(rowY + 1.0f)),
            entries[index].label,
            labelColor);
        DrawFormatString(
            static_cast<int>(std::round(kTuningPanelX + 190.0f)),
            static_cast<int>(std::round(rowY + 1.0f)),
            valueColor,
            "%.2f",
            *entries[index].value);

        DrawTuningButton(kTuningPanelX + kTuningMinusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight, "-", selected);
        DrawTuningButton(kTuningPanelX + kTuningPlusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight, "+", selected);
    }

    DrawFormatString(
        static_cast<int>(kTuningPanelX + 20.0f),
        static_cast<int>(kTuningPanelY + kTuningPanelHeight - 34.0f),
        GetColor(176, 208, 228),
        "Dodge Time: %.2f",
        GetPlayerDodgeDuration());

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const int cursorOuter = GetColor(255, 242, 170);
    const int cursorInner = GetColor(18, 22, 28);
    DrawCircle(mouseX, mouseY, 7, cursorOuter, FALSE);
    DrawCircle(mouseX, mouseY, 2, cursorOuter, TRUE);
    DrawLine(mouseX - 10, mouseY, mouseX + 10, mouseY, cursorInner);
    DrawLine(mouseX, mouseY - 10, mouseX, mouseY + 10, cursorInner);
    DrawLine(mouseX - 9, mouseY, mouseX + 9, mouseY, cursorOuter);
    DrawLine(mouseX, mouseY - 9, mouseX, mouseY + 9, cursorOuter);
}

void GameScene::DrawDevelopedPhotoPreview() const
{
    if (m_flow.developedPhotoPreviewRemaining <= 0.0f || !m_photo.pendingStore.active || m_photo.pendingStore.capture.items.empty())
    {
        return;
    }

    const PhotoCaptureState& previewCapture = m_photo.pendingStore.capture;
    constexpr float kPreviewLifetime = 4.2f;
    const float progress = 1.0f - Clamp01(m_flow.developedPhotoPreviewRemaining / kPreviewLifetime);
    const float cardPhaseT = Clamp01(progress / 0.34f);
    const float orbPhaseT = Clamp01((progress - 0.14f) / 0.30f);
    const float orbArriveT = Clamp01((progress - 0.34f) / 0.10f);
    const float finalFade = Clamp01(m_flow.developedPhotoPreviewRemaining / 0.34f);

    float accentR = 0.32f;
    float accentG = 0.92f;
    float accentB = 1.0f;
    GetPhotoFilterThemeOverlayColor(previewCapture.capturedTheme, accentR, accentG, accentB);

    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    const float targetSlotX = trayX + m_photo.pendingStore.slotIndex * (kPhotoTraySlotWidth + kPhotoTraySlotGap);
    const float targetCenterX = targetSlotX + 54.0f;
    const float targetCenterY = trayY + 53.0f;

    const float photoWidth = 220.0f;
    const float photoHeight = 248.0f;
    const float frameInset = 16.0f;
    const float imageWidth = photoWidth - frameInset * 2.0f;
    const float imageHeight = 150.0f;
    const float baseX = static_cast<float>(SCREEN_WIDTH) - photoWidth - 42.0f;
    const float startY = static_cast<float>(SCREEN_HEIGHT) + 30.0f;
    const float cardExitX = static_cast<float>(SCREEN_WIDTH) - 126.0f;
    const float cardCruiseY = 34.0f;

    const float cardRiseT = Clamp01(cardPhaseT / 0.56f);
    const float cardSlideT = Clamp01((cardPhaseT - 0.42f) / 0.58f);
    const float riseEase = cardRiseT * cardRiseT * (3.0f - 2.0f * cardRiseT);
    const float slideEase = cardSlideT * cardSlideT * (3.0f - 2.0f * cardSlideT);
    const float overshootT = std::sin(cardRiseT * 3.14159265f);
    const float settleScale = 0.95f + std::sin(cardRiseT * 1.57079632f) * 0.05f;
    const float pauseSlow = cardPhaseT >= 0.44f && cardPhaseT <= 0.68f
        ? std::sin(((cardPhaseT - 0.44f) / 0.24f) * 3.14159265f)
        : 0.0f;
    const float cardX = baseX + (1.0f - riseEase) * 4.0f;
    const float cardY = std::lerp(startY, cardCruiseY, riseEase) - overshootT * 14.0f + pauseSlow * 10.0f;
    const float cardAlpha = Clamp01(1.0f - Clamp01((progress - 0.22f) / 0.12f)) * finalFade;

    if (cardAlpha > 0.0f)
    {
        const float visibleHeight = photoHeight * settleScale;
        const float previewScaleOut = 1.0f;
        const float imageScaleOut = settleScale;

        DrawBox(
            static_cast<int>(std::round(cardX + 8.0f)),
            static_cast<int>(std::round(cardY + 10.0f)),
            static_cast<int>(std::round(cardX + photoWidth + 8.0f)),
            static_cast<int>(std::round(cardY + visibleHeight + 10.0f)),
            GetColor(16, 18, 24),
            TRUE);

        Shader_ResetStyle();
        Shader_SetTint(accentR, accentG, accentB, 0.14f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX - 8.0f, cardY - 8.0f, photoWidth + 16.0f, visibleHeight + 16.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(0.98f, 0.96f, 0.90f, 0.96f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, visibleHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(accentR, accentG, accentB, 0.20f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY, photoWidth, 20.0f * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_SetTint(0.92f, 0.88f, 0.74f, 0.12f * cardAlpha);
        SpriteDraw(m_whiteTexture, cardX, cardY + 22.0f * imageScaleOut, photoWidth, 10.0f * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);

        const float photoX = cardX + frameInset * previewScaleOut;
        const float photoY = cardY + frameInset * imageScaleOut;
        Shader_SetTint(0.12f, 0.14f, 0.18f, 0.88f * cardAlpha);
        SpriteDraw(m_whiteTexture, photoX - 3.0f, photoY - 3.0f, imageWidth + 6.0f, imageHeight * imageScaleOut + 6.0f, 0.0f, 0.0f, 1.0f, 1.0f);

        const float previewScale = std::min(
            imageWidth / std::max(1.0f, previewCapture.width),
            imageHeight / std::max(1.0f, previewCapture.height));
        const float previewPop = settleScale + overshootT * 0.015f;
        const float scaledContentWidth = previewCapture.width * previewScale * previewPop;
        const float scaledContentHeight = previewCapture.height * previewScale * previewPop * settleScale;
        const float contentOffsetX = photoX + (imageWidth - scaledContentWidth) * 0.5f;
        const float contentOffsetY = photoY + (imageHeight * imageScaleOut - scaledContentHeight) * 0.5f;

        Shader_SetTint(0.10f, 0.12f, 0.14f, 0.95f * cardAlpha);
        SpriteDraw(m_whiteTexture, photoX, photoY, imageWidth, imageHeight * imageScaleOut, 0.0f, 0.0f, 1.0f, 1.0f);

        for (const auto& item : previewCapture.items)
        {
            DrawCapturedPreviewItem(
                m_tileTexture,
                item,
                contentOffsetX + item.relativeX * previewScale * previewPop,
                contentOffsetY + item.relativeY * previewScale * previewPop * settleScale,
                item.width * previewScale * previewPop,
                item.height * previewScale * previewPop * settleScale,
                cardAlpha);
        }

        DrawBox(
            static_cast<int>(std::round(photoX)),
            static_cast<int>(std::round(photoY)),
            static_cast<int>(std::round(photoX + imageWidth)),
            static_cast<int>(std::round(photoY + imageHeight * imageScaleOut)),
            GetColor(215, 205, 180),
            FALSE);
        DrawString(
            static_cast<int>(cardX + 18.0f),
            static_cast<int>(cardY + visibleHeight + 22.0f),
            "Captured",
            GetColor(62, 56, 48));
        DrawFormatString(
            static_cast<int>(cardX + 18.0f),
            static_cast<int>(cardY + visibleHeight + 46.0f),
            GetColor(110, 96, 78),
            "%s  %.0fx%.0f",
            GetPhotoFilterThemeLabel(previewCapture.capturedTheme),
            previewCapture.width,
            previewCapture.height);
        Shader_ResetStyle();
    }

    const float launchX = cardX + photoWidth * 0.5f - 18.0f;
    const float launchY = cardCruiseY + photoHeight * 0.48f;
    const float control1X = launchX + 6.0f;
    const float control1Y = launchY - 172.0f;
    const float control2X = targetCenterX + 4.0f;
    const float control2Y = targetCenterY - 138.0f;
    const float easedTravel = orbPhaseT * orbPhaseT * (3.0f - 2.0f * orbPhaseT);
    const float invT = 1.0f - easedTravel;
    const float orbX = invT * invT * invT * launchX
        + 3.0f * invT * invT * easedTravel * control1X
        + 3.0f * invT * easedTravel * easedTravel * control2X
        + easedTravel * easedTravel * easedTravel * targetCenterX;
    const float orbY = invT * invT * invT * launchY
        + 3.0f * invT * invT * easedTravel * control1Y
        + 3.0f * invT * easedTravel * easedTravel * control2Y
        + easedTravel * easedTravel * easedTravel * targetCenterY;

    const float derivX = 3.0f * invT * invT * (control1X - launchX)
        + 6.0f * invT * easedTravel * (control2X - control1X)
        + 3.0f * easedTravel * easedTravel * (targetCenterX - control2X);
    const float derivY = 3.0f * invT * invT * (control1Y - launchY)
        + 6.0f * invT * easedTravel * (control2Y - control1Y)
        + 3.0f * easedTravel * easedTravel * (targetCenterY - control2Y);
    const float derivLength = std::sqrt(derivX * derivX + derivY * derivY);
    const float tangentX = derivLength > 0.001f ? derivX / derivLength : 0.0f;
    const float tangentY = derivLength > 0.001f ? derivY / derivLength : -1.0f;
    const float normalY = tangentX;

    const float orbAppear = Clamp01((progress - 0.13f) / 0.03f);
    const float orbAlpha = orbAppear * finalFade;
    if (orbAlpha <= 0.0f)
    {
        return;
    }

    const float pulse = 1.0f + std::sin(progress * 21.0f) * 0.08f;
    const float orbRadius = std::lerp(14.0f, 7.0f, orbArriveT) * pulse;
    const int orbColor = GetColor(
        static_cast<int>(std::round(120.0f + accentR * 135.0f)),
        static_cast<int>(std::round(118.0f + accentG * 122.0f)),
        static_cast<int>(std::round(140.0f + accentB * 112.0f)));
    const int coreColor = GetColor(255, 248, 228);
    const int sparkColor = GetColor(
        static_cast<int>(std::round(188.0f + accentR * 67.0f)),
        static_cast<int>(std::round(186.0f + accentG * 69.0f)),
        static_cast<int>(std::round(198.0f + accentB * 57.0f)));

    for (int i = 0; i < 8; ++i)
    {
        const float sampleT = Clamp01(orbPhaseT - i * 0.060f);
        const float sampleEase = sampleT * sampleT * (3.0f - 2.0f * sampleT);
        const float sampleInvT = 1.0f - sampleEase;
        const float sampleX = sampleInvT * sampleInvT * sampleInvT * launchX
            + 3.0f * sampleInvT * sampleInvT * sampleEase * control1X
            + 3.0f * sampleInvT * sampleEase * sampleEase * control2X
            + sampleEase * sampleEase * sampleEase * targetCenterX;
        const float sampleY = sampleInvT * sampleInvT * sampleInvT * launchY
            + 3.0f * sampleInvT * sampleInvT * sampleEase * control1Y
            + 3.0f * sampleInvT * sampleEase * sampleEase * control2Y
            + sampleEase * sampleEase * sampleEase * targetCenterY;
        const float sampleAlpha = orbAlpha * (1.0f - static_cast<float>(i) / 8.5f);
        const float sampleRadius = orbRadius * (1.6f - i * 0.10f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(108.0f * sampleAlpha)));
        DrawCircle(
            static_cast<int>(std::round(sampleX)),
            static_cast<int>(std::round(sampleY)),
            static_cast<int>(std::round(sampleRadius)),
            orbColor,
            TRUE);
    }

    for (int i = 0; i < 20; ++i)
    {
        const float orbitT = static_cast<float>(i) / 20.0f;
        const float angle = progress * 12.0f + orbitT * 6.2831853f;
        const float orbitRadius = orbRadius * (1.0f + std::sin(progress * 7.0f + orbitT * 11.0f) * 0.26f);
        const float particleX = orbX + std::cos(angle) * orbitRadius + tangentX * (orbitT - 0.5f) * 15.0f * (1.0f - orbArriveT);
        const float particleY = orbY + std::sin(angle) * orbitRadius * 0.58f + normalY * (orbitT - 0.5f) * 10.0f;
        const float particleRadius = std::lerp(3.2f, 1.0f, orbitT) * (1.0f - orbArriveT * 0.35f);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round((122.0f - orbitT * 56.0f) * orbAlpha)));
        DrawCircle(
            static_cast<int>(std::round(particleX)),
            static_cast<int>(std::round(particleY)),
            static_cast<int>(std::max(1.0f, particleRadius)),
            sparkColor,
            TRUE);
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(164.0f * orbAlpha)));
    DrawCircle(
        static_cast<int>(std::round(orbX)),
        static_cast<int>(std::round(orbY)),
        static_cast<int>(std::round(orbRadius * 1.9f)),
        orbColor,
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(236.0f * orbAlpha)));
    DrawCircle(
        static_cast<int>(std::round(orbX)),
        static_cast<int>(std::round(orbY)),
        static_cast<int>(std::round(orbRadius)),
        coreColor,
        TRUE);
    DrawCircle(
        static_cast<int>(std::round(orbX - orbRadius * 0.34f)),
        static_cast<int>(std::round(orbY - orbRadius * 0.36f)),
        static_cast<int>(std::round(orbRadius * 0.34f)),
        GetColor(255, 255, 255),
        TRUE);

    if (orbArriveT > 0.0f)
    {
        const float ringRadius = std::lerp(10.0f, 24.0f, orbArriveT);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(132.0f * orbAlpha * (1.0f - orbArriveT))));
        DrawCircle(
            static_cast<int>(std::round(targetCenterX)),
            static_cast<int>(std::round(targetCenterY)),
            static_cast<int>(std::round(ringRadius)),
            orbColor,
            FALSE);
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    Shader_ResetStyle();
}
bool GameScene::IsPhotoTrayHit(float screenX, float screenY) const
{
    if (m_flow.photoTrayReveal <= 0.05f)
    {
        return false;
    }

    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    return
        screenX >= trayX &&
        screenX <= trayX + trayWidth &&
        screenY >= trayY &&
        screenY <= trayY + kPhotoTraySlotHeight;
}

void GameScene::DrawPhotoStorageTray() const
{
    if (m_flow.photoTrayReveal <= 0.01f)
    {
        return;
    }

    constexpr float kInnerPadding = 10.0f;
    const float trayWidth = kPhotoTraySlotCount * kPhotoTraySlotWidth + (kPhotoTraySlotCount - 1) * kPhotoTraySlotGap;
    const float trayX = (static_cast<float>(SCREEN_WIDTH) - trayWidth) * 0.5f;
    const float hiddenOffset = (1.0f - m_flow.photoTrayReveal) * (kPhotoTraySlotHeight + 36.0f);
    const float trayY = static_cast<float>(SCREEN_HEIGHT) - kPhotoTraySlotHeight - 28.0f + hiddenOffset;
    const int textBright = static_cast<int>(150.0f + m_flow.photoTrayReveal * 105.0f);
    const int textBrightCool = std::min(255, textBright + 10);

    for (int slotIndex = 0; slotIndex < kPhotoTraySlotCount; ++slotIndex)
    {
        const bool slotIsPending = m_photo.pendingStore.active && m_photo.pendingStore.slotIndex == slotIndex;
        const PhotoCaptureState& storedCapture = slotIsPending ? m_photo.pendingStore.capture : m_photo.savedCaptures[slotIndex];
        const bool selected = slotIndex == m_photo.selectedCaptureSlot;
        const float slotX = trayX + slotIndex * (kPhotoTraySlotWidth + kPhotoTraySlotGap);
        const float slotY = trayY;
        const unsigned int fillColor = selected ? GetColor(30, 42, 56) : GetColor(16, 22, 30);
        const unsigned int outlineColor = selected ? GetColor(255, 234, 156) : GetColor(188, 204, 224);

        DrawBox(
            static_cast<int>(std::round(slotX)),
            static_cast<int>(std::round(slotY)),
            static_cast<int>(std::round(slotX + kPhotoTraySlotWidth)),
            static_cast<int>(std::round(slotY + kPhotoTraySlotHeight)),
            fillColor,
            TRUE);
        DrawBox(
            static_cast<int>(std::round(slotX)),
            static_cast<int>(std::round(slotY)),
            static_cast<int>(std::round(slotX + kPhotoTraySlotWidth)),
            static_cast<int>(std::round(slotY + kPhotoTraySlotHeight)),
            outlineColor,
            FALSE);

        DrawFormatString(
            static_cast<int>(slotX + 10.0f),
            static_cast<int>(slotY + 8.0f),
            selected ? GetColor(255, textBright, 196) : GetColor(textBright, textBrightCool, 244),
            "PHOTO %d",
            slotIndex + 1);

        if (!storedCapture.hasPhoto || storedCapture.items.empty())
        {
            DrawString(
                static_cast<int>(slotX + 50.0f),
                static_cast<int>(slotY + 42.0f),
                slotIsPending ? "STORING" : "EMPTY",
                slotIsPending ? GetColor(214, 204, 156) : GetColor(122, 136, 156));
            continue;
        }

        const float previewX = slotX + kInnerPadding;
        const float previewY = slotY + 28.0f;
        const float previewWidth = 88.0f;
        const float previewHeight = 50.0f;
        const float scale = std::min(
            previewWidth / std::max(1.0f, storedCapture.width),
            previewHeight / std::max(1.0f, storedCapture.height));
        const float contentX = previewX + (previewWidth - storedCapture.width * scale) * 0.5f;
        const float contentY = previewY + (previewHeight - storedCapture.height * scale) * 0.5f;

        DrawBox(
            static_cast<int>(std::round(previewX)),
            static_cast<int>(std::round(previewY)),
            static_cast<int>(std::round(previewX + previewWidth)),
            static_cast<int>(std::round(previewY + previewHeight)),
            GetColor(10, 14, 18),
            TRUE);

        for (const auto& item : storedCapture.items)
        {
            DrawCapturedPreviewItem(
                m_tileTexture,
                item,
                contentX + item.relativeX * scale,
                contentY + item.relativeY * scale,
                item.width * scale,
                item.height * scale,
                1.0f);
        }

        DrawBox(
            static_cast<int>(std::round(previewX)),
            static_cast<int>(std::round(previewY)),
            static_cast<int>(std::round(previewX + previewWidth)),
            static_cast<int>(std::round(previewY + previewHeight)),
            GetColor(215, 205, 180),
            FALSE);

        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 34.0f),
            GetColor(230, 236, 242),
            "%s",
            GetPhotoFilterThemeLabel(storedCapture.capturedTheme));
        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 56.0f),
            GetColor(170, 186, 204),
            "%.0fx%.0f",
            storedCapture.width,
            storedCapture.height);
        DrawFormatString(
            static_cast<int>(slotX + 108.0f),
            static_cast<int>(slotY + 74.0f),
            slotIsPending ? GetColor(230, 214, 158) : GetColor(150, 170, 190),
            "%s",
            slotIsPending ? "Storing..." : (selected ? "Selected" : "Click to select"));
    }
}

void GameScene::DrawPhotoPlacementPreview() const
{
    photo_system::DrawPlacementPreview(*this);
}

void GameScene::DrawMapEditorOverlay() const
{
    if (!m_mapEditor.active)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float tileSize = m_tileMap.GetTileSize();
    if (viewScale <= 0.0f || tileSize <= 0.0f)
    {
        return;
    }

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const float worldX = m_flow.cameraX + (static_cast<float>(mouseX) - GetViewOriginX()) / viewScale;
    const float worldY = m_flow.cameraY + (static_cast<float>(mouseY) - GetViewOriginY()) / viewScale;
    const int column = static_cast<int>(std::floor(worldX / tileSize));
    const int row = static_cast<int>(std::floor(worldY / tileSize));
    int hoveredTileValue = 0;
    char hoveredMarkerValue = '\0';
    const bool markerMode = m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Marker;
    const int cursorOuterColor = markerMode ? GetColor(116, 220, 255) : GetColor(255, 225, 120);
    const int cursorInnerColor = markerMode ? GetColor(72, 168, 255) : GetColor(255, 170, 80);
    if (column >= 0 && row >= 0 && column < m_tileMap.GetWidth() && row < m_tileMap.GetHeight())
    {
        hoveredTileValue = m_tileMap.GetTile(column, row);
        hoveredMarkerValue = m_tileMap.GetMarker(column, row);
        const int left = static_cast<int>(std::round(GetViewOriginX() + (static_cast<float>(column) * tileSize - m_flow.cameraX) * viewScale));
        const int top = static_cast<int>(std::round(GetViewOriginY() + (static_cast<float>(row) * tileSize - m_flow.cameraY) * viewScale));
        const int right = static_cast<int>(std::round(GetViewOriginX() + ((static_cast<float>(column) + 1.0f) * tileSize - m_flow.cameraX) * viewScale));
        const int bottom = static_cast<int>(std::round(GetViewOriginY() + ((static_cast<float>(row) + 1.0f) * tileSize - m_flow.cameraY) * viewScale));
        DrawBox(left, top, right, bottom, cursorOuterColor, FALSE);
        DrawBox(left + 1, top + 1, right - 1, bottom - 1, cursorInnerColor, FALSE);
    }

    const int panelLeft = 22;
    const int panelTop = 22;
    const int panelRight = 560;
    const int panelBottom = 244;
    const char selectedMarker = m_mapEditor.selectedMarker;
    const char markerLabel = selectedMarker == '\0' ? '-' : selectedMarker;
    const char hoveredMarkerLabel = hoveredMarkerValue == '\0' ? '-' : static_cast<char>(std::toupper(static_cast<unsigned char>(hoveredMarkerValue)));
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 206);
    DrawBox(panelLeft, panelTop, panelRight, panelBottom, GetColor(16, 22, 30), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(panelLeft, panelTop, panelRight, panelBottom, GetColor(198, 214, 232), FALSE);

    DrawString(panelLeft + 16, panelTop + 14, "マップエディター", GetColor(244, 250, 255));
    DrawBox(panelLeft + 330, panelTop + 10, panelRight - 14, panelTop + 34, markerMode ? GetColor(28, 78, 134) : GetColor(96, 72, 24), TRUE);
    DrawBox(panelLeft + 330, panelTop + 10, panelRight - 14, panelTop + 34, markerMode ? GetColor(116, 220, 255) : GetColor(255, 220, 120), FALSE);
    DrawString(panelLeft + 342, panelTop + 15, markerMode ? "MARKER MODE" : "TILE MODE", GetColor(244, 250, 255));
    DrawString(panelLeft + 16, panelTop + 38, "F4: 閉じる  M: タイル/マーカー切替  WASD/十字: カメラ移動", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 58, "左ドラッグ: 塗る  右ドラッグ: 消す", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 78, "タイル: 0-9 / Q,E / F9(10)", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 96, "マーカー: 0(None),1(G),2(S),3(E),4(T),5(W),6(R),7(B),8(V),9(C),F10(M Log),F11(Y),F12(N Boss),H,I,K,L,Q,E", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 114, "F5: 保存  F6: CSV再読込  F7: 新規作成  F8: 別名保存", GetColor(168, 192, 220));
    DrawFormatString(
        panelLeft + 16,
        panelTop + 138,
        markerMode ? GetColor(180, 238, 255) : GetColor(255, 236, 160),
        "編集モード: %s",
        markerMode ? "マーカー" : "タイル");
    DrawFormatString(
        panelLeft + 16,
        panelTop + 158,
        GetColor(255, 236, 160),
        "選択タイル: %d  /  選択マーカー: %c",
        m_mapEditor.selectedTileValue,
        markerLabel);
    DrawFormatString(
        panelLeft + 16,
        panelTop + 178,
        GetColor(255, 236, 160),
        "カーソル: (%d,%d)  tile=%d marker=%c",
        column,
        row,
        hoveredTileValue,
        hoveredMarkerLabel);
    DrawFormatString(
        panelLeft + 16,
        panelTop + 198,
        GetColor(220, 230, 244),
        "現在マップ: %s",
        GetMapDisplayName(gCurrentMapCsvPath).c_str());
    if (!m_mapEditor.statusMessage.empty())
    {
        DrawString(panelLeft + 16, panelTop + 222, m_mapEditor.statusMessage.c_str(), GetColor(142, 236, 166));
    }
}

void GameScene::DrawBackdrop() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();
    const float panelRight = viewOriginX + viewWidth;
    const float panelBottom = viewOriginY + viewHeight;

    Shader_ResetStyle();
    Shader_SetTint(0.02f, 0.02f, 0.03f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetGradientMap(0.03f, 0.03f, 0.05f, 1.0f, 0.10f, 0.10f, 0.14f, 1.0f, 1.0f);
    Shader_SetTint(0.92f, 0.92f, 0.96f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, 210.0f * viewScale, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.05f, 0.05f, 0.07f, 0.98f);
    SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    if (m_photo.capture.selectedTheme != PhotoFilterTheme::None)
    {
        float filterR = 1.0f;
        float filterG = 1.0f;
        float filterB = 1.0f;
        GetPhotoFilterThemeOverlayColor(m_photo.capture.selectedTheme, filterR, filterG, filterB);
        Shader_SetTint(filterR, filterG, filterB, 0.07f);
        SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    {
        const float worldLeft = m_flow.cameraX;
        const float worldRight = m_flow.cameraX + gCameraViewWidth;
        const float gridSpacing = m_tileMap.GetTileSize();
        const unsigned int majorColor = GetColor(72, 188, 128);
        const unsigned int minorColor = GetColor(38, 112, 82);

        for (float worldX = std::floor(worldLeft / gridSpacing) * gridSpacing; worldX <= worldRight; worldX += gridSpacing)
        {
            const float screenX = viewOriginX + (worldX - m_flow.cameraX) * viewScale;
            const int x = static_cast<int>(std::round(screenX));
            const bool major = std::fmod(std::fabs(worldX), gridSpacing * 4.0f) < 0.5f ||
                (gridSpacing * 4.0f - std::fmod(std::fabs(worldX), gridSpacing * 4.0f)) < 0.5f;
            DrawLine(x, static_cast<int>(viewOriginY), x, static_cast<int>(viewOriginY + viewHeight), major ? majorColor : minorColor);
            if (!major)
            {
                DrawLine(x + 1, static_cast<int>(viewOriginY), x + 1, static_cast<int>(viewOriginY + viewHeight), GetColor(22, 56, 40));
            }
        }

        const float worldTop = m_flow.cameraY;
        const float worldBottom = m_flow.cameraY + gCameraViewHeight;
        for (float worldY = std::floor(worldTop / gridSpacing) * gridSpacing; worldY <= worldBottom; worldY += gridSpacing)
        {
            const float screenY = viewOriginY + (worldY - m_flow.cameraY) * viewScale;
            const int y = static_cast<int>(std::round(screenY));
            const bool major = std::fmod(worldY, gridSpacing * 4.0f) < 0.5f ||
                (gridSpacing * 4.0f - std::fmod(worldY, gridSpacing * 4.0f)) < 0.5f;
            DrawLine(static_cast<int>(viewOriginX), y, static_cast<int>(viewOriginX + viewWidth), y, major ? majorColor : minorColor);
            if (!major)
            {
                DrawLine(static_cast<int>(viewOriginX), y + 1, static_cast<int>(viewOriginX + viewWidth), y + 1, GetColor(22, 56, 40));
            }
        }
    }

    Shader_SetTint(0.18f, 0.18f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY - 10.0f, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, panelBottom, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    m_tileMap.Draw(m_tileTexture, viewOriginX - m_flow.cameraX * viewScale, viewOriginY - m_flow.cameraY * viewScale, viewScale);

    // Stage-transition marker visualization (A/Z/X etc.) in the current viewport.
    {
        const float tileSize = m_tileMap.GetTileSize();
        if (tileSize > 0.0f)
        {
            const int columnStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraX / tileSize)) - 1);
            const int rowStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraY / tileSize)) - 1);
            const int columnEnd = std::min(m_tileMap.GetWidth() - 1, static_cast<int>(std::ceil((m_flow.cameraX + gCameraViewWidth) / tileSize)) + 1);
            const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>(std::ceil((m_flow.cameraY + gCameraViewHeight) / tileSize)) + 1);

            for (int row = rowStart; row <= rowEnd; ++row)
            {
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
                    if (marker == '\0')
                    {
                        continue;
                    }

                    const StageTransitionLink* transition = nullptr;
                    for (const StageTransitionLink& link : gStageTransitionLinks)
                    {
                        const bool sourceMatches = link.sourceMapCsv == "*" || link.sourceMapCsv == gCurrentMapCsvPath;
                        if (sourceMatches && link.marker == marker)
                        {
                            transition = &link;
                            break;
                        }
                    }
                    if (!transition)
                    {
                        continue;
                    }

                    const float worldX = static_cast<float>(column) * tileSize;
                    const float worldY = static_cast<float>(row) * tileSize;
                    const int left = static_cast<int>(std::round(viewOriginX + (worldX - m_flow.cameraX) * viewScale));
                    const int top = static_cast<int>(std::round(viewOriginY + (worldY - m_flow.cameraY) * viewScale));
                    const int right = static_cast<int>(std::round(viewOriginX + (worldX + tileSize - m_flow.cameraX) * viewScale));
                    const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + tileSize - m_flow.cameraY) * viewScale));

                    DrawBox(left, top, right, bottom, GetColor(255, 210, 90), FALSE);
                    DrawFormatString(left + 4, top + 2, GetColor(255, 245, 190), "%c", marker);

                    const std::string destName = GetMapDisplayName(transition->destinationMapCsv);
                    DrawFormatString(left, top - 16, GetColor(180, 240, 255), "-> %s", destName.c_str());
                }
            }
        }
    }

    if (m_mapEditor.active)
    {
        const float tileSize = m_tileMap.GetTileSize();
        if (tileSize > 0.0f)
        {
            const int columnStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraX / tileSize)) - 1);
            const int rowStart = std::max(0, static_cast<int>(std::floor(m_flow.cameraY / tileSize)) - 1);
            const int columnEnd = std::min(m_tileMap.GetWidth() - 1, static_cast<int>(std::ceil((m_flow.cameraX + gCameraViewWidth) / tileSize)) + 1);
            const int rowEnd = std::min(m_tileMap.GetHeight() - 1, static_cast<int>(std::ceil((m_flow.cameraY + gCameraViewHeight) / tileSize)) + 1);

            for (int row = rowStart; row <= rowEnd; ++row)
            {
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
                    if (marker == '\0')
                    {
                        continue;
                    }

                    int r = 236;
                    int g = 236;
                    int b = 236;
                    switch (marker)
                    {
                    case 'W': r = 120; g = 212; b = 255; break;
                    case 'R': r = 255; g = 128; b = 128; break;
                    case 'S': r = 255; g = 176; b = 88; break;
                    case 'B': r = 172; g = 142; b = 255; break;
                    case 'V': r = 146; g = 255; b = 170; break;
                    case 'C': r = 246; g = 238; b = 122; break;
                    case 'M': r = 255; g = 142; b = 210; break;
                    case 'Y': r = 240; g = 208; b = 90; break;
                    case 'H': r = 214; g = 124; b = 255; break;
                    case 'I': r = 188; g = 108; b = 255; break;
                    case 'K': r = 250; g = 112; b = 96; break;
                    case 'L': r = 140; g = 186; b = 230; break;
                    case 'G': r = 255; g = 235; b = 128; break;
                    case 'T': r = 122; g = 230; b = 255; break;
                    case 'E': r = 180; g = 255; b = 196; break;
                    default: break;
                    }

                    const float worldX = static_cast<float>(column) * tileSize;
                    const float worldY = static_cast<float>(row) * tileSize;
                    const int left = static_cast<int>(std::round(viewOriginX + (worldX - m_flow.cameraX) * viewScale));
                    const int top = static_cast<int>(std::round(viewOriginY + (worldY - m_flow.cameraY) * viewScale));
                    const int right = static_cast<int>(std::round(viewOriginX + (worldX + tileSize - m_flow.cameraX) * viewScale));
                    const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + tileSize - m_flow.cameraY) * viewScale));

                    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 88);
                    DrawBox(left, top, right, bottom, GetColor(r, g, b), TRUE);
                    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
                    DrawBox(left, top, right, bottom, GetColor(r, g, b), FALSE);
                    DrawFormatString(left + 4, top + 2, GetColor(20, 28, 36), "%c", marker);
                }
            }
        }
    }

    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* transform = player->GetComponent<TransformComponent>())
        {
            DrawString(
                static_cast<int>(viewOriginX + 24.0f),
                static_cast<int>(viewOriginY + GetViewHeight() - 42.0f),
                GetStageGuideText(transform->x),
                GetColor(238, 244, 255));
        }
    }

    {
        float filterR = 1.0f;
        float filterG = 1.0f;
        float filterB = 1.0f;
        GetPhotoFilterThemeOverlayColor(m_photo.capture.selectedTheme, filterR, filterG, filterB);
        const int panelX = static_cast<int>(viewOriginX + 22.0f);
        const int panelY = static_cast<int>(viewOriginY + 18.0f);
        const int panelWidth = 308;
        const int panelHeight = 78;
        DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(14, 18, 24), TRUE);
        DrawBox(panelX, panelY, panelX + panelWidth, panelY + panelHeight, GetColor(220, 228, 236), FALSE);
        DrawBox(
            panelX + 10,
            panelY + 10,
            panelX + 44,
            panelY + 44,
            GetColor(
                static_cast<int>(filterR * 255.0f),
                static_cast<int>(filterG * 255.0f),
                static_cast<int>(filterB * 255.0f)),
            TRUE);
        DrawFormatString(
            panelX + 56,
            panelY + 10,
            GetColor(245, 248, 255),
            "Filter: %s",
            GetPhotoFilterThemeLabel(m_photo.capture.selectedTheme));
        DrawFormatString(
            panelX + 56,
            panelY + 32,
            GetColor(180, 210, 235),
            "%s",
            GetPhotoFilterThemeEffectText(m_photo.capture.selectedTheme));
        DrawFormatString(
            panelX + 12,
            panelY + 54,
            GetColor(205, 220, 235),
            "C cycle  1 None  2 Hot  3 Cold  4 Invert  5 Sepia");
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const
{
    static_cast<void>(playerTransform);
    width = m_tileMap.GetTileSize() * gCaptureWidthTiles * m_flow.captureFinderScale;
    height = m_tileMap.GetTileSize() * gCaptureHeightTiles * m_flow.captureFinderScale;

    const float cursorStartWorldX = m_flow.cameraX + gCameraViewWidth * 0.5f;
    const float cursorStartWorldY = m_flow.cameraY + gCameraViewHeight * 0.5f;

    static float padCursorWorldX = cursorStartWorldX;
    static float padCursorWorldY = cursorStartWorldY;
    static float padCursorVelocityX = 0.0f;
    static float padCursorVelocityY = 0.0f;
    static unsigned int lastTimeMs = 0;
    static bool initialized = false;
    static int lastSessionId = -1;
    static int lastMouseX = Input_GetMouseX();
    static int lastMouseY = Input_GetMouseY();

    if (!initialized)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        initialized = true;
    }

    if (lastSessionId != m_flow.cameraModeSessionId)
    {
        padCursorWorldX = cursorStartWorldX;
        padCursorWorldY = cursorStartWorldY;
        padCursorVelocityX = 0.0f;
        padCursorVelocityY = 0.0f;
        lastSessionId = m_flow.cameraModeSessionId;
    }

    const unsigned int nowMs = static_cast<unsigned int>(GetNowCount());
    const float dt = lastTimeMs ? (static_cast<float>(nowMs - lastTimeMs) / 1000.0f) : (1.0f / 60.0f);
    lastTimeMs = nowMs;

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const bool mouseMoved = mouseX != lastMouseX || mouseY != lastMouseY;
    lastMouseX = mouseX;
    lastMouseY = mouseY;

    if (mouseMoved)
    {
        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        padCursorWorldX = ((static_cast<float>(mouseX) - viewOriginX) / viewScale) + m_flow.cameraX;
        padCursorWorldY = ((static_cast<float>(mouseY) - viewOriginY) / viewScale) + m_flow.cameraY;
        padCursorVelocityX = 0.0f;
        padCursorVelocityY = 0.0f;
    }

    const float rightX = Input_GetRightStickX();
    const float rightY = Input_GetRightStickY();
    const float magnitude = std::sqrt(rightX * rightX + rightY * rightY);
    const bool padActive = Input_IsGamepadConnected() && magnitude > kPadDeadZone;
    if (padActive)
    {
        const float normalizedMagnitude = std::clamp((magnitude - kPadDeadZone) / (1.0f - kPadDeadZone), 0.0f, 1.0f);
        const float curvedMagnitude = normalizedMagnitude * normalizedMagnitude;
        const float scale = curvedMagnitude / magnitude;
        const float desiredVelocityX = rightX * scale * kPadCursorMaxSpeed;
        const float desiredVelocityY = rightY * scale * kPadCursorMaxSpeed;
        const float response = std::min(1.0f, dt * kPadCursorResponse);
        padCursorVelocityX += (desiredVelocityX - padCursorVelocityX) * response;
        padCursorVelocityY += (desiredVelocityY - padCursorVelocityY) * response;
    }
    else
    {
        const float damping = std::max(0.0f, 1.0f - dt * kPadCursorDamping);
        padCursorVelocityX *= damping;
        padCursorVelocityY *= damping;
    }

    padCursorWorldX += padCursorVelocityX * dt;
    padCursorWorldY += padCursorVelocityY * dt;

    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    padCursorWorldX = std::clamp(padCursorWorldX, 0.0f, std::max(0.0f, mapWidth));
    padCursorWorldY = std::clamp(padCursorWorldY, 0.0f, std::max(0.0f, mapHeight));

    x = padCursorWorldX - width * 0.5f;
    y = std::clamp(padCursorWorldY - height * 0.5f, 0.0f, std::max(0.0f, mapHeight - height));
}

Entity* GameScene::FindCaptureTarget(const TransformComponent& playerTransform) const
{
    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(playerTransform, frameX, frameY, frameWidth, frameHeight);

    TransformComponent captureFrame(frameX, frameY, frameWidth, frameHeight);
    Entity* bestTarget = nullptr;
    float bestDistance = 1000000.0f;
    for (const auto& entity : m_entities)
    {
        if (HasTag(*entity, kTagPlayer) || HasTag(*entity, kTagEnemy))
        {
            continue;
        }

        if (HasTag(*entity, kTagPhotoBox))
        {
            const auto* layer = entity->GetComponent<PhotoCopyLayerComponent>();
            if (!layer || layer->layer != PhotoCopyLayer::Foreground)
            {
                continue;
            }

            if (const auto* pasteAnimation = entity->GetComponent<PhotoPasteAnimationComponent>())
            {
                if (!pasteAnimation->IsFinished())
                {
                    continue;
                }
            }
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
        if (!transform || !sprite || !IntersectsRect(captureFrame, *transform))
        {
            continue;
        }

        const float targetCenterX = transform->x + transform->width * transform->scale * 0.5f;
        const float playerCenterX = playerTransform.x + playerTransform.width * playerTransform.scale * 0.5f;
        const float distance = std::fabs(targetCenterX - playerCenterX);
        if (!bestTarget || distance < bestDistance)
        {
            bestTarget = entity.get();
            bestDistance = distance;
        }
    }

    return bestTarget;
}

void GameScene::DrawPlayerHpBar() const
{
    const Entity* player = FindEntityByTag(kTagPlayer);
    if (!player) return;

    const auto* health = player->GetComponent<HealthComponent>();
    if (!health) return;

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);

    constexpr float kBarWidth = 240.0f;
    constexpr float kBarHeight = 24.0f;
    constexpr float kPanelPadding = 12.0f;
    constexpr float kMarginRight = 32.0f;
    constexpr float kMarginTop = 32.0f;

    const float barX = static_cast<float>(SCREEN_WIDTH) - kBarWidth - kMarginRight;
    const float barY = kMarginTop;
    const float panelX = barX - kPanelPadding;
    const float panelY = barY - kPanelPadding;
    const float panelWidth = kBarWidth + kPanelPadding * 2.0f;
    const float panelHeight = kBarHeight + 38.0f;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(14, 20, 28),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(200, 214, 230),
        FALSE);

    const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
    const float displayRatio = m_flow.hpUiInitialized ? m_flow.hpDisplayRatio : targetRatio;
    const float lagRatio = m_flow.hpUiInitialized ? m_flow.hpDamageLagRatio : targetRatio;
    const float flash = m_flow.hpDamageFlash;

    // バー背景
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(38, 46, 58),
        TRUE);

    // 被弾遅延バー（減った量が一瞬残る）
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * std::clamp(lagRatio, 0.0f, 1.0f))),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(232, 94, 84),
        TRUE);

    // 現在HPバー（割合で色を変化）
    const float clampedRatio = std::clamp(displayRatio, 0.0f, 1.0f);
    const int hpR = static_cast<int>(std::round(230.0f - 160.0f * clampedRatio));
    const int hpG = static_cast<int>(std::round(76.0f + 144.0f * clampedRatio));
    const int hpB = static_cast<int>(std::round(72.0f + 46.0f * clampedRatio));
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * clampedRatio)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(hpR, hpG, hpB),
        TRUE);

    // ハイライト
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth * clampedRatio)),
        static_cast<int>(std::round(barY + kBarHeight * 0.45f)),
        GetColor(255, 255, 255),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    // 被弾フラッシュ
    if (flash > 0.0f)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(150.0f * flash)));
        DrawBox(
            static_cast<int>(std::round(barX)),
            static_cast<int>(std::round(barY)),
            static_cast<int>(std::round(barX + kBarWidth)),
            static_cast<int>(std::round(barY + kBarHeight)),
            GetColor(255, 246, 238),
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    // 目盛り
    for (int i = 1; i < maxHp; ++i)
    {
        const float x = barX + kBarWidth * (static_cast<float>(i) / static_cast<float>(maxHp));
        DrawLine(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + 2.0f)),
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + kBarHeight - 2.0f)),
            GetColor(42, 48, 58));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + kBarWidth)),
        static_cast<int>(std::round(barY + kBarHeight)),
        GetColor(232, 236, 246),
        FALSE);

    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY - 18.0f)),
        "LIFE",
        GetColor(196, 214, 236));
    DrawFormatString(
        static_cast<int>(std::round(barX + kBarWidth * 0.5f) - 34.0f),
        static_cast<int>(std::round(barY + 4.0f)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);
}

void GameScene::DrawBatterySwitchCounters() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float tileOffsetY = m_tileMap.GetTileSize() * 2.0f * viewScale;

    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagBatterySwitch))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        const auto* batterySwitch = entity->GetComponent<BatterySwitchComponent>();
        if (!transform || !batterySwitch)
        {
            continue;
        }

        const float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
        const float drawY = viewOriginY + (transform->y - m_flow.cameraY) * viewScale;
        const float drawWidth = transform->width * transform->scale * viewScale;
        const float panelWidth = 58.0f;
        const float panelHeight = 22.0f;
        const float panelX = drawX + drawWidth * 0.5f - panelWidth * 0.5f;
        const float panelY = drawY - tileOffsetY - 26.0f;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 208);
        DrawBox(
            static_cast<int>(std::round(panelX)),
            static_cast<int>(std::round(panelY)),
            static_cast<int>(std::round(panelX + panelWidth)),
            static_cast<int>(std::round(panelY + panelHeight)),
            GetColor(18, 20, 24),
            TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        DrawBox(
            static_cast<int>(std::round(panelX)),
            static_cast<int>(std::round(panelY)),
            static_cast<int>(std::round(panelX + panelWidth)),
            static_cast<int>(std::round(panelY + panelHeight)),
            batterySwitch->isPressed ? GetColor(180, 255, 196) : GetColor(242, 226, 190),
            FALSE);

        DrawFormatString(
            static_cast<int>(std::round(panelX + 10.0f)),
            static_cast<int>(std::round(panelY + 4.0f)),
            batterySwitch->isPressed ? GetColor(220, 255, 228) : GetColor(255, 244, 220),
            "%d/%d",
            batterySwitch->insertedBatteryCount,
            batterySwitch->requiredBatteryCount);
    }
}
