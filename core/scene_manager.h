#pragma once

#include <memory>

class ResourceManager;
class Scene;

class SceneManager
{
public:
    SceneManager();
    ~SceneManager() = default;

    void SetScene(std::unique_ptr<Scene> nextScene, ResourceManager& resources);
    void Shutdown();
    void Update(float deltaTime);
    void Draw();
    void DrawDebugUI();
    Scene* GetCurrentScene();

private:
    std::unique_ptr<Scene> m_currentScene;
};
