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

inline void UpdateEnemies(
    std::vector<std::unique_ptr<Entity>>& entities,
    int tileTexture,
    GameSceneFlowState& flow,
    const PhotoState& photo,
    const TransformComponent* playerTransform)
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
            const float dist = std::fabs(dx);
            constexpr float kWalkerSpeed = 120.0f;

            switch (enemy->GetAIState())
            {
            case EnemyComponent::AIState::Idle:
                if (dist < enemy->detectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                break;
            case EnemyComponent::AIState::Chase:
                if (dist < enemy->attackRange)
                {
                    enemy->attackTimer = 0.0f;
                    enemy->SetAIState(EnemyComponent::AIState::Attack);
                }
                else if (dist > enemy->detectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Idle);
                }
                else
                {
                    transform->x += (dx > 0.0f ? 1.0f : -1.0f) * kWalkerSpeed * flow.lastDeltaTime;
                }
                break;
            case EnemyComponent::AIState::Attack:
                enemy->attackTimer += flow.lastDeltaTime;
                if (dist >= enemy->attackRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                }
                else if (enemy->attackTimer >= enemy->attackCooldown)
                {
                    enemy->attackTimer = 0.0f;
                }
                break;
            }
        }
        else if (enemy->GetArchetype() == EnemyArchetype::Ranged)
        {
            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            enemy->attackTimer += flow.lastDeltaTime;

            if (dist < enemy->detectRange && enemy->attackTimer >= enemy->attackCooldown)
            {
                enemy->attackTimer = 0.0f;

                constexpr float kBulletSpeed = 300.0f;
                const float length = std::max(1.0f, dist);
                const float velX = (dx / length) * kBulletSpeed;
                const float velY = (dy / length) * kBulletSpeed;

                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>("Bullet");
                bullet->AddComponent<TransformComponent>(
                    transform->x + 24.0f,
                    transform->y + 24.0f,
                    16.0f,
                    16.0f);
                bullet->AddComponent<TintComponent>(1.0f, 0.9f, 0.2f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(tileTexture);
                bullet->AddComponent<ProjectileComponent>(velX, velY, 1);
                newBullets.push_back(std::move(bullet));
            }
        }
    }

    for (auto& bullet : newBullets)
    {
        entities.push_back(std::move(bullet));
    }

    flow.goalUnlocked = photo.groups.hasSpawnedCopy;
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn>
void UpdateBullets(
    std::vector<std::unique_ptr<Entity>>& entities,
    float mapWidth,
    float mapHeight,
    float deltaTime,
    Entity* player,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage)
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

                if (player && intersectsEntity(*player, *entity))
                {
                    handlePlayerDamage(*player, entity.get(), "GameScene player damaged by bullet");
                    return true;
                }

                return transform->x < 0.0f
                    || transform->x > mapWidth
                    || transform->y < 0.0f
                    || transform->y > mapHeight;
            }),
        entities.end());
}
}
