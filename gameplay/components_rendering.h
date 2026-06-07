#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "game_object.h"

class EventBus;

class TransformComponent final : public MonoBehaviour
{
public:
    TransformComponent(float x, float y, float width, float height);

    float x;
    float y;
    float width;
    float height;
    float rotation;
    float scale;
};

class TintComponent final : public MonoBehaviour
{
public:
    TintComponent(float rValue, float gValue, float bValue, float aValue);

    float r;
    float g;
    float b;
    float a;
};

class FlickerLightComponent final : public MonoBehaviour
{
public:
    FlickerLightComponent(
        float radius,
        float intensity,
        float flickerAmplitude,
        float flickerSpeed,
        float offsetX,
        float offsetY,
        float r,
        float g,
        float b,
        bool godRayEnabled,
        float godRayLength,
        float godRayWidth,
        float godRayIntensity,
        float godRayDriftSpeed,
        float godRaySoftness);

    void DrawDebugUI() override;

    float radius;
    float intensity;
    float flickerAmplitude;
    float flickerSpeed;
    float offsetX;
    float offsetY;
    float r;
    float g;
    float b;
    bool godRayEnabled;
    float godRayLength;
    float godRayWidth;
    float godRayIntensity;
    float godRayDriftSpeed;
    float godRaySoftness;
};

class MarkerLightComponent final : public MonoBehaviour
{
public:
    MarkerLightComponent(float radius, float intensity, int linkId = -1);

    void DrawDebugUI() override;

    float radius;
    float intensity;
    int linkId = -1;
    bool activated = false;
};

class StageLightComponent final : public MonoBehaviour
{
public:
    StageLightComponent(
        bool enabled,
        float fixtureTopWidthRatio,
        float beamLength,
        float beamTopWidth,
        float beamBottomWidth,
        float beamFeather,
        float r,
        float g,
        float b,
        float intensity);

    void DrawDebugUI() override;

    bool enabled;
    float fixtureTopWidthRatio;
    float beamLength;
    float beamTopWidth;
    float beamBottomWidth;
    float beamFeather;
    float r;
    float g;
    float b;
    float intensity;
};

class SpriteRenderComponent final : public MonoBehaviour
{
public:
    explicit SpriteRenderComponent(int textureId);

    void Draw() override;
    int GetTextureId() const;
    void SetTextureId(int textureId);
    void SetSourceRect(float tx, float ty, float tw, float th);
    float GetSourceX() const;
    float GetSourceY() const;
    float GetSourceWidth() const;
    float GetSourceHeight() const;
    void SetFlipX(bool value);
    bool GetFlipX() const;
    void SetRenderOffset(float x, float y);
    void SetRenderScale(float x, float y);
    void SetRenderRotationOffset(float radians);
    float GetRenderOffsetX() const;
    float GetRenderOffsetY() const;
    float GetRenderScaleX() const;
    float GetRenderScaleY() const;
    float GetRenderRotationOffset() const;

private:
    int m_textureId;
    float m_sourceX;
    float m_sourceY;
    float m_sourceWidth;
    float m_sourceHeight;
    bool m_flipX;
    float m_renderOffsetX;
    float m_renderOffsetY;
    float m_renderScaleX;
    float m_renderScaleY;
    float m_renderRotationOffset;
};

class SpriteSheetAnimationComponent final : public MonoBehaviour
{
public:
    struct Clip
    {
        int textureId = -1;
        std::vector<int> textureIds;
        std::vector<std::string> textureKeys;
        std::function<int(const std::string&)> textureResolver;
        bool frameTextureMode = false;
        std::vector<int> textureRows;
        int columns = 1;
        int columnsPerTexture = 1;
        int rows = 1;
        int rowsPerTexture = 1;
        int texturePageColumns = 1;
        int startFrame = 0;
        int frameCount = 1;
        float fps = 1.0f;
        bool loop = true;
        float sourceX = 0.0f;
        float sourceY = 0.0f;
        float sourceWidth = 1.0f;
        float sourceHeight = 1.0f;
    };

    SpriteSheetAnimationComponent();

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

    void DefineClip(
        const std::string& name,
        int textureId,
        int columns,
        int rows,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    void DefinePagedRowsClip(
        const std::string& name,
        const std::vector<int>& textureIds,
        int columns,
        int rowsPerTexture,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    void DefinePagedRowsClip(
        const std::string& name,
        const std::vector<int>& textureIds,
        int columns,
        const std::vector<int>& rowsPerTexture,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    void DefineLazyPagedRowsClip(
        const std::string& name,
        const std::vector<std::string>& textureKeys,
        std::function<int(const std::string&)> textureResolver,
        int columns,
        const std::vector<int>& rowsPerTexture,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    void DefinePagedGridClip(
        const std::string& name,
        const std::vector<int>& textureIds,
        int pageColumns,
        int columnsPerTexture,
        int rowsPerTexture,
        int startFrame,
        int frameCount,
        float fps,
        bool loop = true);
    void DefineFrameTextureClip(
        const std::string& name,
        const std::vector<int>& textureIds,
        float fps,
        bool loop = true,
        float sourceX = 0.0f,
        float sourceY = 0.0f,
        float sourceWidth = 1.0f,
        float sourceHeight = 1.0f);
    void DefineLazyFrameTextureClip(
        const std::string& name,
        const std::vector<std::string>& textureKeys,
        std::function<int(const std::string&)> textureResolver,
        float fps,
        bool loop = true,
        float sourceX = 0.0f,
        float sourceY = 0.0f,
        float sourceWidth = 1.0f,
        float sourceHeight = 1.0f);
    bool HasClip(const std::string& name) const;

    bool Play(const std::string& name, bool restartIfSame = false);
    const std::string& GetCurrentClipName() const;
    int GetCurrentFrameIndex() const;
    bool IsCurrentClipFinished() const;
    void SetPlaybackSpeed(float speed);

private:
    void ApplyFrameToSprite();

    std::unordered_map<std::string, Clip> m_clips;
    std::string m_currentClipName;
    float m_elapsedSeconds;
    float m_playbackSpeed;
};
