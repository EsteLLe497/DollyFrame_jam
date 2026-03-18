#include "application.h"

#include <algorithm>
#include <memory>
#include <string>

#include <tracy/Tracy.hpp>

#include "DxLib.h"
#include "audio.h"
#include "demo_scene.h"
#include "directX.h"
#include "event_bus.h"
#include "game_scene.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "result_scene.h"
#include "scene_manager.h"
#include "scene_registry.h"
#include "shader.h"
#include "shader_showcase_scene.h"
#include "sprite.h"
#include "title_scene.h"

namespace
{
    constexpr int TARGET_FPS = 60;
    constexpr float SCENE_TRANSITION_DURATION = 0.48f;
    constexpr float SCENE_TRANSITION_SWAP_TIME = 0.24f;
}

Application::Application()
    : m_hWnd(nullptr)
    , m_running(true)
    , m_initialized(false)
    , m_exitConfirmationOpen(false)
    , m_sceneTransitionActive(false)
    , m_sceneTransitionSwapped(false)
    , m_currentFps(0.0f)
    , m_sceneTransitionTimer(0.0f)
    , m_sceneTransitionDuration(SCENE_TRANSITION_DURATION)
    , m_frameCount(0)
    , m_fpsTick(0)
    , m_pendingSceneId()
    , m_resources(std::make_unique<ResourceManager>())
    , m_sceneManager(std::make_unique<SceneManager>())
    , m_sceneRegistry(std::make_unique<SceneRegistry>())
{
}

Application::~Application()
{
    Shutdown();
}

int Application::Run(HINSTANCE instance, int nCmdShow)
{
    ZoneScoped;
    if (!Initialize(instance, nCmdShow))
    {
        return 1;
    }

    const int frameMs = 1000 / TARGET_FPS;
    int lastTick = GetNowCount();
    m_fpsTick = static_cast<DWORD>(lastTick);

    while (m_running && ProcessMessage() == 0)
    {
        FrameMark;

        const int now = GetNowCount();
        if ((now - static_cast<int>(m_fpsTick)) >= 1000)
        {
            m_currentFps = static_cast<float>(m_frameCount);
            m_frameCount = 0;
            m_fpsTick = static_cast<DWORD>(now);
        }

        if ((now - lastTick) < frameMs)
        {
            WaitTimer(1);
            continue;
        }

        const float deltaTime = static_cast<float>(now - lastTick) / 1000.0f;
        lastTick = now;

        Update(deltaTime);
        Draw();
        ++m_frameCount;
    }

    return 0;
}

bool Application::Initialize(HINSTANCE instance, int nCmdShow)
{
    static_cast<void>(instance);
    static_cast<void>(nCmdShow);

    ZoneScoped;
    Logger::Initialize();
    Logger::Info("Application initialization started");

    SetOutApplicationLogValidFlag(FALSE);
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
    ChangeWindowMode(FALSE);
    SetGraphMode(SCREEN_WIDTH, SCREEN_HEIGHT, 32, 60);
    SetAlwaysRunFlag(TRUE);
    if (DxLib_Init() == -1)
    {
        return false;
    }

    DirectXInitialize(nullptr);
    if (!Shader_Initialize(nullptr, nullptr))
    {
        return false;
    }

    m_resources->Initialize(nullptr);
    SpriteInitialize();
    if (!InitializeMiddleware())
    {
        return false;
    }

    Audio_LoadCueFromFile("shutter", "assets/effects/Sound/shutter.wav");

    m_sceneRegistry->Register("title", []()
        {
            return std::make_unique<TitleScene>();
        });
    m_sceneRegistry->Register("game", []()
        {
            return std::make_unique<GameScene>();
        });
    m_sceneRegistry->Register("result", []()
        {
            return std::make_unique<ResultScene>();
        });
    m_sceneRegistry->Register("demo", []()
        {
            return std::make_unique<DemoScene>();
        });
    m_sceneRegistry->Register("shader_showcase", []()
        {
            return std::make_unique<ShaderShowcaseScene>();
        });

    m_sceneManager->SetScene(m_sceneRegistry->Create("title"), *m_resources);
    m_initialized = true;
    Logger::Info("Application initialization completed");
    return true;
}

void Application::Shutdown()
{
    ZoneScoped;
    if (!m_initialized)
    {
        return;
    }

    m_sceneManager->Shutdown();
    Audio_Shutdown();
    SpriteFinalize();
    m_resources->Shutdown();
    Shader_Finalize();
    DirectXFinalaize();
    DxLib_End();
    m_initialized = false;
    Logger::Shutdown();
}

void Application::Update(float deltaTime)
{
    ZoneScoped;
    Input_Update();
    Audio_Update();

    const bool escapePressed = Input_IsKeyPressed(VK_ESCAPE);
    if (escapePressed)
    {
        m_exitConfirmationOpen = !m_exitConfirmationOpen;
    }

    if (m_exitConfirmationOpen)
    {
        if (Input_IsKeyPressed(VK_RETURN) || Input_IsKeyPressed('Y'))
        {
            m_running = false;
        }
        else if (Input_IsKeyPressed('N') || (!escapePressed && Input_IsKeyPressed(VK_ESCAPE)))
        {
            m_exitConfirmationOpen = false;
        }
        return;
    }

    if (m_sceneTransitionActive)
    {
        UpdateSceneTransition(deltaTime);
        return;
    }

    m_sceneManager->Update(deltaTime);
    ProcessSceneEvents();
}

