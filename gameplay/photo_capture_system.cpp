#include "photo_capture_system.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game_scene_internal.h"
#include "photo_filter_rules.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kDevelopedPhotoPreviewSeconds = 4.2f;

    using OutlinePoint = CapturedPhotoItem::OutlinePoint;

    OutlinePoint LerpPoint(const OutlinePoint& a, const OutlinePoint& b, float t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
        };
    }

    void ClipPolygonAgainstEdge(
        const std::vector<OutlinePoint>& input,
        std::vector<OutlinePoint>& output,
        auto&& isInside,
        auto&& intersect)
    {
        output.clear();
        if (input.empty())
        {
            return;
        }

        OutlinePoint previous = input.back();
        bool previousInside = isInside(previous);
        for (const OutlinePoint& current : input)
        {
            const bool currentInside = isInside(current);
            if (currentInside != previousInside)
            {
                output.push_back(intersect(previous, current));
            }
            if (currentInside)
            {
                output.push_back(current);
            }
            previous = current;
            previousInside = currentInside;
        }
    }

    bool BuildCapturedOutlineFromEntity(
        const Entity& entity,
        float localLeft,
        float localTop,
        float localWidth,
        float localHeight,
        std::vector<OutlinePoint>& outOutline)
    {
        outOutline.clear();

        const auto* imageCollider = entity.GetComponent<ImageOutlineColliderComponent>();
        if (!imageCollider)
        {
            return false;
        }

        const auto& normalizedOutline = imageCollider->GetNormalizedOutline();
        if (normalizedOutline.size() < 3 || localWidth <= 0.0001f || localHeight <= 0.0001f)
        {
            return false;
        }

        std::vector<OutlinePoint> clipped;
        clipped.reserve(normalizedOutline.size());
        for (const b2Vec2& point : normalizedOutline)
        {
            clipped.push_back({ point.x, point.y });
        }

        std::vector<OutlinePoint> scratch;
        auto clipVertical = [&](float edgeX, bool keepGreater)
        {
            ClipPolygonAgainstEdge(
                clipped,
                scratch,
                [=](const OutlinePoint& point)
                {
                    return keepGreater ? point.x >= edgeX : point.x <= edgeX;
                },
                [=](const OutlinePoint& a, const OutlinePoint& b)
                {
                    const float delta = b.x - a.x;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeX - a.x) / delta;
                    return LerpPoint(a, b, std::clamp(t, 0.0f, 1.0f));
                });
            clipped.swap(scratch);
        };
        auto clipHorizontal = [&](float edgeY, bool keepGreater)
        {
            ClipPolygonAgainstEdge(
                clipped,
                scratch,
                [=](const OutlinePoint& point)
                {
                    return keepGreater ? point.y >= edgeY : point.y <= edgeY;
                },
                [=](const OutlinePoint& a, const OutlinePoint& b)
                {
                    const float delta = b.y - a.y;
                    const float t = std::fabs(delta) <= 0.0001f ? 0.0f : (edgeY - a.y) / delta;
                    return LerpPoint(a, b, std::clamp(t, 0.0f, 1.0f));
                });
            clipped.swap(scratch);
        };

        clipVertical(localLeft, true);
        clipVertical(localLeft + localWidth, false);
        clipHorizontal(localTop, true);
        clipHorizontal(localTop + localHeight, false);

        if (clipped.size() < 3)
        {
            return false;
        }

        outOutline.reserve(clipped.size());
        for (const OutlinePoint& point : clipped)
        {
            outOutline.push_back({
                (point.x - localLeft) / localWidth,
                (point.y - localTop) / localHeight
            });
        }
        return outOutline.size() >= 3;
    }
}

void PhotoCaptureSystem::HandleCapture(GameScene& scene)
{
    if (!scene.m_flow.cameraMode || !Input_IsActionPressed(InputAction::CapturePhoto))
    {
        return;
    }
    if (scene.IsPhotoTrayHit(static_cast<float>(Input_GetMouseX()), static_cast<float>(Input_GetMouseY())))
    {
        return;
    }

    Entity* player = scene.FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    scene.GetCaptureFrameRect(*playerTransform, frameX, frameY, frameWidth, frameHeight);

    scene.m_photo.capture.items.clear();
    float capturedMaxRight = 0.0f;
    float capturedMaxBottom = 0.0f;
    CaptureEntitiesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom);
    CaptureTilesInFrame(scene, frameX, frameY, frameWidth, frameHeight, capturedMaxRight, capturedMaxBottom);
    if (scene.m_photo.capture.items.empty())
    {
        return;
    }

    FinalizeCapturedPhoto(scene, *player, frameWidth, frameHeight);
}

