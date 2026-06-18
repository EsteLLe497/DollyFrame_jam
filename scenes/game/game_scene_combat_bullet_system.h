#pragma once

#include <limits>

#include "game_scene_combat_common.h"

namespace game_scene_combat_system
{
struct CollisionPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

inline void BuildMidBoss2SpearCollisionSegment(
    const TransformComponent& transform,
    CollisionPoint& outStart,
    CollisionPoint& outEnd,
    float& outRadius)
{
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const float centerX = transform.x + width * 0.5f;
    const float centerY = transform.y + height * 0.5f;
    const float halfLength = width * 0.44f;
    const float cosTheta = std::cos(transform.rotation);
    const float sinTheta = std::sin(transform.rotation);

    const auto rotateOffset = [&](float localX, float localY) -> CollisionPoint
    {
        return {
            centerX + localX * cosTheta - localY * sinTheta,
            centerY + localX * sinTheta + localY * cosTheta
        };
    };

    outStart = rotateOffset(-halfLength, 0.0f);
    outEnd = rotateOffset(halfLength, 0.0f);
    outRadius = std::max(4.5f, height * 0.12f);
}

inline void BuildMidBoss2SpearTipCollisionSegment(
    const TransformComponent& transform,
    const ProjectileComponent& projectile,
    const MidBoss2SpearComponent& spear,
    CollisionPoint& outStart,
    CollisionPoint& outEnd,
    float& outRadius)
{
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const float centerX = transform.x + width * 0.5f;
    const float centerY = transform.y + height * 0.5f;
    const float halfLength = width * 0.44f;
    const float tipLength = std::min(halfLength, std::max(width * 0.20f, height * 0.5f));

    float dirX = projectile.GetVelocityX();
    float dirY = projectile.GetVelocityY();
    float dirLength = std::hypot(dirX, dirY);
    if (dirLength < 0.0001f)
    {
        dirX = spear.directionX;
        dirY = spear.directionY;
        dirLength = std::hypot(dirX, dirY);
    }
    if (dirLength < 0.0001f)
    {
        dirX = std::cos(transform.rotation);
        dirY = std::sin(transform.rotation);
        dirLength = std::hypot(dirX, dirY);
    }
    if (dirLength < 0.0001f)
    {
        dirX = 1.0f;
        dirY = 0.0f;
        dirLength = 1.0f;
    }

    dirX /= dirLength;
    dirY /= dirLength;

    const float tipStartDistance = halfLength - tipLength;
    outStart = {
        centerX + dirX * tipStartDistance,
        centerY + dirY * tipStartDistance
    };
    outEnd = {
        centerX + dirX * halfLength,
        centerY + dirY * halfLength
    };
    outRadius = std::max(4.5f, height * 0.12f);
}

inline bool SegmentIntersectsExpandedAabb(
    const CollisionPoint& start,
    const CollisionPoint& end,
    float left,
    float top,
    float right,
    float bottom)
{
    float tMin = 0.0f;
    float tMax = 1.0f;
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;

    const auto clip = [&](float p, float q) -> bool
    {
        constexpr float kEpsilon = 0.0001f;
        if (std::fabs(p) < kEpsilon)
        {
            return q >= 0.0f;
        }

        const float r = q / p;
        if (p < 0.0f)
        {
            if (r > tMax)
            {
                return false;
            }
            tMin = std::max(tMin, r);
        }
        else
        {
            if (r < tMin)
            {
                return false;
            }
            tMax = std::min(tMax, r);
        }
        return true;
    };

    return clip(-dx, start.x - left) &&
        clip(dx, right - start.x) &&
        clip(-dy, start.y - top) &&
        clip(dy, bottom - start.y);
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename IsSolidTileFn>
inline void UpdateBullets(
    const std::vector<Entity*>& bulletEntities,
    const std::vector<Entity*>& enemyEntities,
    const std::vector<Entity*>& protectiveWallEntities,
    const std::vector<TransformComponent>& obstacleBounds,
    float mapWidth,
    float mapHeight,
    float deltaTime,
    Entity* player,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage,
    HandleEnemyDamageFn&& handleEnemyDamage,
    IsSolidTileFn&& isSolidTile,
    std::vector<Entity*>& bulletsToRemove)
{
    bulletsToRemove.clear();

    for (Entity* entity : bulletEntities)
    {
        if (!entity)
        {
            continue;
        }

        auto* transform = entity->GetComponent<TransformComponent>();
        auto* projectile = entity->GetComponent<ProjectileComponent>();
        if (!transform || !projectile)
        {
            continue;
        }

        if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
        {
            const float targetAngle = std::atan2(spear->targetDirectionY, spear->targetDirectionX);
            spear->launchTimer += deltaTime;
            if (!spear->launched)
            {
                if (spear->launchTimer < spear->launchDelay)
                {
                    transform->rotation = targetAngle + spear->launchTimer * 18.0f;
                    continue;
                }

                spear->launched = true;
                projectile->SetVelocityX(spear->targetDirectionX * kMidBoss2SpearSpeed);
                projectile->SetVelocityY(spear->targetDirectionY * kMidBoss2SpearSpeed);
                transform->rotation = targetAngle;
            }
            else
            {
                if (spear->stuck)
                {
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    transform->rotation = std::atan2(spear->directionY, spear->directionX);
                }
                else
                {
                    spear->travelDistance += std::hypot(
                        projectile->GetVelocityX(),
                        projectile->GetVelocityY()) * deltaTime;
                    transform->rotation = std::atan2(
                        projectile->GetVelocityY(),
                        projectile->GetVelocityX());
                }
            }
            if (spear->stuck)
            {
                spear->fadeRemaining = std::max(0.0f, spear->fadeRemaining - deltaTime);
                if (auto* tint = entity->GetComponent<TintComponent>())
                {
                    const float fadeAlpha = spear->fadeDuration > 0.0f
                        ? std::clamp(spear->fadeRemaining / spear->fadeDuration, 0.0f, 1.0f)
                        : 1.0f;
                    tint->a = fadeAlpha;
                }
                if (spear->fadeRemaining <= 0.0f)
                {
                    bulletsToRemove.push_back(entity);
                }
                continue;
            }
        }

        if (auto* capturedMidBoss3Attack = entity->GetComponent<CapturedMidBoss3AttackComponent>())
        {
            const auto findMidBoss3Target = [&]() -> Entity*
            {
                Entity* bestBoss = nullptr;
                float bestDistanceSq = std::numeric_limits<float>::max();
                const float attackCenterX = transform->x + transform->width * transform->scale * 0.5f;
                const float attackCenterY = transform->y + transform->height * transform->scale * 0.5f;
                bool fistHitEnemy = false;
                for (Entity* target : enemyEntities)
                {
                    if (!target || target == entity)
                    {
                        continue;
                    }
                    const auto* enemy = target->GetComponent<EnemyComponent>();
                    const auto* boss = target->GetComponent<MidBoss3Component>();
                    const auto* targetTransform = target->GetComponent<TransformComponent>();
                    if (!enemy || !boss || !targetTransform || !enemy->IsEnabled() || enemy->IsDefeated())
                    {
                        continue;
                    }
                    const float targetCenterX = targetTransform->x + targetTransform->width * targetTransform->scale * 0.5f;
                    const float targetCenterY = targetTransform->y + targetTransform->height * targetTransform->scale * 0.5f;
                    const float dx = targetCenterX - attackCenterX;
                    const float dy = targetCenterY - attackCenterY;
                    const float distanceSq = dx * dx + dy * dy;
                    if (distanceSq < bestDistanceSq)
                    {
                        bestDistanceSq = distanceSq;
                        bestBoss = target;
                    }
                }
                return bestBoss;
            };
            const auto aimTowardTarget = [&](Entity* target)
            {
                if (!target)
                {
                    return;
                }
                const auto* targetTransform = target->GetComponent<TransformComponent>();
                if (!targetTransform)
                {
                    return;
                }
                const float attackCenterX = transform->x + transform->width * transform->scale * 0.5f;
                const float attackCenterY = transform->y + transform->height * transform->scale * 0.5f;
                const float targetCenterX = targetTransform->x + targetTransform->width * targetTransform->scale * 0.5f;
                const float targetCenterY = targetTransform->y + targetTransform->height * targetTransform->scale * 0.5f;
                const float dx = targetCenterX - attackCenterX;
                const float dy = targetCenterY - attackCenterY;
                const float length = std::max(0.001f, std::hypot(dx, dy));
                capturedMidBoss3Attack->aimX = dx / length;
                capturedMidBoss3Attack->aimY = dy / length;
                capturedMidBoss3Attack->direction = capturedMidBoss3Attack->aimX >= 0.0f ? 1 : -1;
                transform->rotation = std::atan2(capturedMidBoss3Attack->aimY, capturedMidBoss3Attack->aimX);
            };
            const auto rectIntersectsSolid = [&](float x, float y, float width, float height) -> bool
            {
                constexpr float kSampleStep = 24.0f;
                const float left = x + 2.0f;
                const float right = x + width - 3.0f;
                const float top = y + 2.0f;
                const float bottom = y + height - 3.0f;
                if (right < left || bottom < top)
                {
                    return false;
                }

                for (float sampleY = top; sampleY <= bottom; sampleY += kSampleStep)
                {
                    if (isSolidTile(left, sampleY) || isSolidTile(right, sampleY))
                    {
                        return true;
                    }
                }
                for (float sampleX = left; sampleX <= right; sampleX += kSampleStep)
                {
                    if (isSolidTile(sampleX, top) || isSolidTile(sampleX, bottom))
                    {
                        return true;
                    }
                }
                return isSolidTile(left, top) ||
                    isSolidTile(right, top) ||
                    isSolidTile(left, bottom) ||
                    isSolidTile(right, bottom);
            };

            if (capturedMidBoss3Attack->kind == CapturedMidBoss3AttackKind::Fist)
            {
                transform->x += projectile->GetVelocityX() * deltaTime;
                transform->y += projectile->GetVelocityY() * deltaTime;
                transform->rotation = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());

                bool fistHitEnemy = false;
                for (Entity* target : enemyEntities)
                {
                    if (!target || target == entity)
                    {
                        continue;
                    }
                    if (!intersectsEntity(*target, *entity))
                    {
                        continue;
                    }
                    handleEnemyDamage(*target, entity, projectile->GetDamage(), "Captured MidBoss3 fist hit enemy");
                    bulletsToRemove.push_back(entity);
                    fistHitEnemy = true;
                    break;
                }
                if (fistHitEnemy)
                {
                    continue;
                }

                const float centerX = transform->x + transform->width * transform->scale * 0.5f;
                const float centerY = transform->y + transform->height * transform->scale * 0.5f;
                const bool hitSolidTile = isSolidTile(centerX, centerY);
                const bool outOfBounds =
                    transform->x + transform->width * transform->scale < 0.0f ||
                    transform->x > mapWidth ||
                    transform->y + transform->height * transform->scale < 0.0f ||
                    transform->y > mapHeight;
                if (hitSolidTile || outOfBounds)
                {
                    bulletsToRemove.push_back(entity);
                }
                continue;
            }

            if (capturedMidBoss3Attack->kind == CapturedMidBoss3AttackKind::Drill)
            {
                constexpr float kDrillLaunchSpeed = 620.0f;
                constexpr float kDrillRushSpeed = 720.0f;
                constexpr float kBossDamageInterval = 1.0f;
                Entity* targetBoss = capturedMidBoss3Attack->carriedBoss
                    ? capturedMidBoss3Attack->carriedBoss
                    : findMidBoss3Target();
                if (targetBoss)
                {
                    const auto* targetEnemy = targetBoss->GetComponent<EnemyComponent>();
                    if (!targetEnemy || !targetEnemy->IsEnabled() || targetEnemy->IsDefeated())
                    {
                        bulletsToRemove.push_back(entity);
                        continue;
                    }
                }

                if (capturedMidBoss3Attack->attachedToBoss && targetBoss)
                {
                    capturedMidBoss3Attack->bossDamageTimer += deltaTime;
                    const float pushX = capturedMidBoss3Attack->aimX * kDrillRushSpeed * deltaTime;
                    const float pushY = capturedMidBoss3Attack->aimY * kDrillRushSpeed * deltaTime;
                    auto* targetTransform = targetBoss->GetComponent<TransformComponent>();
                    auto* boss = targetBoss->GetComponent<MidBoss3Component>();
                    bool stoppedBySolid = false;
                    if (targetTransform)
                    {
                        const float bossWidth = targetTransform->width * targetTransform->scale;
                        const float bossHeight = targetTransform->height * targetTransform->scale;
                        const float currentX = boss ? boss->homeX : targetTransform->x;
                        const float currentY = boss ? boss->homeY : targetTransform->y;
                        const float maxX = std::max(0.0f, mapWidth - bossWidth);
                        const float maxY = std::max(0.0f, mapHeight - bossHeight);
                        float resolvedX = currentX;
                        float resolvedY = currentY;

                        const float requestedX = currentX + pushX;
                        const float requestedY = currentY + pushY;
                        const float nextX = std::clamp(requestedX, 0.0f, maxX);
                        const float nextY = std::clamp(requestedY, 0.0f, maxY);
                        const bool hitMapBoundary =
                            std::fabs(requestedX - nextX) > 0.01f ||
                            std::fabs(requestedY - nextY) > 0.01f;
                        if (!hitMapBoundary && !rectIntersectsSolid(nextX, nextY, bossWidth, bossHeight))
                        {
                            resolvedX = nextX;
                            resolvedY = nextY;
                        }
                        else
                        {
                            stoppedBySolid = true;
                        }

                        if (boss)
                        {
                            boss->homeX = resolvedX;
                            boss->homeY = resolvedY;
                        }
                        targetTransform->x = resolvedX;
                        targetTransform->y = resolvedY;
                        transform->x = targetTransform->x + targetTransform->width * targetTransform->scale * 0.5f - transform->width * transform->scale * 0.5f;
                        transform->y = targetTransform->y + targetTransform->height * targetTransform->scale * 0.5f - transform->height * transform->scale * 0.5f;
                    }
                    if (capturedMidBoss3Attack->bossDamageTimer >= kBossDamageInterval)
                    {
                        capturedMidBoss3Attack->bossDamageTimer = 0.0f;
                        handleEnemyDamage(*targetBoss, entity, projectile->GetDamage(), "Captured MidBoss3 drill damaged enemy");
                    }
                    transform->rotation = std::atan2(capturedMidBoss3Attack->aimY, capturedMidBoss3Attack->aimX);
                    if (stoppedBySolid)
                    {
                        bulletsToRemove.push_back(entity);
                    }
                    continue;
                }

                if (capturedMidBoss3Attack->waitRemaining > 0.0f)
                {
                    aimTowardTarget(targetBoss);
                    capturedMidBoss3Attack->waitRemaining = std::max(0.0f, capturedMidBoss3Attack->waitRemaining - deltaTime);
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    if (capturedMidBoss3Attack->waitRemaining > 0.0f)
                    {
                        continue;
                    }
                    capturedMidBoss3Attack->launched = true;
                    projectile->SetVelocityX(capturedMidBoss3Attack->aimX * kDrillLaunchSpeed);
                    projectile->SetVelocityY(capturedMidBoss3Attack->aimY * kDrillLaunchSpeed);
                }

                const float nextX = transform->x + projectile->GetVelocityX() * deltaTime;
                const float nextY = transform->y + projectile->GetVelocityY() * deltaTime;
                const float attackWidth = transform->width * transform->scale;
                const float attackHeight = transform->height * transform->scale;
                if (rectIntersectsSolid(nextX, nextY, attackWidth, attackHeight))
                {
                    bulletsToRemove.push_back(entity);
                    continue;
                }
                transform->x = nextX;
                transform->y = nextY;
                transform->rotation = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());

                bool attachedThisFrame = false;
                for (Entity* target : enemyEntities)
                {
                    if (!target ||
                        target == entity ||
                        !target->GetComponent<MidBoss3Component>() ||
                        !intersectsEntity(*target, *entity))
                    {
                        continue;
                    }
                    capturedMidBoss3Attack->attachedToBoss = true;
                    capturedMidBoss3Attack->carriedBoss = target;
                    capturedMidBoss3Attack->bossDamageTimer = kBossDamageInterval;
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    attachedThisFrame = true;
                    break;
                }
                if (attachedThisFrame)
                {
                    continue;
                }

                const bool outOfBounds =
                    transform->x + transform->width * transform->scale < 0.0f ||
                    transform->x > mapWidth ||
                    transform->y > mapHeight + transform->height * transform->scale;
                if (outOfBounds)
                {
                    bulletsToRemove.push_back(entity);
                }
                continue;
            }
        }

        transform->x += projectile->GetVelocityX() * deltaTime;
        transform->y += projectile->GetVelocityY() * deltaTime;

        if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
        {
            if (projectile->GetOwner() == ProjectileComponent::Owner::Photo &&
                spear->launched &&
                !spear->stuck)
            {
                CollisionPoint spearStart;
                CollisionPoint spearEnd;
                float spearRadius = 0.0f;
                BuildMidBoss2SpearTipCollisionSegment(*transform, *projectile, *spear, spearStart, spearEnd, spearRadius);

                Entity* targetBoss = nullptr;
                float bestDistanceSq = std::numeric_limits<float>::max();
                const float spearCenterX = transform->x + transform->width * transform->scale * 0.5f;
                const float spearCenterY = transform->y + transform->height * transform->scale * 0.5f;
                for (Entity* target : enemyEntities)
                {
                    if (!target)
                    {
                        continue;
                    }

                    const auto* enemy = target->GetComponent<EnemyComponent>();
                    const auto* boss = target->GetComponent<MidBoss2Component>();
                    const auto* targetTransform = target->GetComponent<TransformComponent>();
                    if (!enemy || !boss || !targetTransform || !enemy->IsEnabled() || enemy->IsDefeated())
                    {
                        continue;
                    }

                    const float targetCenterX = targetTransform->x + targetTransform->width * targetTransform->scale * 0.5f;
                    const float targetCenterY = targetTransform->y + targetTransform->height * targetTransform->scale * 0.5f;
                    const float dx = targetCenterX - spearCenterX;
                    const float dy = targetCenterY - spearCenterY;
                    const float distanceSq = dx * dx + dy * dy;
                    if (distanceSq < bestDistanceSq)
                    {
                        bestDistanceSq = distanceSq;
                        targetBoss = target;
                    }
                }

                if (targetBoss)
                {
                    const auto* targetTransform = targetBoss->GetComponent<TransformComponent>();
                    if (targetTransform)
                    {
                        const float left = targetTransform->x - spearRadius;
                        const float top = targetTransform->y - spearRadius;
                        const float right = targetTransform->x + targetTransform->width * targetTransform->scale + spearRadius;
                        const float bottom = targetTransform->y + targetTransform->height * targetTransform->scale + spearRadius;
                        if (SegmentIntersectsExpandedAabb(spearStart, spearEnd, left, top, right, bottom))
                        {
                            handleEnemyDamage(*targetBoss, entity, projectile->GetDamage(), "Captured MidBoss2 spear hit boss");
                            bulletsToRemove.push_back(entity);
                            continue;
                        }
                    }
                }
            }

            if (!spear->stuck)
            {
                CollisionPoint spearStart;
                CollisionPoint spearEnd;
                float spearRadius = 0.0f;
                BuildMidBoss2SpearTipCollisionSegment(*transform, *projectile, *spear, spearStart, spearEnd, spearRadius);

                const auto spearHitsSolidTile = [&]() -> bool
                {
                    const float dx = spearEnd.x - spearStart.x;
                    const float dy = spearEnd.y - spearStart.y;
                    const float length = std::max(0.001f, std::hypot(dx, dy));
                    const float dirX = dx / length;
                    const float dirY = dy / length;
                    const float perpX = -dirY;
                    const float perpY = dirX;
                    constexpr float kOffsets[] = { -0.85f, 0.0f, 0.85f };
                    const int sampleCount = std::max(5, static_cast<int>(std::ceil(length / 20.0f)));

                    for (int sampleIndex = 0; sampleIndex <= sampleCount; ++sampleIndex)
                    {
                        const float t = static_cast<float>(sampleIndex) / static_cast<float>(sampleCount);
                        const float centerX = spearStart.x + dx * t;
                        const float centerY = spearStart.y + dy * t;
                        for (float offsetScale : kOffsets)
                        {
                            const float sampleX = centerX + perpX * spearRadius * offsetScale;
                            const float sampleY = centerY + perpY * spearRadius * offsetScale;
                            if (isSolidTile(sampleX, sampleY))
                            {
                                return true;
                            }
                        }
                    }
                    return false;
                };

                const auto spearHitsProtectiveWall = [&]() -> Entity*
                {
                    for (Entity* wallEntity : protectiveWallEntities)
                    {
                        if (!wallEntity)
                        {
                            continue;
                        }

                        auto* wall = wallEntity->GetComponent<ProtectiveWallComponent>();
                        auto* wallTransform = wallEntity->GetComponent<TransformComponent>();
                        if (!wall || !wallTransform || wall->IsDestroyed() || !wall->isOn)
                        {
                            continue;
                        }

                        const float left = wallTransform->x - spearRadius;
                        const float top = wallTransform->y - spearRadius;
                        const float right = wallTransform->x + wallTransform->width * wallTransform->scale + spearRadius;
                        const float bottom = wallTransform->y + wallTransform->height * wallTransform->scale + spearRadius;
                        if (SegmentIntersectsExpandedAabb(spearStart, spearEnd, left, top, right, bottom))
                        {
                            return wallEntity;
                        }
                    }
                    return nullptr;
                };

                const auto spearHitsPlayer = [&]() -> bool
                {
                    if (!player)
                    {
                        return false;
                    }

                    const auto* playerTransform = player->GetComponent<TransformComponent>();
                    if (!playerTransform)
                    {
                        return false;
                    }

                    const float left = playerTransform->x - spearRadius;
                    const float top = playerTransform->y - spearRadius;
                    const float right = playerTransform->x + playerTransform->width * playerTransform->scale + spearRadius;
                    const float bottom = playerTransform->y + playerTransform->height * playerTransform->scale + spearRadius;
                    return SegmentIntersectsExpandedAabb(spearStart, spearEnd, left, top, right, bottom);
                };

                const auto spearHitsObstacle = [&]() -> bool
                {
                    for (const auto& obstacle : obstacleBounds)
                    {
                        const float left = obstacle.x - spearRadius;
                        const float top = obstacle.y - spearRadius;
                        const float right = obstacle.x + obstacle.width * obstacle.scale + spearRadius;
                        const float bottom = obstacle.y + obstacle.height * obstacle.scale + spearRadius;
                        if (SegmentIntersectsExpandedAabb(spearStart, spearEnd, left, top, right, bottom))
                        {
                            return true;
                        }
                    }
                    return false;
                };

                Entity* hitProtectiveWall = spearHitsProtectiveWall();
                if (spearHitsSolidTile() || spearHitsObstacle() || hitProtectiveWall)
                {
                    if (hitProtectiveWall)
                    {
                        if (auto* wall = hitProtectiveWall->GetComponent<ProtectiveWallComponent>())
                        {
                            wall->ApplyDamage(1);
                        }
                    }
                    const float hitLength = std::hypot(projectile->GetVelocityX(), projectile->GetVelocityY());
                    if (hitLength > 0.0001f)
                    {
                        spear->directionX = projectile->GetVelocityX() / hitLength;
                        spear->directionY = projectile->GetVelocityY() / hitLength;
                    }
                    spear->stuck = true;
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    continue;
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::Enemy &&
                    spearHitsPlayer())
                {
                    handlePlayerDamage(*player, entity, "GameScene player damaged by spear");
                    bulletsToRemove.push_back(entity);
                    continue;
                }
            }

            continue;
        }

        const bool hitSolidTile =
            isSolidTile(transform->x, transform->y) ||
            isSolidTile(transform->x + transform->width, transform->y) ||
            isSolidTile(transform->x, transform->y + transform->height) ||
            isSolidTile(transform->x + transform->width, transform->y + transform->height);
        if (hitSolidTile)
        {
            if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
            {
                const float hitLength = std::hypot(projectile->GetVelocityX(), projectile->GetVelocityY());
                if (hitLength > 0.0001f)
                {
                    spear->directionX = projectile->GetVelocityX() / hitLength;
                    spear->directionY = projectile->GetVelocityY() / hitLength;
                }
                spear->stuck = true;
                projectile->SetVelocityX(0.0f);
                projectile->SetVelocityY(0.0f);
                continue;
            }

            bulletsToRemove.push_back(entity);
            continue;
        }

        if (projectile->GetOwner() == ProjectileComponent::Owner::Enemy &&
            player && intersectsEntity(*player, *entity))
        {
            handlePlayerDamage(*player, entity, "GameScene player damaged by bullet");
            bulletsToRemove.push_back(entity);
            continue;
        }

        if (projectile->GetOwner() == ProjectileComponent::Owner::Photo)
        {
            for (Entity* target : enemyEntities)
            {
                if (!target || target == entity)
                {
                    continue;
                }

                if (!intersectsEntity(*target, *entity))
                {
                    continue;
                }

                handleEnemyDamage(*target, entity, projectile->GetDamage(), "Photo bullet hit enemy");
                bulletsToRemove.push_back(entity);
                goto next_bullet;
            }
        }

        if (projectile->GetOwner() == ProjectileComponent::Owner::BlasterRobot)
        {
            if (player && intersectsEntity(*player, *entity))
            {
                handlePlayerDamage(*player, entity, "GameScene player damaged by blaster");
                bulletsToRemove.push_back(entity);
                continue;
            }

            for (Entity* target : enemyEntities)
            {
                if (!target || target == entity)
                {
                    continue;
                }
                if (target == projectile->sourceEntity)
                {
                    continue;
                }
                auto* targetEnemy = target->GetComponent<EnemyComponent>();
                if (!targetEnemy || !targetEnemy->IsEnabled())
                {
                    continue;
                }
                if (!intersectsEntity(*target, *entity))
                {
                    continue;
                }

                handleEnemyDamage(*target, entity, projectile->GetDamage(), "Blaster bullet hit enemy");
                projectile->pierceRemaining--;
                if (projectile->pierceRemaining <= 0)
                {
                    bulletsToRemove.push_back(entity);
                }
                goto next_bullet;
            }
        }

        if (transform->x < 0.0f ||
            transform->x > mapWidth ||
            transform->y < 0.0f ||
            transform->y > mapHeight)
        {
            bulletsToRemove.push_back(entity);
        }

    next_bullet:
        continue;
    }
}
}
