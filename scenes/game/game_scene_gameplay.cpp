#include "game_scene_internal.h"
#include "game_scene_combat_system.h"
#include "game_scene_player_system.h"
#include "game_scene_player_movement_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_photo_tray_system.h"
#include "game_scene_world_interaction_system.h"
#include "photo_system.h"

using namespace game_scene_detail;

void GameScene::UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame)
{
    game_scene_player_visual_system::UpdatePresentation(
        m_player,
        player,
        deltaTime,
        moveAxis,
        wasGrounded,
        isDodging,
        landedThisFrame);
}

void GameScene::UpdateTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    auto entries = BuildGameSceneTuningEntries();
    const int kEntryCount = static_cast<int>(entries.size());

    if (Input_IsActionPressed(InputAction::MoveUp))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + kEntryCount - 1) % kEntryCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + 1) % kEntryCount;
    }

    float delta = 0.0f;
    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        delta -= entries[m_debug.tuningSelection].step;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        delta += entries[m_debug.tuningSelection].step;
    }

    if (delta != 0.0f)
    {
        *entries[m_debug.tuningSelection].value = std::clamp(
            *entries[m_debug.tuningSelection].value + delta,
            entries[m_debug.tuningSelection].minValue,
            entries[m_debug.tuningSelection].maxValue);
        WriteTuningJsonFile();
    }
}

