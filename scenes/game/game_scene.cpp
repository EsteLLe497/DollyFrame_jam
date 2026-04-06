#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "audio.h"
#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kBarrelDebrisGravity = 980.0f;
    constexpr float kZoomTargetTilesX = 23.0f;
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
    if (Entity* player = FindEntityByTag(kTagPlayer))
    {
        game_scene_player_visual_system::ConfigurePlayerSpriteAnimation(*player);
    }

    GameSession_Reset(3, m_flow.timeLimit);
    const float initialMasterVolume = Audio_GetMasterVolume();
    m_debug.bgmRestoreVolume = initialMasterVolume > 0.001f ? initialMasterVolume : 0.6f;
    m_debug.bgmEnabled = initialMasterVolume > 0.001f;
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
    UpdateTuningHotReload(deltaTime);

    if (m_debug.showEscapeMenu)
    {
        UpdateEscapeMenuInput();
        return;
    }

    if (m_mapEditor.active)
    {
        UpdateMapEditorInput(deltaTime);
        return;
    }

    HandleGlobalSceneShortcuts();
    ProcessFilterInput();

    UpdateTuningPanel();
    if (m_debug.showTuningPanel)
    {
        return;
    }

    if (UpdatePitRestartFlow(deltaTime))
    {
        return;
    }
    if (UpdateStageTransitionFlow(deltaTime))
    {
        return;
    }

    const float gameplayDeltaTime = UpdatePhotoModes(deltaTime);
    m_flow.hitStopRemaining = std::max(0.0f, m_flow.hitStopRemaining - deltaTime);
    m_flow.screenShakeRemaining = std::max(0.0f, m_flow.screenShakeRemaining - deltaTime);
    const float effectiveGameplayDeltaTime = m_flow.hitStopRemaining > 0.0f ? 0.0f : gameplayDeltaTime;
    m_flow.lastDeltaTime = effectiveGameplayDeltaTime;

    UpdateFrameTimers(deltaTime, gameplayDeltaTime, effectiveGameplayDeltaTime);
    for (const auto& entity : m_entities)
    {
        entity->Update(effectiveGameplayDeltaTime);
    }

    GameSession_SetTimeRemaining(m_flow.timeRemaining);
    RunGameplayFrame(effectiveGameplayDeltaTime);
    if (Entity* player = FindEntityByTag(kTagPlayer))
    {
        game_scene_player_visual_system::UpdateAnimation(m_player, *player, m_player.dodgeRemaining > 0.0f);
    }
}
void GameScene::Draw()
{
    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderZoomAnchorScreenCenter = false;
    gRenderZoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    gRenderZoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
    float baseCameraZoomMultiplier = 1.0f;
    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize > 0.0f)
    {
        const float targetWorldWidth = tileSize * kZoomTargetTilesX;
        if (targetWorldWidth > 0.0f)
        {
            baseCameraZoomMultiplier = std::max(1.0f, static_cast<float>(SCREEN_WIDTH) / targetWorldWidth);
        }
    }
    gRenderViewScaleMultiplier = m_mapEditor.active ? 1.0f : baseCameraZoomMultiplier;
    if (m_flow.screenShakeRemaining > 0.0f && m_flow.screenShakeDuration > 0.0f && m_flow.screenShakeAmplitude > 0.0f)
    {
        const float elapsed = m_flow.screenShakeDuration - m_flow.screenShakeRemaining;
        const float intensity = Clamp01(m_flow.screenShakeRemaining / m_flow.screenShakeDuration);
        gRenderShakeOffsetX = std::sin(elapsed * 91.0f) * m_flow.screenShakeAmplitude * intensity;
        gRenderShakeOffsetY = std::cos(elapsed * 123.0f) * (m_flow.screenShakeAmplitude * 0.6f) * intensity;
    }
    const float zoomBlend = m_flow.captureModeZoomBlend * m_flow.captureModeZoomBlend * (3.0f - 2.0f * m_flow.captureModeZoomBlend);

    // Capture zoom uses the currently rendered camera-center as pivot.
    gRenderZoomAnchorX = GetViewOriginX() + GetViewWidth() * 0.5f;
    gRenderZoomAnchorY = GetViewOriginY() + GetViewHeight() * 0.5f;

    if (!m_mapEditor.active)
    {
        gRenderViewScaleMultiplier = baseCameraZoomMultiplier + zoomBlend * 0.08f;
        gRenderZoomAnchorScreenCenter = m_flow.cameraMode;
    }

    DrawBackdrop();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Background);
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Shadow);
    for (const auto& entity : m_entities)
    {
        if (entity && (HasTag(*entity, kTagPhotoBox) || entity->GetComponent<PhotoPasteOrderComponent>()))
        {
            continue;
        }
        DrawEntity(*entity);
    }
    DrawEffects();
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Foreground);
    DrawPastedEntitiesFront();
    DrawPhotoPlacementPreview();
    DrawStageDarknessOverlay();
    DrawCaptureOverlay();
    DrawPhotoStorageTray();
    DrawDevelopedPhotoPreview();
    DrawPitRestartOverlay();
    DrawEscapeMenuOverlay();
    DrawMapEditorOverlay();
    DrawTuningPanel();
    DrawBatterySwitchCounters();
    DrawPlayerHpBar();
    DrawEnemyAttackRects();

    gRenderShakeOffsetX = 0.0f;
    gRenderShakeOffsetY = 0.0f;
    gRenderViewScaleMultiplier = 1.0f;
    gRenderZoomAnchorScreenCenter = false;
    gRenderZoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    gRenderZoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
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
    for (auto& spark : m_effects.laserSparks)
    {
        spark.life = std::max(0.0f, spark.life - deltaTime);
        spark.x += spark.velocityX * deltaTime;
        spark.y += spark.velocityY * deltaTime;
        spark.velocityY += kBarrelDebrisGravity * deltaTime * 0.35f;
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
    m_effects.laserSparks.erase(
        std::remove_if(
            m_effects.laserSparks.begin(),
            m_effects.laserSparks.end(),
            [](const LaserSparkParticle& spark)
            {
                return spark.life <= 0.0f;
            }),
        m_effects.laserSparks.end());
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
    ImGui::End();
}

