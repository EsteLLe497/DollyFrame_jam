#pragma once

#include <box2d/box2d.h>

#include "event_bus.h"

class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    bool Initialize(float gravityX, float gravityY, EventBus& eventBus);
    void Shutdown();
    void Step(float deltaTime);

    b2WorldId GetWorldId() const;
    EventBus& GetEventBus();
    const EventBus& GetEventBus() const;

    void SyncEntityToPhysics(Entity& entity);
    void SyncEntityFromPhysics(Entity& entity);

private:
    void GatherContactEvents();

    b2WorldId m_worldId;
    EventBus* m_eventBus;
    bool m_initialized;
    // Accumulates real elapsed time so the simulation can advance in
    // fixed-size steps regardless of the caller's frame rate.
    float m_stepAccumulator;
};
