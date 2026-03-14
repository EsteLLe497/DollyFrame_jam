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
constexpr float kViewOriginX = 120.0f;
constexpr float kViewOriginY = 136.0f;
constexpr float kViewWidth = 960.0f;
constexpr float kViewHeight = 480.0f;
constexpr float kPlayerMoveSpeed = 320.0f;
constexpr float kPlayerJumpSpeed = -760.0f;
constexpr float kPlayerGravity = 1900.0f;
constexpr float kPlayerMaxFallSpeed = 980.0f;
constexpr float kAttackCooldownSeconds = 0.28f;
constexpr float kAttackFlashSeconds = 0.12f;
constexpr float kJumpBufferSeconds = 0.14f;
constexpr float kCoyoteTimeSeconds = 0.10f;

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
    , m_attackCooldownRemaining(0.0f)
    , m_attackFlashRemaining(0.0f)
    , m_jumpBufferRemaining(0.0f)
    , m_coyoteTimeRemaining(0.0f)
    , m_playerFacingRight(true)
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
    m_attackCooldownRemaining = 0.0f;
    m_attackFlashRemaining = 0.0f;
    m_jumpBufferRemaining = 0.0f;
    m_coyoteTimeRemaining = 0.0f;
    m_playerFacingRight = true;

    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_tileTexture = resources.LoadTexture(L"assets\\texture\\block.png");
    m_tileMap.LoadFromCsv("assets/maps/side_scroll_stage01.csv", 48.0f);
    m_eventBus.Clear();

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

    const float goalX = GetMapPixelWidth() - 120.0f;
    Entity& goal = addActor("Goal", m_tileTexture, goalX, 248.0f, 80.0f, 80.0f);
    if (auto* tint = goal.GetComponent<TintComponent>())
    {
        tint->r = 0.98f;
        tint->g = 0.84f;
        tint->b = 0.24f;
        tint->a = 1.0f;
    }

    GameSession_Reset(3, m_timeLimit);
    Logger::Info("GameScene entered as side-scroll sample");
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
    m_attackCooldownRemaining = std::max(0.0f, m_attackCooldownRemaining - deltaTime);
    m_attackFlashRemaining = std::max(0.0f, m_attackFlashRemaining - deltaTime);
    m_jumpBufferRemaining = std::max(0.0f, m_jumpBufferRemaining - deltaTime);
    m_coyoteTimeRemaining = std::max(0.0f, m_coyoteTimeRemaining - deltaTime);
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

    m_timeRemaining = std::max(0.0f, m_timeRemaining - deltaTime);
    GameSession_SetTimeRemaining(m_timeRemaining);
    if (!m_resultQueued && m_timeRemaining <= 0.0f)
    {
        QueueResult(GameEndReason::TimeUp);
    }

    UpdatePlayer(deltaTime);
    UpdateGoalVisual(deltaTime);
    HandleWorldInteractions();
}

void GameScene::Draw()
{
    DrawBackdrop();
    for (const auto& entity : m_entities)
    {
        DrawEntity(*entity);
    }
}

