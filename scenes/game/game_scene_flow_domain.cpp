#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>

using namespace game_scene_detail;

namespace
{
    constexpr float kStageTransitionFadeInDuration = 1.10f;
}

bool GameScene::UpdatePitRestartFlow(float deltaTime)
{
    if (!m_flow.pitRestartActive)
    {
        return false;
    }

    m_flow.pitRestartTimer = std::max(0.0f, m_flow.pitRestartTimer - deltaTime);
    if (m_flow.pitRestartTimer > 0.0f)
    {
        return true;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    if (player)
    {
        RespawnPlayer(*player);
    }
    else
    {
        m_flow.pitRestartActive = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    return true;
}

bool GameScene::UpdateStageTransitionFlow(float deltaTime)
{
    if (!m_flow.stageTransitionActive)
    {
        return false;
    }

    m_flow.stageTransitionTimer = std::max(0.0f, m_flow.stageTransitionTimer - deltaTime);
    if (m_flow.stageTransitionTimer > 0.0f)
    {
        return true;
    }

    const bool transitioned = m_hasPendingStageTransition &&
        ExecuteStageTransition(
            m_pendingStageTransitionMapCsv,
            m_pendingStageTransitionSpawnMarker,
            m_pendingStageTransitionMarker);
    m_hasPendingStageTransition = false;
    m_pendingStageTransitionMapCsv.clear();
    m_pendingStageTransitionSpawnMarker = '\0';
    m_pendingStageTransitionMarker = '\0';
    m_flow.stageTransitionActive = false;
    m_flow.stageTransitionTimer = 0.0f;
    m_flow.stageTransitionFadeInTimer = transitioned ? kStageTransitionFadeInDuration : 0.0f;
    return true;
}

void GameScene::UpdateFrameTimers(float deltaTime, float gameplayDeltaTime, float effectiveGameplayDeltaTime)
{
    m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - effectiveGameplayDeltaTime);
    m_flow.shutterFlashRemaining = std::max(0.0f, m_flow.shutterFlashRemaining - deltaTime);
    m_flow.cameraFlash.pulseRemaining = std::max(0.0f, m_flow.cameraFlash.pulseRemaining - deltaTime);
    m_flow.pitRestartFadeInTimer = std::max(0.0f, m_flow.pitRestartFadeInTimer - deltaTime);
    m_flow.stageTransitionFadeInTimer = std::max(0.0f, m_flow.stageTransitionFadeInTimer - deltaTime);
    const bool previewWasActive = m_flow.developedPhotoPreviewRemaining > 0.0f;
    m_flow.developedPhotoPreviewRemaining = std::max(0.0f, m_flow.developedPhotoPreviewRemaining - deltaTime);
    if (previewWasActive && m_flow.developedPhotoPreviewRemaining <= 0.0f)
    {
        CommitPendingCapturedPhoto();
    }
    m_flow.pickupPulse += gameplayDeltaTime;

    // HPバー演出の更新: 実HPとは別に表示用比率を補間する。
    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* health = player->GetComponent<HealthComponent>())
        {
            const int maxHp = (std::max)(1, health->GetMaxHealth());
            const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
            const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);

            if (!m_flow.hpUiInitialized)
            {
                m_flow.hpDisplayRatio = targetRatio;
                m_flow.hpDamageLagRatio = targetRatio;
                m_flow.hpDamageFlash = 0.0f;
                m_flow.hpLastRaw = currentHp;
                m_flow.hpUiInitialized = true;
            }
            else
            {
                if (m_flow.hpLastRaw >= 0 && currentHp < m_flow.hpLastRaw)
                {
                    m_flow.hpDamageFlash = 1.0f;
                }
                m_flow.hpLastRaw = currentHp;

                const float displaySpeed = targetRatio < m_flow.hpDisplayRatio ? 10.0f : 14.0f;
                m_flow.hpDisplayRatio += (targetRatio - m_flow.hpDisplayRatio) * std::min(1.0f, deltaTime * displaySpeed);
                m_flow.hpDisplayRatio = std::clamp(m_flow.hpDisplayRatio, 0.0f, 1.0f);

                if (m_flow.hpDamageLagRatio < m_flow.hpDisplayRatio)
                {
                    m_flow.hpDamageLagRatio = m_flow.hpDisplayRatio;
                }
                else
                {
                    const float lagSpeed = 2.4f;
                    m_flow.hpDamageLagRatio += (m_flow.hpDisplayRatio - m_flow.hpDamageLagRatio) * std::min(1.0f, deltaTime * lagSpeed);
                    m_flow.hpDamageLagRatio = std::clamp(m_flow.hpDamageLagRatio, 0.0f, 1.0f);
                }
            }
        }
    }

    m_flow.hpDamageFlash = std::max(0.0f, m_flow.hpDamageFlash - deltaTime * 4.5f);
}

