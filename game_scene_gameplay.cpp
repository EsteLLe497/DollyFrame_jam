#include "game_scene_internal.h"

#include <fstream>

#include <nlohmann/json.hpp>

using namespace game_scene_detail;

namespace
{
    struct TuningEntry
    {
        const char* label;
        float* value;
        float step;
        float minValue;
        float maxValue;
    };

    PhotoCopyLayer CyclePlacementLayer(PhotoCopyLayer current)
    {
        switch (current)
        {
        case PhotoCopyLayer::Foreground:
            return PhotoCopyLayer::Background;
        case PhotoCopyLayer::Background:
            return PhotoCopyLayer::Shadow;
        case PhotoCopyLayer::Shadow:
        default:
            return PhotoCopyLayer::Foreground;
        }
    }

    void AddBridgeSegments(std::vector<CapturedPhotoItem>& items, int textureId, bool flipX, bool enabled, PhotoFilterTheme theme)
    {
        if (!enabled || items.size() < 2)
        {
            return;
        }

        const std::vector<CapturedPhotoItem> baseItems = items;
        constexpr float kSegmentSize = 18.0f;
        for (size_t index = 1; index < baseItems.size(); ++index)
        {
            const auto& a = baseItems[index - 1];
            const auto& b = baseItems[index];
            const float ax = a.relativeX + a.width * 0.5f;
            const float ay = a.relativeY + a.height * 0.5f;
            const float bx = b.relativeX + b.width * 0.5f;
            const float by = b.relativeY + b.height * 0.5f;
            const float length = std::max(std::fabs(bx - ax), std::fabs(by - ay));
            const int steps = std::max(1, static_cast<int>(length / kSegmentSize));
            for (int step = 1; step < steps; ++step)
            {
                const float t = static_cast<float>(step) / static_cast<float>(steps);
                CapturedPhotoItem bridge;
                bridge.textureId = textureId;
                bridge.role = PhotoCopyRole::Solid;
                bridge.layer = PhotoCopyLayer::Foreground;
                bridge.appliedTheme = theme;
                bridge.relativeX = std::lerp(ax, bx, t) - kSegmentSize * 0.5f;
                bridge.relativeY = std::lerp(ay, by, t) - kSegmentSize * 0.5f;
                bridge.width = kSegmentSize;
                bridge.height = kSegmentSize;
                bridge.sourceX = 0.0f;
                bridge.sourceY = 0.0f;
                bridge.sourceWidth = 1.0f;
                bridge.sourceHeight = 1.0f;
                bridge.tintR = 0.90f;
                bridge.tintG = 0.96f;
                bridge.tintB = 1.0f;
                bridge.tintA = 0.92f;
                bridge.flipX = flipX;
                items.push_back(bridge);
            }
        }
    }

    constexpr int kMaxPhotoGroups = 3;

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

    m_playerVelocityX = moveAxis * gPlayerMoveSpeed;
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
    const bool canJumpNow = jumpPressed && m_coyoteTimeRemaining > 0.0f;
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
        item.role = GetEntityCopyRole(*entity);
        item.layer = PhotoCopyLayer::Foreground;
        item.origin = GetEntityCopyOrigin(*entity);
        item.appliedTheme = m_selectedFilterTheme;
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
            item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
            item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
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
            item.role = GetTileCopyRole(tileValue);
            item.layer = PhotoCopyLayer::Foreground;
            item.origin = GetTileCopyOrigin(tileValue);
            item.appliedTheme = m_selectedFilterTheme;
            item.relativeX = overlapLeft - frameX;
            item.relativeY = overlapTop - frameY;
            item.width = overlapWidth;
            item.height = overlapHeight;
            item.sourceX = 0.0f;
            item.sourceY = 0.0f;
            item.sourceWidth = 1.0f;
            item.sourceHeight = 1.0f;
            GetTileCaptureTint(tileValue, item.tintR, item.tintG, item.tintB, item.tintA);
            item.role = GetRoleFromTint(item.tintR, item.tintG, item.tintB);
            item.layer = GetLayerFromTint(item.tintR, item.tintG, item.tintB);
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
    m_capturedPhotoTheme = m_selectedFilterTheme;
    m_photoPlacementLayer = PhotoCopyLayer::Foreground;
    m_photoPlacementFlipX = false;
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
    switch (m_capturedPhotoTheme)
    {
    case PhotoFilterTheme::Hot:
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Captured framed objects with Hot filter", 0.0f, 0.0f });
        break;
    case PhotoFilterTheme::Cold:
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Captured framed objects with Cold filter", 0.0f, 0.0f });
        break;
    case PhotoFilterTheme::Invert:
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Captured framed objects with Invert filter", 0.0f, 0.0f });
        break;
    case PhotoFilterTheme::None:
    default:
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Captured framed objects with no filter", 0.0f, 0.0f });
        break;
    }
    m_shutterFlashRemaining = gShutterFlashSeconds;
}

