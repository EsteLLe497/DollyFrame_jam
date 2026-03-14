#pragma once

#include <string>

#include <box2d/box2d.h>

#include "component.h"

class PhysicsWorld;
class EventBus;

class TransformComponent final : public Component
{
public:
    TransformComponent(float x, float y, float width, float height);

    float x;
    float y;
    float width;
    float height;
    float rotation;
    float scale;
};

class TintComponent final : public Component
{
public:
    TintComponent(float rValue, float gValue, float bValue, float aValue);

    float r;
    float g;
    float b;
    float a;
};

class TagComponent final : public Component
{
public:
    explicit TagComponent(const char* value);

    std::string tag;
};

class HealthComponent final : public Component
{
public:
    explicit HealthComponent(int maxHealth);

    void DrawDebugUI() override;
    void ApplyDamage(int amount);
    int GetCurrentHealth() const;
    int GetMaxHealth() const;
    bool IsDead() const;

private:
    int m_maxHealth;
    int m_currentHealth;
};

class DamageCooldownComponent final : public Component
{
public:
    explicit DamageCooldownComponent(float cooldownSeconds);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;
    bool CanTakeDamage() const;
    void Trigger();
    float GetRemainingSeconds() const;

private:
    float m_cooldownSeconds;
    float m_remainingSeconds;
};

class SpriteRenderComponent final : public Component
{
public:
    explicit SpriteRenderComponent(int textureId);

    void Draw() override;
    int GetTextureId() const;

private:
    int m_textureId;
};

class PlayerControllerComponent final : public Component
{
public:
    explicit PlayerControllerComponent(EventBus& eventBus);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

private:
    EventBus* m_eventBus;
};

class EnemyMoverComponent final : public Component
{
public:
    EnemyMoverComponent(float originX, float originY, float amplitudeX, float amplitudeY, float frequency);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

private:
    float m_originX;
    float m_originY;
    float m_amplitudeX;
    float m_amplitudeY;
    float m_frequency;
    float m_time;
};

class RigidBodyComponent final : public Component
{
public:
    RigidBodyComponent(PhysicsWorld& physicsWorld, b2BodyType bodyType, bool fixedRotation, float gravityScale = 1.0f);
    ~RigidBodyComponent() override;

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;
    void PushTransformToPhysics();
    void PullTransformFromPhysics();

    b2BodyId GetBodyId() const;
    b2BodyType GetBodyType() const;
    void SetLinearVelocity(float x, float y);

private:
    PhysicsWorld* m_physicsWorld;
    b2BodyType m_bodyType;
    bool m_fixedRotation;
    float m_gravityScale;
    b2BodyId m_bodyId;
};

class BoxColliderComponent final : public Component
{
public:
    BoxColliderComponent(float density, float friction, bool isSensor = false);
    ~BoxColliderComponent() override;

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;
    b2ShapeId GetShapeId() const;

private:
    float m_density;
    float m_friction;
    bool m_isSensor;
    b2ShapeId m_shapeId;
};
