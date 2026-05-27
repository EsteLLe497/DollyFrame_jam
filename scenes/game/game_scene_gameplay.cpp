#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_combat_system.h"
#include "game_scene_player_system.h"
#include "game_scene_player_movement_system.h"
#include "game_scene_player_visual_system.h"
#include "game_scene_photo_tray_system.h"
#include "photo_system.h"

#include <unordered_map>

#include "DxLib.h"
#include <game_scene_player_visual_system.h>

using namespace game_scene_detail;

namespace
{
    constexpr float kFloorCameraWidth = 1920.0f;
    constexpr float kCameraFollowTargetTilesX = 23.0f;
    constexpr float kCameraOffsetTilesY = -2.0f;
    constexpr float kFixedLockExitMargin = 24.0f;
    constexpr float kBarrelDebrisLifetime = 0.55f;
    constexpr float kBarrelRespawnOffscreenMargin = 64.0f;
    constexpr float kTuningPanelX = 24.0f;
    constexpr float kTuningPanelY = 24.0f;
    constexpr float kTuningPanelWidth = 460.0f;
    constexpr float kTuningPanelHeight = 620.0f;
    constexpr float kTuningRowStartY = 124.0f;
    constexpr float kTuningRowHeight = 22.0f;
    constexpr float kTuningSectionGap = 14.0f;
    constexpr float kTuningSectionHeaderHeight = 24.0f;
    constexpr float kTuningMinusButtonX = 314.0f;
    constexpr float kTuningPlusButtonX = 390.0f;
    constexpr float kTuningButtonWidth = 52.0f;
    constexpr float kTuningButtonHeight = 18.0f;
    struct TuningRowLayout
    {
        float y;
        bool isSectionHeader;
    };

    TuningRowLayout GetTuningRowLayout(int index)
    {
        float y = kTuningPanelY + kTuningRowStartY;
        if (index >= 2)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        if (index >= 12)
        {
            y += kTuningSectionHeaderHeight + kTuningSectionGap;
        }
        y += static_cast<float>(index) * kTuningRowHeight;
        return { y, false };
    }

    bool IsPointInside(float pointX, float pointY, float x, float y, float width, float height)
    {
        return pointX >= x && pointX <= x + width && pointY >= y && pointY <= y + height;
    }

    float EaseInOutCubic(float t)
    {
        const float clamped = Clamp01(t);
        if (clamped < 0.5f)
        {
            return 4.0f * clamped * clamped * clamped;
        }

        const float f = -2.0f * clamped + 2.0f;
        return 1.0f - (f * f * f) * 0.5f;
    }

    bool IsVerticalLaserDirection(LaserTurretFireDirection direction)
    {
        return direction == LaserTurretFireDirection::Up ||
            direction == LaserTurretFireDirection::Down;
    }

    float GetCameraFollowSpanX(const TileMap& tileMap)
    {
        const float tileSize = tileMap.GetTileSize();
        if (tileSize <= 0.0f)
        {
            return gCameraViewWidth;
        }

        return std::min(gCameraViewWidth, tileSize * kCameraFollowTargetTilesX);
    }

    float GetCameraFollowOffsetY(const TileMap& tileMap)
    {
        return std::max(0.0f, tileMap.GetTileSize()) * kCameraOffsetTilesY;
    }

    float GetCameraVisibleHeight(const TileMap& tileMap)
    {
        const float visibleWidth = GetCameraFollowSpanX(tileMap);
        const float safeViewWidth = std::max(1.0f, gCameraViewWidth);
        const float aspect = gCameraViewHeight / safeViewWidth;
        return std::max(1.0f, visibleWidth * aspect);
    }
}

bool GameScene::TryGetFixedCameraByPlayerPosition(float playerCenterX, float playerCenterY, float& outCameraX, float& outCameraY) const
{
    (void)playerCenterY;
    for (const CameraFixedRange& range : m_cameraFixedRanges)
    {
        if (playerCenterX >= range.startX && playerCenterX <= range.endX)
        {
            outCameraX = range.cameraX;
            outCameraY = range.cameraY;
            return true;
        }
    }

    return false;
}

void GameScene::StartFloorCameraTransition(int directionX, int directionY)
{
    static_cast<void>(directionY);
    if (directionX == 0)
    {
        return;
    }

    const float maxCameraX = std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth);
    const float targetX = std::clamp(
        m_flow.cameraX + static_cast<float>(directionX) * kFloorCameraWidth,
        0.0f,
        maxCameraX);
    // T marker transition: keep current Y, move only one floor on X.
    const float targetY = m_flow.cameraY;
    if (targetX == m_flow.cameraX && targetY == m_flow.cameraY)
    {
        return;
    }

    m_floorCameraTransitionActive = true;
    m_floorCameraTransitionElapsed = 0.0f;
    m_floorCameraTransitionStartX = m_flow.cameraX;
    m_floorCameraTransitionStartY = m_flow.cameraY;
    m_floorCameraTransitionTargetX = targetX;
    m_floorCameraTransitionTargetY = targetY;
}

void GameScene::UpdateCameraByMarkers(const TransformComponent& playerTransform, float deltaTime)
{
    const float playerWidth = playerTransform.width * playerTransform.scale;
    const float playerHeight = playerTransform.height * playerTransform.scale;
    const float playerCenterX = playerTransform.x + playerWidth * 0.5f;
    const float playerCenterY = playerTransform.y + playerHeight * 0.5f;
    const float followSpanX = GetCameraFollowSpanX(m_tileMap);
    const float visibleHeight = GetCameraVisibleHeight(m_tileMap);

    if (!m_hasPreviousPlayerCameraProbe)
    {
        m_hasPreviousPlayerCameraProbe = true;
        m_previousPlayerCameraProbeX = playerCenterX;
        m_previousPlayerCameraProbeY = playerCenterY;
    }

    const CameraFixedRange* fixedRange = nullptr;
    for (const CameraFixedRange& range : m_cameraFixedRanges)
    {
        if (playerCenterX >= range.startX && playerCenterX <= range.endX)
        {
            fixedRange = &range;
            break;
        }
    }

    if (!m_cameraFixedLockActive && fixedRange)
    {
        m_cameraFixedLockActive = true;
        m_cameraFixedLockStartX = fixedRange->startX;
        m_cameraFixedLockEndX = fixedRange->endX;
        m_cameraFixedLockX = fixedRange->cameraX;
        m_cameraFixedLockY = fixedRange->cameraY;
        m_floorCameraTransitionActive = true;
        m_floorCameraTransitionElapsed = 0.0f;
        m_floorCameraTransitionStartX = m_flow.cameraX;
        m_floorCameraTransitionStartY = m_flow.cameraY;
        m_floorCameraTransitionTargetX = m_cameraFixedLockX;
        m_floorCameraTransitionTargetY = m_flow.cameraY;
    }

    if (m_cameraFixedLockActive)
    {
        // Add hysteresis around S/E bounds to prevent lock thrashing near borders.
        const bool exitedRight = playerCenterX > m_cameraFixedLockEndX + kFixedLockExitMargin;
        const bool exitedLeft = playerCenterX < m_cameraFixedLockStartX - kFixedLockExitMargin;
        if (exitedRight || exitedLeft)
        {
            m_cameraFixedLockActive = false;
            m_floorCameraTransitionActive = true;
            m_floorCameraTransitionElapsed = 0.0f;
            m_floorCameraTransitionStartX = m_flow.cameraX;
            m_floorCameraTransitionStartY = m_flow.cameraY;

            const float mapWidth = GetMapPixelWidth();
            const float mapHeight = GetMapPixelHeight();
            m_floorCameraTransitionTargetX = std::clamp(
                playerCenterX - followSpanX * 0.5f,
                0.0f,
                std::max(0.0f, mapWidth - followSpanX));
            if (gCameraFollowY >= 0.5f)
            {
                m_floorCameraTransitionTargetY = std::clamp(
                    playerCenterY - visibleHeight * 0.5f + GetCameraFollowOffsetY(m_tileMap),
                    0.0f,
                    std::max(0.0f, mapHeight - visibleHeight));
            }
            else
            {
                m_floorCameraTransitionTargetY = 0.0f;
            }
        }
        else
        {
            if (m_floorCameraTransitionActive)
            {
                m_floorCameraTransitionElapsed += std::max(0.0f, deltaTime);
                const float duration = std::max(0.0001f, m_floorCameraTransitionDuration);
                const float t = Clamp01(m_floorCameraTransitionElapsed / duration);
                const float easedT = EaseInOutCubic(t);
                m_flow.cameraX = std::lerp(m_floorCameraTransitionStartX, m_floorCameraTransitionTargetX, easedT);
                if (t >= 1.0f)
                {
                    m_floorCameraTransitionActive = false;
                    m_floorCameraTransitionElapsed = 0.0f;
                    m_flow.cameraX = m_cameraFixedLockX;
                }
            }
            else
            {
                m_flow.cameraX = m_cameraFixedLockX;
            }

            m_previousPlayerCameraProbeX = playerCenterX;
            m_previousPlayerCameraProbeY = playerCenterY;
            return;
        }
    }

    if (m_floorCameraTransitionActive)
    {
        m_floorCameraTransitionElapsed += std::max(0.0f, deltaTime);
        const float duration = std::max(0.0001f, m_floorCameraTransitionDuration);
        const float t = Clamp01(m_floorCameraTransitionElapsed / duration);
        const float easedT = EaseInOutCubic(t);
        m_flow.cameraX = std::lerp(m_floorCameraTransitionStartX, m_floorCameraTransitionTargetX, easedT);
        if (t >= 1.0f)
        {
            m_floorCameraTransitionActive = false;
            m_floorCameraTransitionElapsed = 0.0f;
            m_flow.cameraX = m_floorCameraTransitionTargetX;
        }
    }
    else
    {
        const float tileSize = m_tileMap.GetTileSize();
        if (tileSize > 0.0f)
        {
            const float dx = playerCenterX - m_previousPlayerCameraProbeX;

            for (CameraTransitionMarker& marker : m_cameraTransitionMarkers)
            {
                const bool inside =
                    playerCenterX >= marker.x &&
                    playerCenterX < marker.x + tileSize &&
                    playerCenterY >= marker.y &&
                    playerCenterY < marker.y + tileSize;

                if (inside && !marker.wasInside)
                {
                    int directionX = 0;
                    if (std::fabs(dx) > 0.001f)
                    {
                        directionX = dx > 0.0f ? 1 : -1;
                    }
                    else
                    {
                        directionX = m_player.facingRight ? 1 : -1;
                    }

                    StartFloorCameraTransition(directionX, 0);
                }

                marker.wasInside = inside;
            }
        }
    }

    m_previousPlayerCameraProbeX = playerCenterX;
    m_previousPlayerCameraProbeY = playerCenterY;
}

