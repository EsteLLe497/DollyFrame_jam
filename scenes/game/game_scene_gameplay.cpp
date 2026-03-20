#include "game_scene_internal.h"
#include "game_scene_combat_system.h"
#include "game_scene_photo_tray_system.h"
#include "photo_system.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPlayerAfterimageLifetime = 0.18f;
    constexpr float kPlayerAfterimageSpawnInterval = 0.03f;
    constexpr int kMaxPlayerAfterimages = 8;
    constexpr float kPlayerVisualSmoothing = 14.0f;
    constexpr float kPlayerLandingDecay = 6.5f;
    constexpr float kPlayerJumpDecay = 5.0f;
    constexpr float kPlayerDodgeDecay = 7.5f;

    struct TuningEntry
    {
        const char* label;
        float* value;
        float step;
        float minValue;
        float maxValue;
    };

    void SetSpriteSheetCell1Based(SpriteRenderComponent& sprite, int row, int column, int rows = 4, int columns = 4)
    {
        const float cellWidth = 1.0f / static_cast<float>(columns);
        const float cellHeight = 1.0f / static_cast<float>(rows);
        sprite.SetSourceRect(
            static_cast<float>(column - 1) * cellWidth,
            static_cast<float>(row - 1) * cellHeight,
            cellWidth,
            cellHeight);
    }

}

void GameScene::UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame)
{
    auto* sprite = player.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    sprite->SetFlipX(false);

    if (landedThisFrame)
    {
        m_player.landingImpact = 1.0f;
    }
    if (!wasGrounded && m_player.velocityY < -80.0f)
    {
        m_player.jumpStretch = 1.0f;
    }
    if (isDodging)
    {
        m_player.dodgeStretch = 1.0f;
    }

    m_player.landingImpact = std::max(0.0f, m_player.landingImpact - deltaTime * kPlayerLandingDecay);
    m_player.jumpStretch = std::max(0.0f, m_player.jumpStretch - deltaTime * kPlayerJumpDecay);
    m_player.dodgeStretch = std::max(0.0f, m_player.dodgeStretch - deltaTime * kPlayerDodgeDecay);

    const float horizontalSpeedRatio = Clamp01(std::fabs(m_player.velocityX) / std::max(1.0f, gPlayerMoveSpeed));
    if (m_player.grounded && horizontalSpeedRatio > 0.05f)
    {
        m_player.runAnimationTime += deltaTime * (2.6f + horizontalSpeedRatio * 5.2f);
    }

    int frameRow = 2;
    int frameColumn = 2;
    if (isDodging)
    {
        frameRow = 3;
        frameColumn = m_player.facingRight ? 2 : 3;
    }
    else if (!m_player.grounded)
    {
        if (m_player.velocityY < -40.0f)
        {
            frameRow = 1;
            frameColumn = m_player.facingRight ? 2 : 4;
        }
        else
        {
            frameRow = 1;
            frameColumn = m_player.facingRight ? 1 : 3;
        }
    }
    else if (horizontalSpeedRatio > 0.20f)
    {
        frameRow = 3;
        frameColumn = m_player.facingRight ? 1 : 4;
    }
    else
    {
        frameRow = 2;
        frameColumn = m_player.facingRight ? 2 : 1;
    }

    SetSpriteSheetCell1Based(*sprite, frameRow, frameColumn);

    float targetScaleX = 1.0f;
    float targetScaleY = 1.0f;
    float targetOffsetY = 0.0f;
    float targetRotation = 0.0f;

    if (m_player.grounded)
    {
        const float runWave = std::sin(m_player.runAnimationTime * 6.2831853f);
        const float runBounce = std::fabs(runWave);
        targetScaleX += runBounce * 0.05f * horizontalSpeedRatio;
        targetScaleY -= runBounce * 0.07f * horizontalSpeedRatio;
        targetOffsetY += runBounce * 1.8f * horizontalSpeedRatio;
        targetRotation += runWave * 0.03f * horizontalSpeedRatio;
        targetRotation += moveAxis * 0.035f;
    }
    else if (m_player.velocityY < 0.0f)
    {
        targetScaleX -= 0.05f;
        targetScaleY += 0.10f;
        targetOffsetY -= 2.0f;
        targetRotation += moveAxis * 0.05f;
    }
    else
    {
        targetScaleX += 0.07f;
        targetScaleY -= 0.06f;
        targetOffsetY += 1.0f;
        targetRotation += moveAxis * 0.04f;
    }

    targetScaleX += m_player.landingImpact * 0.14f;
    targetScaleY -= m_player.landingImpact * 0.18f;
    targetOffsetY += m_player.landingImpact * 3.5f;

    targetScaleX -= m_player.jumpStretch * 0.07f;
    targetScaleY += m_player.jumpStretch * 0.13f;
    targetOffsetY -= m_player.jumpStretch * 2.5f;

    targetScaleX += m_player.dodgeStretch * 0.13f;
    targetScaleY -= m_player.dodgeStretch * 0.10f;
    targetRotation += (m_player.facingRight ? 1.0f : -1.0f) * m_player.dodgeStretch * 0.08f;

    const float blend = std::min(1.0f, deltaTime * kPlayerVisualSmoothing);
    m_player.visualScaleX += (targetScaleX - m_player.visualScaleX) * blend;
    m_player.visualScaleY += (targetScaleY - m_player.visualScaleY) * blend;
    m_player.visualOffsetY += (targetOffsetY - m_player.visualOffsetY) * blend;
    m_player.visualRotation += (targetRotation - m_player.visualRotation) * blend;

    sprite->SetRenderScale(m_player.visualScaleX, m_player.visualScaleY);
    sprite->SetRenderOffset(0.0f, m_player.visualOffsetY);
    sprite->SetRenderRotationOffset(m_player.visualRotation);
}

