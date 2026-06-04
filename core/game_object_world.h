#pragma once

#include <memory>
#include <vector>

#include "entity.h"
#include "entity_tag.h"

class GameObjectWorld
{
public:
    using EntityList = std::vector<std::unique_ptr<Entity>>;

    EntityList& Entities();
    const EntityList& Entities() const;
    EntityList& PendingEntities();
    const EntityList& PendingEntities() const;

    Entity& Spawn(std::unique_ptr<Entity> entity);
    void QueueSpawn(std::unique_ptr<Entity> entity);
    void FlushPending();
    void Clear();
    void Reserve(std::size_t entityCount, std::size_t pendingCount);

    Entity* FindByTag(const char* tag) const;
    Entity* FindByTag(EntityTag tag) const;
    void RemoveByPointerList(const std::vector<Entity*>& entitiesToRemove);

private:
    EntityList m_entities;
    EntityList m_pendingEntities;
};
