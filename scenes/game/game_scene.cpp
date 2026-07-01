#include "pch.h"

#include "game_scene_internal.h"

using namespace game_scene_detail;

GameScene::GameScene()
    : m_whiteTexture(-1)
    , m_tileTexture(-1)
    , m_tileTexture2(-1)
    , m_tileTexture3(-1)
    , m_photo()
{
}

GameSceneTuningState& GameScene::Tuning()
{
    return m_tuning;
}

const GameSceneTuningState& GameScene::Tuning() const
{
    return m_tuning;
}

