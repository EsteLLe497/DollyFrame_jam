#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_world_interaction_system.h"

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

    m_player.velocityX = 0.0f;
    m_player.velocityY = 0.0f;
    m_player.grounded = false;
    m_player.coyoteTimeRemaining = 0.0f;
    m_player.dodgeRemaining = 0.0f;
    m_player.dodgeCooldownRemaining = 0.0f;
    m_player.afterimages.clear();
    m_player.visualOffsetY = 0.0f;
    m_player.visualRotation = 0.0f;
    m_player.visualScaleX = 1.0f;
    m_player.visualScaleY = 1.0f;
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
    m_hasPreviousPlayerCameraProbe = false;
    m_floorCameraTransitionActive = false;
    m_floorCameraTransitionElapsed = 0.0f;
    m_cameraFixedLockActive = false;
    m_cameraFixedLockStartX = 0.0f;
    m_cameraFixedLockEndX = 0.0f;
    m_cameraFixedLockX = 0.0f;
    m_cameraFixedLockY = 0.0f;
    for (CameraTransitionMarker& marker : m_cameraTransitionMarkers)
    {
        marker.wasInside = false;
    }

    for (const auto& entity : m_entities)
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

    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float targetCameraX = transform->x - (gCameraViewWidth - playerWidth) * 0.5f;
    m_flow.cameraX = std::clamp(targetCameraX, 0.0f, std::max(0.0f, mapWidth - gCameraViewWidth));
    const bool followY = gCameraFollowY >= 0.5f;
    if (followY)
    {
        const float targetCameraY = transform->y - (gCameraViewHeight - playerHeight) * 0.5f;
        m_flow.cameraY = std::clamp(targetCameraY, 0.0f, std::max(0.0f, mapHeight - gCameraViewHeight));
    }
    else
    {
        m_flow.cameraY = 0.0f;
    }
}
