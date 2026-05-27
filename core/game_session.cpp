#include "pch.h"

#include "game_session.h"

#include <algorithm>

namespace
{
    GameSessionState g_sessionState;
}

void GameSession_Reset(int maxHp, float timeLimit)
{
    g_sessionState.maxHp = maxHp;
    g_sessionState.currentHp = maxHp;
    g_sessionState.timeLimit = timeLimit;
    g_sessionState.timeRemaining = timeLimit;
    g_sessionState.endReason = GameEndReason::None;
}

void GameSession_SetCurrentHp(int currentHp)
{
    g_sessionState.currentHp = std::clamp(currentHp, 0, g_sessionState.maxHp);
}

void GameSession_SetTimeRemaining(float timeRemaining)
{
    g_sessionState.timeRemaining = std::max(0.0f, timeRemaining);
}

void GameSession_SetEndReason(GameEndReason reason)
{
    g_sessionState.endReason = reason;
}

const GameSessionState& GameSession_Get()
{
    return g_sessionState;
}
