#include "game_scene_internal.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    void GetFilterThemeOverlayColor(PhotoFilterTheme theme, float& r, float& g, float& b)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            r = 1.0f;
            g = 0.20f;
            b = 0.08f;
            break;
        case PhotoFilterTheme::Cold:
            r = 0.14f;
            g = 0.56f;
            b = 1.0f;
            break;
        case PhotoFilterTheme::Invert:
            r = 0.86f;
            g = 0.86f;
            b = 0.90f;
            break;
        case PhotoFilterTheme::Sepia:
            r = 0.78f;
            g = 0.60f;
            b = 0.36f;
            break;
        case PhotoFilterTheme::None:
        default:
            r = 1.0f;
            g = 1.0f;
            b = 1.0f;
            break;
        }
    }

    void DrawWorldRectOutline(float worldX, float worldY, float worldWidth, float worldHeight, float cameraX, unsigned int color)
    {
        const float viewScale = GetViewScale();
        const float viewOriginX = GetViewOriginX();
        const float viewOriginY = GetViewOriginY();
        const int left = static_cast<int>(std::round(viewOriginX + (worldX - cameraX) * viewScale));
        const int top = static_cast<int>(std::round(viewOriginY + worldY * viewScale));
        const int right = static_cast<int>(std::round(viewOriginX + (worldX + worldWidth - cameraX) * viewScale));
        const int bottom = static_cast<int>(std::round(viewOriginY + (worldY + worldHeight) * viewScale));
        DrawBox(left, top, right, bottom, color, FALSE);
    }

    const char* GetStageGuideText(float playerX)
    {
        static_cast<void>(playerX);
        return "Sandbox: choose filter 1-4, capture, then place up to three copy groups.";
    }

    const char* GetRoleLabel(PhotoCopyRole role)
    {
        switch (role)
        {
        case PhotoCopyRole::Hazard:
            return "Hazard";
        case PhotoCopyRole::GoalRelay:
            return "Goal";
        case PhotoCopyRole::Pickup:
            return "Pickup";
        case PhotoCopyRole::Ally:
            return "Ally";
        case PhotoCopyRole::Solid:
        default:
            return "Solid";
        }
    }

    const char* GetLayerLabel(PhotoCopyLayer layer)
    {
        switch (layer)
        {
        case PhotoCopyLayer::Background:
            return "Background";
        case PhotoCopyLayer::Shadow:
            return "Shadow";
        case PhotoCopyLayer::Foreground:
        default:
            return "Foreground";
        }
    }

    const char* GetLayerEffectText(PhotoCopyLayer layer)
    {
        switch (layer)
        {
        case PhotoCopyLayer::Background:
            return "Visible only / pass through";
        case PhotoCopyLayer::Shadow:
            return "Black shadow / pass through";
        case PhotoCopyLayer::Foreground:
        default:
            return "Solid in world";
        }
    }

    const char* GetFilterThemeLabel(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            return "Hot";
        case PhotoFilterTheme::Cold:
            return "Cold";
        case PhotoFilterTheme::Invert:
            return "Invert";
        case PhotoFilterTheme::Sepia:
            return "Sepia";
        case PhotoFilterTheme::None:
        default:
            return "None";
        }
    }

    void ApplyPreviewFilterTheme(CapturedPhotoItem& item)
    {
        switch (item.appliedTheme)
        {
        case PhotoFilterTheme::Hot:
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 1.0f;
            item.tintG = 0.34f;
            item.tintB = 0.12f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Cold:
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.76f;
            item.tintG = 0.90f;
            item.tintB = 1.0f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Invert:
            item.role = item.origin == PhotoCopyOrigin::Enemy ? PhotoCopyRole::Ally : PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.62f;
            item.tintG = 0.62f;
            item.tintB = 0.64f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::Sepia:
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.tintR = 0.76f;
            item.tintG = 0.58f;
            item.tintB = 0.34f;
            item.tintA = 1.0f;
            break;
        case PhotoFilterTheme::None:
        default:
            break;
        }
    }

    const char* GetFilterThemeEffectText(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Hot:
            return "Makes copies dangerous";
        case PhotoFilterTheme::Cold:
            return "Freezes copies into footing";
        case PhotoFilterTheme::Invert:
            return "Enemy shots become allies";
        case PhotoFilterTheme::Sepia:
            return "Rewinds the target in time";
        case PhotoFilterTheme::None:
        default:
            return "Keeps captured properties";
        }
    }
}

