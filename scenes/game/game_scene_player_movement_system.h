#pragma once

#include <algorithm>
#include <vector>

#include "components.h"
#include "game_scene_internal.h"
#include "game_scene_state.h"

namespace game_scene_player_movement_system
{
using namespace game_scene_detail;

struct PlayerMovementContext
{
    float deltaTime = 0.0f;
    float tileSize = 0.0f;
    float playerWidth = 0.0f;
    float playerHeight = 0.0f;
    float mapWidth = 0.0f;
    float mapHeight = 0.0f;
    float previousX = 0.0f;
    float previousY = 0.0f;
    float previousBottom = 0.0f;
    float verticalSnapDistance = 0.0f;
};

template <typename IsSolidTileFn>
bool CanOccupyTileSpace(const TransformComponent& transform, const PlayerMovementContext& ctx, IsSolidTileFn&& isSolidTile)
{
    const int columnStart = static_cast<int>((transform.x + 6.0f) / ctx.tileSize);
    const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - 6.0f) / ctx.tileSize);
    const int rowStart = static_cast<int>((transform.y + 4.0f) / ctx.tileSize);
    const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - 4.0f) / ctx.tileSize);
    for (int row = rowStart; row <= rowEnd; ++row)
    {
        for (int column = columnStart; column <= columnEnd; ++column)
        {
            if (isSolidTile(column, row))
            {
                return false;
            }
        }
    }

    return true;
}

template <typename IsSolidTileFn>
void ResolveHorizontalTileCollisions(
    TransformComponent& transform,
    GameScenePlayerState& player,
    const PlayerMovementContext& ctx,
    IsSolidTileFn&& isSolidTile)
{
    transform.x += player.velocityX * ctx.deltaTime;
    const float maxStepHeight = std::clamp(gGroundStepUpHeight, 0.0f, ctx.tileSize * 0.5f);
    if (player.velocityX > 0.0f)
    {
        const int column = static_cast<int>((transform.x + ctx.playerWidth - 1.0f) / ctx.tileSize);
        const int rowStart = static_cast<int>((transform.y + 4.0f) / ctx.tileSize);
        const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - 4.0f) / ctx.tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (isSolidTile(column, row))
            {
                bool steppedUp = false;
                if (maxStepHeight > 0.0f)
                {
                    TransformComponent stepCandidate(transform.x, transform.y - maxStepHeight, transform.width, transform.height);
                    stepCandidate.scale = transform.scale;
                    if (stepCandidate.y >= 0.0f &&
                        CanOccupyTileSpace(stepCandidate, ctx, isSolidTile))
                    {
                        transform.y = stepCandidate.y;
                        steppedUp = true;
                    }
                }

                if (steppedUp)
                {
                    continue;
                }

                transform.x = static_cast<float>(column) * ctx.tileSize - ctx.playerWidth;
                player.velocityX = 0.0f;
                break;
            }
        }
    }
    else if (player.velocityX < 0.0f)
    {
        const int column = static_cast<int>(transform.x / ctx.tileSize);
        const int rowStart = static_cast<int>((transform.y + 4.0f) / ctx.tileSize);
        const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - 4.0f) / ctx.tileSize);
        for (int row = rowStart; row <= rowEnd; ++row)
        {
            if (isSolidTile(column, row))
            {
                bool steppedUp = false;
                if (maxStepHeight > 0.0f)
                {
                    TransformComponent stepCandidate(transform.x, transform.y - maxStepHeight, transform.width, transform.height);
                    stepCandidate.scale = transform.scale;
                    if (stepCandidate.y >= 0.0f &&
                        CanOccupyTileSpace(stepCandidate, ctx, isSolidTile))
                    {
                        transform.y = stepCandidate.y;
                        steppedUp = true;
                    }
                }

                if (steppedUp)
                {
                    continue;
                }

                transform.x = static_cast<float>(column + 1) * ctx.tileSize;
                player.velocityX = 0.0f;
                break;
            }
        }
    }

    transform.x = std::clamp(transform.x, 0.0f, std::max(0.0f, ctx.mapWidth - ctx.playerWidth));
}

