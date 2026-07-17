#include "pch.h"

#include "application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

#include <tracy/Tracy.hpp>

#include "DxLib.h"
#include "audio.h"
#include "demo_scene.h"
#include "directX.h"
#include "event_bus.h"
#include "game_font.h"
#include "game_scene.h"
#include "imgui_layer.h"
#include "input.h"
#include "logger.h"
#include "loading_preview_scene.h"
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
    constexpr float SCENE_TRANSITION_DURATION = 0.70f;
    constexpr float SCENE_TRANSITION_SWAP_TIME = SCENE_TRANSITION_DURATION * 0.5f;
    constexpr float STARTUP_FADE_OUT_DURATION = 1.35f;
    constexpr float kCursorTrailSpawnDistance = 18.0f;
    constexpr const char* kGameProgressSavePath = "savegame.json";

    struct SoundCueAsset
    {
        const char* cueName;
        const char* filePath;
    };

    constexpr SoundCueAsset kSoundCueAssets[] =
    {
        { "shutter", "assets/effects/Sound/Player_SE/shutter.wav" },
        { "jump", "assets/effects/Sound/Player_SE/jump.wav" },
        { "fall", "assets/effects/Sound/Stage_SE/fall.wav" },
        { "battery_fall", "assets/effects/Sound/Stage_SE/battery_fall.wav" },
        { "shutter_open", "assets/effects/Sound/Stage_SE/door.wav" },
        { "enemy1_attack", "assets/effects/Sound/Enemy_SE/leazer.wav" },
        { "switch_press", "assets/effects/Sound/Stage_SE/switch.wav" },
        { "elevator_up", "assets/effects/Sound/Stage_SE/elevator03.wav" },
        { "barrel", "assets/effects/Sound/Stage_SE/barrel.wav" },
        { "cant_paste", "assets/effects/Sound/Player_SE/cantPaste.wav" },
        { "enemy_gun", "assets/effects/Sound/Enemy_SE/EnemyGun.wav" },
        { "bgm_forest", "assets/effects/Sound/Stage_BGM/forest.wav" },
        { "bgm_ruins", "assets/effects/Sound/Stage_BGM/ruins.wav" },
        { "bgm_under", "assets/effects/Sound/Stage_BGM/under.wav" },
        { "bgm_title", "assets/effects/Sound/Stage_BGM/title.wav" },
        { "bgm_result", "assets/effects/Sound/Stage_BGM/result.wav" },
        { "bgm_forest_boss", "assets/effects/Sound/Boss_BGM/forest_boss.wav" },
        { "bgm_ruins_boss", "assets/effects/Sound/Boss_BGM/ruins_boss.wav" },
        { "boss_forest_attack2", "assets/effects/Sound/Boss_SE/Forest/attack2.wav" },
        { "boss_forest_boost", "assets/effects/Sound/Boss_SE/Forest/boost.wav" },
        { "boss_forest_dead", "assets/effects/Sound/Boss_SE/Forest/dead.wav" },
        { "boss_forest_destroy", "assets/effects/Sound/Boss_SE/Forest/detroy.wav" },
        { "boss_forest_knockback", "assets/effects/Sound/Boss_SE/Forest/nockback.wav" },
        { "boss_forest_roar", "assets/effects/Sound/Boss_SE/Forest/roar.wav" },
        { "boss_forest_shield_drop", "assets/effects/Sound/Boss_SE/Forest/shield_drop.wav" },
        { "boss_ruins_roar", "assets/effects/Sound/Boss_SE/Ruins/roar.wav" },
        { "boss_ruins_rocket_charge", "assets/effects/Sound/Boss_SE/Ruins/rocketCharge.wav" },
        { "boss_ruins_rocket", "assets/effects/Sound/Boss_SE/Ruins/rocket.wav" },
        { "boss_ruins_rocket_hit", "assets/effects/Sound/Boss_SE/Ruins/rocketHit.wav" },
        { "boss_ruins_firing", "assets/effects/Sound/Boss_SE/Ruins/firing.wav" },
        { "boss_ruins_hit", "assets/effects/Sound/Boss_SE/Ruins/hit.wav" },
        { "boss_ruins_hit2", "assets/effects/Sound/Boss_SE/Ruins/hit2.wav" },
        { "boss_ruins_dead", "assets/effects/Sound/Boss_SE/Ruins/dead.wav" },
        { "boss_ruins_destroy", "assets/effects/Sound/Boss_SE/Ruins/detroy.wav" },
    };

    void LoadSoundCueAssets()
    {
        // Register split sound files once so gameplay can request cues by stable names.
        for (const SoundCueAsset& asset : kSoundCueAssets)
        {
            if (!Audio_LoadCueFromFile(asset.cueName, asset.filePath))
            {
                Logger::Warn(std::string("Failed to load sound cue: ") + asset.cueName + " from " + asset.filePath);
            }
        }
    }

    void ConfigureDpiAwareness()
    {
        HMODULE user32 = GetModuleHandleA("user32.dll");
        if (user32 != nullptr)
        {
            using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
            auto* setProcessDpiAwarenessContext =
                reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                    GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
            if (setProcessDpiAwarenessContext != nullptr)
            {
                const HANDLE kPerMonitorAwareV2 = reinterpret_cast<HANDLE>(-4);
                if (setProcessDpiAwarenessContext(kPerMonitorAwareV2))
                {
                    return;
                }
            }

            using SetProcessDPIAwareFn = BOOL(WINAPI*)();
            auto* setProcessDPIAware =
                reinterpret_cast<SetProcessDPIAwareFn>(
                    GetProcAddress(user32, "SetProcessDPIAware"));
            if (setProcessDPIAware != nullptr)
            {
                setProcessDPIAware();
            }
        }
    }

    void DrawStartupBlackScreen()
    {
        SetDrawScreen(DX_SCREEN_BACK);
        ClearDrawScreen();
        DrawBox(0, 0, kVirtualScreenWidth, kVirtualScreenHeight, GetColor(0, 0, 0), TRUE);
        ScreenFlip();
    }

    void DeleteGameProgressSave()
    {
        std::error_code error;
        const bool removed = std::filesystem::remove(kGameProgressSavePath, error);
        if (error)
        {
            Logger::Warn("Failed to delete save file: " + error.message());
            return;
        }
        if (removed)
        {
            Logger::Info("Deleted save file: " + std::string(kGameProgressSavePath));
        }
    }

}