void GameScene::UpdateTuningPanel()
{
    if (!m_debug.showTuningPanel)
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
        { "Dodge Speed", &gPlayerDodgeSpeed, 10.0f, 0.0f, 1600.0f },
        { "Dodge Dist", &gPlayerDodgeDistance, 4.0f, 0.0f, 480.0f },
        { "Dodge I-Frame", &gPlayerDodgeInvincibilitySeconds, 0.01f, 0.0f, 1.0f },
        { "Coyote", &gCoyoteTimeSeconds, 0.01f, 0.0f, 0.4f },
        { "Ground Snap", &gGroundSnapDistance, 0.5f, 0.0f, 24.0f },
        { "Capture W Tiles", &gCaptureWidthTiles, 0.25f, 1.0f, 16.0f },
        { "Capture H Tiles", &gCaptureHeightTiles, 0.25f, 1.0f, 16.0f },
        { "Print Pad X", &gPrintedPhotoPaddingX, 1.0f, 0.0f, 80.0f },
        { "Print Pad Top", &gPrintedPhotoPaddingTop, 1.0f, 0.0f, 80.0f },
        { "Print Footer", &gPrintedPhotoFooterHeight, 2.0f, 0.0f, 160.0f },
        { "Print Min W", &gPrintedPhotoMinWidth, 4.0f, 32.0f, 320.0f },
        { "Print Min H", &gPrintedPhotoMinHeight, 4.0f, 32.0f, 400.0f },
        { "Matte Inset", &gPrintedPhotoMatteInset, 0.5f, 0.0f, 24.0f },
        { "Pickup Bonus", &gPickupTimeBonus, 1.0f, 0.0f, 60.0f },
    };
    constexpr int kEntryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));

    if (Input_IsActionPressed(InputAction::MoveUp))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + kEntryCount - 1) % kEntryCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown))
    {
        m_debug.tuningSelection = (m_debug.tuningSelection + 1) % kEntryCount;
    }

    float delta = 0.0f;
    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        delta -= entries[m_debug.tuningSelection].step;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        delta += entries[m_debug.tuningSelection].step;
    }

    if (delta != 0.0f)
    {
        *entries[m_debug.tuningSelection].value = std::clamp(
            *entries[m_debug.tuningSelection].value + delta,
            entries[m_debug.tuningSelection].minValue,
            entries[m_debug.tuningSelection].maxValue);
        WriteTuningJsonFile();
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

    const bool blockPlayerInput = m_flow.cameraMode || m_photo.placement.active;

    float moveAxis = 0.0f;
    if (!blockPlayerInput)
    {
        if (Input_IsActionDown(InputAction::MoveLeft)) { moveAxis -= 1.0f; }
        if (Input_IsActionDown(InputAction::MoveRight)) { moveAxis += 1.0f; }
        moveAxis += Input_GetAxis(InputAxis::MoveX);
        moveAxis = std::clamp(moveAxis, -1.0f, 1.0f);
        if (std::fabs(moveAxis) < 0.15f)
        {
            moveAxis = 0.0f;
        }
    }
    else
    {
        moveAxis = 0.0f;
	}

    m_player.dodgeRemaining = std::max(0.0f, m_player.dodgeRemaining - deltaTime);
    m_player.dodgeCooldownRemaining = std::max(0.0f, m_player.dodgeCooldownRemaining - deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    if (moveAxis > 0.1f)
    {
        m_player.facingRight = true;
    }
    else if (moveAxis < -0.1f)
    {
        m_player.facingRight = false;
    }

    const bool dodgePressed = blockPlayerInput?false:Input_IsActionPressed(InputAction::Dodge);
    if (dodgePressed && m_player.dodgeRemaining <= 0.0f && m_player.dodgeCooldownRemaining <= 0.0f)
    {
        m_player.dodgeDirection = moveAxis != 0.0f ? (moveAxis > 0.0f ? 1.0f : -1.0f) : (m_player.facingRight ? 1.0f : -1.0f);
        m_player.facingRight = m_player.dodgeDirection > 0.0f;
        m_player.dodgeRemaining = GetPlayerDodgeDuration();
        m_player.dodgeCooldownRemaining = gPlayerDodgeCooldown;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_player.dodgeRemaining > 0.0f;
    m_player.velocityX = isDodging
        ? m_player.dodgeDirection * gPlayerDodgeSpeed
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
    m_player.grounded = wasGrounded;
    if (wasGrounded)
    {
        m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
    }

    if (wasGrounded && m_player.velocityY > 0.0f)
    {
        m_player.velocityY = 0.0f;
    }

    const bool jumpPressed =blockPlayerInput?false: Input_IsActionPressed(InputAction::Jump);
    const bool canJumpNow = !isDodging && jumpPressed && m_player.coyoteTimeRemaining > 0.0f;
    if (canJumpNow)
    {
        m_player.velocityY = gPlayerJumpSpeed;
        m_player.grounded = false;
        m_player.coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    if (m_player.grounded && !canJumpNow)
    {
        m_player.velocityY = 0.0f;
    }
    else
    {
        m_player.velocityY = std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * deltaTime);
    }
    const float verticalSnapDistance = std::max(gGroundSnapDistance, std::fabs(m_player.velocityY) * deltaTime + 4.0f);

    transform->x += m_player.velocityX * deltaTime;
    if (m_player.velocityX > 0.0f)
    {
        const int column = static_cast<int>((transform->x + playerWidth - 1.0f) / tileSize);
        const int rowStart = static_cast<int>((transform->y + 4.0f) / tileSize);
        const int rowEnd = static_cast<int>((transform->y + playerHeight - 4.0f) / tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (IsSolidTile(column, row))
            {
                transform->x = static_cast<float>(column) * tileSize - playerWidth;
                m_player.velocityX = 0.0f;
                break;
            }
        }
    }
    else if (m_player.velocityX < 0.0f)
    {
        const int column = static_cast<int>(transform->x / tileSize);
        const int rowStart = static_cast<int>((transform->y + 4.0f) / tileSize);
        const int rowEnd = static_cast<int>((transform->y + playerHeight - 4.0f) / tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (IsSolidTile(column, row))
            {
                transform->x = static_cast<float>(column + 1) * tileSize;
                m_player.velocityX = 0.0f;
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
            if (m_player.velocityX > 0.0f && previousX + playerWidth <= photoBoxX + kHorizontalCollisionEpsilon)
            {
                transform->x = photoBoxX - playerWidth;
                m_player.velocityX = 0.0f;
                break;
            }
            if (m_player.velocityX < 0.0f && previousX >= photoBoxX + photoBoxWidth - kHorizontalCollisionEpsilon)
            {
                transform->x = photoBoxX + photoBoxWidth;
                m_player.velocityX = 0.0f;
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
            if (m_player.velocityX > 0.0f && previousX + playerWidth <= photoSourceX + kHorizontalCollisionEpsilon)
            {
                transform->x = photoSourceX - playerWidth;
                m_player.velocityX = 0.0f;
            }
            else if (m_player.velocityX < 0.0f && previousX >= photoSourceX + photoSourceWidth - kHorizontalCollisionEpsilon)
            {
                transform->x = photoSourceX + photoSourceWidth;
                m_player.velocityX = 0.0f;
            }
        }
    }

    m_player.grounded = false;
    if (m_player.velocityY == 0.0f && wasGrounded)
    {
        m_player.grounded = TrySnapToGround(*transform, verticalSnapDistance);
    }
    else
    {
        transform->y += m_player.velocityY * deltaTime;
        if (m_player.velocityY > 0.0f)
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
                        m_player.velocityY = 0.0f;
                        m_player.grounded = true;
                        collided = true;
                        break;
                    }
                }
                if (collided)
                {
                    break;
                }
            }

            if (!m_player.grounded && hasPhotoBox)
            {
                for (const auto& photoBoxBounds : photoBoxes)
                {
                    TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                    playerBounds.scale = transform->scale;
                    if (IntersectsRect(playerBounds, photoBoxBounds) &&
                        previousBottom <= photoBoxBounds.y + kSurfaceContactEpsilon)
                    {
                        transform->y = photoBoxBounds.y - playerHeight;
                        m_player.velocityY = 0.0f;
                        m_player.grounded = true;
                        break;
                    }
                }
            }
            if (!m_player.grounded && hasPhotoSource)
            {
                TransformComponent playerBounds(transform->x, transform->y, transform->width, transform->height);
                playerBounds.scale = transform->scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && previousBottom <= photoSourceY + kSurfaceContactEpsilon)
                {
                    transform->y = photoSourceY - playerHeight;
                    m_player.velocityY = 0.0f;
                    m_player.grounded = true;
                }
            }
        }
        else if (m_player.velocityY < 0.0f)
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
                        m_player.velocityY = 0.0f;
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
                        m_player.velocityY = 0.0f;
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
                    m_player.velocityY = 0.0f;
                }
            }
        }
        if (!m_player.grounded && m_player.velocityY >= 0.0f)
        {
            if (TrySnapToGround(*transform, verticalSnapDistance))
            {
                m_player.velocityY = 0.0f;
                m_player.grounded = true;
            }
        }
    }

    if (transform->y + playerHeight >= mapHeight)
    {
        transform->y = mapHeight - playerHeight;
        m_player.velocityY = 0.0f;
        m_player.grounded = true;
    }

    const bool landedThisFrame = !wasGrounded && m_player.grounded;
    UpdatePlayerPresentation(*player, deltaTime, moveAxis, wasGrounded, isDodging, landedThisFrame);

    const float cameraTarget = std::clamp(
        transform->x + playerWidth * 0.5f - gCameraViewWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - gCameraViewWidth));
    m_flow.cameraX += (cameraTarget - m_flow.cameraX) * std::min(1.0f, deltaTime * 8.0f);
}

