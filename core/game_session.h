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
    int parts = 0;
    int photoStorageSlots = 2;
    bool hasRecoveryFilter = false;
    float timeLimit = 30.0f;
    float timeRemaining = 30.0f;
    GameEndReason endReason = GameEndReason::None;
};

void GameSession_Reset(int maxHp, float timeLimit);
void GameSession_SetCurrentHp(int currentHp);
void GameSession_AddParts(int amount);
bool GameSession_SpendParts(int amount);
void GameSession_SetPhotoStorageSlots(int slots);
void GameSession_SetRecoveryFilterOwned(bool owned);
void GameSession_SetTimeRemaining(float timeRemaining);
void GameSession_SetEndReason(GameEndReason reason);
const GameSessionState& GameSession_Get();
