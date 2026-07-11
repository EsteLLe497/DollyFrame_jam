#pragma once

#include "event_bus.h"
#include "scene.h"

class LoadingPreviewScene final : public Scene
{
public:
    LoadingPreviewScene();

    const char* GetSceneId() const override;
    void OnEnter(ResourceManager& resources) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawDebugUI() override;
    bool OnCancelAction() override;
    EventBus* GetEventBus() override;

private:
    void DrawDarkroomPreview(float progress) const;
    void DrawViewfinderPreview(float progress) const;
    void DrawFilmstripPreview(float progress) const;
    void ReturnToTitle();

    EventBus m_eventBus;
    int m_whiteTexture;
    int m_stageTextures[3];
    int m_mode;
    int m_stageIndex;
    float m_elapsed;
    bool m_returnRequested;
};