void GameScene::HandlePhotoSpawn()
{
    m_photoPlacementActive = false;
    m_photoPlacementValid = false;

    if (!m_hasBoxPhoto || !Input_IsKeyDown('E'))
    {
        return;
    }

    if (Input_IsKeyPressed('Q'))
    {
        m_photoPlacementLayer = CyclePlacementLayer(m_photoPlacementLayer);
    }
    if (Input_IsKeyPressed('F'))
    {
        m_photoPlacementFlipX = !m_photoPlacementFlipX;
    }
    if (Input_IsKeyPressed('B'))
    {
        m_photoPlacementBridgeEnabled = !m_photoPlacementBridgeEnabled;
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

    std::vector<CapturedPhotoItem> spawnedItems = m_capturedPhotoItems;
    if (m_photoPlacementFlipX)
    {
        for (auto& item : spawnedItems)
        {
            item.relativeX = spawnWidth - item.relativeX - item.width;
            item.flipX = !item.flipX;
        }
    }
    AddBridgeSegments(spawnedItems, m_whiteTexture, m_photoPlacementFlipX, m_photoPlacementBridgeEnabled, m_capturedPhotoTheme);

    if (m_activePhotoGroupCount >= kMaxPhotoGroups)
    {
        const int groupToRemove = m_nextPhotoGroupId - m_activePhotoGroupCount;
        m_entities.erase(
            std::remove_if(
                m_entities.begin(),
                m_entities.end(),
                [&](const std::unique_ptr<Entity>& entity)
                {
                    if (!entity || !HasTag(*entity, "PhotoBox"))
                    {
                        return false;
                    }

                    const auto* group = entity->GetComponent<PhotoCopyGroupComponent>();
                    return group && group->groupId == groupToRemove;
                }),
            m_entities.end());
        m_activePhotoGroupCount = std::max(0, m_activePhotoGroupCount - 1);
    }

    const int groupId = m_nextPhotoGroupId++;
    Entity* lastSpawnedBox = nullptr;
    for (const auto& item : spawnedItems)
    {
        auto entity = std::make_unique<Entity>();
        lastSpawnedBox = entity.get();
        lastSpawnedBox->AddComponent<TagComponent>("PhotoBox");
        lastSpawnedBox->AddComponent<PhotoCopyGroupComponent>(groupId);
        lastSpawnedBox->AddComponent<PhotoCopyRoleComponent>(item.role);
        lastSpawnedBox->AddComponent<PhotoCopyOriginComponent>(item.origin);
        lastSpawnedBox->AddComponent<PhotoCopyEffectComponent>(item.appliedTheme);
        lastSpawnedBox->AddComponent<PhotoCopyLayerComponent>(
            item.layer == PhotoCopyLayer::Shadow ? PhotoCopyLayer::Shadow : m_photoPlacementLayer);
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
            sprite->SetFlipX(item.flipX);
        }
        ApplyPhotoFilterTheme(*lastSpawnedBox, item.appliedTheme);
        m_entities.push_back(std::move(entity));
    }

    m_activePhotoGroupCount = std::min(kMaxPhotoGroups, m_activePhotoGroupCount + 1);
    m_photoBoxSpawned = true;
    m_eventBus.Publish({ EventType::PlaySoundRequest, player, lastSpawnedBox, "test_tone", 0.0f, 0.0f });
    m_eventBus.Publish({ EventType::LogMessage, player, lastSpawnedBox, "Spawned filtered reconstruction", 0.0f, 0.0f });
}