void GameScene::UpdatePlayerAfterimages(float deltaTime)
{
    for (auto& afterimage : m_player.afterimages)
    {
        afterimage.life = std::max(0.0f, afterimage.life - deltaTime);
    }
    m_player.afterimages.erase(
        std::remove_if(
            m_player.afterimages.begin(),
            m_player.afterimages.end(),
            [](const PlayerAfterimage& afterimage)
            {
                return afterimage.life <= 0.0f;
            }),
        m_player.afterimages.end());
}

void GameScene::TrySpawnPlayerAfterimage(const TransformComponent& transform)
{
    const bool shouldAddAfterimage =
        m_player.afterimages.empty() ||
        (kPlayerAfterimageLifetime - m_player.afterimages.front().life) >= kPlayerAfterimageSpawnInterval;
    if (!shouldAddAfterimage)
    {
        return;
    }

    PlayerAfterimage afterimage;
    afterimage.x = transform.x;
    afterimage.y = transform.y;
    afterimage.rotation = transform.rotation;
    afterimage.scale = transform.scale;
    afterimage.flipX = m_player.facingRight ? false : true;
    afterimage.life = kPlayerAfterimageLifetime;
    m_player.afterimages.insert(m_player.afterimages.begin(), afterimage);
    if (static_cast<int>(m_player.afterimages.size()) > kMaxPlayerAfterimages)
    {
        m_player.afterimages.resize(kMaxPlayerAfterimages);
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

void GameScene::StoreCapturedPhoto()
{
    int slotToStore = -1;
    for (int index = 0; index < static_cast<int>(m_photo.savedCaptures.size()); ++index)
    {
        if (!m_photo.savedCaptures[index].hasPhoto)
        {
            slotToStore = index;
            break;
        }
    }

    if (slotToStore < 0)
    {
        slotToStore = m_photo.nextCaptureSlot;
    }

    m_photo.savedCaptures[slotToStore] = m_photo.capture;
    m_photo.selectedCaptureSlot = slotToStore;
    m_photo.nextCaptureSlot = (slotToStore + 1) % static_cast<int>(m_photo.savedCaptures.size());
}

void GameScene::SetSelectedPhotoSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    const PhotoCaptureState& storedCapture = m_photo.savedCaptures[slotIndex];
    if (!storedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    m_photo.capture = storedCapture;
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = slotIndex;
}

void GameScene::ConsumeSelectedPhotoSlot()
{
    if (m_photo.selectedCaptureSlot < 0 || m_photo.selectedCaptureSlot >= static_cast<int>(m_photo.savedCaptures.size()))
    {
        return;
    }

    PhotoCaptureState& selectedCapture = m_photo.savedCaptures[m_photo.selectedCaptureSlot];
    if (!selectedCapture.hasPhoto)
    {
        return;
    }

    const PhotoFilterTheme selectedTheme = m_photo.capture.selectedTheme;
    selectedCapture = PhotoCaptureState{};

    for (int offset = 1; offset <= static_cast<int>(m_photo.savedCaptures.size()); ++offset)
    {
        const int slotIndex = (m_photo.selectedCaptureSlot + offset) % static_cast<int>(m_photo.savedCaptures.size());
        if (!m_photo.savedCaptures[slotIndex].hasPhoto)
        {
            continue;
        }

        SetSelectedPhotoSlot(slotIndex);
        m_photo.capture.selectedTheme = selectedTheme;
        return;
    }

    m_photo.capture = PhotoCaptureState{};
    m_photo.capture.selectedTheme = selectedTheme;
    m_photo.selectedCaptureSlot = 0;
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
}

void GameScene::UpdatePhotoTraySelection()
{
    game_scene_photo_tray_system::UpdateSelection(
        m_photo,
        m_flow.photoTrayReveal,
        [this](int slotIndex)
        {
            SetSelectedPhotoSlot(slotIndex);
        });
}

void GameScene::UpdateEnemies()
{
    Entity* player = FindEntityByTag("Player");
    const TransformComponent* playerTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    game_scene_combat_system::UpdateEnemies(
        m_entities,
        m_tileTexture,
        m_flow,
        m_photo,
        playerTransform);
}

void GameScene::UpdateBullets()
{
    Entity* player = FindEntityByTag("Player");
    game_scene_combat_system::UpdateBullets(
        m_entities,
        GetMapPixelWidth(),
        GetMapPixelHeight(),
        m_flow.lastDeltaTime,
        player,
        [this](const Entity& a, const Entity& b)
        {
            return IntersectsEntity(a, b);
        },
        [this](Entity& playerEntity, Entity* sourceEntity, const char* logMessage)
        {
            HandlePlayerDamage(playerEntity, sourceEntity, logMessage);
        });
}
void GameScene::HandleAttackHits()
{
    return;
}

void GameScene::UpdateGoalVisual(float deltaTime)
{
    m_flow.goalPulse += deltaTime;
    if (Entity* goal = FindEntityByTag("Goal"))
    {
        if (auto* tint = goal->GetComponent<TintComponent>())
        {
            const float pulse = 0.65f + 0.35f * std::sin(m_flow.goalPulse * 3.2f);
            if (m_flow.goalUnlocked)
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

    m_flow.playerTouchingHazard = false;
    m_flow.playerTouchingTarget = false;

    HandleWorldTileInteractions(*player);

    std::vector<Entity*> consumedGimmicks;
    HandleWorldEntityInteractions(*player, consumedGimmicks);

    std::vector<Entity*> consumedPickups;
    std::vector<Entity*> defeatedEnemies;
    HandlePhotoBoxInteractions(*player, consumedPickups, defeatedEnemies);

    std::vector<Entity*> entitiesToRemove = consumedGimmicks;
    entitiesToRemove.insert(entitiesToRemove.end(), consumedPickups.begin(), consumedPickups.end());
    RemoveEntitiesByPointerList(entitiesToRemove);

    if (!defeatedEnemies.empty())
    {
        m_eventBus.Publish({ EventType::LogMessage, player, defeatedEnemies.front(), "Invert photo neutralized an enemy", 0.0f, 0.0f });
    }
}

void GameScene::HandleWorldTileInteractions(Entity& player)
{
    auto* playerTransform = player.GetComponent<TransformComponent>();
    if (!playerTransform)
    {
        return;
    }

    if (IntersectsHazardTile(*playerTransform))
    {
        m_flow.playerTouchingHazard = true;
        HandlePlayerDamage(player, nullptr, "GameScene player damaged by hazard tile");
    }

    if (m_flow.goalUnlocked && IntersectsGoalTile(*playerTransform))
    {
        m_flow.playerTouchingTarget = true;
        if (!m_flow.resultQueued)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, nullptr, "contact_tone", 0.0f, 0.0f });
            QueueResult(GameEndReason::GoalReached);
        }
    }
}

void GameScene::HandleWorldEntityInteractions(Entity& player, std::vector<Entity*>& consumedGimmicks)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == &player || !IntersectsEntity(player, *entity))
        {
            continue;
        }

        if (const auto* enemy = entity->GetComponent<EnemyComponent>())
        {
            if (enemy->IsEnabled())
            {
                m_flow.playerTouchingHazard = true;
                HandlePlayerDamage(player, entity.get(), "GameScene player damaged by enemy");
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
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by gimmick hazard");
            break;
        case GimmickType::Goal:
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case GimmickType::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up gimmick item", 0.0f, 0.0f });
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
}

void GameScene::HandlePhotoBoxInteractions(Entity& player, std::vector<Entity*>& consumedPickups, std::vector<Entity*>& defeatedEnemies)
{
    for (const auto& entity : m_entities)
    {
        if (!entity || !HasTag(*entity, "PhotoBox"))
        {
            continue;
        }

        const auto* photoRole = entity->GetComponent<PhotoCopyRoleComponent>();
        const auto* photoLayer = entity->GetComponent<PhotoCopyLayerComponent>();
        if (!photoRole || !IntersectsEntity(player, *entity))
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
            m_flow.playerTouchingHazard = true;
            HandlePlayerDamage(player, entity.get(), "GameScene player damaged by copied hazard");
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
            if (m_flow.goalUnlocked && !m_flow.resultQueued)
            {
                m_flow.playerTouchingTarget = true;
                m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "contact_tone", 0.0f, 0.0f });
                QueueResult(GameEndReason::GoalReached);
            }
            break;
        case PhotoCopyRole::Pickup:
            m_eventBus.Publish({ EventType::PlaySoundRequest, &player, entity.get(), "scene_change", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, &player, entity.get(), "Picked up copied item", 0.0f, 0.0f });
            consumedPickups.push_back(entity.get());
            break;
        case PhotoCopyRole::Solid:
        default:
            break;
        }
    }
}

