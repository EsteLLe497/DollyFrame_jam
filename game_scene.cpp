#include "game_scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "components.h"
#include "directX.h"
#include "game_session.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include <tracy/Tracy.hpp>

namespace
{
constexpr float kPixelsPerMeter = 100.0f;
constexpr float kBaseViewWidth = 960.0f;
constexpr float kBaseViewHeight = 480.0f;
constexpr float kPlayerMoveSpeed = 320.0f;
constexpr float kPlayerJumpSpeed = -760.0f;
constexpr float kPlayerGravity = 1900.0f;
constexpr float kPlayerMaxFallSpeed = 980.0f;
constexpr float kCoyoteTimeSeconds = 0.10f;
constexpr float kGroundSnapDistance = 8.0f;
constexpr float kShutterFlashSeconds = 0.18f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool IntersectsRect(const TransformComponent& a, const TransformComponent& b)
{
    const float aWidth = a.width * a.scale;
    const float aHeight = a.height * a.scale;
    const float bWidth = b.width * b.scale;
    const float bHeight = b.height * b.scale;
    return a.x < b.x + bWidth &&
        a.x + aWidth > b.x &&
        a.y < b.y + bHeight &&
        a.y + aHeight > b.y;
}

bool HasTag(const Entity& entity, const char* value)
{
    const auto* tag = entity.GetComponent<TagComponent>();
    return tag && tag->tag == value;
}

float GetViewScale()
{
    const float maxWidth = static_cast<float>(SCREEN_WIDTH) - 128.0f;
    const float maxHeight = static_cast<float>(SCREEN_HEIGHT) - 128.0f;
    return std::max(1.0f, std::min(maxWidth / kBaseViewWidth, maxHeight / kBaseViewHeight));
}

float GetViewWidth()
{
    return kBaseViewWidth * GetViewScale();
}

float GetViewHeight()
{
    return kBaseViewHeight * GetViewScale();
}

float GetViewOriginX()
{
    return std::round((static_cast<float>(SCREEN_WIDTH) - GetViewWidth()) * 0.5f);
}

float GetViewOriginY()
{
    return std::round((static_cast<float>(SCREEN_HEIGHT) - GetViewHeight()) * 0.5f);
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
    , m_capturedTextureId(-1)
    , m_capturedPhotoWidth(64.0f)
    , m_capturedPhotoHeight(64.0f)
    , m_capturedTintR(0.86f)
    , m_capturedTintG(0.92f)
    , m_capturedTintB(1.0f)
    , m_capturedTintA(1.0f)
    , m_shutterFlashRemaining(0.0f)
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
    m_capturedTextureId = -1;
    m_capturedPhotoWidth = 64.0f;
    m_capturedPhotoHeight = 64.0f;
    m_capturedTintR = 0.86f;
    m_capturedTintG = 0.92f;
    m_capturedTintB = 1.0f;
    m_capturedTintA = 1.0f;
    m_shutterFlashRemaining = 0.0f;

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
    ImGui::Text("Camera Mode: %s", m_cameraMode ? "On" : "Off");
    ImGui::Text("Goal: %s", m_goalUnlocked ? "Unlocked" : "Locked");
    ImGui::Text("Goal Contact: %s", m_playerTouchingTarget ? "Hit" : "No Hit");
    ImGui::Text("Hazard Contact: %s", m_playerTouchingHazard ? "Hit" : "No Hit");

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

void GameScene::UpdatePlayer(float deltaTime)
{
    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    float moveAxis = 0.0f;
    if (Input_IsKeyDown('A') || Input_IsKeyDown(VK_LEFT)) { moveAxis -= 1.0f; }
    if (Input_IsKeyDown('D') || Input_IsKeyDown(VK_RIGHT)) { moveAxis += 1.0f; }
    moveAxis += Input_GetMoveX();
    moveAxis = std::clamp(moveAxis, -1.0f, 1.0f);
    if (std::fabs(moveAxis) < 0.15f)
    {
        moveAxis = 0.0f;
    }

    m_playerVelocityX = moveAxis * kPlayerMoveSpeed;
    if (moveAxis > 0.1f)
    {
        m_playerFacingRight = true;
    }
    else if (moveAxis < -0.1f)
    {
        m_playerFacingRight = false;
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float previousX = transform->x;
    const float previousY = transform->y;
    const float previousBottom = previousY + playerHeight;
    const bool wasGrounded = IsStandingOnGround(*transform);
    m_playerGrounded = wasGrounded;
    if (wasGrounded)
    {
        m_coyoteTimeRemaining = kCoyoteTimeSeconds;
    }

    if (wasGrounded && m_playerVelocityY > 0.0f)
    {
        m_playerVelocityY = 0.0f;
    }

    const bool jumpPressed =
        Input_IsKeyPressed(VK_SPACE) ||
        Input_IsKeyPressed('W') ||
        Input_IsKeyPressed(VK_UP) ||
        Input_IsSouthButtonPressed();
    const bool canJumpNow = jumpPressed && m_coyoteTimeRemaining > 0.0f;
    if (canJumpNow)
    {
        m_playerVelocityY = kPlayerJumpSpeed;
        m_playerGrounded = false;
        m_coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    if (m_playerGrounded && !canJumpNow)
    {
        m_playerVelocityY = 0.0f;
    }
    else
    {
        m_playerVelocityY = std::min(kPlayerMaxFallSpeed, m_playerVelocityY + kPlayerGravity * deltaTime);
    }

    transform->x += m_playerVelocityX * deltaTime;
    if (m_playerVelocityX > 0.0f)
    {
        const int column = static_cast<int>((transform->x + playerWidth - 1.0f) / tileSize);
        const int rowStart = static_cast<int>((transform->y + 4.0f) / tileSize);
        const int rowEnd = static_cast<int>((transform->y + playerHeight - 4.0f) / tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (IsSolidTile(column, row))
            {
                transform->x = static_cast<float>(column) * tileSize - playerWidth;
                m_playerVelocityX = 0.0f;
                break;
            }
        }
    }
    else if (m_playerVelocityX < 0.0f)
    {
        const int column = static_cast<int>(transform->x / tileSize);
        const int rowStart = static_cast<int>((transform->y + 4.0f) / tileSize);
        const int rowEnd = static_cast<int>((transform->y + playerHeight - 4.0f) / tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (IsSolidTile(column, row))
            {
                transform->x = static_cast<float>(column + 1) * tileSize;
                m_playerVelocityX = 0.0f;
                break;
            }
        }
    }

    transform->x = std::clamp(transform->x, 0.0f, std::max(0.0f, mapWidth - playerWidth));

    float photoBoxX = 0.0f;
    float photoBoxY = 0.0f;
    float photoBoxWidth = 0.0f;
    float photoBoxHeight = 0.0f;
    const bool hasPhotoBox = GetPhotoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight);
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    const bool hasPhotoSource = GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
    if (hasPhotoBox)
    {
        TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
        playerBounds.scale = transform->scale;
        TransformComponent photoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight);
        if (IntersectsRect(playerBounds, photoBoxBounds))
        {
            if (m_playerVelocityX > 0.0f && previousX + playerWidth <= photoBoxX + 2.0f)
            {
                transform->x = photoBoxX - playerWidth;
                m_playerVelocityX = 0.0f;
            }
            else if (m_playerVelocityX < 0.0f && previousX >= photoBoxX + photoBoxWidth - 2.0f)
            {
                transform->x = photoBoxX + photoBoxWidth;
                m_playerVelocityX = 0.0f;
            }
        }
    }
    if (hasPhotoSource)
    {
        TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
        playerBounds.scale = transform->scale;
        TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
        if (IntersectsRect(playerBounds, photoSourceBounds))
        {
            if (m_playerVelocityX > 0.0f && previousX + playerWidth <= photoSourceX + 2.0f)
            {
                transform->x = photoSourceX - playerWidth;
                m_playerVelocityX = 0.0f;
            }
            else if (m_playerVelocityX < 0.0f && previousX >= photoSourceX + photoSourceWidth - 2.0f)
            {
                transform->x = photoSourceX + photoSourceWidth;
                m_playerVelocityX = 0.0f;
            }
        }
    }

    m_playerGrounded = false;
    if (m_playerVelocityY == 0.0f && wasGrounded)
    {
        m_playerGrounded = TrySnapToGround(*transform, kGroundSnapDistance);
    }
    else
    {
        transform->y += m_playerVelocityY * deltaTime;
        if (m_playerVelocityY > 0.0f)
        {
            const int rowStart = static_cast<int>(previousBottom / tileSize);
            const int rowEnd = static_cast<int>((transform->y + playerHeight - 1.0f) / tileSize);
            const int columnStart = static_cast<int>((transform->x + 6.0f) / tileSize);
            const int columnEnd = static_cast<int>((transform->x + playerWidth - 6.0f) / tileSize);
            for (int row = rowStart; row <= rowEnd; ++row)
            {
                bool collided = false;
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    const bool solidHit = IsSolidTile(column, row);
                    const bool platformHit =
                        IsPlatformTile(column, row) &&
                        previousBottom <= static_cast<float>(row) * tileSize + 8.0f;
                    if (solidHit || platformHit)
                    {
                        transform->y = static_cast<float>(row) * tileSize - playerHeight;
                        m_playerVelocityY = 0.0f;
                        m_playerGrounded = true;
                        collided = true;
                        break;
                    }
                }
                if (collided)
                {
                    break;
                }
            }

            if (!m_playerGrounded && hasPhotoBox)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight);
                if (IntersectsRect(playerBounds, photoBoxBounds) && previousBottom <= photoBoxY + 4.0f)
                {
                    transform->y = photoBoxY - playerHeight;
                    m_playerVelocityY = 0.0f;
                    m_playerGrounded = true;
                }
            }
            if (!m_playerGrounded && hasPhotoSource)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && previousBottom <= photoSourceY + 4.0f)
                {
                    transform->y = photoSourceY - playerHeight;
                    m_playerVelocityY = 0.0f;
                    m_playerGrounded = true;
                }
            }
        }
        else if (m_playerVelocityY < 0.0f)
        {
            const int rowStart = static_cast<int>(previousY / tileSize);
            const int rowEnd = static_cast<int>(transform->y / tileSize);
            const int columnStart = static_cast<int>((transform->x + 6.0f) / tileSize);
            const int columnEnd = static_cast<int>((transform->x + playerWidth - 6.0f) / tileSize);
            for (int row = rowStart; row >= rowEnd; --row)
            {
                bool collided = false;
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    if (IsSolidTile(column, row))
                    {
                        transform->y = static_cast<float>(row + 1) * tileSize;
                        m_playerVelocityY = 0.0f;
                        collided = true;
                        break;
                    }
                }
                if (collided)
                {
                    break;
                }
            }

            if (hasPhotoBox)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight);
                if (IntersectsRect(playerBounds, photoBoxBounds) && previousY >= photoBoxY + photoBoxHeight - 4.0f)
                {
                    transform->y = photoBoxY + photoBoxHeight;
                    m_playerVelocityY = 0.0f;
                }
            }
            if (hasPhotoSource)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && previousY >= photoSourceY + photoSourceHeight - 4.0f)
                {
                    transform->y = photoSourceY + photoSourceHeight;
                    m_playerVelocityY = 0.0f;
                }
            }
        }
        if (!m_playerGrounded && m_playerVelocityY >= 0.0f)
        {
            if (TrySnapToGround(*transform, kGroundSnapDistance))
            {
                m_playerVelocityY = 0.0f;
                m_playerGrounded = true;
            }
        }
    }

    if (transform->y + playerHeight >= mapHeight)
    {
        transform->y = mapHeight - playerHeight;
        m_playerVelocityY = 0.0f;
        m_playerGrounded = true;
    }

    const float cameraTarget = std::clamp(
        transform->x + playerWidth * 0.5f - kBaseViewWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - kBaseViewWidth));
    m_cameraX += (cameraTarget - m_cameraX) * std::min(1.0f, deltaTime * 8.0f);
    m_cameraX = std::round(m_cameraX);

}

