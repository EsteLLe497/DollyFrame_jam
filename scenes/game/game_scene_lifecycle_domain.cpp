#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "audio.h"

using namespace game_scene_detail;

void GameScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;

    ResetSceneState();
    m_loadingResources = &resources;
    m_loadingActive = true;
    m_loadingFinished = false;
    m_loadingWarmupFramesRemaining = 0;
    m_loadingStep = 0;
    m_loadingElapsed = 0.0f;
    m_loadingProgress = 0.0f;
    Logger::Info("GameScene loading started");
}

void GameScene::UpdateLoading(float deltaTime)
{
    m_loadingElapsed += std::max(0.0f, deltaTime);
    if (m_loadingFinished)
    {
        return;
    }
    AdvanceLoadingStep();
}

void GameScene::AdvanceLoadingStep()
{
    if (!m_loadingActive)
    {
        return;
    }

    switch (m_loadingStep)
    {
    case 0:
        LoadTuningState();
        m_loadingProgress = 0.15f;
        ++m_loadingStep;
        break;
    case 1:
        if (m_loadingResources)
        {
            InitializeStageResources(*m_loadingResources);
        }
        m_loadingProgress = 0.45f;
        ++m_loadingStep;
        break;
    case 2:
        InitializeStageEntities();
        m_loadingProgress = 0.78f;
        ++m_loadingStep;
        break;
    case 3:
        FinishLoading();
        break;
    default:
        FinishLoading();
        break;
    }
}

void GameScene::FinishLoading()
{
    if (Entity* player = FindEntityByTag(kTagPlayer))
    {
        game_scene_player_visual_system::ConfigurePlayerSpriteAnimation(
            *player,
            m_assets.GetTexture("player_idle"),
            m_assets.GetTexture("player_move"),
            m_assets.GetTexture("player_jump"),
            m_assets.GetTexture("player_capture"),
            m_assets.GetTexture("player_paste"),
            m_assets.GetTexture("player_attack"));
        game_scene_player_visual_system::ResetSpriteAnimationToIdle(m_player, *player);
    }

    GameSession_Reset(3, m_flow.timeLimit);
    const float initialMasterVolume = Audio_GetMasterVolume();
    m_debug.bgmRestoreVolume = initialMasterVolume > 0.001f ? initialMasterVolume : 0.6f;
    m_debug.bgmEnabled = initialMasterVolume > 0.001f;
    Audio_LoadCueFromFile("demo_bgm", "assets/effects/Sound/demo.wav");
    Audio_PlayCue("demo_bgm");
    m_loadingProgress = 1.0f;
    m_loadingStep = 4;
    m_loadingFinished = true;
    m_loadingWarmupFramesRemaining = 3;
    m_loadingResources = nullptr;
    Logger::Info("GameScene entered as photo sandbox stage");
}

void GameScene::OnExit()
{
    m_loadingActive = false;
    m_loadingFinished = false;
    m_loadingWarmupFramesRemaining = 0;
    m_loadingResources = nullptr;
    m_scriptEngine.Shutdown();
    m_entities.clear();
    m_physicsWorld.Shutdown();
}

