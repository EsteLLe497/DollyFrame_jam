#include "game_scene_internal.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace game_scene_detail;

namespace
{
    constexpr const char* kTuningFilePath = "assets/tuning.json";

    PhotoFilterTheme GetNextFilterTheme(PhotoFilterTheme current)
    {
        switch (current)
        {
        case PhotoFilterTheme::None:
            return PhotoFilterTheme::Hot;
        case PhotoFilterTheme::Hot:
            return PhotoFilterTheme::Cold;
        case PhotoFilterTheme::Cold:
            return PhotoFilterTheme::Invert;
        case PhotoFilterTheme::Invert:
        default:
            return PhotoFilterTheme::None;
        }
    }

    void WriteTuningJsonFile()
    {
        nlohmann::json root;
        root["camera_view_width"] = gCameraViewWidth;
        root["camera_view_height"] = gCameraViewHeight;
        root["move_speed"] = gPlayerMoveSpeed;
        root["jump_speed"] = gPlayerJumpSpeed;
        root["gravity"] = gPlayerGravity;
        root["max_fall_speed"] = gPlayerMaxFallSpeed;
        root["coyote_time"] = gCoyoteTimeSeconds;
        root["ground_snap_distance"] = gGroundSnapDistance;
        root["capture_width_scale"] = gCaptureWidthScale;
        root["capture_height_scale"] = gCaptureHeightScale;
        root["pickup_time_bonus"] = gPickupTimeBonus;

        std::ofstream stream(kTuningFilePath, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            return;
        }
        stream << root.dump(2);
    }

    void LoadTuningJsonFile()
    {
        std::ifstream stream(kTuningFilePath, std::ios::binary);
        if (!stream.is_open())
        {
            WriteTuningJsonFile();
            return;
        }

        nlohmann::json root;
        try
        {
            stream >> root;
        }
        catch (...)
        {
            return;
        }

        gCameraViewWidth = root.value("camera_view_width", gCameraViewWidth);
        gCameraViewHeight = root.value("camera_view_height", gCameraViewHeight);
        gPlayerMoveSpeed = root.value("move_speed", gPlayerMoveSpeed);
        gPlayerJumpSpeed = root.value("jump_speed", gPlayerJumpSpeed);
        gPlayerGravity = root.value("gravity", gPlayerGravity);
        gPlayerMaxFallSpeed = root.value("max_fall_speed", gPlayerMaxFallSpeed);
        gCoyoteTimeSeconds = root.value("coyote_time", gCoyoteTimeSeconds);
        gGroundSnapDistance = root.value("ground_snap_distance", gGroundSnapDistance);
        gCaptureWidthScale = root.value("capture_width_scale", gCaptureWidthScale);
        gCaptureHeightScale = root.value("capture_height_scale", gCaptureHeightScale);
        gPickupTimeBonus = root.value("pickup_time_bonus", gPickupTimeBonus);
    }
}

GameScene::GameScene()
    : m_whiteTexture(-1)
    , m_tileTexture(-1)
    , m_playerTouchingTarget(false)
    , m_playerTouchingHazard(false)
    , m_resultQueued(false)
    , m_playerGrounded(false)
    , m_timeLimit(60.0f)
    , m_timeRemaining(60.0f)
    , m_cameraX(0.0f)
    , m_playerVelocityX(0.0f)
    , m_playerVelocityY(0.0f)
    , m_goalPulse(0.0f)
    , m_pickupPulse(0.0f)
    , m_coyoteTimeRemaining(0.0f)
    , m_goalUnlocked(false)
    , m_cameraMode(false)
    , m_hasBoxPhoto(false)
    , m_photoBoxSpawned(false)
    , m_enemyCount(0)
    , m_playerFacingRight(true)
    , m_selectedFilterTheme(PhotoFilterTheme::None)
    , m_capturedPhotoTheme(PhotoFilterTheme::None)
    , m_capturedPhotoItems()
    , m_capturedTextureId(-1)
    , m_capturedPhotoWidth(64.0f)
    , m_capturedPhotoHeight(64.0f)
    , m_capturedSourceX(0.0f)
    , m_capturedSourceY(0.0f)
    , m_capturedSourceWidth(1.0f)
    , m_capturedSourceHeight(1.0f)
    , m_capturedTintR(0.86f)
    , m_capturedTintG(0.92f)
    , m_capturedTintB(1.0f)
    , m_capturedTintA(1.0f)
    , m_shutterFlashRemaining(0.0f)
    , m_showCollisionDebug(false)
    , m_photoPlacementActive(false)
    , m_photoPlacementValid(false)
    , m_photoPlacementX(0.0f)
    , m_photoPlacementY(0.0f)
    , m_photoPlacementWidth(0.0f)
    , m_photoPlacementHeight(0.0f)
    , m_photoPlacementLayer(PhotoCopyLayer::Foreground)
    , m_photoPlacementFlipX(false)
    , m_photoPlacementBridgeEnabled(true)
    , m_nextPhotoGroupId(1)
    , m_activePhotoGroupCount(0)
    , m_showTuningPanel(false)
    , m_tuningSelection(0)
    , m_tuningReloadTimer(0.0f)
    , m_tuningFileWriteTime()
    , m_hasTuningFileWriteTime(false)
    , m_lastDeltaTime(0.0f)  //3/18’Ç‰Á
{
}

