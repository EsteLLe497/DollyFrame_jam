#pragma once

#include <vector>

#include "game_object.h"
#include "components_photo.h"

enum class GimmickType
{
    Hazard,
    Goal,
    Checkpoint,
    Pickup,
    PhotoSource,
    Filter,
    Gate,
    Switch,
};

class GimmickComponent final : public MonoBehaviour
{
public:
    GimmickComponent(GimmickType type, bool startsEnabled = true, bool oneShot = false);

    void DrawDebugUI() override;
    GimmickType GetType() const;
    bool IsEnabled() const;
    void SetEnabled(bool enabled);
    bool IsOneShot() const;
    bool IsConsumed() const;
    void Consume();
    void Restore();

private:
    GimmickType m_type;
    bool m_enabled;
    bool m_oneShot;
    bool m_consumed;
};

class CheckpointComponent final : public MonoBehaviour
{
public:
    CheckpointComponent(int checkpointId, float respawnX, float respawnY);

    void DrawDebugUI() override;

    int checkpointId;
    float respawnX;
    float respawnY;
    bool activated;
};

class PhotoFilterComponent final : public MonoBehaviour
{
public:
    PhotoFilterComponent(PhotoFilterTheme theme, PhotoCopyRole outputRole, PhotoCopyLayer outputLayer, float tintR, float tintG, float tintB, float tintA);

    void DrawDebugUI() override;
    PhotoFilterTheme GetTheme() const;
    PhotoCopyRole GetOutputRole() const;
    PhotoCopyLayer GetOutputLayer() const;
    float GetTintR() const;
    float GetTintG() const;
    float GetTintB() const;
    float GetTintA() const;

private:
    PhotoFilterTheme m_theme;
    PhotoCopyRole m_outputRole;
    PhotoCopyLayer m_outputLayer;
    float m_tintR;
    float m_tintG;
    float m_tintB;
    float m_tintA;
};

class MerchantComponent final : public MonoBehaviour
{
public:
    MerchantComponent() = default;

    bool playerInRange = false;
    float promptPulse = 0.0f;
};

enum class SepiaRubbleSource
{
    Generic,
    MidBoss3Fist,
    MidBoss3Drill,
};

class SepiaRubbleComponent final : public MonoBehaviour
{
public:
    SepiaRubbleComponent() = default;
    explicit SepiaRubbleComponent(SepiaRubbleSource sourceValue)
        : source(sourceValue)
    {
    }

    SepiaRubbleSource source = SepiaRubbleSource::Generic;
};

class SepiaRubbleGroupComponent final : public MonoBehaviour
{
public:
    SepiaRubbleGroupComponent(
        char markerTypeValue,
        int imageNoValue,
        int restoredTileValue,
        char restoredMarkerTypeValue,
        int restoredMarkerParameterValue,
        int minColumnValue,
        int minRowValue,
        int maxColumnValue,
        int maxRowValue,
        bool isRestoredValue);

    char markerType = '\0';
    int imageNo = 0;
    int restoredTileValue = 0;
    char restoredMarkerType = '\0';
    int restoredMarkerParameter = 0;
    int minColumn = 0;
    int minRow = 0;
    int maxColumn = 0;
    int maxRow = 0;
    bool isRestored = false;
    float restoredLifetime = 0.0f;
    std::vector<int> cellColumns;
    std::vector<int> cellRows;
    std::vector<int> cellRestoredTileValues;
    std::vector<char> cellRestoredMarkerTypes;
    std::vector<int> cellRestoredMarkerParameters;
};

class SepiaElevatorComponent final : public Component
{
public:
    SepiaElevatorComponent(
        float moveRangeY,
        float moveSpeed,
        float topPauseSeconds);

    void OnAttach(Entity& owner) override;
    void DrawDebugUI() override;

    float moveRangeY = 144.0f;
    float moveSpeed = 140.0f;
    float topPauseSeconds = 1.0f;
    float baseY = 0.0f;
    bool cycleStarted = false;
    bool movingUp = true;
    float pauseTimer = 0.0f;
    bool wasPlayerTouching = false;
};
