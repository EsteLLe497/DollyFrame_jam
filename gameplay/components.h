#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <box2d/box2d.h>

#include "component.h"

class PhysicsWorld;
class EventBus;

// ============================================================================
// Photo System Domain
// ============================================================================
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
    Ranged, 
    ShieldBoss,
    Ghost,        
    BlasterRobot, 
};

enum class ShieldBossState
{
    Idle,
    Detect,
    Rush,          
    RushCooldown, 
    Jump,          
    JumpAscend,    
    JumpDescend,   
    SlamPhase1,    
    SlamPhase2,    
    Cooldown,      
};

enum class ShieldBossFacing
{
    Right,
    Left,
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

// ============================================================================
// Physics / Shared Basics
// ============================================================================
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

class BatteryComponent final : public Component
{
public:
    BatteryComponent(
        float gravity,
        float maxFallSpeed,
        float pushSpeed,
        float fallDamageSpeed,
        int contactDamage);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    bool grounded = false;
    float gravity = 0.0f;
    float maxFallSpeed = 0.0f;
    float pushSpeed = 0.0f;
    float fallDamageSpeed = 0.0f;
    int contactDamage = 1;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
};

class BatterySwitchComponent final : public Component
{
public:
    BatterySwitchComponent(
        int linkId,
        int requiredBatteryCount,
        float pressDepth,
        float pressSpeed,
        float releaseSpeed,
        bool controlsLaserPower = false);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    int linkId = 0;
    int requiredBatteryCount = 1;
    int insertedBatteryCount = 0;
    float pressDepth = 8.0f;
    float pressSpeed = 60.0f;
    float releaseSpeed = 60.0f;
    float activationGraceSeconds = 0.20f;
    float activationGraceRemaining = 0.0f;
    float baseY = 0.0f;
    float currentPress = 0.0f;
    bool isPressed = false;
    bool controlsLaserPower = false;
};

class ElevatorComponent final : public Component
{
public:
    ElevatorComponent(
        int linkId,
        float moveRangeY,
        float moveSpeed,
        float topPauseSeconds);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    int linkId = 0;
    float moveRangeY = 144.0f;
    float moveSpeed = 140.0f;
    float topPauseSeconds = 1.0f;
    float baseY = 0.0f;
    bool cycleStarted = false;
    bool movingUp = true;
    float pauseTimer = 0.0f;
    bool wasPlayerTouching = false;
    bool wasPowered = false;
};

class LaserSwitchComponent final : public Component
{
public:
    explicit LaserSwitchComponent(int linkId);

    void DrawDebugUI() override;

    int linkId = 0;
    bool isOn = false;
};

class ShutterComponent final : public Component
{
public:
    ShutterComponent(
        int linkId,
        float moveRangeY,
        float moveSpeed,
        bool useBossDefeatSignal,
        bool opensWhenUnpowered = false);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    int linkId = 0;
    float moveRangeY = 144.0f;
    float moveSpeed = 240.0f;
    float baseY = 0.0f;
    bool isOpen = false;
    bool useBossDefeatSignal = false;
    bool opensWhenUnpowered = false;
};

class LaserTurretComponent final : public Component
{
public:
    LaserTurretComponent(
        float beamThickness,
        float damagePerSecond,
        bool vertical = false,
        bool shootsLeft = false,
        bool requiresLaserPower = false);

    void DrawDebugUI() override;

    float beamThickness = 8.0f;
    float damagePerSecond = 1.0f;
    bool vertical = false;
    bool shootsLeft = false;
    bool requiresLaserPower = false;
    float playerDamageTimer = 0.0f;
    std::unordered_map<const Entity*, float> enemyDamageTimers;
    float sparkTimer = 0.0f;
};

class LaserBeamComponent final : public Component
{
public:
    LaserBeamComponent() = default;
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

class FlickerLightComponent final : public Component
{
public:
    FlickerLightComponent(
        float radius,
        float intensity,
        float flickerAmplitude,
        float flickerSpeed,
        float offsetX,
        float offsetY,
        float r,
        float g,
        float b,
        bool godRayEnabled,
        float godRayLength,
        float godRayWidth,
        float godRayIntensity,
        float godRayDriftSpeed,
        float godRaySoftness);

    void DrawDebugUI() override;

