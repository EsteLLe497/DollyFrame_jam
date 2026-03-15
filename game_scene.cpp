#include "game_scene_internal.h"

using namespace game_scene_detail;

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
            goalY = static_cast<float>(row + 1) * tileSize - 80.0f;
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

    Entity& player = addActor("Player", m_tileTexture, 72.0f, 180.0f, 72.0f, 96.0f);
    player.AddComponent<HealthComponent>(3);
    player.AddComponent<DamageCooldownComponent>(0.75f);
    if (auto* tint = player.GetComponent<TintComponent>())
    {
        tint->r = 0.30f;
        tint->g = 0.82f;
        tint->b = 0.98f;
        tint->a = 1.0f;
    }

    Entity& goal = addActor("Goal", m_tileTexture, goalX, goalY, 80.0f, 80.0f);
    if (auto* tint = goal.GetComponent<TintComponent>())
    {
        tint->r = 0.62f;
        tint->g = 0.30f;
        tint->b = 0.24f;
        tint->a = 1.0f;
    }

    Entity& photoSource = addActor("PhotoSource", m_tileTexture, 336.0f, 320.0f, 64.0f, 64.0f);
    if (auto* tint = photoSource.GetComponent<TintComponent>())
    {
        tint->r = 0.22f;
        tint->g = 0.86f;
        tint->b = 0.92f;
        tint->a = 1.0f;
    }

    GameSession_Reset(3, m_timeLimit);
    Logger::Info("GameScene entered as photo-platform prototype");
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
    if (Input_IsKeyPressed(VK_F3))
    {
        m_showCollisionDebug = !m_showCollisionDebug;
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
    UpdateGoalVisual(deltaTime);
    HandleWorldInteractions();
    RemoveDefeatedEnemies();
}

void GameScene::Draw()
{
    DrawBackdrop();
    for (const auto& entity : m_entities)
    {
        DrawEntity(*entity);
    }
    DrawPhotoPlacementPreview();
    DrawCaptureOverlay();
}

void GameScene::DrawDebugUI()
{
    ImGui::Begin("Game Scene");
    ImGui::Text("2D photo-platform prototype");
    ImGui::Text("Move: A / D or gamepad stick");
    ImGui::Text("Jump: W / Space / Gamepad A");
    ImGui::Text("Camera: Right Click hold");
    ImGui::Text("Capture: Left Click in camera mode");
    ImGui::Text("Spawn Captured Object: E");
    ImGui::Text("Restart: R  Title: T");
    ImGui::Text("Collision Debug: F3 (%s)", m_showCollisionDebug ? "On" : "Off");
    ImGui::Text("Entity Count: %d", static_cast<int>(m_entities.size()));
    ImGui::Text("CSV TileMap: %s", m_tileMap.IsLoaded() ? "Loaded" : "Missing");
    ImGui::Text("TileMap Size: %d x %d (tile %.0f)",
        m_tileMap.GetWidth(),
        m_tileMap.GetHeight(),
        m_tileMap.GetTileSize());
    ImGui::Text("Camera X: %.1f / %.1f", m_cameraX, std::max(0.0f, GetMapPixelWidth() - kBaseViewWidth));
    ImGui::Text("View Scale: %.2f", GetViewScale());
    ImGui::Text("Time Remaining: %.1f / %.1f", m_timeRemaining, m_timeLimit);
    ImGui::Text("Captured Photo: %s", m_hasBoxPhoto ? "Ready" : "Missing");
    ImGui::Text("Spawned Copy: %s", m_photoBoxSpawned ? "Active" : "None");
    ImGui::Text("Placement Mode: %s", m_photoPlacementActive ? "On" : "Off");
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
