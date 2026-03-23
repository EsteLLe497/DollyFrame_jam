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
    Ranged, // 3/19�ǉ�(�c�V��r)
};

enum class GimmickType
{
    Hazard,
    Goal,
    Checkpoint,
    Pickup,
    PhotoSource,
    Filter,
    Gate,
    Switch,
};

class BarrelComponent final : public Component
{
public:
    BarrelComponent(
        float gravity,
        float maxFallSpeed,
        float rollSpeed,
        float groundFriction,
        int contactDamage,
        float breakMinFallDistance,
        float breakMinImpactSpeed);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    bool grounded = false;
    bool destroyed = false;
    float accumulatedFallDistance = 0.0f;
    float gravity = 0.0f;
    float maxFallSpeed = 0.0f;
    float rollSpeed = 0.0f;
    float groundFriction = 0.0f;
    int contactDamage = 1;
    float breakMinFallDistance = 0.0f;
    float breakMinImpactSpeed = 0.0f;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    bool active = false;
    bool cooldownActive = false;
    float cooldownRemaining = 0.0f;
    bool respawnEnabled = true;
    bool respawnWhenOffscreen = false;
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

class PhotoPasteAnimationComponent final : public Component
{
public:
    explicit PhotoPasteAnimationComponent(float durationSeconds);

    void Update(float deltaTime) override;
    float GetNormalizedProgress() const;
    bool IsFinished() const;

private:
    float m_durationSeconds;
    float m_elapsedSeconds;
};

class PhotoCopyOriginComponent final : public Component
{
public:
    explicit PhotoCopyOriginComponent(PhotoCopyOrigin originValue);

    PhotoCopyOrigin origin;
};

class PhotoCopyTileValueComponent final : public Component
{
public:
    explicit PhotoCopyTileValueComponent(int tileValue);

    int tileValue;
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

    // 3/19�ǉ��F�ȒP��AI��ԊǗ��̂��߂̗񋓌^(�c�V��r)
    enum class AIState { Idle, Chase, Attack };

    void DrawDebugUI() override;
    EnemyArchetype GetArchetype() const;
    int GetContactDamage() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    bool IsDefeated() const;
    void MarkDefeated();
    void Restore(); 

    // 3/19�ǉ��F�ȒP��AI��ԊǗ��̂��߂̃v���p�e�B�ƃ^�C�}�[(�c�V��r)
    AIState GetAIState() const { return m_aiState; }
    void SetAIState(AIState state) { m_aiState = state; }
    float attackTimer = 0.0f;
    float attackCooldown = 3.0f;
    float detectRange = 400.0f;
    float attackRange = 48.0f;
    float detectHeight = 96.0f; // 3/21追加(田之上俊)
    float velocityY = 0.0f;

private:
    EnemyArchetype m_archetype;
    int m_contactDamage;
    bool m_enabled;
    bool m_defeated;
    // 3/19�ǉ�(�c�V��r)
    AIState m_aiState = AIState::Idle;
};

// 3/19�ǉ��F�������U���̒e�R���|�[�l���g(�c�V��r)
class ProjectileComponent final : public Component
{
public:
    ProjectileComponent(float velocityX, float velocityY, int damage = 1)
        : m_velocityX(velocityX)
        , m_velocityY(velocityY)
        , m_damage(damage)
    {
    }

    float GetVelocityX() const { return m_velocityX; }
    float GetVelocityY() const { return m_velocityY; }
    int GetDamage() const { return m_damage; }

private:
    float m_velocityX;
    float m_velocityY;
    int m_damage;
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

class CheckpointComponent final : public Component
{
public:
    CheckpointComponent(int checkpointId, float respawnX, float respawnY);

    void DrawDebugUI() override;

    int checkpointId;
    float respawnX;
    float respawnY;
    bool activated;
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
    void SetCurrentHealth(int value);
    void RestoreToFull();
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
    void SetRemainingSeconds(float seconds);
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
    void SetRenderOffset(float x, float y);
    void SetRenderScale(float x, float y);
    void SetRenderRotationOffset(float radians);
    float GetRenderOffsetX() const;
    float GetRenderOffsetY() const;
    float GetRenderScaleX() const;
    float GetRenderScaleY() const;
    float GetRenderRotationOffset() const;

private:
    int m_textureId;
    float m_sourceX;
    float m_sourceY;
    float m_sourceWidth;
    float m_sourceHeight;
    bool m_flipX;
    float m_renderOffsetX;
    float m_renderOffsetY;
    float m_renderScaleX;
    float m_renderScaleY;
    float m_renderRotationOffset;
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
