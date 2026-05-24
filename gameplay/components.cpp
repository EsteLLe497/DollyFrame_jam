#include "pch.h"

#include "components.h"

#include <algorithm>

#include "audio.h"
#include "directX.h"
#include "entity.h"
#include "event_bus.h"
#include "imgui.h"
#include "input.h"
#include "physics_world.h"
#include "image_outline.h"
#include "photo_filter_rules.h"
#include "shader.h"
#include "sprite.h"

namespace
{
    constexpr float kMoveSpeed = 420.0f;
    constexpr float kRotateSpeed = 1.8f;
    constexpr float kScaleSpeed = 0.9f;
    constexpr float kPixelsPerMeter = 100.0f;
}

// ============================================================================
// Physics / Shared Basics
// ============================================================================
BarrelComponent::BarrelComponent(
    float gravityValue,
    float maxFallSpeedValue,
    float rollSpeedValue,
    float groundFrictionValue,
    int contactDamageValue,
    float breakMinFallDistanceValue,
    float breakMinImpactSpeedValue)
    : gravity(gravityValue)
    , maxFallSpeed(maxFallSpeedValue)
    , rollSpeed(rollSpeedValue)
    , groundFriction(groundFrictionValue)
    , contactDamage(std::max(1, contactDamageValue))
    , breakMinFallDistance(breakMinFallDistanceValue)
    , breakMinImpactSpeed(breakMinImpactSpeedValue)
{
}

void BarrelComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);

    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        spawnX = transform->x;
        spawnY = transform->y;
    }
}

void BarrelComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Barrel");
    ImGui::Text("Velocity: %.1f, %.1f", velocityX, velocityY);
    ImGui::Text("Grounded: %s", grounded ? "Yes" : "No");
    ImGui::Text("Active: %s", active ? "Yes" : "No");
    ImGui::Text("Cooldown: %.2f", cooldownRemaining);
    ImGui::Text("Destroyed: %s", destroyed ? "Yes" : "No");
    ImGui::Text("Fall Distance: %.1f", accumulatedFallDistance);
    ImGui::Text("Respawn Offscreen: %s", respawnWhenOffscreen ? "Yes" : "No");
}

BatteryComponent::BatteryComponent(
    float gravityValue,
    float maxFallSpeedValue,
    float pushSpeedValue,
    float fallDamageSpeedValue,
    int contactDamageValue)
    : gravity(gravityValue)
    , maxFallSpeed(maxFallSpeedValue)
    , pushSpeed(pushSpeedValue)
    , fallDamageSpeed(fallDamageSpeedValue)
    , contactDamage(std::max(1, contactDamageValue))
{
}

void BatteryComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);

    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        spawnX = transform->x;
        spawnY = transform->y;
    }
}

void BatteryComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Battery");
    ImGui::Text("Velocity: %.1f, %.1f", velocityX, velocityY);
    ImGui::Text("Grounded: %s", grounded ? "Yes" : "No");
    ImGui::Text("Push Speed: %.1f", pushSpeed);
    ImGui::Text("Fall Damage Speed: %.1f", fallDamageSpeed);
}

BatterySwitchComponent::BatterySwitchComponent(
    int linkIdValue,
    int requiredBatteryCountValue,
    float pressDepthValue,
    float pressSpeedValue,
    float releaseSpeedValue,
    bool controlsLaserPowerValue)
    : linkId((std::max)(0, linkIdValue))
    , requiredBatteryCount((std::max)(1, requiredBatteryCountValue))
    , pressDepth((std::max)(0.0f, pressDepthValue))
    , pressSpeed((std::max)(1.0f, pressSpeedValue))
    , releaseSpeed((std::max)(1.0f, releaseSpeedValue))
    , controlsLaserPower(controlsLaserPowerValue)
{
}

void BatterySwitchComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);
    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        baseY = transform->y;
    }
}

void BatterySwitchComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Battery Switch");
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("Target: %s", controlsLaserPower ? "Laser Power" : "Linked Gimmick");
    ImGui::Text("Battery: %d / %d", insertedBatteryCount, requiredBatteryCount);
    ImGui::Text("Press: %.1f / %.1f", currentPress, pressDepth);
    ImGui::Text("Pressed: %s", isPressed ? "Yes" : "No");
    ImGui::Text("Grace: %.2f", activationGraceRemaining);
}

ElevatorComponent::ElevatorComponent(
    int linkIdValue,
    float moveRangeYValue,
    float moveSpeedValue,
    float topPauseSecondsValue)
    : linkId((std::max)(0, linkIdValue))
    , moveRangeY((std::max)(0.0f, moveRangeYValue))
    , moveSpeed((std::max)(1.0f, moveSpeedValue))
    , topPauseSeconds((std::max)(0.0f, topPauseSecondsValue))
{
}

void ElevatorComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);
    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        baseY = transform->y;
    }
}

void ElevatorComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Elevator");
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("MoveRangeY: %.1f", moveRangeY);
    ImGui::Text("MoveSpeed: %.1f", moveSpeed);
    ImGui::Text("TopPause: %.2f", topPauseSeconds);
    ImGui::Text("CycleStarted: %s", cycleStarted ? "Yes" : "No");
    ImGui::Text("MovingUp: %s", movingUp ? "Yes" : "No");
}

LaserSwitchComponent::LaserSwitchComponent(int linkIdValue)
    : linkId((std::max)(0, linkIdValue))
{
}

void LaserSwitchComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Laser Switch");
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("Is On: %s", isOn ? "Yes" : "No");
}

ShutterComponent::ShutterComponent(
    int linkIdValue,
    float moveRangeYValue,
    float moveSpeedValue,
    bool useBossDefeatSignalValue,
    bool opensWhenUnpoweredValue)
    : linkId((std::max)(0, linkIdValue))
    , moveRangeY((std::max)(0.0f, moveRangeYValue))
    , moveSpeed((std::max)(1.0f, moveSpeedValue))
    , useBossDefeatSignal(useBossDefeatSignalValue)
    , opensWhenUnpowered(opensWhenUnpoweredValue)
{
}

void ShutterComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);
    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        baseY = transform->y;
    }
}

void ShutterComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Shutter");
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("MoveRangeY: %.1f", moveRangeY);
    ImGui::Text("MoveSpeed: %.1f", moveSpeed);
    ImGui::Text("Open: %s", isOpen ? "Yes" : "No");
    ImGui::Text("Boss Trigger: %s", useBossDefeatSignal ? "Yes" : "No");
    ImGui::Text("Open When Off: %s", opensWhenUnpowered ? "Yes" : "No");
}

ProtectiveWallComponent::ProtectiveWallComponent(
    int linkIdValue,
    int maxDurabilityValue,
    float moveRangeYValue,
    float moveSpeedValue,
    bool startsOnValue)
    : linkId((std::max)(0, linkIdValue))
    , moveRangeY((std::max)(0.0f, moveRangeYValue))
    , moveSpeed((std::max)(1.0f, moveSpeedValue))
    , isOn(startsOnValue)
    , m_maxDurability((std::max)(1, maxDurabilityValue))
    , m_currentDurability((std::max)(1, maxDurabilityValue))
{
}

void ProtectiveWallComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);
    if (auto* transform = owner.GetComponent<TransformComponent>())
    {
        baseY = transform->y;
        if (!isOn)
        {
            transform->y = baseY + moveRangeY;
        }
    }
}

void ProtectiveWallComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Protective Wall");
    ImGui::Text("Activation: Marker Light Number");
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("HP: %d / %d", m_currentDurability, m_maxDurability);
    ImGui::Text("On: %s", isOn ? "Yes" : "No");
    ImGui::Text("Destroyed: %s", destroyed ? "Yes" : "No");
    ImGui::Text("MoveRangeY: %.1f", moveRangeY);
    ImGui::Text("MoveSpeed: %.1f", moveSpeed);
}

void ProtectiveWallComponent::ApplyDamage(int amount)
{
    if (destroyed)
    {
        return;
    }

    m_currentDurability = std::max(0, m_currentDurability - std::max(0, amount));
    if (m_currentDurability <= 0)
    {
        destroyed = true;
        isOn = false;
    }
}

int ProtectiveWallComponent::GetCurrentDurability() const
{
    return m_currentDurability;
}

int ProtectiveWallComponent::GetMaxDurability() const
{
    return m_maxDurability;
}

bool ProtectiveWallComponent::IsDestroyed() const
{
    return destroyed || m_currentDurability <= 0;
}

LaserTurretComponent::LaserTurretComponent(
    float beamThicknessValue,
    float damagePerSecondValue,
    bool verticalValue,
    bool shootsLeftValue,
    bool requiresLaserPowerValue)
    : beamThickness((std::max)(1.0f, beamThicknessValue))
    , damagePerSecond((std::max)(0.1f, damagePerSecondValue))
    , vertical(verticalValue)
    , shootsLeft(shootsLeftValue)
    , requiresLaserPower(requiresLaserPowerValue)
    , fireToLeft(shootsLeftValue)
    , fireDirection(verticalValue
        ? LaserTurretFireDirection::Down
        : (shootsLeftValue ? LaserTurretFireDirection::Left : LaserTurretFireDirection::Right))
{
}

void LaserTurretComponent::DrawDebugUI()
{
    const char* directionName = "Down";
    switch (fireDirection)
    {
    case LaserTurretFireDirection::Down:
        directionName = "Down";
        break;
    case LaserTurretFireDirection::Up:
        directionName = "Up";
        break;
    case LaserTurretFireDirection::Left:
        directionName = "Left";
        break;
    case LaserTurretFireDirection::Right:
        directionName = "Right";
        break;
    }

    ImGui::SeparatorText("Laser Turret");
    ImGui::Text("Beam Thickness: %.1f", beamThickness);
    ImGui::Text("Damage / sec: %.2f", damagePerSecond);
    ImGui::Text("Direction: %s", directionName);
    ImGui::Text("Needs X Switch: %s", requiresLaserPower ? "Yes" : "No");
    ImGui::Text("Player Damage Timer: %.2f", playerDamageTimer);
    ImGui::Text("Active: %s", active ? "Yes" : "No");
    ImGui::Text("Warmup: %.2f", warmupRemaining);
    ImGui::Text("Beam Facing: %s", directionName);
    ImGui::Text("Enemy Knockback: %.1f", enemyKnockbackSpeed);
    ImGui::Text("Origin Offset: (%.1f, %.1f)", beamOriginOffsetX, beamOriginOffsetY);
}

LaserBeamComponent::LaserBeamComponent(float damagePerSecondValue, float enemyKnockbackSpeedValue)
    : damagePerSecond((std::max)(0.1f, damagePerSecondValue))
    , enemyKnockbackSpeed((std::max)(0.0f, enemyKnockbackSpeedValue))
{
}

void LaserBeamComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Laser Beam");
    ImGui::Text("Damage / sec: %.2f", damagePerSecond);
    ImGui::Text("Enemy Knockback: %.1f", enemyKnockbackSpeed);
}

TransformComponent::TransformComponent(float xValue, float yValue, float widthValue, float heightValue)
    : x(xValue)
    , y(yValue)
    , width(widthValue)
    , height(heightValue)
    , rotation(0.0f)
    , scale(1.0f)
{
}

TintComponent::TintComponent(float rValue, float gValue, float bValue, float aValue)
    : r(rValue)
    , g(gValue)
    , b(bValue)
    , a(aValue)
{
}

FlickerLightComponent::FlickerLightComponent(
    float radiusValue,
    float intensityValue,
    float flickerAmplitudeValue,
    float flickerSpeedValue,
    float offsetXValue,
    float offsetYValue,
    float rValue,
    float gValue,
    float bValue,
    bool godRayEnabledValue,
    float godRayLengthValue,
    float godRayWidthValue,
    float godRayIntensityValue,
    float godRayDriftSpeedValue,
    float godRaySoftnessValue)
    : radius(std::max(1.0f, radiusValue))
    , intensity(std::clamp(intensityValue, 0.0f, 1.0f))
    , flickerAmplitude(std::clamp(flickerAmplitudeValue, 0.0f, 1.0f))
    , flickerSpeed(std::max(0.0f, flickerSpeedValue))
    , offsetX(offsetXValue)
    , offsetY(offsetYValue)
    , r(std::clamp(rValue, 0.0f, 1.0f))
    , g(std::clamp(gValue, 0.0f, 1.0f))
    , b(std::clamp(bValue, 0.0f, 1.0f))
    , godRayEnabled(godRayEnabledValue)
    , godRayLength(std::max(1.0f, godRayLengthValue))
    , godRayWidth(std::max(1.0f, godRayWidthValue))
    , godRayIntensity(std::clamp(godRayIntensityValue, 0.0f, 1.0f))
    , godRayDriftSpeed(std::max(0.0f, godRayDriftSpeedValue))
    , godRaySoftness(std::clamp(godRaySoftnessValue, 0.0f, 1.0f))
{
}

void FlickerLightComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Flicker Light");
    ImGui::Text("Radius: %.1f", radius);
    ImGui::Text("Intensity: %.2f", intensity);
    ImGui::Text("Flicker: %.2f @ %.2f", flickerAmplitude, flickerSpeed);
    ImGui::Text("Offset: %.1f, %.1f", offsetX, offsetY);
    ImGui::Text("Color: %.2f %.2f %.2f", r, g, b);
    ImGui::Text("God Ray: %s", godRayEnabled ? "On" : "Off");
    if (godRayEnabled)
    {
        ImGui::Text("Beam: %.1f x %.1f", godRayLength, godRayWidth);
        ImGui::Text("Beam Intensity: %.2f", godRayIntensity);
        ImGui::Text("Beam Drift: %.2f  Softness: %.2f", godRayDriftSpeed, godRaySoftness);
    }
}

MarkerLightComponent::MarkerLightComponent(float radiusValue, float intensityValue, int linkIdValue)
    : radius(std::max(1.0f, radiusValue))
    , intensity(std::clamp(intensityValue, 0.0f, 1.0f))
    , linkId((std::max)(-1, linkIdValue))
{
}

void MarkerLightComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Marker Light");
    ImGui::Text("Radius: %.1f", radius);
    ImGui::Text("Intensity: %.2f", intensity);
    ImGui::Text("LinkId: %d", linkId);
    ImGui::Text("Activated: %s", activated ? "On" : "Off");
}

StageLightComponent::StageLightComponent(
    bool enabledValue,
    float fixtureTopWidthRatioValue,
    float beamLengthValue,
    float beamTopWidthValue,
    float beamBottomWidthValue,
    float beamFeatherValue,
    float rValue,
    float gValue,
    float bValue,
    float intensityValue)
    : enabled(enabledValue)
    , fixtureTopWidthRatio(std::clamp(fixtureTopWidthRatioValue, 0.05f, 1.0f))
    , beamLength(std::max(0.0f, beamLengthValue))
    , beamTopWidth(std::max(0.0f, beamTopWidthValue))
    , beamBottomWidth(std::max(0.0f, beamBottomWidthValue))
    , beamFeather(std::max(0.0f, beamFeatherValue))
    , r(std::clamp(rValue, 0.0f, 1.0f))
    , g(std::clamp(gValue, 0.0f, 1.0f))
    , b(std::clamp(bValue, 0.0f, 1.0f))
    , intensity(std::clamp(intensityValue, 0.0f, 1.0f))
{
}

void StageLightComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Stage Light");
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::SliderFloat("Fixture Top Ratio", &fixtureTopWidthRatio, 0.05f, 1.0f);
    ImGui::DragFloat("Beam Length", &beamLength, 1.0f, 0.0f, 4096.0f);
    ImGui::DragFloat("Beam Top Width", &beamTopWidth, 1.0f, 0.0f, 4096.0f);
    ImGui::DragFloat("Beam Bottom Width", &beamBottomWidth, 1.0f, 0.0f, 4096.0f);
    ImGui::DragFloat("Beam Feather", &beamFeather, 1.0f, 0.0f, 1024.0f);
    ImGui::SliderFloat("Intensity", &intensity, 0.0f, 1.0f);
    ImGui::ColorEdit3("Color", &r);
}

TagComponent::TagComponent(const char* value)
    : tag(value ? value : "")
{
}

// ============================================================================
// Photo System Components
// ============================================================================
PhotoCopyRoleComponent::PhotoCopyRoleComponent(PhotoCopyRole roleValue)
    : role(roleValue)
{
}

PhotoCopyLayerComponent::PhotoCopyLayerComponent(PhotoCopyLayer layerValue)
    : layer(layerValue)
{
}

PhotoCopyGroupComponent::PhotoCopyGroupComponent(int groupIdValue)
    : groupId(groupIdValue)
{
}

PhotoPasteOrderComponent::PhotoPasteOrderComponent(int orderValue)
    : order(orderValue)
{
}

PhotoCopyLifetimeComponent::PhotoCopyLifetimeComponent(float lifetimeSeconds)
    : m_lifetimeSeconds(std::max(0.0f, lifetimeSeconds))
    , m_remainingSeconds(std::max(0.0f, lifetimeSeconds))
{
}

void PhotoCopyLifetimeComponent::Update(float deltaTime)
{
    // マイナス方向へだけ減衰させ、下限は 0 に固定。
    m_remainingSeconds = std::max(0.0f, m_remainingSeconds - deltaTime);
}

void PhotoCopyLifetimeComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Photo Lifetime");
    ImGui::Text("Remaining: %.2f / %.2f", m_remainingSeconds, m_lifetimeSeconds);
}

float PhotoCopyLifetimeComponent::GetRemainingSeconds() const
{
    return m_remainingSeconds;
}

float PhotoCopyLifetimeComponent::GetLifetimeSeconds() const
{
    return m_lifetimeSeconds;
}

bool PhotoCopyLifetimeComponent::IsExpired() const
{
    return m_remainingSeconds <= 0.0f;
}

PhotoPasteAnimationComponent::PhotoPasteAnimationComponent(float durationSeconds)
    : m_durationSeconds(std::max(0.001f, durationSeconds))
    , m_elapsedSeconds(0.0f)
{
}

void PhotoPasteAnimationComponent::Update(float deltaTime)
{
    m_elapsedSeconds = std::min(m_durationSeconds, m_elapsedSeconds + std::max(0.0f, deltaTime));
}

float PhotoPasteAnimationComponent::GetNormalizedProgress() const
{
    return std::clamp(m_elapsedSeconds / m_durationSeconds, 0.0f, 1.0f);
}

bool PhotoPasteAnimationComponent::IsFinished() const
{
    return m_elapsedSeconds >= m_durationSeconds;
}

PhotoCopyOriginComponent::PhotoCopyOriginComponent(PhotoCopyOrigin originValue)
    : origin(originValue)
{
}

