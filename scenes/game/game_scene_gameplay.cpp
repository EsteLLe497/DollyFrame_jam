#include "game_scene_internal.h"
#include "photo_system.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace game_scene_detail;

namespace
{
    constexpr float kPlayerAfterimageLifetime = 0.18f;
    constexpr float kPlayerAfterimageSpawnInterval = 0.03f;
    constexpr int kMaxPlayerAfterimages = 8;

    struct TuningEntry
    {
        const char* label;
        float* value;
        float step;
        float minValue;
        float maxValue;
    };

}

void GameScene::UpdateTuningPanel()
{
    if (!m_showTuningPanel)
    {
        return;
    }

    TuningEntry entries[] =
    {
        { "Camera Width", &gCameraViewWidth, 20.0f, 640.0f, 1920.0f },
        { "Camera Height", &gCameraViewHeight, 20.0f, 360.0f, 1080.0f },
        { "Move Speed", &gPlayerMoveSpeed, 10.0f, 80.0f, 960.0f },
        { "Jump Speed", &gPlayerJumpSpeed, 20.0f, -1600.0f, -120.0f },
        { "Gravity", &gPlayerGravity, 50.0f, 200.0f, 4000.0f },
        { "Max Fall", &gPlayerMaxFallSpeed, 20.0f, 200.0f, 2400.0f },
        { "Coyote", &gCoyoteTimeSeconds, 0.01f, 0.0f, 0.4f },
        { "Ground Snap", &gGroundSnapDistance, 0.5f, 0.0f, 24.0f },
        { "Capture Width", &gCaptureWidthScale, 0.05f, 0.5f, 3.0f },
        { "Capture Height", &gCaptureHeightScale, 0.05f, 0.5f, 3.0f },
        { "Pickup Bonus", &gPickupTimeBonus, 1.0f, 0.0f, 60.0f },
    };
    constexpr int kEntryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));

    if (Input_IsKeyPressed(VK_UP))
    {
        m_tuningSelection = (m_tuningSelection + kEntryCount - 1) % kEntryCount;
    }
    if (Input_IsKeyPressed(VK_DOWN))
    {
        m_tuningSelection = (m_tuningSelection + 1) % kEntryCount;
    }

    float delta = 0.0f;
    if (Input_IsKeyDown(VK_LEFT))
    {
        delta -= entries[m_tuningSelection].step;
    }
    if (Input_IsKeyDown(VK_RIGHT))
    {
        delta += entries[m_tuningSelection].step;
    }

    if (delta != 0.0f)
    {
        *entries[m_tuningSelection].value = std::clamp(
            *entries[m_tuningSelection].value + delta,
            entries[m_tuningSelection].minValue,
            entries[m_tuningSelection].maxValue);
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
        std::ofstream stream("assets/tuning.json", std::ios::binary | std::ios::trunc);
        if (stream.is_open())
        {
            stream << root.dump(2);
        }
    }
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

    m_playerDodgeRemaining = std::max(0.0f, m_playerDodgeRemaining - deltaTime);
    m_playerDodgeCooldownRemaining = std::max(0.0f, m_playerDodgeCooldownRemaining - deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    if (moveAxis > 0.1f)
    {
        m_playerFacingRight = true;
    }
    else if (moveAxis < -0.1f)
    {
        m_playerFacingRight = false;
    }

    const bool dodgePressed =
        Input_IsKeyPressed(VK_LSHIFT) ||
        Input_IsKeyPressed(VK_RSHIFT) ||
        Input_IsKeyPressed(VK_SHIFT);
    if (dodgePressed && m_playerDodgeRemaining <= 0.0f && m_playerDodgeCooldownRemaining <= 0.0f)
    {
        m_playerDodgeDirection = moveAxis != 0.0f ? (moveAxis > 0.0f ? 1.0f : -1.0f) : (m_playerFacingRight ? 1.0f : -1.0f);
        m_playerFacingRight = m_playerDodgeDirection > 0.0f;
        m_playerDodgeRemaining = gPlayerDodgeDuration;
        m_playerDodgeCooldownRemaining = gPlayerDodgeCooldown;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_playerDodgeRemaining > 0.0f;
    m_playerVelocityX = isDodging
        ? m_playerDodgeDirection * gPlayerDodgeSpeed
        : moveAxis * gPlayerMoveSpeed;

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
        m_coyoteTimeRemaining = gCoyoteTimeSeconds;
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
    const bool canJumpNow = !isDodging && jumpPressed && m_coyoteTimeRemaining > 0.0f;
    if (canJumpNow)
    {
        m_playerVelocityY = gPlayerJumpSpeed;
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
        m_playerVelocityY = std::min(gPlayerMaxFallSpeed, m_playerVelocityY + gPlayerGravity * deltaTime);
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

    if (isDodging)
    {
        TrySpawnPlayerAfterimage(*transform);
    }

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    const bool hasPhotoBox = !photoBoxes.empty();
    float photoSourceX = 0.0f;
    float photoSourceY = 0.0f;
    float photoSourceWidth = 0.0f;
    float photoSourceHeight = 0.0f;
    const bool hasPhotoSource = GetEntityBoundsByTag("PhotoSource", photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
    if (hasPhotoBox)
    {
        for (const auto& photoBoxBounds : photoBoxes)
        {
            TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
            playerBounds.scale = transform->scale;
            if (!IntersectsRect(playerBounds, photoBoxBounds))
            {
                continue;
            }

            const float photoBoxX = photoBoxBounds.x;
            const float photoBoxWidth = photoBoxBounds.width * photoBoxBounds.scale;
            if (m_playerVelocityX > 0.0f && previousX + playerWidth <= photoBoxX + kHorizontalCollisionEpsilon)
            {
                transform->x = photoBoxX - playerWidth;
                m_playerVelocityX = 0.0f;
                break;
            }
            if (m_playerVelocityX < 0.0f && previousX >= photoBoxX + photoBoxWidth - kHorizontalCollisionEpsilon)
            {
                transform->x = photoBoxX + photoBoxWidth;
                m_playerVelocityX = 0.0f;
                break;
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
            if (m_playerVelocityX > 0.0f && previousX + playerWidth <= photoSourceX + kHorizontalCollisionEpsilon)
            {
                transform->x = photoSourceX - playerWidth;
                m_playerVelocityX = 0.0f;
            }
            else if (m_playerVelocityX < 0.0f && previousX >= photoSourceX + photoSourceWidth - kHorizontalCollisionEpsilon)
            {
                transform->x = photoSourceX + photoSourceWidth;
                m_playerVelocityX = 0.0f;
            }
        }
    }

    m_playerGrounded = false;
    if (m_playerVelocityY == 0.0f && wasGrounded)
    {
        m_playerGrounded = TrySnapToGround(*transform, gGroundSnapDistance);
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
                for (const auto& photoBoxBounds : photoBoxes)
                {
                    TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                    playerBounds.scale = transform->scale;
                    if (IntersectsRect(playerBounds, photoBoxBounds) &&
                        previousBottom <= photoBoxBounds.y + kSurfaceContactEpsilon)
                    {
                        transform->y = photoBoxBounds.y - playerHeight;
                        m_playerVelocityY = 0.0f;
                        m_playerGrounded = true;
                        break;
                    }
                }
            }
            if (!m_playerGrounded && hasPhotoSource)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && previousBottom <= photoSourceY + kSurfaceContactEpsilon)
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
                for (const auto& photoBoxBounds : photoBoxes)
                {
                    TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                    playerBounds.scale = transform->scale;
                    const float photoBoxHeight = photoBoxBounds.height * photoBoxBounds.scale;
                    if (IntersectsRect(playerBounds, photoBoxBounds) &&
                        previousY >= photoBoxBounds.y + photoBoxHeight - kSurfaceContactEpsilon)
                    {
                        transform->y = photoBoxBounds.y + photoBoxHeight;
                        m_playerVelocityY = 0.0f;
                        break;
                    }
                }
            }
            if (hasPhotoSource)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && previousY >= photoSourceY + photoSourceHeight - kSurfaceContactEpsilon)
                {
                    transform->y = photoSourceY + photoSourceHeight;
                    m_playerVelocityY = 0.0f;
                }
            }
        }
        if (!m_playerGrounded && m_playerVelocityY >= 0.0f)
        {
            if (TrySnapToGround(*transform, gGroundSnapDistance))
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
        transform->x + playerWidth * 0.5f - gCameraViewWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - gCameraViewWidth));
    m_cameraX += (cameraTarget - m_cameraX) * std::min(1.0f, deltaTime * 8.0f);
    m_cameraX = std::round(m_cameraX);
}

