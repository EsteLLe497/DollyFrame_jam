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
    g_sessionState.parts = 0;
    g_sessionState.photoStorageSlots = 2;
    g_sessionState.hasRecoveryFilter = false;
    g_sessionState.timeLimit = timeLimit;
    g_sessionState.timeRemaining = timeLimit;
    g_sessionState.endReason = GameEndReason::None;
    g_sessionState.loadSavedProgress = true;
}

void GameSession_SetCurrentHp(int currentHp)
{
    g_sessionState.currentHp = std::clamp(currentHp, 0, g_sessionState.maxHp);
}

void GameSession_AddParts(int amount)
{
    g_sessionState.parts = std::max(0, g_sessionState.parts + std::max(0, amount));
}

bool GameSession_SpendParts(int amount)
{
    const int cost = std::max(0, amount);
    if (g_sessionState.parts < cost)
    {
        return false;
    }

    g_sessionState.parts -= cost;
    return true;
}

void GameSession_SetPhotoStorageSlots(int slots)
{
    g_sessionState.photoStorageSlots = std::clamp(slots, 1, 3);
}

void GameSession_SetRecoveryFilterOwned(bool owned)
{
    g_sessionState.hasRecoveryFilter = owned;
}

void GameSession_SetTimeRemaining(float timeRemaining)
{
    g_sessionState.timeRemaining = std::max(0.0f, timeRemaining);
}

void GameSession_SetEndReason(GameEndReason reason)
{
    g_sessionState.endReason = reason;
}

void GameSession_SetStartMapCsvPath(const std::string& startMapCsvPath)
{
    g_sessionState.startMapCsvPath = startMapCsvPath.empty()
        ? std::string("assets/maps/stages/forest.csv")
        : startMapCsvPath;
}

void GameSession_SetLoadSavedProgress(bool loadSavedProgress)
{
    g_sessionState.loadSavedProgress = loadSavedProgress;
}

const GameSessionState& GameSession_Get()
{
    return g_sessionState;
}

const std::string& GameSession_GetStartMapCsvPath()
{
    return g_sessionState.startMapCsvPath;
}

bool GameSession_ShouldLoadSavedProgress()
{
    return g_sessionState.loadSavedProgress;
}