void GameScene::RemoveEntitiesByPointerList(const std::vector<Entity*>& entitiesToRemove)
{
    if (entitiesToRemove.empty())
    {
        return;
    }

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [&](const std::unique_ptr<Entity>& entity)
            {
                return entity && std::find(entitiesToRemove.begin(), entitiesToRemove.end(), entity.get()) != entitiesToRemove.end();
            }),
        m_entities.end());
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

    RefreshPhotoGroupState();
}

void GameScene::RefreshPhotoGroupState()
{
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
    const float dodgeDuration = GetPlayerDodgeDuration();
    const float dodgeElapsed = std::max(0.0f, dodgeDuration - m_player.dodgeRemaining);
    const float dodgeInvincibility = std::min(gPlayerDodgeInvincibilitySeconds, dodgeDuration);
    if (m_player.dodgeRemaining > 0.0f && dodgeElapsed < dodgeInvincibility)
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
    if (health->IsDead() && !m_flow.resultQueued)
    {
        QueueResult(GameEndReason::HpZero);
    }
}

void GameScene::QueueResult(GameEndReason reason)
{
    if (m_flow.resultQueued)
    {
        return;
    }

    m_flow.resultQueued = true;
    GameSession_SetEndReason(reason);
    m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "result", 0.0f, 0.0f });
}
