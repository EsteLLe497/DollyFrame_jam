#include "components.h"

#include <algorithm>

#include "audio.h"
#include "directX.h"
#include "entity.h"
#include "event_bus.h"
#include "imgui.h"
#include "input.h"
#include "physics_world.h"
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

TagComponent::TagComponent(const char* value)
    : tag(value ? value : "")
{
}

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

PhotoCopyLifetimeComponent::PhotoCopyLifetimeComponent(float lifetimeSeconds)
    : m_lifetimeSeconds(std::max(0.0f, lifetimeSeconds))
    , m_remainingSeconds(std::max(0.0f, lifetimeSeconds))
{
}

void PhotoCopyLifetimeComponent::Update(float deltaTime)
{
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

PhotoCopyOriginComponent::PhotoCopyOriginComponent(PhotoCopyOrigin originValue)
    : origin(originValue)
{
}

PhotoCopyTileValueComponent::PhotoCopyTileValueComponent(int tileValueValue)
    : tileValue(tileValueValue)
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
        case EnemyArchetype::Ranged: // 3/19’Ç‰Á(“c”Vãr)
            return "Ranged";
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
}

void EnemyComponent::Restore()
{
    m_defeated = false;
    m_enabled = true;
}

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

float DamageCooldownComponent::GetRemainingSeconds() const
{
    return m_remainingSeconds;
}

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
