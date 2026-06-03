#pragma once

#include <memory>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

#include "component.h"

class GameObject
{
public:
    GameObject() = default;
    ~GameObject() = default;

    void SetActive(bool active);
    bool IsActive() const;

    void Update(float deltaTime);
    void Draw();
    void DrawDebugUI();

    template <typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<MonoBehaviour, T>, "T must derive from MonoBehaviour");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *component;
        component->OnAttach(*this);
        m_componentLookup.emplace_back(std::type_index(typeid(T)), &ref);
        m_components.push_back(std::move(component));
        ref.Awake();
        if (m_activeSelf && ref.IsEnabled())
        {
            ref.OnEnable();
        }
        return ref;
    }

    template <typename T>
    T* GetComponent()
    {
        using ComponentType = std::remove_cv_t<T>;
        static_assert(std::is_base_of_v<MonoBehaviour, ComponentType>, "T must derive from MonoBehaviour");
        const std::type_index requestedType(typeid(ComponentType));
        for (const auto& cached : m_componentLookup)
        {
            if (cached.first == requestedType)
            {
                return static_cast<T*>(cached.second);
            }
        }

        for (const auto& component : m_components)
        {
            if (auto* typed = dynamic_cast<T*>(component.get()))
            {
                m_componentLookup.emplace_back(requestedType, typed);
                return typed;
            }
        }
        return nullptr;
    }

    template <typename T>
    const T* GetComponent() const
    {
        using ComponentType = std::remove_cv_t<T>;
        static_assert(std::is_base_of_v<MonoBehaviour, ComponentType>, "T must derive from MonoBehaviour");
        const std::type_index requestedType(typeid(ComponentType));
        for (const auto& cached : m_componentLookup)
        {
            if (cached.first == requestedType)
            {
                return static_cast<const T*>(cached.second);
            }
        }

        for (const auto& component : m_components)
        {
            if (auto* typed = dynamic_cast<const T*>(component.get()))
            {
                return typed;
            }
        }
        return nullptr;
    }

    template <typename T>
    bool TryGetComponent(T*& outComponent)
    {
        using ComponentType = std::remove_cv_t<T>;
        static_assert(std::is_base_of_v<MonoBehaviour, ComponentType>, "T must derive from MonoBehaviour");
        outComponent = GetComponent<ComponentType>();
        return outComponent != nullptr;
    }

    template <typename T>
    bool TryGetComponent(const T*& outComponent) const
    {
        using ComponentType = std::remove_cv_t<T>;
        static_assert(std::is_base_of_v<MonoBehaviour, ComponentType>, "T must derive from MonoBehaviour");
        outComponent = GetComponent<ComponentType>();
        return outComponent != nullptr;
    }

    template <typename T>
    bool HasComponent() const
    {
        return GetComponent<T>() != nullptr;
    }

private:
    bool m_activeSelf = true;
    std::vector<std::unique_ptr<MonoBehaviour>> m_components;
    std::vector<std::pair<std::type_index, MonoBehaviour*>> m_componentLookup;
};

template <typename T, typename... Args>
T& MonoBehaviour::AddComponent(Args&&... args)
{
    return GetGameObject().AddComponent<T>(std::forward<Args>(args)...);
}

template <typename T>
T* MonoBehaviour::GetComponent()
{
    return m_owner ? m_owner->GetComponent<T>() : nullptr;
}

template <typename T>
const T* MonoBehaviour::GetComponent() const
{
    return m_owner ? m_owner->GetComponent<T>() : nullptr;
}

template <typename T>
bool MonoBehaviour::TryGetComponent(T*& outComponent)
{
    outComponent = GetComponent<T>();
    return outComponent != nullptr;
}

template <typename T>
bool MonoBehaviour::TryGetComponent(const T*& outComponent) const
{
    outComponent = GetComponent<T>();
    return outComponent != nullptr;
}

template <typename T>
bool MonoBehaviour::HasComponent() const
{
    return GetComponent<T>() != nullptr;
}
