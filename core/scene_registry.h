#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

class Scene;

class SceneRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Scene>()>;

    void Register(std::string sceneId, Factory factory);
    std::unique_ptr<Scene> Create(std::string_view sceneId) const;
    bool Contains(std::string_view sceneId) const;

private:
    std::unordered_map<std::string, Factory> m_factories;
};