void GameScene::DrawDebugUI()
{
    ImGui::Begin("Game Scene");
    ImGui::Text("2D side-scroll sample");
    ImGui::Text("Move: A / D or gamepad stick");
    ImGui::Text("Jump: W / Space / Gamepad A");
    ImGui::Text("Attack: Left Click");
    ImGui::Text("Restart: R  Title: T");
    ImGui::Text("Entity Count: %d", static_cast<int>(m_entities.size()));
    ImGui::Text("CSV TileMap: %s", m_tileMap.IsLoaded() ? "Loaded" : "Missing");
    ImGui::Text("TileMap Size: %d x %d (tile %.0f)",
        m_tileMap.GetWidth(),
        m_tileMap.GetHeight(),
        m_tileMap.GetTileSize());
    ImGui::Text("Camera X: %.1f / %.1f", m_cameraX, std::max(0.0f, GetMapPixelWidth() - kViewWidth));
    ImGui::Text("Time Remaining: %.1f / %.1f", m_timeRemaining, m_timeLimit);
    ImGui::Text("Goal Contact: %s", m_playerTouchingTarget ? "Hit" : "No Hit");
    ImGui::Text("Hazard Contact: %s", m_playerTouchingHazard ? "Hit" : "No Hit");

    if (auto* player = FindEntityByTag("Player"))
    {
        if (auto* transform = player->GetComponent<TransformComponent>())
        {
            ImGui::Text("Player Pos: %.1f, %.1f", transform->x, transform->y);
        }
        ImGui::Text("Grounded: %s", m_playerGrounded ? "Yes" : "No");
        ImGui::Text("Velocity: %.1f, %.1f", m_playerVelocityX, m_playerVelocityY);
        ImGui::Text("Attack Cooldown: %.2f", m_attackCooldownRemaining);
        ImGui::Text("Jump Buffer: %.2f  Coyote: %.2f", m_jumpBufferRemaining, m_coyoteTimeRemaining);
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
    if (jumpPressed)
    {
        m_jumpBufferRemaining = kJumpBufferSeconds;
    }

    const bool canJumpNow = (m_jumpBufferRemaining > 0.0f && m_coyoteTimeRemaining > 0.0f);
    if (canJumpNow)
    {
        m_playerVelocityY = kPlayerJumpSpeed;
        m_playerGrounded = false;
        m_jumpBufferRemaining = 0.0f;
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

    m_playerGrounded = false;
    if (m_playerVelocityY == 0.0f && wasGrounded)
    {
        const int standingRow = static_cast<int>((transform->y + playerHeight) / tileSize);
        transform->y = static_cast<float>(standingRow) * tileSize - playerHeight;
        m_playerGrounded = true;
    }
    else
    {
        transform->y += m_playerVelocityY * deltaTime;
        if (m_playerVelocityY > 0.0f)
        {
            const int row = static_cast<int>((transform->y + playerHeight - 1.0f) / tileSize);
            const int columnStart = static_cast<int>((transform->x + 6.0f) / tileSize);
            const int columnEnd = static_cast<int>((transform->x + playerWidth - 6.0f) / tileSize);
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
                    break;
                }
            }
        }
        else if (m_playerVelocityY < 0.0f)
        {
            const int row = static_cast<int>(transform->y / tileSize);
            const int columnStart = static_cast<int>((transform->x + 6.0f) / tileSize);
            const int columnEnd = static_cast<int>((transform->x + playerWidth - 6.0f) / tileSize);
            for (int column = columnStart; column <= columnEnd; ++column)
            {
                if (IsSolidTile(column, row))
                {
                    transform->y = static_cast<float>(row + 1) * tileSize;
                    m_playerVelocityY = 0.0f;
                    break;
                }
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
        transform->x + playerWidth * 0.5f - kViewWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - kViewWidth));
    m_cameraX += (cameraTarget - m_cameraX) * std::min(1.0f, deltaTime * 8.0f);
    m_cameraX = std::round(m_cameraX);

    HandlePlayerAttack();
}

void GameScene::HandlePlayerAttack()
{
    if (m_attackCooldownRemaining > 0.0f || !Input_IsMouseLeftPressed())
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

    m_attackCooldownRemaining = kAttackCooldownSeconds;
    m_attackFlashRemaining = kAttackFlashSeconds;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag("Goal"))
    {
        if (auto* tint = goal->GetComponent<TintComponent>())
        {
            const float pulse = 0.65f + 0.35f * std::sin(m_goalPulse * 3.2f);
            tint->r = 1.0f;
            tint->g = 0.82f + pulse * 0.18f;
            tint->b = 0.32f + pulse * 0.30f;
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

        if (IntersectsGoalTile(*playerTransform))
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
        if (IntersectsEntity(*player, *goal))
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

void GameScene::DrawEntity(const Entity& entity) const
{
    const auto* transform = entity.GetComponent<TransformComponent>();
    const auto* sprite = entity.GetComponent<SpriteRenderComponent>();
    if (!transform || !sprite)
    {
        return;
    }

    const float drawX = std::round(kViewOriginX + transform->x - m_cameraX);
    const float drawY = std::round(kViewOriginY + transform->y);
    const float drawWidth = transform->width * transform->scale;
    const float drawHeight = transform->height * transform->scale;
    if (drawX + drawWidth < kViewOriginX || drawX > kViewOriginX + kViewWidth)
    {
        return;
    }

    Shader_ResetStyle();

    const auto* tag = entity.GetComponent<TagComponent>();
    if (tag && tag->tag == "Goal")
    {
        Shader_SetOutline(1.0f, 0.86f, 0.20f, 1.0f, 1.5f);
    }
    else if (tag && tag->tag == "Player")
    {
        if (m_attackFlashRemaining > 0.0f)
        {
            const float intensity = 0.45f + 0.55f * std::sin(m_attackFlashRemaining * 48.0f);
            Shader_SetFlash(0.32f, 0.88f, 1.0f, 1.0f, Clamp01(intensity));
        }
        else if (const auto* cooldown = entity.GetComponent<DamageCooldownComponent>())
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
    const float panelRight = kViewOriginX + kViewWidth;
    const float panelBottom = kViewOriginY + kViewHeight;

    Shader_ResetStyle();
    Shader_SetTint(0.07f, 0.10f, 0.15f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetGradientMap(0.10f, 0.18f, 0.32f, 1.0f, 0.48f, 0.72f, 0.98f, 1.0f, 1.2f);
    Shader_SetTint(0.95f, 0.95f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, kViewOriginX, kViewOriginY, kViewWidth, 210.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.12f, 0.16f, 0.24f, 0.95f);
    SpriteDraw(m_whiteTexture, kViewOriginX, kViewOriginY, kViewWidth, kViewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.22f, 0.30f, 0.42f, 1.0f);
    SpriteDraw(m_whiteTexture, kViewOriginX - 10.0f, kViewOriginY - 10.0f, kViewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, kViewOriginX - 10.0f, panelBottom, kViewWidth + 20.0f, 10.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, kViewOriginX - 10.0f, kViewOriginY, 10.0f, kViewHeight, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, panelRight, kViewOriginY, 10.0f, kViewHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    m_tileMap.Draw(m_tileTexture, kViewOriginX - m_cameraX, kViewOriginY);

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
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

float GameScene::GetMapPixelWidth() const
{
    return static_cast<float>(m_tileMap.GetWidth()) * m_tileMap.GetTileSize();
}

float GameScene::GetMapPixelHeight() const
{
    return static_cast<float>(m_tileMap.GetHeight()) * m_tileMap.GetTileSize();
}