void GameScene::HandlePhotoCapture()
{
    if (!m_cameraMode || !Input_IsMouseLeftPressed())
    {
        return;
    }

    Entity* player = FindEntityByTag("Player");
    if (!player || m_hasBoxPhoto)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    Entity* captureTarget = FindCaptureTarget(*playerTransform);
    if (!captureTarget)
    {
        return;
    }

    m_hasBoxPhoto = true;
    if (const auto* sprite = captureTarget->GetComponent<SpriteRenderComponent>())
    {
        m_capturedTextureId = sprite->GetTextureId();
    }
    if (const auto* sourceTransform = captureTarget->GetComponent<TransformComponent>())
    {
        m_capturedPhotoWidth = sourceTransform->width * sourceTransform->scale;
        m_capturedPhotoHeight = sourceTransform->height * sourceTransform->scale;
    }
    if (auto* tint = captureTarget->GetComponent<TintComponent>())
    {
        m_capturedTintR = tint->r;
        m_capturedTintG = tint->g;
        m_capturedTintB = tint->b;
        m_capturedTintA = tint->a;
        tint->r = 0.16f;
        tint->g = 0.34f;
        tint->b = 0.38f;
        tint->a = 0.55f;
    }
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, captureTarget, "scene_change", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, captureTarget, "Captured object photo", 0.0f, 0.0f });
    m_shutterFlashRemaining = kShutterFlashSeconds;
}