void GameScene::DrawCaptureOverlay() const
{
    if (!m_cameraMode && m_shutterFlashRemaining <= 0.0f)
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
    const float drawX = viewOriginX + (frameX - m_cameraX) * viewScale;
    const float drawY = viewOriginY + frameY * viewScale;
    const float drawWidth = frameWidth * viewScale;
    const float drawHeight = frameHeight * viewScale;

    const float shutterT = Clamp01(m_shutterFlashRemaining / gShutterFlashSeconds);
    const float frameInset = 10.0f * shutterT * viewScale;
    const float innerX = drawX + frameInset;
    const float innerY = drawY + frameInset;
    const float innerWidth = std::max(8.0f, drawWidth - frameInset * 2.0f);
    const float innerHeight = std::max(8.0f, drawHeight - frameInset * 2.0f);
    float overlayR = 1.0f;
    float overlayG = 1.0f;
    float overlayB = 1.0f;
    GetFilterThemeOverlayColor(m_photo.capture.selectedTheme, overlayR, overlayG, overlayB);

    Shader_ResetStyle();
    Shader_SetOutline(
        overlayR * (0.70f + shutterT * 0.30f),
        overlayG * (0.70f + shutterT * 0.22f),
        overlayB * (0.70f + shutterT * 0.22f),
        1.0f,
        2.0f + shutterT * 1.4f);
    Shader_SetTint(
        overlayR * (0.24f + shutterT * 0.44f),
        overlayG * (0.24f + shutterT * 0.36f),
        overlayB * (0.24f + shutterT * 0.44f),
        0.30f + shutterT * 0.24f);
    SpriteDraw(m_whiteTexture, innerX, innerY, innerWidth, innerHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    if (Entity* target = FindCaptureTarget(*transform))
    {
        if (const auto* targetTransform = target->GetComponent<TransformComponent>())
        {
            const float targetDrawX = viewOriginX + (targetTransform->x - m_cameraX) * viewScale;
            const float targetDrawY = viewOriginY + targetTransform->y * viewScale;
            const float targetDrawWidth = targetTransform->width * targetTransform->scale * viewScale;
            const float targetDrawHeight = targetTransform->height * targetTransform->scale * viewScale;
            Shader_SetOutline(0.34f, 1.0f, 0.48f, 1.0f, 1.8f);
            Shader_SetTint(0.10f, 0.30f, 0.14f, 0.12f);
            SpriteDraw(m_whiteTexture, targetDrawX, targetDrawY, targetDrawWidth, targetDrawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    if (m_shutterFlashRemaining > 0.0f)
    {
        Shader_ResetStyle();
        Shader_SetTint(overlayR, overlayG, overlayB, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, GetViewOriginX(), GetViewOriginY(), GetViewWidth(), GetViewHeight(), 0.0f, 0.0f, 1.0f, 1.0f);

        const float lineWidth = 6.0f + shutterT * 10.0f;
        const float lineHeight = std::max(12.0f, 32.0f * shutterT * viewScale);
        Shader_SetTint(overlayR, overlayG, overlayB, 0.24f + shutterT * 0.40f);
        SpriteDraw(m_whiteTexture, drawX, drawY - lineHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX, drawY + drawHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX - lineHeight, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX + drawWidth, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_ResetStyle();
}

void GameScene::DrawTuningPanel() const
{
    if (!m_showTuningPanel)
    {
        return;
    }

    const int left = 32;
    const int top = 32;
    const int width = 360;
    const int height = 300;

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
        { "Coyote", gCoyoteTimeSeconds },
        { "Ground Snap", gGroundSnapDistance },
        { "Capture Width", gCaptureWidthScale },
        { "Capture Height", gCaptureHeightScale },
        { "Pickup Bonus", gPickupTimeBonus },
    };

    for (int index = 0; index < static_cast<int>(sizeof(entries) / sizeof(entries[0])); ++index)
    {
        const int y = top + 72 + index * 22;
        const unsigned int color = index == m_tuningSelection
            ? GetColor(255, 240, 120)
            : GetColor(235, 235, 235);
        DrawFormatString(left + 16, y, color, "%c %-14s : %7.2f", index == m_tuningSelection ? '>' : ' ', entries[index].label, entries[index].value);
    }
}

void GameScene::DrawPhotoPlacementPreview() const
{
    if (!m_photo.placement.active || m_photo.capture.items.empty())
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    std::vector<CapturedPhotoItem> previewItems = m_photo.capture.items;
    if (m_photo.placement.flipX)
    {
        for (auto& item : previewItems)
        {
            item.relativeX = m_photo.placement.width - item.relativeX - item.width;
            item.flipX = !item.flipX;
        }
    }
    if (m_photo.placement.bridgeEnabled && previewItems.size() >= 2)
    {
        const std::vector<CapturedPhotoItem> baseItems = previewItems;
        constexpr float kSegmentSize = 18.0f;
        for (size_t index = 1; index < baseItems.size(); ++index)
        {
            const auto& a = baseItems[index - 1];
            const auto& b = baseItems[index];
            const float ax = a.relativeX + a.width * 0.5f;
            const float ay = a.relativeY + a.height * 0.5f;
            const float bx = b.relativeX + b.width * 0.5f;
            const float by = b.relativeY + b.height * 0.5f;
            const float length = std::max(std::fabs(bx - ax), std::fabs(by - ay));
            const int steps = std::max(1, static_cast<int>(length / kSegmentSize));
            for (int step = 1; step < steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                CapturedPhotoItem bridge;
                bridge.textureId = m_whiteTexture;
                bridge.appliedTheme = m_photo.capture.capturedTheme;
                bridge.relativeX = std::lerp(ax, bx, t) - kSegmentSize * 0.5f;
                bridge.relativeY = std::lerp(ay, by, t) - kSegmentSize * 0.5f;
                bridge.width = kSegmentSize;
                bridge.height = kSegmentSize;
                bridge.sourceX = 0.0f;
                bridge.sourceY = 0.0f;
                bridge.sourceWidth = 1.0f;
                bridge.sourceHeight = 1.0f;
                bridge.tintR = 0.90f;
                bridge.tintG = 0.96f;
                bridge.tintB = 1.0f;
                bridge.tintA = 0.92f;
                previewItems.push_back(bridge);
            }
        }
    }

    for (const auto& item : previewItems)
    {
        CapturedPhotoItem previewItem = item;
        ApplyPreviewFilterTheme(previewItem);
        const float drawX = viewOriginX + ((m_photo.placement.x + item.relativeX) - m_cameraX) * viewScale;
        const float drawY = viewOriginY + (m_photo.placement.y + item.relativeY) * viewScale;
        const float drawWidth = item.width * viewScale;
        const float drawHeight = item.height * viewScale;

        Shader_ResetStyle();
        if (m_photo.placement.valid)
        {
            switch (previewItem.appliedTheme)
            {
            case PhotoFilterTheme::Hot:
                Shader_SetOutline(1.0f, 0.42f, 0.18f, 1.0f, 1.8f);
                break;
            case PhotoFilterTheme::Cold:
                Shader_SetOutline(0.76f, 0.94f, 1.0f, 1.0f, 1.8f);
                break;
            case PhotoFilterTheme::Invert:
                Shader_SetOutline(0.86f, 0.86f, 0.92f, 1.0f, 1.8f);
                break;
            case PhotoFilterTheme::Sepia:
                Shader_SetOutline(0.90f, 0.72f, 0.42f, 1.0f, 1.8f);
                break;
            case PhotoFilterTheme::None:
            default:
                Shader_SetOutline(0.32f, 0.92f, 1.0f, 1.0f, 1.6f);
                break;
            }
            Shader_SetTint(previewItem.tintR, previewItem.tintG, previewItem.tintB, 0.55f);
        }
        else
        {
            Shader_SetOutline(1.0f, 0.24f, 0.24f, 1.0f, 1.6f);
            Shader_SetTint(1.0f, 0.24f, 0.24f, 0.42f);
        }

        SpriteDraw(
            item.textureId >= 0 ? item.textureId : m_tileTexture,
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            item.sourceX,
            item.sourceY,
            item.sourceWidth,
            item.sourceHeight,
            item.flipX,
            0.0f);
    }

    DrawFormatString(
        static_cast<int>(viewOriginX + 24.0f),
        static_cast<int>(viewOriginY + 24.0f),
        GetColor(230, 240, 255),
        "Filter:%s  Layer:%s  Flip:%s  Bridge:%s",
        GetFilterThemeLabel(m_photo.capture.capturedTheme),
        GetLayerLabel(m_photo.placement.layer),
        m_photo.placement.flipX ? "On" : "Off",
        m_photo.placement.bridgeEnabled ? "On" : "Off");
    DrawFormatString(
        static_cast<int>(viewOriginX + 24.0f),
        static_cast<int>(viewOriginY + 48.0f),
        GetColor(190, 220, 255),
        "%s  Groups:%d/3  Keys:Q/F/B",
        GetLayerEffectText(m_photo.placement.layer),
        m_photo.groups.activeGroupCount);

    Shader_ResetStyle();
}

void GameScene::DrawPhotoBoxesByLayer(PhotoCopyLayer layer) const
{
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        const PhotoCopyLayer currentLayer = photoLayer ? photoLayer->layer : PhotoCopyLayer::Foreground;
        if (currentLayer != layer)
        {
            continue;
        }

        DrawEntity(*entity);
    }
}

void GameScene::DrawEntity(const Entity& entity) const
{
    const auto* transform = entity.GetComponent<TransformComponent>();
    const auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    if (!transform || !sprite)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float drawX = viewOriginX + (transform->x - m_cameraX) * viewScale;
    const float drawY = viewOriginY + transform->y * viewScale;
    const float drawWidth = transform->width * transform->scale * viewScale;
    const float drawHeight = transform->height * transform->scale * viewScale;
    if (drawX + drawWidth < viewOriginX || drawX > viewOriginX + viewWidth)
    {
        return;
    }

    Shader_ResetStyle();

    const auto* tag = entity.GetComponent<TagComponent>();
    if (tag && tag->tag == "Goal")
    {
        Shader_SetOutline(
            m_goalUnlocked ? 0.28f : 0.92f,
            m_goalUnlocked ? 1.0f : 0.22f,
            m_goalUnlocked ? 0.42f : 0.18f,
            1.0f,
            1.5f);
    }
    else if (tag && tag->tag == "PhotoSource")
    {
        Shader_SetOutline(0.18f, 0.90f, 1.0f, 1.0f, 1.4f);
    }
    else if (entity.GetComponent<PhotoFilterComponent>())
    {
        if (const auto* filter = entity.GetComponent<PhotoFilterComponent>())
        {
            switch (filter->GetTheme())
            {
            case PhotoFilterTheme::Hot:
                Shader_SetOutline(1.0f, 0.40f, 0.18f, 1.0f, 1.9f);
                Shader_SetFlash(1.0f, 0.28f, 0.10f, 1.0f, 0.26f);
                break;
            case PhotoFilterTheme::Cold:
                Shader_SetOutline(0.70f, 0.92f, 1.0f, 1.0f, 1.9f);
                Shader_SetFlash(0.18f, 0.74f, 1.0f, 1.0f, 0.18f);
                break;
            case PhotoFilterTheme::Invert:
                Shader_SetOutline(0.92f, 0.92f, 0.96f, 1.0f, 1.8f);
                Shader_SetFlash(0.72f, 0.72f, 0.78f, 1.0f, 0.16f);
                break;
            case PhotoFilterTheme::Sepia:
                Shader_SetOutline(0.88f, 0.66f, 0.34f, 1.0f, 1.9f);
                Shader_SetFlash(0.74f, 0.56f, 0.28f, 1.0f, 0.16f);
                break;
            case PhotoFilterTheme::None:
            default:
                Shader_SetOutline(0.26f, 1.0f, 0.92f, 1.0f, 1.8f);
                Shader_SetFlash(0.18f, 0.92f, 0.88f, 1.0f, 0.22f);
                break;
            }
        }
    }
    else if (tag && tag->tag == "PhotoBox")
    {
        const auto* photoLayer = entity.GetComponent<PhotoCopyLayerComponent>();
        if (const auto* photoRole = entity.GetComponent<PhotoCopyRoleComponent>())
        {
            switch (photoRole->role)
            {
            case PhotoCopyRole::Hazard:
                Shader_SetFlash(1.0f, 0.28f, 0.22f, 1.0f, 0.24f);
                break;
            case PhotoCopyRole::GoalRelay:
                Shader_SetOutline(0.96f, 0.88f, 0.22f, 1.0f, 1.6f);
                break;
            case PhotoCopyRole::Pickup:
                Shader_SetOutline(0.18f, 0.90f, 1.0f, 1.0f, 1.6f);
                break;
            case PhotoCopyRole::Ally:
                Shader_SetOutline(0.78f, 0.94f, 0.82f, 1.0f, 1.8f);
                break;
            case PhotoCopyRole::Solid:
            default:
                Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
                break;
            }
        }
        else
        {
            Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
        }

        if (photoLayer)
        {
            if (photoLayer->layer == PhotoCopyLayer::Background)
            {
                Shader_SetTint(0.64f, 0.72f, 0.84f, 0.44f);
            }
            else if (photoLayer->layer == PhotoCopyLayer::Shadow)
            {
                Shader_SetOutline(0.04f, 0.04f, 0.06f, 1.0f, 1.6f);
                Shader_SetTint(0.02f, 0.02f, 0.03f, 0.72f);
            }
        }

        if (const auto* effect = entity.GetComponent<PhotoCopyEffectComponent>())
        {
            switch (effect->GetTheme())
            {
            case PhotoFilterTheme::Hot:
                Shader_SetOutline(1.0f, 0.52f, 0.20f, 1.0f, 2.1f);
                Shader_SetFlash(1.0f, 0.30f, 0.12f, 1.0f, 0.28f);
                break;
            case PhotoFilterTheme::Cold:
                Shader_SetOutline(0.74f, 0.92f, 1.0f, 1.0f, 2.2f);
                Shader_SetFlash(0.34f, 0.74f, 1.0f, 1.0f, 0.12f);
                break;
            case PhotoFilterTheme::Invert:
                Shader_SetOutline(0.90f, 0.94f, 0.92f, 1.0f, 2.0f);
                Shader_SetFlash(0.78f, 0.96f, 0.84f, 1.0f, 0.16f);
                break;
            case PhotoFilterTheme::Sepia:
                Shader_SetOutline(0.92f, 0.72f, 0.44f, 1.0f, 2.0f);
                Shader_SetFlash(0.82f, 0.64f, 0.34f, 1.0f, 0.14f);
                break;
            case PhotoFilterTheme::None:
            default:
                break;
            }
        }
    }
    else if (tag && tag->tag == "Player")
    {
        for (size_t index = m_playerAfterimages.size(); index > 0; --index)
        {
            const auto& afterimage = m_playerAfterimages[index - 1];
            const float afterimageDrawX = viewOriginX + (afterimage.x - m_cameraX) * viewScale;
            const float afterimageDrawY = viewOriginY + afterimage.y * viewScale;
            const float afterimageDrawWidth = transform->width * afterimage.scale * viewScale;
            const float afterimageDrawHeight = transform->height * afterimage.scale * viewScale;
            const float alpha = Clamp01(afterimage.life / 0.18f) * 0.32f;
            Shader_ResetStyle();
            Shader_SetOutline(0.30f, 0.86f, 1.0f, 1.0f, 1.4f);
            Shader_SetTint(0.42f, 0.88f, 1.0f, alpha);
            SpriteDraw(
                sprite->GetTextureId(),
                afterimageDrawX,
                afterimageDrawY,
                afterimageDrawWidth,
                afterimageDrawHeight,
                sprite->GetSourceX(),
                sprite->GetSourceY(),
                sprite->GetSourceWidth(),
                sprite->GetSourceHeight(),
                afterimage.flipX,
                afterimage.rotation);
        }

        if (const auto* cooldown = entity.GetComponent<DamageCooldownComponent>())
        {
            if (cooldown->GetRemainingSeconds() > 0.0f)
            {
                const float flash = 0.40f + 0.60f * std::sin(cooldown->GetRemainingSeconds() * 28.0f);
                Shader_SetFlash(1.0f, 0.30f, 0.22f, 1.0f, Clamp01(flash));
            }
        }
    }

    if (const auto* tint = entity.GetComponent<TintComponent>())
    {
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    }

    SpriteDraw(
        sprite->GetTextureId(),
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        sprite->GetSourceX(),
        sprite->GetSourceY(),
        sprite->GetSourceWidth(),
        sprite->GetSourceHeight(),
        sprite->GetFlipX(),
        transform->rotation);

    if (m_showCollisionDebug && (entity.GetComponent<PhotoFilterComponent>() || (tag && (tag->tag == "Player" || tag->tag == "PhotoSource" || tag->tag == "PhotoBox"))))
    {
        unsigned int color = GetColor(255, 255, 255);
        if (tag && tag->tag == "Player")
        {
            color = GetColor(255, 96, 96);
        }
        else if (tag && tag->tag == "PhotoSource")
        {
            color = GetColor(96, 255, 255);
        }
        else if (tag && tag->tag == "PhotoBox")
        {
            color = GetColor(255, 220, 96);
        }
        else if (entity.GetComponent<PhotoFilterComponent>())
        {
            color = GetColor(96, 255, 220);
        }

        DrawWorldRectOutline(
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale,
            m_cameraX,
            color);
    }

    if ((tag && (tag->tag == "PhotoSource" || tag->tag == "Hazard")) || entity.GetComponent<PhotoFilterComponent>())
    {
        const float textX = drawX;
        const float textY = drawY - 18.0f;
        PhotoCopyRole labelRole = PhotoCopyRole::Solid;
        if (tag && tag->tag == "Hazard")
        {
            labelRole = PhotoCopyRole::Hazard;
        }
        else if (entity.GetComponent<PhotoFilterComponent>())
        {
            if (const auto* filter = entity.GetComponent<PhotoFilterComponent>())
            {
                labelRole = filter->GetOutputRole();
            }
        }
        else if (const auto* tint = entity.GetComponent<TintComponent>())
        {
            labelRole = GetRoleFromTint(tint->r, tint->g, tint->b);
        }

        PhotoCopyLayer labelLayer = PhotoCopyLayer::Foreground;
        if (entity.GetComponent<PhotoFilterComponent>())
        {
            if (const auto* filter = entity.GetComponent<PhotoFilterComponent>())
            {
                labelLayer = filter->GetOutputLayer();
            }
        }
        else if (const auto* tint = entity.GetComponent<TintComponent>())
        {
            labelLayer = GetLayerFromTint(tint->r, tint->g, tint->b);
        }

        const char* header = "Filter";
        if (const auto* filter = entity.GetComponent<PhotoFilterComponent>())
        {
            switch (filter->GetTheme())
            {
            case PhotoFilterTheme::Hot:
                header = "Hot";
                break;
            case PhotoFilterTheme::Cold:
                header = "Cold";
                break;
            case PhotoFilterTheme::Invert:
                header = "Invert";
                break;
            case PhotoFilterTheme::Sepia:
                header = "Sepia";
                break;
            case PhotoFilterTheme::None:
            default:
                header = "Filter";
                break;
            }
        }

        DrawFormatString(
            static_cast<int>(textX),
            static_cast<int>(textY),
            GetColor(245, 248, 255),
            "%s : %s / %s",
            header,
            GetRoleLabel(labelRole),
            GetLayerLabel(labelLayer));
    }
    else if (tag && tag->tag == "PhotoBox")
    {
        const auto* photoRole = entity.GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity.GetComponent<PhotoCopyLayerComponent>();
        const auto* photoEffect = entity.GetComponent<PhotoCopyEffectComponent>();
        DrawFormatString(
            static_cast<int>(drawX),
            static_cast<int>(drawY - 18.0f),
            GetColor(255, 248, 220),
            "%s / %s / %s",
            photoRole ? GetRoleLabel(photoRole->role) : "Solid",
            photoLayer ? GetLayerLabel(photoLayer->layer) : "Foreground",
            photoEffect ? GetFilterThemeLabel(photoEffect->GetTheme()) : "None");
    }

    Shader_ResetStyle();
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
        GetFilterThemeOverlayColor(m_photo.capture.selectedTheme, filterR, filterG, filterB);
        Shader_SetTint(filterR, filterG, filterB, 0.07f);
        SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    {
        const float worldLeft = m_cameraX;
        const float worldRight = m_cameraX + gCameraViewWidth;
        const float gridSpacing = m_tileMap.GetTileSize();
        const unsigned int majorColor = GetColor(72, 188, 128);
        const unsigned int minorColor = GetColor(38, 112, 82);

        for (float worldX = std::floor(worldLeft / gridSpacing) * gridSpacing; worldX <= worldRight; worldX += gridSpacing)
        {
            const float screenX = viewOriginX + (worldX - m_cameraX) * viewScale;
            const int x = static_cast<int>(std::round(screenX));
            const bool major = std::fmod(std::fabs(worldX), gridSpacing * 4.0f) < 0.5f ||
                (gridSpacing * 4.0f - std::fmod(std::fabs(worldX), gridSpacing * 4.0f)) < 0.5f;
            DrawLine(x, static_cast<int>(viewOriginY), x, static_cast<int>(viewOriginY + viewHeight),
                major ? majorColor : minorColor);
            if (!major)
            {
                DrawLine(x + 1, static_cast<int>(viewOriginY), x + 1, static_cast<int>(viewOriginY + viewHeight),
                    GetColor(22, 56, 40));
            }
        }

        for (float worldY = 0.0f; worldY <= gCameraViewHeight; worldY += gridSpacing)
        {
            const float screenY = viewOriginY + worldY * viewScale;
            const int y = static_cast<int>(std::round(screenY));
            const bool major = std::fmod(worldY, gridSpacing * 4.0f) < 0.5f ||
                (gridSpacing * 4.0f - std::fmod(worldY, gridSpacing * 4.0f)) < 0.5f;
            DrawLine(static_cast<int>(viewOriginX), y, static_cast<int>(viewOriginX + viewWidth), y,
                major ? majorColor : minorColor);
            if (!major)
            {
                DrawLine(static_cast<int>(viewOriginX), y + 1, static_cast<int>(viewOriginX + viewWidth), y + 1,
                    GetColor(22, 56, 40));
            }
        }
    }

    Shader_SetTint(0.18f, 0.18f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY - 10.0f, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, panelBottom, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    m_tileMap.Draw(m_tileTexture, viewOriginX - m_cameraX * viewScale, viewOriginY, viewScale);

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
        GetFilterThemeOverlayColor(m_photo.capture.selectedTheme, filterR, filterG, filterB);
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
            GetFilterThemeLabel(m_photo.capture.selectedTheme));
        DrawFormatString(
            panelX + 56,
            panelY + 32,
            GetColor(180, 210, 235),
            "%s",
            GetFilterThemeEffectText(m_photo.capture.selectedTheme));
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
    const float playerWidth = playerTransform.width * playerTransform.scale;
    const float playerHeight = playerTransform.height * playerTransform.scale;
    width = std::clamp(playerWidth * gCaptureWidthScale, 120.0f, 196.0f);
    height = std::clamp(playerHeight * gCaptureHeightScale, 96.0f, 168.0f);

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float cursorWorldX =
        ((static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale) + m_cameraX;
    const float cursorWorldY =
        (static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale;

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
