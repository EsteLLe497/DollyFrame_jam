#include "components.h"

#include <algorithm>

#include "audio.h"
#include "directX.h"
#include "entity.h"
#include "event_bus.h"
#include "imgui.h"
#include "input.h"
#include "physics_world.h"
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
        transform->x,
        transform->y,
        transform->width * transform->scale,
        transform->height * transform->scale,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        transform->rotation);
}

int SpriteRenderComponent::GetTextureId() const
{
    return m_textureId;
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
{
}

void EnemyMoverComponent::Update(float deltaTime)
{
    auto* transform = m_owner ? m_owner->GetComponent<TransformComponent>() : nullptr;
    if (!transform)
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
