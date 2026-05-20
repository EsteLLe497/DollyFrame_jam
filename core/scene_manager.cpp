#include "pch.h"

#include "scene_manager.h"

#include "resource_manager.h"
#include "scene.h"

SceneManager::SceneManager() = default;

void SceneManager::SetScene(std::unique_ptr<Scene> nextScene, ResourceManager& resources)
{
    if (m_currentScene)
    {
        m_currentScene->OnExit();
    }

    m_currentScene = std::move(nextScene);
    if (m_currentScene)
    {
        m_currentScene->OnEnter(resources);
    }
}

void SceneManager::Shutdown()
{
    if (m_currentScene)
    {
        m_currentScene->OnExit();
        m_currentScene.reset();
    }
}

void SceneManager::Update(float deltaTime)
{
    if (m_currentScene)
    {
        m_currentScene->Update(deltaTime);
    }
}

void SceneManager::Draw()
{
    if (m_currentScene)
    {
        m_currentScene->Draw();
    }
}

void SceneManager::DrawDebugUI()
{
    if (m_currentScene)
    {
        m_currentScene->DrawDebugUI();
    }
}

Scene* SceneManager::GetCurrentScene()
{
    return m_currentScene.get();
}
