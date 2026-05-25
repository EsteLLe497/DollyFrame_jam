#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"

#include <cctype>
#include <limits>
#include <unordered_map>

using namespace game_scene_detail;

namespace
{
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kStageTransitionFadeOutDuration = 0.45f;
}

void GameScene::UpdateLinkedGimmicks(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    auto* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    const float tileSize = m_tileMap.GetTileSize();
    constexpr float kPlatformPlayerInsetX = 6.0f;
    constexpr float kPlatformBatteryInsetX = 2.0f;

    constexpr float kSwitchTopToleranceMin = 8.0f;
    constexpr float kPlatformTopToleranceMin = 10.0f;

    std::unordered_map<int, bool> linkPowered;
    auto setLinkPowered = [&](int linkId, bool isOn)
    {
        if (!isOn)
        {
            return;
        }
        linkPowered[linkId] = true;
    };
    bool shieldBossDefeated = false;
    bool hasProtectiveWalls = false;
    bool hasIntactProtectiveWall = false;
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }
        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy)
        {
            continue;
        }
        if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss && enemy->IsDefeated())
        {
            shieldBossDefeated = true;
            break;
        }
    }

    auto isPlayerOnTopOfPlatform = [&](const TransformComponent& platform, float topTolerance) -> bool
    {
        if (!playerTransform)
        {
            return false;
        }

        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerLeft = playerTransform->x + kPlatformPlayerInsetX;
        const float playerRight = playerTransform->x + playerWidth - kPlatformPlayerInsetX;
        const float playerBottom = playerTransform->y + playerHeight;
        const float platformWidth = platform.width * platform.scale;
        const float platformLeft = platform.x;
        const float platformRight = platform.x + platformWidth;
        const bool overlapX = playerRight > platformLeft && playerLeft < platformRight;
        if (!overlapX)
        {
            return false;
        }

        return std::fabs(playerBottom - platform.y) <= topTolerance;
    };

    auto isPlayerTouchingPlatform = [&](const TransformComponent& platform, float tolerance) -> bool
    {
        if (!playerTransform)
        {
            return false;
        }

        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float platformWidth = platform.width * platform.scale;
        const float platformHeight = platform.height * platform.scale;

        return playerTransform->x + playerWidth > platform.x - tolerance &&
            playerTransform->x < platform.x + platformWidth + tolerance &&
            playerTransform->y + playerHeight > platform.y - tolerance &&
            playerTransform->y < platform.y + platformHeight + tolerance;
    };
    auto setEntityTint = [](Entity& target, float r, float g, float b)
    {
        if (auto* tint = target.GetComponent<TintComponent>())
        {
            tint->r = r;
            tint->g = g;
            tint->b = b;
            tint->a = 1.0f;
        }
    };

    auto carryPlayerByPlatformDeltaY = [&](
        const TransformComponent& platform,
        float previousTopY,
        float currentTopY,
        float deltaY,
        float topTolerance,
        float horizontalInset)
    {
        if (!playerTransform || std::fabs(deltaY) <= 0.001f)
        {
            return;
        }

        const float platformWidth = platform.width * platform.scale;
        const float playerWidth = playerTransform->width * playerTransform->scale;
        const float playerHeight = playerTransform->height * playerTransform->scale;
        const float playerLeft = playerTransform->x + horizontalInset;
        const float playerRight = playerTransform->x + playerWidth - horizontalInset;
        const float platformLeft = platform.x;
        const float platformRight = platform.x + platformWidth;
        const bool overlapX = playerRight > platformLeft && playerLeft < platformRight;
        const float playerBottom = playerTransform->y + playerHeight;
        const bool stoodOnPreviousTop = overlapX && std::fabs(playerBottom - previousTopY) <= topTolerance;
        const bool stoodOnCurrentTop = overlapX && std::fabs(playerBottom - currentTopY) <= topTolerance;
        if (!stoodOnPreviousTop && !stoodOnCurrentTop)
        {
            return;
        }

        playerTransform->y += deltaY;
        m_player.velocityY = 0.0f;
        m_player.grounded = true;
    };

    auto carryBatteriesByPlatformDeltaY = [&](
        const TransformComponent& platform,
        float previousTopY,
        float currentTopY,
        float deltaY,
        float topTolerance,
        float horizontalInset)
    {
        if (std::fabs(deltaY) <= 0.001f)
        {
            return;
        }

        const float platformWidth = platform.width * platform.scale;
        const float platformLeft = platform.x;
        const float platformRight = platform.x + platformWidth;

        for (const auto& batteryEntity : m_entities)
        {
            if (!batteryEntity)
            {
                continue;
            }

            auto* battery = batteryEntity->GetComponent<BatteryComponent>();
            auto* batteryTransform = batteryEntity->GetComponent<TransformComponent>();
            if (!battery || !batteryTransform)
            {
                continue;
            }

            const float batteryWidth = batteryTransform->width * batteryTransform->scale;
            const float batteryHeight = batteryTransform->height * batteryTransform->scale;
            const float batteryLeft = batteryTransform->x + horizontalInset;
            const float batteryRight = batteryTransform->x + batteryWidth - horizontalInset;
            const float batteryBottom = batteryTransform->y + batteryHeight;
            const bool overlapX = batteryRight > platformLeft && batteryLeft < platformRight;
            const bool wasOnTop = overlapX && std::fabs(batteryBottom - previousTopY) <= topTolerance;
            const bool isOnTop = overlapX && std::fabs(batteryBottom - currentTopY) <= topTolerance;
            if (!wasOnTop && !isOnTop)
            {
                continue;
            }

            batteryTransform->y += deltaY;
            battery->velocityY = 0.0f;
            battery->grounded = true;
        }
    };

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* switchComponent = entity->GetComponent<BatterySwitchComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!switchComponent || !transform)
        {
            continue;
        }

        const float previousY = transform->y;
        const float switchWidth = transform->width * transform->scale;
        const float switchHeight = transform->height * transform->scale;
        const float batteryTopTolerance = std::max(kSwitchTopToleranceMin, switchHeight * 0.7f);
        auto isBatteryOnSwitchTop = [&](const TransformComponent& batteryTransform, float switchTopY) -> bool
        {
            const float batteryWidth = batteryTransform.width * batteryTransform.scale;
            const float batteryHeight = batteryTransform.height * batteryTransform.scale;
            const float batteryLeft = batteryTransform.x + kPlatformBatteryInsetX;
            const float batteryRight = batteryTransform.x + batteryWidth - kPlatformBatteryInsetX;
            const float batteryBottom = batteryTransform.y + batteryHeight;
            const float platformLeft = transform->x;
            const float platformRight = transform->x + switchWidth;
            const bool overlapX = batteryRight > platformLeft && batteryLeft < platformRight;
            const bool onTop = batteryBottom >= switchTopY - batteryTopTolerance
                && batteryBottom <= switchTopY + batteryTopTolerance;
            return overlapX && onTop;
        };

        int batteriesOnTop = 0;
        for (const auto& batteryEntity : m_entities)
        {
            if (!batteryEntity)
            {
                continue;
            }

            auto* battery = batteryEntity->GetComponent<BatteryComponent>();
            auto* batteryTransform = batteryEntity->GetComponent<TransformComponent>();
            if (!battery || !batteryTransform)
            {
                continue;
            }

            if (isBatteryOnSwitchTop(*batteryTransform, transform->y))
            {
                ++batteriesOnTop;
            }
        }

        // 個数判定が一瞬途切れてもチラつかないよう、短い保持時間を設ける。
        switchComponent->insertedBatteryCount = batteriesOnTop;
        const bool hasRequiredBatteries = switchComponent->insertedBatteryCount >= switchComponent->requiredBatteryCount;
        if (hasRequiredBatteries)
        {
            switchComponent->activationGraceRemaining = switchComponent->activationGraceSeconds;
        }
        else
        {
            switchComponent->activationGraceRemaining = std::max(
                0.0f,
                switchComponent->activationGraceRemaining - deltaTime);
        }
        switchComponent->isPressed = hasRequiredBatteries || switchComponent->activationGraceRemaining > 0.0f;
        const float targetPress = switchComponent->isPressed ? switchComponent->pressDepth : 0.0f;
        const float responseSpeed = switchComponent->isPressed ? switchComponent->pressSpeed : switchComponent->releaseSpeed;
        switchComponent->currentPress += (targetPress - switchComponent->currentPress) * std::min(1.0f, deltaTime * responseSpeed);
        switchComponent->currentPress = std::clamp(switchComponent->currentPress, 0.0f, switchComponent->pressDepth);
        transform->y = switchComponent->baseY + switchComponent->currentPress;
        const float switchDeltaY = transform->y - previousY;
        carryPlayerByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            switchDeltaY,
            std::max(kSwitchTopToleranceMin, switchHeight * 0.7f),
            kPlatformPlayerInsetX);
        carryBatteriesByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            switchDeltaY,
            std::max(kSwitchTopToleranceMin, switchHeight * 0.7f),
            kPlatformBatteryInsetX);

        const bool powered = switchComponent->isPressed;
        if (!switchComponent->controlsLaserPower)
        {
            setLinkPowered(switchComponent->linkId, powered);
        }

        if (switchComponent->controlsLaserPower)
        {
            setEntityTint(*entity, powered ? 1.0f : 0.40f, powered ? 0.42f : 0.44f, powered ? 0.28f : 0.50f);
        }
        else
        {
            setEntityTint(*entity, powered ? 0.22f : 0.92f, powered ? 0.90f : 0.26f, powered ? 0.40f : 0.20f);
        }
    }

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* laserSwitch = entity->GetComponent<LaserSwitchComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!laserSwitch || !transform)
        {
            continue;
        }

        const float width = transform->width * transform->scale;
        const float height = transform->height * transform->scale;
        TransformComponent switchBounds(transform->x, transform->y, width, height);
        switchBounds.scale = 1.0f;

        bool isLaserHit = false;
        for (const auto& beamEntity : m_entities)
        {
            if (!beamEntity || !HasTag(*beamEntity, kTagLaserBeam))
            {
                continue;
            }

            const auto* beamTransform = beamEntity->GetComponent<TransformComponent>();
            if (!beamTransform)
            {
                continue;
            }
            if (beamTransform->width * beamTransform->scale <= 0.5f ||
                beamTransform->height * beamTransform->scale <= 0.5f)
            {
                continue;
            }
            TransformComponent beamSense = *beamTransform;
            // Beam may terminate exactly at the switch edge from either direction; keep a tiny overlap margin.
            beamSense.x -= 2.0f;
            beamSense.y -= 2.0f;
            beamSense.width += 4.0f;
            beamSense.height += 4.0f;
            if (IntersectsRect(switchBounds, beamSense))
            {
                isLaserHit = true;
                break;
            }
        }

        laserSwitch->isOn = isLaserHit;
        setLinkPowered(laserSwitch->linkId, isLaserHit);

        setEntityTint(*entity, isLaserHit ? 1.0f : 0.92f, isLaserHit ? 0.94f : 0.82f, isLaserHit ? 0.34f : 0.22f);
    }

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* elevator = entity->GetComponent<ElevatorComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!elevator || !transform)
        {
            continue;
        }

        const float previousY = transform->y;
        const bool powered = linkPowered[elevator->linkId];
        if (!powered)
        {
            elevator->cycleStarted = false;
            elevator->pauseTimer = 0.0f;
            elevator->wasPlayerTouching = false;
            elevator->wasPowered = false;
        }
        else
        {
            const bool playerTouchingElevator =
                isPlayerTouchingPlatform(*transform, std::max(kPlatformTopToleranceMin, tileSize * 0.18f)) ||
                isPlayerOnTopOfPlatform(*transform, std::max(kPlatformTopToleranceMin + 2.0f, tileSize * 0.28f));
            const bool touchTriggeredThisFrame =
                playerTouchingElevator &&
                (!elevator->wasPlayerTouching || !elevator->wasPowered);
            if (!elevator->cycleStarted && touchTriggeredThisFrame)
            {
                elevator->cycleStarted = true;
                elevator->pauseTimer = 0.0f;
            }

            if (elevator->cycleStarted)
            {
                if (elevator->movingUp)
                {
                    const float topY = elevator->baseY - elevator->moveRangeY;
                    transform->y = std::max(topY, transform->y - elevator->moveSpeed * deltaTime);
                    if (transform->y <= topY + 0.5f)
                    {
                        transform->y = topY;
                        elevator->movingUp = false;
                        elevator->cycleStarted = false;
                    }
                }
                else
                {
                    transform->y = std::min(elevator->baseY, transform->y + elevator->moveSpeed * deltaTime);
                    if (transform->y >= elevator->baseY - 0.5f)
                    {
                        transform->y = elevator->baseY;
                        elevator->movingUp = true;
                        elevator->cycleStarted = false;
                    }
                }
            }

            elevator->wasPlayerTouching = playerTouchingElevator;
            elevator->wasPowered = true;
        }

        const float deltaY = transform->y - previousY;
        carryPlayerByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            std::max(kPlatformTopToleranceMin, tileSize * 0.24f),
            kPlatformPlayerInsetX);

        setEntityTint(*entity, powered ? 0.74f : 0.42f, powered ? 0.86f : 0.46f, powered ? 0.98f : 0.52f);
    }

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* shutter = entity->GetComponent<ShutterComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!shutter || !transform)
        {
            continue;
        }

        const bool poweredByLink = linkPowered[shutter->linkId];
        const bool linkActive = poweredByLink || (shutter->useBossDefeatSignal && shieldBossDefeated);
        const bool open = shutter->opensWhenUnpowered
            ? !linkActive
            : linkActive;
        const float previousY = transform->y;
        const float targetY = open
            ? shutter->baseY - shutter->moveRangeY
            : shutter->baseY;
        const float maxStep = shutter->moveSpeed * deltaTime;
        const float deltaToTarget = targetY - transform->y;
        if (std::fabs(deltaToTarget) <= maxStep)
        {
            transform->y = targetY;
        }
        else
        {
            transform->y += (deltaToTarget > 0.0f ? maxStep : -maxStep);
        }
        shutter->isOpen = open;

        const float deltaY = transform->y - previousY;
        if (std::fabs(deltaY) <= 0.001f)
        {
            continue;
        }

        const float topTolerance = std::max(kPlatformTopToleranceMin, tileSize * 0.24f);
        carryPlayerByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            topTolerance,
            kPlatformPlayerInsetX);
        carryBatteriesByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            topTolerance,
            kPlatformBatteryInsetX);
    }

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* wall = entity->GetComponent<ProtectiveWallComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!wall || !transform)
        {
            continue;
        }

        hasProtectiveWalls = true;
        if (!wall->IsDestroyed())
        {
            hasIntactProtectiveWall = true;
        }

        const Entity* nearestLightEntity = FindNearestMarkerLightEntity(*transform, wall->linkId, false);
        const auto* nearestLight = nearestLightEntity ? nearestLightEntity->GetComponent<MarkerLightComponent>() : nullptr;
        const bool powered = !wall->IsDestroyed() && nearestLight != nullptr && nearestLight->activated;
        wall->isOn = powered;
        const float previousY = transform->y;
        const float targetY = powered
            ? wall->baseY
            : wall->baseY + wall->moveRangeY;
        const float maxStep = wall->moveSpeed * deltaTime;
        const float deltaToTarget = targetY - transform->y;
        if (std::fabs(deltaToTarget) <= maxStep)
        {
            transform->y = targetY;
        }
        else
        {
            transform->y += (deltaToTarget > 0.0f ? maxStep : -maxStep);
        }

        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            const float durabilityRatio = static_cast<float>(wall->GetCurrentDurability()) /
                static_cast<float>((std::max)(1, wall->GetMaxDurability()));
            tint->r = wall->IsDestroyed() ? 0.16f : 0.18f + 0.18f * (1.0f - durabilityRatio);
            tint->g = wall->IsDestroyed() ? 0.18f : 0.58f + 0.20f * durabilityRatio;
            tint->b = wall->IsDestroyed() ? 0.20f : 0.52f + 0.22f * durabilityRatio;
            tint->a = wall->IsDestroyed() ? 0.0f : 1.0f;
        }

        const float deltaY = transform->y - previousY;
        if (!powered || std::fabs(deltaY) <= 0.001f)
        {
            continue;
        }

        const float topTolerance = std::max(kPlatformTopToleranceMin, tileSize * 0.24f);
        carryPlayerByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            topTolerance,
            kPlatformPlayerInsetX);
        carryBatteriesByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            topTolerance,
            kPlatformBatteryInsetX);
    }

    if (hasProtectiveWalls && !hasIntactProtectiveWall)
    {
        RefreshProtectiveWallsFromMarkers();
    }
}

