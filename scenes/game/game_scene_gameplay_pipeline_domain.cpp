#include "pch.h"

#include "game_scene_internal.h"

using namespace game_scene_detail;

void GameScene::RunGameplayFrame(float gameplayDeltaTime)
{
    UpdateGameplayActors(gameplayDeltaTime);
    ResolveGameplayOutcomes(gameplayDeltaTime);
    FlushPendingEntities();
}

void GameScene::UpdateGameplayActors(float gameplayDeltaTime)
{
    UpdatePlayer(gameplayDeltaTime);
    HandlePhotoCapture();
    HandlePhotoSpawn();
    UpdateBarrels(gameplayDeltaTime);
    UpdateBatteries(gameplayDeltaTime);
    UpdateLaserTurrets(gameplayDeltaTime);
    UpdateLinkedGimmicks(gameplayDeltaTime);
    UpdateEnemies();
    UpdateShields(gameplayDeltaTime);
    UpdateBullets();
    UpdateDropItems(); // Legacy update order: drop item step
}

void GameScene::ResolveGameplayOutcomes(float gameplayDeltaTime)
{
    UpdateGoalVisual(gameplayDeltaTime);
    HandleWorldInteractions();
    RemoveDefeatedEnemies();
    UpdateEffects(gameplayDeltaTime);
}

void GameScene::FlushPendingEntities()
{
    // Flush entities queued during gameplay update.
    for (auto& entity : m_pendingEntities)
    {
        m_entities.push_back(std::move(entity));
    }
    m_pendingEntities.clear();
}

