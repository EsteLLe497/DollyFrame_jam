#include "game_scene_internal.h"

using namespace game_scene_detail;

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
    if (!player)
    {
        return;
    }

    const auto* playerTransform = player->GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    float frameX = 0.0f;
    float frameY = 0.0f;
    float frameWidth = 0.0f;
    float frameHeight = 0.0f;
    GetCaptureFrameRect(*playerTransform, frameX, frameY, frameWidth, frameHeight);

    m_capturedPhotoItems.clear();
    float capturedMaxRight = 0.0f;
    float capturedMaxBottom = 0.0f;

    for (const auto& entity : m_entities)
    {
        if (!entity || HasTag(*entity, "Player") || HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* targetTransform = entity->GetComponent<TransformComponent>();
        const auto* sprite = entity->GetComponent<SpriteRenderComponent>();
        if (!targetTransform || !sprite)
        {
            continue;
        }

        const float targetX = targetTransform->x;
        const float targetY = targetTransform->y;
        const float targetWidth = targetTransform->width * targetTransform->scale;
        const float targetHeight = targetTransform->height * targetTransform->scale;
        const float overlapLeft = (std::max)(frameX, targetX);
        const float overlapTop = (std::max)(frameY, targetY);
        const float overlapRight = (std::min)(frameX + frameWidth, targetX + targetWidth);
        const float overlapBottom = (std::min)(frameY + frameHeight, targetY + targetHeight);
        const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
        const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
        if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
        {
            continue;
        }

        const float localLeft = (overlapLeft - targetX) / targetWidth;
        const float localTop = (overlapTop - targetY) / targetHeight;
        const float localWidth = overlapWidth / targetWidth;
        const float localHeight = overlapHeight / targetHeight;

        CapturedPhotoItem item;
        item.textureId = sprite->GetTextureId();
        item.relativeX = overlapLeft - frameX;
        item.relativeY = overlapTop - frameY;
        item.width = overlapWidth;
        item.height = overlapHeight;
        item.sourceX = sprite->GetSourceX() + sprite->GetSourceWidth() * localLeft;
        item.sourceY = sprite->GetSourceY() + sprite->GetSourceHeight() * localTop;
        item.sourceWidth = sprite->GetSourceWidth() * localWidth;
        item.sourceHeight = sprite->GetSourceHeight() * localHeight;
        if (auto* tint = entity->GetComponent<TintComponent>())
        {
            item.tintR = tint->r;
            item.tintG = tint->g;
            item.tintB = tint->b;
            item.tintA = tint->a;
            tint->r = 0.16f;
            tint->g = 0.34f;
            tint->b = 0.38f;
            tint->a = 0.55f;
        }
        m_capturedPhotoItems.push_back(item);
        capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
        capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
    }

    const float tileSize = m_tileMap.GetTileSize();
    const int leftColumn = std::max(0, static_cast<int>(frameX / tileSize));
    const int rightColumn = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((frameX + frameWidth - 1.0f) / tileSize));
    const int topRow = std::max(0, static_cast<int>(frameY / tileSize));
    const int bottomRow = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((frameY + frameHeight - 1.0f) / tileSize));

    for (int row = topRow; row <= bottomRow; ++row)
    {
        for (int column = leftColumn; column <= rightColumn; ++column)
        {
            const int tileValue = m_tileMap.GetTile(column, row);
            if (tileValue <= 0)
            {
                continue;
            }

            const float tileX = static_cast<float>(column) * tileSize;
            const float tileY = static_cast<float>(row) * tileSize;
            const float overlapLeft = (std::max)(frameX, tileX);
            const float overlapTop = (std::max)(frameY, tileY);
            const float overlapRight = (std::min)(frameX + frameWidth, tileX + tileSize);
            const float overlapBottom = (std::min)(frameY + frameHeight, tileY + tileSize);
            const float overlapWidth = (std::max)(0.0f, overlapRight - overlapLeft);
            const float overlapHeight = (std::max)(0.0f, overlapBottom - overlapTop);
            if (overlapWidth <= 1.0f || overlapHeight <= 1.0f)
            {
                continue;
            }

            CapturedPhotoItem item;
            item.textureId = m_tileTexture;
            item.relativeX = overlapLeft - frameX;
            item.relativeY = overlapTop - frameY;
            item.width = overlapWidth;
            item.height = overlapHeight;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            GetTileCaptureTint(tileValue, item.tintR, item.tintG, item.tintB, item.tintA);
            m_capturedPhotoItems.push_back(item);
            capturedMaxRight = (std::max)(capturedMaxRight, item.relativeX + item.width);
            capturedMaxBottom = (std::max)(capturedMaxBottom, item.relativeY + item.height);
        }
    }

    if (m_capturedPhotoItems.empty())
    {
        return;
    }

    m_hasBoxPhoto = true;
    m_capturedPhotoWidth = (std::max)(1.0f, capturedMaxRight);
    m_capturedPhotoHeight = (std::max)(1.0f, capturedMaxBottom);
    m_capturedTextureId = m_capturedPhotoItems.front().textureId;
    m_capturedSourceX = m_capturedPhotoItems.front().sourceX;
    m_capturedSourceY = m_capturedPhotoItems.front().sourceY;
    m_capturedSourceWidth = m_capturedPhotoItems.front().sourceWidth;
    m_capturedSourceHeight = m_capturedPhotoItems.front().sourceHeight;
    m_capturedTintR = m_capturedPhotoItems.front().tintR;
    m_capturedTintG = m_capturedPhotoItems.front().tintG;
    m_capturedTintB = m_capturedPhotoItems.front().tintB;
    m_capturedTintA = m_capturedPhotoItems.front().tintA;

    m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "scene_change", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Captured framed objects", 0.0f, 0.0f });
    m_shutterFlashRemaining = kShutterFlashSeconds;
}

