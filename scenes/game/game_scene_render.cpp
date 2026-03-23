#include "game_scene_internal.h"
#include "photo_filter_rules.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    float EaseOutCubic(float t)
    {
        const float clamped = Clamp01(t);
        const float inv = 1.0f - clamped;
        return 1.0f - inv * inv * inv;
    }

    float EaseOutBack(float t)
    {
        const float clamped = Clamp01(t);
        constexpr float c1 = 1.70158f;
        constexpr float c3 = c1 + 1.0f;
        const float shifted = clamped - 1.0f;
        return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
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

    bool DrawSlopeTriangle(float x, float y, float width, float height, int tileValue, const TintComponent* tint, bool flipX, float rotation)
    {
        const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue);
        if (!tint || !triangle.isTriangle)
        {
            return false;
        }

        const int color = GetColor(
            static_cast<int>(std::round(tint->r * 255.0f)),
            static_cast<int>(std::round(tint->g * 255.0f)),
            static_cast<int>(std::round(tint->b * 255.0f)));
        const bool risesRight = flipX ? !triangle.risesRight : triangle.risesRight;
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        if (risesRight)
        {
            ax = x;
            ay = y + height;
            bx = x + width;
            by = y + height;
            cx = x + width;
            cy = y;
        }
        else
        {
            ax = x;
            ay = y;
            bx = x;
            by = y + height;
            cx = x + width;
            cy = y + height;
        }

        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        RotatePoint(centerX, centerY, rotation, ax, ay);
        RotatePoint(centerX, centerY, rotation, bx, by);
        RotatePoint(centerX, centerY, rotation, cx, cy);
        DrawTriangleAA(ax, ay, bx, by, cx, cy, color, TRUE);
        return true;
    }

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

