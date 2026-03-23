#pragma once

#include <algorithm>
#include <vector>

#include "components.h"
#include "entity.h"
#include "event_bus.h"
#include "game_scene_photo_state.h"
#include "game_scene_state.h"

namespace game_scene_world_interaction_system
{
using DefeatedEnemyCallback = void(*)(Entity*);

inline void RemoveEntitiesByPointerList(
    std::vector<std::unique_ptr<Entity>>& entities,
    const std::vector<Entity*>& entitiesToRemove)
{
    if (entitiesToRemove.empty())
    {
        return;
    }

    entities.erase(
        std::remove_if(
            entities.begin(),
            entities.end(),
            [&](const std::unique_ptr<Entity>& entity)
            {
                return entity && std::find(entitiesToRemove.begin(), entitiesToRemove.end(), entity.get()) != entitiesToRemove.end();
            }),
        entities.end());
}

template <typename IntersectsHazardTileFn, typename IntersectsGoalTileFn, typename HandlePlayerDamageFn, typename QueueResultFn>
void HandleTileInteractions(
    GameSceneFlowState& flow,
    Entity& player,
    IntersectsHazardTileFn&& intersectsHazardTile,
    IntersectsGoalTileFn&& intersectsGoalTile,
    HandlePlayerDamageFn&& handlePlayerDamage,
    QueueResultFn&& queueResult,
    EventBus& eventBus)
{
    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    if (intersectsHazardTile(*playerTransform))
    {
        flow.playerTouchingHazard = true;
        handlePlayerDamage(player, nullptr, "GameScene player damaged by hazard tile");
    }

    if (flow.goalUnlocked && intersectsGoalTile(*playerTransform))
    {
        flow.playerTouchingTarget = true;
        if (!flow.resultQueued)
        {
            eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "contact_tone", 0.0f, 0.0f });
            queueResult(GameEndReason::GoalReached);
        }
    }
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename QueueResultFn>
void HandleEntityInteractions(
    std::vector<std::unique_ptr<Entity>>& entities,
    GameSceneFlowState& flow,
    Entity& player,
    std::vector<Entity*>& consumedGimmicks,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage,
    QueueResultFn&& queueResult,
    EventBus& eventBus)
{
    for (const auto& entity : entities)
    {
        if (!entity || entity.get() == &player || !intersectsEntity(player, *entity))
        {
            continue;
        }

        auto* gimmick = entity->GetComponent<GimmickComponent>();
        if (!gimmick || !gimmick->IsEnabled())
        {
            continue;
        }

        switch (gimmick->GetType())
        {
        case GimmickType::Hazard:
            flow.playerTouchingHazard = true;
            handlePlayerDamage(player, entity.get(), "GameScene player damaged by gimmick hazard");
            break;
        case GimmickType::Goal:
            if (flow.goalUnlocked && !flow.resultQueued)
            {
                flow.playerTouchingTarget = true;
                eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                queueResult(GameEndReason::GoalReached);
            }
            break;
        case GimmickType::Pickup:
            eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up gimmick item", 0.0f, 0.0f });
            gimmick->Consume();
            if (gimmick->IsConsumed())
            {
                consumedGimmicks.push_back(entity.get());
            }
            break;
        case GimmickType::PhotoSource:
        case GimmickType::Gate:
        case GimmickType::Switch:
        default:
            break;
        }
    }
}

template <typename IntersectsEntityFn, typename HandlePlayerDamageFn, typename QueueResultFn>
void HandlePhotoBoxInteractions(
    std::vector<std::unique_ptr<Entity>>& entities,
    GameSceneFlowState& flow,
    Entity& player,
    std::vector<Entity*>& consumedPickups,
    std::vector<Entity*>& defeatedEnemies,
    IntersectsEntityFn&& intersectsEntity,
    HandlePlayerDamageFn&& handlePlayerDamage,
    QueueResultFn&& queueResult,
    EventBus& eventBus)
{
    for (const auto& entity : entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* tag = entity->GetComponent<TagComponent>();
        if (!tag || tag->tag != "PhotoBox")
        {
            continue;
        }

        const auto* photoRole = entity->GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        if (!photoRole || !intersectsEntity(player, *entity))
        {
            continue;
        }
        if (photoLayer && photoLayer->layer != PhotoCopyLayer::Foreground)
        {
            continue;
        }

        switch (photoRole->role)
        {
        case PhotoCopyRole::Hazard:
            flow.playerTouchingHazard = true;
            handlePlayerDamage(player, entity.get(), "GameScene player damaged by copied hazard");
            break;
        case PhotoCopyRole::Ally:
            for (const auto& enemyEntity : entities)
            {
                if (!enemyEntity || enemyEntity.get() == entity.get())
                {
                    continue;
                }

                auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
                if (!enemy || !enemy->IsEnabled() || !intersectsEntity(*entity, *enemyEntity))
                {
                    continue;
                }

                enemy->MarkDefeated();
                defeatedEnemies.push_back(enemyEntity.get());
            }
            break;
        case PhotoCopyRole::GoalRelay:
            if (flow.goalUnlocked && !flow.resultQueued)
            {
                flow.playerTouchingTarget = true;
                eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                queueResult(GameEndReason::GoalReached);
            }
            break;
        case PhotoCopyRole::Pickup:
            eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up copied item", 0.0f, 0.0f });
            consumedPickups.push_back(entity.get());
            break;
        case PhotoCopyRole::Solid:
        default:
            break;
        }
    }
}

inline void RemoveDefeatedEnemies(std::vector<std::unique_ptr<Entity>>& entities)
{
    entities.erase(
        std::remove_if(
            entities.begin(),
            entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                const auto* enemy = entity ? entity->GetComponent<EnemyComponent>() : nullptr;
                if (enemy && enemy->IsDefeated())
                {
                    return true;
                }

                const auto* lifetime = entity ? entity->GetComponent<PhotoCopyLifetimeComponent>() : nullptr;
                if (!lifetime)
                {
                    return false;
                }

                return lifetime->IsExpired();
            }),
        entities.end());
}

inline bool IsPlayerDamageBlocked(const GameScenePlayerState& playerState, float dodgeDuration, float dodgeInvincibilitySeconds)
{
    const float dodgeElapsed = std::max(0.0f, dodgeDuration - playerState.dodgeRemaining);
    const float dodgeInvincibility = std::min(dodgeInvincibilitySeconds, dodgeDuration);
    return playerState.dodgeRemaining > 0.0f && dodgeElapsed < dodgeInvincibility;
}
}
