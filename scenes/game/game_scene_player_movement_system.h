#pragma once

#include <algorithm>
#include <vector>

#include "components.h"
#include "game_scene_internal.h"
#include "game_scene_state.h"

namespace game_scene_player_movement_system
{
using namespace game_scene_detail;

constexpr float kPlayerCollisionInsetX = 1.0f;
constexpr float kPlayerCollisionInsetY = 1.0f;

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

template <typename IsSolidTileFn, typename IntersectsSolidObjectFn>
bool CanOccupyTileSpace(
    const TransformComponent& transform,
    const PlayerMovementContext& ctx,
    IsSolidTileFn&& isSolidTile,
    IntersectsSolidObjectFn&& intersectsSolidObject)
{
    const int columnStart = static_cast<int>((transform.x + kPlayerCollisionInsetX) / ctx.tileSize);
    const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - kPlayerCollisionInsetX) / ctx.tileSize);
    const int rowStart = static_cast<int>((transform.y + kPlayerCollisionInsetY) / ctx.tileSize);
    const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - kPlayerCollisionInsetY) / ctx.tileSize);
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

    return !intersectsSolidObject(transform);
}

template <typename IsSolidTileFn, typename IntersectsSolidObjectFn>
void ResolveHorizontalTileCollisions(
    TransformComponent& transform,
    GameScenePlayerState& player,
    const PlayerMovementContext& ctx,
    IsSolidTileFn&& isSolidTile,
    IntersectsSolidObjectFn&& intersectsSolidObject)
{
    transform.x += player.velocityX * ctx.deltaTime;
    const float maxStepHeight = std::clamp(gGroundStepUpHeight, 0.0f, ctx.tileSize * 0.5f);
    if (player.velocityX > 0.0f)
    {
        const int column = static_cast<int>((transform.x + ctx.playerWidth - 1.0f) / ctx.tileSize);
        const int rowStart = static_cast<int>((transform.y + kPlayerCollisionInsetY) / ctx.tileSize);
        const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - kPlayerCollisionInsetY) / ctx.tileSize);
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
                        CanOccupyTileSpace(stepCandidate, ctx, isSolidTile, intersectsSolidObject))
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
        const int rowStart = static_cast<int>((transform.y + kPlayerCollisionInsetY) / ctx.tileSize);
        const int rowEnd = static_cast<int>((transform.y + ctx.playerHeight - kPlayerCollisionInsetY) / ctx.tileSize);
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
                        CanOccupyTileSpace(stepCandidate, ctx, isSolidTile, intersectsSolidObject))
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