const Entity* GameScene::FindNearestMarkerLightEntity(
    const TransformComponent& referenceTransform,
    int linkId,
    bool requireActivated) const
{
    const float referenceWidth = referenceTransform.width * referenceTransform.scale;
    const float referenceHeight = referenceTransform.height * referenceTransform.scale;
    const float referenceCenterX = referenceTransform.x + referenceWidth * 0.5f;
    const float referenceCenterY = referenceTransform.y + referenceHeight * 0.5f;

    const Entity* nearestEntity = nullptr;
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, kTagMarkerLight))
        {
            continue;
        }

        const auto* markerLight = entity->GetComponent<MarkerLightComponent>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!markerLight || !transform)
        {
            continue;
        }
        if (requireActivated && !markerLight->activated)
        {
            continue;
        }
        if (linkId >= 0 && markerLight->linkId != linkId)
        {
            continue;
        }

        const float lightWidth = transform->width * transform->scale;
        const float lightHeight = transform->height * transform->scale;
        const float lightCenterX = transform->x + lightWidth * 0.5f;
        const float lightCenterY = transform->y + lightHeight * 0.5f;
        const float dx = lightCenterX - referenceCenterX;
        const float dy = lightCenterY - referenceCenterY;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq < nearestDistanceSq)
        {
            nearestDistanceSq = distanceSq;
            nearestEntity = entity.get();
        }
    }

    return nearestEntity;
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
    HandleWalkerMeleeAttackCollisions(*player);
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
    RefreshStageRenderProfile();

    m_entities.clear();
    m_pendingEntities.clear();
    m_photo = PhotoState{};
    m_player = GameScenePlayerState{};
    m_flow = GameSceneFlowState{};
    m_effects = GameSceneEffectsState{};
    m_mapEditor.active = false;
    m_mapEditor.brushTarget = GameSceneMapEditorState::BrushTarget::Tile;
    m_mapEditor.selectedTileValue = 1;
    m_mapEditor.selectedMarker = 'G';
    m_mapEditor.selectedMarkerParameter = 1;
    m_mapEditor.selectedStageLightTiles = 3;
    m_mapEditor.selectedStageLightFixtureTiles = 1;
    m_mapEditor.statusMessage.clear();
    m_mapEditor.statusMessageTimer = 0.0f;
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
        game_scene_player_visual_system::ConfigurePlayerSpriteAnimation(
            *transitionedPlayer,
            m_assets.GetTexture("player_idle"),
            m_assets.GetTexture("player_move"),
            m_assets.GetTexture("player_jump"),
            m_assets.GetTexture("player_capture"),
            m_assets.GetTexture("player_paste"),
            m_assets.GetTexture("player_attack"));
        game_scene_player_visual_system::ResetSpriteAnimationToIdle(m_player, *transitionedPlayer);

        if (auto* transformed = transitionedPlayer->GetComponent<TransformComponent>())
        {
            {
                const char resolvedSpawnMarker = spawnMarker == '\0' ? '*' : spawnMarker;
                bool foundSpawnMarker = false;
                for (int spawnRow = 0; spawnRow < m_tileMap.GetHeight() && !foundSpawnMarker; ++spawnRow)
                {
                    for (int spawnColumn = 0; spawnColumn < m_tileMap.GetWidth(); ++spawnColumn)
                    {
                        const char tileMarker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(spawnColumn, spawnRow))));
                        if (tileMarker != resolvedSpawnMarker)
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

bool GameScene::RestoreSepiaBackgroundGroupInFrame(
    float frameX,
    float frameY,
    float frameWidth,
    float frameHeight)
{
    const float frameRight = frameX + frameWidth;
    const float frameBottom = frameY + frameHeight;
    const float tileSize = m_tileMap.GetTileSize();

    if (tileSize <= 0.0f)
    {
        return false;
    }

    bool restoredAny = false;
    std::vector<std::unique_ptr<Entity>> floorBlocksToAdd;

    auto createFloorBlock =
        [&](int column, int row, int restoredTileValue)
        {
            auto floorBlock = std::make_unique<Entity>();

            floorBlock->AddComponent<TagComponent>(kTagSepiaRubble);

            floorBlock->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize,
                tileSize,
                tileSize);

            floorBlock->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
            floorBlock->AddComponent<SpriteRenderComponent>(
                m_assets.GetTexture("sepia_ground"));


            floorBlock->AddComponent<ImageOutlineColliderComponent>(
                std::vector<b2Vec2>{
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                    { 1.0f, 1.0f },
                    { 0.0f, 1.0f }
            },
                0.5f);

            floorBlock->AddComponent<PhotoCopyTileValueComponent>(
                restoredTileValue);

            floorBlocksToAdd.push_back(std::move(floorBlock));
        };

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* group =
            entity->GetComponent<SepiaRubbleGroupComponent>();

        if (!group ||
            group->markerType != '<' ||
            group->isRestored)
        {
            continue;
        }

        const auto* transform =
            entity->GetComponent<TransformComponent>();

        if (!transform)
        {
            continue;
        }

        const float groupLeft = transform->x;
        const float groupTop = transform->y;
        const float groupRight =
            transform->x + transform->width * transform->scale;
        const float groupBottom =
            transform->y + transform->height * transform->scale;

        const bool inside =
            groupLeft >= frameX &&
            groupTop >= frameY &&
            groupRight <= frameRight &&
            groupBottom <= frameBottom;

        if (!inside)
        {
            continue;
        }

        group->isRestored = true;
        restoredAny = true;

        const size_t cellCount = std::min(
            group->cellColumns.size(),
            group->cellRows.size());

        for (size_t index = 0; index < cellCount; ++index)
        {
            createFloorBlock(
                group->cellColumns[index],
                group->cellRows[index],
                group->restoredTileValue);
        }
    }

    for (auto& floorBlock : floorBlocksToAdd)
    {
        m_entities.push_back(std::move(floorBlock));
    }

    return restoredAny;
}