void GameScene::UpdateEnemies()
{
    m_enemyCount = 0;
    Entity* player = FindEntityByTag("Player");
    const TransformComponent* playerTransform =
        player ? player->GetComponent<TransformComponent>() : nullptr;

    // 3/19追加：ループ中にm_entitiesを変更しないための一時バッファ(田之上俊)
    std::vector<std::unique_ptr<Entity>> newBullets;

    for (const auto& entity : m_entities)
    {
        if (!entity) continue;
        auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled()) continue;
        ++m_enemyCount;

        if (!playerTransform) continue;
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform) continue;

        if (enemy->GetArchetype() == EnemyArchetype::Walker)
        {
            // Walker処理はそのまま
            const float dx = playerTransform->x - transform->x;
            const float dist = std::fabs(dx);
            constexpr float kWalkerSpeed = 120.0f;

            switch (enemy->GetAIState())
            {
            case EnemyComponent::AIState::Idle:
                if (dist < enemy->detectRange)
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                break;
            case EnemyComponent::AIState::Chase:
                if (dist < enemy->attackRange)
                {
                    enemy->attackTimer = 0.0f;
                    enemy->SetAIState(EnemyComponent::AIState::Attack);
                }
                else if (dist > enemy->detectRange)
                {
                    enemy->SetAIState(EnemyComponent::AIState::Idle);
                }
                else
                {
                    transform->x += (dx > 0.0f ? 1.0f : -1.0f) * kWalkerSpeed * m_lastDeltaTime;
                }
                break;
            case EnemyComponent::AIState::Attack:
                enemy->attackTimer += m_lastDeltaTime;
                if (dist >= enemy->attackRange)
                    enemy->SetAIState(EnemyComponent::AIState::Chase);
                else if (enemy->attackTimer >= enemy->attackCooldown)
                    enemy->attackTimer = 0.0f;
                break;
            }
        }
        else if (enemy->GetArchetype() == EnemyArchetype::Ranged)
        {
            const float dx = playerTransform->x - transform->x;
            const float dy = playerTransform->y - transform->y;
            const float dist = std::sqrt(dx * dx + dy * dy);

            enemy->attackTimer += m_lastDeltaTime;

            if (dist < enemy->detectRange && enemy->attackTimer >= enemy->attackCooldown)
            {
                enemy->attackTimer = 0.0f;

                constexpr float kBulletSpeed = 300.0f;
                const float length = std::max(1.0f, dist);
                const float velX = (dx / length) * kBulletSpeed;
                const float velY = (dy / length) * kBulletSpeed;

                // 3/19修正：一時バッファに追加(田之上俊)
                auto bullet = std::make_unique<Entity>();
                bullet->AddComponent<TagComponent>("Bullet");
                bullet->AddComponent<TransformComponent>(
                    transform->x + 24.0f,
                    transform->y + 24.0f,
                    16.0f, 16.0f);
                bullet->AddComponent<TintComponent>(1.0f, 0.9f, 0.2f, 1.0f);
                bullet->AddComponent<SpriteRenderComponent>(m_tileTexture);
                bullet->AddComponent<ProjectileComponent>(velX, velY, 1);
                newBullets.push_back(std::move(bullet));
            }
        }
    }

    // ループ後にまとめて追加
    for (auto& bullet : newBullets)
    {
        m_entities.push_back(std::move(bullet));
    }

    m_goalUnlocked = m_photoBoxSpawned;
}

