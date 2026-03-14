#pragma once

class ResourceManager;
class EventBus;

class Scene
{
public:
    virtual ~Scene() = default;

    virtual const char* GetSceneId() const = 0;
    virtual void OnEnter(ResourceManager&) {}
    virtual void OnExit() {}
    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;
    virtual void DrawDebugUI() = 0;
    virtual EventBus* GetEventBus() { return nullptr; }
};
