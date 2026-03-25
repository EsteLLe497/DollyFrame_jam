#include "prefab_factory.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include "asset_manifest.h"
#include "components.h"
#include "entity.h"
#include "event_bus.h"
#include "logger.h"
#include "physics_world.h"

PrefabFactory::PrefabFactory(const AssetManifest& manifest, PhysicsWorld& physicsWorld, EventBus& eventBus)
    : m_manifest(manifest)
    , m_eventBus(eventBus)
    , m_physicsWorld(physicsWorld)
{
    LoadDefinitions();
}

std::unique_ptr<Entity> PrefabFactory::Create(const std::string& prefabId) const
{
    const auto found = m_definitions.find(prefabId);
    if (found == m_definitions.end())
    {
        return {};
    }

    const PrefabDefinition& definition = found->second;
    auto entity = std::make_unique<Entity>();
    entity->AddComponent<TagComponent>(definition.tag.c_str());

    auto& transform = entity->AddComponent<TransformComponent>(
        definition.x,
        definition.y,
        definition.width,
        definition.height);
    transform.rotation = definition.rotation;
    transform.scale = definition.scale;

    entity->AddComponent<TintComponent>(definition.tintR, definition.tintG, definition.tintB, definition.tintA);

    if (definition.hasRigidBody)
    {
        entity->AddComponent<RigidBodyComponent>(
            m_physicsWorld,
            definition.bodyType,
            definition.fixedRotation,
            definition.gravityScale);
    }

    if (definition.hasCollider)
    {
        entity->AddComponent<BoxColliderComponent>(
            definition.colliderDensity,
            definition.colliderFriction,
            definition.colliderSensor);
    }

    if (definition.hasImageOutlineCollider)
    {
        entity->AddComponent<ImageOutlineColliderComponent>(
            definition.colliderImagePath,
            definition.colliderFriction,
            definition.colliderAlphaThreshold,
            definition.colliderVertexStride);
    }

    if (definition.hasPlayerController)
    {
        entity->AddComponent<PlayerControllerComponent>(m_eventBus);
    }

    if (definition.hasHealth)
    {
        entity->AddComponent<HealthComponent>(definition.maxHealth);
    }

    if (definition.hasDamageCooldown)
    {
        entity->AddComponent<DamageCooldownComponent>(definition.damageCooldown);
    }

    if (definition.hasEnemy)
    {
        auto& enemyComp = entity->AddComponent<EnemyComponent>(definition.enemyArchetype, definition.enemyContactDamage);
        // 3/21追加(田之上俊)
        enemyComp.detectRange = definition.enemyDetectRange;
        enemyComp.attackRange = definition.enemyAttackRange;
        enemyComp.attackCooldown = definition.enemyAttackCooldown;
        enemyComp.detectHeight = definition.enemyDetectHeight;
    }

    if (definition.hasEnemyMover)
    {
        entity->AddComponent<EnemyMoverComponent>(
            definition.enemyOriginX,
            definition.enemyOriginY,
            definition.enemyAmplitudeX,
            definition.enemyAmplitudeY,
            definition.enemyFrequency);
    }

    if (definition.hasGimmick)
    {
        entity->AddComponent<GimmickComponent>(
            definition.gimmickType,
            definition.gimmickStartsEnabled,
            definition.gimmickOneShot);
    }

    if (definition.hasBarrel)
    {
        entity->AddComponent<BarrelComponent>(
            definition.barrelGravity,
            definition.barrelMaxFallSpeed,
            definition.barrelRollSpeed,
            definition.barrelGroundFriction,
            definition.barrelContactDamage,
            definition.barrelBreakMinFallDistance,
            definition.barrelBreakMinImpactSpeed);
    }

    if (definition.hasPhotoFilter)
    {
        entity->AddComponent<PhotoFilterComponent>(
            definition.filterTheme,
            definition.filterOutputRole,
            definition.filterOutputLayer,
            definition.filterTintR,
            definition.filterTintG,
            definition.filterTintB,
            definition.filterTintA);
    }

    entity->AddComponent<SpriteRenderComponent>(m_manifest.GetTexture(definition.textureKey));
    return entity;
}

