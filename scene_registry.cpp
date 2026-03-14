#include "scene_registry.h"

#include "scene.h"

void SceneRegistry::Register(std::string sceneId, Factory factory)
{
    m_factories[std::move(sceneId)] = std::move(factory);
}

std::unique_ptr<Scene> SceneRegistry::Create(std::string_view sceneId) const
{
    const auto found = m_factories.find(std::string(sceneId));
    if (found == m_factories.end())
    {
        return nullptr;
    }

    return found->second();
}

bool SceneRegistry::Contains(std::string_view sceneId) const
{
    return m_factories.contains(std::string(sceneId));
}
