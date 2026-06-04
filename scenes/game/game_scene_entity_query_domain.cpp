#include "pch.h"

#include "game_scene_internal.h"

using namespace game_scene_detail;

Entity* GameScene::FindEntityByTag(const char* tag) const
{
    return m_world.FindByTag(tag);
}

Entity* GameScene::FindEntityByTag(EntityTag tag) const
{
    return m_world.FindByTag(tag);
}

const std::vector<Entity*>& GameScene::EntitiesByTag(EntityTag tag) const
{
    return m_world.EntitiesByTag(tag);
}
