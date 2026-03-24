#include "game_scene_internal.h"
#include "game_scene_world_interaction_system.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPitRestartFadeDuration = 0.45f;
}

void GameScene::HandleWorldInteractions()
{
    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    m_flow.playerTouchingHazard = false;
    m_flow.playerTouchingTarget = false;

    if (const auto* playerTransform = player->GetComponent<TransformComponent>())
    {
        if (IntersectsPitTile(*playerTransform))
        {
            StartPitRestart(player, "GameScene player fell into pit tile");
            return;
        }
    }

    HandleEnemyPlayerCollisions(*player);
    if (m_flow.pitRestartActive || m_flow.resultQueued)
    {
        return;
    }

    game_scene_world_interaction_system::HandleTileInteractions(
        m_flow,
        *player,
        [this](const TransformComponent& transform)
        {
            return IntersectsHazardTile(transform);
        },
        [this](const TransformComponent& transform)
        {
            return IntersectsGoalTile(transform);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        m_eventBus);

    std::vector<Entity*> consumedGimmicks;
    game_scene_world_interaction_system::HandleEntityInteractions(
        m_entities,
        m_flow,
        *player,
        consumedGimmicks,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        [this](Entity& playerEntity, Entity& checkpointEntity)
        {
            ActivateCheckpoint(playerEntity, checkpointEntity);
        },
        m_eventBus);

    std::vector<Entity*> consumedPickups;
    std::vector<Entity*> defeatedEnemies;
    game_scene_world_interaction_system::HandlePhotoBoxInteractions(
        m_entities,
        m_flow,
        *player,
        consumedPickups,
        defeatedEnemies,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        },
        [this](GameEndReason reason)
        {
            QueueResult(reason);
        },
        m_eventBus);

    std::vector<Entity*> entitiesToRemove = consumedGimmicks;
    entitiesToRemove.insert(entitiesToRemove.end(), consumedPickups.begin(), consumedPickups.end());
    game_scene_world_interaction_system::RemoveEntitiesByPointerList(m_entities, entitiesToRemove);

    if (!defeatedEnemies.empty())
    {
        m_eventBus.Publish({ EventType::LogMessage, player, defeatedEnemies.front(), "Invert photo neutralized an enemy", 0.0f, 0.0f });
    }
}

void GameScene::HandleWorldTileInteractions(Entity& player)
{
    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    if (IntersectsHazardTile(*playerTransform))
    {
        m_flow.playerTouchingHazard = true;
        HandlePlayerDamage(player, nullptr, "GameScene player damaged by hazard tile");
    }

    if (IntersectsPitTile(*playerTransform))
    {
        StartPitRestart(&player, "GameScene player fell into pit tile");
        return;
    }

    if (m_flow.goalUnlocked && IntersectsGoalTile(*playerTransform))
    {
        m_flow.playerTouchingTarget = true;
        if (!m_flow.resultQueued)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "contact_tone", 0.0f, 0.0f });
            QueueResult(GameEndReason::GoalReached);
        }
    }
}

void GameScene::HandleWorldEntityInteractions(Entity& player, std::vector<Entity*>& consumedGimmicks)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == &player || !IntersectsEntity(player, *entity))
        {
            continue;
        }

        if (const auto* enemy = entity->GetComponent<EnemyComponent>())
        {
            if (enemy->IsEnabled())
            {
                m_flow.playerTouchingHazard = true;
                HandlePlayerDamage(player, entity.get(), "GameScene player damaged by enemy");
            }
        }

        auto* gimmick = entity->GetComponent<GimmickComponent>();
        if (!gimmick || !gimmick->IsEnabled())
        {
            continue;
        }

        switch (gimmick->GetType())
        {
        case GimmickType::Hazard:
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by gimmick hazard");
            break;
        case GimmickType::Goal:
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case GimmickType::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up gimmick item", 0.0f, 0.0f });
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

void GameScene::HandlePhotoBoxInteractions(Entity& player, std::vector<Entity*>& consumedPickups, std::vector<Entity*>& defeatedEnemies)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* photoRole = entity->GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        if (!photoRole || !IntersectsEntity(player, *entity))
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
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by copied hazard");
            break;
        case PhotoCopyRole::Ally:
            for (const auto& enemyEntity : m_entities)
            {
                if (!enemyEntity || enemyEntity.get() == entity.get())
                {
                    continue;
                }

                auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
                if (!enemy || !enemy->IsEnabled() || !IntersectsEntity(*entity, *enemyEntity))
                {
                    continue;
                }

                enemy->MarkDefeated();
                defeatedEnemies.push_back(enemyEntity.get());
            }
            break;
        case PhotoCopyRole::GoalRelay:
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case PhotoCopyRole::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up copied item", 0.0f, 0.0f });
            consumedPickups.push_back(entity.get());
            break;
        case PhotoCopyRole::Solid:
        default:
            break;
        }
    }
}

void GameScene::RemoveEntitiesByPointerList(const std::vector<Entity*>& entitiesToRemove)
{
    if (entitiesToRemove.empty())
    {
        return;
    }

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [&](const std::unique_ptr<Entity>& entity)
            {
                return entity && std::find(entitiesToRemove.begin(), entitiesToRemove.end(), entity.get()) != entitiesToRemove.end();
            }),
        m_entities.end());
}

void GameScene::ActivateCheckpoint(Entity& player, Entity& checkpoint)
{
    auto* checkpointData = checkpoint.GetComponent<CheckpointComponent>();
    if (!checkpointData)
    {
        return;
    }

    if (m_flow.activeCheckpointId == checkpointData->checkpointId)
    {
        return;
    }

    m_flow.hasCheckpoint = true;
    m_flow.activeCheckpointId = checkpointData->checkpointId;
    m_flow.respawnX = checkpointData->respawnX;
    m_flow.respawnY = checkpointData->respawnY;
    checkpointData->activated = true;
    if (auto* tint = checkpoint.GetComponent<TintComponent>())
    {
        tint->r = 0.80f;
        tint->g = 0.92f;
        tint->b = 1.0f;
        tint->a = 1.0f;
    }

    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, &checkpoint, "scene_change", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &player, &checkpoint, "Checkpoint activated", 0.0f, 0.0f });
}

void GameScene::QueueResult(GameEndReason reason)
{
    if (m_flow.resultQueued)
    {
        return;
    }

    m_flow.resultQueued = true;
    GameSession_SetEndReason(reason);
    m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
}

void GameScene::StartPitRestart(Entity* player, const char* logMessage)
{
    if (m_flow.pitRestartActive)
    {
        return;
    }

    m_flow.pitRestartActive = true;
    m_flow.pitRestartTimer = kPitRestartFadeDuration;
    m_flow.cameraMode = false;
    m_flow.captureSlowRemaining = 0.0f;
    m_flow.placementSlowRemaining = 0.0f;
    m_photo.placement.active = false;
    m_flow.playerTouchingHazard = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, nullptr, logMessage, 0.0f, 0.0f });
}

