#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>

using namespace game_scene_detail;

void GameScene::DrawDebugUI()
{
    const auto toMidBoss2StateLabel = [](MidBoss2State state) -> const char*
    {
        switch (state)
        {
        case MidBoss2State::Idle: return "Idle";
        case MidBoss2State::SpearJump: return "SpearJump";
        case MidBoss2State::SpearThrow: return "SpearThrow";
        case MidBoss2State::SpearLanding: return "SpearLanding";
        case MidBoss2State::SpearCooldown: return "SpearCooldown";
        case MidBoss2State::BeamCharge: return "BeamCharge";
        case MidBoss2State::BeamFire: return "BeamFire";
        case MidBoss2State::BeamCooldown: return "BeamCooldown";
        case MidBoss2State::Damaged: return "Damaged";
        case MidBoss2State::Dead: return "Dead";
        default: return "Unknown";
        }
    };

    ImGui::Begin("Game Scene");
    ImGui::Text("2D photo-platform prototype");
    ImGui::Text("Move: A / D or gamepad stick");
    ImGui::Text("Jump: W / Space / Gamepad A");
    ImGui::Text("Dodge: Left Shift / Right Shift");
    ImGui::Text("Camera: Right Click hold");
    ImGui::Text("Capture: Left Click in camera mode");
    ImGui::Text("Filter: C cycle  1 None  2 Hot  3 Cold  4 Invert  5 Sepia");
    ImGui::Text("Spawn Captured Object: Hold E");
    ImGui::Text("Placement: Flip F  Bridge B");
    ImGui::Text("Stage: solve one gimmick at a time from left to right");
    ImGui::Text("Restart: R  Title: T");
    ImGui::Text("Collision Debug: F3 (%s)", m_debug.showCollisionDebug ? "On" : "Off");
    ImGui::Text("Entity Count: %d", static_cast<int>(m_entities.size()));
    ImGui::Text("CSV TileMap: %s", m_tileMap.IsLoaded() ? "Loaded" : "Missing");
    ImGui::Text("TileMap Size: %d x %d (tile %.0f)",
        m_tileMap.GetWidth(),
        m_tileMap.GetHeight(),
        m_tileMap.GetTileSize());
    ImGui::Text("Camera X: %.1f / %.1f", m_flow.cameraX, std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth));
    ImGui::Text("Camera Y: %.1f / %.1f", m_flow.cameraY, std::max(0.0f, GetMapPixelHeight() - gCameraViewHeight));
    ImGui::Text("Camera Follow Y: %s", gCameraFollowY >= 0.5f ? "On" : "Off");
    bool followY = gCameraFollowY >= 0.5f;
    if (ImGui::Checkbox("Enable Camera Y Follow", &followY))
    {
        gCameraFollowY = followY ? 1.0f : 0.0f;
    }
    ImGui::Text("View Scale: %.2f", GetViewScale());
    ImGui::Text("Time Limit: Off");
    ImGui::Text("Captured Photo: %s", m_photo.capture.hasPhoto ? "Ready" : "Missing");
    ImGui::Text("Stored Photos: %d / 3",
        static_cast<int>(std::count_if(
            m_photo.savedCaptures.begin(),
            m_photo.savedCaptures.end(),
            [](const PhotoCaptureState& capture) { return capture.hasPhoto; })));
    ImGui::Text("Selected Slot: %d", m_photo.selectedCaptureSlot + 1);
    ImGui::Text("Developed Preview: %.2f", m_flow.developedPhotoPreviewRemaining);
    ImGui::Text("Selected Filter: %s", GetPhotoFilterThemeLabel(m_photo.capture.selectedTheme));
    ImGui::Text("Captured Filter: %s", GetPhotoFilterThemeLabel(m_photo.capture.capturedTheme));
    ImGui::Text("Spawned Copy: %s", m_photo.groups.hasSpawnedCopy ? "Active" : "None");
    ImGui::Text("Copy Groups: %d / 3", m_photo.groups.activeGroupCount);
    ImGui::Text("Active Enemies: %d", m_flow.enemyCount);
    ImGui::Text("Placement Mode: %s", m_photo.placement.active ? "On" : "Off");
    ImGui::Text("Map Editor: %s (F4)", m_mapEditor.active ? "On" : "Off");
    ImGui::Text("Placement Flip: %s", m_photo.placement.flipX ? "On" : "Off");
    ImGui::Text("Bridge: %s", m_photo.placement.bridgeEnabled ? "On" : "Off");
    ImGui::Text("Camera Mode: %s", m_flow.cameraMode ? "On" : "Off");
    ImGui::Text("Focus Slow: %s", ((m_flow.cameraMode && m_flow.captureSlowRemaining > 0.0f) || ((m_photo.capture.hasPhoto && Input_IsActionDown(InputAction::HoldPlacement)) && m_flow.placementSlowRemaining > 0.0f)) ? "On" : "Off");
    ImGui::Text("Capture Focus: %.2f", m_flow.captureSlowRemaining);
    ImGui::Text("Placement Focus: %.2f", m_flow.placementSlowRemaining);
    ImGui::Text("Goal: %s", m_flow.goalUnlocked ? "Unlocked" : "Locked");
    ImGui::Text("Goal Contact: %s", m_flow.playerTouchingTarget ? "Hit" : "No Hit");
    ImGui::Text("Hazard Contact: %s", m_flow.playerTouchingHazard ? "Hit" : "No Hit");
    ImGui::Checkbox("Show Collision Debug", &m_debug.showCollisionDebug);
    ImGui::Checkbox("Enable HP Damage", &m_debug.playerHealthDamageEnabled);

    if (auto* player = FindEntityByTag(kTagPlayer))
    {
        if (auto* transform = player->GetComponent<TransformComponent>())
        {
            ImGui::Text("Player Pos: %.1f, %.1f", transform->x, transform->y);
            if (m_flow.cameraMode)
            {
                float frameX = 0.0f;
                float frameY = 0.0f;
                float frameWidth = 0.0f;
                float frameHeight = 0.0f;
                GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);
                ImGui::Text("Capture Frame: %.1f, %.1f, %.1f, %.1f", frameX, frameY, frameWidth, frameHeight);
            }
        }
        ImGui::Text("Grounded: %s", m_player.grounded ? "Yes" : "No");
        ImGui::Text("Velocity: %.1f, %.1f", m_player.velocityX, m_player.velocityY);
        ImGui::Text("Dodge: %.2f / Cooldown: %.2f", m_player.dodgeRemaining, m_player.dodgeCooldownRemaining);
        ImGui::Text("Coyote: %.2f", m_player.coyoteTimeRemaining);
        if (auto* health = player->GetComponent<HealthComponent>())
        {
            ImGui::Text("Player HP: %d / %d", health->GetCurrentHealth(), health->GetMaxHealth());
        }
        if (auto* cooldown = player->GetComponent<DamageCooldownComponent>())
        {
            ImGui::Text("Damage Cooldown: %.2f", cooldown->GetRemainingSeconds());
        }
    }

    ImGui::Text("Events This Frame: %d", static_cast<int>(m_eventBus.GetEvents().size()));

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        const auto* boss = entity->GetComponent<MidBoss2Component>();
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss2 || !transform)
        {
            continue;
        }

        ImGui::SeparatorText("MidBoss2");
        ImGui::Text("Boss Hitbox: %.1f, %.1f, %.1f, %.1f",
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale);
        ImGui::Text("State: %s", toMidBoss2StateLabel(boss->state));
        ImGui::Text("Attack Flow: %d", boss->attackFlowStep);
        ImGui::Text("Cooldown Remaining: %.2f", boss->cooldownRemaining);
        ImGui::Text("Capture Window: %s", boss->captureWindowActive ? "Yes" : "No");
        ImGui::Text("Spear Direction: %.2f, %.2f", boss->lastSpearDirX, boss->lastSpearDirY);

        if (boss->beamEntity)
        {
            if (const auto* beamTransform = boss->beamEntity->GetComponent<TransformComponent>())
            {
                ImGui::Text("Beam Hitbox: %.1f, %.1f, %.1f, %.1f",
                    beamTransform->x,
                    beamTransform->y,
                    beamTransform->width * beamTransform->scale,
                    beamTransform->height * beamTransform->scale);
            }
        }
    }

    ImGui::End();
}

