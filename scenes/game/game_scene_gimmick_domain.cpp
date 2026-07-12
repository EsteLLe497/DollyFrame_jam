#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"

#include <cctype>
#include <limits>
#include <unordered_map>
#include <vector>

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

    bool shieldBossDefeated = m_flow.shieldBossDefeatedThisScene || m_flow.midBoss3DefeatedThisScene;
    bool hasProtectiveWalls = false;
    bool hasIntactProtectiveWall = false;
    //bool hasIntactProtectiveWall = false;
    //for (const auto& entity : m_world.Entities())
    //{
    //    if (!entity)
    //    {
    //        continue;
    //    }
    //    const auto* enemy = entity->GetComponent<EnemyComponent>();
    //    if (!enemy)
    //    {
    //        continue;
    //    }
    //    if (enemy->GetArchetype() == EnemyArchetype::ShieldBoss && enemy->IsDefeated())
    //    {
    //        shieldBossDefeated = true;
    //        break;
    //    }
    //}

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

    auto isEnemyOnTopOfPlatform = [&](const TransformComponent& platform, float topTolerance) -> bool
    {
        const float platformWidth = platform.width * platform.scale;
        const float platformLeft = platform.x;
        const float platformRight = platform.x + platformWidth;
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            const auto* enemy = entity->GetComponent<EnemyComponent>();
            const auto* enemyTransform = entity->GetComponent<TransformComponent>();
            if (!enemy || !enemyTransform ||
                !enemy->IsEnabled() ||
                enemy->IsDefeated())
            {
                continue;
            }

            const float enemyWidth = enemyTransform->width * enemyTransform->scale;
            const float enemyHeight = enemyTransform->height * enemyTransform->scale;
            const float enemyLeft = enemyTransform->x + kPlatformPlayerInsetX;
            const float enemyRight = enemyTransform->x + enemyWidth - kPlatformPlayerInsetX;
            const float enemyBottom = enemyTransform->y + enemyHeight;
            const bool overlapX = enemyRight > platformLeft && enemyLeft < platformRight;
            if (!overlapX)
            {
                continue;
            }

            if (std::fabs(enemyBottom - platform.y) <= topTolerance)
            {
                return true;
            }
        }

        return false;
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

        for (const auto& batteryEntity : m_world.Entities())
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

    for (const auto& entity : m_world.Entities())
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
        if (switchComponent->pressMode == SwitchPressMode::Battery)
        {
            for (const auto& batteryEntity : m_world.Entities())
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
        }

        // 個数判定が一瞬途切れてもチラつかないよう、短い保持時間を設ける。
        switchComponent->insertedBatteryCount = batteriesOnTop;
        const float switchTopTolerance = std::max(kSwitchTopToleranceMin, switchHeight * 0.7f);
        const bool pressCondition =
            switchComponent->pressMode == SwitchPressMode::Player
            ? (isPlayerOnTopOfPlatform(*transform, switchTopTolerance) ||
                isEnemyOnTopOfPlatform(*transform, switchTopTolerance))
            : switchComponent->insertedBatteryCount >= switchComponent->requiredBatteryCount;
        if (pressCondition)
        {
            switchComponent->activationGraceRemaining = switchComponent->activationGraceSeconds;
        }
        else
        {
            switchComponent->activationGraceRemaining = std::max(
                0.0f,
                switchComponent->activationGraceRemaining - deltaTime);
        }
        switchComponent->isPressed = pressCondition || switchComponent->activationGraceRemaining > 0.0f;
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
        else if (switchComponent->pressMode == SwitchPressMode::Player)
        {
            setEntityTint(*entity, powered ? 1.0f : 0.40f, powered ? 0.42f : 0.44f, powered ? 0.28f : 0.50f);
        }
        else
        {
            setEntityTint(*entity, powered ? 0.22f : 0.92f, powered ? 0.90f : 0.26f, powered ? 0.40f : 0.20f);
        }
    }

    for (const auto& entity : m_world.Entities())
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
        for (const auto& beamEntity : m_world.Entities())
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

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        if (auto* gear = entity->GetComponent<GearComponent>())
        {
            gear->inserted = false;
        }
    }

    constexpr float kGearSocketAssistDistance = 25.0f;
    constexpr float kGearSocketMinSizeRatio = 0.90f;
    constexpr float kGearSocketMaxSizeRatio = 1.10f;
    constexpr float kGearSocketRotationSpeed = 3.5f;
    std::unordered_map<int, int> gearSocketActiveCounts;
    std::unordered_map<int, int> gearSocketRequiredCounts;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* socket = entity->GetComponent<GearSocketComponent>();
        auto* socketTransform = entity->GetComponent<TransformComponent>();
        if (!socket || !socketTransform)
        {
            continue;
        }

        socket->insertedGearCount = 0;
        const float socketWidth = socketTransform->width * socketTransform->scale;
        const float socketHeight = socketTransform->height * socketTransform->scale;
        const float socketCenterX = socketTransform->x + socketWidth * 0.5f;
        const float socketCenterY = socketTransform->y + socketHeight * 0.5f;
        for (const auto& gearEntity : m_world.Entities())
        {
            if (!gearEntity)
            {
                continue;
            }

            auto* gear = gearEntity->GetComponent<GearComponent>();
            auto* gearTransform = gearEntity->GetComponent<TransformComponent>();
            if (!gear ||
                !gearTransform ||
                !gear->functional ||
                gear->inserted ||
                gear->gearNo != socket->gearNo)
            {
                continue;
            }

            const float gearWidth = gearTransform->width * gearTransform->scale;
            const float gearHeight = gearTransform->height * gearTransform->scale;
            if (gearWidth < socketWidth * kGearSocketMinSizeRatio ||
                gearWidth > socketWidth * kGearSocketMaxSizeRatio ||
                gearHeight < socketHeight * kGearSocketMinSizeRatio ||
                gearHeight > socketHeight * kGearSocketMaxSizeRatio)
            {
                continue;
            }

            const float gearCenterX = gearTransform->x + gearWidth * 0.5f;
            const float gearCenterY = gearTransform->y + gearHeight * 0.5f;
            const float dx = gearCenterX - socketCenterX;
            const float dy = gearCenterY - socketCenterY;
            if (dx * dx + dy * dy > kGearSocketAssistDistance * kGearSocketAssistDistance)
            {
                continue;
            }

            gearTransform->x = socketCenterX - gearWidth * 0.5f;
            gearTransform->y = socketCenterY - gearHeight * 0.5f;
            gear->inserted = true;
            ++socket->insertedGearCount;
        }

        socket->active = socket->insertedGearCount > 0;
        if (socket->linkId >= 0)
        {
            int& requiredCount = gearSocketRequiredCounts[socket->linkId];
            requiredCount = (std::max)(requiredCount, socket->requiredGearCount);
            if (socket->active)
            {
                ++gearSocketActiveCounts[socket->linkId];
            }
        }

        if (socket->active)
        {
            socket->rotation += deltaTime * kGearSocketRotationSpeed;
            socketTransform->rotation = socket->rotation;
            setEntityTint(*entity, 0.72f, 0.72f, 0.72f);
        }
        else
        {
            socketTransform->rotation = socket->rotation;
            setEntityTint(*entity, 0.45f, 0.45f, 0.45f);
        }
    }

    for (const auto& [linkId, requiredCount] : gearSocketRequiredCounts)
    {
        const auto activeIt = gearSocketActiveCounts.find(linkId);
        const int activeCount = activeIt != gearSocketActiveCounts.end()
            ? activeIt->second
            : 0;
        setLinkPowered(linkId, activeCount >= requiredCount);
    }

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* gear = entity->GetComponent<GearComponent>();
        auto* gearTransform = entity->GetComponent<TransformComponent>();
        if (!gear || !gearTransform || !gear->inserted)
        {
            continue;
        }

        for (const auto& socketEntity : m_world.Entities())
        {
            if (!socketEntity)
            {
                continue;
            }

            const auto* socket = socketEntity->GetComponent<GearSocketComponent>();
            const auto* socketTransform = socketEntity->GetComponent<TransformComponent>();
            if (!socket ||
                !socketTransform ||
                !socket->active ||
                socket->gearNo != gear->gearNo)
            {
                continue;
            }

            const float gearWidth = gearTransform->width * gearTransform->scale;
            const float gearHeight = gearTransform->height * gearTransform->scale;
            const float gearCenterX = gearTransform->x + gearWidth * 0.5f;
            const float gearCenterY = gearTransform->y + gearHeight * 0.5f;
            const float socketWidth = socketTransform->width * socketTransform->scale;
            const float socketHeight = socketTransform->height * socketTransform->scale;
            const float socketCenterX = socketTransform->x + socketWidth * 0.5f;
            const float socketCenterY = socketTransform->y + socketHeight * 0.5f;
            const float dx = gearCenterX - socketCenterX;
            const float dy = gearCenterY - socketCenterY;
            if (dx * dx + dy * dy <= kGearSocketAssistDistance * kGearSocketAssistDistance)
            {
                gearTransform->rotation = socketTransform->rotation;
                break;
            }
        }
    }

    for (const auto& entity : m_world.Entities())
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
        const int elevatorTexture = powered
            ? m_assets.GetTexture("tile_value_elevator_on")
            : m_assets.GetTexture("tile_value_elevator_off");
        if (auto* sprite = entity->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetTextureId(elevatorTexture >= 0 ? elevatorTexture : m_whiteTexture);
        }
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
                if (elevator->pauseTimer > 0.0f)
                {
                    // 上端・下端の到着後は、指定時間だけ停止してから反転移動する。
                    elevator->pauseTimer = std::max(0.0f, elevator->pauseTimer - deltaTime);
                }
                else if (elevator->movingUp)
                {
                    const float topY = elevator->baseY - elevator->moveRangeY;
                    transform->y = std::max(topY, transform->y - elevator->moveSpeed * deltaTime);
                    if (transform->y <= topY + 0.5f)
                    {
                        transform->y = topY;
                        elevator->movingUp = false;
                        elevator->pauseTimer = elevator->endpointPauseSeconds;
                    }
                }
                else
                {
                    transform->y = std::min(elevator->baseY, transform->y + elevator->moveSpeed * deltaTime);
                    if (transform->y >= elevator->baseY - 0.5f)
                    {
                        transform->y = elevator->baseY;
                        elevator->movingUp = true;
                        elevator->pauseTimer = elevator->endpointPauseSeconds;
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

        setEntityTint(*entity, 1.0f, 1.0f, 1.0f);
    }

    for (const auto& entity : m_world.Entities())
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
        const float targetX = open
            ? shutter->baseX + shutter->moveRangeX
            : shutter->baseX;
        const float targetY = open
            ? shutter->baseY + shutter->moveRangeY
            : shutter->baseY;
        const float maxStep = shutter->moveSpeed * deltaTime;
        const float deltaToTargetX = targetX - transform->x;
        if (std::fabs(deltaToTargetX) <= maxStep)
        {
            transform->x = targetX;
        }
        else
        {
            transform->x += (deltaToTargetX > 0.0f ? maxStep : -maxStep);
        }
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

    for (const auto& entity : m_world.Entities())
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
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            if (auto* markerLight = entity->GetComponent<MarkerLightComponent>())
            {
                markerLight->activated = false;
            }
        }
        RefreshProtectiveWallsFromMarkers();
    }

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* sepiaElevator = entity->GetComponent<SepiaElevatorComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!sepiaElevator || !transform)
        {
            continue;
        }

        const float previousY = transform->y;
     
        const bool playerTouchingElevator =
            isPlayerOnTopOfPlatform(*transform, std::max(kPlatformTopToleranceMin + 2.0f, tileSize * 0.28f));
        const bool touchTriggeredThisFrame =
            playerTouchingElevator &&
            !sepiaElevator->wasPlayerTouching;
        if (!sepiaElevator->cycleStarted && touchTriggeredThisFrame)
        {
            sepiaElevator->cycleStarted = true;
            sepiaElevator->pauseTimer = 0.0f;
        }

        if (sepiaElevator->cycleStarted)
        {
            if (sepiaElevator->movingUp)
            {
                const float topY = sepiaElevator->baseY - sepiaElevator->moveRangeY;
                transform->y = std::max(topY, transform->y - sepiaElevator->moveSpeed * deltaTime);
                if (transform->y <= topY + 0.5f)
                {
                    transform->y = topY;
                    sepiaElevator->movingUp = false;
                    sepiaElevator->cycleStarted = false;
                }
            }
            else
            {
                transform->y = std::min(sepiaElevator->baseY, transform->y + sepiaElevator->moveSpeed * deltaTime);
                if (transform->y >= sepiaElevator->baseY - 0.5f)
                {
                    transform->y = sepiaElevator->baseY;
                    sepiaElevator->movingUp = true;
                    sepiaElevator->cycleStarted = false;
                }
            }
        }

        sepiaElevator->wasPlayerTouching = playerTouchingElevator;
        const float deltaY = transform->y - previousY;
        carryPlayerByPlatformDeltaY(
            *transform,
            previousY,
            transform->y,
            deltaY,
            std::max(kPlatformTopToleranceMin, tileSize * 0.24f),
            kPlatformPlayerInsetX);
    }

    std::vector<std::unique_ptr<Entity>> generatedBatteries;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* batteryGenerator = entity->GetComponent<BatteryGeneratorComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!batteryGenerator || !transform)
        {
            continue;
        }

        batteryGenerator->cooldownRemaining = std::max(
            0.0f,
            batteryGenerator->cooldownRemaining - deltaTime);

        const int batteryTexture = m_assets.GetTexture("tile_value_battery");
		const bool powered = linkPowered[batteryGenerator->linkId];
        if (powered && batteryGenerator->cooldownRemaining <= 0.0f && tileSize > 0.0f)
		{
            const float generatorWidth = transform->width * transform->scale;
            const float spawnX = batteryGenerator->spawnDirectionX < 0
                ? transform->x - tileSize - 10.0f
                : transform->x + generatorWidth + 10.0f;
            const float spawnY = transform->y + tileSize;

            auto battery = std::make_unique<Entity>();
            battery->AddComponent<TagComponent>(kTagBattery);
            battery->AddComponent<TransformComponent>(
                spawnX,
                spawnY,
                tileSize,
                tileSize);
            battery->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
            battery->AddComponent<SpriteRenderComponent>(batteryTexture);
            battery->AddComponent<BatteryComponent>(
                1900.0f,
                980.0f,
                260.0f,
                320.0f,
                1);
            generatedBatteries.push_back(std::move(battery));
            batteryGenerator->cooldownRemaining = batteryGenerator->cooldownSeconds;
        }

        batteryGenerator->wasPowered = powered;
    }

    for (auto& battery : generatedBatteries)
    {
        m_world.Spawn(std::move(battery));
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
    for (const auto& entity : m_world.Entities())
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
    HandleWalkerMeleeAttackCollisions();
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
        m_world.Entities(),
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
        m_world.Entities(),
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
    m_world.RemoveByPointerList(entitiesToRemove);

    if (!defeatedEnemies.empty())
    {
        m_eventBus.Publish({ EventType::LogMessage, player, defeatedEnemies.front(), "Invert photo neutralized an enemy", 0.0f, 0.0f });
    }
}

