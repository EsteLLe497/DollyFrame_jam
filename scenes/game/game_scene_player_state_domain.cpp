#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"
#include "game_scene_player_state_logic.h"

#include <algorithm>

using namespace game_scene_detail;

namespace
{
    constexpr float kPitRestartFadeDuration = 0.45f;
    constexpr float kRespawnInvulnerabilitySeconds = 0.6f;
    constexpr float kPlayerDamageHitStopSeconds = 0.06f;
    constexpr float kPlayerDamageShakeSeconds = 0.22f;
    constexpr float kPlayerDamageShakeAmplitude = 18.0f;

    void TriggerPlayerDamageFeedback(GameSceneFlowState& flow, bool screenShakeEnabled)
    {
        flow.hitStopRemaining = (std::max)(flow.hitStopRemaining, kPlayerDamageHitStopSeconds);
        if (screenShakeEnabled)
        {
            flow.screenShakeRemaining = kPlayerDamageShakeSeconds;
            flow.screenShakeDuration = kPlayerDamageShakeSeconds;
            flow.screenShakeAmplitude = kPlayerDamageShakeAmplitude;
        }
        else
        {
            flow.screenShakeRemaining = 0.0f;
            flow.screenShakeDuration = 0.0f;
            flow.screenShakeAmplitude = 0.0f;
        }
    }
}

void GameScene::ApplyHazardDamageToPlayer(Entity& player, Entity* sourceEntity, const char* logMessage, int amount)
{
    m_flow.playerTouchingHazard = true;
    HandlePlayerDamage(player, sourceEntity, logMessage, amount);
}

void GameScene::HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage, int amount)
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

    if (!m_debug.playerHealthDamageEnabled)
    {
        return;
    }

    auto* cooldown = player.GetComponent<DamageCooldownComponent>();
    if (cooldown && !cooldown->CanTakeDamage())
    {
        return;
    }

    const bool shouldTriggerDamageFeedback = health->GetCurrentHealth() > (std::max)(1, amount);

    if (cooldown)
    {
        cooldown->Trigger();
    }

    health->ApplyDamage((std::max)(1, amount));
    if (shouldTriggerDamageFeedback)
    {
        TriggerPlayerDamageFeedback(m_flow, m_debug.screenShakeEnabled);
    }

    GameSession_SetCurrentHp(health->GetCurrentHealth());
    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &player, sourceEntity, logMessage, 0.0f, 0.0f });
    if (health->IsDead() && !m_flow.resultQueued)
    {
        StartPitRestart(&player, "GameScene player was defeated");
    }

    if (health->IsDead())
    {
        for (Entity* entity : m_world.EntitiesByTag(EntityTag::SepiaRubble))
        {
            if (!entity)
            {
                continue;
            }
            auto* sepiaGroup = entity->GetComponent<SepiaRubbleGroupComponent>();
            if (!sepiaGroup || !sepiaGroup->isRestored)
            {
                continue;
            }
            // Clear tiles and restore marker
            for (int col = sepiaGroup->minColumn; col <= sepiaGroup->maxColumn; ++col)
            {
                for (int row = sepiaGroup->minRow; row <= sepiaGroup->maxRow; ++row)
                {
                    m_tileMap.SetTile(col, row, 0);
                    m_tileMap.SetMarker(col, row, '<', 0);
                }
            }
            sepiaGroup->isRestored = false;
            sepiaGroup->restoredLifetime = 0.0f;
        }
    }
}