void GameScene::DrawEffects() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();

    for (const auto& particle : m_effects.barrelDebris)
    {
        const float lifeT = Clamp01(particle.life / std::max(0.001f, particle.maxLife));
        Shader_ResetStyle();
        Shader_SetTint(particle.r, particle.g, particle.b, lifeT * 0.85f);
        SpriteDraw(
            m_whiteTexture,
            viewOriginX + (particle.x - m_flow.cameraX) * viewScale,
            viewOriginY + particle.y * viewScale,
            particle.size * viewScale,
            particle.size * viewScale,
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            particle.rotation);
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
    float drawX = viewOriginX + (transform->x - m_flow.cameraX) * viewScale;
    float drawY = viewOriginY + transform->y * viewScale;
    float drawWidth = transform->width * transform->scale * viewScale;
    float drawHeight = transform->height * transform->scale * viewScale;
    if (drawX + drawWidth < viewOriginX || drawX > viewOriginX + viewWidth)
    {
        return;
    }

    Shader_ResetStyle();
    float alphaMultiplier = 1.0f;

    const auto* tag = entity.GetComponent<TagComponent>();
    if (tag && tag->tag == "Goal")
    {
        Shader_SetOutline(
            m_flow.goalUnlocked ? 0.28f : 0.92f,
            m_flow.goalUnlocked ? 1.0f : 0.22f,
            m_flow.goalUnlocked ? 0.42f : 0.18f,
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
        if (const auto* lifetime = entity.GetComponent<PhotoCopyLifetimeComponent>())
        {
            const float totalLifetime = std::max(0.001f, lifetime->GetLifetimeSeconds());
            alphaMultiplier = Clamp01(lifetime->GetRemainingSeconds() / totalLifetime);
        }

        if (const auto* pasteAnimation = entity.GetComponent<PhotoPasteAnimationComponent>())
        {
            const float progress = pasteAnimation->GetNormalizedProgress();
            const float settleT = EaseOutBack(progress);
            const float slamT = EaseOutCubic(progress);
            const float animationScale = 0.82f + 0.18f * settleT;
            const float centerX = drawX + drawWidth * 0.5f;
            const float bottomY = drawY + drawHeight;
            const float animatedWidth = drawWidth * animationScale;
            const float animatedHeight = drawHeight * (1.12f - 0.12f * slamT);
            drawX = centerX - animatedWidth * 0.5f;
            drawY = bottomY - animatedHeight - (1.0f - slamT) * 18.0f * viewScale;
            drawWidth = animatedWidth;
            drawHeight = animatedHeight;
            alphaMultiplier *= 0.45f + 0.55f * slamT;
            Shader_SetFlash(1.0f, 0.98f, 0.92f, 1.0f, (1.0f - progress) * 0.28f);
        }

        const auto* photoLayer = entity.GetComponent<PhotoCopyLayerComponent>();
        const auto* photoOrigin = entity.GetComponent<PhotoCopyOriginComponent>();
        const auto* tint = entity.GetComponent<TintComponent>();
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
                const bool looksLikePrintedPhotoPaper =
                    photoOrigin &&
                    photoOrigin->origin == PhotoCopyOrigin::Generic &&
                    tint &&
                    tint->r > 0.9f &&
                    tint->g > 0.9f &&
                    tint->b > 0.85f;
                if (looksLikePrintedPhotoPaper)
                {
                    Shader_SetOutline(0.90f, 0.84f, 0.72f, 1.0f, 1.4f);
                    Shader_SetTint(0.98f, 0.96f, 0.90f, 0.92f);
                }
                else if (tint && tint->r < 0.2f && tint->g < 0.2f && tint->b < 0.2f)
                {
                    Shader_SetOutline(0.22f, 0.22f, 0.24f, 1.0f, 1.2f);
                    Shader_SetTint(0.10f, 0.12f, 0.14f, 0.94f);
                }
                else
                {
                    Shader_SetTint(0.64f, 0.72f, 0.84f, 0.44f);
                }
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
    else if (tag && tag->tag == "Bullet")
    {
        // 3/23追加：弾を三角形で描画(田之上俊)
        const auto* projectile = entity.GetComponent<ProjectileComponent>();
        if (projectile)
        {
            const bool goingRight = projectile->GetVelocityX() >= 0.0f;
            const int color = GetColor(255, 230, 50);

            // 当たり判定の矩形範囲内に三角形を描画
            if (goingRight)
            {
                // 右向き：左辺が底辺、右側が尖る
                DrawTriangleAA(
                    drawX, drawY,                          // 左上
                    drawX, drawY + drawHeight,             // 左下
                    drawX + drawWidth, drawY + drawHeight * 0.5f, // 右中央（尖り）
                    color, TRUE);
            }
            else
            {
                // 左向き：右辺が底辺、左側が尖る
                DrawTriangleAA(
                    drawX + drawWidth, drawY,              // 右上
                    drawX + drawWidth, drawY + drawHeight, // 右下
                    drawX, drawY + drawHeight * 0.5f,      // 左中央（尖り）
                    color, TRUE);
            }
            Shader_ResetStyle();
            return;
        }
    }

    else if (tag && tag->tag == "Player")
    {
        for (size_t index = m_player.afterimages.size(); index > 0; --index)
        {
            const auto& afterimage = m_player.afterimages[index - 1];
            const float afterimageDrawX = viewOriginX + (afterimage.x - m_flow.cameraX) * viewScale;
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
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a * alphaMultiplier);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, alphaMultiplier);
    }

    if (!DrawSlopeTriangle(
            drawX,
            drawY,
            drawWidth,
            drawHeight,
            entity.GetComponent<PhotoCopyTileValueComponent>() ? entity.GetComponent<PhotoCopyTileValueComponent>()->tileValue : 0,
            entity.GetComponent<TintComponent>(),
            sprite->GetFlipX(),
            transform->rotation))
    {
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
    }

    if (m_debug.showCollisionDebug && (entity.GetComponent<PhotoFilterComponent>() || (tag && (tag->tag == "Player" || tag->tag == "PhotoSource" || tag->tag == "PhotoBox"))))
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
            m_flow.cameraX,
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
            photoEffect ? GetPhotoFilterThemeLabel(photoEffect->GetTheme()) : "None");
    }


    Shader_ResetStyle();
}

