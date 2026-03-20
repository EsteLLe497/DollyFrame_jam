#pragma once

#include <algorithm>
#include <cmath>

#include "components.h"
#include "game_scene_internal.h"
#include "game_scene_state.h"

namespace game_scene_player_visual_system
{
using namespace game_scene_detail;

inline constexpr float kPlayerAfterimageLifetime = 0.18f;
inline constexpr float kPlayerAfterimageSpawnInterval = 0.03f;
inline constexpr int kMaxPlayerAfterimages = 8;
inline constexpr float kPlayerVisualSmoothing = 14.0f;
inline constexpr float kPlayerLandingDecay = 6.5f;
inline constexpr float kPlayerJumpDecay = 5.0f;
inline constexpr float kPlayerDodgeDecay = 7.5f;

inline void SetSpriteSheetCell1Based(SpriteRenderComponent& sprite, int row, int column, int rows = 4, int columns = 4)
{
    const float cellWidth = 1.0f / static_cast<float>(columns);
    const float cellHeight = 1.0f / static_cast<float>(rows);
    sprite.SetSourceRect(
        static_cast<float>(column - 1) * cellWidth,
        static_cast<float>(row - 1) * cellHeight,
        cellWidth,
        cellHeight);
}

inline void UpdatePresentation(
    GameScenePlayerState& playerState,
    Entity& player,
    float deltaTime,
    float moveAxis,
    bool wasGrounded,
    bool isDodging,
    bool landedThisFrame)
{
    auto* sprite = player.GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    sprite->SetFlipX(false);

    if (landedThisFrame)
    {
        playerState.landingImpact = 1.0f;
    }
    if (!wasGrounded && playerState.velocityY < -80.0f)
    {
        playerState.jumpStretch = 1.0f;
    }
    if (isDodging)
    {
        playerState.dodgeStretch = 1.0f;
    }

    playerState.landingImpact = std::max(0.0f, playerState.landingImpact - deltaTime * kPlayerLandingDecay);
    playerState.jumpStretch = std::max(0.0f, playerState.jumpStretch - deltaTime * kPlayerJumpDecay);
    playerState.dodgeStretch = std::max(0.0f, playerState.dodgeStretch - deltaTime * kPlayerDodgeDecay);

    const float horizontalSpeedRatio = Clamp01(std::fabs(playerState.velocityX) / std::max(1.0f, gPlayerMoveSpeed));
    if (playerState.grounded && horizontalSpeedRatio > 0.05f)
    {
        playerState.runAnimationTime += deltaTime * (2.6f + horizontalSpeedRatio * 5.2f);
    }

    int frameRow = 2;
    int frameColumn = 2;
    if (isDodging)
    {
        frameRow = 3;
        frameColumn = playerState.facingRight ? 2 : 3;
    }
    else if (!playerState.grounded)
    {
        if (playerState.velocityY < -40.0f)
        {
            frameRow = 1;
            frameColumn = playerState.facingRight ? 2 : 4;
        }
        else
        {
            frameRow = 1;
            frameColumn = playerState.facingRight ? 1 : 3;
        }
    }
    else if (horizontalSpeedRatio > 0.20f)
    {
        frameRow = 3;
        frameColumn = playerState.facingRight ? 1 : 4;
    }
    else
    {
        frameRow = 2;
        frameColumn = playerState.facingRight ? 2 : 1;
    }

    SetSpriteSheetCell1Based(*sprite, frameRow, frameColumn);

    float targetScaleX = 1.0f;
    float targetScaleY = 1.0f;
    float targetOffsetY = 0.0f;
    float targetRotation = 0.0f;

    if (playerState.grounded)
    {
        const float runWave = std::sin(playerState.runAnimationTime * 6.2831853f);
        const float runBounce = std::fabs(runWave);
        targetScaleX += runBounce * 0.05f * horizontalSpeedRatio;
        targetScaleY -= runBounce * 0.07f * horizontalSpeedRatio;
        targetOffsetY += runBounce * 1.8f * horizontalSpeedRatio;
        targetRotation += runWave * 0.03f * horizontalSpeedRatio;
        targetRotation += moveAxis * 0.035f;
    }
    else if (playerState.velocityY < 0.0f)
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

    targetScaleX += playerState.landingImpact * 0.14f;
    targetScaleY -= playerState.landingImpact * 0.18f;
    targetOffsetY += playerState.landingImpact * 3.5f;

    targetScaleX -= playerState.jumpStretch * 0.07f;
    targetScaleY += playerState.jumpStretch * 0.13f;
    targetOffsetY -= playerState.jumpStretch * 2.5f;

    targetScaleX += playerState.dodgeStretch * 0.13f;
    targetScaleY -= playerState.dodgeStretch * 0.10f;
    targetRotation += (playerState.facingRight ? 1.0f : -1.0f) * playerState.dodgeStretch * 0.08f;

    const float blend = std::min(1.0f, deltaTime * kPlayerVisualSmoothing);
    playerState.visualScaleX += (targetScaleX - playerState.visualScaleX) * blend;
    playerState.visualScaleY += (targetScaleY - playerState.visualScaleY) * blend;
    playerState.visualOffsetY += (targetOffsetY - playerState.visualOffsetY) * blend;
    playerState.visualRotation += (targetRotation - playerState.visualRotation) * blend;

    sprite->SetRenderScale(playerState.visualScaleX, playerState.visualScaleY);
    sprite->SetRenderOffset(0.0f, playerState.visualOffsetY);
    sprite->SetRenderRotationOffset(playerState.visualRotation);
}

inline void UpdateAfterimages(GameScenePlayerState& playerState, float deltaTime)
{
    for (auto& afterimage : playerState.afterimages)
    {
        afterimage.life = std::max(0.0f, afterimage.life - deltaTime);
    }
    playerState.afterimages.erase(
        std::remove_if(
            playerState.afterimages.begin(),
            playerState.afterimages.end(),
            [](const PlayerAfterimage& afterimage)
            {
                return afterimage.life <= 0.0f;
            }),
        playerState.afterimages.end());
}

inline void TrySpawnAfterimage(GameScenePlayerState& playerState, const TransformComponent& transform)
{
    const bool shouldAddAfterimage =
        playerState.afterimages.empty() ||
        (kPlayerAfterimageLifetime - playerState.afterimages.front().life) >= kPlayerAfterimageSpawnInterval;
    if (!shouldAddAfterimage)
    {
        return;
    }

    PlayerAfterimage afterimage;
    afterimage.x = transform.x;
    afterimage.y = transform.y;
    afterimage.rotation = transform.rotation;
    afterimage.scale = transform.scale;
    afterimage.flipX = playerState.facingRight ? false : true;
    afterimage.life = kPlayerAfterimageLifetime;
    playerState.afterimages.insert(playerState.afterimages.begin(), afterimage);
    if (static_cast<int>(playerState.afterimages.size()) > kMaxPlayerAfterimages)
    {
        playerState.afterimages.resize(kMaxPlayerAfterimages);
    }
}
}