Application::Application()
    : m_hWnd(nullptr)
    , m_running(true)
    , m_initialized(false)
    , m_exitConfirmationOpen(false)
    , m_startupFadeActive(false)
    , m_titleBgmPending(false)
    , m_sceneTransitionActive(false)
    , m_sceneTransitionSwapped(false)
    , m_currentFps(0.0f)
    , m_startupFadeTimer(0.0f)
    , m_sceneTransitionTimer(0.0f)
    , m_sceneTransitionDuration(SCENE_TRANSITION_DURATION)
    , m_frameCount(0)
    , m_fpsTick(0)
    , m_pendingSceneId()
    , m_cursorParticles()
    , m_cursorParticleCursor(0)
    , m_lastCursorX(0)
    , m_lastCursorY(0)
    , m_cursorParticleSpawnRemainder(0.0f)
    , m_resources(std::make_unique<ResourceManager>())
    , m_sceneManager(std::make_unique<SceneManager>())
    , m_sceneRegistry(std::make_unique<SceneRegistry>())
{
    for (CursorParticle& particle : m_cursorParticles)
    {
        particle.active = false;
    }
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

    // GetNowCount はミリ秒精度（実分解能 約15.6ms）で 60FPS のフレーム予算と同程度のため、
    // マイクロ秒精度の GetNowHiPerformanceCount でフレームを刻む。
    constexpr LONGLONG kFrameMicroseconds = 1000000LL / TARGET_FPS;
    // ヒッチ（ウィンドウ操作・ロード等）後に deltaTime が暴れないよう上限を設ける。
    constexpr float kMaxDeltaTime = 0.1f;

    LONGLONG lastFrameTick = GetNowHiPerformanceCount();
    LONGLONG nextFrameTick = lastFrameTick + kFrameMicroseconds;
    m_fpsTick = lastFrameTick;

    while (m_running && ProcessMessage() == 0)
    {
        FrameMark;

        const LONGLONG now = GetNowHiPerformanceCount();
        if ((now - m_fpsTick) >= 1000000LL)
        {
            m_currentFps = static_cast<float>(m_frameCount);
            m_frameCount = 0;
            m_fpsTick = now;
        }

        if (now < nextFrameTick)
        {
            // 残りが 2ms 以上あればスリープ、それ未満はスピンで精度を出す。
            if ((nextFrameTick - now) >= 2000)
            {
                WaitTimer(1);
            }
            continue;
        }

        // 次フレームの締切は前回締切からの加算で刻み、周期のドリフトを防ぐ。
        // 大きく遅れた場合は現在時刻から貼り直す。
        nextFrameTick += kFrameMicroseconds;
        if (now >= nextFrameTick)
        {
            nextFrameTick = now + kFrameMicroseconds;
        }

        const float deltaTime = std::min(
            static_cast<float>(now - lastFrameTick) / 1000000.0f,
            kMaxDeltaTime);
        lastFrameTick = now;

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

    ConfigureDpiAwareness();
    SetOutApplicationLogValidFlag(FALSE);
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
    ChangeWindowMode(FALSE);
    SetFullScreenResolutionMode(DX_FSRESOLUTIONMODE_BORDERLESS_WINDOW);
    SetFullScreenScalingMode(DX_FSSCALINGMODE_NEAREST, TRUE);
    SetGraphMode(kVirtualScreenWidth, kVirtualScreenHeight, 32, 60);
    SetWindowSizeChangeEnableFlag(FALSE, FALSE);
    SetAlwaysRunFlag(TRUE);
    // ScreenFlip の垂直同期待ちを無効化し、フレームペーシングは Run() のリミッターに一本化する。
    // モニタのリフレッシュレート（60Hz/144Hz 等）で挙動が変わったり、
    // VSync 待ちと手動ウェイトが競合してジッターになるのを防ぐ。
    SetWaitVSyncFlag(FALSE);
    if (DxLib_Init() == -1)
    {
        return false;
    }
    initializeGameFont();
    SetMouseDispFlag(TRUE);
    DrawStartupBlackScreen();

    DirectXInitialize(GetMainWindowHandle());
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
    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        if (!ImGuiLayer_Initialize(GetMainWindowHandle(), DirectXGetDevice(), DirectXGetDeviceContext()))
        {
            return false;
        }
    }

    LoadSoundCueAssets();

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
    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        m_sceneRegistry->Register("loading_preview", []()
            {
                return std::make_unique<LoadingPreviewScene>();
            });
    }

    m_sceneManager->SetScene(m_sceneRegistry->Create("title"), *m_resources);
    m_startupFadeActive = true;
    m_titleBgmPending = true;
    m_startupFadeTimer = 0.0f;
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

    DeleteGameProgressSave();
    m_sceneManager->Shutdown();
    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        ImGuiLayer_Shutdown();
    }
    Audio_Shutdown();
    SpriteFinalize();
    m_resources->Shutdown();
    Shader_Finalize();
    DirectXFinalaize();
    shutdownGameFont();
    DxLib_End();
    m_initialized = false;
    Logger::Shutdown();
}

