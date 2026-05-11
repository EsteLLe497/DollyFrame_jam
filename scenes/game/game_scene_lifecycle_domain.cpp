#include "game_scene_internal.h"
#include "game_scene_player_visual_system.h"
#include "audio.h"

using namespace game_scene_detail;

void GameScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;

    ResetSceneState();
    LoadTuningState();
    InitializeStageResources(resources);
    InitializeStageEntities();
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
    Logger::Info("GameScene entered as photo sandbox stage");
}

void GameScene::OnExit()
{
    m_scriptEngine.Shutdown();
    m_entities.clear();
    m_physicsWorld.Shutdown();
}

