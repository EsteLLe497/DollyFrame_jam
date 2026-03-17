#pragma once

class Entity;

class Component
{
public:
    virtual ~Component() = default;

    virtual void OnAttach(Entity& owner);
    virtual void Update(float deltaTime);
    virtual void Draw();
    virtual void DrawDebugUI();

protected:
    Entity* m_owner = nullptr;
};