void Application::Update(float deltaTime)
{
    ZoneScoped;
    Input_Update();
    Audio_Update();
    UpdateStartupFade(deltaTime);

    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        if (Input_IsActionPressed(InputAction::TogglePostProcess))
        {
            DirectXTogglePostProcess();
            Logger::Info(DirectXIsPostProcessEnabled() ? "Post process: ON" : "Post process: OFF");
        }
    }

    ClearCurrentSceneEvents();

    const bool escapePressed = Input_IsActionPressed(InputAction::Cancel);
    if (!m_exitConfirmationOpen && escapePressed)
    {
        Scene* currentScene = m_sceneManager ? m_sceneManager->GetCurrentScene() : nullptr;
        if (currentScene && currentScene->OnCancelAction())
        {
            return;
        }

        m_exitConfirmationOpen = true;
        return;
    }

    if (m_exitConfirmationOpen)
    {
        if (Input_IsActionPressed(InputAction::ExitPromptYes))
        {
            m_running = false;
        }
        else if (Input_IsActionPressed(InputAction::ExitPromptNo))
        {
            m_exitConfirmationOpen = false;
        }
        return;
    }

    if (m_sceneTransitionActive)
    {
        UpdateSceneTransition(deltaTime);
        UpdateCursorParticles(deltaTime);
        return;
    }

    m_sceneManager->Update(deltaTime);
    UpdateCursorParticles(deltaTime);
    ProcessSceneEvents();
}