PhotoCopyTileValueComponent::PhotoCopyTileValueComponent(int tileValueValue)
    : tileValue(tileValueValue)
{
}

DamagePlatformComponent::DamagePlatformComponent(int tileSpanValue)
    : tileSpan(std::max(1, tileSpanValue))
{
}

SpikeStripComponent::SpikeStripComponent(int tileSpanValue)
    : tileSpan(std::max(1, tileSpanValue))
{
}

VanishOnCaptureComponent::VanishOnCaptureComponent(bool enabledValue)
    : enabled(enabledValue)
{
}

namespace
{
    const char* ToEnemyArchetypeLabel(EnemyArchetype archetype)
    {
        switch (archetype)
        {
        case EnemyArchetype::Walker:
            return "Walker";
        case EnemyArchetype::Turret:
            return "Turret";
        case EnemyArchetype::Ranged: 
            return "Ranged";
        case EnemyArchetype::ShieldBoss:
            return "ShieldBoss";
        case EnemyArchetype::MidBoss2:
            return "MidBoss2";
        case EnemyArchetype::Ghost:
            return "Ghost";
        case EnemyArchetype::BlasterRobot:
            return "BlasterRobot";
        case EnemyArchetype::Charger:
            return "Charger";
        case EnemyArchetype::Floater:
        default:
            return "Floater";
        }
    }

    const char* ToGimmickTypeLabel(GimmickType type)
    {
        switch (type)
        {
        case GimmickType::Goal:
            return "Goal";
        case GimmickType::Pickup:
            return "Pickup";
        case GimmickType::Checkpoint:
            return "Checkpoint";
        case GimmickType::PhotoSource:
            return "Photo Source";
        case GimmickType::Filter:
            return "Filter";
        case GimmickType::Gate:
            return "Gate";
        case GimmickType::Switch:
            return "Switch";
        case GimmickType::Hazard:
        default:
            return "Hazard";
        }
    }
}

PhotoCopyEffectComponent::PhotoCopyEffectComponent(PhotoFilterTheme themeValue)
    : m_theme(themeValue)
{
}

void PhotoCopyEffectComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Photo Effect");
    ImGui::Text("Theme: %s", GetPhotoFilterThemeLabel(m_theme));
}

PhotoFilterTheme PhotoCopyEffectComponent::GetTheme() const
{
    return m_theme;
}

void PhotoCopyEffectComponent::SetTheme(PhotoFilterTheme themeValue)
{
    m_theme = themeValue;
}

// ============================================================================
// Enemy / Combat Domain
// ============================================================================
EnemyComponent::EnemyComponent(EnemyArchetype archetype, int contactDamage)
    : m_archetype(archetype)
    , m_contactDamage(std::max(0, contactDamage))
    , m_enabled(true)
    , m_defeated(false)
{
}

void EnemyComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Enemy");
    ImGui::Text("Type: %s", ToEnemyArchetypeLabel(m_archetype));
    ImGui::Text("Contact Damage: %d", m_contactDamage);
    ImGui::Text("Enabled: %s", m_enabled ? "Yes" : "No");
    ImGui::Text("Defeated: %s", m_defeated ? "Yes" : "No");
}

EnemyArchetype EnemyComponent::GetArchetype() const
{
    return m_archetype;
}

int EnemyComponent::GetContactDamage() const
{
    return m_contactDamage;
}

bool EnemyComponent::IsEnabled() const
{
    return m_enabled && !m_defeated;
}

void EnemyComponent::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool EnemyComponent::IsDefeated() const
{
    return m_defeated;
}

void EnemyComponent::MarkDefeated()
{
    m_defeated = true;
    m_enabled = false;
    attackFrameTriggered = false;
    attackCaptureWindowActive = false;
    attackWarningProgress = 0.0f;
    attackFlashRemaining = 0.0f;
    knockbackActive = false;
    knockbackTimer = 0.0f;
}

void EnemyComponent::Restore()
{
    m_defeated = false;
    m_enabled = true;
    attackFrameTriggered = false;
    attackCaptureWindowActive = false;
    attackWarningProgress = 0.0f;
    attackFlashRemaining = 0.0f;
    knockbackActive = false;
    knockbackTimer = 0.0f;
}

// ============================================================================
// Stage Gimmick Domain
// ============================================================================
GimmickComponent::GimmickComponent(GimmickType type, bool startsEnabled, bool oneShot)
    : m_type(type)
    , m_enabled(startsEnabled)
    , m_oneShot(oneShot)
    , m_consumed(false)
{
}

void GimmickComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Gimmick");
    ImGui::Text("Type: %s", ToGimmickTypeLabel(m_type));
    ImGui::Text("Enabled: %s", m_enabled ? "Yes" : "No");
    ImGui::Text("One Shot: %s", m_oneShot ? "Yes" : "No");
    ImGui::Text("Consumed: %s", m_consumed ? "Yes" : "No");
}

GimmickType GimmickComponent::GetType() const
{
    return m_type;
}

bool GimmickComponent::IsEnabled() const
{
    return m_enabled && (!m_oneShot || !m_consumed);
}

void GimmickComponent::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool GimmickComponent::IsOneShot() const
{
    return m_oneShot;
}

bool GimmickComponent::IsConsumed() const
{
    return m_consumed;
}

void GimmickComponent::Consume()
{
    if (m_oneShot)
    {
        m_consumed = true;
    }
}

void GimmickComponent::Restore()
{
    m_enabled = true;
    m_consumed = false;
}

CheckpointComponent::CheckpointComponent(int checkpointIdValue, float respawnXValue, float respawnYValue)
    : checkpointId(checkpointIdValue)
    , respawnX(respawnXValue)
    , respawnY(respawnYValue)
    , activated(false)
{
}

void CheckpointComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Checkpoint");
    ImGui::Text("Id: %d", checkpointId);
    ImGui::Text("Respawn: %.1f, %.1f", respawnX, respawnY);
    ImGui::Text("Activated: %s", activated ? "Yes" : "No");
}

PhotoFilterComponent::PhotoFilterComponent(PhotoFilterTheme theme, PhotoCopyRole outputRole, PhotoCopyLayer outputLayer, float tintR, float tintG, float tintB, float tintA)
    : m_theme(theme)
    , m_outputRole(outputRole)
    , m_outputLayer(outputLayer)
    , m_tintR(tintR)
    , m_tintG(tintG)
    , m_tintB(tintB)
    , m_tintA(tintA)
{
}

void PhotoFilterComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Photo Filter");
    ImGui::Text("Theme: %s", GetPhotoFilterThemeLabel(m_theme));
    ImGui::Text("Role: %d", static_cast<int>(m_outputRole));
    ImGui::Text("Layer: %d", static_cast<int>(m_outputLayer));
    ImGui::Text("Tint: %.2f %.2f %.2f %.2f", m_tintR, m_tintG, m_tintB, m_tintA);
}

PhotoFilterTheme PhotoFilterComponent::GetTheme() const
{
    return m_theme;
}

PhotoCopyRole PhotoFilterComponent::GetOutputRole() const
{
    return m_outputRole;
}

PhotoCopyLayer PhotoFilterComponent::GetOutputLayer() const
{
    return m_outputLayer;
}

float PhotoFilterComponent::GetTintR() const
{
    return m_tintR;
}

float PhotoFilterComponent::GetTintG() const
{
    return m_tintG;
}

float PhotoFilterComponent::GetTintB() const
{
    return m_tintB;
}

float PhotoFilterComponent::GetTintA() const
{
    return m_tintA;
}

// ============================================================================
// Gameplay Common Domain
// ============================================================================
HealthComponent::HealthComponent(int maxHealth)
    : m_maxHealth(std::max(1, maxHealth))
    , m_currentHealth(std::max(1, maxHealth))
{
}

void HealthComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Health");
    ImGui::Text("HP: %d / %d", m_currentHealth, m_maxHealth);
}

void HealthComponent::ApplyDamage(int amount)
{
    m_currentHealth = std::max(0, m_currentHealth - std::max(0, amount));
}

void HealthComponent::SetCurrentHealth(int value)
{
    m_currentHealth = std::clamp(value, 0, m_maxHealth);
}

void HealthComponent::RestoreToFull()
{
    m_currentHealth = m_maxHealth;
}

int HealthComponent::GetCurrentHealth() const
{
    return m_currentHealth;
}

int HealthComponent::GetMaxHealth() const
{
    return m_maxHealth;
}

bool HealthComponent::IsDead() const
{
    return m_currentHealth <= 0;
}

DamageCooldownComponent::DamageCooldownComponent(float cooldownSeconds)
    : m_cooldownSeconds(std::max(0.0f, cooldownSeconds))
    , m_remainingSeconds(0.0f)
{
}

void DamageCooldownComponent::Update(float deltaTime)
{
    m_remainingSeconds = std::max(0.0f, m_remainingSeconds - deltaTime);
}

void DamageCooldownComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Damage Cooldown");
    ImGui::Text("Remaining: %.2f", m_remainingSeconds);
}

bool DamageCooldownComponent::CanTakeDamage() const
{
    return m_remainingSeconds <= 0.0f;
}

void DamageCooldownComponent::Trigger()
{
    m_remainingSeconds = m_cooldownSeconds;
}

void DamageCooldownComponent::SetRemainingSeconds(float seconds)
{
    m_remainingSeconds = std::max(0.0f, seconds);
}

float DamageCooldownComponent::GetCooldownSeconds() const
{
    return m_cooldownSeconds;
}

float DamageCooldownComponent::GetRemainingSeconds() const
{
    return m_remainingSeconds;
}

// ============================================================================
// Rendering / Animation Domain
// ============================================================================
SpriteRenderComponent::SpriteRenderComponent(int textureId)
    : m_textureId(textureId)
    , m_sourceX(0.0f)
    , m_sourceY(0.0f)
    , m_sourceWidth(1.0f)
    , m_sourceHeight(1.0f)
    , m_flipX(false)
    , m_renderOffsetX(0.0f)
    , m_renderOffsetY(0.0f)
    , m_renderScaleX(1.0f)
    , m_renderScaleY(1.0f)
    , m_renderRotationOffset(0.0f)
{
}

void SpriteRenderComponent::Draw()
{
    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }

    const auto* tint = m_owner->GetComponent<TintComponent>();
    if (tint)
    {
        Shader_SetTint(tint->r, tint->g, tint->b, tint->a);
    }
    else
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    }
    SpriteDraw(
        m_textureId,
        transform->x + m_renderOffsetX,
        transform->y + m_renderOffsetY,
        transform->width * transform->scale * m_renderScaleX,
        transform->height * transform->scale * m_renderScaleY,
        m_sourceX,
        m_sourceY,
        m_sourceWidth,
        m_sourceHeight,
        m_flipX,
        transform->rotation + m_renderRotationOffset);
}

int SpriteRenderComponent::GetTextureId() const
{
    return m_textureId;
}

void SpriteRenderComponent::SetTextureId(int textureId)
{
    m_textureId = textureId;
}

void SpriteRenderComponent::SetSourceRect(float tx, float ty, float tw, float th)
{
    m_sourceX = tx;
    m_sourceY = ty;
    m_sourceWidth = tw;
    m_sourceHeight = th;
}

float SpriteRenderComponent::GetSourceX() const
{
    return m_sourceX;
}

float SpriteRenderComponent::GetSourceY() const
{
    return m_sourceY;
}

float SpriteRenderComponent::GetSourceWidth() const
{
    return m_sourceWidth;
}

float SpriteRenderComponent::GetSourceHeight() const
{
    return m_sourceHeight;
}

void SpriteRenderComponent::SetFlipX(bool value)
{
    m_flipX = value;
}

bool SpriteRenderComponent::GetFlipX() const
{
    return m_flipX;
}

void SpriteRenderComponent::SetRenderOffset(float x, float y)
{
    m_renderOffsetX = x;
    m_renderOffsetY = y;
}

void SpriteRenderComponent::SetRenderScale(float x, float y)
{
    m_renderScaleX = x;
    m_renderScaleY = y;
}

void SpriteRenderComponent::SetRenderRotationOffset(float radians)
{
    m_renderRotationOffset = radians;
}

float SpriteRenderComponent::GetRenderOffsetX() const
{
    return m_renderOffsetX;
}

float SpriteRenderComponent::GetRenderOffsetY() const
{
    return m_renderOffsetY;
}

float SpriteRenderComponent::GetRenderScaleX() const
{
    return m_renderScaleX;
}