void PrefabFactory::LoadDefinitions()
{
    m_definitions.clear();

    std::ifstream ifs("assets/prefabs.json");
    if (!ifs)
    {
        Logger::Warn("assets/prefabs.json not found. Falling back to built-in prefab defaults.");
        LoadBuiltInDefaults();
        return;
    }

    nlohmann::json root;
    ifs >> root;

    const auto& prefabs = root["prefabs"];
    if (!prefabs.is_object())
    {
        Logger::Warn("assets/prefabs.json is invalid. Falling back to built-in prefab defaults.");
        LoadBuiltInDefaults();
        return;
    }

    for (auto it = prefabs.begin(); it != prefabs.end(); ++it)
    {
        const auto& data = it.value();
        PrefabDefinition definition;
        definition.tag = data.value("tag", it.key());
        definition.textureKey = data.value("texture", "white");

        const auto& transform = data.contains("transform") && data["transform"].is_object() ? data["transform"] : nlohmann::json::object();
        definition.x = transform.value("x", 0.0f);
        definition.y = transform.value("y", 0.0f);
        definition.width = transform.value("width", 64.0f);
        definition.height = transform.value("height", 64.0f);
        definition.rotation = transform.value("rotation", 0.0f);
        definition.scale = transform.value("scale", 1.0f);

        const auto& tint = data.contains("tint") && data["tint"].is_object() ? data["tint"] : nlohmann::json::object();
        definition.tintR = tint.value("r", 1.0f);
        definition.tintG = tint.value("g", 1.0f);
        definition.tintB = tint.value("b", 1.0f);
        definition.tintA = tint.value("a", 1.0f);

        const auto& rigidBody = data.contains("rigidBody") && data["rigidBody"].is_object() ? data["rigidBody"] : nlohmann::json::object();
        definition.hasRigidBody = rigidBody.value("enabled", false);
        const std::string bodyType = rigidBody.value("type", "static");
        if (bodyType == "dynamic")
        {
            definition.bodyType = b2_dynamicBody;
        }
        else if (bodyType == "kinematic")
        {
            definition.bodyType = b2_kinematicBody;
        }
        else
        {
            definition.bodyType = b2_staticBody;
        }
        definition.fixedRotation = rigidBody.value("fixedRotation", true);
        definition.gravityScale = rigidBody.value("gravityScale", 0.0f);

        const auto& collider = data.contains("collider") && data["collider"].is_object() ? data["collider"] : nlohmann::json::object();
        definition.hasCollider = collider.value("enabled", false);
        definition.colliderDensity = collider.value("density", 1.0f);
        definition.colliderFriction = collider.value("friction", 0.2f);
        definition.colliderSensor = collider.value("isSensor", false);
        definition.hasImageOutlineCollider = collider.value("imageEnabled", false);
        definition.colliderImagePath = collider.value("imagePath", "");
        definition.colliderAlphaThreshold = collider.value("alphaThreshold", 16);
        definition.colliderVertexStride = collider.value("vertexStride", 4);

        const auto& controller = data.contains("controller") && data["controller"].is_object() ? data["controller"] : nlohmann::json::object();
        definition.hasPlayerController = controller.value("player", false);

        const auto& health = data.contains("health") && data["health"].is_object() ? data["health"] : nlohmann::json::object();
        definition.hasHealth = health.value("enabled", false);
        definition.maxHealth = health.value("maxHp", 3);

        const auto& damageCooldown = data.contains("damageCooldown") && data["damageCooldown"].is_object() ? data["damageCooldown"] : nlohmann::json::object();
        definition.hasDamageCooldown = damageCooldown.value("enabled", false);
        definition.damageCooldown = damageCooldown.value("seconds", 0.0f);

        const auto& enemy = data.contains("enemy") && data["enemy"].is_object() ? data["enemy"] : nlohmann::json::object();
        definition.hasEnemy = enemy.value("enabled", false);
        const std::string enemyArchetype = enemy.value("archetype", "floater");
        if (enemyArchetype == "walker")
        {
            definition.enemyArchetype = EnemyArchetype::Walker;
        }
        else if (enemyArchetype == "turret")
        {
            definition.enemyArchetype = EnemyArchetype::Turret;
        }
        else if (enemyArchetype == "ranged") // 3/19追加(田之上俊)
        {
            definition.enemyArchetype = EnemyArchetype::Ranged;
        }
        else
        {
            definition.enemyArchetype = EnemyArchetype::Floater;
        }
        definition.enemyContactDamage = enemy.value("contactDamage", 1);

        // 3/21追加(田之上俊)
        definition.enemyDetectRange = enemy.value("detectRange", 400.0f);
        definition.enemyAttackRange = enemy.value("attackRange", 80.0f);
        definition.enemyAttackCooldown = enemy.value("attackCooldown", 3.0f);
        definition.enemyDetectHeight = enemy.value("detectHeight", 96.0f);

        const auto& enemyMover = data.contains("enemyMover") && data["enemyMover"].is_object() ? data["enemyMover"] : nlohmann::json::object();
        definition.hasEnemyMover = enemyMover.value("enabled", false);
        definition.enemyOriginX = enemyMover.value("originX", definition.x);
        definition.enemyOriginY = enemyMover.value("originY", definition.y);
        definition.enemyAmplitudeX = enemyMover.value("amplitudeX", 0.0f);
        definition.enemyAmplitudeY = enemyMover.value("amplitudeY", 0.0f);
        definition.enemyFrequency = enemyMover.value("frequency", 1.0f);

        const auto& gimmick = data.contains("gimmick") && data["gimmick"].is_object() ? data["gimmick"] : nlohmann::json::object();
        definition.hasGimmick = gimmick.value("enabled", false);
        const std::string gimmickType = gimmick.value("type", "hazard");
        if (gimmickType == "goal")
        {
            definition.gimmickType = GimmickType::Goal;
        }
        else if (gimmickType == "pickup")
        {
            definition.gimmickType = GimmickType::Pickup;
        }
        else if (gimmickType == "checkpoint")
        {
            definition.gimmickType = GimmickType::Checkpoint;
        }
        else if (gimmickType == "photo_source")
        {
            definition.gimmickType = GimmickType::PhotoSource;
        }
        else if (gimmickType == "filter")
        {
            definition.gimmickType = GimmickType::Filter;
        }
        else if (gimmickType == "gate")
        {
            definition.gimmickType = GimmickType::Gate;
        }
        else if (gimmickType == "switch")
        {
            definition.gimmickType = GimmickType::Switch;
        }
        else
        {
            definition.gimmickType = GimmickType::Hazard;
        }
        definition.gimmickStartsEnabled = gimmick.value("startsEnabled", true);
        definition.gimmickOneShot = gimmick.value("oneShot", false);

        const auto& barrel = data.contains("barrel") && data["barrel"].is_object() ? data["barrel"] : nlohmann::json::object();
        definition.hasBarrel = barrel.value("enabled", false);
        definition.barrelGravity = barrel.value("gravity", definition.barrelGravity);
        definition.barrelMaxFallSpeed = barrel.value("maxFallSpeed", definition.barrelMaxFallSpeed);
        definition.barrelRollSpeed = barrel.value("rollSpeed", definition.barrelRollSpeed);
        definition.barrelGroundFriction = barrel.value("groundFriction", definition.barrelGroundFriction);
        definition.barrelContactDamage = barrel.value("contactDamage", definition.barrelContactDamage);
        definition.barrelBreakMinFallDistance = barrel.value("breakMinFallDistance", definition.barrelBreakMinFallDistance);
        definition.barrelBreakMinImpactSpeed = barrel.value("breakMinImpactSpeed", definition.barrelBreakMinImpactSpeed);

        const auto& filter = data.contains("photoFilter") && data["photoFilter"].is_object() ? data["photoFilter"] : nlohmann::json::object();
        definition.hasPhotoFilter = filter.value("enabled", false);
        const std::string filterTheme = filter.value("theme", "none");
        if (filterTheme == "hot")
        {
            definition.filterTheme = PhotoFilterTheme::Hot;
        }
        else if (filterTheme == "cold")
        {
            definition.filterTheme = PhotoFilterTheme::Cold;
        }
        else if (filterTheme == "invert")
        {
            definition.filterTheme = PhotoFilterTheme::Invert;
        }
        else
        {
            definition.filterTheme = PhotoFilterTheme::None;
        }
        const std::string outputRole = filter.value("outputRole", "solid");
        if (outputRole == "hazard")
        {
            definition.filterOutputRole = PhotoCopyRole::Hazard;
        }
        else if (outputRole == "goal")
        {
            definition.filterOutputRole = PhotoCopyRole::GoalRelay;
        }
        else if (outputRole == "pickup")
        {
            definition.filterOutputRole = PhotoCopyRole::Pickup;
        }
        else if (outputRole == "ally")
        {
            definition.filterOutputRole = PhotoCopyRole::Ally;
        }
        else
        {
            definition.filterOutputRole = PhotoCopyRole::Solid;
        }

        const std::string outputLayer = filter.value("outputLayer", "foreground");
        if (outputLayer == "background")
        {
            definition.filterOutputLayer = PhotoCopyLayer::Background;
        }
        else if (outputLayer == "shadow")
        {
            definition.filterOutputLayer = PhotoCopyLayer::Shadow;
        }
        else
        {
            definition.filterOutputLayer = PhotoCopyLayer::Foreground;
        }

        const auto& filterTint = filter.contains("tint") && filter["tint"].is_object() ? filter["tint"] : nlohmann::json::object();
        definition.filterTintR = filterTint.value("r", 1.0f);
        definition.filterTintG = filterTint.value("g", 1.0f);
        definition.filterTintB = filterTint.value("b", 1.0f);
        definition.filterTintA = filterTint.value("a", 1.0f);

        m_definitions.emplace(it.key(), std::move(definition));
    }

    Logger::Info("Prefab definitions loaded from JSON");
}