void PhotoCaptureSystem::CaptureEntitiesInFrame(
    GameScene& scene,
    float frameX,
    float frameY,
    float frameWidth,
    float frameHeight,
    float& capturedMaxRight,
    float& capturedMaxBottom)
{
    std::vector<Entity*> entitiesToRemove;
    for (const auto& entity : scene.m_entities)
    {
        if (!entity || HasTag(*entity, "Player") || HasTag(*entity, "Enemy"))
        {
            continue;
        }

        const bool isPhotoBox = HasTag(*entity, "PhotoBox");
        if (isPhotoBox)
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

        const auto* targetTransform = entity->GetComponent<TransformComponent>();
        const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
        if (!targetTransform || !sprite)
        {
            continue;
        }

        const float targetX = targetTransform->x;
        const float targetY = targetTransform->y;
        const float targetWidth = targetTransform->width * targetTransform->scale;
        const float targetHeight = targetTransform->height * targetTransform->scale;
        const float overlapLeft = (std::max)(frameX, targetX);
        const float overlapTop = (std::max)(frameY, targetY);
        const float overlapRight = (std::min)(frameX + frameWidth, targetX + targetWidth);
        const float overlapBottom = (std::min)(frameY + frameHeight, targetY + targetHeight);
        const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
        const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
        if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
        {
            continue;
        }

        const float localLeft = (overlapLeft - targetX) / targetWidth;
        const float localTop = (overlapTop - targetY) / targetHeight;
        const float localWidth = overlapWidth / targetWidth;
        const float localHeight = overlapHeight / targetHeight;

        CapturedPhotoItem item;
        const bool capturedBarrel = entity->GetComponent<BarrelComponent>() != nullptr;
        const auto* vanishOnCapture = entity->GetComponent<VanishOnCaptureComponent>();
        const bool capturedVanishObject = vanishOnCapture && vanishOnCapture->enabled;
        const auto* projectile = entity->GetComponent<ProjectileComponent>();
        const bool capturedProjectile = projectile != nullptr;
        item.textureId = sprite->GetTextureId();
        item.role = GetEntityCopyRole(*entity);
        item.layer = PhotoCopyLayer::Foreground;
        item.origin = GetEntityCopyOrigin(*entity);
        item.appliedTheme = scene.m_photo.capture.selectedTheme;
        item.spawnArchetype = capturedBarrel
            ? CapturedSpawnArchetype::Barrel
            : (capturedProjectile ? CapturedSpawnArchetype::Projectile : CapturedSpawnArchetype::None);
        item.vanishOnCapture = capturedVanishObject;
        item.relativeX = overlapLeft - frameX;
        item.relativeY = overlapTop - frameY;
        item.width = overlapWidth;
        item.height = overlapHeight;
        item.sourceX = sprite->GetSourceX() + sprite->GetSourceWidth() * localLeft;
        item.sourceY = sprite->GetSourceY() + sprite->GetSourceHeight() * localTop;
        item.sourceWidth = sprite->GetSourceWidth() * localWidth;
        item.sourceHeight = sprite->GetSourceHeight() * localHeight;
        BuildCapturedOutlineFromEntity(*entity, localLeft, localTop, localWidth, localHeight, item.collisionOutline);
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            item.tintR = tint->r;
            item.tintG = tint->g;
            item.tintB = tint->b;
            item.tintA = tint->a;
        }
        if (capturedProjectile)
        {
            item.role = PhotoCopyRole::Hazard;
            item.layer = PhotoCopyLayer::Foreground;
            item.projectileVelocityX = projectile->GetVelocityX();
            item.projectileVelocityY = projectile->GetVelocityY();
            item.projectileDamage = projectile->GetDamage();
            item.rotation = std::atan2(item.projectileVelocityY, item.projectileVelocityX);
        }
        else if (!capturedBarrel)
        {
            item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
            item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
        }
        else
        {
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
        }
        if (capturedVanishObject)
        {
            item.role = PhotoCopyRole::Solid;
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = PhotoCopyOrigin::Generic;
        }

        if (item.role == PhotoCopyRole::Solid)
        {
            item.layer = PhotoCopyLayer::Foreground;
        }

        if (!capturedBarrel && !isPhotoBox && !capturedVanishObject)
        {
            ApplyPhotoFilterToCapturedTarget(*entity, scene.m_photo.capture.selectedTheme);
        }
        scene.m_photo.capture.items.push_back(item);
        capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
        capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
        if (capturedVanishObject)
        {
            entitiesToRemove.push_back(entity.get());
        }
    }
    if (!entitiesToRemove.empty())
    {
        scene.m_entities.erase(
            std::remove_if(
                scene.m_entities.begin(),
                scene.m_entities.end(),
                [&](const std::unique_ptr<Entity>& candidate)
                {
                    if (!candidate)
                    {
                        return false;
                    }
                    return std::find(entitiesToRemove.begin(), entitiesToRemove.end(), candidate.get()) != entitiesToRemove.end();
                }),
            scene.m_entities.end());
    }
}