const char* GameScene::GetSceneId() const
{
    return "game";
}

void GameScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;

    m_entities.clear();
    m_playerTouchingTarget = false;
    m_playerTouchingHazard = false;
    m_resultQueued = false;
    m_playerGrounded = false;
    m_timeRemaining = m_timeLimit;
    m_cameraX = 0.0f;
    m_playerVelocityX = 0.0f;
    m_playerVelocityY = 0.0f;
    m_goalPulse = 0.0f;
    m_pickupPulse = 0.0f;
    m_coyoteTimeRemaining = 0.0f;
    m_goalUnlocked = false;
    m_cameraMode = false;
    m_hasBoxPhoto = false;
    m_photoBoxSpawned = false;
    m_enemyCount = 0;
    m_playerFacingRight = true;
    m_selectedFilterTheme = PhotoFilterTheme::None;
    m_capturedPhotoTheme = PhotoFilterTheme::None;
    m_capturedPhotoItems.clear();
    m_capturedTextureId = -1;
    m_capturedPhotoWidth = 64.0f;
    m_capturedPhotoHeight = 64.0f;
    m_capturedSourceX = 0.0f;
    m_capturedSourceY = 0.0f;
    m_capturedSourceWidth = 1.0f;
    m_capturedSourceHeight = 1.0f;
    m_capturedTintR = 0.86f;
    m_capturedTintG = 0.92f;
    m_capturedTintB = 1.0f;
    m_capturedTintA = 1.0f;
    m_shutterFlashRemaining = 0.0f;
    m_showCollisionDebug = false;
    m_photoPlacementActive = false;
    m_photoPlacementValid = false;
    m_photoPlacementX = 0.0f;
    m_photoPlacementY = 0.0f;
    m_photoPlacementWidth = 0.0f;
    m_photoPlacementHeight = 0.0f;
    m_photoPlacementLayer = PhotoCopyLayer::Foreground;
    m_photoPlacementFlipX = false;
    m_photoPlacementBridgeEnabled = true;
    m_nextPhotoGroupId = 1;
    m_activePhotoGroupCount = 0;
    m_showTuningPanel = false;
    m_tuningSelection = 0;
    m_tuningReloadTimer = 0.0f;
    m_tuningFileWriteTime = {};
    m_hasTuningFileWriteTime = false;
    m_lastDeltaTime = 0.0f;  //3/18’Ç‰Á

    LoadTuningJsonFile();
    {
        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
        if (!ec)
        {
            m_tuningFileWriteTime = writeTime;
            m_hasTuningFileWriteTime = true;
        }
    }

    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_tileTexture = resources.LoadTexture(L"assets\\texture\\block.png");
    m_tileMap.LoadFromCsv("assets/maps/side_scroll_stage01.csv", 48.0f);
    m_eventBus.Clear();

    float goalX = GetMapPixelWidth() - 120.0f;
    float goalY = 248.0f;
    const float tileSize = m_tileMap.GetTileSize();
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (!IsGoalTile(column, row))
            {
                continue;
            }

            goalX = static_cast<float>(column) * tileSize + 8.0f;
            goalY = static_cast<float>(row + 1) * tileSize - 176.0f;
            row = m_tileMap.GetHeight();
            break;
        }
    }

    auto addActor = [this](const char* tag, int textureId, float x, float y, float width, float height) -> Entity&
    {
        auto entity = std::make_unique<Entity>();
        Entity& entityRef = *entity;
        entityRef.AddComponent<TagComponent>(tag);
        entityRef.AddComponent<TransformComponent>(x, y, width, height);
        entityRef.AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
        entityRef.AddComponent<SpriteRenderComponent>(textureId);
        m_entities.push_back(std::move(entity));
        return entityRef;
    };

    auto addEnemy = [&](float x, float y, float width, float height, float amplitudeX, float amplitudeY, float frequency) -> Entity&
    {
        Entity& enemy = addActor("Enemy", m_tileTexture, x, y, width, height);
        enemy.AddComponent<EnemyComponent>(EnemyArchetype::Floater, 1);
        enemy.AddComponent<EnemyMoverComponent>(x, y, amplitudeX, amplitudeY, frequency);
        if (auto* tint = enemy.GetComponent<TintComponent>())
        {
            tint->r = 0.78f;
            tint->g = 0.38f;
            tint->b = 0.92f;
            tint->a = 1.0f;
        }
        return enemy;
    };

	//3/18ƒTƒCƒY‚Ì•às“G‚ð’Ç‰Á‚·‚é‚½‚ß‚Ìƒ‰ƒ€ƒ_ŠÖ”(“c”Vãr)
    auto addWalker = [&](float x, float y) -> Entity&
    {
        Entity& enemy = addActor("Enemy", m_tileTexture, x, y, 64.0f, 80.0f);
        enemy.AddComponent<EnemyComponent>(EnemyArchetype::Walker, 1);
        // EnemyMoverComponent ‚Í•t‚¯‚È‚¢
        if (auto* tint = enemy.GetComponent<TintComponent>())
        {
            tint->r = 0.92f;
            tint->g = 0.38f;
            tint->b = 0.38f;
            tint->a = 1.0f;
        }
        return enemy;
    };

    // 3/19’Ç‰ÁF‰“‹——£UŒ‚“G‚ð’Ç‰Á‚·‚é‚½‚ß‚Ìƒ‰ƒ€ƒ_ŠÖ”(“c”Vãr)
    auto addRanged = [&](float x, float y) -> Entity&
        {
            Entity& enemy = addActor("Enemy", m_tileTexture, x, y, 64.0f, 80.0f);
            enemy.AddComponent<EnemyComponent>(EnemyArchetype::Ranged, 1);
            if (auto* tint = enemy.GetComponent<TintComponent>())
            {
                tint->r = 0.38f;
                tint->g = 0.38f;
                tint->b = 0.92f;
                tint->a = 1.0f;
            }
            return enemy;
        };

    Entity& player = addActor("Player", m_tileTexture, 96.0f, 336.0f, 72.0f, 96.0f);
    player.AddComponent<HealthComponent>(3);
    player.AddComponent<DamageCooldownComponent>(0.75f);
    if (auto* tint = player.GetComponent<TintComponent>())
    {
        tint->r = 0.30f;
        tint->g = 0.82f;
        tint->b = 0.98f;
        tint->a = 1.0f;
    }

    Entity& goal = addActor("Goal", m_tileTexture, goalX, 304.0f, 80.0f, 80.0f);
    goal.AddComponent<GimmickComponent>(GimmickType::Goal);
    if (auto* tint = goal.GetComponent<TintComponent>())
    {
        tint->r = 0.62f;
        tint->g = 0.30f;
        tint->b = 0.24f;
        tint->a = 1.0f;
    }

    Entity& photoSourceA = addActor("PhotoSource", m_tileTexture, 320.0f, 320.0f, 64.0f, 64.0f);
    photoSourceA.AddComponent<GimmickComponent>(GimmickType::PhotoSource);
    if (auto* tint = photoSourceA.GetComponent<TintComponent>())
    {
        tint->r = 0.20f;
        tint->g = 0.52f;
        tint->b = 0.96f;
        tint->a = 1.0f;
    }

    Entity& photoSourceB = addActor("PhotoSource", m_tileTexture, 620.0f, 320.0f, 64.0f, 64.0f);
    photoSourceB.AddComponent<GimmickComponent>(GimmickType::PhotoSource);
    if (auto* tint = photoSourceB.GetComponent<TintComponent>())
    {
        tint->r = 0.18f;
        tint->g = 0.90f;
        tint->b = 0.82f;
        tint->a = 1.0f;
    }

    Entity& shadowSource = addActor("PhotoSource", m_tileTexture, 920.0f, 320.0f, 64.0f, 64.0f);
    shadowSource.AddComponent<GimmickComponent>(GimmickType::PhotoSource);
    if (auto* tint = shadowSource.GetComponent<TintComponent>())
    {
        tint->r = 0.08f;
        tint->g = 0.08f;
        tint->b = 0.10f;
        tint->a = 1.0f;
    }

    Entity& flipSourceA = addActor("PhotoSource", m_tileTexture, 1220.0f, 288.0f, 64.0f, 64.0f);
    flipSourceA.AddComponent<GimmickComponent>(GimmickType::PhotoSource);
    if (auto* tint = flipSourceA.GetComponent<TintComponent>())
    {
        tint->r = 0.96f;
        tint->g = 0.68f;
        tint->b = 0.18f;
        tint->a = 1.0f;
    }

    Entity& flipSourceB = addActor("PhotoSource", m_tileTexture, 1300.0f, 352.0f, 64.0f, 64.0f);
    flipSourceB.AddComponent<GimmickComponent>(GimmickType::PhotoSource);
    if (auto* tint = flipSourceB.GetComponent<TintComponent>())
    {
        tint->r = 0.96f;
        tint->g = 0.68f;
        tint->b = 0.18f;
        tint->a = 1.0f;
    }

    Entity& hazardSource = addActor("Hazard", m_tileTexture, 1600.0f, 320.0f, 64.0f, 64.0f);
    hazardSource.AddComponent<GimmickComponent>(GimmickType::Hazard);
    if (auto* tint = hazardSource.GetComponent<TintComponent>())
    {
        tint->r = 1.0f;
        tint->g = 0.28f;
        tint->b = 0.24f;
        tint->a = 1.0f;
    }

    addEnemy(760.0f, 248.0f, 72.0f, 72.0f, 88.0f, 34.0f, 1.4f);
    addEnemy(1470.0f, 230.0f, 80.0f, 80.0f, 54.0f, 82.0f, 1.1f);

	// 3/18’Ç‰Á
    addWalker(500.0f, 352.0f);

	// 3/19’Ç‰Á
	addRanged(1100.0f, 352.0f);

    GameSession_Reset(3, m_timeLimit);
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
	m_lastDeltaTime = deltaTime;  // 3/18’Ç‰Á
    ZoneScoped;

    m_eventBus.Clear();
    m_tuningReloadTimer = std::max(0.0f, m_tuningReloadTimer - deltaTime);
    if (m_tuningReloadTimer <= 0.0f)
    {
        m_tuningReloadTimer = 0.25f;
        std::error_code ec;
        const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
        if (!ec && (!m_hasTuningFileWriteTime || writeTime != m_tuningFileWriteTime))
        {
            m_tuningFileWriteTime = writeTime;
            m_hasTuningFileWriteTime = true;
            LoadTuningJsonFile();
        }
    }
    m_coyoteTimeRemaining = std::max(0.0f, m_coyoteTimeRemaining - deltaTime);
    m_shutterFlashRemaining = std::max(0.0f, m_shutterFlashRemaining - deltaTime);
    m_pickupPulse += deltaTime;
    for (const auto& entity : m_entities)
    {
        entity->Update(deltaTime);
    }

    if (Input_IsKeyPressed('T'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
    if (Input_IsKeyPressed('R'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    if (Input_IsKeyPressed(VK_F1))
    {
        m_showTuningPanel = !m_showTuningPanel;
    }
    if (Input_IsKeyPressed(VK_F3))
    {
        m_showCollisionDebug = !m_showCollisionDebug;
    }
    if (Input_IsKeyPressed('1'))
    {
        m_selectedFilterTheme = PhotoFilterTheme::None;
    }
    if (Input_IsKeyPressed('2'))
    {
        m_selectedFilterTheme = PhotoFilterTheme::Hot;
    }
    if (Input_IsKeyPressed('3'))
    {
        m_selectedFilterTheme = PhotoFilterTheme::Cold;
    }
    if (Input_IsKeyPressed('4'))
    {
        m_selectedFilterTheme = PhotoFilterTheme::Invert;
    }
    if (Input_IsKeyPressed('C'))
    {
        m_selectedFilterTheme = GetNextFilterTheme(m_selectedFilterTheme);
    }

    UpdateTuningPanel();
    if (m_showTuningPanel)
    {
        return;
    }

    UpdateCameraMode();
    m_timeRemaining = std::max(0.0f, m_timeRemaining - deltaTime);
    GameSession_SetTimeRemaining(m_timeRemaining);
    if (!m_resultQueued && m_timeRemaining <= 0.0f)
    {
        QueueResult(GameEndReason::TimeUp);
    }

    UpdatePlayer(deltaTime);
    HandlePhotoCapture();
    HandlePhotoSpawn();
    UpdateEnemies();
    UpdateBullets();  // 3/19’Ç‰Á(“c”Vãr)
    UpdateGoalVisual(deltaTime);
    HandleWorldInteractions();
    RemoveDefeatedEnemies();
}

void GameScene::Draw()
{
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
    DrawPhotoBoxesByLayer(PhotoCopyLayer::Foreground);
    DrawPhotoPlacementPreview();
    DrawCaptureOverlay();
    DrawTuningPanel();
}

void GameScene::DrawDebugUI()
{
    ImGui::Begin("Game Scene");
    ImGui::Text("2D photo-platform prototype");
    ImGui::Text("Move: A / D or gamepad stick");
    ImGui::Text("Jump: W / Space / Gamepad A");
    ImGui::Text("Camera: Right Click hold");
    ImGui::Text("Capture: Left Click in camera mode");
    ImGui::Text("Filter: C cycle  1 None  2 Hot  3 Cold  4 Invert");
    ImGui::Text("Spawn Captured Object: Hold E");
    ImGui::Text("Placement Layer: Q cycle  Flip: F  Bridge: B");
    ImGui::Text("Stage: solve one gimmick at a time from left to right");
    ImGui::Text("Restart: R  Title: T");
    ImGui::Text("Collision Debug: F3 (%s)", m_showCollisionDebug ? "On" : "Off");
    ImGui::Text("Entity Count: %d", static_cast<int>(m_entities.size()));
    ImGui::Text("CSV TileMap: %s", m_tileMap.IsLoaded() ? "Loaded" : "Missing");
    ImGui::Text("TileMap Size: %d x %d (tile %.0f)",
        m_tileMap.GetWidth(),
        m_tileMap.GetHeight(),
        m_tileMap.GetTileSize());
    ImGui::Text("Camera X: %.1f / %.1f", m_cameraX, std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth));
    ImGui::Text("View Scale: %.2f", GetViewScale());
    ImGui::Text("Time Remaining: %.1f / %.1f", m_timeRemaining, m_timeLimit);
    ImGui::Text("Captured Photo: %s", m_hasBoxPhoto ? "Ready" : "Missing");
    ImGui::Text("Selected Filter: %s",
        m_selectedFilterTheme == PhotoFilterTheme::Hot ? "Hot" :
        m_selectedFilterTheme == PhotoFilterTheme::Cold ? "Cold" :
        m_selectedFilterTheme == PhotoFilterTheme::Invert ? "Invert" : "None");
    ImGui::Text("Captured Filter: %s",
        m_capturedPhotoTheme == PhotoFilterTheme::Hot ? "Hot" :
        m_capturedPhotoTheme == PhotoFilterTheme::Cold ? "Cold" :
        m_capturedPhotoTheme == PhotoFilterTheme::Invert ? "Invert" : "None");
    ImGui::Text("Spawned Copy: %s", m_photoBoxSpawned ? "Active" : "None");
    ImGui::Text("Copy Groups: %d / 3", m_activePhotoGroupCount);
    ImGui::Text("Active Enemies: %d", m_enemyCount);
    ImGui::Text("Placement Mode: %s", m_photoPlacementActive ? "On" : "Off");
    ImGui::Text("Placement Flip: %s", m_photoPlacementFlipX ? "On" : "Off");
    ImGui::Text("Bridge: %s", m_photoPlacementBridgeEnabled ? "On" : "Off");
    ImGui::Text("Camera Mode: %s", m_cameraMode ? "On" : "Off");
    ImGui::Text("Goal: %s", m_goalUnlocked ? "Unlocked" : "Locked");
    ImGui::Text("Goal Contact: %s", m_playerTouchingTarget ? "Hit" : "No Hit");
    ImGui::Text("Hazard Contact: %s", m_playerTouchingHazard ? "Hit" : "No Hit");
    ImGui::Checkbox("Show Collision Debug", &m_showCollisionDebug);

    if (auto* player = FindEntityByTag("Player"))
    {
        if (auto* transform = player->GetComponent<TransformComponent>())
        {
            ImGui::Text("Player Pos: %.1f, %.1f", transform->x, transform->y);
            if (m_cameraMode)
            {
                float frameX = 0.0f;
                float frameY = 0.0f;
                float frameWidth = 0.0f;
                float frameHeight = 0.0f;
                GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);
                ImGui::Text("Capture Frame: %.1f, %.1f, %.1f, %.1f", frameX, frameY, frameWidth, frameHeight);
            }
        }
        ImGui::Text("Grounded: %s", m_playerGrounded ? "Yes" : "No");
        ImGui::Text("Velocity: %.1f, %.1f", m_playerVelocityX, m_playerVelocityY);
        ImGui::Text("Coyote: %.2f", m_coyoteTimeRemaining);
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

void GameScene::UpdateCameraMode()
{
    m_cameraMode = Input_IsKeyDown(VK_RBUTTON);
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
