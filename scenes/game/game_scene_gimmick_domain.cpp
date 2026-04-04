#include "game_scene_internal.h"
#include "game_scene_world_interaction_system.h"

#include <cctype>

using namespace game_scene_detail;

namespace
{
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kStageTransitionFadeOutDuration = 0.45f;
}

void GameScene::HandleWorldInteractions()
{
    Entity* player = FindEntityByTag(kTagPlayer);
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

    if (TryQueueStageTransition(*player))
    {
        return;
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
            ApplyHazardDamageToPlayer(playerEntity, sourceEntity, logMessage);
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
            const auto* gimmick = b.GetComponent<GimmickComponent>();
            if (gimmick && gimmick->GetType() == GimmickType::Hazard)
            {
                return IntersectsHazardEntity(a, b);
            }
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            ApplyHazardDamageToPlayer(playerEntity, sourceEntity, logMessage);
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
            const auto* role = b.GetComponent<PhotoCopyRoleComponent>();
            if (role && role->role == PhotoCopyRole::Hazard)
            {
                return IntersectsHazardEntity(a, b);
            }
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            ApplyHazardDamageToPlayer(playerEntity, sourceEntity, logMessage);
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

bool GameScene::TryQueueStageTransition(Entity& player)
{
    if (m_flow.stageTransitionActive || m_hasPendingStageTransition)
    {
        return false;
    }

    if (gStageTransitionLinks.empty())
    {
        gLastStageTransitionMarker = '\0';
        return false;
    }

    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        gLastStageTransitionMarker = '\0';
        return false;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        gLastStageTransitionMarker = '\0';
        return false;
    }

    const float centerX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
    const float centerY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
    const int column = static_cast<int>(centerX / tileSize);
    const int row = static_cast<int>(centerY / tileSize);
    const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
    if (marker == '\0')
    {
        gLastStageTransitionMarker = '\0';
        return false;
    }

    if (marker == gLastStageTransitionMarker)
    {
        return false;
    }

    const StageTransitionLink* matchedLink = nullptr;
    for (const StageTransitionLink& link : gStageTransitionLinks)
    {
        const bool sourceMatches =
            link.sourceMapCsv == "*" ||
            link.sourceMapCsv == gCurrentMapCsvPath;
        if (!sourceMatches || link.marker != marker)
        {
            continue;
        }

        matchedLink = &link;
        break;
    }

    if (!matchedLink)
    {
        return false;
    }

    m_hasPendingStageTransition = true;
    m_pendingStageTransitionMapCsv = matchedLink->destinationMapCsv;
    m_pendingStageTransitionSpawnMarker = matchedLink->spawnMarker;
    m_pendingStageTransitionMarker = marker;
    gLastStageTransitionMarker = marker;
    m_flow.stageTransitionActive = true;
    m_flow.stageTransitionTimer = kStageTransitionFadeOutDuration;
    m_flow.stageTransitionFadeInTimer = 0.0f;
    m_flow.cameraMode = false;
    m_flow.captureSlowRemaining = 0.0f;
    m_flow.placementSlowRemaining = 0.0f;
    m_photo.placement.active = false;
    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "scene_change", 0.0f, 0.0f });
    return true;
}