void Application::Draw()
{
    ZoneScoped;
    DirectXBeginSceneRender();
    m_sceneManager->Draw();
    DirectXCompositeSceneToBackBuffer(static_cast<float>(GetNowCount()) * 0.001f);
    if (m_sceneTransitionActive)
    {
        DrawSceneTransition();
    }
    DrawCursorParticles();
    DrawStartupFade();
    if (m_exitConfirmationOpen)
    {
        DrawExitConfirmation();
    }
    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        ImGuiLayer_BeginFrame();
        ImGuiLayer_SetFoundationOverlayVisible(true);
        m_sceneManager->DrawDebugUI();
        ImGuiLayer_EndFrame();
        ImGuiLayer_DrawFoundationWindow(m_currentFps);
    }
    Present();
}

void Application::UpdateCursorParticles(float deltaTime)
{
    for (CursorParticle& particle : m_cursorParticles)
    {
        if (!particle.active)
        {
            continue;
        }

        particle.age += deltaTime;
        if (particle.age >= particle.lifetime)
        {
            particle.active = false;
            continue;
        }

        particle.x += particle.vx * deltaTime;
        particle.y += particle.vy * deltaTime;
        particle.vx *= std::pow(0.04f, deltaTime);
        particle.vy = particle.vy * std::pow(0.06f, deltaTime) - 8.0f * deltaTime;
    }

    if (!ShouldShowCursorParticles())
    {
        m_lastCursorX = Input_GetMouseX();
        m_lastCursorY = Input_GetMouseY();
        m_cursorParticleSpawnRemainder = 0.0f;
        return;
    }

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const float dx = static_cast<float>(mouseX - m_lastCursorX);
    const float dy = static_cast<float>(mouseY - m_lastCursorY);
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance > 0.5f)
    {
        m_cursorParticleSpawnRemainder += distance;
        const float invDistance = 1.0f / std::max(distance, 0.001f);
        const float dirX = dx * invDistance;
        const float dirY = dy * invDistance;
        while (m_cursorParticleSpawnRemainder >= kCursorTrailSpawnDistance)
        {
            m_cursorParticleSpawnRemainder -= kCursorTrailSpawnDistance;
            const float stepBack = m_cursorParticleSpawnRemainder;
            const float x = static_cast<float>(mouseX) - dirX * stepBack;
            const float y = static_cast<float>(mouseY) - dirY * stepBack;
            const float side = ((m_cursorParticleCursor % 2) == 0) ? 1.0f : -1.0f;
            SpawnCursorParticle(
                x,
                y,
                -dirX * 42.0f - dirY * side * 10.0f,
                -dirY * 42.0f + dirX * side * 10.0f,
                0.34f,
                2.2f + static_cast<float>(m_cursorParticleCursor % 3) * 0.45f,
                255,
                226,
                160,
                false);
        }
    }

    if (Input_IsMouseLeftPressed())
    {
        SpawnCursorParticle(static_cast<float>(mouseX), static_cast<float>(mouseY), 0.0f, 0.0f, 0.26f, 13.0f, 255, 245, 220, true);
        for (int index = 0; index < 6; ++index)
        {
            const float angle = static_cast<float>(index) * 1.04719755f;
            SpawnCursorParticle(
                static_cast<float>(mouseX),
                static_cast<float>(mouseY),
                std::cos(angle) * 58.0f,
                std::sin(angle) * 58.0f,
                0.28f,
                2.0f,
                255,
                236,
                190,
                false);
        }
    }

    m_lastCursorX = mouseX;
    m_lastCursorY = mouseY;
}