float SpriteRenderComponent::GetRenderScaleY() const
{
    return m_renderScaleY;
}

float SpriteRenderComponent::GetRenderRotationOffset() const
{
    return m_renderRotationOffset;
}

SpriteSheetAnimationComponent::SpriteSheetAnimationComponent()
    : m_elapsedSeconds(0.0f)
    , m_playbackSpeed(1.0f)
{
}

void SpriteSheetAnimationComponent::Update(float deltaTime)
{
    if (m_currentClipName.empty())
    {
        return;
    }

    const auto found = m_clips.find(m_currentClipName);
    if (found == m_clips.end())
    {
        return;
    }

    const Clip& clip = found->second;
    if (clip.frameCount <= 0)
    {
        return;
    }

    m_elapsedSeconds += std::max(0.0f, deltaTime) * std::max(0.0f, m_playbackSpeed);
    ApplyFrameToSprite();
}

void SpriteSheetAnimationComponent::DrawDebugUI()
{
    if (m_currentClipName.empty())
    {
        return;
    }

    ImGui::SeparatorText("Sprite Sheet Animation");
    ImGui::Text("Clip: %s", m_currentClipName.c_str());
    ImGui::Text("Frame: %d", GetCurrentFrameIndex());
    ImGui::Text("Elapsed: %.2f", m_elapsedSeconds);
}

void SpriteSheetAnimationComponent::DefineClip(
    const std::string& name,
    int textureId,
    int columns,
    int rows,
    int startFrame,
    int frameCount,
    float fps,
    bool loop)
{
    if (name.empty())
    {
        return;
    }

    Clip clip;
    clip.textureId = textureId;
    clip.columns = (std::max)(1, columns);
    clip.rows = (std::max)(1, rows);
    clip.startFrame = (std::max)(0, startFrame);
    clip.frameCount = (std::max)(1, frameCount);
    clip.fps = (std::max)(0.0f, fps);
    clip.loop = loop;
    m_clips[name] = clip;

    if (m_currentClipName == name)
    {
        ApplyFrameToSprite();
    }
}

bool SpriteSheetAnimationComponent::HasClip(const std::string& name) const
{
    return m_clips.find(name) != m_clips.end();
}

bool SpriteSheetAnimationComponent::Play(const std::string& name, bool restartIfSame)
{
    if (!HasClip(name))
    {
        return false;
    }

    if (!restartIfSame && m_currentClipName == name)
    {
        return false;
    }

    m_currentClipName = name;
    m_elapsedSeconds = 0.0f;
    ApplyFrameToSprite();
    return true;
}

const std::string& SpriteSheetAnimationComponent::GetCurrentClipName() const
{
    return m_currentClipName;
}

int SpriteSheetAnimationComponent::GetCurrentFrameIndex() const
{
    const auto found = m_clips.find(m_currentClipName);
    if (found == m_clips.end())
    {
        return 0;
    }

    const Clip& clip = found->second;
    if (clip.frameCount <= 1 || clip.fps <= 0.0f)
    {
        return clip.startFrame;
    }

    // 経過秒をフレームへ変換。loop=false の場合は末尾フレームで停止。
    const float rawFrame = m_elapsedSeconds * clip.fps;
    const int localFrame = clip.loop
        ? static_cast<int>(rawFrame) % clip.frameCount
        : (std::min)(clip.frameCount - 1, static_cast<int>(rawFrame));
    return clip.startFrame + localFrame;
}

void SpriteSheetAnimationComponent::SetPlaybackSpeed(float speed)
{
    m_playbackSpeed = (std::max)(0.0f, speed);
}

void SpriteSheetAnimationComponent::ApplyFrameToSprite()
{
    if (!m_owner || m_currentClipName.empty())
    {
        return;
    }

    auto* sprite = m_owner->GetComponent<SpriteRenderComponent>();
    if (!sprite)
    {
        return;
    }

    const auto found = m_clips.find(m_currentClipName);
    if (found == m_clips.end())
    {
        return;
    }

    const Clip& clip = found->second;
    const int frameIndex = GetCurrentFrameIndex();
    // スプライトシート上のフレーム番号を行列インデックスへ変換。
    const int column = frameIndex % clip.columns;
    const int row = frameIndex / clip.columns;
    const float cellWidth = 1.0f / static_cast<float>(clip.columns);
    const float cellHeight = 1.0f / static_cast<float>(clip.rows);

    if (clip.textureId >= 0)
    {
        sprite->SetTextureId(clip.textureId);
    }

    sprite->SetSourceRect(
        static_cast<float>(column) * cellWidth,
        static_cast<float>(row) * cellHeight,
        cellWidth,
        cellHeight);
}

void PlayerControllerComponent::Update(float deltaTime)
{
    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }

    float moveX = 0.0f;
    float moveY = 0.0f;
    if (Input_IsKeyDown(VK_LEFT)) { moveX -= 1.0f; }
    if (Input_IsKeyDown(VK_RIGHT)) { moveX += 1.0f; }
    if (Input_IsKeyDown(VK_UP)) { moveY -= 1.0f; }
    if (Input_IsKeyDown(VK_DOWN)) { moveY += 1.0f; }

    moveX += Input_GetMoveX();
    moveY += Input_GetMoveY();

    if (auto* rigidBody = m_owner->GetComponent<RigidBodyComponent>())
    {
        rigidBody->SetLinearVelocity(moveX * (kMoveSpeed / kPixelsPerMeter), moveY * (kMoveSpeed / kPixelsPerMeter));
    }
    else
    {
        transform->x += moveX * kMoveSpeed * deltaTime;
        transform->y += moveY * kMoveSpeed * deltaTime;
    }

    if (Input_IsKeyDown('Q')) { transform->rotation -= kRotateSpeed * deltaTime; }
    if (Input_IsKeyDown('E')) { transform->rotation += kRotateSpeed * deltaTime; }
    transform->rotation += Input_GetRotateAxis() * kRotateSpeed * deltaTime;

    if (Input_IsKeyDown('Z')) { transform->scale = std::max(0.25f, transform->scale - kScaleSpeed * deltaTime); }
    if (Input_IsKeyDown('X')) { transform->scale = std::min(3.0f, transform->scale + kScaleSpeed * deltaTime); }

    if (Input_IsKeyPressed(VK_SPACE) || Input_IsSouthButtonPressed())
    {
        if (m_eventBus)
        {
            m_eventBus->Publish({ EventType::PlaySoundRequest, m_owner, nullptr, "test_tone", 0.0f, 0.0f });
        }
    }

    transform->x = std::clamp(transform->x, 0.0f, static_cast<float>(SCREEN_WIDTH) - transform->width * transform->scale);
    transform->y = std::clamp(transform->y, 0.0f, static_cast<float>(SCREEN_HEIGHT) - transform->height * transform->scale);
}

