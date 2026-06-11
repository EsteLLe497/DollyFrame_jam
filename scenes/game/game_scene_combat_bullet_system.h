#pragma once

#include <limits>

#include "game_scene_combat_common.h"

namespace game_scene_combat_system
{
template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename IsSolidTileFn>
inline void UpdateBullets(
    const std::vector<Entity*>& bulletEntities,
    const std::vector<Entity*>& enemyEntities,
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
                constexpr float kBossKnockbackSeconds = 3.0f;
                constexpr float kBossDamageInterval = 1.0f;
                Entity* targetBoss = capturedMidBoss3Attack->carriedBoss
                    ? capturedMidBoss3Attack->carriedBoss
                    : findMidBoss3Target();

                if (capturedMidBoss3Attack->attachedToBoss && targetBoss)
                {
                    capturedMidBoss3Attack->knockbackRemaining -= deltaTime;
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

                        const float nextX = std::clamp(currentX + pushX, 0.0f, maxX);
                        if (std::fabs(pushX) > 0.01f && !rectIntersectsSolid(nextX, currentY, bossWidth, bossHeight))
                        {
                            resolvedX = nextX;
                        }
                        else if (std::fabs(pushX) > 0.01f)
                        {
                            stoppedBySolid = true;
                        }

                        const float nextY = std::clamp(currentY + pushY, 0.0f, maxY);
                        if (std::fabs(pushY) > 0.01f && !rectIntersectsSolid(resolvedX, nextY, bossWidth, bossHeight))
                        {
                            resolvedY = nextY;
                        }
                        else if (std::fabs(pushY) > 0.01f)
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
                        capturedMidBoss3Attack->knockbackRemaining = 0.0f;
                    }
                    if (capturedMidBoss3Attack->knockbackRemaining <= 0.0f)
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

                if (!capturedMidBoss3Attack->groundRush)
                {
                    const float nextX = transform->x + projectile->GetVelocityX() * deltaTime;
                    const float nextY = transform->y + projectile->GetVelocityY() * deltaTime;
                    const bool hitSolidTile =
                        isSolidTile(nextX + transform->width * transform->scale * 0.5f, nextY + transform->height * transform->scale * 0.5f) ||
                        nextY + transform->height * transform->scale >= mapHeight - 96.0f;
                    if (hitSolidTile && projectile->GetVelocityY() >= 0.0f)
                    {
                        transform->y = std::clamp(nextY, 0.0f, std::max(0.0f, mapHeight - 96.0f - transform->height * transform->scale));
                        capturedMidBoss3Attack->groundRush = true;
                        capturedMidBoss3Attack->aimX = static_cast<float>(capturedMidBoss3Attack->direction);
                        capturedMidBoss3Attack->aimY = 0.0f;
                        projectile->SetVelocityX(capturedMidBoss3Attack->aimX * kDrillRushSpeed);
                        projectile->SetVelocityY(0.0f);
                        transform->rotation = 0.0f;
                    }
                    else
                    {
                        transform->x = nextX;
                        transform->y = nextY;
                        transform->rotation = std::atan2(projectile->GetVelocityY(), projectile->GetVelocityX());
                    }
                }
                else
                {
                    transform->x += projectile->GetVelocityX() * deltaTime;
                    transform->rotation = capturedMidBoss3Attack->direction >= 0 ? 0.0f : 3.14159265f;
                }

                bool attachedThisFrame = false;
                for (Entity* target : enemyEntities)
                {
                    if (!target || target == entity || !intersectsEntity(*target, *entity))
                    {
                        continue;
                    }
                    capturedMidBoss3Attack->attachedToBoss = true;
                    capturedMidBoss3Attack->carriedBoss = target;
                    capturedMidBoss3Attack->knockbackRemaining = kBossKnockbackSeconds;
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
            if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
            {
                bulletsToRemove.push_back(entity);
                continue;
            }

            bulletsToRemove.push_back(entity);
            continue;
        }

        if (auto* spear = entity->GetComponent<MidBoss2SpearComponent>())
        {
            if (!spear->stuck && !obstacleBounds.empty())
            {
                const float previousX = transform->x - projectile->GetVelocityX() * deltaTime;
                const float previousY = transform->y - projectile->GetVelocityY() * deltaTime;
                const auto collidesAt = [&](float x, float y) -> bool
                {
                    TransformComponent candidate = *transform;
                    candidate.x = x;
                    candidate.y = y;
                    for (const auto& obstacle : obstacleBounds)
                    {
                        if (IntersectsBounds(candidate, obstacle))
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (collidesAt(transform->x, transform->y))
                {
                    float low = 0.0f;
                    float high = 1.0f;
                    const float deltaX = transform->x - previousX;
                    const float deltaY = transform->y - previousY;
                    for (int iteration = 0; iteration < 8; ++iteration)
                    {
                        const float mid = (low + high) * 0.5f;
                        const float candidateX = previousX + deltaX * mid;
                        const float candidateY = previousY + deltaY * mid;
                        if (collidesAt(candidateX, candidateY))
                        {
                            high = mid;
                        }
                        else
                        {
                            low = mid;
                        }
                    }

                    transform->x = previousX + deltaX * low;
                    transform->y = previousY + deltaY * low;
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
            }
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
