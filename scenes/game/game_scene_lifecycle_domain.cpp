#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "audio.h"

using namespace game_scene_detail;

void GameScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    const ActiveGameSceneScope activeScene(*this);

    ResetSceneState();
    m_lifecycle.loadingResources = &resources;
    m_lifecycle.loadingActive = true;
    m_lifecycle.loadingFinished = false;
    m_lifecycle.loadingWarmupFramesRemaining = 0;
    m_lifecycle.loadingStep = 0;
    m_lifecycle.loadingElapsed = 0.0f;
    m_lifecycle.loadingProgress = 0.0f;
    Logger::Info("GameScene loading started");
}

void GameScene::UpdateLoading(float deltaTime)
{
    m_lifecycle.loadingElapsed += std::max(0.0f, deltaTime);
    if (m_lifecycle.loadingFinished)
    {
        return;
    }
    AdvanceLoadingStep();
}

void GameScene::AdvanceLoadingStep()
{
    if (!m_lifecycle.loadingActive)
    {
        return;
    }

    switch (m_lifecycle.loadingStep)
    {
    case 0:
        LoadTuningState();
        m_lifecycle.loadingProgress = 0.15f;
        ++m_lifecycle.loadingStep;
        break;
    case 1:
        if (m_lifecycle.loadingResources)
        {
            InitializeStageResources(*m_lifecycle.loadingResources);
        }
        m_lifecycle.loadingProgress = 0.45f;
        ++m_lifecycle.loadingStep;
        break;
    case 2:
        InitializeStageEntities();
        m_lifecycle.loadingProgress = 0.78f;
        ++m_lifecycle.loadingStep;
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
    m_lifecycle.loadingProgress = 1.0f;
    m_lifecycle.loadingStep = 4;
    m_lifecycle.loadingFinished = true;
    m_lifecycle.loadingWarmupFramesRemaining = 3;
    m_lifecycle.loadingResources = nullptr;
    Logger::Info("GameScene entered as photo sandbox stage");
}

void GameScene::OnExit()
{
    const ActiveGameSceneScope activeScene(*this);
    m_lifecycle.loadingActive = false;
    m_lifecycle.loadingFinished = false;
    m_lifecycle.loadingWarmupFramesRemaining = 0;
    m_lifecycle.loadingResources = nullptr;
    m_scriptEngine.Shutdown();
    m_world.Clear();
    m_physicsWorld.Shutdown();
}

