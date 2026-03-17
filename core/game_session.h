#pragma once

enum class GameEndReason
{
    None,
    GoalReached,
    TimeUp,
    HpZero,
};

struct GameSessionState
{
    int maxHp = 3;
    int currentHp = 3;
    float timeLimit = 30.0f;
    float timeRemaining = 30.0f;
    GameEndReason endReason = GameEndReason::None;
};

void GameSession_Reset(int maxHp, float timeLimit);
void GameSession_SetCurrentHp(int currentHp);
void GameSession_SetTimeRemaining(float timeRemaining);
void GameSession_SetEndReason(GameEndReason reason);
const GameSessionState& GameSession_Get();