void GameScene::UpdatePlayerAfterimages(float deltaTime)
{
    for (auto& afterimage : m_playerAfterimages)
    {
        afterimage.life = std::max(0.0f, afterimage.life - deltaTime);
    }
    m_playerAfterimages.erase(
        std::remove_if(
            m_playerAfterimages.begin(),
            m_playerAfterimages.end(),
            [](const PlayerAfterimage& afterimage)
            {
                return afterimage.life <= 0.0f;
            }),
        m_playerAfterimages.end());
}

void GameScene::TrySpawnPlayerAfterimage(const TransformComponent& transform)
{
    const bool shouldAddAfterimage =
        m_playerAfterimages.empty() ||
        (kPlayerAfterimageLifetime - m_playerAfterimages.front().life) >= kPlayerAfterimageSpawnInterval;
    if (!shouldAddAfterimage)
    {
        return;
    }

    PlayerAfterimage afterimage;
    afterimage.x = transform.x;
    afterimage.y = transform.y;
    afterimage.rotation = transform.rotation;
    afterimage.scale = transform.scale;
    afterimage.flipX = m_playerFacingRight ? false : true;
    afterimage.life = kPlayerAfterimageLifetime;
    m_playerAfterimages.insert(m_playerAfterimages.begin(), afterimage);
    if (static_cast<int>(m_playerAfterimages.size()) > kMaxPlayerAfterimages)
    {
        m_playerAfterimages.resize(kMaxPlayerAfterimages);
    }
}