void GameScene::SpawnBarrelBreakEffect(float x, float y, float width, float height)
{
    const float centerX = x + width * 0.5f;
    const float centerY = y + height * 0.5f;
    constexpr float velocities[][2] =
    {
        { -180.0f, -260.0f },
        { -120.0f, -210.0f },
        { -60.0f, -180.0f },
        {  60.0f, -190.0f },
        {  120.0f, -220.0f },
        {  180.0f, -250.0f },
        { -90.0f, -140.0f },
        {  90.0f, -150.0f },
    };
    constexpr float sizes[] = { 12.0f, 10.0f, 9.0f, 11.0f, 8.0f, 10.0f, 7.0f, 9.0f };
    constexpr float rotations[] = { -0.4f, 0.2f, -0.8f, 0.6f, -0.3f, 0.5f, -0.9f, 0.9f };
    constexpr float rotationSpeeds[] = { -4.0f, 3.2f, -5.4f, 4.6f, -3.8f, 5.0f, -6.2f, 6.0f };
    constexpr float colors[][3] =
    {
        { 0.52f, 0.31f, 0.16f },
        { 0.44f, 0.25f, 0.12f },
        { 0.60f, 0.38f, 0.20f },
        { 0.34f, 0.18f, 0.08f },
    };

    for (int index = 0; index < 8; ++index)
    {
        BarrelDebrisParticle particle;
        particle.x = centerX - sizes[index] * 0.5f;
        particle.y = centerY - sizes[index] * 0.5f;
        particle.velocityX = velocities[index][0];
        particle.velocityY = velocities[index][1];
        particle.size = sizes[index];
        particle.rotation = rotations[index];
        particle.rotationSpeed = rotationSpeeds[index];
        particle.life = kBarrelDebrisLifetime;
        particle.maxLife = kBarrelDebrisLifetime;
        particle.r = colors[index % 4][0];
        particle.g = colors[index % 4][1];
        particle.b = colors[index % 4][2];
        m_effects.barrelDebris.push_back(particle);
    }
}

void GameScene::UpdatePlayerPresentation(Entity& player, float deltaTime, float moveAxis, bool wasGrounded, bool isDodging, bool landedThisFrame)
{
    game_scene_player_visual_system::UpdatePresentation(
        m_player,
        player,
        deltaTime,
        moveAxis,
        wasGrounded,
        isDodging,
        landedThisFrame);
}

void GameScene::UpdateTuningPanel()
{
    if (!m_debug.showTuningPanel)
    {
        return;
    }

    auto entries = BuildGameSceneTuningEntries();
    const int kEntryCount = static_cast<int>(entries.size());

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

    if (!Input_IsMouseLeftPressed())
    {
        return;
    }

    const float mouseX = static_cast<float>(Input_GetMouseX());
    const float mouseY = static_cast<float>(Input_GetMouseY());
    if (!IsPointInside(mouseX, mouseY, kTuningPanelX, kTuningPanelY, kTuningPanelWidth, kTuningPanelHeight))
    {
        return;
    }

    for (int index = 0; index < kEntryCount; ++index)
    {
        const auto layout = GetTuningRowLayout(index);
        const float rowY = layout.y;
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningMinusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value - entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
        if (IsPointInside(mouseX, mouseY, kTuningPanelX + kTuningPlusButtonX, rowY, kTuningButtonWidth, kTuningButtonHeight))
        {
            m_debug.tuningSelection = index;
            *entries[index].value = std::clamp(
                *entries[index].value + entries[index].step,
                entries[index].minValue,
                entries[index].maxValue);
            WriteTuningJsonFile();
            return;
        }
    }
}

