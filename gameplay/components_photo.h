#pragma once

#include "game_object.h"

enum class PhotoCopyRole
{
    Solid,
    Hazard,
    GoalRelay,
    Pickup,
    Ally,
};

enum class PhotoCopyLayer
{
    Foreground,
    Background,
    Shadow,
};

enum class PhotoCopyOrigin
{
    Generic,
    Enemy,
    Hazard,
    Goal,
    Pickup,
    Tile,
};

enum class PhotoFilterTheme
{
    None,
    Hot,
    Cold,
    Invert,
    Sepia,
};

class PhotoCopyRoleComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyRoleComponent(PhotoCopyRole roleValue);

    PhotoCopyRole role;
};

class PhotoCopyLayerComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyLayerComponent(PhotoCopyLayer layerValue);

    PhotoCopyLayer layer;
};

class PhotoCopyGroupComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyGroupComponent(int groupIdValue);

    int groupId;
};

class PhotoPasteOrderComponent final : public MonoBehaviour
{
public:
    explicit PhotoPasteOrderComponent(int orderValue);

    int order;
};

class PhotoCopyLifetimeComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyLifetimeComponent(float lifetimeSeconds);

    void Update(float deltaTime) override;
    void DrawDebugUI() override;

    float GetRemainingSeconds() const;
    float GetLifetimeSeconds() const;
    bool IsExpired() const;

private:
    float m_lifetimeSeconds;
    float m_remainingSeconds;
};

class PhotoPasteAnimationComponent final : public MonoBehaviour
{
public:
    explicit PhotoPasteAnimationComponent(float durationSeconds);

    void Update(float deltaTime) override;

    float GetNormalizedProgress() const;
    bool IsFinished() const;

private:
    float m_durationSeconds;
    float m_elapsedSeconds;
};

class PhotoCopyOriginComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyOriginComponent(PhotoCopyOrigin originValue);

    PhotoCopyOrigin origin;
};

class PhotoCopyTileValueComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyTileValueComponent(int tileValue);

    int tileValue;
};

class DamagePlatformComponent final : public MonoBehaviour
{
public:
    explicit DamagePlatformComponent(int tileSpanValue);

    int tileSpan;
};

class SpikeStripComponent final : public MonoBehaviour
{
public:
    explicit SpikeStripComponent(int tileSpanValue);

    int tileSpan;
};

class VanishOnCaptureComponent final : public MonoBehaviour
{
public:
    explicit VanishOnCaptureComponent(bool enabled = true);

    bool enabled;
};

class PhotoCopyEffectComponent final : public MonoBehaviour
{
public:
    explicit PhotoCopyEffectComponent(PhotoFilterTheme themeValue = PhotoFilterTheme::None);

    void DrawDebugUI() override;
    PhotoFilterTheme GetTheme() const;
    void SetTheme(PhotoFilterTheme themeValue);

private:
    PhotoFilterTheme m_theme;
};
