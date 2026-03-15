#include "game_scene_internal.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
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

    const float shutterT = Clamp01(m_shutterFlashRemaining / kShutterFlashSeconds);
    const float frameInset = 10.0f * shutterT * viewScale;
    const float innerX = drawX + frameInset;
    const float innerY = drawY + frameInset;
    const float innerWidth = std::max(8.0f, drawWidth - frameInset * 2.0f);
    const float innerHeight = std::max(8.0f, drawHeight - frameInset * 2.0f);

    Shader_ResetStyle();
    Shader_SetOutline(
        0.42f + shutterT * 0.48f,
        0.78f + shutterT * 0.18f,
        1.0f,
        1.0f,
        1.6f + shutterT * 1.2f);
    Shader_SetTint(0.14f + shutterT * 0.42f, 0.28f + shutterT * 0.42f, 0.38f + shutterT * 0.42f, 0.18f + shutterT * 0.24f);
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
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, GetViewOriginX(), GetViewOriginY(), GetViewWidth(), GetViewHeight(), 0.0f, 0.0f, 1.0f, 1.0f);

        const float lineWidth = 6.0f + shutterT * 10.0f;
        const float lineHeight = std::max(12.0f, 32.0f * shutterT * viewScale);
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.24f + shutterT * 0.40f);
        SpriteDraw(m_whiteTexture, drawX, drawY - lineHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX, drawY + drawHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX - lineHeight, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX + drawWidth, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_ResetStyle();
}

void GameScene::DrawPhotoPlacementPreview() const
{
    if (!m_photoPlacementActive || m_capturedPhotoItems.empty())
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    for (const auto& item : m_capturedPhotoItems)
    {
        const float drawX = viewOriginX + ((m_photoPlacementX + item.relativeX) - m_cameraX) * viewScale;
        const float drawY = viewOriginY + (m_photoPlacementY + item.relativeY) * viewScale;
        const float drawWidth = item.width * viewScale;
        const float drawHeight = item.height * viewScale;

        Shader_ResetStyle();
        if (m_photoPlacementValid)
        {
            Shader_SetOutline(0.32f, 0.92f, 1.0f, 1.0f, 1.6f);
            Shader_SetTint(item.tintR, item.tintG, item.tintB, 0.55f);
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
            0.0f);
    }

    Shader_ResetStyle();
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
    else if (tag && tag->tag == "PhotoBox")
    {
        Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
    }
    else if (tag && tag->tag == "Player")
    {
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
        transform->rotation);

    if (m_showCollisionDebug && tag && (tag->tag == "Player" || tag->tag == "PhotoSource" || tag->tag == "PhotoBox"))
    {
        unsigned int color = GetColor(255, 255, 255);
        if (tag->tag == "Player")
        {
            color = GetColor(255, 96, 96);
        }
        else if (tag->tag == "PhotoSource")
        {
            color = GetColor(96, 255, 255);
        }
        else if (tag->tag == "PhotoBox")
        {
            color = GetColor(255, 220, 96);
        }

        DrawWorldRectOutline(
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale,
            m_cameraX,
            color);
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

    Shader_SetTint(0.18f, 0.18f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY - 10.0f, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, panelBottom, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    m_tileMap.Draw(m_tileTexture, viewOriginX - m_cameraX * viewScale, viewOriginY, viewScale);

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const
{
    const float playerWidth = playerTransform.width * playerTransform.scale;
    const float playerHeight = playerTransform.height * playerTransform.scale;
    width = std::clamp(playerWidth * 1.85f, 120.0f, 196.0f);
    height = std::clamp(playerHeight * 1.15f, 96.0f, 168.0f);

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
