#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "entity_tag.h"
#include "game_object.h"

class PhysicsWorld;
class EventBus;

class BarrelComponent final : public MonoBehaviour
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

    void OnAttach(GameObject& owner) override;
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

class FallingRockComponent final : public MonoBehaviour
{
public:
    FallingRockComponent(
        float gravity,
        float maxFallSpeed,
        float rollSpeed,
        float groundFriction,
        int contactDamage,
        float breakMinFallDistance,
        float breakMinImpactSpeed);

    void OnAttach(GameObject& owner) override;
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
    bool rubbleActive = false;
    float rubbleRemaining = 0.0f;
    bool pendingJumpPadBreak = false;
};

class JumpPadComponent final : public MonoBehaviour
{
public:
    JumpPadComponent(
        float maxTiltRadians,
        float tiltSpeed,
        float returnSpeed,
        float baseLaunchVelocity,
        float fallDistanceLaunchScale,
        float maxLaunchVelocity);

    void DrawDebugUI() override;

    float tilt = 0.0f;
    float targetTilt = 0.0f;
    float leftLoad = 0.0f;
    float rightLoad = 0.0f;
    float lastRockFallDistance = 0.0f;
    float edgeRockContactGrace = 0.0f;
    float edgeRockFallDistance = 0.0f;
    int edgeRockSide = 0;
    bool launchConsumed = false;
    bool boardGrounded = false;
    float maxTiltRadians = 0.0f;
    float tiltSpeed = 0.0f;
    float returnSpeed = 0.0f;
    float baseLaunchVelocity = 0.0f;
    float fallDistanceLaunchScale = 0.0f;
    float maxLaunchVelocity = 0.0f;
};

class BatteryComponent final : public MonoBehaviour
{
public:
    BatteryComponent(
        float gravity,
        float maxFallSpeed,
        float pushSpeed,
        float fallDamageSpeed,
        int contactDamage);

    void OnAttach(GameObject& owner) override;
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

class BatterySwitchComponent final : public MonoBehaviour
{
public:
    BatterySwitchComponent(
        int linkId,
        int requiredBatteryCount,
        float pressDepth,
        float pressSpeed,
        float releaseSpeed,
        bool controlsLaserPower = false);

    void OnAttach(GameObject& owner) override;
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

class ElevatorComponent final : public MonoBehaviour
{
public:
    ElevatorComponent(
        int linkId,
        float moveRangeY,
        float moveSpeed,
        float topPauseSeconds);

    void OnAttach(GameObject& owner) override;
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

class LaserSwitchComponent final : public MonoBehaviour
{
public:
    explicit LaserSwitchComponent(int linkId);

    void DrawDebugUI() override;

    int linkId = 0;
    bool isOn = false;
};

class ShutterComponent final : public MonoBehaviour
{
public:
    ShutterComponent(
        int linkId,
        float moveRangeY,
        float moveSpeed,
        bool useBossDefeatSignal,
        bool opensWhenUnpowered = false);

    void OnAttach(GameObject& owner) override;
    void DrawDebugUI() override;

    int linkId = 0;
    float moveRangeY = 144.0f;
    float moveSpeed = 240.0f;
    float baseY = 0.0f;
    bool isOpen = false;
    bool useBossDefeatSignal = false;
    bool opensWhenUnpowered = false;
};

class ProtectiveWallComponent final : public MonoBehaviour
{
public:
    ProtectiveWallComponent(
        int linkId,
        int maxDurability,
        float moveRangeY,
        float moveSpeed,
        bool startsOn = false);

    void OnAttach(GameObject& owner) override;
    void DrawDebugUI() override;
    void ApplyDamage(int amount);
    int GetCurrentDurability() const;
    int GetMaxDurability() const;
    bool IsDestroyed() const;

    int linkId = 0;
    float moveRangeY = 144.0f;
    float moveSpeed = 240.0f;
    float baseY = 0.0f;
    bool isOn = false;
    bool destroyed = false;
    float damageAccumulator = 0.0f;

private:
    int m_maxDurability = 2;
    int m_currentDurability = 2;
};

enum class LaserTurretFireDirection
{
    Down,
    Up,
    Left,
    Right
};

class LaserTurretComponent final : public MonoBehaviour
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
    std::unordered_map<const GameObject*, float> enemyDamageTimers;
    float sparkTimer = 0.0f;
    bool active = true;
    bool fireToLeft = false;
    LaserTurretFireDirection fireDirection = LaserTurretFireDirection::Down;
    float warmupRemaining = 0.0f;
    float enemyKnockbackSpeed = 0.0f;
    GameObject* beamEntity = nullptr;
    float beamOriginOffsetX = 0.0f;
    float beamOriginOffsetY = 0.0f;
};

class LaserBeamComponent final : public MonoBehaviour
{
public:
    LaserBeamComponent(
        float damagePerSecond = 1.0f,
        float enemyKnockbackSpeed = 0.0f);

    void DrawDebugUI() override;

    float damagePerSecond = 1.0f;
    float enemyKnockbackSpeed = 0.0f;
    std::unordered_map<const GameObject*, float> enemyDamageTimers;
};

class BossBeamCaptureComponent final : public MonoBehaviour
{
public:
    BossBeamCaptureComponent() = default;

    bool captureEnabled = false;
    bool sourceOnLeft = true;
    float visualLeakLength = 12.0f;
};

class TagComponent final : public MonoBehaviour
{
public:
    explicit TagComponent(const char* value);
    explicit TagComponent(EntityTag value);

    bool Is(EntityTag value) const;
    bool Is(const char* value) const;

    EntityTag tagId = EntityTag::Unknown;

private:
    std::unique_ptr<std::string> m_customTag;
};