    float radius;
    float intensity;
    float flickerAmplitude;
    float flickerSpeed;
    float offsetX;
    float offsetY;
    float r;
    float g;
    float b;
    bool godRayEnabled;
    float godRayLength;
    float godRayWidth;
    float godRayIntensity;
    float godRayDriftSpeed;
    float godRaySoftness;
};

class MarkerLightComponent final : public Component
{
public:
    MarkerLightComponent(float radius, float intensity);

    void DrawDebugUI() override;

    float radius;
    float intensity;
    bool activated = false;
};

class TagComponent final : public Component
{
public:
    explicit TagComponent(const char* value);

    std::string tag;
};

// ============================================================================
// Photo System Components
// ============================================================================
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

class PhotoPasteOrderComponent final : public Component
{
public:
    explicit PhotoPasteOrderComponent(int orderValue);

    int order;
};

class PhotoCopyLifetimeComponent final : public Component
{
public:
    explicit PhotoCopyLifetimeComponent(float lifetimeSeconds);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;
    // 残り寿命。0 になると IsExpired が true になる。
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
    // 0.0～1.0 の正規化進捗（ペースト演出用）。
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

class DamagePlatformComponent final : public Component
{
public:
    explicit DamagePlatformComponent(int tileSpanValue);

    int tileSpan;
};

class SpikeStripComponent final : public Component
{
public:
    explicit SpikeStripComponent(int tileSpanValue);

    int tileSpan;
};

class VanishOnCaptureComponent final : public Component
{
public:
    explicit VanishOnCaptureComponent(bool enabled = true);

    bool enabled;
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

// ============================================================================
// Enemy / Combat Domain
// ============================================================================
class EnemyComponent final : public Component
{
public:
    EnemyComponent(EnemyArchetype archetype, int contactDamage = 1);

   
    enum class AIState { Idle, Chase, Attack };
    enum class FacingDirection { Right, Left };
    FacingDirection facing = FacingDirection::Right;

    void DrawDebugUI() override;
    EnemyArchetype GetArchetype() const;
    int GetContactDamage() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    bool IsDefeated() const;
    void MarkDefeated();
    void Restore(); 

    
    AIState GetAIState() const { return m_aiState; }
    void SetAIState(AIState state) { m_aiState = state; }
    float attackTimer = 0.0f;
    float attackCooldown = 3.0f;
    float detectRange = 400.0f;
    float attackRange = 48.0f;
    float detectHeight = 96.0f; // 3/21追加(田之上俊)
    float velocityY = 0.0f;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    bool respawnEnabled = true;
    bool attackRectActive = false;
    float attackRectX = 0.0f;
    float attackRectY = 0.0f;
    float attackRectWidth = 0.0f;
    float attackRectHeight = 0.0f;
    float attackRectRemaining = 0.0f;

private:
    EnemyArchetype m_archetype;
    int m_contactDamage;
    bool m_enabled;
    bool m_defeated;
   
    AIState m_aiState = AIState::Idle;
};

class ShieldBossComponent final : public Component
{
public:
    ShieldBossComponent() = default;

    
    ShieldBossState state = ShieldBossState::Idle;
    ShieldBossFacing facing = ShieldBossFacing::Right;

    
    float stateTimer = 0.0f;

    
    int rushCount = 0;         
    int rushCountMax = 3;       

 
    float detectRange = 600.0f;
    float detectHeight = 192.0f;

    
    float rushSpeed = 400.0f;
    float rushDamage = 1.0f;
    float rushCooldown = 1.0f;
    float rushDuration = 2.0f;  

   
    float jumpHeight = 4.0f;    
    float targetX = 0.0f;

    
    float slamPhase1Duration = 0.2f;
    float slamPhase2Duration = 0.2f;
    float slamDamage1 = 1.0f;
    float slamDamage2 = 2.0f;
    float slamCooldown = 3.0f;

    
    float velocityY = 0.0f;
    float velocityX = 0.0f;

    
    bool attackRectActive = false;
    float attackRectX = 0.0f;
    float attackRectY = 0.0f;
    float attackRectWidth = 0.0f;
    float attackRectHeight = 0.0f;
    float attackRectDamage = 1.0f;


    std::vector<Entity*> hitEntities;
};

class GhostComponent final : public Component
{
public:
    GhostComponent() = default;

    float detectRange = 6.0f * 48.0f;
    float moveSpeed = 80.0f;
    float visibilityAlpha = 0.3f;
    float targetAlpha = 0.3f;
    int dropMin = 10;
    int dropMax = 30;
};

class BlasterRobotComponent final : public Component
{
public:
    BlasterRobotComponent() = default;