void Application::Draw()
{
    ZoneScoped;
    Clear();
    m_sceneManager->Draw();
    if (m_sceneTransitionActive)
    {
        DrawSceneTransition();
    }
    if (m_exitConfirmationOpen)
    {
        DrawExitConfirmation();
    }
    Present();
}

void Application::DrawExitConfirmation() const
{
    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const int panelWidth = 520;
    const int panelHeight = 180;
    const int left = centerX - panelWidth / 2;
    const int top = centerY - panelHeight / 2;
    const int right = left + panelWidth;
    const int bottom = top + panelHeight;

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 160);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);

    DrawBox(left, top, right, bottom, GetColor(22, 26, 32), TRUE);
    DrawBox(left, top, right, bottom, GetColor(220, 230, 255), FALSE);

    DrawString(centerX - 96, top + 36, "Exit the game?", GetColor(255, 255, 255));
    DrawString(centerX - 178, top + 82, "Press Enter or Y to quit", GetColor(255, 220, 220));
    DrawString(centerX - 184, top + 112, "Press Escape or N to continue", GetColor(220, 255, 220));
}

void Application::DrawSceneTransition() const
{
    const float halfDuration = m_sceneTransitionDuration * 0.5f;
    const float clampedTimer = std::clamp(m_sceneTransitionTimer, 0.0f, m_sceneTransitionDuration);
    float alpha = 0.0f;
    if (clampedTimer <= halfDuration)
    {
        alpha = clampedTimer / std::max(0.001f, halfDuration);
    }
    else
    {
        alpha = 1.0f - ((clampedTimer - halfDuration) / std::max(0.001f, halfDuration));
    }

    const int drawAlpha = static_cast<int>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, drawAlpha);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(4, 8, 14), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);

    const int centerX = SCREEN_WIDTH / 2;
    const int centerY = SCREEN_HEIGHT / 2;
    const int lineWidth = 220;
    const int lineHeight = 6;
    const int glowAlpha = static_cast<int>(std::clamp(alpha * 0.85f, 0.0f, 1.0f) * 255.0f);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, glowAlpha);
    DrawBox(centerX - lineWidth / 2, centerY - 16, centerX + lineWidth / 2, centerY - 16 + lineHeight, GetColor(80, 180, 255), TRUE);
    DrawBox(centerX - lineWidth / 3, centerY + 10, centerX + lineWidth / 3, centerY + 10 + lineHeight, GetColor(255, 150, 64), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

bool Application::InitializeMiddleware()
{
    Input_Initialize();
    return Audio_Initialize();
}

void Application::ProcessSceneEvents()
{
    Scene* currentScene = m_sceneManager->GetCurrentScene();
    if (!currentScene)
    {
        return;
    }

    EventBus* eventBus = currentScene->GetEventBus();
    if (!eventBus)
    {
        return;
    }

    std::string requestedSceneId;
    for (const auto& eventData : eventBus->GetEvents())
    {
        switch (eventData.type)
        {
        case EventType::PlaySoundRequest:
            Audio_PlayCue(eventData.name.c_str());
            break;

        case EventType::LogMessage:
            Logger::Info(eventData.name);
            break;

        case EventType::SceneChangeRequested:
            requestedSceneId = eventData.name;
            break;

        case EventType::ContactBegin:
        case EventType::ContactEnd:
        default:
            break;
        }
    }

    if (!requestedSceneId.empty())
    {
        RequestSceneChange(requestedSceneId);
    }
}

void Application::UpdateSceneTransition(float deltaTime)
{
    m_sceneTransitionTimer += deltaTime;
    if (!m_sceneTransitionSwapped && m_sceneTransitionTimer >= SCENE_TRANSITION_SWAP_TIME)
    {
        m_sceneTransitionSwapped = true;

        std::unique_ptr<Scene> nextScene = m_sceneRegistry->Create(m_pendingSceneId);
        if (!nextScene)
        {
            Logger::Warn("Unknown scene change request: " + m_pendingSceneId);
            m_sceneTransitionActive = false;
            m_pendingSceneId.clear();
            return;
        }

        Logger::Info("Changing scene to " + m_pendingSceneId);
        Audio_PlayCue("scene_change");
        m_sceneManager->SetScene(std::move(nextScene), *m_resources);
    }

    if (m_sceneTransitionTimer >= m_sceneTransitionDuration)
    {
        m_sceneTransitionActive = false;
        m_sceneTransitionSwapped = false;
        m_sceneTransitionTimer = 0.0f;
        m_pendingSceneId.clear();
    }
}

bool Application::RequestSceneChange(const std::string& sceneId)
{
    if (sceneId.empty())
    {
        return false;
    }
    if (m_sceneTransitionActive)
    {
        return false;
    }

    m_pendingSceneId = sceneId;
    m_sceneTransitionActive = true;
    m_sceneTransitionSwapped = false;
    m_sceneTransitionTimer = 0.0f;
    return true;
}

void Application::UpdateWindowTitle()
{
}

HWND Application::CreateAppWindow(HINSTANCE instance, int nCmdShow)
{
    static_cast<void>(instance);
    static_cast<void>(nCmdShow);
    return nullptr;
}

LRESULT Application::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK Application::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, message, wParam, lParam);
}