void GameScene::UpdatePlayer(float deltaTime)
{
    UpdateCaptureFinderZoomInput();

    Entity* player = FindEntityByTag(kTagPlayer);
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
    const auto controls = game_scene_player_system::SampleControls(blockPlayerInput);
    const float moveAxis = controls.moveAxis;
    const float tileSize = m_tileMap.GetTileSize();
    const bool wasGrounded = IsStandingOnGround(*transform);
    const float dodgeDuration = wasGrounded
        ? GetPlayerDodgeDuration()
        : (gPlayerDodgeSpeed > 0.0f ? tileSize / gPlayerDodgeSpeed : 0.0f);

    game_scene_player_system::TickDodgeState(m_player, deltaTime);
    UpdatePlayerAfterimages(deltaTime);

    game_scene_player_system::UpdateFacingFromMoveAxis(m_player, moveAxis);

    if (controls.dodgePressed &&wasGrounded&& game_scene_player_system::TryBeginDodge(
        m_player,
        moveAxis,
        dodgeDuration,
        gPlayerDodgeCooldown))
    {
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::LogMessage, player, nullptr, "Player dodged", 0.0f, 0.0f });
    }

    const bool isDodging = m_player.dodgeRemaining > 0.0f;
    const float playerWidth = transform->width * transform->scale;
    const float playerHeight = transform->height * transform->scale;
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const bool canJumpNow = !isDodging && controls.jumpPressed && (wasGrounded || m_player.coyoteTimeRemaining > 0.0f);
    if (canJumpNow)
    {
        m_player.velocityY = gPlayerJumpSpeed;
        m_player.grounded = false;
        m_player.coyoteTimeRemaining = 0.0f;
        m_eventBus.Publish({ EventType::PlaySoundRequest, player, nullptr, "test_tone", 0.0f, 0.0f });
    }

    std::vector<TransformComponent> photoBoxes;
    GetPhotoBoxBounds(photoBoxes);
    std::vector<TransformComponent> groundPlatformsForSnap;
    GetGroundPlatformBounds(groundPlatformsForSnap);
    std::vector<TransformComponent> solidObjects;
    BuildPlayerSolidObjectBounds(solidObjects);

    const float targetHorizontalVelocity = game_scene_player_system::GetHorizontalVelocity(
        m_player,
        moveAxis,
        gPlayerDodgeSpeed,
        gPlayerMoveSpeed);
    const float estimatedVerticalVelocity = canJumpNow
        ? gPlayerJumpSpeed
        : std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * deltaTime);
    const float maxDisplacement = std::max(
        std::fabs(targetHorizontalVelocity) * deltaTime,
        std::fabs(estimatedVerticalVelocity) * deltaTime);
    const int subSteps = std::clamp(static_cast<int>(std::ceil(maxDisplacement / 8.0f)), 1, 8);
    const float stepDeltaTime = deltaTime / static_cast<float>(subSteps);

    bool groundedAtStepStart = wasGrounded;
    for (int stepIndex = 0; stepIndex < subSteps; ++stepIndex)
    {
        m_player.velocityX = targetHorizontalVelocity;
        m_player.grounded = groundedAtStepStart;
        if (groundedAtStepStart)
        {
            m_player.coyoteTimeRemaining = gCoyoteTimeSeconds;
        }
        else
        {
            m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - stepDeltaTime);
        }

        if (groundedAtStepStart && m_player.velocityY > 0.0f)
        {
            m_player.velocityY = 0.0f;
        }

        if (m_player.grounded && !canJumpNow)
        {
            m_player.velocityY = 0.0f;
        }
        else
        {
            m_player.velocityY = std::min(gPlayerMaxFallSpeed, m_player.velocityY + gPlayerGravity * stepDeltaTime);
        }

        const float previousX = transform->x;
        const float previousY = transform->y;
        const float previousBottom = previousY + playerHeight;
        const float verticalSnapDistance = std::max(gGroundSnapDistance, std::fabs(m_player.velocityY) * stepDeltaTime + 4.0f);
        const game_scene_player_movement_system::PlayerMovementContext movementContext{
            stepDeltaTime,
            tileSize,
            playerWidth,
            playerHeight,
            mapWidth,
            mapHeight,
            previousX,
            previousY,
            previousBottom,
            verticalSnapDistance,
        };
        const float horizontalVelocity = m_player.velocityX;
        const auto intersectsPhotoBoxForHorizontalMove = [this, groundedAtStepStart, tileSize](const TransformComponent& candidate)
        {
            if (!groundedAtStepStart)
            {
                return IntersectsSolidPhotoBoxForMovement(candidate);
            }

            TransformComponent liftedCandidate = candidate;
            liftedCandidate.y -= std::max(2.0f, tileSize * 0.09f);
            return IntersectsSolidPhotoBoxForMovement(liftedCandidate);
        };

        game_scene_player_movement_system::ResolveHorizontalTileCollisions(
            *transform,
            m_player,
            movementContext,
            [this, horizontalVelocity](int column, int row)
            {
                return horizontalVelocity > 0.0f
                    ? IsTileBlockingFromLeft(column, row)
                    : IsTileBlockingFromRight(column, row);
            },
            [&intersectsPhotoBoxForHorizontalMove](const TransformComponent& candidate)
            {
                return intersectsPhotoBoxForHorizontalMove(candidate);
            });

        game_scene_player_movement_system::ResolveHorizontalObjectCollisions(
            *transform,
            m_player,
            movementContext,
            photoBoxes,
            [&intersectsPhotoBoxForHorizontalMove](const TransformComponent& candidate)
            {
                return intersectsPhotoBoxForHorizontalMove(candidate);
            },
            solidObjects);

        if (m_player.velocityY >= 0.0f && groundedAtStepStart)
        {
            if (TrySnapToGroundUsingPlatforms(*transform, verticalSnapDistance, groundPlatformsForSnap))
            {
                m_player.grounded = true;
            }
        }

        game_scene_player_movement_system::ResolveVerticalMotion(
            *transform,
            m_player,
            groundedAtStepStart,
            movementContext,
            photoBoxes,
            solidObjects,
            [this](int column, int row)
            {
                return IsSolidTile(column, row);
            },
            [this](int column, int row)
            {
                return IsPlatformTile(column, row);
            },
            [this](int column, int row)
            {
                return IsSolidTile(column, row) || IsSlopeTile(column, row);
            },
            [this, &groundPlatformsForSnap](TransformComponent& targetTransform, float snapDistance)
            {
                return TrySnapToGroundUsingPlatforms(targetTransform, snapDistance, groundPlatformsForSnap);
            },
            [this](const TransformComponent& candidate)
            {
                return IntersectsSolidPhotoBoxForMovement(candidate);
            });

        groundedAtStepStart = m_player.grounded;

        if (isDodging)
        {
            TrySpawnPlayerAfterimage(*transform);
        }
    }

    const bool landedThisFrame = !wasGrounded && m_player.grounded;
    UpdatePlayerPresentation(*player, deltaTime, moveAxis, wasGrounded, isDodging, landedThisFrame);

    game_scene_player_movement_system::UpdateCamera(
        m_flow.cameraX,
        m_flow.cameraY,
        transform->x,
        transform->y,
        playerWidth,
        playerHeight,
        GetCameraFollowSpanX(m_tileMap),
        GetCameraVisibleHeight(m_tileMap),
        mapWidth,
        mapHeight,
        GetCameraFollowOffsetY(m_tileMap),
        deltaTime,
        gCameraFollowY >= 0.5f);
    UpdateCameraByMarkers(*transform, deltaTime);

    // Safety clamp: keep the player inside the vertical camera view even when marker/fixed-lock
    // transitions are active, so the player never drops out of frame.
    if (gCameraFollowY >= 0.5f)
    {
        const float visibleHeight = GetCameraVisibleHeight(m_tileMap);
        const float maxCameraY = std::max(0.0f, mapHeight - visibleHeight);
        const float tileSizeForMargin = std::max(1.0f, m_tileMap.GetTileSize());
        const float topMargin = tileSizeForMargin * 1.0f;
        const float bottomMargin = tileSizeForMargin * 1.5f;
        const float playerTop = transform->y;
        const float playerBottom = transform->y + playerHeight;

        // Hard catch-up for downward movement: if the player is close to leaving the lower edge,
        // snap camera Y enough to keep them inside a stable margin.
        const float lowerLimitY = m_flow.cameraY + visibleHeight - bottomMargin;
        if (playerBottom > lowerLimitY)
        {
            const float requiredCameraY = playerBottom - (visibleHeight - bottomMargin);
            m_flow.cameraY = std::max(m_flow.cameraY, requiredCameraY);
        }

        const float minAllowedCameraY = playerBottom - (visibleHeight - bottomMargin);
        const float maxAllowedCameraY = playerTop - topMargin;
        const float clampedToPlayerY = std::clamp(m_flow.cameraY, minAllowedCameraY, maxAllowedCameraY);
        m_flow.cameraY = std::clamp(clampedToPlayerY, 0.0f, maxCameraY);
    }
}

void GameScene::BuildPlayerSolidObjectBounds(std::vector<TransformComponent>& bounds) const
{
    bounds.clear();

    GetGroundPlatformBounds(bounds);

    std::vector<TransformComponent> batteryBounds;
    GetEntityBoundsByTag(kTagBattery, batteryBounds);
    bounds.insert(bounds.end(), batteryBounds.begin(), batteryBounds.end());

    std::vector<TransformComponent> enemyBounds;
    GetEntityBoundsByTag(kTagEnemy, enemyBounds);
    bounds.insert(bounds.end(), enemyBounds.begin(), enemyBounds.end());
}