template <typename IntersectsSolidObjectFn>
inline void ResolveHorizontalObjectCollisions(
    TransformComponent& transform,
    GameScenePlayerState& player,
    const PlayerMovementContext& ctx,
    const std::vector<TransformComponent>& photoBoxes,
    IntersectsSolidObjectFn&& intersectsSolidObject,
    const std::vector<TransformComponent>& photoSources)
{
    const float maxStepHeight = std::max(0.0f, ctx.tileSize * 0.5f);
    auto intersectsAnyPhotoSource = [&](const TransformComponent& candidate) -> bool
    {
        for (const auto& sourceBounds : photoSources)
        {
            if (IntersectsRect(candidate, sourceBounds))
            {
                return true;
            }
        }
        return false;
    };
    auto tryStepUpOverObject = [&]() -> bool
    {
        if (!player.grounded || maxStepHeight <= 0.0f)
        {
            return false;
        }

        TransformComponent stepCandidate(transform.x, transform.y - maxStepHeight, transform.width, transform.height);
        stepCandidate.scale = transform.scale;
        if (stepCandidate.y < 0.0f)
        {
            return false;
        }
        if (intersectsSolidObject(stepCandidate) || intersectsAnyPhotoSource(stepCandidate))
        {
            return false;
        }

        transform.y = stepCandidate.y;
        return true;
    };

    if (!photoBoxes.empty())
    {
        for (const auto& photoBoxBounds : photoBoxes)
        {
            TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
            playerBounds.scale = transform.scale;
            const float boxLeft = photoBoxBounds.x;
            const float boxTop = photoBoxBounds.y;
            const float boxWidth = photoBoxBounds.width * photoBoxBounds.scale;
            const float boxHeight = photoBoxBounds.height * photoBoxBounds.scale;
            const float boxRight = boxLeft + boxWidth;
            const float boxBottom = boxTop + boxHeight;
            const float currentRight = transform.x + ctx.playerWidth;
            const float currentBottom = transform.y + ctx.playerHeight;
            const bool verticallyOverlapping =
                currentBottom > boxTop + kHorizontalCollisionEpsilon &&
                transform.y < boxBottom - kHorizontalCollisionEpsilon;

            if (verticallyOverlapping)
            {
                // Resolve side-to-side hits before polygon movement checks can treat them as top contact.
                if (player.velocityX > 0.0f &&
                    ctx.previousX + ctx.playerWidth <= boxLeft + kHorizontalCollisionEpsilon &&
                    currentRight > boxLeft)
                {
                    transform.x = boxLeft - ctx.playerWidth;
                    player.velocityX = 0.0f;
                    break;
                }
                if (player.velocityX < 0.0f &&
                    ctx.previousX >= boxRight - kHorizontalCollisionEpsilon &&
                    transform.x < boxRight)
                {
                    transform.x = boxRight;
                    player.velocityX = 0.0f;
                    break;
                }
            }

            if (!IntersectsRect(playerBounds, photoBoxBounds) || !intersectsSolidObject(playerBounds))
            {
                continue;
            }

            float resolvedX = transform.x;
            float low = ctx.previousX;
            float high = transform.x;
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const float mid = (low + high) * 0.5f;
                TransformComponent probe(mid, transform.y, transform.width, transform.height);
                probe.scale = transform.scale;
                if (intersectsSolidObject(probe))
                {
                    high = mid;
                }
                else
                {
                    low = mid;
                    resolvedX = mid;
                }
            }

            if (player.velocityX > 0.0f)
            {
                transform.x = resolvedX;
                player.velocityX = 0.0f;
                break;
            }
            if (player.velocityX < 0.0f)
            {
                transform.x = resolvedX;
                player.velocityX = 0.0f;
                break;
            }
        }
    }

    for (const auto& photoSourceBounds : photoSources)
    {
        TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
        playerBounds.scale = transform.scale;
        if (!IntersectsRect(playerBounds, photoSourceBounds))
        {
            continue;
        }

        const float sourceX = photoSourceBounds.x;
        const float sourceWidth = photoSourceBounds.width * photoSourceBounds.scale;
        if (player.velocityX > 0.0f && ctx.previousX + ctx.playerWidth <= sourceX + kHorizontalCollisionEpsilon)
        {
            if (tryStepUpOverObject())
            {
                continue;
            }

            transform.x = sourceX - ctx.playerWidth;
            player.velocityX = 0.0f;
            break;
        }
        if (player.velocityX < 0.0f && ctx.previousX >= sourceX + sourceWidth - kHorizontalCollisionEpsilon)
        {
            if (tryStepUpOverObject())
            {
                continue;
            }

            transform.x = sourceX + sourceWidth;
            player.velocityX = 0.0f;
            break;
        }
    }
}

