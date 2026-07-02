#pragma once

#include <string>

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
    bool hasCameraFlash = false;
    float timeLimit = 30.0f;
    float timeRemaining = 30.0f;
    GameEndReason endReason = GameEndReason::None;
    std::string startMapCsvPath = "assets/maps/stages/forest_v2.csv";
    bool loadSavedProgress = true;
};

void GameSession_Reset(int maxHp, float timeLimit);
void GameSession_SetCurrentHp(int currentHp);
void GameSession_AddParts(int amount);
bool GameSession_SpendParts(int amount);
void GameSession_SetPhotoStorageSlots(int slots);
void GameSession_SetRecoveryFilterOwned(bool owned);
void GameSession_SetCameraFlashOwned(bool owned);
void GameSession_SetTimeRemaining(float timeRemaining);
void GameSession_SetEndReason(GameEndReason reason);
void GameSession_SetStartMapCsvPath(const std::string& startMapCsvPath);
void GameSession_SetLoadSavedProgress(bool loadSavedProgress);
const GameSessionState& GameSession_Get();
const std::string& GameSession_GetStartMapCsvPath();
bool GameSession_ShouldLoadSavedProgress();
