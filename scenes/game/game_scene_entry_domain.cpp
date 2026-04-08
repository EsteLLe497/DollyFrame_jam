#include "game_scene_internal.h"

using namespace game_scene_detail;

const char* GameScene::GetSceneId() const
{
    return "game";
}

void GameScene::Update(float deltaTime)
{
    ZoneScoped;

    BeginFrameUpdate(deltaTime);
    if (TryHandleModalUpdates(deltaTime))
    {
        return;
    }

    const float effectiveGameplayDeltaTime = PrepareGameplayDeltaTime(deltaTime);
    TickEntities(effectiveGameplayDeltaTime);
    FinalizeGameplayFrame(effectiveGameplayDeltaTime);
}

void GameScene::Draw()
{
    PrepareFrameRendering();
    DrawWorldAndUiLayers();
    ResetFrameRendering();
}

EventBus* GameScene::GetEventBus()
{
    return &m_eventBus;
}

