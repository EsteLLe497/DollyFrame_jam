#pragma once

#include "game_object_fwd.h"

class MonoBehaviour
{
public:
    virtual ~MonoBehaviour() = default;

    virtual void OnAttach(GameObject& owner);
    virtual void Awake();
    virtual void Start();
    virtual void OnEnable();
    virtual void OnDisable();
    virtual void Update(float deltaTime);
    virtual void Draw();
    virtual void DrawDebugUI();

    bool IsEnabled() const;
    bool IsActiveAndEnabled() const;
    void SetEnabled(bool enabled);

    GameObject& GetGameObject();
    const GameObject& GetGameObject() const;

    template <typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template <typename T>
    T* GetComponent();

    template <typename T>
    const T* GetComponent() const;

    template <typename T>
    bool TryGetComponent(T*& outComponent);

    template <typename T>
    bool TryGetComponent(const T*& outComponent) const;

    template <typename T>
    bool HasComponent() const;

protected:
    GameObject* m_owner = nullptr;

private:
    friend class GameObject;

    bool m_enabled = true;
    bool m_started = false;
};
