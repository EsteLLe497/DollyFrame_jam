#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "audio.h"

using namespace game_scene_detail;

namespace
{
    const char* ResolveStageBgmCueName(const std::string& mapCsvPath)
    {
        std::string stem;
        try
        {
            stem = std::filesystem::path(mapCsvPath).stem().string();
        }
        catch (...)
        {
            stem = mapCsvPath;
        }

        std::transform(
            stem.begin(),
            stem.end(),
            stem.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        if (stem.find("under") != std::string::npos)
        {
            return "bgm_under";
        }
        if (stem.find("ruins") != std::string::npos)
        {
            return "bgm_ruins";
        }
        if (stem.find("forest") != std::string::npos)
        {
            return "bgm_forest";
        }

        return "bgm_forest";
    }
}

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
        if (GameSession_ShouldLoadSavedProgress())
        {
            LoadProgressStateFromDisk();
        }
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

    if (m_save.hasData)
    {
        ApplyLoadedProgressState();
    }
    else
    {
        GameSession_Reset(3, m_flow.timeLimit);
    }
    const float initialMasterVolume = Audio_GetMasterVolume();
    m_debug.bgmRestoreVolume = initialMasterVolume > 0.001f ? initialMasterVolume : 1.0f;
    m_debug.bgmEnabled = initialMasterVolume > 0.001f;
    PlayStageBgmForCurrentMap();
    m_lifecycle.loadingProgress = 1.0f;
    m_lifecycle.loadingStep = 4;
    m_lifecycle.loadingFinished = true;
    m_lifecycle.loadingWarmupFramesRemaining = 3;
    m_lifecycle.loadingResources = nullptr;
    Logger::Info("GameScene entered as photo sandbox stage");
}

void GameScene::PlayStageBgmForCurrentMap()
{
    // Select the BGM from the active map name so stage transitions update music.
    m_lifecycle.shieldBossBgmCrossFadeStarted = false;
    Audio_PlayBgmCue(ResolveStageBgmCueName(m_lifecycle.currentMapCsvPath));
}

void GameScene::CrossFadeStageBgmForCurrentMap(float durationSeconds)
{
    // Return from boss music to the regular BGM for the current stage.
    Audio_CrossFadeBgmCue(ResolveStageBgmCueName(m_lifecycle.currentMapCsvPath), durationSeconds);
}

void GameScene::UpdateShieldBossBgmCue()
{
    constexpr float kShieldBossBgmCrossFadeSeconds = 1.6f;

    if (m_lifecycle.shieldBossBgmCrossFadeStarted)
    {
        return;
    }
    if (!IsShieldBossIntroCinematicActive())
    {
        return;
    }

    // Start the boss BGM when the black curtain appears for mid-boss 1.
    m_lifecycle.shieldBossBgmCrossFadeStarted = true;
    Audio_CrossFadeBgmCue("bgm_forest_boss", kShieldBossBgmCrossFadeSeconds);
}

void GameScene::OnExit()
{
    const ActiveGameSceneScope activeScene(*this);
    DirectXSetPostProcessVignette(0.08f, 0.72f, 0.72f, 0.70f);
    DirectXSetPostProcessPlayerLight(
        static_cast<float>(kVirtualScreenWidth) * 0.5f,
        static_cast<float>(kVirtualScreenHeight) * 0.5f,
        0.0f,
        120.0f,
        170.0f);
    Audio_StopBgm();
    m_lifecycle.loadingActive = false;
    m_lifecycle.loadingFinished = false;
    m_lifecycle.loadingWarmupFramesRemaining = 0;
    m_lifecycle.loadingResources = nullptr;
    m_scriptEngine.Shutdown();
    ShutdownGameSceneTestPhotos(m_testPhotos);
    // 立ち絵はResourceManagerが所有し、他テクスチャとまとめて解放します。
    m_tutorial.portraitTextureId = -1;
    m_tutorial.loadedPortraitPath.clear();
    releaseTutorialVideo(m_tutorial.videoPlayer);
    m_world.Clear();
    m_physicsWorld.Shutdown();
}
