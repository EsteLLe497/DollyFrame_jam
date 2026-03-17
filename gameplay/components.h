#pragma once

#include <string>

#include <box2d/box2d.h>

#include "component.h"

class PhysicsWorld;
class EventBus;

enum class PhotoCopyRole
{
    Solid,
    Hazard,
    GoalRelay,
    Pickup,
    Ally,
};

enum class PhotoCopyLayer
{
    Foreground,
    Background,
    Shadow,
};

enum class PhotoCopyOrigin
{
    Generic,
    Enemy,
    Hazard,
    Goal,
    Pickup,
    Tile,
};

enum class PhotoFilterTheme
{
    None,
    Hot,
    Cold,
    Invert,
    Sepia,
};

enum class EnemyArchetype
{
    Floater,
    Walker,
    Turret,
};

enum class GimmickType
{
    Hazard,
    Goal,
    Pickup,
    PhotoSource,
    Filter,
    Gate,
    Switch,
};

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

class PhotoCopyRoleComponent final : public Component
{
public:
    explicit PhotoCopyRoleComponent(PhotoCopyRole roleValue);

    PhotoCopyRole role;
};

class PhotoCopyLayerComponent final : public Component
{
public:
    explicit PhotoCopyLayerComponent(PhotoCopyLayer layerValue);

    PhotoCopyLayer layer;
};

class PhotoCopyGroupComponent final : public Component
{
public:
    explicit PhotoCopyGroupComponent(int groupIdValue);

    int groupId;
};

class PhotoCopyLifetimeComponent final : public Component
{
public:
    explicit PhotoCopyLifetimeComponent(float lifetimeSeconds);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;
    float GetRemainingSeconds() const;
    float GetLifetimeSeconds() const;
    bool IsExpired() const;

private:
    float m_lifetimeSeconds;
    float m_remainingSeconds;
};

class PhotoCopyOriginComponent final : public Component
{
public:
    explicit PhotoCopyOriginComponent(PhotoCopyOrigin originValue);

    PhotoCopyOrigin origin;
};

class PhotoCopyEffectComponent final : public Component
{
public:
    explicit PhotoCopyEffectComponent(PhotoFilterTheme themeValue = PhotoFilterTheme::None);

    void DrawDebugUI() override;
    PhotoFilterTheme GetTheme() const;
    void SetTheme(PhotoFilterTheme themeValue);

private:
    PhotoFilterTheme m_theme;
};

class EnemyComponent final : public Component
{
public:
    EnemyComponent(EnemyArchetype archetype, int contactDamage = 1);

    void DrawDebugUI() override;
    EnemyArchetype GetArchetype() const;
    int GetContactDamage() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    bool IsDefeated() const;
    void MarkDefeated();
    void Restore();

private:
    EnemyArchetype m_archetype;
    int m_contactDamage;
    bool m_enabled;
    bool m_defeated;
};

class GimmickComponent final : public Component
{
public:
    GimmickComponent(GimmickType type, bool startsEnabled = true, bool oneShot = false);

    void DrawDebugUI() override;
    GimmickType GetType() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    bool IsOneShot() const;
    bool IsConsumed() const;
    void Consume();
    void Restore();

private:
    GimmickType m_type;
    bool m_enabled;
    bool m_oneShot;
    bool m_consumed;
};

class PhotoFilterComponent final : public Component
{
public:
    PhotoFilterComponent(PhotoFilterTheme theme, PhotoCopyRole outputRole, PhotoCopyLayer outputLayer, float tintR, float tintG, float tintB, float tintA);

    void DrawDebugUI() override;
    PhotoFilterTheme GetTheme() const;
    PhotoCopyRole GetOutputRole() const;
    PhotoCopyLayer GetOutputLayer() const;
    float GetTintR() const;
    float GetTintG() const;
    float GetTintB() const;
    float GetTintA() const;

private:
    PhotoFilterTheme m_theme;
    PhotoCopyRole m_outputRole;
    PhotoCopyLayer m_outputLayer;
    float m_tintR;
    float m_tintG;
    float m_tintB;
    float m_tintA;
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
    void SetTextureId(int textureId);
    void SetSourceRect(float tx, float ty, float tw, float th);
    float GetSourceX() const;
    float GetSourceY() const;
    float GetSourceWidth() const;
    float GetSourceHeight() const;
    void SetFlipX(bool value);
    bool GetFlipX() const;

private:
    int m_textureId;
    float m_sourceX;
    float m_sourceY;
    float m_sourceWidth;
    float m_sourceHeight;
    bool m_flipX;
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