void GameScene::UpdateBarrels(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    const float tileSize = m_tileMap.GetTileSize();
    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    const float activeLeft = std::max(0.0f, m_flow.cameraX - gBarrelActivationPaddingX);
    const float activeRight = std::min(mapWidth, m_flow.cameraX + gCameraViewWidth + gBarrelActivationPaddingX);
    const float activationDistance = tileSize * 10.0f;

    auto setBarrelVisible = [](Entity& barrelEntity, bool visible)
    {
        if (auto* tint = barrelEntity.GetComponent<TintComponent>())
        {
            tint->a = visible ? 1.0f : 0.0f;
        }
    };

    auto resetBarrel = [&](Entity& barrelEntity, BarrelComponent& barrel, TransformComponent& transform)
    {
        SpawnBarrelBreakEffect(transform.x, transform.y, transform.width * transform.scale, transform.height * transform.scale);
        m_eventBus.Publish({ EventType::PlaySoundRequest, &barrelEntity, nullptr, "barrel", 0.0f, 0.0f });
        if (!barrel.respawnEnabled)
        {
            barrel.destroyed = true;
        }
        barrel.active = false;
        barrel.cooldownActive = barrel.respawnEnabled && !barrel.destroyed;
        barrel.cooldownRemaining = barrel.cooldownActive ? 3.0f : 0.0f;
        barrel.velocityX = 0.0f;
        barrel.velocityY = 0.0f;
        barrel.grounded = false;
        barrel.accumulatedFallDistance = 0.0f;
        setBarrelVisible(barrelEntity, false);
    };

    auto isBarrelObjectCollision = [&](const Entity& barrelEntity, const TransformComponent& barrelBounds, Entity*& outHit) -> bool
    {
        outHit = nullptr;
        for (const auto& candidate : m_entities)
        {
            if (!candidate || candidate.get() == &barrelEntity)
            {
                continue;
            }

            if (candidate->GetComponent<BarrelComponent>())
            {
                continue;
            }

            if (HasTag(*candidate, kTagPlayer) || HasTag(*candidate, kTagEnemy) || HasTag(*candidate, kTagBullet))
            {
                continue;
            }

            if (HasTag(*candidate, kTagPhotoBox))
            {
                const auto* layer = candidate->GetComponent<PhotoCopyLayerComponent>();
                if (layer && layer->layer != PhotoCopyLayer::Foreground)
                {
                    continue;
                }
            }

            const auto* otherTransform = candidate->GetComponent<TransformComponent>();
            if (!otherTransform)
            {
                continue;
            }

            if (IntersectsRect(barrelBounds, *otherTransform))
            {
                outHit = candidate.get();
                return true;
            }
        }

        return false;
    };

    auto intersectsDespawnTile = [&](const TransformComponent& bounds) -> bool
    {
        const float width = bounds.width * bounds.scale;
        const float height = bounds.height * bounds.scale;
        const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
        const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
        const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
        const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
        for (int row = top; row <= bottom; ++row)
        {
            for (int column = left; column <= right; ++column)
            {
                const int tileValue = m_tileMap.GetTile(column, row);
                if (tileValue != 1 && tileValue != TileMap::kPitTileValue)
                {
                    continue;
                }

                TransformComponent tileBounds(
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize,
                    tileSize,
                    tileSize);
                if (IntersectsRect(bounds, tileBounds))
                {
                    return true;
                }
            }
        }
        return false;
    };

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        auto* barrel = entity->GetComponent<BarrelComponent>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!barrel || !transform)
        {
            continue;
        }

        const bool isLog = HasTag(*entity, kTagLog);

        if (barrel->destroyed)
        {
            setBarrelVisible(*entity, false);
            continue;
        }

        const float barrelWidth = transform->width * transform->scale;
        const float barrelHeight = transform->height * transform->scale;
        if (barrel->respawnWhenOffscreen)
        {
            const float cameraLeft = m_flow.cameraX - kBarrelRespawnOffscreenMargin;
            const float cameraRight = m_flow.cameraX + gCameraViewWidth + kBarrelRespawnOffscreenMargin;
            const float cameraBottom = m_flow.cameraY + gCameraViewHeight + kBarrelRespawnOffscreenMargin;
            const bool isOffscreen =
                (transform->x + barrelWidth) < cameraLeft ||
                transform->x > cameraRight ||
                transform->y > cameraBottom;
            if (isOffscreen)
            {
                transform->x = std::clamp(barrel->spawnX, 0.0f, std::max(0.0f, mapWidth - barrelWidth));
                transform->y = std::clamp(barrel->spawnY, 0.0f, std::max(0.0f, mapHeight - barrelHeight));
                barrel->velocityX = 0.0f;
                barrel->velocityY = 0.0f;
                barrel->accumulatedFallDistance = 0.0f;
                barrel->grounded = false;
                barrel->destroyed = false;
                continue;
            }
        }

        if (!isLog && (transform->x + barrelWidth < activeLeft || transform->x > activeRight))
        {
            continue;
        }

        if (barrel->cooldownActive)
        {
            barrel->cooldownRemaining = std::max(0.0f, barrel->cooldownRemaining - deltaTime);
            if (barrel->cooldownRemaining <= 0.0f)
            {
                barrel->cooldownActive = false;
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
                setBarrelVisible(*entity, true);
            }
        }

        if (!barrel->active)
        {
            barrel->velocityX = 0.0f;
            barrel->velocityY = 0.0f;
            barrel->grounded = false;
            barrel->accumulatedFallDistance = 0.0f;

            if (!barrel->cooldownActive)
            {
                transform->x = barrel->spawnX;
                transform->y = barrel->spawnY;
            }

            if (!barrel->cooldownActive && player)
            {
                const auto* playerTransform = player->GetComponent<TransformComponent>();
                if (playerTransform)
                {
                    const float playerCenterX = playerTransform->x + playerTransform->width * playerTransform->scale * 0.5f;
                    const float playerCenterY = playerTransform->y + playerTransform->height * playerTransform->scale * 0.5f;
                    const float barrelCenterX = transform->x + barrelWidth * 0.5f;
                    const float barrelCenterY = transform->y + barrelHeight * 0.5f;
                    const float dx = playerCenterX - barrelCenterX;
                    const float dy = playerCenterY - barrelCenterY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance <= activationDistance)
                    {
                        barrel->active = true;
                        setBarrelVisible(*entity, true);
                    }
                }
            }
            else if (!barrel->respawnEnabled)
            {
                barrel->active = true;
                setBarrelVisible(*entity, true);
            }

            continue;
        }

        const float previousY = transform->y;
        barrel->velocityY = std::min(barrel->maxFallSpeed, barrel->velocityY + barrel->gravity * deltaTime);
        transform->y += barrel->velocityY * deltaTime;
        const bool canCollideAfterDrop = transform->y >= barrel->spawnY + std::max(8.0f, tileSize * 0.25f);

        if (isLog)
        {
            const bool snapped = TrySnapToGround(*transform, std::max(gGroundSnapDistance, std::fabs(barrel->velocityY) * deltaTime + 4.0f));
            barrel->grounded = snapped;
            if (snapped)
            {
                barrel->velocityY = 0.0f;
            }

            if (transform->y + barrelHeight > mapHeight)
            {
                transform->y = mapHeight - barrelHeight;
                barrel->velocityY = 0.0f;
                barrel->grounded = true;
            }

            Entity* hitObject = nullptr;
            if (!barrel->grounded && isBarrelObjectCollision(*entity, *transform, hitObject))
            {
                transform->y = previousY;
                barrel->velocityY = 0.0f;
                barrel->grounded = TrySnapToGround(*transform, tileSize * 0.5f);
            }

            continue;
        }

        if (transform->y + barrelHeight >= mapHeight)
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (canCollideAfterDrop && intersectsDespawnTile(*transform))
        {
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        if (player && IntersectsEntity(*entity, *player))
        {
            HandlePlayerDamage(*player, entity.get(), "GameScene player damaged by barrel");
            resetBarrel(*entity, *barrel, *transform);
            continue;
        }

        bool consumed = false;
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

            HandleEnemyDamage(*enemyEntity, entity.get(), barrel->contactDamage, "Barrel hit enemy");
            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

        for (const auto& gimmickEntity : m_entities)
        {
            if (!gimmickEntity || gimmickEntity.get() == entity.get())
            {
                continue;
            }

            auto* gimmick = gimmickEntity->GetComponent<GimmickComponent>();
            if (!gimmick || !gimmick->IsEnabled() || !IntersectsEntity(*entity, *gimmickEntity))
            {
                continue;
            }

            if (gimmick->GetType() == GimmickType::Switch)
            {
                m_flow.goalUnlockedBySwitch = true;
                gimmick->Consume();
            }

            resetBarrel(*entity, *barrel, *transform);
            consumed = true;
            break;
        }
        if (consumed)
        {
            continue;
        }

    }
}
void GameScene::UpdateBatteries(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    std::vector<Entity*> enemies;
    enemies.reserve(m_entities.size());
    for (const auto& candidate : m_entities)
    {
        if (!candidate || !HasTag(*candidate, kTagEnemy))
        {
            continue;
        }
        if (auto* enemy = candidate->GetComponent<EnemyComponent>())
        {
            if (!enemy->IsEnabled() || enemy->IsDefeated())
            {
                continue;
            }
        }
        enemies.push_back(candidate.get());
    }
    std::vector<TransformComponent> groundPlatformsForSnap;
    GetGroundPlatformBounds(groundPlatformsForSnap);

    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }
        UpdateSingleBattery(*entity, player, enemies, groundPlatformsForSnap, deltaTime, tileSize);
    }
}