bool GameScene::ExecuteStageTransition(const std::string& destinationMapCsv, char spawnMarker, char marker)
{
    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return false;
    }

    const GameSessionState session = GameSession_Get();
    gCurrentMapCsvPath = destinationMapCsv;
    gLastStageTransitionMarker = marker;

    m_entities.clear();
    m_pendingEntities.clear();
    m_photo = PhotoState{};
    m_player = GameScenePlayerState{};
    m_flow = GameSceneFlowState{};
    m_effects = GameSceneEffectsState{};
    m_mapEditor = GameSceneMapEditorState{};
    m_cameraTransitionMarkers.clear();
    m_cameraFixedRanges.clear();
    m_hasPreviousPlayerCameraProbe = false;
    m_previousPlayerCameraProbeX = 0.0f;
    m_previousPlayerCameraProbeY = 0.0f;
    m_floorCameraTransitionActive = false;
    m_floorCameraTransitionElapsed = 0.0f;
    m_floorCameraTransitionStartX = 0.0f;
    m_floorCameraTransitionStartY = 0.0f;
    m_floorCameraTransitionTargetX = 0.0f;
    m_floorCameraTransitionTargetY = 0.0f;
    m_cameraFixedLockActive = false;
    m_cameraFixedLockStartX = 0.0f;
    m_cameraFixedLockEndX = 0.0f;
    m_cameraFixedLockX = 0.0f;
    m_cameraFixedLockY = 0.0f;
    m_hasPendingStageTransition = false;
    m_pendingStageTransitionMapCsv.clear();
    m_pendingStageTransitionSpawnMarker = '\0';
    m_pendingStageTransitionMarker = '\0';
    m_flow.timeLimit = session.timeLimit;
    m_flow.timeRemaining = session.timeRemaining;

    m_eventBus.Clear();
    m_physicsWorld.Shutdown();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
    if (!m_tileMap.LoadFromCsv(gCurrentMapCsvPath, tileSize))
    {
        return false;
    }

    InitializeStageEntities();

    Entity* transitionedPlayer = FindEntityByTag(kTagPlayer);
    if (transitionedPlayer)
    {
        if (auto* transformed = transitionedPlayer->GetComponent<TransformComponent>())
        {
            if (spawnMarker != '\0')
            {
                bool foundSpawnMarker = false;
                for (int spawnRow = 0; spawnRow < m_tileMap.GetHeight() && !foundSpawnMarker; ++spawnRow)
                {
                    for (int spawnColumn = 0; spawnColumn < m_tileMap.GetWidth(); ++spawnColumn)
                    {
                        const char tileMarker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(spawnColumn, spawnRow))));
                        if (tileMarker != spawnMarker)
                        {
                            continue;
                        }

                        const float playerWidth = transformed->width * transformed->scale;
                        const float playerHeight = transformed->height * transformed->scale;
                        transformed->x = static_cast<float>(spawnColumn) * tileSize + (tileSize - playerWidth) * 0.5f;
                        transformed->y = static_cast<float>(spawnRow) * tileSize + (tileSize - playerHeight) * 0.5f;
                        m_flow.stageStartX = transformed->x;
                        m_flow.stageStartY = transformed->y;
                        m_flow.respawnX = transformed->x;
                        m_flow.respawnY = transformed->y;
                        m_flow.hasCheckpoint = false;
                        m_flow.activeCheckpointId = -1;
                        foundSpawnMarker = true;
                        break;
                    }
                }
            }

            const float playerWidth = transformed->width * transformed->scale;
            const float playerHeight = transformed->height * transformed->scale;
            const float mapWidth = GetMapPixelWidth();
            const float mapHeight = GetMapPixelHeight();
            const float targetCameraX = transformed->x - (gCameraViewWidth - playerWidth) * 0.5f;
            m_flow.cameraX = std::clamp(targetCameraX, 0.0f, std::max(0.0f, mapWidth - gCameraViewWidth));
            if (gCameraFollowY >= 0.5f)
            {
                const float targetCameraY = transformed->y - (gCameraViewHeight - playerHeight) * 0.5f;
                m_flow.cameraY = std::clamp(targetCameraY, 0.0f, std::max(0.0f, mapHeight - gCameraViewHeight));
            }
            else
            {
                m_flow.cameraY = 0.0f;
            }
        }

        if (auto* health = transitionedPlayer->GetComponent<HealthComponent>())
        {
            health->SetCurrentHealth(session.currentHp);
            GameSession_SetCurrentHp(health->GetCurrentHealth());
        }
    }

    GameSession_SetTimeRemaining(session.timeRemaining);
    Entity* currentPlayer = FindEntityByTag(kTagPlayer);
    m_eventBus.Publish({ EventType::PlaySoundRequest, currentPlayer, nullptr, "scene_change", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, currentPlayer, nullptr, std::string("Stage transition: ") + gCurrentMapCsvPath, 0.0f, 0.0f });
    return true;
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
        ApplyHazardDamageToPlayer(player, nullptr, "GameScene player damaged by hazard tile");
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
                ApplyHazardDamageToPlayer(player, entity.get(), "GameScene player damaged by enemy");
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
            ApplyHazardDamageToPlayer(player, entity.get(), "GameScene player damaged by gimmick hazard");
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
        if (!entity || !HasTag(*entity, kTagPhotoBox))
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
    m_flow.pitRestartFadeInTimer = 0.0f;
    m_flow.cameraMode = false;
    m_flow.captureSlowRemaining = 0.0f;
    m_flow.placementSlowRemaining = 0.0f;
    m_photo.placement.active = false;
    m_flow.playerTouchingHazard = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, nullptr, logMessage, 0.0f, 0.0f });
}

