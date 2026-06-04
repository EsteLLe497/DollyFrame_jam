#include "pch.h"

#include "game_object_world.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

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

const GameObjectWorld::EntityPointerList& GameObjectWorld::EntitiesByTag(EntityTag tag) const
{
    const size_t index = static_cast<size_t>(tag);
    if (index >= m_entitiesByTag.size())
    {
        static const EntityPointerList kEmpty;
        return kEmpty;
    }

    return m_entitiesByTag[index];
}

Entity& GameObjectWorld::Spawn(std::unique_ptr<Entity> entity)
{
    assert(entity);
    Entity& spawned = *entity;
    m_entities.push_back(std::move(entity));
    AddToTagIndex(&spawned);
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
    if (m_pendingEntities.empty())
    {
        return;
    }

    m_entities.reserve(m_entities.size() + m_pendingEntities.size());
    for (auto& entity : m_pendingEntities)
    {
        Entity* spawned = entity.get();
        m_entities.push_back(std::move(entity));
        AddToTagIndex(spawned);
    }
    m_pendingEntities.clear();
}

void GameObjectWorld::Clear()
{
    m_pendingEntities.clear();
    m_entities.clear();
    for (auto& list : m_entitiesByTag)
    {
        list.clear();
    }
}

void GameObjectWorld::Reserve(std::size_t entityCount, std::size_t pendingCount)
{
    m_entities.reserve(entityCount);
    m_pendingEntities.reserve(pendingCount);
}

Entity* GameObjectWorld::FindByTag(const char* tag) const
{
    const EntityTag entityTag = EntityTagFromString(tag);
    if (entityTag != EntityTag::Unknown)
    {
        return FindByTag(entityTag);
    }

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
    const EntityPointerList& entities = EntitiesByTag(tag);
    if (!entities.empty())
    {
        for (Entity* entity : entities)
        {
            if (entity)
            {
                return entity;
            }
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

    std::unordered_set<Entity*> removalSet;
    removalSet.reserve(entitiesToRemove.size());
    for (Entity* entity : entitiesToRemove)
    {
        if (entity)
        {
            removalSet.insert(entity);
        }
    }

    EraseIf(
        [&](const std::unique_ptr<Entity>& entity)
        {
            return entity && removalSet.find(entity.get()) != removalSet.end();
        });
}

void GameObjectWorld::AddToTagIndex(Entity* entity)
{
    if (!entity)
    {
        return;
    }

    const auto* tag = entity->GetComponent<TagComponent>();
    if (!tag)
    {
        return;
    }

    const size_t index = static_cast<size_t>(tag->tagId);
    if (index >= m_entitiesByTag.size() || tag->tagId == EntityTag::Unknown)
    {
        return;
    }

    m_entitiesByTag[index].push_back(entity);
}

void GameObjectWorld::RebuildTagIndex()
{
    for (auto& list : m_entitiesByTag)
    {
        list.clear();
    }

    for (const auto& entity : m_entities)
    {
        AddToTagIndex(entity.get());
    }
}
