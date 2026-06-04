#pragma once

#include "game_scene_combat_common.h"

namespace game_scene_combat_system
{
template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename IsSolidTileFn>
inline void UpdateBullets(
    const std::vector<Entity*>& bulletEntities,
    const std::vector<Entity*>& enemyEntities,
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
                    continue;
                }
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