void PlayerControllerComponent::DrawDebugUI()
{
    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }

    ImGui::SeparatorText("Player Controller");
    ImGui::SliderFloat2("Position", &transform->x, 0.0f, static_cast<float>(std::max(SCREEN_WIDTH, SCREEN_HEIGHT)));
    ImGui::SliderFloat("Rotation", &transform->rotation, -6.283f, 6.283f);
    ImGui::SliderFloat("Scale", &transform->scale, 0.25f, 3.0f);
}

PlayerControllerComponent::PlayerControllerComponent(EventBus& eventBus)
    : m_eventBus(&eventBus)
{
}

// ============================================================================
// Movement / Physics Domain
// ============================================================================
EnemyMoverComponent::EnemyMoverComponent(float originX, float originY, float amplitudeX, float amplitudeY, float frequency)
    : m_originX(originX)
    , m_originY(originY)
    , m_amplitudeX(amplitudeX)
    , m_amplitudeY(amplitudeY)
    , m_frequency(frequency)
    , m_time(0.0f)
    , m_frozen(false)
{
}

void EnemyMoverComponent::Update(float deltaTime)
{
    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }
    if (m_frozen)
    {
        return;
    }

    m_time += deltaTime;
    transform->x = m_originX + std::sin(m_time * m_frequency) * m_amplitudeX;
    transform->y = m_originY + std::cos(m_time * m_frequency * 0.8f) * m_amplitudeY;
}

void EnemyMoverComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Enemy Mover");
    ImGui::Text("Origin: %.1f, %.1f", m_originX, m_originY);
    ImGui::Text("Amplitude: %.1f, %.1f", m_amplitudeX, m_amplitudeY);
    ImGui::Text("Frequency: %.2f", m_frequency);
    ImGui::Text("Frozen: %s", m_frozen ? "Yes" : "No");
}

void EnemyMoverComponent::SetOrigin(float originX, float originY)
{
    m_originX = originX;
    m_originY = originY;
}

void EnemyMoverComponent::SetFrozen(bool frozen)
{
    m_frozen = frozen;
}

bool EnemyMoverComponent::IsFrozen() const
{
    return m_frozen;
}

void EnemyMoverComponent::Rewind(float seconds)
{
    if (seconds <= 0.0f)
    {
        return;
    }

    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
    {
        return;
    }

    m_time = std::max(0.0f, m_time - seconds);
    transform->x = m_originX + std::sin(m_time * m_frequency) * m_amplitudeX;
    transform->y = m_originY + std::cos(m_time * m_frequency * 0.8f) * m_amplitudeY;
}

RigidBodyComponent::RigidBodyComponent(PhysicsWorld& physicsWorld, b2BodyType bodyType, bool fixedRotation, float gravityScale)
    : m_physicsWorld(&physicsWorld)
    , m_bodyType(bodyType)
    , m_fixedRotation(fixedRotation)
    , m_gravityScale(gravityScale)
    , m_bodyId(b2_nullBodyId)
{
}

RigidBodyComponent::~RigidBodyComponent()
{
    if (B2_IS_NON_NULL(m_bodyId))
    {
        b2DestroyBody(m_bodyId);
        m_bodyId = b2_nullBodyId;
    }
}

void RigidBodyComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);

    const auto* transform = owner.GetComponent<TransformComponent>();
    if (!m_physicsWorld || !transform)
    {
        return;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = m_bodyType;
    // 描画基準（左上）から物理基準（中心）へ変換して初期配置する。
    bodyDef.position = {
        (transform->x + (transform->width * transform->scale * 0.5f)) / kPixelsPerMeter,
        (transform->y + (transform->height * transform->scale * 0.5f)) / kPixelsPerMeter
    };
    bodyDef.rotation = b2MakeRot(transform->rotation);
    bodyDef.gravityScale = m_gravityScale;
    bodyDef.motionLocks.angularZ = m_fixedRotation;
    bodyDef.userData = &owner;
    m_bodyId = b2CreateBody(m_physicsWorld->GetWorldId(), &bodyDef);
}

void RigidBodyComponent::DrawDebugUI()
{
    if (B2_IS_NULL(m_bodyId))
    {
        return;
    }

    const b2Vec2 velocity = b2Body_GetLinearVelocity(m_bodyId);
    ImGui::SeparatorText("RigidBody");
    ImGui::Text("Body Type: %d", static_cast<int>(m_bodyType));
    ImGui::Text("Velocity: %.2f, %.2f", velocity.x, velocity.y);
}

void RigidBodyComponent::PushTransformToPhysics()
{
    if (B2_IS_NULL(m_bodyId) || !m_owner)
    {
        return;
    }

    const auto* transform = m_owner->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    if (m_bodyType == b2_kinematicBody || m_bodyType == b2_staticBody)
    {
        const b2Vec2 position = {
            (transform->x + (transform->width * transform->scale * 0.5f)) / kPixelsPerMeter,
            (transform->y + (transform->height * transform->scale * 0.5f)) / kPixelsPerMeter
        };
        b2Body_SetTransform(m_bodyId, position, b2MakeRot(transform->rotation));
    }
}

void RigidBodyComponent::PullTransformFromPhysics()
{
    if (B2_IS_NULL(m_bodyId) || !m_owner)
    {
        return;
    }

    auto* transform = m_owner->GetComponent<TransformComponent>();
    if (!transform)
    {
        return;
    }

    const b2Vec2 position = b2Body_GetPosition(m_bodyId);
    const b2Rot rotation = b2Body_GetRotation(m_bodyId);
    const float width = transform->width * transform->scale;
    const float height = transform->height * transform->scale;
    // 物理中心座標を描画用の左上座標へ戻す。
    transform->x = position.x * kPixelsPerMeter - (width * 0.5f);
    transform->y = position.y * kPixelsPerMeter - (height * 0.5f);
    transform->rotation = b2Rot_GetAngle(rotation);
}

