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
    TryUseAttackCaptureSlot();
    UpdateBarrels(gameplayDeltaTime);
    UpdateFallingRocks(gameplayDeltaTime);
    UpdateJumpPads(gameplayDeltaTime);
    UpdateBatteries(gameplayDeltaTime);
    UpdateLaserTurrets(gameplayDeltaTime);
    UpdateLinkedGimmicks(gameplayDeltaTime);
    UpdateMerchants(gameplayDeltaTime);
    UpdateEnemies();
    UpdateShields(gameplayDeltaTime);
    ApplyShieldBossSlamCameraWork(gameplayDeltaTime);
    ApplyShieldBossFramingCameraWork(gameplayDeltaTime);
    // 配置座標は、このフレームのボスカメラとズームが確定してから計算する。
    HandlePhotoSpawn();
    UpdateBullets();
    UpdateDropItems(); // Legacy update order: drop item step
    UpdateSepiaRestoredLifetimes(gameplayDeltaTime);
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
    m_world.FlushPending();
}