void GameScene::HandlePhotoCapture()
{
    photo_system::HandleCapture(*this);
}

void GameScene::HandlePhotoSpawn()
{
    photo_system::HandleSpawn(*this);
}

void GameScene::UpdateEnemies()
{
    m_enemyCount = 0;
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (enemy && enemy->IsEnabled())
        {
            ++m_enemyCount;
        }
    }
    m_goalUnlocked = m_photo.groups.hasSpawnedCopy;
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

    std::vector<Entity*> consumedGimmicks;
    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == player || !IntersectsEntity(*player, *entity))
        {
            continue;
        }

        if (const auto* enemy = entity->GetComponent<EnemyComponent>())
        {
            if (enemy->IsEnabled())
            {
                m_playerTouchingHazard = true;
                HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by enemy");
            }
        }

        auto* gimmick = entity->GetComponent<GimmickComponent>();
        if (!gimmick || !gimmick->IsEnabled())
        {
            continue;
        }

        switch (gimmick->GetType())
        {
        case GimmickType::Hazard:
            m_playerTouchingHazard = true;
            HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by gimmick hazard");
            break;
        case GimmickType::Goal:
            if (m_goalUnlocked && !m_resultQueued)
            {
                m_playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case GimmickType::Pickup:
            m_timeRemaining = std::min(m_timeLimit, m_timeRemaining + gPickupTimeBonus);
            GameSession_SetTimeRemaining(m_timeRemaining);
            m_eventBus.Publish({ EventType::PlaySoundRequest, player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, player, entity.get(), "Recovered time from gimmick pickup", 0.0f, 0.0f });
            gimmick->Consume();
            if (gimmick->IsConsumed())
            {
                consumedGimmicks.push_back(entity.get());
            }
            break;
        case GimmickType::PhotoSource:
        case GimmickType::Gate:
        case GimmickType::Switch:
        default:
            break;
        }
    }

    std::vector<Entity*> consumedPickups;
    std::vector<Entity*> defeatedEnemies;
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* photoRole = entity->GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        if (!photoRole || !IntersectsEntity(*player, *entity))
        {
            continue;
        }
        if (photoLayer && photoLayer->layer != PhotoCopyLayer::Foreground)
        {
            continue;
        }

        switch (photoRole->role)
        {
        case PhotoCopyRole::Hazard:
            m_playerTouchingHazard = true;
            HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by copied hazard");
            break;
        case PhotoCopyRole::Ally:
            for (const auto& enemyEntity : m_entities)
            {
                if (!enemyEntity || enemyEntity.get() == entity.get())
                {
                    continue;
                }

                auto* enemy = enemyEntity->GetComponent<EnemyComponent>();
                if (!enemy || !enemy->IsEnabled() || !IntersectsEntity(*entity, *enemyEntity))
                {
                    continue;
                }

                enemy->MarkDefeated();
                defeatedEnemies.push_back(enemyEntity.get());
            }
            break;
        case PhotoCopyRole::GoalRelay:
            if (m_goalUnlocked && !m_resultQueued)
            {
                m_playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case PhotoCopyRole::Pickup:
            m_timeRemaining = std::min(m_timeLimit, m_timeRemaining + gPickupTimeBonus);
            GameSession_SetTimeRemaining(m_timeRemaining);
            m_eventBus.Publish({ EventType::PlaySoundRequest, player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, player, entity.get(), "Recovered time from copied pickup", 0.0f, 0.0f });
            consumedPickups.push_back(entity.get());
            break;
        case PhotoCopyRole::Solid:
        default:
            break;
        }
    }

    if (!consumedGimmicks.empty() || !consumedPickups.empty())
    {
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& entity)
                {
                    if (!entity)
                    {
                        return false;
                    }

                    return std::find(consumedGimmicks.begin(), consumedGimmicks.end(), entity.get()) != consumedGimmicks.end() ||
                        std::find(consumedPickups.begin(), consumedPickups.end(), entity.get()) != consumedPickups.end();
                }),
            m_entities.end());
    }

    if (!defeatedEnemies.empty())
    {
        m_eventBus.Publish({ EventType::LogMessage, player, defeatedEnemies.front(), "Invert photo neutralized an enemy", 0.0f, 0.0f });
    }
}

