#include "pch.h"

#include "game_scene_player_state_logic.h"

#include <algorithm>

namespace game_scene_player_state_logic
{
    GameScenePlayerRespawnResult ComputeRespawnCamera(const GameScenePlayerRespawnContext& context)
    {
        GameScenePlayerRespawnResult result;
        const float targetCameraX = context.playerX - (context.cameraViewWidth - context.playerWidth) * 0.5f;
        result.cameraX = std::clamp(targetCameraX, 0.0f, std::max(0.0f, context.mapWidth - context.cameraViewWidth));
        if (context.followCameraY)
        {
            const float targetCameraY = context.playerY - (context.cameraViewHeight - context.playerHeight) * 0.5f;
            result.cameraY = std::clamp(targetCameraY, 0.0f, std::max(0.0f, context.mapHeight - context.cameraViewHeight));
        }
        else
        {
            result.cameraY = 0.0f;
        }
        return result;
    }

    void ResetPlayerStateAfterRespawn(GameScenePlayerState& playerState)
    {
        playerState.velocityX = 0.0f;
        playerState.velocityY = 0.0f;
        playerState.grounded = false;
        playerState.coyoteTimeRemaining = 0.0f;
        playerState.dodgeRemaining = 0.0f;
        playerState.dodgeCooldownRemaining = 0.0f;
        playerState.afterimages.clear();
        playerState.visualOffsetY = 0.0f;
        playerState.visualRotation = 0.0f;
        playerState.visualScaleX = 1.0f;
        playerState.visualScaleY = 1.0f;
    }
}