void GameScene::UpdatePlayer(float deltaTime)
{
    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    const bool blockPlayerInput = m_flow.cameraMode || m_photo.placement.active;
    const auto controls = game_scene_player_system::SampleControls(blockPlayerInput);
    const float moveAxis = controls.moveAxis;

    game_scene_player_system::TickDodgeState(m_player, deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    game_scene_player_system::UpdateFacingFromMoveAxis(m_player, moveAxis);

    if (controls.dodgePressed && game_scene_player_system::TryBeginDodge(
        m_player,
        moveAxis,
        GetPlayerDodgeDuration(),
        gPlayerDodgeCooldown))
    {
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_player.dodgeRemaining > 0.0f;
    m_player.velocityX = game_scene_player_system::GetHorizontalVelocity(
        m_player,
        moveAxis,
        gPlayerDodgeSpeed,
        gPlayerMoveSpeed);

    const float tileSize = m_tileMap.GetTileSize();
    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float previousX = transform->x;
    const float previousY = transform->y;
    const float previousBottom = previousY + playerHeight;
    const bool wasGrounded = IsStandingOnGround(*transform);
    m_player.grounded = wasGrounded;
    if (wasGrounded)
    {
        m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
    }

    if (wasGrounded && m_player.velocityY > 0.0f)
    {
        m_player.velocityY = 0.0f;
    }

    const bool canJumpNow = !isDodging && controls.jumpPressed && m_player.coyoteTimeRemaining > 0.0f;
    if (canJumpNow)
    {
        m_player.velocityY = gPlayerJumpSpeed;
        m_player.grounded = false;
        m_player.coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    if (m_player.grounded && !canJumpNow)
    {
        m_player.velocityY = 0.0f;
    }
    else
    {
        m_player.velocityY = std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * deltaTime);
    }
    const float verticalSnapDistance = std::max(gGroundSnapDistance, std::fabs(m_player.velocityY) * deltaTime + 4.0f);
    const game_scene_player_movement_system::PlayerMovementContext movementContext{
        deltaTime,
        tileSize,
        playerWidth,
        playerHeight,
        mapWidth,
        mapHeight,
        previousX,
        previousY,
        previousBottom,
        verticalSnapDistance,
    };

    game_scene_player_movement_system::ResolveHorizontalTileCollisions(
        *transform,
        m_player,
        movementContext,
        [this](int column, int row)
        {
            return IsSolidTile(column, row);
        });

    if (isDodging)
    {
        TrySpawnPlayerAfterimage(*transform);
    }

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    const bool hasPhotoBox = !photoBoxes.empty();
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    const bool hasPhotoSource = GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
    game_scene_player_movement_system::ResolveHorizontalObjectCollisions(
        *transform,
        m_player,
        movementContext,
        photoBoxes,
        hasPhotoSource,
        photoSourceX,
        photoSourceY,
        photoSourceWidth,
        photoSourceHeight);

    game_scene_player_movement_system::ResolveVerticalMotion(
        *transform,
        m_player,
        wasGrounded,
        movementContext,
        photoBoxes,
        hasPhotoSource,
        photoSourceX,
        photoSourceY,
        photoSourceWidth,
        photoSourceHeight,
        [this](int column, int row)
        {
            return IsSolidTile(column, row);
        },
        [this](int column, int row)
        {
            return IsPlatformTile(column, row);
        },
        [this](TransformComponent& targetTransform, float snapDistance)
        {
            return TrySnapToGround(targetTransform, snapDistance);
        });

    const bool landedThisFrame = !wasGrounded && m_player.grounded;
    UpdatePlayerPresentation(*player, deltaTime, moveAxis, wasGrounded, isDodging, landedThisFrame);

    game_scene_player_movement_system::UpdateCameraX(
        m_flow.cameraX,
        transform->x,
        playerWidth,
        mapWidth,
        deltaTime);
}

void GameScene::UpdatePlayerAfterimages(float deltaTime)
{
    game_scene_player_visual_system::UpdateAfterimages(m_player, deltaTime);
}

void GameScene::TrySpawnPlayerAfterimage(const TransformComponent& transform)
{
    game_scene_player_visual_system::TrySpawnAfterimage(m_player, transform);
}

void GameScene::HandlePhotoCapture()
{
    photo_system::HandleCapture(*this);
}

void GameScene::HandlePhotoSpawn()
{
    photo_system::HandleSpawn(*this);
}

void GameScene::StoreCapturedPhoto()
{
    int slotToStore = -1;
    for (int index = 0; index < static_cast<int>(m_photo.savedCaptures.size()); ++index)
    {
        if (!m_photo.savedCaptures[index].hasPhoto)
        {
            slotToStore = index;
            break;
        }
    }

    if (slotToStore < 0)
    {
        slotToStore = m_photo.nextCaptureSlot;
    }

    m_photo.savedCaptures[slotToStore] = m_photo.capture;
    m_photo.selectedCaptureSlot = slotToStore;
    m_photo.nextCaptureSlot = (slotToStore + 1) % static_cast<int>(m_photo.savedCaptures.size());
}

void GameScene::SetSelectedPhotoSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    const PhotoCaptureState& storedCapture = m_photo.savedCaptures[slotIndex];
    if (!storedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    m_photo.capture = storedCapture;
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = slotIndex;
}

void GameScene::ConsumeSelectedPhotoSlot()
{
    if (m_photo.selectedCaptureSlot < 0 || m_photo.selectedCaptureSlot >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    PhotoCaptureState& selectedCapture = m_photo.savedCaptures[m_photo.selectedCaptureSlot];
    if (!selectedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    selectedCapture = PhotoCaptureState{};

    for (int offset = 1; offset <= static_cast<int>(m_photo.savedCaptures.size()); ++offset)
    {
        const int slotIndex = (m_photo.selectedCaptureSlot + offset) % static_cast<int>(m_photo.savedCaptures.size());
        if (!m_photo.savedCaptures[slotIndex].hasPhoto)
        {
            continue;
        }

        SetSelectedPhotoSlot(slotIndex);
        m_photo.capture.selectedTheme = selectedTheme;
        return;
    }

    m_photo.capture = PhotoCaptureState{};
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = 0;
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
}

void GameScene::UpdatePhotoTraySelection()
{
    game_scene_photo_tray_system::UpdateSelection(
        m_photo,
        m_flow.photoTrayReveal,
        [this](int slotIndex)
        {
            SetSelectedPhotoSlot(slotIndex);
        });
}

void GameScene::UpdateEnemies()
{
    Entity* player = FindEntityByTag("Player");
    const TransformComponent* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    game_scene_combat_system::UpdateEnemies(
        m_entities,
        m_tileTexture,
        m_flow,
        m_photo,
        playerTransform);
}

void GameScene::UpdateBullets()
{
    Entity* player = FindEntityByTag("Player");
    game_scene_combat_system::UpdateBullets(
        m_entities,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow.lastDeltaTime,
        player,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        });
}
void GameScene::HandleAttackHits()
{
    return;
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_flow.goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag("Goal"))
    {
        if (auto* tint = goal->GetComponent<TintComponent>())
        {
            const float pulse = 0.65f + 0.35f * std::sin(m_flow.goalPulse * 3.2f);
            if (m_flow.goalUnlocked)
            {
                tint->r = 0.32f + pulse * 0.18f;
                tint->g = 0.92f;
                tint->b = 0.42f + pulse * 0.20f;
            }
            else
            {
                tint->r = 0.62f + pulse * 0.10f;
                tint->g = 0.30f;
                tint->b = 0.24f;
            }
            tint->a = 1.0f;
        }
    }
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

void GameScene::RemoveDefeatedEnemies()
{
    game_scene_world_interaction_system::RemoveDefeatedEnemies(m_entities);
    RefreshPhotoGroupState();
}

void GameScene::RefreshPhotoGroupState()
{
    m_photo.groups.hasSpawnedCopy = FindEntityByTag("PhotoBox") != nullptr;
    int maxGroupId = 0;
    std::vector<int> groups;
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        if (const auto* group = entity->GetComponent<PhotoCopyGroupComponent>())
        {
            maxGroupId = std::max(maxGroupId, group->groupId);
            if (std::find(groups.begin(), groups.end(), group->groupId) == groups.end())
            {
                groups.push_back(group->groupId);
            }
        }
    }
    m_photo.groups.activeGroupCount = static_cast<int>(groups.size());
    m_photo.groups.nextGroupId = std::max(m_photo.groups.nextGroupId, maxGroupId + 1);
}

void GameScene::HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage)
{
    if (game_scene_world_interaction_system::IsPlayerDamageBlocked(
        m_player,
        GetPlayerDodgeDuration(),
        gPlayerDodgeInvincibilitySeconds))
    {
        return;
    }

    auto* health = player.GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    auto* cooldown = player.GetComponent<DamageCooldownComponent>();
    if (cooldown && !cooldown->CanTakeDamage())
    {
        return;
    }

    if (cooldown)
    {
        cooldown->Trigger();
    }

    health->ApplyDamage(1);
    GameSession_SetCurrentHp(health->GetCurrentHealth());
    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &player, sourceEntity, logMessage, 0.0f, 0.0f });
    if (health->IsDead() && !m_flow.resultQueued)
    {
        QueueResult(GameEndReason::HpZero);
    }
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