// 3/19追加：弾の移動と画面外削除(田之上俊)
void GameScene::UpdateBullets()
{
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    Entity* player = FindEntityByTag("Player");

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [&](const std::unique_ptr<Entity>& entity) -> bool
            {
                if (!entity || !HasTag(*entity, "Bullet")) return false;

                auto* transform = entity->GetComponent<TransformComponent>();
                auto* projectile = entity->GetComponent<ProjectileComponent>();
                if (!transform || !projectile) return false;

                transform->x += projectile->GetVelocityX() * m_lastDeltaTime;
                transform->y += projectile->GetVelocityY() * m_lastDeltaTime;

                // 3/19追加：プレイヤーへの当たり判定とダメージ(田之上俊)
                if (player && IntersectsEntity(*player, *entity))
                {
                    HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by bullet");
                    return true; // 当たったら削除
                }

                // 画面外で削除
                return transform->x < 0.0f
                    || transform->x > mapWidth
                    || transform->y < 0.0f
                    || transform->y > mapHeight;
            }),
        m_entities.end());
}

bool GameScene::ApplyPhotoFilterTheme(Entity& photoBox, PhotoFilterTheme theme)
{
    auto* role = photoBox.GetComponent<PhotoCopyRoleComponent>();
    auto* layer = photoBox.GetComponent<PhotoCopyLayerComponent>();
    auto* tint = photoBox.GetComponent<TintComponent>();
    auto* effect = photoBox.GetComponent<PhotoCopyEffectComponent>();
    const auto* origin = photoBox.GetComponent<PhotoCopyOriginComponent>();
    if (!role || !layer || !tint || !effect)
    {
        return false;
    }

    PhotoCopyRole nextRole = role->role;
    PhotoCopyLayer nextLayer = layer->layer;
    float nextTintR = tint->r;
    float nextTintG = tint->g;
    float nextTintB = tint->b;
    float nextTintA = tint->a;

    switch (theme)
    {
    case PhotoFilterTheme::Hot:
        nextRole = PhotoCopyRole::Hazard;
        nextLayer = PhotoCopyLayer::Foreground;
        nextTintR = 1.0f;
        nextTintG = 0.34f;
        nextTintB = 0.12f;
        nextTintA = 1.0f;
        break;
    case PhotoFilterTheme::Cold:
        nextRole = PhotoCopyRole::Solid;
        nextLayer = PhotoCopyLayer::Foreground;
        nextTintR = 0.76f;
        nextTintG = 0.90f;
        nextTintB = 1.0f;
        nextTintA = 1.0f;
        break;
    case PhotoFilterTheme::Invert:
        nextRole = origin && origin->origin == PhotoCopyOrigin::Enemy
            ? PhotoCopyRole::Ally
            : PhotoCopyRole::Solid;
        nextLayer = PhotoCopyLayer::Foreground;
        nextTintR = 0.62f;
        nextTintG = 0.62f;
        nextTintB = 0.64f;
        nextTintA = 1.0f;
        break;
    case PhotoFilterTheme::None:
    default:
        break;
    }

    const bool changed =
        role->role != nextRole ||
        layer->layer != nextLayer ||
        effect->GetTheme() != theme ||
        std::fabs(tint->r - nextTintR) > 0.001f ||
        std::fabs(tint->g - nextTintG) > 0.001f ||
        std::fabs(tint->b - nextTintB) > 0.001f ||
        std::fabs(tint->a - nextTintA) > 0.001f;

    role->role = nextRole;
    layer->layer = nextLayer;
    effect->SetTheme(theme);
    tint->r = nextTintR;
    tint->g = nextTintG;
    tint->b = nextTintB;
    tint->a = nextTintA;
    return changed;
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
                return enemy && enemy->IsDefeated();
            }),
        m_entities.end());

    m_photoBoxSpawned = FindEntityByTag("PhotoBox") != nullptr;
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
    m_activePhotoGroupCount = static_cast<int>(groups.size());
    m_nextPhotoGroupId = std::max(m_nextPhotoGroupId, maxGroupId + 1);
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