inline void ResolveHorizontalObjectCollisions(
    TransformComponent& transform,
    GameScenePlayerState& player,
    const PlayerMovementContext& ctx,
    const std::vector<TransformComponent>& photoBoxes,
    bool hasPhotoSource,
    float photoSourceX,
    float photoSourceY,
    float photoSourceWidth,
    float photoSourceHeight)
{
    if (!photoBoxes.empty())
    {
        for (const auto& photoBoxBounds : photoBoxes)
        {
            TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
            playerBounds.scale = transform.scale;
            if (!IntersectsRect(playerBounds, photoBoxBounds))
            {
                continue;
            }

            const float photoBoxX = photoBoxBounds.x;
            const float photoBoxWidth = photoBoxBounds.width * photoBoxBounds.scale;
            if (player.velocityX > 0.0f && ctx.previousX + ctx.playerWidth <= photoBoxX + kHorizontalCollisionEpsilon)
            {
                transform.x = photoBoxX - ctx.playerWidth;
                player.velocityX = 0.0f;
                break;
            }
            if (player.velocityX < 0.0f && ctx.previousX >= photoBoxX + photoBoxWidth - kHorizontalCollisionEpsilon)
            {
                transform.x = photoBoxX + photoBoxWidth;
                player.velocityX = 0.0f;
                break;
            }
        }
    }

    if (!hasPhotoSource)
    {
        return;
    }

    TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
    playerBounds.scale = transform.scale;
    TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
    if (!IntersectsRect(playerBounds, photoSourceBounds))
    {
        return;
    }

    if (player.velocityX > 0.0f && ctx.previousX + ctx.playerWidth <= photoSourceX + kHorizontalCollisionEpsilon)
    {
        transform.x = photoSourceX - ctx.playerWidth;
        player.velocityX = 0.0f;
    }
    else if (player.velocityX < 0.0f && ctx.previousX >= photoSourceX + photoSourceWidth - kHorizontalCollisionEpsilon)
    {
        transform.x = photoSourceX + photoSourceWidth;
        player.velocityX = 0.0f;
    }
}