    float detectRange = 5.0f * 48.0f;
    int burstCount = 3;
    float burstInterval = 0.15f;
    float cooldown = 3.0f;
    int shotsRemaining = 0;
    float burstTimer = 0.0f;
    float cooldownTimer = 0.0f;
    bool mountedOnCeiling = false;
    bool facingRight = true;
};

// 3/21追加：ドロップアイテムコンポーネント(田之上俊)
class DropItemComponent final : public Component
{
public:
    DropItemComponent(int value, float velocityX, float velocityY)
        : m_value(value)
        , m_velocityX(velocityX)
        , m_velocityY(velocityY)
        , m_attracting(false)
        , m_attractTimer(0.0f)
    {
    }

    int GetValue() const { return m_value; }
    float GetVelocityX() const { return m_velocityX; }
    float GetVelocityY() const { return m_velocityY; }
    void SetVelocityX(float v) { m_velocityX = v; }
    void SetVelocityY(float v) { m_velocityY = v; }
    bool IsAttracting() const { return m_attracting; }
    void SetAttracting(bool v) { m_attracting = v; }
    float GetAttractTimer() const { return m_attractTimer; }
    void SetAttractTimer(float v) { m_attractTimer = v; }

private:
    int m_value;
    float m_velocityX;
    float m_velocityY;
    bool m_attracting;
    float m_attractTimer;
};


class ProjectileComponent final : public Component
{
public:
    enum class Owner
    {
        Enemy,
        Photo,
        BlasterRobot,
    };

    ProjectileComponent(float velocityX, float velocityY, int damage = 1, Owner owner = Owner::Enemy)
        : m_velocityX(velocityX)
        , m_velocityY(velocityY)
        , m_damage(damage)
        , m_owner(owner)
    {
    }

    float GetVelocityX() const { return m_velocityX; }
    float GetVelocityY() const { return m_velocityY; }
    int GetDamage() const { return m_damage; }
    Owner GetOwner() const { return m_owner; }
    int pierceRemaining = 0;    
    int maxEnemyHits = 0;    
    Entity* sourceEntity = nullptr; 

private:
    float m_velocityX;
    float m_velocityY;
    int m_damage;
    Owner m_owner;
};

// ============================================================================
// Stage Gimmick Domain
// ============================================================================
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

// ============================================================================
// Gameplay Common Domain
// ============================================================================
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
    float GetCooldownSeconds() const;
    float GetRemainingSeconds() const;

private:
    float m_cooldownSeconds;
    float m_remainingSeconds;
};

// ============================================================================
// Rendering / Animation Domain
// ============================================================================
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

class SpriteSheetAnimationComponent final : public Component
{
public:
    struct Clip
    {
        int textureId = -1;
        int columns = 1;
        int rows = 1;
        int startFrame = 0;
        int frameCount = 1;
        float fps = 1.0f;
        bool loop = true;
    };

    SpriteSheetAnimationComponent();

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

    void DefineClip(
        const std::string& name,
        int textureId,
        int columns,
        int rows,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    bool HasClip(const std::string& name) const;
    // 再生中クリップと同名の場合、restartIfSame=true のときだけ先頭フレームへ戻す。
    bool Play(const std::string& name, bool restartIfSame = false);
    const std::string& GetCurrentClipName() const;
    int GetCurrentFrameIndex() const;
    void SetPlaybackSpeed(float speed);

private:
    void ApplyFrameToSprite();

    std::unordered_map<std::string, Clip> m_clips;
    std::string m_currentClipName;
    float m_elapsedSeconds;
    float m_playbackSpeed;
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

// ============================================================================
// Movement / Physics Domain
// ============================================================================
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
    // Transform -> Box2D 反映（主に static / kinematic 用）。
    void PushTransformToPhysics();
    // Box2D -> Transform 反映（主に dynamic の結果取り込み）。
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

class ImageOutlineColliderComponent final : public Component
{
public:
    ImageOutlineColliderComponent(std::string imagePath, float friction, int alphaThreshold = 16, int vertexStride = 4);
    ImageOutlineColliderComponent(std::vector<b2Vec2> normalizedOutline, float friction);
    ~ImageOutlineColliderComponent() override;

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;
    b2ChainId GetChainId() const;
    // [0,1] 正規化頂点。実体生成時に Transform サイズへスケールして使う。
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
