#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "component.h"

class Entity
{
public:
    Entity() = default;
    ~Entity() = default;

    void Update(float deltaTime);
    void Draw();
    void DrawDebugUI();

    template <typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *component;
        component->OnAttach(*this);
        m_components.push_back(std::move(component));
        return ref;
    }

    template <typename T>
    T* GetComponent()
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        for (const auto& component : m_components)
        {
            if (auto* typed = dynamic_cast<T*>(component.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }

    template <typename T>
    const T* GetComponent() const
    {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");
        for (const auto& component : m_components)
        {
            if (auto* typed = dynamic_cast<const T*>(component.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<Component>> m_components;
};
