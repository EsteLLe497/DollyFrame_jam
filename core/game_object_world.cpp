#include "pch.h"

#include "game_object_world.h"

#include <algorithm>
#include <cassert>

#include "components.h"

GameObjectWorld::EntityList& GameObjectWorld::Entities()
{
    return m_entities;
}

const GameObjectWorld::EntityList& GameObjectWorld::Entities() const
{
    return m_entities;
}

GameObjectWorld::EntityList& GameObjectWorld::PendingEntities()
{
    return m_pendingEntities;
}

const GameObjectWorld::EntityList& GameObjectWorld::PendingEntities() const
{
    return m_pendingEntities;
}

Entity& GameObjectWorld::Spawn(std::unique_ptr<Entity> entity)
{
    assert(entity);
    Entity& spawned = *entity;
    m_entities.push_back(std::move(entity));
    return spawned;
}

void GameObjectWorld::QueueSpawn(std::unique_ptr<Entity> entity)
{
    if (!entity)
    {
        return;
    }

    m_pendingEntities.push_back(std::move(entity));
}

void GameObjectWorld::FlushPending()
{
    for (auto& entity : m_pendingEntities)
    {
        m_entities.push_back(std::move(entity));
    }
    m_pendingEntities.clear();
}

void GameObjectWorld::Clear()
{
    m_pendingEntities.clear();
    m_entities.clear();
}

void GameObjectWorld::Reserve(std::size_t entityCount, std::size_t pendingCount)
{
    m_entities.reserve(entityCount);
    m_pendingEntities.reserve(pendingCount);
}

Entity* GameObjectWorld::FindByTag(const char* tag) const
{
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* entityTag = entity->GetComponent<TagComponent>();
        if (entityTag && entityTag->Is(tag))
        {
            return entity.get();
        }
    }
    return nullptr;
}

Entity* GameObjectWorld::FindByTag(EntityTag tag) const
{
    for (const auto& entity : m_entities)
    {
        if (!entity)
        {
            continue;
        }

        const auto* entityTag = entity->GetComponent<TagComponent>();
        if (entityTag && entityTag->Is(tag))
        {
            return entity.get();
        }
    }
    return nullptr;
}

void GameObjectWorld::RemoveByPointerList(const std::vector<Entity*>& entitiesToRemove)
{
    if (entitiesToRemove.empty())
    {
        return;
    }

    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [&](const std::unique_ptr<Entity>& entity)
            {
                return entity &&
                    std::find(entitiesToRemove.begin(), entitiesToRemove.end(), entity.get()) != entitiesToRemove.end();
            }),
        m_entities.end());
}
