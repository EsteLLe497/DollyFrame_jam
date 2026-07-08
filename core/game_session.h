#pragma once

#include <string>
#include <vector>

enum class GameEndReason
{
    None,
    GoalReached,
    TimeUp,
    HpZero,
    BossDefeated,   //forest_boss(boss1) Œ‚”j
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
    bool cameraTutorialCompleted = false;
    std::vector<int> completedTutorialNumbers;
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
void GameSession_SetCameraTutorialCompleted(bool completed);
bool gameSessionIsTutorialCompleted(int tutorialNumber);
void gameSessionSetTutorialCompleted(int tutorialNumber, bool completed);
const std::vector<int>& gameSessionGetCompletedTutorialNumbers();
void gameSessionSetCompletedTutorialNumbers(const std::vector<int>& tutorialNumbers);
const GameSessionState& GameSession_Get();
const std::string& GameSession_GetStartMapCsvPath();
bool GameSession_ShouldLoadSavedProgress();