void GameScene::HandlePhotoSpawn()
{
    m_photoPlacementActive = false;
    m_photoPlacementValid = false;

    if (!m_hasBoxPhoto || !Input_IsKeyDown('E'))
    {
        return;
    }

    Entity* player = FindEntityByTag("Player");
    if (!player)
    {
        return;
    }

    const float spawnWidth = std::max(32.0f, m_capturedPhotoWidth);
    const float spawnHeight = std::max(32.0f, m_capturedPhotoHeight);
    const float viewScale = GetViewScale();
    const float viewOriginX = GetViewOriginX();
    const float viewOriginY = GetViewOriginY();
    const float cursorWorldX =
        ((static_cast<float>(Input_GetMouseX()) - viewOriginX) / viewScale) + m_cameraX;
    const float cursorWorldY =
        (static_cast<float>(Input_GetMouseY()) - viewOriginY) / viewScale;

    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float spawnX = std::clamp(cursorWorldX - spawnWidth * 0.5f, 0.0f, std::max(0.0f, mapWidth - spawnWidth));
    const float spawnY = std::clamp(cursorWorldY - spawnHeight * 0.5f, 0.0f, std::max(0.0f, mapHeight - spawnHeight));

    m_photoPlacementActive = true;
    m_photoPlacementX = spawnX;
    m_photoPlacementY = spawnY;
    m_photoPlacementWidth = spawnWidth;
    m_photoPlacementHeight = spawnHeight;
    m_photoPlacementValid = IsPhotoPlacementValid(spawnX, spawnY, spawnWidth, spawnHeight);

    if (!m_photoPlacementValid || !Input_IsMouseLeftPressed())
    {
        return;
    }

    for (auto it = m_entities.begin(); it != m_entities.end();)
    {
        if (*it && HasTag(*(*it), "PhotoBox"))
        {
            it = m_entities.erase(it);
            continue;
        }
        ++it;
    }

    Entity* lastSpawnedBox = nullptr;
    for (const auto& item : m_capturedPhotoItems)
    {
        auto entity = std::make_unique<Entity>();
        lastSpawnedBox = entity.get();
        lastSpawnedBox->AddComponent<TagComponent>("PhotoBox");
        lastSpawnedBox->AddComponent<TransformComponent>(
            spawnX + item.relativeX,
            spawnY + item.relativeY,
            item.width,
            item.height);
        lastSpawnedBox->AddComponent<TintComponent>(item.tintR, item.tintG, item.tintB, item.tintA);
        lastSpawnedBox->AddComponent<SpriteRenderComponent>(item.textureId >= 0 ? item.textureId : m_tileTexture);
        if (auto* sprite = lastSpawnedBox->GetComponent<SpriteRenderComponent>())
        {
            sprite->SetSourceRect(item.sourceX, item.sourceY, item.sourceWidth, item.sourceHeight);
        }
        m_entities.push_back(std::move(entity));
    }

    m_photoBoxSpawned = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, lastSpawnedBox, "test_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, lastSpawnedBox, "Spawned reconstructed objects", 0.0f, 0.0f });
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
