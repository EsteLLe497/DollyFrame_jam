#pragma once

#include <vector>

#include "game_object.h"

enum class EnemyArchetype
{
    Floater,
    Walker,
    Turret,
    Ranged,
    ShieldBoss,
    MidBoss2,
    Ghost,
    BlasterRobot,
    Charger,
};

enum class ShieldBossState
{
    Idle,
    Detect,
    Rush,
    RushCooldown,
    Jump,
    JumpAscend,
    AirHover,
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

enum class MidBoss2State
{
    Idle,
    SpearJump,
    SpearThrow,
    SpearLanding,
    SpearCooldown,
    BeamCharge,
    BeamFire,
    BeamCooldown,
    Damaged,
    Dead,
};

enum class ShieldAttackType
{
    None,
    Rush,
    Base,
    Slam,
};

enum class CapturedShieldMode
{
    None,
    Normal,
    RushBurst,
    JumpBurst,
};

class EnemyComponent final : public MonoBehaviour
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
    float detectHeight = 96.0f;
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
    bool attackFrameTriggered = false;
    bool attackCaptureWindowActive = false;
    float attackWarningProgress = 0.0f;
    float attackFlashRemaining = 0.0f;
    bool knockbackActive = false;
    float knockbackTimer = 0.0f;
    float knockbackDuration = 0.26f;
    float knockbackStartX = 0.0f;
    float knockbackStartY = 0.0f;
    float knockbackTargetX = 0.0f;
    float knockbackHeight = 0.65f;

private:
    EnemyArchetype m_archetype;
    int m_contactDamage;
    bool m_enabled;
    bool m_defeated;
    AIState m_aiState = AIState::Idle;
};

class ShieldBossComponent final : public MonoBehaviour
{
public:
    ShieldBossComponent() = default;

    ShieldBossState state = ShieldBossState::Idle;
    ShieldBossFacing facing = ShieldBossFacing::Right;

    float detectRange = 12.0f * 48.0f;
    float detectHeight = 4.0f * 48.0f;
    float stateTimer = 0.0f;

    float rushSpeed = 520.0f;
    float rushDuration = 0.45f;
    float rushCooldown = 2.5f;
    int rushCount = 0;
    int rushCountMax = 2;

    float jumpHeight = 6.0f;
    float jumpAscendDuration = 0.35f;
    float airHoverDuration = 1.5f;
    float descendSpeed = 1200.0f;

    float slamPhase1Duration = 0.2f;
    float slamPhase2Duration = 0.3f;
    float slamCooldown = 1.2f;

    float targetX = 0.0f;
    float targetY = 0.0f;
    float jumpStartX = 0.0f;
    float jumpStartY = 0.0f;
    float returnJumpDuration = 0.9f;
    float returnJumpHeight = 9.0f;
    bool returningHomeJump = false;
    bool knockbackActive = false;
    float knockbackTimer = 0.0f;
    float knockbackDuration = 0.28f;
    float knockbackStartX = 0.0f;
    float knockbackStartY = 0.0f;
    float knockbackTargetX = 0.0f;
    float knockbackHeight = 0.85f;
    float hoverShieldX = 0.0f;
    float hoverShieldY = 0.0f;
    float hoverPlayerOffsetX = 0.0f;
    bool attackRectActive = false;
    float attackRectX = 0.0f;
    float attackRectY = 0.0f;
    float attackRectWidth = 0.0f;
    float attackRectHeight = 0.0f;

    bool combatStarted = false;
    bool appearAnimationActive = false;
    bool appearAnimationFinished = false;
    bool roarPlayed = false;
    bool roarAnimationActive = false;
    bool deathAnimationActive = false;
    bool deathAnimationFinished = false;
    GameObject* shieldEntity = nullptr;
};

class ShieldComponent final : public MonoBehaviour
{
public:
    ShieldComponent() = default;

    GameObject* ownerBoss = nullptr;
    bool attached = true;
    bool gravityEnabled = false;
    ShieldAttackType attackType = ShieldAttackType::None;