template <typename IsSolidTileFn, typename IsPlatformTileFn, typename IsCeilingTileFn, typename TrySnapToGroundFn, typename IntersectsSolidObjectFn>
void ResolveVerticalMotion(
    TransformComponent& transform,
    GameScenePlayerState& player,
    bool wasGrounded,
    const PlayerMovementContext& ctx,
    const std::vector<TransformComponent>& photoBoxes,
    const std::vector<TransformComponent>& photoSources,
    IsSolidTileFn&& isSolidTile,
    IsPlatformTileFn&& isPlatformTile,
    IsCeilingTileFn&& isCeilingTile,
    TrySnapToGroundFn&& trySnapToGround,
    IntersectsSolidObjectFn&& intersectsSolidObject)
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
            const int columnStart = static_cast<int>((transform.x + kPlayerCollisionInsetX) / ctx.tileSize);
            const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - kPlayerCollisionInsetX) / ctx.tileSize);
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
                        intersectsSolidObject(playerBounds) &&
                        // Land on pasted objects only when the player was above their top surface last step.
                        ctx.previousBottom <= photoBoxBounds.y + kSurfaceContactEpsilon)
                    {
                        float resolvedY = transform.y;
                        float low = ctx.previousY;
                        float high = transform.y;
                        for (int iteration = 0; iteration < 8; ++iteration)
                        {
                            const float mid = (low + high) * 0.5f;
                            TransformComponent probe(transform.x, mid, transform.width, transform.height);
                            probe.scale = transform.scale;
                            if (intersectsSolidObject(probe))
                            {
                                high = mid;
                            }
                            else
                            {
                                low = mid;
                                resolvedY = mid;
                            }
                        }

                        transform.y = resolvedY;
                        player.velocityY = 0.0f;
                        player.grounded = true;
                        break;
                    }
                }
            }

            if (!player.grounded)
            {
                for (const auto& photoSourceBounds : photoSources)
                {
                    TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                    playerBounds.scale = transform.scale;
                    const float sourceY = photoSourceBounds.y;
                    if (IntersectsRect(playerBounds, photoSourceBounds) && ctx.previousBottom <= sourceY + kSurfaceContactEpsilon)
                    {
                        transform.y = sourceY - ctx.playerHeight;
                        player.velocityY = 0.0f;
                        player.grounded = true;
                        break;
                    }
                }
            }
        }
        else if (player.velocityY < 0.0f)
        {
            const int rowStart = static_cast<int>(ctx.previousY / ctx.tileSize);
            const int rowEnd = static_cast<int>(transform.y / ctx.tileSize);
            const int columnStart = static_cast<int>((transform.x + kPlayerCollisionInsetX) / ctx.tileSize);
            const int columnEnd = static_cast<int>((transform.x + ctx.playerWidth - kPlayerCollisionInsetX) / ctx.tileSize);
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
                if (IntersectsRect(playerBounds, photoBoxBounds) &&
                    intersectsSolidObject(playerBounds))
                {
                    float resolvedY = transform.y;
                    float low = transform.y;
                    float high = ctx.previousY;
                    for (int iteration = 0; iteration < 8; ++iteration)
                    {
                        const float mid = (low + high) * 0.5f;
                        TransformComponent probe(transform.x, mid, transform.width, transform.height);
                        probe.scale = transform.scale;
                        if (intersectsSolidObject(probe))
                        {
                            low = mid;
                        }
                        else
                        {
                            high = mid;
                            resolvedY = mid;
                        }
                    }

                    transform.y = resolvedY;
                    player.velocityY = 0.0f;
                    break;
                }
            }

            for (const auto& photoSourceBounds : photoSources)
            {
                TransformComponent playerBounds(transform.x, transform.y, transform.width, transform.height);
                playerBounds.scale = transform.scale;
                const float sourceY = photoSourceBounds.y;
                const float sourceHeight = photoSourceBounds.height * photoSourceBounds.scale;
                if (IntersectsRect(playerBounds, photoSourceBounds) && ctx.previousY >= sourceY + sourceHeight - kSurfaceContactEpsilon)
                {
                    transform.y = sourceY + sourceHeight;
                    player.velocityY = 0.0f;
                    break;
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

inline void UpdateCamera(
    float& cameraX,
    float& cameraY,
    float playerX,
    float playerY,
    float playerWidth,
    float playerHeight,
    float cameraVisibleWidth,
    float cameraVisibleHeight,
    float mapWidth,
    float mapHeight,
    float cameraYOffsetY,
    float deltaTime,
    bool followY)
{
    const float targetX = std::clamp(
        playerX + playerWidth * 0.5f - cameraVisibleWidth * 0.5f,
        0.0f,
        std::max(0.0f, mapWidth - cameraVisibleWidth));
    const float followX = std::clamp(gCameraFollowSpeedX * deltaTime, 0.0f, 1.0f);
    cameraX += (targetX - cameraX) * followX;

    if (!followY)
    {
        cameraY = 0.0f;
        return;
    }

    const float targetY = std::clamp(
        playerY + playerHeight * 0.5f - cameraVisibleHeight * 0.5f + cameraYOffsetY,
        0.0f,
        std::max(0.0f, mapHeight - cameraVisibleHeight));
    // Downward follow tends to feel laggy, so apply a stronger rate only when camera moves down.
    const bool movingCameraDown = targetY > cameraY;
    const float downFollowBoost = movingCameraDown ? 1.8f : 1.0f;
    const float followYRate = std::clamp(gCameraFollowSpeedY * downFollowBoost * deltaTime, 0.0f, 1.0f);
    cameraY += (targetY - cameraY) * followYRate;
}
}