void GameScene::RespawnPlayer(Entity& player)
{
    auto* transform = player.GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    const float spawnX = m_flow.hasCheckpoint ? m_flow.respawnX : m_flow.stageStartX;
    const float spawnY = m_flow.hasCheckpoint ? m_flow.respawnY : m_flow.stageStartY;
    transform->x = spawnX;
    transform->y = spawnY;
    bool removedPastedBattery = false;
    m_world.EraseIf(
        [&removedPastedBattery](const std::unique_ptr<Entity>& entity)
        {
            if (!entity ||
                !HasTag(*entity, kTagBattery) ||
                !entity->GetComponent<BatteryComponent>() ||
                !entity->GetComponent<PhotoCopyGroupComponent>() ||
                !entity->GetComponent<PhotoPasteOrderComponent>())
            {
                return false;
            }

            removedPastedBattery = true;
            return true;
        });
    if (removedPastedBattery)
    {
        RefreshPhotoGroupState();
        constexpr float kSwitchTopToleranceMin = 8.0f;
        constexpr float kPlatformBatteryInsetX = 2.0f;
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            auto* switchComponent = entity->GetComponent<BatterySwitchComponent>();
            auto* switchTransform = entity->GetComponent<TransformComponent>();
            if (!switchComponent ||
                !switchTransform ||
                !switchComponent->controlsLaserPower ||
                switchComponent->pressMode != SwitchPressMode::Battery)
            {
                continue;
            }

            const float switchWidth = switchTransform->width * switchTransform->scale;
            const float switchHeight = switchTransform->height * switchTransform->scale;
            const float batteryTopTolerance = std::max(kSwitchTopToleranceMin, switchHeight * 0.7f);
            int batteriesOnTop = 0;
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
                const float batteryLeft = batteryTransform->x + kPlatformBatteryInsetX;
                const float batteryRight = batteryTransform->x + batteryWidth - kPlatformBatteryInsetX;
                const float batteryBottom = batteryTransform->y + batteryHeight;
                const float switchLeft = switchTransform->x;
                const float switchRight = switchTransform->x + switchWidth;
                const bool overlapX = batteryRight > switchLeft && batteryLeft < switchRight;
                const bool onTop = batteryBottom >= switchTransform->y - batteryTopTolerance &&
                    batteryBottom <= switchTransform->y + batteryTopTolerance;
                if (overlapX && onTop)
                {
                    ++batteriesOnTop;
                }
            }

            switchComponent->insertedBatteryCount = batteriesOnTop;
            const bool pressed = batteriesOnTop >= switchComponent->requiredBatteryCount;
            switchComponent->isPressed = pressed;
            switchComponent->activationGraceRemaining = pressed ? switchComponent->activationGraceSeconds : 0.0f;
            if (!pressed)
            {
                switchComponent->currentPress = 0.0f;
                switchTransform->y = switchComponent->baseY;
            }
        }
    }
    ResetHangingGravityObjectsForRespawn();

    game_scene_player_state_logic::ResetPlayerStateAfterRespawn(m_player);
    game_scene_player_visual_system::ResetSpriteAnimationToIdle(m_player, player);

    if (auto* health = player.GetComponent<HealthComponent>())
    {
        health->RestoreToFull();
        GameSession_SetCurrentHp(health->GetCurrentHealth());
    }

    if (auto* cooldown = player.GetComponent<DamageCooldownComponent>())
    {
        cooldown->SetRemainingSeconds(kRespawnInvulnerabilitySeconds);
    }

    m_flow.cameraMode = false;
    m_flow.captureSlowRemaining = 0.0f;
    m_flow.placementSlowRemaining = 0.0f;
    m_photo.placement.active = false;
    m_flow.playerTouchingHazard = false;
    m_flow.playerTouchingTarget = false;
    m_flow.pitRestartActive = false;
    m_flow.pitRestartTimer = 0.0f;
    m_flow.pitRestartFadeInTimer = kPitRestartFadeDuration;
    m_camera.hasPreviousPlayerCameraProbe = false;
    m_camera.floorCameraTransitionActive = false;
    m_camera.floorCameraTransitionElapsed = 0.0f;
    m_camera.cameraFixedLockActive = false;
    m_camera.cameraFixedLockStartX = 0.0f;
    m_camera.cameraFixedLockEndX = 0.0f;
    m_camera.cameraFixedLockX = 0.0f;
    m_camera.cameraFixedLockY = 0.0f;
    for (CameraTransitionMarker& marker : m_camera.transitionMarkers)
    {
        marker.wasInside = false;
    }

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        auto* enemyTransform = entity->GetComponent<TransformComponent>();
        if (!enemy || !enemyTransform || enemy->GetArchetype() != EnemyArchetype::Ghost)
        {
            continue;
        }

        enemyTransform->x = enemy->spawnX;
        enemyTransform->y = enemy->spawnY;
        enemy->velocityY = 0.0f;
        enemy->attackTimer = 0.0f;
        enemy->attackRectActive = false;
        enemy->attackRectRemaining = 0.0f;
        enemy->SetAIState(EnemyComponent::AIState::Idle);
        enemy->Restore();

        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            tint->a = 1.0f;
        }
        if (auto* ghost = entity->GetComponent<GhostComponent>())
        {
            ghost->targetAlpha = ghost->visibilityAlpha;
        }
    }

    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        auto* boss = entity->GetComponent<MidBoss3Component>();
        auto* bossTransform = entity->GetComponent<TransformComponent>();
        if (!enemy ||
            !boss ||
            !bossTransform ||
            enemy->GetArchetype() != EnemyArchetype::MidBoss3 ||
            enemy->IsDefeated())
        {
            continue;
        }

        const float centerX = boss->initializedArena ? boss->arenaCenterX : bossTransform->x;
        const float centerY = boss->initializedArena ? boss->arenaCenterY : bossTransform->y;
        boss->homeX = centerX;
        boss->homeY = centerY;
        boss->moveStartX = centerX;
        boss->moveStartY = centerY;
        boss->moveTargetX = centerX;
        boss->moveTargetY = centerY;
        bossTransform->x = centerX;
        bossTransform->y = centerY;
        boss->state = MidBoss3State::Move;
        boss->stateTimer = 0.0f;
        boss->moveTimer = 0.0f;
        boss->moveStep = 0;
        boss->movePattern = 0;
        boss->moveSide = -1;
        boss->nextFlowAttack = 2;
        boss->lastFlowMoveSide = -1;
        boss->launcherShotsFired = 0;
        boss->meteorShotsFired = 0;
        boss->cooldownAttack = 0;
        boss->launcherShotTimer = 0.0f;
        boss->moving = false;
        boss->reloadActive = false;
        boss->reloadStartedForMove = false;
        boss->reloadStartMoveStep = -1;
        boss->reloadTimer = 0.0f;
        boss->flowStarted = false;
        boss->chooseMoveSideFromStageCenter = true;
        boss->launcherPrepared = false;
        boss->drillActive = false;
        boss->drillFormed = false;
        boss->drillGroundRush = false;
        boss->drillDamageApplied = false;
        boss->drillFloorObjectHits = 0;
        boss->drillVelocityX = 0.0f;
        boss->drillVelocityY = 0.0f;
        boss->damageMotionRequested = false;
        boss->damageMotionRemaining = 0.0f;
        boss->damageMotionOffsetX = 0.0f;
        boss->damageMotionOffsetY = 0.0f;

        const float bossWidth = bossTransform->width * bossTransform->scale;
        for (Entity* fistEntity : boss->fistEntities)
        {
            auto* fist = fistEntity ? fistEntity->GetComponent<MidBoss3FistComponent>() : nullptr;
            auto* fistTransform = fistEntity ? fistEntity->GetComponent<TransformComponent>() : nullptr;
            if (!fist || !fistTransform)
            {
                continue;
            }

            const float fistWidth = fistTransform->width * fistTransform->scale;
            const float dockX = boss->facingRight
                ? centerX + bossWidth - fist->baseOffsetX - fistWidth
                : centerX + fist->baseOffsetX;
            const float dockY = centerY + fist->baseOffsetY;
            fist->state = MidBoss3FistState::Docked;
            fist->velocityX = 0.0f;
            fist->velocityY = 0.0f;
            fist->launchTimer = 0.0f;
            fist->attackReadyTimer = 0.0f;
            fist->damageApplied = false;
            fist->atAttackStart = false;
            fist->captureJammerActive = false;
            fist->broken = false;
            fist->impactAttackActive = false;
            fist->impactDamageApplied = false;
            fist->impactAttackRemaining = 0.0f;
            fistTransform->x = dockX;
            fistTransform->y = dockY;
            fistTransform->rotation = 0.0f;
            if (auto* tint = fistEntity->GetComponent<TintComponent>())
            {
                tint->a = 1.0f;
            }
        }
    }

    const GameScenePlayerRespawnContext cameraContext
    {
        transform->x,
        transform->y,
        transform->width * transform->scale,
        transform->height * transform->scale,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        gCameraViewWidth,
        gCameraViewHeight,
        gCameraFollowY >= 0.5f,
    };
    const GameScenePlayerRespawnResult cameraResult =
        game_scene_player_state_logic::ComputeRespawnCamera(cameraContext);
    m_flow.cameraX = cameraResult.cameraX;
    m_flow.cameraY = cameraResult.cameraY;
}
