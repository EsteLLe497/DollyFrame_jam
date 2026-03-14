#include "application.h"

#include <mmsystem.h>

#include <memory>

#include <tracy/Tracy.hpp>

#include "audio.h"
#include "demo_scene.h"
#include "directX.h"
#include "event_bus.h"
#include "game_scene.h"
#include "imgui_layer.h"
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

#pragma comment(lib, "winmm.lib")

namespace
{
    constexpr char WINDOW_CLASS[] = "DirectXFoundationWindow";
    constexpr char WINDOW_TITLE[] = "DirectX Game Foundation";
    constexpr int TARGET_FPS = 60;
}

Application::Application()
    : m_hWnd(nullptr)
    , m_running(true)
    , m_initialized(false)
    , m_currentFps(0.0f)
    , m_frameCount(0)
    , m_fpsTick(0)
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

    timeBeginPeriod(1);
    const DWORD frameMs = 1000 / TARGET_FPS;
    DWORD lastTick = timeGetTime();
    m_fpsTick = lastTick;

    MSG msg{};
    while (m_running)
    {
        FrameMark;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        const DWORD now = timeGetTime();
        if ((now - m_fpsTick) >= 1000)
        {
            m_currentFps = static_cast<float>(m_frameCount);
            m_frameCount = 0;
            m_fpsTick = now;
            UpdateWindowTitle();
        }

        if ((now - lastTick) < frameMs)
        {
            Sleep(1);
            continue;
        }

        const float deltaTime = static_cast<float>(now - lastTick) / 1000.0f;
        lastTick = now;

        Update(deltaTime);
        Draw();
        ++m_frameCount;
    }

    timeEndPeriod(1);
    return static_cast<int>(msg.wParam);
}

bool Application::Initialize(HINSTANCE instance, int nCmdShow)
{
    ZoneScoped;
    Logger::Initialize();
    Logger::Info("Application initialization started");
    m_hWnd = CreateAppWindow(instance, nCmdShow);
    if (!m_hWnd)
    {
        return false;
    }

    DirectXInitialize(m_hWnd);
    if (!Shader_Initialize(DirectXGetDevice(), DirectXGetDeviceContext()))
    {
        return false;
    }

    m_resources->Initialize(DirectXGetDevice());
    SpriteInitialize();
    if (!InitializeMiddleware())
    {
        return false;
    }

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
    UpdateWindow(m_hWnd);
    UpdateWindowTitle();
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

    ImGuiLayer_Shutdown();
    Audio_Shutdown();
    SpriteFinalize();
    m_resources->Shutdown();
    Shader_Finalize();
    DirectXFinalaize();
    m_initialized = false;
    Logger::Shutdown();
}

void Application::Update(float deltaTime)
{
    ZoneScoped;
    Input_Update();
    Audio_Update();

    m_sceneManager->Update(deltaTime);
    ProcessSceneEvents();
}

void Application::Draw()
{
    ZoneScoped;
    Clear();
    m_sceneManager->Draw();

    ImGuiLayer_BeginFrame();
    ImGuiLayer_DrawFoundationWindow(m_currentFps);
    m_sceneManager->DrawDebugUI();
    ImGuiLayer_EndFrame();
    Present();
}

bool Application::InitializeMiddleware()
{
    Input_Initialize();
    if (!Audio_Initialize())
    {
        return false;
    }
    if (!ImGuiLayer_Initialize(m_hWnd, DirectXGetDevice(), DirectXGetDeviceContext()))
    {
        return false;
    }
    return true;
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
        std::unique_ptr<Scene> nextScene = m_sceneRegistry->Create(requestedSceneId);
        if (!nextScene)
        {
            Logger::Warn("Unknown scene change request: " + requestedSceneId);
            return;
        }

        Logger::Info("Changing scene to " + requestedSceneId);
        Audio_PlayCue("scene_change");
        m_sceneManager->SetScene(std::move(nextScene), *m_resources);
    }
}

void Application::UpdateWindowTitle()
{
    if (!m_hWnd)
    {
        return;
    }

    const char* sceneId = "none";
    if (Scene* currentScene = m_sceneManager->GetCurrentScene())
    {
        sceneId = currentScene->GetSceneId();
    }

    char title[128]{};
    wsprintfA(title, "%s | FPS:%d | Scene: %s", WINDOW_TITLE, static_cast<int>(m_currentFps), sceneId);
    SetWindowTextA(m_hWnd, title);
}

HWND Application::CreateAppWindow(HINSTANCE instance, int nCmdShow)
{
    WNDCLASSEXA wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXA);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = StaticWndProc;
    wcex.hInstance = instance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = WINDOW_CLASS;
    RegisterClassExA(&wcex);

    RECT windowRect{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowA(
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        this);

    ShowWindow(hWnd, nCmdShow);
    return hWnd;
}

LRESULT Application::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGuiLayer_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return 1;
    }

    switch (message)
    {
    case WM_NCCREATE:
        return TRUE;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            DirectXResize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hWnd);
        }
        return 0;

    case WM_DESTROY:
        m_running = false;
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

LRESULT CALLBACK Application::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Application* app = nullptr;
    if (message == WM_NCCREATE)
    {
        CREATESTRUCTA* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
        app = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<Application*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->HandleMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