void GameScene::HandlePhotoSpawn()
{
    if (!m_hasBoxPhoto || !Input_IsKeyPressed('E'))
    {
        return;
    }

    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    const float desiredX = m_playerFacingRight
        ? playerTransform->x + playerTransform->width * playerTransform->scale + 56.0f
        : playerTransform->x - 72.0f;

    float spawnX = desiredX;
    float spawnY = 0.0f;
    const float spawnWidth = std::max(32.0f, m_capturedPhotoWidth);
    const float spawnHeight = std::max(32.0f, m_capturedPhotoHeight);
    if (!FindSpawnPosition(desiredX, spawnWidth, spawnHeight, spawnX, spawnY))
    {
        return;
    }

    Entity* spawnedBox = FindEntityByTag("PhotoBox");
    if (!spawnedBox)
    {
        auto entity = std::make_unique<Entity>();
        spawnedBox = entity.get();
        spawnedBox->AddComponent<TagComponent>("PhotoBox");
        spawnedBox->AddComponent<TransformComponent>(spawnX, spawnY, spawnWidth, spawnHeight);
        spawnedBox->AddComponent<TintComponent>(m_capturedTintR, m_capturedTintG, m_capturedTintB, m_capturedTintA);
        spawnedBox->AddComponent<SpriteRenderComponent>(m_capturedTextureId >= 0 ? m_capturedTextureId : m_tileTexture);
        m_entities.push_back(std::move(entity));
    }
    else
    {
        if (auto* transform = spawnedBox->GetComponent<TransformComponent>())
        {
            transform->x = spawnX;
            transform->y = spawnY;
            transform->width = spawnWidth;
            transform->height = spawnHeight;
        }
        if (auto* tint = spawnedBox->GetComponent<TintComponent>())
        {
            tint->r = m_capturedTintR;
            tint->g = m_capturedTintG;
            tint->b = m_capturedTintB;
            tint->a = m_capturedTintA;
        }
        if (auto* sprite = spawnedBox->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetTextureId(m_capturedTextureId >= 0 ? m_capturedTextureId : m_tileTexture);
        }
    }

    m_photoBoxSpawned = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, spawnedBox, "test_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, spawnedBox, "Spawned reconstructed box", 0.0f, 0.0f });
}

