#pragma once

#include <functional>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

class Scene;

struct SceneIdHash
{
    using is_transparent = void;

    template <typename T>
    std::size_t operator()(const T& value) const noexcept
        requires std::is_convertible_v<const T&, std::string_view>
    {
        return std::hash<std::string_view>{}(std::string_view(value));
    }
};

struct SceneIdEqual
{
    using is_transparent = void;

    template <typename Lhs, typename Rhs>
    bool operator()(const Lhs& lhs, const Rhs& rhs) const noexcept
        requires std::is_convertible_v<const Lhs&, std::string_view> &&
                 std::is_convertible_v<const Rhs&, std::string_view>
    {
        return std::string_view(lhs) == std::string_view(rhs);
    }
};

class SceneRegistry
{
public:
    using Factory = std::function<std::unique_ptr<Scene>()>;

    void Register(std::string sceneId, Factory factory);
    std::unique_ptr<Scene> Create(std::string_view sceneId) const;
    bool Contains(std::string_view sceneId) const;

private:
    std::unordered_map<std::string, Factory, SceneIdHash, SceneIdEqual> m_factories;
};
