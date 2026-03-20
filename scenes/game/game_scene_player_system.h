#pragma once

#include <algorithm>
#include <cmath>

#include "game_scene_state.h"
#include "input.h"

namespace game_scene_player_system
{
struct PlayerControlState
{
    float moveAxis = 0.0f;
    bool jumpPressed = false;
    bool dodgePressed = false;
};

inline PlayerControlState SampleControls(bool blockPlayerInput)
{
    PlayerControlState state{};
    if (blockPlayerInput)
    {
        return state;
    }

    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        state.moveAxis -= 1.0f;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        state.moveAxis += 1.0f;
    }

    state.moveAxis += Input_GetAxis(InputAxis::MoveX);
    state.moveAxis = std::clamp(state.moveAxis, -1.0f, 1.0f);
    if (std::fabs(state.moveAxis) < 0.15f)
    {
        state.moveAxis = 0.0f;
    }

    state.jumpPressed = Input_IsActionPressed(InputAction::Jump);
    state.dodgePressed = Input_IsActionPressed(InputAction::Dodge);
    return state;
}

inline void TickDodgeState(GameScenePlayerState& player, float deltaTime)
{
    player.dodgeRemaining = std::max(0.0f, player.dodgeRemaining - deltaTime);
    player.dodgeCooldownRemaining = std::max(0.0f, player.dodgeCooldownRemaining - deltaTime);
}

inline void UpdateFacingFromMoveAxis(GameScenePlayerState& player, float moveAxis)
{
    if (moveAxis > 0.1f)
    {
        player.facingRight = true;
    }
    else if (moveAxis < -0.1f)
    {
        player.facingRight = false;
    }
}

inline bool TryBeginDodge(
    GameScenePlayerState& player,
    float moveAxis,
    float dodgeDuration,
    float dodgeCooldown)
{
    if (player.dodgeRemaining > 0.0f || player.dodgeCooldownRemaining > 0.0f)
    {
        return false;
    }

    player.dodgeDirection = moveAxis != 0.0f
        ? (moveAxis > 0.0f ? 1.0f : -1.0f)
        : (player.facingRight ? 1.0f : -1.0f);
    player.facingRight = player.dodgeDirection > 0.0f;
    player.dodgeRemaining = dodgeDuration;
    player.dodgeCooldownRemaining = dodgeCooldown;
    return true;
}

inline float GetHorizontalVelocity(const GameScenePlayerState& player, float moveAxis, float dodgeSpeed, float moveSpeed)
{
    return player.dodgeRemaining > 0.0f
        ? player.dodgeDirection * dodgeSpeed
        : moveAxis * moveSpeed;
}
}
