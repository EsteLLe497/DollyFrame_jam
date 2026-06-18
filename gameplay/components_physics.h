#pragma once

#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "game_object.h"

class PhysicsWorld;

class EnemyMoverComponent final : public MonoBehaviour
{
public:
    EnemyMoverComponent(float originX, float originY, float amplitudeX, float amplitudeY, float frequency);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;
    void SetOrigin(float originX, float originY);
    void SetFrozen(bool frozen);
    bool IsFrozen() const;
    void Rewind(float seconds);

private:
    float m_originX;
    float m_originY;
    float m_amplitudeX;
    float m_amplitudeY;
    float m_frequency;
    float m_time;
    bool m_frozen;
};

class RigidBodyComponent final : public MonoBehaviour
{
public:
    RigidBodyComponent(PhysicsWorld& physicsWorld, b2BodyType bodyType, bool fixedRotation, float gravityScale = 1.0f);
    ~RigidBodyComponent() override;

    void OnAttach(GameObject& owner) override;
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

class BoxColliderComponent final : public MonoBehaviour
{
public:
    BoxColliderComponent(float density, float friction, bool isSensor = false);
    ~BoxColliderComponent() override;

    void OnAttach(GameObject& owner) override;
    void DrawDebugUI() override;
    b2ShapeId GetShapeId() const;

private:
    float m_density;
    float m_friction;
    bool m_isSensor;
    b2ShapeId m_shapeId;
};

class ImageOutlineColliderComponent final : public MonoBehaviour
{
public:
    ImageOutlineColliderComponent(std::string imagePath, float friction, int alphaThreshold = 16, int vertexStride = 4);
    ImageOutlineColliderComponent(std::vector<b2Vec2> normalizedOutline, float friction);
    ~ImageOutlineColliderComponent() override;

    void OnAttach(GameObject& owner) override;
    void DrawDebugUI() override;
    b2ChainId GetChainId() const;

    const std::vector<b2Vec2>& GetNormalizedOutline() const;

private:
    std::string m_imagePath;
    float m_friction;
    int m_alphaThreshold;
    int m_vertexStride;
    b2ChainId m_chainId;
    int m_vertexCount;
    std::vector<b2Vec2> m_normalizedOutline;
};