void GameScene::UpdateEnemies()
{
    m_enemyCount = 0;
    m_goalUnlocked = m_hasBoxPhoto;
}

void GameScene::HandleAttackHits()
{
    return;
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag("Goal"))
    {
        if (auto* tint = goal->GetComponent<TintComponent>())
        {
            const float pulse = 0.65f + 0.35f * std::sin(m_goalPulse * 3.2f);
            if (m_goalUnlocked)
            {
                tint->r = 0.32f + pulse * 0.18f;
                tint->g = 0.92f;
                tint->b = 0.42f + pulse * 0.20f;
            }
            else
            {
                tint->r = 0.62f + pulse * 0.10f;
                tint->g = 0.30f;
                tint->b = 0.24f;
            }
            tint->a = 1.0f;
        }
    }

}

void GameScene::HandleWorldInteractions()
{
    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    m_playerTouchingHazard = false;
    m_playerTouchingTarget = false;

    if (auto* playerTransform = player->GetComponent<TransformComponent>())
    {
        if (IntersectsHazardTile(*playerTransform))
        {
            m_playerTouchingHazard = true;
            HandlePlayerDamage(*player, nullptr, "GameScene player damaged by hazard tile");
        }

        if (m_goalUnlocked && IntersectsGoalTile(*playerTransform))
        {
            m_playerTouchingTarget = true;
            if (!m_resultQueued)
            {
                m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
        }
    }

    if (Entity* goal = FindEntityByTag("Goal"))
    {
        if (m_goalUnlocked && IntersectsEntity(*player, *goal))
        {
            m_playerTouchingTarget = true;
            if (!m_resultQueued)
            {
                m_eventBus.Publish({ EventType::PlaySoundRequest, player, goal, "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
        }
    }

}

void GameScene::RemoveDefeatedEnemies()
{
    m_photoBoxSpawned = FindEntityByTag("PhotoBox") != nullptr;
}

void GameScene::HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage)
{
    auto* health = player.GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    auto* cooldown = player.GetComponent<DamageCooldownComponent>();
    if (cooldown && !cooldown->CanTakeDamage())
    {
        return;
    }

    if (cooldown)
    {
        cooldown->Trigger();
    }

    health->ApplyDamage(1);
    GameSession_SetCurrentHp(health->GetCurrentHealth());
    m_eventBus.Publish({ EventType::PlaySoundRequest, &player, sourceEntity, "contact_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, &player, sourceEntity, logMessage, 0.0f, 0.0f });
    if (health->IsDead() && !m_resultQueued)
    {
        QueueResult(GameEndReason::HpZero);
    }
}

void GameScene::QueueResult(GameEndReason reason)
{
    if (m_resultQueued)
    {
        return;
    }

    m_resultQueued = true;
    GameSession_SetEndReason(reason);
    m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
}

void GameScene::DrawCaptureOverlay() const
{
    if (!m_cameraMode && m_shutterFlashRemaining <= 0.0f)
    {
        return;
    }

    const Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    const auto* transform = player->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float drawX = std::round(viewOriginX + (frameX - m_cameraX) * viewScale);
    const float drawY = std::round(viewOriginY + frameY * viewScale);
    const float drawWidth = frameWidth * viewScale;
    const float drawHeight = frameHeight * viewScale;

    const float shutterT = Clamp01(m_shutterFlashRemaining / kShutterFlashSeconds);
    const float frameInset = 10.0f * shutterT * viewScale;
    const float innerX = drawX + frameInset;
    const float innerY = drawY + frameInset;
    const float innerWidth = std::max(8.0f, drawWidth - frameInset * 2.0f);
    const float innerHeight = std::max(8.0f, drawHeight - frameInset * 2.0f);

    Shader_ResetStyle();
    Shader_SetOutline(
        0.42f + shutterT * 0.48f,
        0.78f + shutterT * 0.18f,
        1.0f,
        1.0f,
        1.6f + shutterT * 1.2f);
    Shader_SetTint(0.14f + shutterT * 0.42f, 0.28f + shutterT * 0.42f, 0.38f + shutterT * 0.42f, 0.18f + shutterT * 0.24f);
    SpriteDraw(m_whiteTexture, innerX, innerY, innerWidth, innerHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    if (Entity* target = FindCaptureTarget(*transform))
    {
        if (const auto* targetTransform = target->GetComponent<TransformComponent>())
        {
            const float targetDrawX = std::round(viewOriginX + (targetTransform->x - m_cameraX) * viewScale);
            const float targetDrawY = std::round(viewOriginY + targetTransform->y * viewScale);
            const float targetDrawWidth = targetTransform->width * targetTransform->scale * viewScale;
            const float targetDrawHeight = targetTransform->height * targetTransform->scale * viewScale;
            Shader_SetOutline(0.34f, 1.0f, 0.48f, 1.0f, 1.8f);
            Shader_SetTint(0.10f, 0.30f, 0.14f, 0.12f);
            SpriteDraw(m_whiteTexture, targetDrawX, targetDrawY, targetDrawWidth, targetDrawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        }
    }

    if (m_shutterFlashRemaining > 0.0f)
    {
        Shader_ResetStyle();
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.10f + shutterT * 0.55f);
        SpriteDraw(m_whiteTexture, GetViewOriginX(), GetViewOriginY(), GetViewWidth(), GetViewHeight(), 0.0f, 0.0f, 1.0f, 1.0f);

        const float lineWidth = 6.0f + shutterT * 10.0f;
        const float lineHeight = std::max(12.0f, 32.0f * shutterT * viewScale);
        Shader_SetTint(1.0f, 1.0f, 1.0f, 0.24f + shutterT * 0.40f);
        SpriteDraw(m_whiteTexture, drawX, drawY - lineHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX, drawY + drawHeight, drawWidth, lineWidth, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX - lineHeight, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        SpriteDraw(m_whiteTexture, drawX + drawWidth, drawY, lineWidth, drawHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_ResetStyle();
}

void GameScene::DrawEntity(const Entity& entity) const
{
    const auto* transform = entity.GetComponent<TransformComponent>();
    const auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    if (!transform || !sprite)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float drawX = std::round(viewOriginX + (transform->x - m_cameraX) * viewScale);
    const float drawY = std::round(viewOriginY + transform->y * viewScale);
    const float drawWidth = transform->width * transform->scale * viewScale;
    const float drawHeight = transform->height * transform->scale * viewScale;
    if (drawX + drawWidth < viewOriginX || drawX > viewOriginX + viewWidth)
    {
        return;
    }

    Shader_ResetStyle();

    const auto* tag = entity.GetComponent<TagComponent>();
    if (tag && tag->tag == "Goal")
    {
        Shader_SetOutline(
            m_goalUnlocked ? 0.28f : 0.92f,
            m_goalUnlocked ? 1.0f : 0.22f,
            m_goalUnlocked ? 0.42f : 0.18f,
            1.0f,
            1.5f);
    }
    else if (tag && tag->tag == "PhotoSource")
    {
        Shader_SetOutline(0.18f, 0.90f, 1.0f, 1.0f, 1.4f);
    }
    else if (tag && tag->tag == "PhotoBox")
    {
        Shader_SetFlash(0.82f, 0.90f, 1.0f, 1.0f, 0.18f);
    }
    else if (tag && tag->tag == "Player")
    {
        if (const auto* cooldown = entity.GetComponent<DamageCooldownComponent>())
        {
            if (cooldown->GetRemainingSeconds() > 0.0f)
            {
                const float flash = 0.40f + 0.60f * std::sin(cooldown->GetRemainingSeconds() * 28.0f);
                Shader_SetFlash(1.0f, 0.30f, 0.22f, 1.0f, Clamp01(flash));
            }
        }
    }

    if (const auto* tint = entity.GetComponent<TintComponent>())
    {
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    }

    SpriteDraw(
        sprite->GetTextureId(),
        drawX,
        drawY,
        drawWidth,
        drawHeight,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        transform->rotation);

    Shader_ResetStyle();
}

void GameScene::DrawBackdrop() const
{
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float viewWidth = GetViewWidth();
    const float viewHeight = GetViewHeight();
    const float panelRight = viewOriginX + viewWidth;
    const float panelBottom = viewOriginY + viewHeight;

    Shader_ResetStyle();
    Shader_SetTint(0.02f, 0.02f, 0.03f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetGradientMap(0.03f, 0.03f, 0.05f, 1.0f, 0.10f, 0.10f, 0.14f, 1.0f, 1.0f);
    Shader_SetTint(0.92f, 0.92f, 0.96f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, 210.0f * viewScale, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.05f, 0.05f, 0.07f, 0.98f);
    SpriteDraw(m_whiteTexture, viewOriginX, viewOriginY, viewWidth, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.18f, 0.18f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY - 10.0f, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, panelBottom, viewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, viewOriginX - 10.0f, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, viewOriginY, 10.0f, viewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    m_tileMap.Draw(m_tileTexture, viewOriginX - m_cameraX * viewScale, viewOriginY, viewScale);

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::GetCaptureFrameRect(const TransformComponent& playerTransform, float& x, float& y, float& width, float& height) const
{
    width = 140.0f;
    height = 112.0f;
    x = m_playerFacingRight
        ? playerTransform.x + playerTransform.width * playerTransform.scale + 28.0f
        : playerTransform.x - width - 28.0f;
    y = playerTransform.y - 8.0f;
}

Entity* GameScene::FindCaptureTarget(const TransformComponent& playerTransform) const
{
    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(playerTransform, frameX, frameY, frameWidth, frameHeight);

    TransformComponent captureFrame(frameX, frameY, frameWidth, frameHeight);
    Entity* bestTarget = nullptr;
    float bestDistance = 1000000.0f;
    for (const auto& entity : m_entities)
    {
        if (HasTag(*entity, "Player") || HasTag(*entity, "Goal") || HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform || !IntersectsRect(captureFrame, *transform))
        {
            continue;
        }

        const float targetCenterX = transform->x + transform->width * transform->scale * 0.5f;
        const float playerCenterX = playerTransform.x + playerTransform.width * playerTransform.scale * 0.5f;
        const float distance = std::fabs(targetCenterX - playerCenterX);
        if (!bestTarget || distance < bestDistance)
        {
            bestTarget = entity.get();
            bestDistance = distance;
        }
    }

    return bestTarget;
}

bool GameScene::IsSolidTile(int column, int row) const
{
    const int tile = m_tileMap.GetTile(column, row);
    return tile == 1 || tile == 2 || tile == 3 || tile == 4;
}

bool GameScene::IsPlatformTile(int column, int row) const
{
    static_cast<void>(column);
    static_cast<void>(row);
    return false;
}

bool GameScene::IsHazardTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == 4;
}

bool GameScene::IsGoalTile(int column, int row) const
{
    return m_tileMap.GetTile(column, row) == 5;
}

bool GameScene::IsStandingOnGround(const TransformComponent& transform) const
{
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    if (GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight))
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        const float sourceTop = photoSourceY;
        const float sourceLeft = photoSourceX;
        const float sourceRight = photoSourceX + photoSourceWidth;
        const bool horizontallyOverlapping = playerRight > sourceLeft && playerLeft < sourceRight;
        if (horizontallyOverlapping && std::fabs(playerBottom - sourceTop) <= 4.0f)
        {
            return true;
        }
    }

    float photoBoxX = 0.0f;
    float photoBoxY = 0.0f;
    float photoBoxWidth = 0.0f;
    float photoBoxHeight = 0.0f;
    if (GetPhotoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight))
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float playerBottom = transform.y + height;
        const float playerLeft = transform.x + 6.0f;
        const float playerRight = transform.x + width - 6.0f;
        const float boxTop = photoBoxY;
        const float boxLeft = photoBoxX;
        const float boxRight = photoBoxX + photoBoxWidth;
        const bool horizontallyOverlapping = playerRight > boxLeft && playerLeft < boxRight;
        if (horizontallyOverlapping && std::fabs(playerBottom - boxTop) <= 4.0f)
        {
            return true;
        }
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float footY = transform.y + transform.height * transform.scale + 2.0f;
    const int row = static_cast<int>(footY / tileSize);
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 6.0f) / tileSize);
    for (int column = columnStart; column <= columnEnd; ++column)
    {
        if (IsSolidTile(column, row) || IsPlatformTile(column, row))
        {
            return true;
        }
    }
    return false;
}

bool GameScene::TrySnapToGround(TransformComponent& transform, float maxSnapDistance) const
{
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    if (GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight))
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        const bool horizontallyOverlapping = right > photoSourceX && left < photoSourceX + photoSourceWidth;
        if (horizontallyOverlapping)
        {
            const float candidateY = photoSourceY - height;
            if (candidateY >= transform.y - 0.5f && (candidateY - transform.y) <= maxSnapDistance)
            {
                transform.y = candidateY;
                return true;
            }
        }
    }

    float photoBoxX = 0.0f;
    float photoBoxY = 0.0f;
    float photoBoxWidth = 0.0f;
    float photoBoxHeight = 0.0f;
    if (GetPhotoBoxBounds(photoBoxX, photoBoxY, photoBoxWidth, photoBoxHeight))
    {
        const float width = transform.width * transform.scale;
        const float height = transform.height * transform.scale;
        const float bottom = transform.y + height;
        const float left = transform.x + 6.0f;
        const float right = transform.x + width - 6.0f;
        const bool horizontallyOverlapping = right > photoBoxX && left < photoBoxX + photoBoxWidth;
        if (horizontallyOverlapping)
        {
            const float candidateY = photoBoxY - height;
            if (candidateY >= transform.y - 0.5f && (candidateY - transform.y) <= maxSnapDistance)
            {
                transform.y = candidateY;
                return true;
            }
        }
    }

    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const float bottom = transform.y + height;
    const int columnStart = static_cast<int>((transform.x + 6.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 6.0f) / tileSize);
    const int rowStart = static_cast<int>(bottom / tileSize);
    const int rowEnd = static_cast<int>((bottom + maxSnapDistance) / tileSize);

    float nearestGroundY = 0.0f;
    bool foundGround = false;
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (!IsSolidTile(column, row))
            {
                continue;
            }

            const float candidateY = static_cast<float>(row) * tileSize - height;
            if (candidateY < transform.y - 0.5f)
            {
                continue;
            }

            if (!foundGround || candidateY < nearestGroundY)
            {
                nearestGroundY = candidateY;
                foundGround = true;
            }
        }
    }

    if (!foundGround)
    {
        return false;
    }

    if ((nearestGroundY - transform.y) > maxSnapDistance)
    {
        return false;
    }

    transform.y = nearestGroundY;
    return true;
}

bool GameScene::IntersectsHazardTile(const TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const int columnStart = static_cast<int>((transform.x + 8.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 8.0f) / tileSize);
    const int footRow = static_cast<int>((transform.y + transform.height * transform.scale + 2.0f) / tileSize);
    for (int column = columnStart; column <= columnEnd; ++column)
    {
        if (IsHazardTile(column, footRow))
        {
            return true;
        }
    }
    return false;
}

bool GameScene::IntersectsGoalTile(const TransformComponent& transform) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float width = transform.width * transform.scale;
    const float height = transform.height * transform.scale;
    const int columnStart = static_cast<int>((transform.x + 8.0f) / tileSize);
    const int columnEnd = static_cast<int>((transform.x + width - 8.0f) / tileSize);
    const int rowStart = static_cast<int>((transform.y + 8.0f) / tileSize);
    const int rowEnd = static_cast<int>((transform.y + height - 8.0f) / tileSize);
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (IsGoalTile(column, row))
            {
                return true;
            }
        }
    }
    return false;
}

