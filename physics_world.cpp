#include "physics_world.h"

#include "components.h"
#include "entity.h"

PhysicsWorld::PhysicsWorld()
    : m_worldId(b2_nullWorldId)
    , m_eventBus(nullptr)
    , m_initialized(false)
{
}

PhysicsWorld::~PhysicsWorld()
{
    Shutdown();
}

bool PhysicsWorld::Initialize(float gravityX, float gravityY, EventBus& eventBus)
{
    if (m_initialized)
    {
        return true;
    }

    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = { gravityX, gravityY };
    m_worldId = b2CreateWorld(&worldDef);
    m_eventBus = &eventBus;
    m_initialized = B2_IS_NON_NULL(m_worldId);
    return m_initialized;
}

void PhysicsWorld::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    b2DestroyWorld(m_worldId);
    m_worldId = b2_nullWorldId;
    m_eventBus = nullptr;
    m_initialized = false;
}

void PhysicsWorld::Step(float deltaTime)
{
    if (!m_initialized)
    {
        return;
    }

    b2World_Step(m_worldId, deltaTime, 4);
    GatherContactEvents();
}

b2WorldId PhysicsWorld::GetWorldId() const
{
    return m_worldId;
}

EventBus& PhysicsWorld::GetEventBus()
{
    return *m_eventBus;
}

const EventBus& PhysicsWorld::GetEventBus() const
{
    return *m_eventBus;
}

void PhysicsWorld::SyncEntityToPhysics(Entity& entity)
{
    if (auto* rigidBody = entity.GetComponent<RigidBodyComponent>())
    {
        rigidBody->PushTransformToPhysics();
    }
}

void PhysicsWorld::SyncEntityFromPhysics(Entity& entity)
{
    if (auto* rigidBody = entity.GetComponent<RigidBodyComponent>())
    {
        rigidBody->PullTransformFromPhysics();
    }
}

void PhysicsWorld::GatherContactEvents()
{
    if (!m_eventBus)
    {
        return;
    }

    const b2ContactEvents events = b2World_GetContactEvents(m_worldId);

    for (int i = 0; i < events.beginCount; ++i)
    {
        const auto& beginEvent = events.beginEvents[i];
        auto* entityA = static_cast<Entity*>(b2Shape_GetUserData(beginEvent.shapeIdA));
        auto* entityB = static_cast<Entity*>(b2Shape_GetUserData(beginEvent.shapeIdB));
        if (entityA && entityB)
        {
            m_eventBus->Publish({ EventType::ContactBegin, entityA, entityB, {}, 0.0f, 0.0f });
        }
    }

    for (int i = 0; i < events.endCount; ++i)
    {
        const auto& endEvent = events.endEvents[i];
        Entity* entityA = nullptr;
        Entity* entityB = nullptr;
        if (b2Shape_IsValid(endEvent.shapeIdA))
        {
            entityA = static_cast<Entity*>(b2Shape_GetUserData(endEvent.shapeIdA));
        }
        if (b2Shape_IsValid(endEvent.shapeIdB))
        {
            entityB = static_cast<Entity*>(b2Shape_GetUserData(endEvent.shapeIdB));
        }
        if (entityA && entityB)
        {
            m_eventBus->Publish({ EventType::ContactEnd, entityA, entityB, {}, 0.0f, 0.0f });
        }
    }
}
