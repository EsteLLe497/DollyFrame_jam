#include "pch.h"

#include "demo_scene.h"

#include <cstring>

#include "collision.h"
#include "components.h"
#include "directX.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "prefab_factory.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include <tracy/Tracy.hpp>

namespace
{
    constexpr float kSpriteBaseSize = 256.0f;
}

DemoScene::DemoScene()
    : m_whiteTexture(-1)
    , m_playerTouchingTarget(false)
{
}

const char* DemoScene::GetSceneId() const
{
    return "demo";
}

void DemoScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_entities.clear();
    m_playerTouchingTarget = false;
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_eventBus.Clear();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
    m_scriptEngine.Initialize();
    m_scriptEngine.BindEventBus(m_eventBus);
    m_scriptEngine.LoadFile("assets/demo_scene.lua");

    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
    if (auto player = prefabs.Create("player"))
    {
        m_entities.push_back(std::move(player));
    }
    if (auto goal = prefabs.Create("goal"))
    {
        m_entities.push_back(std::move(goal));
    }
    if (auto star = prefabs.Create("star_outline"))
    {
        if (auto* transform = star->GetComponent<TransformComponent>())
        {
            transform->x = 700.0f;
            transform->y = 220.0f;
        }
        m_entities.push_back(std::move(star));
    }
    Logger::Info("DemoScene entered");
}

void DemoScene::OnExit()
{
    m_scriptEngine.Shutdown();
    m_entities.clear();
    m_physicsWorld.Shutdown();
}

void DemoScene::Update(float deltaTime)
{
    ZoneScoped;
    for (const auto& entity : m_entities)
    {
        entity->Update(deltaTime);
    }

    m_scriptEngine.CallUpdate(deltaTime);
    if (Input_IsKeyPressed('R'))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "demo", 0.0f, 0.0f });
    }

    if (m_entities.size() >= 2)
    {
        if (auto* transform = m_entities[1]->GetComponent<TransformComponent>())
        {
            transform->x = static_cast<float>(m_scriptEngine.GetNumber("target_x", transform->x));
            transform->y = static_cast<float>(m_scriptEngine.GetNumber("target_y", transform->y));
        }
    }

    for (const auto& entity : m_entities)
    {
        m_physicsWorld.SyncEntityToPhysics(*entity);
    }

    m_physicsWorld.Step(deltaTime);

    for (const auto& entity : m_entities)
    {
        m_physicsWorld.SyncEntityFromPhysics(*entity);
    }

    ProcessEvents();

    if (auto* target = FindEntityByTag("Goal"))
    {
        if (auto* tint = target->GetComponent<TintComponent>())
        {
            tint->r = 1.0f;
            tint->g = m_playerTouchingTarget ? 0.35f : 1.0f;
            tint->b = m_playerTouchingTarget ? 0.35f : 1.0f;
            tint->a = 1.0f;
        }
    }
}

void DemoScene::Draw()
{
    DrawBackdrop();
    for (const auto& entity : m_entities)
    {
        entity->Draw();
    }
}

void DemoScene::DrawDebugUI()
{
    ImGui::Begin("Demo Scene");
    ImGui::Text("Polymorphic Scene + ECS Example");
    ImGui::Text("Entity Count: %d", static_cast<int>(m_entities.size()));
    ImGui::Text("Gamepad Connected: %s", Input_IsGamepadConnected() ? "Yes" : "No");
    ImGui::Text("Press R to reload the scene");
    ImGui::Text("Lua can request sound/scene/log events");
    ImGui::Text("Physics Contact: %s", m_playerTouchingTarget ? "Hit" : "No Hit");
    ImGui::Text("Lua Target: (%.1f, %.1f)",
        static_cast<float>(m_scriptEngine.GetNumber("target_x")),
        static_cast<float>(m_scriptEngine.GetNumber("target_y")));
    ImGui::Text("Events This Frame: %d", static_cast<int>(m_eventBus.GetEvents().size()));
    ImGui::Text("Contact Begin: %d", m_eventBus.Count(EventType::ContactBegin));
    ImGui::Text("Contact End: %d", m_eventBus.Count(EventType::ContactEnd));
    for (const auto& entity : m_entities)
    {
        entity->DrawDebugUI();
    }
    ImGui::End();
}

EventBus* DemoScene::GetEventBus()
{
    return &m_eventBus;
}

Entity* DemoScene::FindEntityByTag(const char* tag) const
{
    for (const auto& entity : m_entities)
    {
        const auto* entityTag = entity->GetComponent<TagComponent>();
        if (entityTag && entityTag->Is(tag))
        {
            return entity.get();
        }
    }
    return nullptr;
}

void DemoScene::ProcessEvents()
{
    const size_t eventCount = m_eventBus.GetEvents().size();
    for (size_t i = 0; i < eventCount; ++i)
    {
        const auto& eventData = m_eventBus.GetEvents()[i];
        if (eventData.type != EventType::ContactBegin && eventData.type != EventType::ContactEnd)
        {
            continue;
        }

        const auto* tagA = eventData.entityA ? eventData.entityA->GetComponent<TagComponent>() : nullptr;
        const auto* tagB = eventData.entityB ? eventData.entityB->GetComponent<TagComponent>() : nullptr;
        if (!tagA || !tagB)
        {
            continue;
        }

        const bool playerTargetPair =
            (tagA->Is(EntityTag::Player) && tagB->Is(EntityTag::Goal)) ||
            (tagA->Is(EntityTag::Goal) && tagB->Is(EntityTag::Player));

        if (!playerTargetPair)
        {
            continue;
        }

        m_playerTouchingTarget = (eventData.type == EventType::ContactBegin);
        if (m_playerTouchingTarget)
        {
            m_eventBus.Publish({ EventType::PlaySoundRequest, eventData.entityA, eventData.entityB, "contact_tone", 0.0f, 0.0f });
            m_eventBus.Publish({ EventType::LogMessage, eventData.entityA, eventData.entityB, "Physics contact started", 0.0f, 0.0f });
        }
        else
        {
            m_eventBus.Publish({ EventType::LogMessage, eventData.entityA, eventData.entityB, "Physics contact ended", 0.0f, 0.0f });
        }
    }
}

void DemoScene::DrawBackdrop() const
{
    Shader_SetTint(0.12f, 0.16f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, 40.0f, 40.0f, static_cast<float>(SCREEN_WIDTH) - 80.0f, static_cast<float>(SCREEN_HEIGHT) - 80.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.18f, 0.28f, 0.38f, 1.0f);
    SpriteDraw(m_whiteTexture, 80.0f, 80.0f, static_cast<float>(SCREEN_WIDTH) - 160.0f, 96.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}
