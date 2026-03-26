#include "game_scene_internal.h"
#include "audio.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kBarrelDebrisGravity = 980.0f;
}

GameScene::GameScene()
    : m_whiteTexture(-1)
    , m_tileTexture(-1)
    , m_photo()
{
}

const char* GameScene::GetSceneId() const
{
    return "game";
}

void GameScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;

    ResetSceneState();
    LoadTuningState();
    InitializeStageResources(resources);
    InitializeStageEntities();

    GameSession_Reset(3, m_flow.timeLimit);
    Audio_PlayCue("demo_bgm");
    Logger::Info("GameScene entered as photo sandbox stage");
}

void GameScene::OnExit()
{
    m_scriptEngine.Shutdown();
    m_entities.clear();
    m_physicsWorld.Shutdown();
}

void GameScene::Update(float deltaTime)
{
    ZoneScoped;

    m_eventBus.Clear();
    m_debug.tuningReloadTimer = std::max(0.0f, m_debug.tuningReloadTimer - deltaTime);
    if (m_debug.tuningReloadTimer <= 0.0f)
    {
        m_debug.tuningReloadTimer = 0.25f;
        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
        if (!ec && (!m_debug.hasTuningFileWriteTime || writeTime != m_debug.tuningFileWriteTime))
        {
            m_debug.tuningFileWriteTime = writeTime;
            m_debug.hasTuningFileWriteTime = true;
            LoadTuningJsonFile();
        }
    }
    if (Input_IsActionPressed(InputAction::ReturnToTitle))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::RestartScene))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::ToggleTuningPanel))
    {
        m_debug.showTuningPanel = !m_debug.showTuningPanel;
    }
    if (Input_IsActionPressed(InputAction::ToggleCollisionDebug))
    {
        m_debug.showCollisionDebug = !m_debug.showCollisionDebug;
    }
    ProcessFilterInput();

    UpdateTuningPanel();
    if (m_debug.showTuningPanel)
    {
        return;
    }

    if (m_flow.pitRestartActive)
    {
        m_flow.pitRestartTimer = std::max(0.0f, m_flow.pitRestartTimer - deltaTime);
        if (m_flow.pitRestartTimer <= 0.0f)
        {
            Entity* player = FindEntityByTag("Player");
            if (player)
            {
                RespawnPlayer(*player);
            }
            else
            {
                m_flow.pitRestartActive = false;
                m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
            }
        }
        return;
    }

    const float gameplayDeltaTime = UpdatePhotoModes(deltaTime);
    m_flow.hitStopRemaining = std::max(0.0f, m_flow.hitStopRemaining - deltaTime);
    m_flow.screenShakeRemaining = std::max(0.0f, m_flow.screenShakeRemaining - deltaTime);
    const float effectiveGameplayDeltaTime = m_flow.hitStopRemaining > 0.0f ? 0.0f : gameplayDeltaTime;
    m_flow.lastDeltaTime = effectiveGameplayDeltaTime;

    m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - effectiveGameplayDeltaTime);
    m_flow.shutterFlashRemaining = std::max(0.0f, m_flow.shutterFlashRemaining - deltaTime);
    const bool previewWasActive = m_flow.developedPhotoPreviewRemaining > 0.0f;
    m_flow.developedPhotoPreviewRemaining = std::max(0.0f, m_flow.developedPhotoPreviewRemaining - deltaTime);
    if (previewWasActive && m_flow.developedPhotoPreviewRemaining <= 0.0f)
    {
        CommitPendingCapturedPhoto();
    }
    m_flow.pickupPulse += gameplayDeltaTime;
    for (const auto& entity : m_entities)
    {
        entity->Update(effectiveGameplayDeltaTime);
    }

    GameSession_SetTimeRemaining(m_flow.timeRemaining);
    RunGameplayFrame(effectiveGameplayDeltaTime);
}
void GameScene::Draw()
{
    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderViewScaleMultiplier = 1.0f;
    if (m_flow.screenShakeRemaining > 0.0f && m_flow.screenShakeDuration > 0.0f && m_flow.screenShakeAmplitude > 0.0f)
    {
        const float elapsed = m_flow.screenShakeDuration - m_flow.screenShakeRemaining;
        const float intensity = Clamp01(m_flow.screenShakeRemaining / m_flow.screenShakeDuration);
        gRenderShakeOffsetX = std::sin(elapsed * 91.0f) * m_flow.screenShakeAmplitude * intensity;
        gRenderShakeOffsetY = std::cos(elapsed * 123.0f) * (m_flow.screenShakeAmplitude * 0.6f) * intensity;
    }
    const float zoomBlend = m_flow.captureModeZoomBlend * m_flow.captureModeZoomBlend * (3.0f - 2.0f * m_flow.captureModeZoomBlend);
    gRenderViewScaleMultiplier = 1.0f + zoomBlend * 0.08f;

    DrawBackdrop();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Background);
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Shadow);
    for (const auto& entity : m_entities)
    {
        if (entity && HasTag(*entity, "PhotoBox"))
        {
            continue;
        }
        DrawEntity(*entity);
    }
    DrawEffects();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Foreground);
    DrawPhotoPlacementPreview();
    DrawCaptureOverlay();
    DrawPhotoStorageTray();
    DrawDevelopedPhotoPreview();
    DrawPitRestartOverlay();
    DrawTuningPanel();
    DrawPlayerHpBar();

    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderViewScaleMultiplier = 1.0f;
}

void GameScene::UpdateEffects(float deltaTime)
{
    for (auto& particle : m_effects.barrelDebris)
    {
        particle.life = std::max(0.0f, particle.life - deltaTime);
        particle.x += particle.velocityX * deltaTime;
        particle.y += particle.velocityY * deltaTime;
        particle.velocityY += kBarrelDebrisGravity * deltaTime;
        particle.rotation += particle.rotationSpeed * deltaTime;
    }

    m_effects.barrelDebris.erase(
        std::remove_if(
            m_effects.barrelDebris.begin(),
            m_effects.barrelDebris.end(),
            [](const BarrelDebrisParticle& particle)
            {
                return particle.life <= 0.0f;
            }),
        m_effects.barrelDebris.end());
}

void GameScene::DrawDebugUI()
{
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

    if (auto* player = FindEntityByTag("Player"))
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
    ImGui::End();
}

EventBus* GameScene::GetEventBus()
{
    return &m_eventBus;
}

Entity* GameScene::FindEntityByTag(const char* tag) const
{
    for (const auto& entity : m_entities)
    {
        const auto* entityTag = entity->GetComponent<TagComponent>();
        if (entityTag && entityTag->tag == tag)
        {
            return entity.get();
        }
    }
    return nullptr;
}

