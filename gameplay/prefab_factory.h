#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include <box2d/box2d.h>

#include "components.h"

class AssetManifest;
class Entity;
class EventBus;
class PhysicsWorld;

struct PrefabDefinition
{
    std::string tag;
    std::string textureKey;
    float x = 0.0f;
    float y = 0.0f;
    float width = 64.0f;
    float height = 64.0f;
    float rotation = 0.0f;
    float scale = 1.0f;
    float tintR = 1.0f;
    float tintG = 1.0f;
    float tintB = 1.0f;
    float tintA = 1.0f;
    bool hasRigidBody = false;
    b2BodyType bodyType = b2_staticBody;
    bool fixedRotation = true;
    float gravityScale = 0.0f;
    bool hasCollider = false;
    float colliderDensity = 1.0f;
    float colliderFriction = 0.2f;
    bool colliderSensor = false;
    bool hasPlayerController = false;
    bool hasHealth = false;
    int maxHealth = 3;
    bool hasDamageCooldown = false;
    float damageCooldown = 0.0f;
    bool hasEnemy = false;
    EnemyArchetype enemyArchetype = EnemyArchetype::Floater;
    int enemyContactDamage = 1;
    bool hasEnemyMover = false;
    float enemyOriginX = 0.0f;
    float enemyOriginY = 0.0f;
    float enemyAmplitudeX = 0.0f;
    float enemyAmplitudeY = 0.0f;
    float enemyFrequency = 1.0f;
    bool hasGimmick = false;
    GimmickType gimmickType = GimmickType::Hazard;
    bool gimmickStartsEnabled = true;
    bool gimmickOneShot = false;
    bool hasBarrel = false;
    float barrelGravity = 1900.0f;
    float barrelMaxFallSpeed = 980.0f;
    float barrelRollSpeed = 220.0f;
    float barrelGroundFriction = 720.0f;
    int barrelContactDamage = 1;
    // 3/21í«â¡(ìcîVè„èr)
    float enemyDetectRange = 400.0f;
    float enemyAttackRange =48.0f;
    float enemyAttackCooldown = 3.0f;
    float enemyDetectHeight = 96.0f;
    float barrelBreakMinFallDistance = 99999.0f;
    float barrelBreakMinImpactSpeed = 99999.0f;
    bool hasPhotoFilter = false;
    PhotoFilterTheme filterTheme = PhotoFilterTheme::None;
    PhotoCopyRole filterOutputRole = PhotoCopyRole::Solid;
    PhotoCopyLayer filterOutputLayer = PhotoCopyLayer::Foreground;
    float filterTintR = 1.0f;
    float filterTintG = 1.0f;
    float filterTintB = 1.0f;
    float filterTintA = 1.0f;
};

class PrefabFactory
{
public:
    PrefabFactory(const AssetManifest& manifest, PhysicsWorld& physicsWorld, EventBus& eventBus);

    std::unique_ptr<Entity> Create(const std::string& prefabId) const;

private:
    void LoadDefinitions();
    void LoadBuiltInDefaults();

    const AssetManifest& m_manifest;
    EventBus& m_eventBus;
    PhysicsWorld& m_physicsWorld;
    std::unordered_map<std::string, PrefabDefinition> m_definitions;
};
