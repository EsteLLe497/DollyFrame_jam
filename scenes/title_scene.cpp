#include "title_scene.h"

#include "directX.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include <tracy/Tracy.hpp>

TitleScene::TitleScene()
    : m_whiteTexture(-1)
    , m_titleTexture(-1)
    , m_ringTexture(-1)
    , m_burstTexture(-1)
    , m_windTexture(-1)
    , m_cloudTexture(-1)
    , m_laserTexture(-1)
    , m_blinkTimer(0.0f)
    , m_sceneTime(0.0f)
    , m_showPrompt(true)
{
}

const char* TitleScene::GetSceneId() const
{
    return "title";
}

void TitleScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_titleTexture = resources.LoadTexture(L"assets\\texture\\タイトル.png");
    m_ringTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Ring01.png");
    m_burstTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Burst01.png");
    m_windTexture = resources.LoadTexture(L"assets\\effects\\Texture\\wind02.png");
    m_cloudTexture = resources.LoadTexture(L"assets\\effects\\Texture\\Cloud01.png");
    m_laserTexture = resources.LoadTexture(L"assets\\effects\\Texture\\LaserMain01.png");
    m_eventBus.Clear();
    m_blinkTimer = 0.0f;
    m_sceneTime = 0.0f;
    m_showPrompt = true;
    Logger::Info("TitleScene entered");
}

void TitleScene::Update(float deltaTime)
{
    ZoneScoped;
    m_eventBus.Clear();
    m_blinkTimer += deltaTime;
    m_sceneTime += deltaTime;
    if (m_blinkTimer >= 0.5f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    if (Input_IsActionPressed(InputAction::StartGame))
    {
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "scene_change", 0.0f, 0.0f });
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }

    if (Input_IsActionPressed(InputAction::OpenDemoScene))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "demo", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::OpenShaderShowcase))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "shader_showcase", 0.0f, 0.0f });
    }
}

void TitleScene::Draw()
{
    DrawBackdrop();
}

void TitleScene::DrawDebugUI()
{
    ImGui::Begin("Title Scene");
    ImGui::Text("DirectX Game Foundation");
    ImGui::Text("Press Enter / Space / Gamepad A to start");
    ImGui::Text("Press D to open the sandbox demo scene");
    ImGui::Text("Press S to open the shader showcase scene");
    ImGui::Text("Prompt Visible: %s", m_showPrompt ? "Yes" : "No");
    ImGui::End();
}

EventBus* TitleScene::GetEventBus()
{
    return &m_eventBus;
}