bool GameScene::IntersectsEntity(const Entity& a, const Entity& b) const
{
    const auto* transformA = a.GetComponent<TransformComponent>();
    const auto* transformB = b.GetComponent<TransformComponent>();
    if (!transformA || !transformB)
    {
        return false;
    }

    return IntersectsRect(*transformA, *transformB);
}

bool GameScene::GetEntityBoundsByTag(const char* tag, float& x, float& y, float& width, float& height) const
{
    Entity* entity = FindEntityByTag(tag);
    if (!entity)
    {
        return false;
    }

    const auto* transform = entity->GetComponent<TransformComponent>();
    if (!transform)
    {
        return false;
    }

    x = transform->x;
    y = transform->y;
    width = transform->width * transform->scale;
    height = transform->height * transform->scale;
    return true;
}

bool GameScene::GetPhotoBoxBounds(float& x, float& y, float& width, float& height) const
{
    return GetEntityBoundsByTag("PhotoBox", x, y, width, height);
}

bool GameScene::FindSpawnPosition(float desiredX, float objectWidth, float objectHeight, float& outX, float& outY) const
{
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const int centerColumn = static_cast<int>((desiredX + objectWidth * 0.5f) / tileSize);
    const int maxOffset = 4;

    for (int offset = 0; offset <= maxOffset; ++offset)
    {
        const int candidates[2] = { centerColumn + offset, centerColumn - offset };
        for (int i = 0; i < 2; ++i)
        {
            const int column = candidates[i];
            if (column < 0 || column >= m_tileMap.GetWidth())
            {
                continue;
            }

            for (int row = 0; row < m_tileMap.GetHeight(); ++row)
            {
                if (!IsSolidTile(column, row))
                {
                    continue;
                }

                const float candidateX = std::clamp(static_cast<float>(column) * tileSize, 0.0f, std::max(0.0f, mapWidth - objectWidth));
                const float candidateY = static_cast<float>(row) * tileSize - objectHeight;
                outX = candidateX;
                outY = candidateY;
                return true;
            }
        }
    }

    return false;
}

float GameScene::GetMapPixelWidth() const
{
    return static_cast<float>(m_tileMap.GetWidth()) * m_tileMap.GetTileSize();
}

float GameScene::GetMapPixelHeight() const
{
    return static_cast<float>(m_tileMap.GetHeight()) * m_tileMap.GetTileSize();
}