void PrefabFactory::LoadBuiltInDefaults()
{
    PrefabDefinition player;
    player.tag = "Player";
    player.textureKey = "player";
    player.x = 400.0f;
    player.y = 220.0f;
    player.width = 256.0f;
    player.height = 256.0f;
    player.hasRigidBody = true;
    player.bodyType = b2_dynamicBody;
    player.fixedRotation = true;
    player.gravityScale = 0.0f;
    player.hasCollider = true;
    player.colliderDensity = 1.0f;
    player.colliderFriction = 0.2f;
    player.hasPlayerController = true;
    player.hasHealth = true;
    player.maxHealth = 3;
    player.hasDamageCooldown = true;
    player.damageCooldown = 1.0f;
    m_definitions.emplace("player", std::move(player));

    PrefabDefinition goal;
    goal.tag = "Goal";
    goal.textureKey = "target";
    goal.x = 980.0f;
    goal.y = 360.0f;
    goal.width = 192.0f;
    goal.height = 192.0f;
    goal.hasRigidBody = true;
    goal.bodyType = b2_kinematicBody;
    goal.fixedRotation = true;
    goal.gravityScale = 0.0f;
    goal.hasCollider = true;
    goal.colliderDensity = 1.0f;
    goal.colliderFriction = 0.2f;
    goal.hasGimmick = true;
    goal.gimmickType = GimmickType::Goal;
    m_definitions.emplace("goal", std::move(goal));

    PrefabDefinition hazard;
    hazard.tag = "Hazard";
    hazard.textureKey = "hazard";
    hazard.x = 620.0f;
    hazard.y = 500.0f;
    hazard.width = 240.0f;
    hazard.height = 48.0f;
    hazard.hasRigidBody = true;
    hazard.bodyType = b2_staticBody;
    hazard.fixedRotation = true;
    hazard.gravityScale = 0.0f;
    hazard.hasCollider = true;
    hazard.colliderDensity = 1.0f;
    hazard.colliderFriction = 0.2f;
    hazard.colliderSensor = true;
    hazard.hasGimmick = true;
    hazard.gimmickType = GimmickType::Hazard;
    m_definitions.emplace("hazard", std::move(hazard));

    PrefabDefinition filter;
    filter.tag = "Filter";
    filter.textureKey = "white";
    filter.x = 1100.0f;
    filter.y = 300.0f;
    filter.width = 112.0f;
    filter.height = 112.0f;
    filter.tintR = 0.16f;
    filter.tintG = 0.78f;
    filter.tintB = 0.86f;
    filter.tintA = 0.96f;
    filter.hasGimmick = true;
    filter.gimmickType = GimmickType::Filter;
    filter.hasPhotoFilter = true;
    filter.filterTheme = PhotoFilterTheme::Cold;
    filter.filterOutputRole = PhotoCopyRole::Pickup;
    filter.filterOutputLayer = PhotoCopyLayer::Foreground;
    filter.filterTintR = 0.20f;
    filter.filterTintG = 0.88f;
    filter.filterTintB = 0.92f;
    filter.filterTintA = 1.0f;
    m_definitions.emplace("photo_filter_pickup", std::move(filter));

    PrefabDefinition enemy;
    enemy.tag = "Enemy";
    enemy.textureKey = "enemy";
    enemy.x = 760.0f;
    enemy.y = 180.0f;
    enemy.width = 144.0f;
    enemy.height = 144.0f;
    enemy.hasRigidBody = true;
    enemy.bodyType = b2_kinematicBody;
    enemy.fixedRotation = true;
    enemy.gravityScale = 0.0f;
    enemy.hasCollider = true;
    enemy.colliderDensity = 1.0f;
    enemy.colliderFriction = 0.2f;
    enemy.colliderSensor = true;
    enemy.hasEnemy = true;
    enemy.enemyArchetype = EnemyArchetype::Floater;
    enemy.enemyContactDamage = 1;
    enemy.hasEnemyMover = true;
    enemy.enemyOriginX = enemy.x;
    enemy.enemyOriginY = enemy.y;
    enemy.enemyAmplitudeX = 180.0f;
    enemy.enemyAmplitudeY = 70.0f;
    enemy.enemyFrequency = 1.6f;
    m_definitions.emplace("enemy", std::move(enemy));
}
