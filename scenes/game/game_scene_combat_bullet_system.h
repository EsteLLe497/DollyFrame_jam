#pragma once

#include <limits>

#include "audio.h"
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

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename SpawnMidBoss2SpearFadeEffectFn, typename IsSolidTileFn>
inline void UpdateBullets(
    const std::vector<Entity*>& bulletEntities,
    const std::vector<Entity*>& enemyEntities,
    const std::vector<Entity*>& protectiveWallEntities,
    const std::vector<TransformComponent>& obstacleBounds,
    float mapWidth,
    float mapHeight,
    float deltaTime,
    GameSceneFlowState& flow,
    bool screenShakeEnabled,
    Entity* player,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage,
    HandleEnemyDamageFn&& handleEnemyDamage,
    SpawnMidBoss2SpearFadeEffectFn&& spawnMidBoss2SpearFadeEffect,
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

        const auto getDamageAgainstTarget = [&](Entity& target) -> int
        {
            const int baseDamage = projectile->GetDamage();
            if (projectile->GetOwner() == ProjectileComponent::Owner::Photo &&
                target.GetComponent<MidBoss3Component>() &&
                entity->GetComponent<CapturedMidBoss3AttackComponent>())
            {
                return baseDamage * 2;
            }
            return baseDamage;
        };

        if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
        {
            const float targetAngle = std::atan2(spear->targetDirectionY, spear->targetDirectionX);
            spear->launchTimer += deltaTime;
            if (!spear->launched)
            {
                const float launchProgress = spear->launchDelay > 0.0f
                    ? std::clamp(spear->launchTimer / spear->launchDelay, 0.0f, 1.0f)
                    : 1.0f;
                const float pullbackProgress = std::clamp((launchProgress - 0.08f) / 0.78f, 0.0f, 1.0f);
                const float pullbackEase = pullbackProgress * pullbackProgress * (3.0f - 2.0f * pullbackProgress);
                const float pullbackDistance = transform->width * kMidBoss2SpearPullbackRatio * pullbackEase;
                transform->x = spear->spawnX - spear->targetDirectionX * pullbackDistance;
                transform->y = spear->spawnY - spear->targetDirectionY * pullbackDistance;
                if (spear->launchTimer < spear->launchDelay)
                {
                    transform->rotation = targetAngle + spear->launchTimer * 18.0f;
                    continue;
                }

                spear->launched = true;
                projectile->SetVelocityX(spear->targetDirectionX * kMidBoss2SpearSpeed);
                projectile->SetVelocityY(spear->targetDirectionY * kMidBoss2SpearSpeed);
                transform->rotation = targetAngle;
                if (screenShakeEnabled)
                {
                    flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, kMidBoss2SpearLaunchShakeSeconds);
                    flow.screenShakeDuration = std::max(flow.screenShakeDuration, kMidBoss2SpearLaunchShakeSeconds);
                    flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, kMidBoss2SpearLaunchShakeAmplitude);
                }
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
                if (isSolidTile(left, top) ||
                    isSolidTile(right, top) ||
                    isSolidTile(left, bottom) ||
                    isSolidTile(right, bottom))
                {
                    return true;
                }

                TransformComponent attackBounds(x, y, width, height);
                for (const auto& obstacle : obstacleBounds)
                {
                    if (IntersectsBounds(attackBounds, obstacle))
                    {
                        return true;
                    }
                }

                for (Entity* target : enemyEntities)
                {
                    const auto* boss = target ? target->GetComponent<MidBoss3Component>() : nullptr;
                    if (!boss || !boss->drillActive || boss->drillWidth <= 0.0f || boss->drillHeight <= 0.0f)
                    {
                        continue;
                    }
                    TransformComponent drillBounds(boss->drillX, boss->drillY, boss->drillWidth, boss->drillHeight);
                    if (IntersectsBounds(attackBounds, drillBounds))
                    {
                        return true;
                    }
                }
                return false;
            };

            if (capturedMidBoss3Attack->kind == CapturedMidBoss3AttackKind::Fist)
            {
                transform->x += projectile->GetVelocityX() * deltaTime;
                transform->y += projectile->GetVelocityY() * deltaTime;
                transform->rotation = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX()) + 3.14159265f;

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
                    if (screenShakeEnabled && target->GetComponent<MidBoss3Component>())
                    {
                        flow.hitStopRemaining = std::max(flow.hitStopRemaining, 0.085f);
                        flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, 0.26f);
                        flow.screenShakeDuration = std::max(flow.screenShakeDuration, 0.26f);
                        flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, 32.0f);
                    }
                    if (!capturedMidBoss3Attack->hitSoundPlayed)
                    {
                        Audio_PlayCue("boss_ruins_hit");
                        capturedMidBoss3Attack->hitSoundPlayed = true;
                    }
                    handleEnemyDamage(*target, entity, getDamageAgainstTarget(*target), "Captured MidBoss3 fist hit enemy");
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
                    if (hitSolidTile && !capturedMidBoss3Attack->hitSoundPlayed)
                    {
                        Audio_PlayCue("boss_ruins_hit");
                        capturedMidBoss3Attack->hitSoundPlayed = true;
                    }
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

                const auto pushAttachedTarget = [&](Entity& target, float pushDeltaTime) -> bool
                {
                    auto* targetTransform = target.GetComponent<TransformComponent>();
                    auto* boss = target.GetComponent<MidBoss3Component>();
                    if (!targetTransform)
                    {
                        return false;
                    }

                    const float bossWidth = targetTransform->width * targetTransform->scale;
                    const float bossHeight = targetTransform->height * targetTransform->scale;
                    const float currentX = boss ? boss->homeX : targetTransform->x;
                    const float currentY = boss ? boss->homeY : targetTransform->y;
                    const float pushX = capturedMidBoss3Attack->aimX * kDrillRushSpeed * pushDeltaTime;
                    const float pushY = capturedMidBoss3Attack->aimY * kDrillRushSpeed * pushDeltaTime;
                    const float maxX = std::max(0.0f, mapWidth - bossWidth);
                    const float maxY = std::max(0.0f, mapHeight - bossHeight);
                    const float requestedX = currentX + pushX;
                    const float requestedY = currentY + pushY;
                    const float nextX = std::clamp(requestedX, 0.0f, maxX);
                    const float nextY = std::clamp(requestedY, 0.0f, maxY);
                    const bool stoppedBySolid =
                        std::fabs(requestedX - nextX) > 0.01f ||
                        std::fabs(requestedY - nextY) > 0.01f ||
                        rectIntersectsSolid(nextX, nextY, bossWidth, bossHeight);
                    const float resolvedX = stoppedBySolid ? currentX : nextX;
                    const float resolvedY = stoppedBySolid ? currentY : nextY;

                    if (boss)
                    {
                        boss->homeX = resolvedX;
                        boss->homeY = resolvedY;
                    }
                    targetTransform->x = resolvedX;
                    targetTransform->y = resolvedY;
                    const float attackWidth = transform->width * transform->scale;
                    const float attackHeight = transform->height * transform->scale;
                    const float aimLength = std::max(
                        0.001f,
                        std::hypot(capturedMidBoss3Attack->aimX, capturedMidBoss3Attack->aimY));
                    const float aimX = capturedMidBoss3Attack->aimX / aimLength;
                    const float aimY = capturedMidBoss3Attack->aimY / aimLength;
                    const float bossCenterX = targetTransform->x + bossWidth * 0.5f;
                    const float bossCenterY = targetTransform->y + bossHeight * 0.5f;
                    const float bossHalfExtentAlongAim =
                        std::fabs(aimX) * bossWidth * 0.5f +
                        std::fabs(aimY) * bossHeight * 0.5f;
                    const float embedDepth = std::min(36.0f, std::min(bossWidth, bossHeight) * 0.28f);
                    const float tipX = bossCenterX - aimX * bossHalfExtentAlongAim + aimX * embedDepth;
                    const float tipY = bossCenterY - aimY * bossHalfExtentAlongAim + aimY * embedDepth;
                    const float drillCenterX = tipX - aimX * attackWidth * 0.5f;
                    const float drillCenterY = tipY - aimY * attackWidth * 0.5f;
                    transform->x = drillCenterX - attackWidth * 0.5f;
                    transform->y = drillCenterY - attackHeight * 0.5f;
                    return stoppedBySolid;
                };

                const auto stopAttachedDrill = [&]()
                {
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    capturedMidBoss3Attack->attachedToBoss = false;
                    capturedMidBoss3Attack->carriedBoss = nullptr;
                    bulletsToRemove.push_back(entity);
                };

                if (capturedMidBoss3Attack->attachedToBoss && targetBoss)
                {
                    capturedMidBoss3Attack->attachedLifeRemaining = std::max(
                        0.0f,
                        capturedMidBoss3Attack->attachedLifeRemaining - deltaTime);
                    if (capturedMidBoss3Attack->attachedLifeRemaining <= 0.0f)
                    {
                        stopAttachedDrill();
                        continue;
                    }

                    capturedMidBoss3Attack->bossDamageTimer += deltaTime;
                    if (pushAttachedTarget(*targetBoss, deltaTime))
                    {
                        stopAttachedDrill();
                        continue;
                    }
                    if (capturedMidBoss3Attack->bossDamageTimer >= kBossDamageInterval)
                    {
                        capturedMidBoss3Attack->bossDamageTimer = 0.0f;
                        handleEnemyDamage(*targetBoss, entity, getDamageAgainstTarget(*targetBoss), "Captured MidBoss3 drill damaged enemy");
                    }
                    transform->rotation = std::atan2(capturedMidBoss3Attack->aimY, capturedMidBoss3Attack->aimX);
                    continue;
                }

                if (capturedMidBoss3Attack->waitRemaining > 0.0f)
                {
                    if (!capturedMidBoss3Attack->waitBaseInitialized)
                    {
                        capturedMidBoss3Attack->waitBaseX = transform->x;
                        capturedMidBoss3Attack->waitBaseY = transform->y;
                        capturedMidBoss3Attack->waitBaseInitialized = true;
                    }
                    capturedMidBoss3Attack->waitShakeTimer += deltaTime;
                    aimTowardTarget(targetBoss);
                    const float shakePhase = capturedMidBoss3Attack->waitShakeTimer * 92.0f;
                    transform->x = capturedMidBoss3Attack->waitBaseX + std::sin(shakePhase) * 2.0f;
                    transform->y = capturedMidBoss3Attack->waitBaseY + std::cos(shakePhase * 1.31f) * 0.9f;
                    capturedMidBoss3Attack->waitRemaining = std::max(0.0f, capturedMidBoss3Attack->waitRemaining - deltaTime);
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    if (capturedMidBoss3Attack->waitRemaining > 0.0f)
                    {
                        continue;
                    }
                    capturedMidBoss3Attack->launched = true;
                    if (capturedMidBoss3Attack->chargeSoundPlayed)
                    {
                        Audio_StopCue("boss_ruins_rocket_charge");
                    }
                    if (!capturedMidBoss3Attack->launchSoundPlayed)
                    {
                        Audio_PlayCue("boss_ruins_rocket");
                        capturedMidBoss3Attack->launchSoundPlayed = true;
                    }
                    projectile->SetVelocityX(capturedMidBoss3Attack->aimX * kDrillLaunchSpeed);
                    projectile->SetVelocityY(capturedMidBoss3Attack->aimY * kDrillLaunchSpeed);
                }

                const float nextX = transform->x + projectile->GetVelocityX() * deltaTime;
                const float nextY = transform->y + projectile->GetVelocityY() * deltaTime;
                const float attackWidth = transform->width * transform->scale;
                const float attackHeight = transform->height * transform->scale;
                const float hitInsetX = attackWidth * 0.14f;
                const float hitInsetY = attackHeight * 0.22f;
                TransformComponent attackHitRect(
                    nextX + hitInsetX,
                    nextY + hitInsetY,
                    std::max(1.0f, attackWidth - hitInsetX * 2.0f),
                    std::max(1.0f, attackHeight - hitInsetY * 2.0f));
                bool attachedThisFrame = false;
                for (Entity* target : enemyEntities)
                {
                    const auto* targetTransform = target ? target->GetComponent<TransformComponent>() : nullptr;
                    if (!target ||
                        target == entity ||
                        !target->GetComponent<MidBoss3Component>() ||
                        !targetTransform ||
                        !IntersectsBounds(attackHitRect, *targetTransform))
                    {
                        continue;
                    }
                    capturedMidBoss3Attack->attachedToBoss = true;
                    capturedMidBoss3Attack->carriedBoss = target;
                    capturedMidBoss3Attack->bossDamageTimer = 0.0f;
                    capturedMidBoss3Attack->attachedLifeRemaining = 4.0f;
                    projectile->SetVelocityX(0.0f);
                    projectile->SetVelocityY(0.0f);
                    aimTowardTarget(target);
                    if (screenShakeEnabled)
                    {
                        flow.hitStopRemaining = std::max(flow.hitStopRemaining, 0.115f);
                        flow.screenShakeRemaining = std::max(flow.screenShakeRemaining, 0.34f);
                        flow.screenShakeDuration = std::max(flow.screenShakeDuration, 0.34f);
                        flow.screenShakeAmplitude = std::max(flow.screenShakeAmplitude, 44.0f);
                    }
                    if (!capturedMidBoss3Attack->hitSoundPlayed)
                    {
                        Audio_PlayCue("boss_ruins_rocket_hit");
                        capturedMidBoss3Attack->hitSoundPlayed = true;
                    }
                    handleEnemyDamage(*target, entity, getDamageAgainstTarget(*target), "Captured MidBoss3 drill damaged enemy");
                    (void)pushAttachedTarget(*target, std::max(deltaTime, 1.0f / 60.0f));
                    attachedThisFrame = true;
                    break;
                }
                if (attachedThisFrame)
                {
                    continue;
                }

                if (rectIntersectsSolid(nextX, nextY, attackWidth, attackHeight))
                {
                    if (!capturedMidBoss3Attack->hitSoundPlayed)
                    {
                        Audio_PlayCue("boss_ruins_rocket_hit");
                        capturedMidBoss3Attack->hitSoundPlayed = true;
                    }
                    bulletsToRemove.push_back(entity);
                    continue;
                }
                transform->x = nextX;
                transform->y = nextY;
                transform->rotation = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());

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
                            handleEnemyDamage(*targetBoss, entity, getDamageAgainstTarget(*targetBoss), "Captured MidBoss2 spear hit boss");
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
                    spawnMidBoss2SpearFadeEffect(
                        spearEnd.x,
                        spearEnd.y,
                        transform->width * transform->scale,
                        transform->height * transform->scale);
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
                    spawnMidBoss2SpearFadeEffect(
                        spearEnd.x,
                        spearEnd.y,
                        transform->width * transform->scale,
                        transform->height * transform->scale);
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
                CollisionPoint spearStart;
                CollisionPoint spearEnd;
                float spearRadius = 0.0f;
                BuildMidBoss2SpearTipCollisionSegment(*transform, *projectile, *spear, spearStart, spearEnd, spearRadius);
                spawnMidBoss2SpearFadeEffect(
                    spearEnd.x,
                    spearEnd.y,
                    transform->width * transform->scale,
                    transform->height * transform->scale);
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

        if (projectile->GetOwner() == ProjectileComponent::Owner::BlasterRobot)
        {
            bool hitObstacle = false;
            for (const auto& obstacle : obstacleBounds)
            {
                if (IntersectsBounds(*transform, obstacle))
                {
                    hitObstacle = true;
                    break;
                }
            }
            if (hitObstacle)
            {
                bulletsToRemove.push_back(entity);
                continue;
            }
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

                handleEnemyDamage(*target, entity, getDamageAgainstTarget(*target), "Photo bullet hit enemy");
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
