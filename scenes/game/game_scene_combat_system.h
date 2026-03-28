#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "components.h"
#include "entity.h"
#include "game_scene_photo_state.h"
#include "game_scene_state.h"

namespace game_scene_combat_system
{
inline bool HasTag(const Entity& entity, const char* value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->tag == value;
}

template <typename SnapToGroundFn, typename PlayEnemyGunFn>
inline void UpdateEnemies(
    std::vector<std::unique_ptr<Entity>>& entities,
    int tileTexture,
    GameSceneFlowState& flow,
    const PhotoState& photo,
    const TransformComponent* playerTransform,
    SnapToGroundFn&& snapToGround,
    PlayEnemyGunFn&& playEnemyGun)
    
{
    flow.enemyCount = 0;
    std::vector<std::unique_ptr<Entity>> newBullets;

    for (const auto& entity : entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled())
        {
            continue;
        }

        ++flow.enemyCount;

        if (!playerTransform)
        {
            continue;
        }

        auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        if (enemy->GetArchetype() == EnemyArchetype::Walker)
        {
            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::fabs(dx);
            constexpr float kWalkerSpeed = 120.0f;
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;

            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

            // d—Íˆ—
            enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
            transform->y += enemy->velocityY * flow.lastDeltaTime;
            const bool onGround = snapToGround(*transform);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            // Œü‚«XV
            if (enemy->GetAIState() != EnemyComponent::AIState::Attack)
            {
                enemy->facing = dx > 0.0f
                    ? EnemyComponent::FacingDirection::Right
                    : EnemyComponent::FacingDirection::Left;
            }

            // UŒ‚ŽlŠp‚ÌŽc‚èŽžŠÔ‚ðXV
            if (enemy->attackRectActive)
            {
                enemy->attackRectRemaining -= flow.lastDeltaTime;
                if (enemy->attackRectRemaining <= 0.0f)
                {
                    enemy->attackRectActive = false;
                }
            }

            switch (enemy->GetAIState())
            {
            case EnemyComponent::AIState::Idle:
                if (inDetectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            case EnemyComponent::AIState::Chase:
                if (dist < enemy->attackRange)
                {
                    // UŒ‚ŠJŽnŽž‚ÉŒü‚«‚ðŒÅ’è‚µ‚ÄUŒ‚ŽlŠp‚ð¶¬
                    enemy->facing = dx > 0.0f
                        ? EnemyComponent::FacingDirection::Right
                        : EnemyComponent::FacingDirection::Left;

                    const float attackWidth = 48.0f;
                    const float attackHeight = 60.0f;
                    const float attackOffsetY = transform->height * transform->scale * -0.1f;

                    enemy->attackRectX = enemy->facing == EnemyComponent::FacingDirection::Right
                        ? transform->x + transform->width * transform->scale
                        : transform->x - attackWidth;
                    enemy->attackRectY = transform->y + attackOffsetY;
                    enemy->attackRectWidth = attackWidth;
                    enemy->attackRectHeight = attackHeight;
                    enemy->attackRectRemaining = enemy->attackCooldown;
                    enemy->attackRectActive = true;

                    enemy->attackTimer = 0.0f;
                    enemy->SetAIState(EnemyComponent::AIState::Attack);
                }
                else if (!inDetectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Idle);
                }
                else
                {
                    transform->x += (dx > 0.0f ? 1.0f : -1.0f) * kWalkerSpeed * flow.lastDeltaTime;
                    snapToGround(*transform);
                }
                break;
            case EnemyComponent::AIState::Attack:
                enemy->attackTimer += flow.lastDeltaTime;
                if (dist >= enemy->attackRange)
                {
                    enemy->attackRectActive = false;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                else if (enemy->attackTimer >= enemy->attackCooldown)
                {
                    enemy->attackTimer = 0.0f;
                    enemy->attackRectActive = false;
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            }
        }

        else if (enemy->GetArchetype() == EnemyArchetype::Ranged)
        {
            constexpr float kGravity = 1900.0f;
            constexpr float kMaxFallSpeed = 980.0f;
            enemy->velocityY = std::min(kMaxFallSpeed, enemy->velocityY + kGravity * flow.lastDeltaTime);
            transform->y += enemy->velocityY * flow.lastDeltaTime;
            const bool onGround = snapToGround(*transform);
            if (onGround)
            {
                enemy->velocityY = 0.0f;
            }

            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            
            const bool inDetectRange = dist < enemy->detectRange && std::fabs(dy) < enemy->detectHeight;

      

            enemy->attackTimer += flow.lastDeltaTime;

            if (inDetectRange && enemy->attackTimer >= enemy->attackCooldown)
            {
                enemy->attackTimer = 0.0f;

                constexpr float kBulletSpeed = 300.0f;
                
                const float velX = (dx > 0.0f ? 1.0f : -1.0f) * kBulletSpeed;
                const float velY = 0.0f;

                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>("Bullet");
                bullet->AddComponent<TransformComponent>(
                    transform->x + 24.0f,
                    transform->y + 24.0f,
                    48.0f, 24.0f); 
                bullet->AddComponent<TintComponent>(1.0f, 0.9f, 0.2f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(tileTexture);
                bullet->AddComponent<ProjectileComponent>(velX, velY, 1);
                playEnemyGun(*entity);
                newBullets.push_back(std::move(bullet));
            }
        }
    }

    for (auto& bullet : newBullets)
    {
        entities.push_back(std::move(bullet));
    }

    flow.goalUnlocked = photo.groups.hasSpawnedCopy || flow.goalUnlockedBySwitch;
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename HandleEnemyDamageFn, typename IsSolidTileFn>
void UpdateBullets(
    std::vector<std::unique_ptr<Entity>>& entities,
    float mapWidth,
    float mapHeight,
    float deltaTime,
    Entity* player,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage
    , HandleEnemyDamageFn&& handleEnemyDamage
    , IsSolidTileFn&& isSolidTile) 
{
    entities.erase(
        std::remove_if(
            entities.begin(),
            entities.end(),
            [&](const std::unique_ptr<Entity>& entity) -> bool
            {
                if (!entity || !HasTag(*entity, "Bullet"))
                {
                    return false;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                auto* projectile = entity->GetComponent<ProjectileComponent>();
                if (!transform || !projectile)
                {
                    return false;
                }

                transform->x += projectile->GetVelocityX() * deltaTime;
                transform->y += projectile->GetVelocityY() * deltaTime;

                if (isSolidTile(transform->x, transform->y) ||
                    isSolidTile(transform->x + transform->width, transform->y) ||
                    isSolidTile(transform->x, transform->y + transform->height) ||
                    isSolidTile(transform->x + transform->width, transform->y + transform->height))
                {
                    return true;
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::Enemy &&
                    player && intersectsEntity(*player, *entity))
                {
                    handlePlayerDamage(*player, entity.get(), "GameScene player damaged by bullet");
                    return true;
                }

                if (projectile->GetOwner() == ProjectileComponent::Owner::Photo)
                {
                    for (const auto& target : entities)
                    {
                        if (!target || target.get() == entity.get() || !HasTag(*target, "Enemy"))
                        {
                            continue;
                        }

                        if (!intersectsEntity(*target, *entity))
                        {
                            continue;
                        }

                        handleEnemyDamage(*target, entity.get(), projectile->GetDamage(), "Photo bullet hit enemy");
                        return true;
                    }
                }

                return transform->x < 0.0f
                    || transform->x > mapWidth
                    || transform->y < 0.0f
                    || transform->y > mapHeight;
            }),
        entities.end());
}
}