    float followOffsetX = 0.0f;
    float followOffsetY = 0.0f;

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    float rotationSpeed = 0.0f;

    int contactDamage = 1;
    float knockbackGrids = 3.0f;

    float elapsed = 0.0f;
    float lifetime = 0.0f;

    float baseAttackElapsed = 0.0f;
    float baseAttackDuration = 0.5f;
    bool photoSpawned = false;
    CapturedShieldMode capturedMode = CapturedShieldMode::None;
    bool followPlayer = false;
    bool grounded = false;
    bool shockwaveSpawned = false;
    bool fadeStarted = false;
    float hoverElapsed = 0.0f;
    float hoverDuration = 0.0f;
    float descendSpeed = 0.0f;
    std::vector<GameObject*> hitEntities;
};

class ShieldShockwaveComponent final : public MonoBehaviour
{
public:
    ShieldShockwaveComponent() = default;

    GameObject* ownerBoss = nullptr;
    int damage = 1;
    float knockbackGrids = 3.0f;
    float elapsed = 0.0f;
    float lifetime = 0.2f;
    bool damagesPlayer = true;
    bool hitPlayer = false;
    std::vector<GameObject*> hitEntities;
};

class MidBoss2Component final : public MonoBehaviour
{
public:
    struct Params
    {
        int boss2Hp = 15;
        int boss2WidthGrid = 4;
        int boss2HeightGrid = 4;
        int spearDamage = 1;
        float spearFadeTime = 3.0f;
        float spearInterval = 0.3f;
        float spearCooldownAfterLanding = 2.0f;
        float spearLandingPauseTime = 0.2f;
        float spearJumpHeightGrid = 6.0f;
        float spearJumpHorizontalGrid = 8.0f;
        float beamChargeTime = 3.0f;
        float beamDamagePerSecond = 1.0f;
        float beamHeightGrid = 3.0f;
        float pastedBeamDamagePerSecond = 1.0f;
    };

    MidBoss2Component() = default;

    Params params;
    MidBoss2State state = MidBoss2State::Idle;
    bool facingRight = true;
    bool beamFacingRight = true;
    float stateTimer = 0.0f;
    float cooldownRemaining = 0.0f;
    int attackFlowStep = 1;
    int spearCycleCount = 0;
    int spearShotsFired = 0;
    float lastSpearDirX = 0.0f;
    float lastSpearDirY = -1.0f;
    float hoverStartX = 0.0f;
    float hoverStartY = 0.0f;
    float hoverTargetX = 0.0f;
    float hoverTargetY = 0.0f;
    float landingTargetX = 0.0f;
    float landingTargetY = 0.0f;
    float homeX = 0.0f;
    float homeY = 0.0f;
    bool initializedHome = false;
    bool beamEntitiesSpawned = false;
    GameObject* beamTurretEntity = nullptr;
    GameObject* beamEntity = nullptr;
    bool captureWindowActive = false;
};

class MidBoss2SpearComponent final : public MonoBehaviour
{
public:
    MidBoss2SpearComponent() = default;

    bool launched = false;
    bool stuck = false;
    float fadeRemaining = 0.0f;
    float fadeDuration = 3.0f;
    float directionX = 0.0f;
    float directionY = -1.0f;
    float targetDirectionX = 0.0f;
    float targetDirectionY = -1.0f;
    float launchDelay = 0.0f;
    float launchTimer = 0.0f;
    float travelDistance = 0.0f;
};

class GhostComponent final : public MonoBehaviour
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

class BlasterRobotComponent final : public MonoBehaviour
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

class DropItemComponent final : public MonoBehaviour
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

class ProjectileComponent final : public MonoBehaviour
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
    void SetVelocityX(float value) { m_velocityX = value; }
    void SetVelocityY(float value) { m_velocityY = value; }
    int GetDamage() const { return m_damage; }
    Owner GetOwner() const { return m_owner; }
    int pierceRemaining = 0;
    int maxEnemyHits = 0;
    GameObject* sourceEntity = nullptr;

private:
    float m_velocityX;
    float m_velocityY;
    int m_damage;
    Owner m_owner;
};