b2BodyId RigidBodyComponent::GetBodyId() const
{
    return m_bodyId;
}

b2BodyType RigidBodyComponent::GetBodyType() const
{
    return m_bodyType;
}

void RigidBodyComponent::SetLinearVelocity(float x, float y)
{
    if (B2_IS_NULL(m_bodyId))
    {
        return;
    }

    b2Body_SetLinearVelocity(m_bodyId, { x, y });
}

BoxColliderComponent::BoxColliderComponent(float density, float friction, bool isSensor)
    : m_density(density)
    , m_friction(friction)
    , m_isSensor(isSensor)
    , m_shapeId(b2_nullShapeId)
{
}

BoxColliderComponent::~BoxColliderComponent() = default;

void BoxColliderComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);

    const auto* transform = owner.GetComponent<TransformComponent>();
    const auto* rigidBody = owner.GetComponent<RigidBodyComponent>();
    if (!transform || !rigidBody || B2_IS_NULL(rigidBody->GetBodyId()))
    {
        return;
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = m_density;
    shapeDef.material.friction = m_friction;
    shapeDef.isSensor = m_isSensor;
    shapeDef.enableContactEvents = true;
    shapeDef.enableSensorEvents = m_isSensor;
    shapeDef.userData = &owner;

    const b2Polygon box = b2MakeBox(
        (transform->width * transform->scale * 0.5f) / kPixelsPerMeter,
        (transform->height * transform->scale * 0.5f) / kPixelsPerMeter);
    m_shapeId = b2CreatePolygonShape(rigidBody->GetBodyId(), &shapeDef, &box);
}

void BoxColliderComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Collider");
    ImGui::Text("Sensor: %s", m_isSensor ? "Yes" : "No");
    ImGui::Text("Density: %.2f", m_density);
    ImGui::Text("Friction: %.2f", m_friction);
}

b2ShapeId BoxColliderComponent::GetShapeId() const
{
    return m_shapeId;
}

ImageOutlineColliderComponent::ImageOutlineColliderComponent(std::string imagePath, float friction, int alphaThreshold, int vertexStride)
    : m_imagePath(std::move(imagePath))
    , m_friction(friction)
    , m_alphaThreshold(alphaThreshold)
    , m_vertexStride(vertexStride)
    , m_chainId(b2_nullChainId)
    , m_vertexCount(0)
{
}

ImageOutlineColliderComponent::ImageOutlineColliderComponent(std::vector<b2Vec2> normalizedOutline, float friction)
    : m_imagePath()
    , m_friction(friction)
    , m_alphaThreshold(16)
    , m_vertexStride(1)
    , m_chainId(b2_nullChainId)
    , m_vertexCount(0)
    , m_normalizedOutline(std::move(normalizedOutline))
{
}

ImageOutlineColliderComponent::~ImageOutlineColliderComponent()
{
    if (b2Chain_IsValid(m_chainId))
    {
        b2DestroyChain(m_chainId);
        m_chainId = b2_nullChainId;
    }
}

void ImageOutlineColliderComponent::OnAttach(Entity& owner)
{
    Component::OnAttach(owner);

    const auto* transform = owner.GetComponent<TransformComponent>();
    const auto* rigidBody = owner.GetComponent<RigidBodyComponent>();
    if (!transform)
    {
        return;
    }

    if (m_normalizedOutline.empty() && !m_imagePath.empty())
    {
        std::vector<ImageOutline::Point> outline;
        int imageWidth = 0;
        int imageHeight = 0;
        if (!ImageOutline::BuildOutlineFromAlpha(m_imagePath, m_alphaThreshold, m_vertexStride, outline, imageWidth, imageHeight))
        {
            return;
        }

        if (outline.size() < 3 || imageWidth <= 0 || imageHeight <= 0)
        {
            return;
        }

        m_normalizedOutline.clear();
        m_normalizedOutline.reserve(outline.size());
        // 画像ピクセル座標を [0,1] 正規化で保持し、再利用可能な形にする。
        for (const ImageOutline::Point& point : outline)
        {
            const float u = static_cast<float>(point.x) / static_cast<float>(imageWidth);
            const float v = static_cast<float>(point.y) / static_cast<float>(imageHeight);
            m_normalizedOutline.push_back({ u, v });
        }
    }

    m_vertexCount = static_cast<int>(m_normalizedOutline.size());
    if (m_normalizedOutline.size() < 3 || !rigidBody || B2_IS_NULL(rigidBody->GetBodyId()))
    {
        return;
    }

    if (rigidBody->GetBodyType() != b2_staticBody)
    {
        return;
    }

    const float worldWidth = transform->width * transform->scale;
    const float worldHeight = transform->height * transform->scale;
    const float halfWidth = worldWidth * 0.5f;
    const float halfHeight = worldHeight * 0.5f;
    std::vector<b2Vec2> points;
    points.reserve(m_normalizedOutline.size());
    // 正規化輪郭をワールドメートル座標に展開し、Chain 形状を作る。
    for (const b2Vec2& point : m_normalizedOutline)
    {
        points.push_back({
            (point.x * worldWidth - halfWidth) / kPixelsPerMeter,
            (point.y * worldHeight - halfHeight) / kPixelsPerMeter
        });
    }

    if (points.size() < 4)
    {
        return;
    }

    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = m_friction;

    b2ChainDef chainDef = b2DefaultChainDef();
    chainDef.userData = &owner;
    chainDef.points = points.data();
    chainDef.count = static_cast<int>(points.size());
    chainDef.materials = &material;
    chainDef.materialCount = 1;
    chainDef.isLoop = true;
    chainDef.enableSensorEvents = false;
    m_chainId = b2CreateChain(rigidBody->GetBodyId(), &chainDef);
}

void ImageOutlineColliderComponent::DrawDebugUI()
{
    ImGui::SeparatorText("Image Outline Collider");
    ImGui::Text("Image: %s", m_imagePath.c_str());
    ImGui::Text("Alpha Threshold: %d", m_alphaThreshold);
    ImGui::Text("Vertex Stride: %d", m_vertexStride);
    ImGui::Text("Vertices: %d", m_vertexCount);
}

b2ChainId ImageOutlineColliderComponent::GetChainId() const
{
    return m_chainId;
}

const std::vector<b2Vec2>& ImageOutlineColliderComponent::GetNormalizedOutline() const
{
    return m_normalizedOutline;
}