void GameScene::RemoveDefeatedEnemies()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                const auto* enemy = entity ? entity->GetComponent<EnemyComponent>() : nullptr;
                if (enemy && enemy->IsDefeated())
                {
                    return true;
                }

                if (!entity || !HasTag(*entity, "PhotoBox"))
                {
                    return false;
                }

                const auto* lifetime = entity->GetComponent<PhotoCopyLifetimeComponent>();
                return lifetime && lifetime->IsExpired();
            }),
        m_entities.end());

    m_photo.groups.hasSpawnedCopy = FindEntityByTag("PhotoBox") != nullptr;
    int maxGroupId = 0;
    std::vector<int> groups;
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        if (const auto* group = entity->GetComponent<PhotoCopyGroupComponent>())
        {
            maxGroupId = std::max(maxGroupId, group->groupId);
            if (std::find(groups.begin(), groups.end(), group->groupId) == groups.end())
            {
                groups.push_back(group->groupId);
            }
        }
    }
    m_photo.groups.activeGroupCount = static_cast<int>(groups.size());
    m_photo.groups.nextGroupId = std::max(m_photo.groups.nextGroupId, maxGroupId + 1);
}

void GameScene::HandlePlayerDamage(Entity& player, Entity* sourceEntity, const char* logMessage)
{
    if (m_playerDodgeRemaining > 0.0f)
    {
        return;
    }

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
