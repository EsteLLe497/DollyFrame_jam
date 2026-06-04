#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "entity.h"
#include "entity_tag.h"

class GameObjectWorld
{
public:
    using EntityList = std::vector<std::unique_ptr<Entity>>;
    using EntityPointerList = std::vector<Entity*>;

    static constexpr size_t kIndexedTagCount = static_cast<size_t>(EntityTag::BossShockwave) + 1;

    EntityList& Entities();
    const EntityList& Entities() const;
    EntityList& PendingEntities();
    const EntityList& PendingEntities() const;
    const EntityPointerList& EntitiesByTag(EntityTag tag) const;

    Entity& Spawn(std::unique_ptr<Entity> entity);
    void QueueSpawn(std::unique_ptr<Entity> entity);
    void FlushPending();
    void Clear();
    void Reserve(std::size_t entityCount, std::size_t pendingCount);
    template <typename Predicate>
    void EraseIf(Predicate&& predicate);

    Entity* FindByTag(const char* tag) const;
    Entity* FindByTag(EntityTag tag) const;
    void RemoveByPointerList(const std::vector<Entity*>& entitiesToRemove);

private:
    void AddToTagIndex(Entity* entity);
    void RebuildTagIndex();

    EntityList m_entities;
    EntityList m_pendingEntities;
    std::array<EntityPointerList, kIndexedTagCount> m_entitiesByTag{};
};

template <typename Predicate>
void GameObjectWorld::EraseIf(Predicate&& predicate)
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            std::forward<Predicate>(predicate)),
        m_entities.end());
    RebuildTagIndex();
}
