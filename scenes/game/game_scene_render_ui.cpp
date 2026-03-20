#include "game_scene_internal.h"
#include "photo_system.h"
#include "photo_filter_rules.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr int kPhotoTraySlotCount = 3;
    constexpr float kPhotoTraySlotWidth = 170.0f;
    constexpr float kPhotoTraySlotHeight = 92.0f;
    constexpr float kPhotoTraySlotGap = 18.0f;

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
        if (item.sourceTileValue == 6 || item.sourceTileValue == 7)
        {
            const int color = GetColor(
                static_cast<int>(std::round(item.tintR * 255.0f)),
                static_cast<int>(std::round(item.tintG * 255.0f)),
                static_cast<int>(std::round(item.tintB * 255.0f)));
            if (item.sourceTileValue == 6)
            {
                DrawTriangleAA(drawX, drawY + drawHeight, drawX + drawWidth, drawY + drawHeight, drawX + drawWidth, drawY, color, TRUE);
            }
            else
            {
                DrawTriangleAA(drawX, drawY, drawX, drawY + drawHeight, drawX + drawWidth, drawY + drawHeight, color, TRUE);
            }
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
}

void GameScene::DrawCaptureOverlay() const
{
    if (!m_flow.cameraMode && m_flow.shutterFlashRemaining <= 0.0f)
    {
        return;
    }

    const Entity* player = FindEntityByTag("Player");
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
    const float drawX = viewOriginX + (frameX - m_flow.cameraX) * viewScale;
    const float drawY = viewOriginY + frameY * viewScale;
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
            const float targetDrawY = viewOriginY + targetTransform->y * viewScale;
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

void GameScene::DrawTuningPanel() const
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    const int left = 32;
    const int top = 32;
    const int width = 360;
    const int height = 360;

    DrawBox(left, top, left + width, top + height, GetColor(18, 22, 28), TRUE);
    DrawBox(left, top, left + width, top + height, GetColor(210, 220, 240), FALSE);
    DrawString(left + 16, top + 14, "Tuning Panel  F1: Close", GetColor(255, 255, 255));
    DrawString(left + 16, top + 38, "Up/Down: Select  Left/Right: Adjust", GetColor(180, 210, 255));

    struct Entry
    {
        const char* label;
        float value;
    };

    const Entry entries[] =
    {
        { "Camera Width", gCameraViewWidth },
        { "Camera Height", gCameraViewHeight },
        { "Move Speed", gPlayerMoveSpeed },
        { "Jump Speed", gPlayerJumpSpeed },
        { "Gravity", gPlayerGravity },
        { "Max Fall", gPlayerMaxFallSpeed },
        { "Dodge Speed", gPlayerDodgeSpeed },
        { "Dodge Dist", gPlayerDodgeDistance },
        { "Dodge I-Frame", gPlayerDodgeInvincibilitySeconds },
        { "Dodge Time", GetPlayerDodgeDuration() },
        { "Coyote", gCoyoteTimeSeconds },
        { "Ground Snap", gGroundSnapDistance },
        { "Capture W Tiles", gCaptureWidthTiles },
        { "Capture H Tiles", gCaptureHeightTiles },
        { "Pickup Bonus", gPickupTimeBonus },
    };

    for (int index = 0; index < static_cast<int>(sizeof(entries) / sizeof(entries[0])); ++index)
    {
        const int y = top + 72 + index * 22;
        const unsigned int color = index == m_debug.tuningSelection
            ? GetColor(255, 240, 120)
            : GetColor(235, 235, 235);
        DrawFormatString(left + 16, y, color, "%c %-14s : %7.2f", index == m_debug.tuningSelection ? '>' : ' ', entries[index].label, entries[index].value);
    }
}

