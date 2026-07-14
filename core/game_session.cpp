#include "pch.h"

#include "game_session.h"
#include "photo_log.h"

#include <algorithm>

namespace
{
    GameSessionState g_sessionState;

    void SyncRecoveryFilterOwnedFlag()
    {
        g_sessionState.hasRecoveryFilter = g_sessionState.recoveryFilterCount > 0;
    }
}

void GameSession_Reset(int maxHp, float timeLimit)
{
    const bool carryShopPurchases = g_sessionState.carryShopPurchasesToNextStage;
    const int carriedParts = g_sessionState.parts;
    const int carriedPhotoStorageSlots = g_sessionState.photoStorageSlots;
    const int carriedRecoveryFilterCount = g_sessionState.recoveryFilterCount;
    const bool carriedCameraFlash = g_sessionState.hasCameraFlash;

    g_sessionState.maxHp = maxHp;
    g_sessionState.currentHp = maxHp;
    g_sessionState.parts = 0;
    g_sessionState.partsCollectedTotal = 0;
    g_sessionState.photoStorageSlots = 2;
    g_sessionState.recoveryFilterCount = 0;
    g_sessionState.hasRecoveryFilter = false;
    g_sessionState.hasCameraFlash = false;
    g_sessionState.timeLimit = timeLimit;
    g_sessionState.timeRemaining = timeLimit;
    g_sessionState.clearTimeSeconds = 0.0f;
    g_sessionState.elapsedSeconds = 0.0f;
    g_sessionState.endReason = GameEndReason::None;
    g_sessionState.loadSavedProgress = true;
    g_sessionState.cameraTutorialCompleted = false;
    g_sessionState.completedTutorialNumbers.clear();
    g_sessionState.carryShopPurchasesToNextStage = false;
    if (carryShopPurchases)
    {
        g_sessionState.parts = std::max(0, carriedParts);
        g_sessionState.photoStorageSlots = std::clamp(carriedPhotoStorageSlots, 1, 3);
        g_sessionState.recoveryFilterCount = std::clamp(carriedRecoveryFilterCount, 0, 3);
        SyncRecoveryFilterOwnedFlag();
        g_sessionState.hasCameraFlash = carriedCameraFlash;
    }
    PhotoLog_Reset();//新しい周回の開始でログをクリア
}

void GameSession_SetCurrentHp(int currentHp)
{
    g_sessionState.currentHp = std::clamp(currentHp, 0, g_sessionState.maxHp);
}

void GameSession_AddParts(int amount)
{
    const int addedAmount = std::max(0, amount);
    g_sessionState.parts = std::max(0, g_sessionState.parts + addedAmount);
    g_sessionState.partsCollectedTotal += addedAmount;   // 追加
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
    g_sessionState.recoveryFilterCount = owned ? std::max(1, g_sessionState.recoveryFilterCount) : 0;
    g_sessionState.recoveryFilterCount = std::clamp(g_sessionState.recoveryFilterCount, 0, 3);
    SyncRecoveryFilterOwnedFlag();
}

void GameSession_SetRecoveryFilterCount(int count)
{
    g_sessionState.recoveryFilterCount = std::clamp(count, 0, 3);
    SyncRecoveryFilterOwnedFlag();
}

bool GameSession_AddRecoveryFilter(int amount)
{
    const int addedAmount = std::max(0, amount);
    if (addedAmount <= 0 || g_sessionState.recoveryFilterCount >= 3)
    {
        return false;
    }

    g_sessionState.recoveryFilterCount = std::clamp(g_sessionState.recoveryFilterCount + addedAmount, 0, 3);
    SyncRecoveryFilterOwnedFlag();
    return true;
}

bool GameSession_ConsumeRecoveryFilter()
{
    if (g_sessionState.recoveryFilterCount <= 0)
    {
        SyncRecoveryFilterOwnedFlag();
        return false;
    }

    --g_sessionState.recoveryFilterCount;
    SyncRecoveryFilterOwnedFlag();
    return true;
}

void GameSession_SetCameraFlashOwned(bool owned)
{
    g_sessionState.hasCameraFlash = owned;
}

void GameSession_SetTimeRemaining(float timeRemaining)
{
    g_sessionState.timeRemaining = std::max(0.0f, timeRemaining);
}

void GameSession_AddElapsedSeconds(float deltaTime)
{
    if (deltaTime > 0.0f)
    {
        g_sessionState.elapsedSeconds += deltaTime;
    }
}

void GameSession_SetEndReason(GameEndReason reason)
{
    g_sessionState.endReason = reason;
    if (reason != GameEndReason::None)
    {
        g_sessionState.clearTimeSeconds = g_sessionState.elapsedSeconds;  // ← こちらに変更
    }
}

void GameSession_SetStartMapCsvPath(const std::string& startMapCsvPath)
{
    g_sessionState.startMapCsvPath = startMapCsvPath.empty()
        ? std::string("assets/maps/stages/forest_v2.csv")
        : startMapCsvPath;
}

void GameSession_SetLastMapCsvPath(const std::string& lastMapCsvPath)
{
    g_sessionState.lastMapCsvPath = lastMapCsvPath;
}

const std::string& GameSession_GetLastMapCsvPath()
{
    return g_sessionState.lastMapCsvPath;
}

void GameSession_SetLoadSavedProgress(bool loadSavedProgress)
{
    g_sessionState.loadSavedProgress = loadSavedProgress;
}

void GameSession_SetCarryShopPurchasesToNextStage(bool carry)
{
    g_sessionState.carryShopPurchasesToNextStage = carry;
}

void GameSession_SetCameraTutorialCompleted(bool completed)
{
    gameSessionSetTutorialCompleted(1, completed);
}

bool gameSessionIsTutorialCompleted(int tutorialNumber)
{
    return std::find(
        g_sessionState.completedTutorialNumbers.begin(),
        g_sessionState.completedTutorialNumbers.end(),
        tutorialNumber) != g_sessionState.completedTutorialNumbers.end();
}

void gameSessionSetTutorialCompleted(int tutorialNumber, bool completed)
{
    if (tutorialNumber <= 0)
    {
        return;
    }

    auto& completedNumbers = g_sessionState.completedTutorialNumbers;
    const auto found = std::find(
        completedNumbers.begin(),
        completedNumbers.end(),
        tutorialNumber);
    if (completed && found == completedNumbers.end())
    {
        completedNumbers.push_back(tutorialNumber);
        std::sort(completedNumbers.begin(), completedNumbers.end());
    }
    else if (!completed && found != completedNumbers.end())
    {
        completedNumbers.erase(found);
    }

    // 旧セーブデータとデバッグUI向けの互換フラグです。
    g_sessionState.cameraTutorialCompleted = gameSessionIsTutorialCompleted(1);
}

const std::vector<int>& gameSessionGetCompletedTutorialNumbers()
{
    return g_sessionState.completedTutorialNumbers;
}

void gameSessionSetCompletedTutorialNumbers(const std::vector<int>& tutorialNumbers)
{
    g_sessionState.completedTutorialNumbers.clear();
    for (int tutorialNumber : tutorialNumbers)
    {
        gameSessionSetTutorialCompleted(tutorialNumber, true);
    }
    g_sessionState.cameraTutorialCompleted = gameSessionIsTutorialCompleted(1);
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