void GameScene::DrawEscapeMenuOverlay() const
{
    if (!m_debug.showEscapeMenu)
    {
        return;
    }

    const int panelWidth = 560;
    const int panelHeight = 540;
    const int left = (SCREEN_WIDTH - panelWidth) / 2;
    const int top = (SCREEN_HEIGHT - panelHeight) / 2;
    const int right = left + panelWidth;
    const int bottom = top + panelHeight;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 156);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(left, top, right, bottom, GetColor(18, 24, 30), TRUE);
    DrawBox(left, top, right, bottom, GetColor(210, 220, 236), FALSE);
    DrawString(left + 22, top + 18, "一時停止メニュー", GetColor(245, 248, 255));
    DrawString(left + 22, top + 44, "W/S・十字キー・マウス: 選択  A/D・左右キー: 調整  Enter/A/左クリック: 決定  Esc: 閉じる", GetColor(170, 194, 220));

    const int rowStartY = top + 86;
    const int rowHeight = 38;
    for (int index = 0; index < 11; ++index)
    {
        const int rowTop = rowStartY + index * rowHeight;
        const int rowBottom = rowTop + rowHeight - 4;
        const bool selected = (m_debug.escapeMenuSelection == index);

        DrawBox(
            left + 18,
            rowTop,
            right - 18,
            rowBottom,
            selected ? GetColor(72, 102, 136) : GetColor(28, 36, 46),
            TRUE);
        DrawBox(
            left + 18,
            rowTop,
            right - 18,
            rowBottom,
            selected ? GetColor(236, 244, 255) : GetColor(92, 116, 140),
            FALSE);

        const int textColor = selected ? GetColor(245, 252, 255) : GetColor(204, 218, 232);
        switch (index)
        {
        case 0:
            DrawString(left + 34, rowTop + 10, "ゲームに戻る", textColor);
            break;
        case 1:
            DrawFormatString(left + 34, rowTop + 10, textColor, "配置プレビュー脈動: %s", m_debug.effectPlacementPulseEnabled ? "ON" : "OFF");
            break;
        case 2:
            DrawFormatString(left + 34, rowTop + 10, textColor, "貼り付きアニメ: %s", m_debug.effectPasteStickEnabled ? "ON" : "OFF");
            break;
        case 3:
            DrawFormatString(left + 34, rowTop + 10, textColor, "貼り付けリング演出: %s", m_debug.effectPasteRingEnabled ? "ON" : "OFF");
            break;
        case 4:
            DrawFormatString(left + 34, rowTop + 10, textColor, "BGM: %s", m_debug.bgmEnabled ? "ON" : "OFF");
            break;
        case 5:
            DrawFormatString(left + 34, rowTop + 10, textColor, "マスター音量: %d%%", static_cast<int>(std::round(Audio_GetMasterVolume() * 100.0f)));
            break;
        case 6:
            DrawFormatString(left + 34, rowTop + 10, textColor, "SE音量: %d%%", static_cast<int>(std::round(Audio_GetSeVolume() * 100.0f)));
            break;
        case 7:
            DrawFormatString(left + 34, rowTop + 10, textColor, "画面揺れ: %s", m_debug.screenShakeEnabled ? "ON" : "OFF");
            break;
        case 8:
            DrawString(left + 34, rowTop + 10, "シーンをリスタート", textColor);
            break;
        case 9:
            DrawString(left + 34, rowTop + 10, "タイトルに戻る", textColor);
            break;
        case 10:
            DrawString(left + 34, rowTop + 10, "ゲームを終える", textColor);
            break;
        default:
            break;
        }
    }
}

EventBus* GameScene::GetEventBus()
{
    return &m_eventBus;
}

bool GameScene::OnCancelAction()
{
    if (m_mapEditor.active)
    {
        m_mapEditor.active = false;
        return true;
    }

    if (m_debug.showTuningPanel)
    {
        m_debug.showTuningPanel = false;
        return true;
    }

    m_debug.showEscapeMenu = !m_debug.showEscapeMenu;
    if (!m_debug.showEscapeMenu)
    {
        return true;
    }

    m_debug.escapeMenuSelection = 0;
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
    m_photo.placement.blockedByUi = false;
    return true;
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

