#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <memory>

class ResourceManager;
class Scene;
class SceneManager;
class SceneRegistry;

class Application
{
public:
    Application();
    ~Application();

    int Run(HINSTANCE instance, int nCmdShow);

private:
    bool Initialize(HINSTANCE instance, int nCmdShow);
    void Shutdown();
    void Update(float deltaTime);
    void Draw();
    void ProcessSceneEvents();
    void UpdateWindowTitle();
    HWND CreateAppWindow(HINSTANCE instance, int nCmdShow);
    bool InitializeMiddleware();

    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    HWND m_hWnd;
    bool m_running;
    bool m_initialized;
    float m_currentFps;
    DWORD m_frameCount;
    DWORD m_fpsTick;
    std::unique_ptr<ResourceManager> m_resources;
    std::unique_ptr<SceneManager> m_sceneManager;
    std::unique_ptr<SceneRegistry> m_sceneRegistry;
};