void GameScene::UpdateLaserTurrets(float deltaTime)
{
    if (deltaTime <= 0.0f)
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const float mapWidth = GetMapPixelWidth();
    const float mapHeight = GetMapPixelHeight();
    Entity* player = FindEntityByTag(kTagPlayer);
    TransformComponent* playerLaserBlockTransform = player ? player->GetComponent<TransformComponent>() : nullptr;
    auto intersectsRect = [](const TransformComponent& a, const TransformComponent& b) -> bool
    {
        const float aWidth = a.width * a.scale;
        const float aHeight = a.height * a.scale;
        const float bWidth = b.width * b.scale;
        const float bHeight = b.height * b.scale;
        return a.x < b.x + bWidth &&
            a.x + aWidth > b.x &&
            a.y < b.y + bHeight &&
            a.y + aHeight > b.y;
    };

    std::vector<Entity*> enemyEntities;
    enemyEntities.reserve(m_entities.size());
    bool hasLaserPowerSwitch = false;
    bool laserPowerEnabled = false;
    for (const auto& entity : m_entities)
    {
        if (entity)
        {
            if (const auto* batterySwitch = entity->GetComponent<BatterySwitchComponent>())
            {
                if (batterySwitch->controlsLaserPower)
                {
                    hasLaserPowerSwitch = true;
                    laserPowerEnabled = laserPowerEnabled || batterySwitch->isPressed;
                }
            }
        }
        if (!entity || !entity->GetComponent<EnemyComponent>())
        {
            continue;
        }
        enemyEntities.push_back(entity.get());
    }

    for (auto& turretCandidate : m_entities)
    {
        if (!turretCandidate || !HasTag(*turretCandidate, kTagLaserTurret))
        {
            continue;
        }

        auto* turretTransform = turretCandidate->GetComponent<TransformComponent>();
        auto* turret = turretCandidate->GetComponent<LaserTurretComponent>();
        if (!turretTransform || !turret)
        {
            continue;
        }

        if (turret->warmupRemaining > 0.0f)
        {
            turret->warmupRemaining = std::max(0.0f, turret->warmupRemaining - deltaTime);
        }

        Entity* beamEntity = turret->beamEntity;
        const auto fireDirection = turret->fireDirection;
        const bool firesLeft = fireDirection == LaserTurretFireDirection::Left;
        const bool firesUp = fireDirection == LaserTurretFireDirection::Up;
        const bool firesVertical = IsVerticalLaserDirection(fireDirection);
        if (!beamEntity || !HasTag(*beamEntity, kTagLaserBeam))
        {
            beamEntity = nullptr;
            for (const auto& beamCandidate : m_entities)
            {
                if (!beamCandidate || !HasTag(*beamCandidate, kTagLaserBeam))
                {
                    continue;
                }
                auto* beamTransform = beamCandidate->GetComponent<TransformComponent>();
                if (!beamTransform)
                {
                    continue;
                }

                const float beamCenterX = beamTransform->x + beamTransform->width * beamTransform->scale * 0.5f;
                const float beamCenterY = beamTransform->y + beamTransform->height * beamTransform->scale * 0.5f;
                const float turretCenterX = turretTransform->x + turretTransform->width * turretTransform->scale * 0.5f;
                const float turretCenterY = turretTransform->y + turretTransform->height * turretTransform->scale * 0.5f;
                const bool matchesHorizontal =
                    !firesVertical &&
                    std::fabs(beamCenterY - turretCenterY) <= tileSize * 0.25f &&
                    (firesLeft
                        ? beamTransform->x <= turretTransform->x + turretTransform->width * turretTransform->scale
                        : beamTransform->x >= turretTransform->x);
                const bool matchesVertical =
                    firesVertical &&
                    std::fabs(beamCenterX - turretCenterX) <= tileSize * 0.25f &&
                    (firesUp
                        ? beamTransform->y <= turretTransform->y + turretTransform->height * turretTransform->scale
                        : beamTransform->y >= turretTransform->y);
                if (matchesHorizontal || matchesVertical)
                {
                    beamEntity = beamCandidate.get();
                    turret->beamEntity = beamEntity;
                    break;
                }
            }
        }

        if (!beamEntity)
        {
            continue;
        }

        auto* beamTransform = beamEntity->GetComponent<TransformComponent>();
        if (!beamTransform)
        {
            continue;
        }
        auto* beamDamage = beamEntity->GetComponent<LaserBeamComponent>();
        const bool beamPenetratesPlayer =
            turretCandidate->GetComponent<BossBeamCaptureComponent>() != nullptr;
        if (turret->requiresLaserPower && (!hasLaserPowerSwitch || !laserPowerEnabled))
        {
            beamTransform->width = 0.0f;
            beamTransform->height = 0.0f;
            turret->playerDamageTimer = 0.0f;
            turret->enemyDamageTimers.clear();
            if (beamDamage)
            {
                beamDamage->enemyDamageTimers.clear();
            }
            continue;
        }

        if (!turret->active || turret->warmupRemaining > 0.0f)
        {
            beamTransform->width = 0.0f;
            turret->playerDamageTimer = 0.0f;
            turret->enemyDamageTimers.clear();
            if (beamDamage)
            {
                beamDamage->enemyDamageTimers.clear();
            }
            continue;
        }

        const float turretWidth = turretTransform->width * turretTransform->scale;
        const float turretHeight = turretTransform->height * turretTransform->scale;
        const float beamThickness = (std::max)(1.0f, turret->beamThickness);
        const float beamDamagePerSecond = beamDamage ? beamDamage->damagePerSecond : turret->damagePerSecond;
        const float beamEnemyKnockbackSpeed = beamDamage ? beamDamage->enemyKnockbackSpeed : turret->enemyKnockbackSpeed;
        std::unordered_map<const Entity*, float>* enemyDamageTimers =
            beamDamage ? &beamDamage->enemyDamageTimers : &turret->enemyDamageTimers;
        const float damageInterval = 1.0f / (std::max)(0.1f, beamDamagePerSecond);
        float beamLength = 0.0f;
        TransformComponent activeBeam(0.0f, 0.0f, 0.0f, 0.0f);
        float sparkX = 0.0f;
        float sparkY = 0.0f;
        bool blocked = false;
        bool playerHitByLaser = false;
        Entity* blockedProtectiveWall = nullptr;

        if (firesVertical)
        {
            const float beamStartY = turretTransform->y + turret->beamOriginOffsetY;
            const float beamX = turretTransform->x + turret->beamOriginOffsetX;
            const float beamAabbY = firesUp ? 0.0f : beamStartY;
            const float beamAabbHeight = firesUp
                ? std::max(0.0f, beamStartY)
                : std::max(0.0f, mapHeight - beamStartY);
            float hitY = firesUp ? 0.0f : mapHeight;

            const int columnLeft = std::max(0, static_cast<int>(std::floor(beamX / tileSize)));
            const int columnRight = std::min(
                m_tileMap.GetWidth() - 1,
                static_cast<int>(std::floor((beamX + beamThickness - 1.0f) / tileSize)));
            if (firesUp)
            {
                for (int row = std::min(
                        m_tileMap.GetHeight() - 1,
                        static_cast<int>(std::floor((beamStartY - 1.0f) / tileSize)));
                    row >= 0;
                    --row)
                {
                    bool rowBlocked = false;
                    for (int column = columnLeft; column <= columnRight; ++column)
                    {
                        if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                        {
                            continue;
                        }

                        hitY = std::max(hitY, static_cast<float>(row + 1) * tileSize);
                        rowBlocked = true;
                        break;
                    }
                    if (rowBlocked)
                    {
                        break;
                    }
                }
            }
            else
            {
                for (int row = std::max(0, static_cast<int>(std::floor(beamStartY / tileSize))); row < m_tileMap.GetHeight(); ++row)
                {
                    bool rowBlocked = false;
                    for (int column = columnLeft; column <= columnRight; ++column)
                    {
                        if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                        {
                            continue;
                        }

                        hitY = std::min(hitY, static_cast<float>(row) * tileSize);
                        rowBlocked = true;
                        break;
                    }
                    if (rowBlocked)
                    {
                        break;
                    }
                }
            }

            TransformComponent beamAabb(beamX, beamAabbY, beamThickness, beamAabbHeight);
            for (const auto& entity : m_entities)
            {
                if (!entity || entity.get() == turretCandidate.get() || entity.get() == beamEntity)
                {
                    continue;
                }
                if (HasTag(*entity, kTagPlayer) || entity->GetComponent<EnemyComponent>())
                {
                    continue;
                }
                if (!(entity->GetComponent<BarrelComponent>() ||
                    entity->GetComponent<BatteryComponent>() ||
                    IsGroundPlatformEntity(*entity) ||
                    HasTag(*entity, kTagPhotoBox)))
                {
                    continue;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                if (!transform)
                {
                    continue;
                }

                const float objectHeight = transform->height * transform->scale;
                if (firesUp)
                {
                    if (transform->y >= beamStartY)
                    {
                        continue;
                    }
                    if (!intersectsRect(beamAabb, *transform))
                    {
                        continue;
                    }

                    const float objectHitY = transform->y + objectHeight;
                    if (objectHitY > hitY)
                    {
                        hitY = objectHitY;
                        blockedProtectiveWall = HasTag(*entity, kTagProtectiveWall)
                            ? entity.get()
                            : nullptr;
                    }
                }
                else
                {
                    if (transform->y + objectHeight <= beamStartY)
                    {
                        continue;
                    }
                    if (!intersectsRect(beamAabb, *transform))
                    {
                        continue;
                    }

                    if (transform->y < hitY)
                    {
                        hitY = transform->y;
                        blockedProtectiveWall = HasTag(*entity, kTagProtectiveWall)
                            ? entity.get()
                            : nullptr;
                    }
                }
            }

            if (playerLaserBlockTransform && !beamPenetratesPlayer)
            {
                const float playerHeight = playerLaserBlockTransform->height * playerLaserBlockTransform->scale;
                if (firesUp)
                {
                    if (playerLaserBlockTransform->y < beamStartY &&
                        intersectsRect(beamAabb, *playerLaserBlockTransform))
                    {
                        hitY = std::max(hitY, playerLaserBlockTransform->y + playerHeight);
                    }
                }
                else
                {
                    if (playerLaserBlockTransform->y + playerHeight > beamStartY &&
                        intersectsRect(beamAabb, *playerLaserBlockTransform))
                    {
                        hitY = std::min(hitY, playerLaserBlockTransform->y);
                    }
                }
            }

            beamLength = firesUp
                ? std::max(0.0f, beamStartY - hitY)
                : std::max(0.0f, hitY - beamStartY);
            beamTransform->x = beamX;
            beamTransform->y = firesUp ? hitY : beamStartY;
            beamTransform->width = beamThickness;
            beamTransform->height = beamLength;
            activeBeam = TransformComponent(beamX, firesUp ? hitY : beamStartY, beamThickness, beamLength);
            blocked = firesUp ? hitY > 0.1f : hitY < mapHeight - 0.1f;
            if (playerLaserBlockTransform)
            {
                const float playerHeight = playerLaserBlockTransform->height * playerLaserBlockTransform->scale;
                playerHitByLaser =
                    beamLength > 0.0f &&
                    intersectsRect(activeBeam, *playerLaserBlockTransform) &&
                    (firesUp
                        ? playerLaserBlockTransform->y + playerHeight >= hitY - 0.5f
                        : playerLaserBlockTransform->y <= hitY + 0.5f);
            }
            sparkX = beamX + beamThickness * 0.5f;
            sparkY = hitY;
        }
        else
        {
            const float beamStartX = turretTransform->x + turret->beamOriginOffsetX;
            const float beamY = turretTransform->y + turret->beamOriginOffsetY - beamThickness * 0.5f;
            float hitX = firesLeft ? 0.0f : mapWidth;

            const int rowTop = std::max(0, static_cast<int>(std::floor(beamY / tileSize)));
            const int rowBottom = std::min(
                m_tileMap.GetHeight() - 1,
                static_cast<int>(std::floor((beamY + beamThickness - 1.0f) / tileSize)));
            const int startColumn = std::clamp(static_cast<int>(std::floor(beamStartX / tileSize)), 0, m_tileMap.GetWidth() - 1);
            for (int column = startColumn;
                firesLeft ? (column >= 0) : (column < m_tileMap.GetWidth());
                firesLeft ? --column : ++column)
            {
                bool blockedByTile = false;
                for (int row = rowTop; row <= rowBottom; ++row)
                {
                    if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                    {
                        continue;
                    }

                    hitX = firesLeft
                        ? std::max(hitX, static_cast<float>(column + 1) * tileSize)
                        : std::min(hitX, static_cast<float>(column) * tileSize);
                    blockedByTile = true;
                    break;
                }
                if (blockedByTile)
                {
                    break;
                }
            }

            const float beamAabbLeft = firesLeft ? 0.0f : beamStartX;
            const float beamAabbRight = firesLeft ? beamStartX : mapWidth;
            TransformComponent beamAabb(beamAabbLeft, beamY, std::max(0.0f, beamAabbRight - beamAabbLeft), beamThickness);
            for (const auto& entity : m_entities)
            {
                if (!entity || entity.get() == turretCandidate.get() || entity.get() == beamEntity)
                {
                    continue;
                }
                if (HasTag(*entity, kTagPlayer) || entity->GetComponent<EnemyComponent>())
                {
                    continue;
                }
                if (!(entity->GetComponent<BarrelComponent>() ||
                    entity->GetComponent<BatteryComponent>() ||
                    IsGroundPlatformEntity(*entity) ||
                    HasTag(*entity, kTagPhotoBox)))
                {
                    continue;
                }

                auto* transform = entity->GetComponent<TransformComponent>();
                if (!transform)
                {
                    continue;
                }

                const float objectWidth = transform->width * transform->scale;
                if ((!firesLeft && transform->x + objectWidth <= beamStartX) ||
                    (firesLeft && transform->x >= beamStartX))
                {
                    continue;
                }
                if (!intersectsRect(beamAabb, *transform))
                {
                    continue;
                }

                const float objectHitX = firesLeft
                    ? transform->x + objectWidth
                    : transform->x;
                const bool nearerHit = firesLeft
                    ? objectHitX > hitX
                    : objectHitX < hitX;
                if (nearerHit)
                {
                    hitX = objectHitX;
                    blockedProtectiveWall = HasTag(*entity, kTagProtectiveWall)
                        ? entity.get()
                        : nullptr;
                }
            }

            if (playerLaserBlockTransform && !beamPenetratesPlayer)
            {
                const float playerWidth = playerLaserBlockTransform->width * playerLaserBlockTransform->scale;
                if (!(firesLeft
                    ? playerLaserBlockTransform->x >= beamStartX
                    : playerLaserBlockTransform->x + playerWidth <= beamStartX) &&
                    intersectsRect(beamAabb, *playerLaserBlockTransform))
                {
                    hitX = firesLeft
                        ? std::max(hitX, playerLaserBlockTransform->x + playerWidth)
                        : std::min(hitX, playerLaserBlockTransform->x);
                }
            }

            beamLength = firesLeft
                ? std::max(0.0f, beamStartX - hitX)
                : std::max(0.0f, hitX - beamStartX);
            beamTransform->x = firesLeft ? hitX : beamStartX;
            beamTransform->y = beamY;
            beamTransform->width = beamLength;
            beamTransform->height = beamThickness;
            activeBeam = TransformComponent(beamTransform->x, beamY, beamLength, beamThickness);
            blocked = firesLeft ? hitX > 0.1f : hitX < mapWidth - 0.1f;
            if (playerLaserBlockTransform)
            {
                const float playerWidth = playerLaserBlockTransform->width * playerLaserBlockTransform->scale;
                playerHitByLaser =
                    beamLength > 0.0f &&
                    intersectsRect(beamAabb, *playerLaserBlockTransform) &&
                    (firesLeft
                        ? playerLaserBlockTransform->x + playerWidth >= hitX - 0.5f
                        : playerLaserBlockTransform->x <= hitX + 0.5f);
            }
            sparkX = hitX;
            sparkY = beamY + beamThickness * 0.5f;
        }
        if (player)
        {
            if (auto* playerTransform = player->GetComponent<TransformComponent>())
            {
                if (beamLength > 0.0f && (intersectsRect(activeBeam, *playerTransform) || playerHitByLaser))
                {
                    turret->playerDamageTimer += deltaTime;
                    m_flow.playerTouchingHazard = true;
                    while (turret->playerDamageTimer >= damageInterval)
                    {
                        HandlePlayerDamage(*player, turretCandidate.get(), "Laser damaged player", 1);
                        turret->playerDamageTimer -= damageInterval;
                    }
                }
                else
                {
                    turret->playerDamageTimer = 0.0f;
                }
            }
        }

        const bool bossBeamCanDamageWall =
            turretCandidate->GetComponent<BossBeamCaptureComponent>() != nullptr;
        if (bossBeamCanDamageWall && blockedProtectiveWall)
        {
            if (auto* wall = blockedProtectiveWall->GetComponent<ProtectiveWallComponent>())
            {
                wall->damageAccumulator += deltaTime;
                while (!wall->IsDestroyed() && wall->damageAccumulator >= damageInterval)
                {
                    wall->ApplyDamage(1);
                    wall->damageAccumulator -= damageInterval;
                }
            }
        }

        std::unordered_map<const Entity*, float> nextEnemyDamageTimers;
        nextEnemyDamageTimers.reserve(enemyEntities.size());
        for (Entity* enemyEntity : enemyEntities)
        {
            if (!enemyEntity)
            {
                continue;
            }

            auto* enemyTransform = enemyEntity->GetComponent<TransformComponent>();
            if (!enemyTransform || beamLength <= 0.0f || !intersectsRect(activeBeam, *enemyTransform))
            {
                continue;
            }

            float timer = deltaTime;
            auto timerIt = enemyDamageTimers->find(enemyEntity);
            if (timerIt != enemyDamageTimers->end())
            {
                timer = timerIt->second + deltaTime;
            }
            while (timer >= damageInterval)
            {
                HandleEnemyDamage(*enemyEntity, beamEntity, 1, "Laser damaged enemy");
                if (beamEnemyKnockbackSpeed > 0.0f)
                {
                    enemyTransform->x += (firesLeft ? -1.0f : 1.0f) * beamEnemyKnockbackSpeed * damageInterval;
                }
                timer -= damageInterval;
            }
            nextEnemyDamageTimers[enemyEntity] = timer;
        }
        enemyDamageTimers->swap(nextEnemyDamageTimers);
        turret->sparkTimer -= deltaTime;
        if (blocked && turret->sparkTimer <= 0.0f)
        {
            turret->sparkTimer = 0.08f;
            for (int index = 0; index < 2; ++index)
            {
                LaserSparkParticle spark;
                spark.x = sparkX;
                spark.y = sparkY;
                spark.velocityX = firesVertical
                    ? -90.0f + static_cast<float>(GetRand(180))
                    : (firesLeft
                        ? 60.0f + static_cast<float>(GetRand(140))
                        : -60.0f - static_cast<float>(GetRand(140)));
                spark.velocityY = firesVertical
                    ? (firesUp
                        ? 60.0f + static_cast<float>(GetRand(140))
                        : -60.0f - static_cast<float>(GetRand(140)))
                    : -90.0f + static_cast<float>(GetRand(180));
                spark.life = 0.18f;
                spark.maxLife = 0.18f;
                m_effects.laserSparks.push_back(spark);
            }
        }
    }
}

bool GameScene::IsBatteryCollidingWithWorld(const TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float width = bounds.width * bounds.scale;
    const float height = bounds.height * bounds.scale;
    const int left = std::max(0, static_cast<int>((bounds.x + 2.0f) / tileSize));
    const int right = std::min(m_tileMap.GetWidth() - 1, static_cast<int>((bounds.x + width - 2.0f) / tileSize));
    const int top = std::max(0, static_cast<int>((bounds.y + 2.0f) / tileSize));
    const int bottom = std::min(m_tileMap.GetHeight() - 1, static_cast<int>((bounds.y + height - 2.0f) / tileSize));
    for (int row = top; row <= bottom; ++row)
    {
        for (int column = left; column <= right; ++column)
        {
            if (IsSolidTile(column, row))
            {
                return true;
            }
        }
    }

    if (IntersectsSolidPhotoBoxForMovement(bounds))
    {
        return true;
    }

    for (const auto& entity : m_entities)
    {
        if (!entity || entity.get() == self)
        {
            continue;
        }
        if (!(entity->GetComponent<BarrelComponent>() ||
            entity->GetComponent<BatteryComponent>() ||
            IsGroundPlatformEntity(*entity)))
        {
            continue;
        }
        const auto* transform = entity->GetComponent<TransformComponent>();
        if (!transform)
        {
            continue;
        }

        // スイッチ/エレベーターの天面上に乗っているときは横衝突扱いにしない。
        const bool isDynamicPlatform = HasTag(*entity, kTagBatterySwitch) || HasTag(*entity, kTagElevator);
        if (isDynamicPlatform)
        {
            const float boundsBottom = bounds.y + height;
            const float platformTop = transform->y;
            const float topTolerance = std::max(6.0f, tileSize * 0.22f);
            if (boundsBottom <= platformTop + topTolerance)
            {
                continue;
            }
        }

        if (IntersectsRect(bounds, *transform))
        {
            return true;
        }
    }

    return false;
}

bool GameScene::IsBatteryOnTopOfSwitchOrElevator(const TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float boundsWidth = bounds.width * bounds.scale;
    const float boundsHeight = bounds.height * bounds.scale;
    const float boundsLeft = bounds.x + 2.0f;
    const float boundsRight = bounds.x + boundsWidth - 2.0f;
    const float boundsBottom = bounds.y + boundsHeight;
    const float topTolerance = std::max(6.0f, tileSize * 0.22f);

    for (const auto& other : m_entities)
    {
        if (!other || other.get() == self)
        {
            continue;
        }
        if (!(HasTag(*other, kTagBatterySwitch) || HasTag(*other, kTagElevator)))
        {
            continue;
        }

        const auto* otherTransform = other->GetComponent<TransformComponent>();
        if (!otherTransform)
        {
            continue;
        }

        const float platformWidth = otherTransform->width * otherTransform->scale;
        const float platformLeft = otherTransform->x;
        const float platformRight = otherTransform->x + platformWidth;
        const bool overlapX = boundsRight > platformLeft && boundsLeft < platformRight;
        const bool onTop = std::fabs(boundsBottom - otherTransform->y) <= topTolerance;
        if (overlapX && onTop)
        {
            return true;
        }
    }
    return false;
}

bool GameScene::SnapBatteryToSwitchOrElevatorTop(TransformComponent& bounds, const Entity* self, float tileSize) const
{
    const float width = bounds.width * bounds.scale;
    const float height = bounds.height * bounds.scale;
    const float left = bounds.x + 2.0f;
    const float right = bounds.x + width - 2.0f;
    const float bottom = bounds.y + height;
    const float topTolerance = std::max(8.0f, tileSize * 0.28f);

    for (const auto& other : m_entities)
    {
        if (!other || other.get() == self)
        {
            continue;
        }
        if (!(HasTag(*other, kTagBatterySwitch) || HasTag(*other, kTagElevator)))
        {
            continue;
        }

        const auto* otherTransform = other->GetComponent<TransformComponent>();
        if (!otherTransform)
        {
            continue;
        }

        const float platformWidth = otherTransform->width * otherTransform->scale;
        const float platformLeft = otherTransform->x;
        const float platformRight = otherTransform->x + platformWidth;
        const bool overlapX = right > platformLeft && left < platformRight;
        if (!overlapX)
        {
            continue;
        }

        if (std::fabs(bottom - otherTransform->y) <= topTolerance)
        {
            bounds.y = otherTransform->y - height;
            return true;
        }
    }

    return false;
}

float GameScene::GetBatteryPushDirectionFromPlayer(const TransformComponent& playerTransform, const TransformComponent& batteryTransform) const
{
    const float actorHeight = playerTransform.height * playerTransform.scale;
    const float batteryHeight = batteryTransform.height * batteryTransform.scale;
    const float actorTop = playerTransform.y;
    const float actorBottom = playerTransform.y + actorHeight;
    const float batteryTop = batteryTransform.y;
    const float batteryBottom = batteryTransform.y + batteryHeight;
    const float verticalTolerance = 4.0f;
    const bool isSidePushContact = actorBottom > batteryTop + verticalTolerance
        && actorTop < batteryBottom - verticalTolerance;
    if (!isSidePushContact)
    {
        return 0.0f;
    }

    const float playerWidth = playerTransform.width * playerTransform.scale;
    const float batteryWidth = batteryTransform.width * batteryTransform.scale;
    const float playerLeft = playerTransform.x;
    const float playerRight = playerTransform.x + playerWidth;
    const float batteryLeft = batteryTransform.x;
    const float batteryRight = batteryTransform.x + batteryWidth;
    const float sideTolerance = 6.0f;

    const bool touchingLeftSide = std::fabs(playerRight - batteryLeft) <= sideTolerance;
    const bool touchingRightSide = std::fabs(playerLeft - batteryRight) <= sideTolerance;
    if (touchingLeftSide)
    {
        return 1.0f;
    }
    if (touchingRightSide)
    {
        return -1.0f;
    }

    return 0.0f;
}

void GameScene::UpdateSingleBattery(
    Entity& batteryEntity,
    Entity* player,
    const std::vector<Entity*>& enemies,
    const std::vector<TransformComponent>& groundPlatforms,
    float deltaTime,
    float tileSize)
{
    auto* battery = batteryEntity.GetComponent<BatteryComponent>();
    auto* transform = batteryEntity.GetComponent<TransformComponent>();
    if (!battery || !transform)
    {
        return;
    }

    const float width = transform->width * transform->scale;
    const float height = transform->height * transform->scale;
    const float previousX = transform->x;
    const float previousY = transform->y;

    const float fallVelocity = std::min(battery->maxFallSpeed, battery->velocityY + battery->gravity * deltaTime);
    battery->velocityY = fallVelocity;
    transform->y += battery->velocityY * deltaTime;

    const bool snapped = TrySnapToGroundUsingPlatforms(*transform, gGroundSnapDistance, groundPlatforms);
    battery->grounded = snapped;
    if (snapped && battery->velocityY > 0.0f)
    {
        battery->velocityY = 0.0f;
    }

    const float mapHeight = GetMapPixelHeight();
    if (transform->y + height > mapHeight)
    {
        transform->y = mapHeight - height;
        battery->grounded = true;
        battery->velocityY = 0.0f;
    }

    const bool fallingHitActive = !battery->grounded && fallVelocity >= battery->fallDamageSpeed;
    bool touchedActor = false;

    if (player && IntersectsEntity(batteryEntity, *player))
    {
        touchedActor = true;
        if (fallingHitActive)
        {
            HandlePlayerDamage(*player, &batteryEntity, "Battery impact damaged player", battery->contactDamage);
        }
    }

    for (Entity* enemyEntity : enemies)
    {
        if (!enemyEntity || enemyEntity == &batteryEntity)
        {
            continue;
        }
        if (!IntersectsEntity(batteryEntity, *enemyEntity))
        {
            continue;
        }
        touchedActor = true;
        if (fallingHitActive)
        {
            HandleEnemyDamage(*enemyEntity, &batteryEntity, battery->contactDamage, "Battery impact damaged enemy");
        }
    }

    if (!fallingHitActive)
    {
        const bool onTopOfSwitchOrElevator = IsBatteryOnTopOfSwitchOrElevator(*transform, &batteryEntity, tileSize);
        if (onTopOfSwitchOrElevator)
        {
            battery->grounded = true;
            battery->velocityY = 0.0f;
            SnapBatteryToSwitchOrElevatorTop(*transform, &batteryEntity, tileSize);
        }

        float pushDirection = 0.0f;
        if (player)
        {
            const auto* playerTransform = player->GetComponent<TransformComponent>();
            if (playerTransform)
            {
                pushDirection = GetBatteryPushDirectionFromPlayer(*playerTransform, *transform);
            }
        }

        if (std::fabs(pushDirection) > 0.1f)
        {
            battery->velocityX = pushDirection * battery->pushSpeed;
        }
        else
        {
            battery->velocityX *= 0.82f;
            if (std::fabs(battery->velocityX) < 6.0f)
            {
                battery->velocityX = 0.0f;
            }
        }
    }
    else
    {
        battery->velocityX *= 0.90f;
    }

    transform->x += battery->velocityX * deltaTime;
    transform->x = std::clamp(transform->x, 0.0f, std::max(0.0f, GetMapPixelWidth() - width));

    if (IsBatteryCollidingWithWorld(*transform, &batteryEntity, tileSize))
    {
        bool steppedUp = false;
        const float maxStepHeight = tileSize * 0.5f;
        const bool onTopOfSwitchOrElevator = IsBatteryOnTopOfSwitchOrElevator(*transform, &batteryEntity, tileSize);
        if (battery->grounded && maxStepHeight > 0.0f && !onTopOfSwitchOrElevator)
        {
            TransformComponent stepCandidate = *transform;
            stepCandidate.y = std::max(0.0f, stepCandidate.y - maxStepHeight);
            if (!IsBatteryCollidingWithWorld(stepCandidate, &batteryEntity, tileSize))
            {
                transform->y = stepCandidate.y;
                steppedUp = true;
            }
        }

        if (!steppedUp)
        {
            transform->x = previousX;
            battery->velocityX = 0.0f;
        }
    }

    if (fallingHitActive && touchedActor)
    {
        battery->velocityY = std::max(0.0f, battery->velocityY * 0.35f);
        transform->y = std::max(previousY, transform->y - tileSize * 0.08f);
    }

    if (SnapBatteryToSwitchOrElevatorTop(*transform, &batteryEntity, tileSize))
    {
        battery->grounded = true;
        battery->velocityY = 0.0f;
    }
}