void GameScene::DrawDevelopedPhotoPreview() const
{
    if (m_flow.developedPhotoPreviewRemaining <= 0.0f || m_photo.capture.items.empty())
    {
        return;
    }

    constexpr float kPreviewLifetime = 3.2f;
    const float remainingT = Clamp01(m_flow.developedPhotoPreviewRemaining / kPreviewLifetime);
    const float appearT = Clamp01((kPreviewLifetime - m_flow.developedPhotoPreviewRemaining) / 0.35f);
    const float fadeT = Clamp01(m_flow.developedPhotoPreviewRemaining / 0.45f);
    const float alpha = std::min(1.0f, appearT) * std::min(1.0f, fadeT);

    const float photoWidth = 220.0f;
    const float photoHeight = 248.0f;
    const float frameInset = 16.0f;
    const float imageWidth = photoWidth - frameInset * 2.0f;
    const float imageHeight = 150.0f;
    const float baseX = static_cast<float>(SCREEN_WIDTH) - photoWidth - 42.0f;
    const float x = baseX + (1.0f - appearT) * 120.0f;
    const float y = 34.0f + (1.0f - alpha) * -12.0f;

    DrawBox(
        static_cast<int>(std::round(x + 8.0f)),
        static_cast<int>(std::round(y + 10.0f)),
        static_cast<int>(std::round(x + photoWidth + 8.0f)),
        static_cast<int>(std::round(y + photoHeight + 10.0f)),
        GetColor(16, 18, 24),
        TRUE);

    Shader_ResetStyle();
    Shader_SetTint(0.98f, 0.96f, 0.90f, 0.96f * alpha);
    SpriteDraw(m_whiteTexture, x, y, photoWidth, photoHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.92f, 0.88f, 0.74f, 0.20f * alpha);
    SpriteDraw(m_whiteTexture, x, y, photoWidth, 26.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    const float photoX = x + frameInset;
    const float photoY = y + frameInset;
    Shader_SetTint(0.12f, 0.14f, 0.18f, 0.88f * alpha);
    SpriteDraw(m_whiteTexture, photoX - 3.0f, photoY - 3.0f, imageWidth + 6.0f, imageHeight + 6.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    const float previewScale = std::min(
        imageWidth / std::max(1.0f, m_photo.capture.width),
        imageHeight / std::max(1.0f, m_photo.capture.height));
    const float contentOffsetX = photoX + (imageWidth - m_photo.capture.width * previewScale) * 0.5f;
    const float contentOffsetY = photoY + (imageHeight - m_photo.capture.height * previewScale) * 0.5f;

    Shader_SetTint(0.10f, 0.12f, 0.14f, 0.95f * alpha);
    SpriteDraw(m_whiteTexture, photoX, photoY, imageWidth, imageHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    for (const auto& item : m_photo.capture.items)
    {
        DrawCapturedPreviewItem(
            m_tileTexture,
            item,
            contentOffsetX + item.relativeX * previewScale,
            contentOffsetY + item.relativeY * previewScale,
            item.width * previewScale,
            item.height * previewScale,
            alpha);
    }

    DrawBox(
        static_cast<int>(std::round(photoX)),
        static_cast<int>(std::round(photoY)),
        static_cast<int>(std::round(photoX + imageWidth)),
        static_cast<int>(std::round(photoY + imageHeight)),
        GetColor(215, 205, 180),
        FALSE);
    DrawString(
        static_cast<int>(x + 18.0f),
        static_cast<int>(y + imageHeight + 30.0f),
        "Captured",
        GetColor(62, 56, 48));
    DrawFormatString(
        static_cast<int>(x + 18.0f),
        static_cast<int>(y + imageHeight + 54.0f),
        GetColor(110, 96, 78),
        "%s  %.0fx%.0f",
        GetPhotoFilterThemeLabel(m_photo.capture.capturedTheme),
        m_photo.capture.width,
        m_photo.capture.height);

    const int accent = GetColor(
        static_cast<int>(std::round(160.0f * remainingT + 40.0f)),
        static_cast<int>(std::round(190.0f * remainingT + 20.0f)),
        255);
    DrawBox(
        static_cast<int>(std::round(x + photoWidth - 54.0f)),
        static_cast<int>(std::round(y + 16.0f)),
        static_cast<int>(std::round(x + photoWidth - 18.0f)),
        static_cast<int>(std::round(y + 28.0f)),
        accent,
        TRUE);
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
        const PhotoCaptureState& storedCapture = m_photo.savedCaptures[slotIndex];
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
                "EMPTY",
                GetColor(122, 136, 156));
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
            GetColor(150, 170, 190),
            selected ? "Selected" : "Click to select");
    }
}

void GameScene::DrawPhotoPlacementPreview() const
{
    photo_system::DrawPlacementPreview(*this);
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

        for (float worldY = 0.0f; worldY <= gCameraViewHeight; worldY += gridSpacing)
        {
            const float screenY = viewOriginY + worldY * viewScale;
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

    m_tileMap.Draw(m_tileTexture, viewOriginX - m_flow.cameraX * viewScale, viewOriginY, viewScale);

    if (const Entity* player = FindEntityByTag("Player"))
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
    width = m_tileMap.GetTileSize() * gCaptureWidthTiles;
    height = m_tileMap.GetTileSize() * gCaptureHeightTiles;

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float cursorWorldX = ((static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale) + m_flow.cameraX;
    const float cursorWorldY = ((static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale);

    x = std::clamp(cursorWorldX - width * 0.5f, 0.0f, std::max(0.0f, GetMapPixelWidth() - width));
    y = std::clamp(cursorWorldY - height * 0.5f, 0.0f, std::max(0.0f, GetMapPixelHeight() - height));
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
        if (HasTag(*entity, "Player") || HasTag(*entity, "PhotoBox"))
        {
            continue;
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