void PhotoCaptureSystem::CaptureTilesInFrame(
    GameScene& scene,
    float frameX,
    float frameY,
    float frameWidth,
    float frameHeight,
    float& capturedMaxRight,
    float& capturedMaxBottom)
{
    const float tileSize = scene.m_tileMap.GetTileSize();
    const int leftColumn = std::max(0, static_cast<int>(frameX / tileSize));
    const int rightColumn = std::min(scene.m_tileMap.GetWidth() - 1, static_cast<int>((frameX + frameWidth - 1.0f) / tileSize));
    const int topRow = std::max(0, static_cast<int>(frameY / tileSize));
    const int bottomRow = std::min(scene.m_tileMap.GetHeight() - 1, static_cast<int>((frameY + frameHeight - 1.0f) / tileSize));

    for (int row = topRow; row <= bottomRow; ++row)
    {
        for (int column = leftColumn; column <= rightColumn; ++column)
        {
            const int tileValue = scene.m_tileMap.GetTile(column, row);
            if (tileValue <= 0)
            {
                continue;
            }

            if (tileValue == 1 || tileValue == 8)
            {
                continue;
            }

            const float tileX = static_cast<float>(column) * tileSize;
            const float tileY = static_cast<float>(row) * tileSize;
            const TileTriangleShape triangle = TileMap::GetTriangleShape(tileValue);
            const float boundsWidth = triangle.isTriangle
                ? static_cast<float>(triangle.widthTiles) * tileSize
                : tileSize;
            const float boundsHeight = triangle.isTriangle
                ? static_cast<float>(triangle.heightTiles) * tileSize
                : tileSize;
            const float overlapLeft = (std::max)(frameX, tileX);
            const float overlapTop = (std::max)(frameY, tileY);
            const float overlapRight = (std::min)(frameX + frameWidth, tileX + boundsWidth);
            const float overlapBottom = (std::min)(frameY + frameHeight, tileY + boundsHeight);
            const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
            const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
            if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
            {
                continue;
            }

            CapturedPhotoItem item;
            item.textureId = scene.m_tileTexture;
            item.role = GetTileCopyRole(tileValue);
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = GetTileCopyOrigin(tileValue);
            item.appliedTheme = scene.m_photo.capture.selectedTheme;
            item.relativeX = overlapLeft - frameX;
            item.relativeY = overlapTop - frameY;
            item.width = overlapWidth;
            item.height = overlapHeight;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            GetTileCaptureTint(tileValue, item.tintR, item.tintG, item.tintB, item.tintA);
            item.sourceTileValue = tileValue;
            scene.m_photo.capture.items.push_back(item);
            capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
            capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
        }
    }
}

void PhotoCaptureSystem::FinalizeCapturedPhoto(GameScene& scene, Entity& player, float frameWidth, float frameHeight)
{
    scene.m_photo.capture.hasPhoto = true;
    scene.m_photo.capture.capturedTheme = scene.m_photo.capture.selectedTheme;
    scene.m_photo.placement.layer = PhotoCopyLayer::Foreground;
    scene.m_photo.placement.flipX = false;
    scene.m_photo.placement.rotation = 0.0f;
    scene.m_photo.capture.width = (std::max)(1.0f, frameWidth);
    scene.m_photo.capture.height = (std::max)(1.0f, frameHeight);
    scene.m_photo.capture.textureId = scene.m_photo.capture.items.front().textureId;
    scene.m_photo.capture.sourceX = scene.m_photo.capture.items.front().sourceX;
    scene.m_photo.capture.sourceY = scene.m_photo.capture.items.front().sourceY;
    scene.m_photo.capture.sourceWidth = scene.m_photo.capture.items.front().sourceWidth;
    scene.m_photo.capture.sourceHeight = scene.m_photo.capture.items.front().sourceHeight;
    scene.m_photo.capture.tintR = scene.m_photo.capture.items.front().tintR;
    scene.m_photo.capture.tintG = scene.m_photo.capture.items.front().tintG;
    scene.m_photo.capture.tintB = scene.m_photo.capture.items.front().tintB;
    scene.m_photo.capture.tintA = scene.m_photo.capture.items.front().tintA;
    scene.StoreCapturedPhoto();

    scene.m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "shutter", 0.0f, 0.0f });
    scene.m_eventBus.Publish({ EventType::LogMessage, &player, nullptr, GetPhotoCaptureLogMessage(scene.m_photo.capture.capturedTheme), 0.0f, 0.0f });
    scene.m_flow.shutterFlashRemaining = gShutterFlashSeconds;
    scene.m_flow.developedPhotoPreviewRemaining = kDevelopedPhotoPreviewSeconds;
}