void Application::DrawCursorParticles() const
{
    if (!ShouldShowCursorParticles())
    {
        return;
    }

    for (const CursorParticle& particle : m_cursorParticles)
    {
        if (!particle.active)
        {
            continue;
        }

        const float t = std::clamp(particle.age / std::max(0.001f, particle.lifetime), 0.0f, 1.0f);
        const float alphaT = 1.0f - t;
        const int alpha = std::clamp(static_cast<int>(std::round(alphaT * alphaT * 190.0f)), 0, 190);
        if (alpha <= 0)
        {
            continue;
        }

        SetDrawBlendMode(particle.ring ? DX_BLENDMODE_ALPHA : DX_BLENDMODE_ADD, alpha);
        const int color = GetColor(particle.colorR, particle.colorG, particle.colorB);
        if (particle.ring)
        {
            DrawCircleAA(particle.x, particle.y, particle.size + 24.0f * t, 32, color, FALSE, 2.0f);
        }
        else
        {
            DrawCircleAA(particle.x, particle.y, std::max(0.8f, particle.size * alphaT), 16, color, TRUE);
        }
    }
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void Application::SpawnCursorParticle(
    float x,
    float y,
    float vx,
    float vy,
    float lifetime,
    float size,
    int colorR,
    int colorG,
    int colorB,
    bool ring)
{
    CursorParticle& particle = m_cursorParticles[static_cast<std::size_t>(m_cursorParticleCursor)];
    m_cursorParticleCursor = (m_cursorParticleCursor + 1) % static_cast<int>(m_cursorParticles.size());

    particle.x = x;
    particle.y = y;
    particle.vx = vx;
    particle.vy = vy;
    particle.age = 0.0f;
    particle.lifetime = lifetime;
    particle.size = size;
    particle.colorR = colorR;
    particle.colorG = colorG;
    particle.colorB = colorB;
    particle.ring = ring;
    particle.active = true;
}

bool Application::ShouldShowCursorParticles() const
{
    if (!m_sceneManager)
    {
        return false;
    }

    Scene* currentScene = m_sceneManager->GetCurrentScene();
    if (!currentScene)
    {
        return false;
    }

    return std::strcmp(currentScene->GetSceneId(), "game") != 0;
}

void Application::UpdateStartupFade(float deltaTime)
{
    if (!m_startupFadeActive)
    {
        return;
    }

    m_startupFadeTimer += deltaTime;
    if (m_startupFadeTimer >= STARTUP_FADE_OUT_DURATION)
    {
        m_startupFadeActive = false;
        m_startupFadeTimer = 0.0f;
        PlayPendingTitleBgm();
    }
}

void Application::DrawStartupFade() const
{
    if (!m_startupFadeActive)
    {
        return;
    }

    // 起動直後だけタイトルを黒から見せ、専用ロード画面を挟まない。
    const float fadeProgress = std::clamp(
        m_startupFadeTimer / std::max(0.001f, STARTUP_FADE_OUT_DURATION),
        0.0f,
        1.0f);
    const float easedProgress = fadeProgress * fadeProgress * (3.0f - 2.0f * fadeProgress);
    const int drawAlpha = static_cast<int>((1.0f - easedProgress) * 255.0f);
    if (drawAlpha <= 0)
    {
        return;
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, drawAlpha);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void Application::PlayPendingTitleBgm()
{
    if (!m_titleBgmPending)
    {
        return;
    }

    Scene* currentScene = m_sceneManager ? m_sceneManager->GetCurrentScene() : nullptr;
    if (!currentScene || std::strcmp(currentScene->GetSceneId(), "title") != 0)
    {
        return;
    }

    m_titleBgmPending = false;
    Audio_PlayBgmCue("bgm_title");
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
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

bool Application::InitializeMiddleware()
{
    Input_Initialize();
    return Audio_Initialize();
}

void Application::ClearCurrentSceneEvents()
{
    Scene* currentScene = m_sceneManager ? m_sceneManager->GetCurrentScene() : nullptr;
    if (!currentScene)
    {
        return;
    }

    EventBus* eventBus = currentScene->GetEventBus();
    if (!eventBus)
    {
        return;
    }

    eventBus->Clear();
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
        case EventType::ExitApplicationRequested:
            m_running = false;
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
        PlayPendingTitleBgm();
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
    if (sceneId == "title")
    {
        DeleteGameProgressSave();
        // どのシーンからタイトルへ戻っても、現在のBGMは暗転中に必ず落とす。
        Audio_FadeOutBgm(SCENE_TRANSITION_SWAP_TIME);
        m_titleBgmPending = true;
    }
    else
    {
        m_titleBgmPending = false;
    }
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
    if constexpr (build_config::kDebugFeaturesEnabled)
    {
        if (ImGuiLayer_WndProcHandler(hWnd, message, wParam, lParam))
        {
            return TRUE;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK Application::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, message, wParam, lParam);
}


