#pragma once

#include "game_scene_state.h"

struct GameScenePlayerRespawnContext
{
    float playerX = 0.0f;
    float playerY = 0.0f;
    float playerWidth = 0.0f;
    float playerHeight = 0.0f;
    float mapWidth = 0.0f;
    float mapHeight = 0.0f;
    float cameraViewWidth = 0.0f;
    float cameraViewHeight = 0.0f;
    bool followCameraY = false;
};

struct GameScenePlayerRespawnResult
{
    float cameraX = 0.0f;
    float cameraY = 0.0f;
};

namespace game_scene_player_state_logic
{
    GameScenePlayerRespawnResult ComputeRespawnCamera(const GameScenePlayerRespawnContext& context);

    void ResetPlayerStateAfterRespawn(GameScenePlayerState& playerState);
}