bool GameScene::TryQueueStageTransition(Entity& player)
{
    if (m_flow.stageTransitionActive || m_lifecycle.hasPendingStageTransition)
    {
        return false;
    }

    if (gStageTransitionLinks.empty())
    {
        m_lifecycle.lastStageTransitionMarker = '\0';
        return false;
    }

    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        m_lifecycle.lastStageTransitionMarker = '\0';
        return false;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        m_lifecycle.lastStageTransitionMarker = '\0';
        return false;
    }

    const float centerX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
    const float centerY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
    const int column = static_cast<int>(centerX / tileSize);
    const int row = static_cast<int>(centerY / tileSize);
    const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
    if (marker == '\0')
    {
        m_lifecycle.lastStageTransitionMarker = '\0';
        return false;
    }

    if (marker == m_lifecycle.lastStageTransitionMarker)
    {
        return false;
    }

    const StageTransitionLink* matchedLink = nullptr;
    for (const StageTransitionLink& link : gStageTransitionLinks)
    {
        const bool sourceMatches =
            link.sourceMapCsv == "*" ||
            link.sourceMapCsv == m_lifecycle.currentMapCsvPath;
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

    m_lifecycle.hasPendingStageTransition = true;
    m_lifecycle.pendingStageTransitionMapCsv = matchedLink->destinationMapCsv;
    m_lifecycle.pendingStageTransitionSpawnMarker = matchedLink->spawnMarker;
    m_lifecycle.pendingStageTransitionMarker = marker;
    m_lifecycle.lastStageTransitionMarker = marker;
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
    const GameSceneUiTuningState uiTuning = m_ui.tuning;
    const float captureFinderScale = m_ui.captureFinderScale;
    const bool cameraFlashEnabled = m_ui.cameraFlash.enabled;
    m_lifecycle.currentMapCsvPath = destinationMapCsv;
    m_lifecycle.lastStageTransitionMarker = marker;
    RefreshStageRenderProfile();

    m_world.Clear();
    m_photo = PhotoState{};
    m_player = GameScenePlayerState{};
    m_flow = GameSceneFlowState{};
    m_ui = GameSceneUiState{};
    m_ui.tuning = uiTuning;
    m_ui.captureFinderScale = captureFinderScale;
    m_ui.cameraFlash.enabled = cameraFlashEnabled;
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
    m_camera.transitionMarkers.clear();

    m_camera.hasPreviousPlayerCameraProbe = false;
    m_camera.previousPlayerCameraProbeX = 0.0f;
    m_camera.previousPlayerCameraProbeY = 0.0f;
    m_camera.cameraYRecenteringStrength = 0.0f;
    m_camera.floorCameraTransitionActive = false;
    m_camera.floorCameraTransitionElapsed = 0.0f;
    m_camera.floorCameraTransitionStartX = 0.0f;
    m_camera.floorCameraTransitionStartY = 0.0f;
    m_camera.floorCameraTransitionTargetX = 0.0f;
    m_camera.floorCameraTransitionTargetY = 0.0f;
    m_camera.cameraFixedLockActive = false;
    m_camera.cameraFixedLockStartX = 0.0f;
    m_camera.cameraFixedLockEndX = 0.0f;
    m_camera.cameraFixedLockX = 0.0f;
    m_camera.cameraFixedLockY = 0.0f;
    m_camera.midBoss3CameraYLockInitialized = false;
    m_camera.midBoss3CameraYLock = 0.0f;
    m_lifecycle.hasPendingStageTransition = false;
    m_lifecycle.pendingStageTransitionMapCsv.clear();
    m_lifecycle.pendingStageTransitionSpawnMarker = '\0';
    m_lifecycle.pendingStageTransitionMarker = '\0';
    m_lifecycle.bossBgmCrossFadeStarted = false;
    m_flow.timeLimit = session.timeLimit;
    m_flow.timeRemaining = session.timeRemaining;

    m_eventBus.Clear();
    m_physicsWorld.Shutdown();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
    if (!m_tileMap.LoadFromCsv(m_lifecycle.currentMapCsvPath, tileSize))
    {
        return false;
    }
    RefreshTileTextureForCurrentMap();

    InitializeStageEntities();
    PlayStageBgmForCurrentMap();

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
    m_eventBus.Publish({ EventType::LogMessage, currentPlayer, nullptr, std::string("Stage transition: ") + m_lifecycle.currentMapCsvPath, 0.0f, 0.0f });
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
    for (const auto& entity : m_world.Entities())
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
    for (const auto& entity : m_world.Entities())
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
            for (const auto& enemyEntity : m_world.Entities())
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
    m_world.RemoveByPointerList(entitiesToRemove);
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
    m_flow.resultTransitionSceneRequested = false;
    m_flow.resultTransitionTimer = 0.0f;
    GameSession_SetEndReason(reason);
    GameSession_SetLastMapCsvPath(m_lifecycle.currentMapCsvPath);
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });
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