template <typename IsSolidTileFn, typename IsPlatformTileFn, typename IsCeilingTileFn, typename TrySnapToGroundFn>
void ResolveVerticalMotion(
    TransformComponent& transform,
    GameScenePlayerState& player,
    bool wasGrounded,
    const PlayerMovementContext& ctx,
    const std::vector<TransformComponent>& photoBoxes,
    bool hasPhotoSource,
    float photoSourceX,
    float photoSourceY,
    float photoSourceWidth,
    float photoSourceHeight,
    IsSolidTileFn&& isSolidTile,
    IsPlatformTileFn&& isPlatformTile,
    IsCeilingTileFn&& isCeilingTile,
    TrySnapToGroundFn&& trySnapToGround)
{
    player.grounded = false;
    if (player.velocityY == 0.0f && wasGrounded)
    {
        player.grounded = trySnapToGround(transform, ctx.verticalSnapDistance);
    }
    else
    {
        transform.y += player.velocityY * ctx.deltaTime;
        if (player.velocityY > 0.0f)
        {
            const int rowStart = static_cast<int>(ctx.previousBottom / ctx.tileSize);
            const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - 1.0f) / ctx.tileSize);
            const int columnStart = static_cast<int>((transform.x + 6.0f) / ctx.tileSize);
            const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - 6.0f) / ctx.tileSize);
            for (int row = rowStart; row <= rowEnd; ++row)
            {
                bool collided = false;
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    const bool solidHit = isSolidTile(column, row);
                    const bool platformHit =
                        isPlatformTile(column, row) &&
                        ctx.previousBottom <= static_cast<float>(row) * ctx.tileSize + 8.0f;
                    if (solidHit || platformHit)
                    {
                        transform.y = static_cast<float>(row) * ctx.tileSize - ctx.playerHeight;
                        player.velocityY = 0.0f;
                        player.grounded = true;
                        collided = true;
                        break;
                    }
                }
                if (collided)
                {
                    break;
                }
            }

            if (!player.grounded)
            {
                for (const auto& photoBoxBounds : photoBoxes)
                {
                    TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                    playerBounds.scale = transform.scale;
                    if (IntersectsRect(playerBounds, photoBoxBounds) &&
                        ctx.previousBottom <= photoBoxBounds.y + kSurfaceContactEpsilon)
                    {
                        transform.y = photoBoxBounds.y - ctx.playerHeight;
                        player.velocityY = 0.0f;
                        player.grounded = true;
                        break;
                    }
                }
            }

            if (!player.grounded && hasPhotoSource)
            {
                TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                playerBounds.scale = transform.scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && ctx.previousBottom <= photoSourceY + kSurfaceContactEpsilon)
                {
                    transform.y = photoSourceY - ctx.playerHeight;
                    player.velocityY = 0.0f;
                    player.grounded = true;
                }
            }
        }
        else if (player.velocityY < 0.0f)
        {
            const int rowStart = static_cast<int>(ctx.previousY / ctx.tileSize);
            const int rowEnd = static_cast<int>(transform.y / ctx.tileSize);
            const int columnStart = static_cast<int>((transform.x + 6.0f) / ctx.tileSize);
            const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - 6.0f) / ctx.tileSize);
            for (int row = rowStart; row >= rowEnd; --row)
            {
                bool collided = false;
                for (int column = columnStart; column <= columnEnd; ++column)
                {
                    if (isCeilingTile(column, row))
                    {
                        transform.y = static_cast<float>(row + 1) * ctx.tileSize;
                        player.velocityY = 0.0f;
                        collided = true;
                        break;
                    }
                }
                if (collided)
                {
                    break;
                }
            }

            for (const auto& photoBoxBounds : photoBoxes)
            {
                TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                playerBounds.scale = transform.scale;
                const float photoBoxHeight = photoBoxBounds.height * photoBoxBounds.scale;
                if (IntersectsRect(playerBounds, photoBoxBounds) &&
                    ctx.previousY >= photoBoxBounds.y + photoBoxHeight - kSurfaceContactEpsilon)
                {
                    transform.y = photoBoxBounds.y + photoBoxHeight;
                    player.velocityY = 0.0f;
                    break;
                }
            }

            if (hasPhotoSource)
            {
                TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                playerBounds.scale = transform.scale;
                TransformComponent photoSourceBounds(photoSourceX, photoSourceY, photoSourceWidth, photoSourceHeight);
                if (IntersectsRect(playerBounds, photoSourceBounds) && ctx.previousY >= photoSourceY + photoSourceHeight - kSurfaceContactEpsilon)
                {
                    transform.y = photoSourceY + photoSourceHeight;
                    player.velocityY = 0.0f;
                }
            }
        }

        if (!player.grounded && player.velocityY >= 0.0f)
        {
            if (trySnapToGround(transform, ctx.verticalSnapDistance))
            {
                player.velocityY = 0.0f;
                player.grounded = true;
            }
        }
    }

    if (transform.y + ctx.playerHeight >= ctx.mapHeight)
    {
        transform.y = ctx.mapHeight - ctx.playerHeight;
        player.velocityY = 0.0f;
        player.grounded = true;
    }
}

inline void UpdateCameraX(float& cameraX, float playerX, float playerWidth, float mapWidth, float deltaTime)
{
    const float cameraTarget = std::clamp(
        playerX + playerWidth * 0.5f - gCameraViewWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - gCameraViewWidth));
    cameraX += (cameraTarget - cameraX) * std::min(1.0f, deltaTime * 8.0f);
}
}