void TitleScene::DrawBackdrop() const
{
    const float screenWidth = static_cast<float>(SCREEN_WIDTH);
    const float screenHeight = static_cast<float>(SCREEN_HEIGHT);
    const float pulse = 0.55f + 0.45f * std::sinf(m_sceneTime * 2.4f);
    const float promptPulse = 0.5f + 0.5f * std::sinf(m_sceneTime * 4.8f);

    Shader_ResetStyle();
    Shader_SetTint(0.04f, 0.06f, 0.10f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetGradientMap(0.05f, 0.08f, 0.16f, 1.0f, 0.16f, 0.28f, 0.44f, 1.0f, 1.3f);
    Shader_SetTint(0.92f, 0.92f, 1.0f, 1.0f);
    SpriteDraw(m_cloudTexture, -60.0f, -40.0f, screenWidth + 120.0f, screenHeight * 0.65f, 0.0f, 0.0f, 2.8f, 1.4f, 0.0f);

    Shader_SetParallax(0.03f, 0.11f, 0.58f, m_sceneTime * 0.6f);
    Shader_SetTint(0.38f, 0.55f, 0.80f, 0.34f);
    SpriteDraw(m_cloudTexture, 64.0f, 112.0f, screenWidth - 128.0f, 170.0f, 0.0f, 0.0f, 3.0f, 1.1f, 0.0f);

    Shader_SetDistortion(0.012f, 0.010f, m_sceneTime * 0.8f, 0.38f);
    Shader_SetTint(0.28f, 0.62f, 0.96f, 0.34f);
    SpriteDraw(m_windTexture, 96.0f, 86.0f, screenWidth - 192.0f, 198.0f, 0.0f, 0.0f, 3.2f, 1.0f, 0.0f);

    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.20f, 0.68f, 1.0f, 0.18f + pulse * 0.10f);
    SpriteDraw(m_laserTexture, 84.0f, 118.0f, screenWidth - 168.0f, 26.0f, 0.0f, 0.0f, 4.2f, 1.0f, 0.0f);
    Shader_SetTint(0.98f, 0.42f, 0.18f, 0.26f + pulse * 0.12f);
    SpriteDraw(m_laserTexture, 84.0f, 150.0f, screenWidth - 168.0f, 12.0f, 0.0f, 0.0f, 3.6f, 1.0f, 0.0f);
    Shader_SetTint(0.24f, 0.88f, 0.74f, 0.20f + pulse * 0.10f);
    SpriteDraw(m_laserTexture, 84.0f, 612.0f, screenWidth - 168.0f, 10.0f, 0.0f, 0.0f, 3.6f, 1.0f, 0.0f);

    Shader_ResetStyle();
    Shader_SetTint(0.10f, 0.16f, 0.26f, 0.94f);
    SpriteDraw(m_whiteTexture, 84.0f, 98.0f, screenWidth - 168.0f, 520.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.14f, 0.22f, 0.34f, 0.98f);
    SpriteDraw(m_whiteTexture, 106.0f, 122.0f, screenWidth - 212.0f, 134.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.11f, 0.17f, 0.28f, 0.98f);
    SpriteDraw(m_whiteTexture, 106.0f, 276.0f, screenWidth - 212.0f, 238.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.13f, 0.20f, 0.30f, 0.98f);
    SpriteDraw(m_whiteTexture, 106.0f, 534.0f, screenWidth - 212.0f, 58.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.18f, 0.64f, 1.0f, 0.26f);
    SpriteDraw(m_burstTexture, 160.0f, 72.0f, 240.0f, 240.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_sceneTime * 0.14f);
    SpriteDraw(m_burstTexture, screenWidth - 400.0f, 70.0f, 220.0f, 220.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_sceneTime * 0.12f);

    Shader_SetRimLight(1.0f, 0.84f, 0.26f, 1.0f, 2.0f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 0.96f);
    SpriteDraw(m_titleTexture, 146.0f, 126.0f, 520.0f, 104.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetChromaticAberration(0.009f + pulse * 0.005f, m_sceneTime * 0.65f);
    Shader_SetTint(1.0f, 1.0f, 1.0f, 0.92f);
    SpriteDraw(m_titleTexture, 152.0f, 132.0f, 520.0f, 104.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.26f, 0.72f, 1.0f, 0.28f + pulse * 0.08f);
    SpriteDraw(m_ringTexture, 706.0f, 120.0f, 126.0f, 126.0f, 0.0f, 0.0f, 1.0f, 1.0f, m_sceneTime * 0.55f);

    Shader_ResetStyle();
    Shader_SetTint(0.18f, 0.30f, 0.46f, 1.0f);
    SpriteDraw(m_whiteTexture, 146.0f, 320.0f, 292.0f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.95f, 0.44f, 0.18f, 1.0f);
    SpriteDraw(m_whiteTexture, 146.0f, 320.0f, 152.0f + pulse * 88.0f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.18f, 0.26f, 0.38f, 1.0f);
    SpriteDraw(m_whiteTexture, 146.0f, 372.0f, 666.0f, 62.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.14f, 0.22f, 0.32f, 1.0f);
    SpriteDraw(m_whiteTexture, 146.0f, 448.0f, 666.0f, 62.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetBlendMode(ShaderBlendMode2D::Additive);
    Shader_SetTint(0.22f, 0.76f, 1.0f, 0.26f + promptPulse * 0.10f);
    SpriteDraw(m_laserTexture, 168.0f, 392.0f, 622.0f, 14.0f, 0.0f, 0.0f, 4.0f, 1.0f, 0.0f);
    Shader_SetTint(0.94f, 0.82f, 0.24f, m_showPrompt ? 0.34f + promptPulse * 0.24f : 0.10f);
    SpriteDraw(m_burstTexture, 650.0f, 340.0f, 164.0f, 164.0f, 0.0f, 0.0f, 1.0f, 1.0f, -m_sceneTime * 0.28f);

    Shader_ResetStyle();
    Shader_SetTint(0.10f, 0.14f, 0.22f, 1.0f);
    SpriteDraw(m_whiteTexture, 164.0f, 548.0f, 220.0f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, 404.0f, 548.0f, 220.0f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, 644.0f, 548.0f, 148.0f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.92f, 0.50f, 0.20f, 1.0f);
    SpriteDraw(m_whiteTexture, 164.0f, 548.0f, 220.0f * (0.62f + 0.18f * pulse), 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.26f, 0.76f, 1.0f, 1.0f);
    SpriteDraw(m_whiteTexture, 404.0f, 548.0f, 220.0f * (0.52f + 0.22f * promptPulse), 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    Shader_SetTint(0.18f, 0.92f, 0.58f, 1.0f);
    SpriteDraw(m_whiteTexture, 644.0f, 548.0f, 148.0f * 0.54f, 18.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
    Shader_ResetStyle();
}
