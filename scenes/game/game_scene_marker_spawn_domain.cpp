#include "pch.h"

#include "game_scene_internal.h"

#include <array>
#include <algorithm>
#include <cctype>
#include <limits>
#include <string_view>

#include "prefab_factory.h"

using namespace game_scene_detail;

namespace
{
    void AddUnderBatteryGlow(Entity& battery)
    {
        battery.AddComponent<FlickerLightComponent>(
            56.0f,
            0.42f,
            0.08f,
            3.2f,
            0.0f,
            0.0f,
            0.34f,
            0.88f,
            1.0f,
            false,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            0.0f);
    }

    struct StageLightSizing
    {
        float lightTiles = 3.0f;
        float fixtureTiles = 1.0f;
    };

    StageLightSizing ResolveStageLightSizing(int markerParameter)
    {
        constexpr float kDefaultLightTiles = 3.0f;
        constexpr float kDefaultFixtureTiles = 1.0f;
        constexpr float kMinLightTiles = 1.0f;
        constexpr float kMaxLightTiles = 9.0f;
        constexpr float kMinFixtureTiles = 0.5f;
        constexpr float kMaxFixtureTiles = 4.0f;

        const int encoded = markerParameter < 0 ? 0 : markerParameter;
        StageLightSizing sizing;
        sizing.lightTiles = kDefaultLightTiles;
        sizing.fixtureTiles = kDefaultFixtureTiles;

        if (encoded > 0 && encoded < 10)
        {
            sizing.lightTiles = static_cast<float>(encoded);
        }
        else if (encoded >= 10 && encoded < 100)
        {
            sizing.lightTiles = static_cast<float>(encoded / 10);
            const int fixtureDigit = encoded % 10;
            sizing.fixtureTiles = fixtureDigit > 0 ? static_cast<float>(fixtureDigit) : kDefaultFixtureTiles;
        }

        sizing.lightTiles = std::clamp(sizing.lightTiles, kMinLightTiles, kMaxLightTiles);
        sizing.fixtureTiles = std::clamp(sizing.fixtureTiles, kMinFixtureTiles, kMaxFixtureTiles);
        return sizing;
    }

    struct SepiaMarkerParameter
    {
        int imageNo = 0;
        int restoredTileValue = 0;
        char restoredMarkerType = '\0';
        int restoredMarkerParameter = 0;
        bool valid = true;
    };

    inline SepiaMarkerParameter ResolveSepiaMarkerParameter(char marker2, int markerParameter, int markerParameter2)
    {
        constexpr int kDefaultImageNo = 0;
        constexpr int kDefaultRestoredTileValue = 0;
        constexpr int kMinImageNo = 0;
        constexpr int kMaxImageNo = 9;
        constexpr int kMinRestoredTileValue = 1;
        constexpr int kMaxRestoredTileValue = 99;

        SepiaMarkerParameter parameter;
        parameter.imageNo = kDefaultImageNo;
        parameter.restoredTileValue = kDefaultRestoredTileValue;
        parameter.valid = true;

        if (marker2 != '\0')
        {
            if (markerParameter != 0)
            {
                parameter.valid = false;
                return parameter;
            }
            parameter.imageNo = -1;
            parameter.restoredTileValue = 0;
            parameter.restoredMarkerType = static_cast<char>(
                std::toupper(static_cast<unsigned char>(marker2)));
            parameter.restoredMarkerParameter = markerParameter2;
            return parameter;
        }

        if (markerParameter2 != 0)
        {
            parameter.valid = false;
            return parameter;
        }

        const int encoded = markerParameter < 0 ? 0 : markerParameter;

        if (encoded == 0)
        {
            parameter.valid = false;
            return parameter;
        }

        if (encoded > 0 && encoded < 10)
        {
            parameter.imageNo = kDefaultImageNo;
            parameter.restoredTileValue = std::clamp(
                encoded,
                kMinRestoredTileValue,
                kMaxRestoredTileValue);
            return parameter;
        }

        if (encoded >= 10 && encoded < 1000)
        {
            const int restoredTileValue = encoded / 10;
            const int imageDigit = encoded % 10;
            if (restoredTileValue == 0)
            {
                parameter.valid = false;
                return parameter;
            }
            parameter.imageNo = std::clamp(
                imageDigit,
                kMinImageNo,
                kMaxImageNo);
            parameter.restoredTileValue = std::clamp(
                restoredTileValue,
                kMinRestoredTileValue,
                kMaxRestoredTileValue);
            return parameter;
        }

        parameter.valid = false;
        return parameter;
    }

    struct SepiaMarkerCell
    {
		int column = 0;
        int row = 0;
		int imageNo = -1;
        int restoredTileValue = 0;
        char restoredMarkerType = '\0';
        int restoredMarkerParameter = 0;
    };

    bool TryGetSepiaMarkerCell( const TileMap& tileMap, int column, int row,
        SepiaMarkerCell& outCell)
    {
        if (column < 0 ||
            row < 0 ||
            column >= tileMap.GetWidth() ||
            row >= tileMap.GetHeight())
        {
            return false;
        }

        const char marker = static_cast<char>(
            std::toupper(static_cast<unsigned char>(
                tileMap.GetMarker(column, row))));

        if (marker != '>' && marker != '<')
        {
            return false;
        }

        const SepiaMarkerParameter parameter = ResolveSepiaMarkerParameter(
            tileMap.GetMarker2(column,row),
            tileMap.GetMarkerParameter(column,row),
            tileMap.GetMarkerParameter2(column,row)
                );

        if (!parameter.valid)
        {
            return false;
        }
        if (parameter.restoredMarkerType != '\0')
        {
            if (!IsRestoreSepiaObjectMarker(parameter.restoredMarkerType))
            {
                return false;
            }
        }
        outCell.column = column;
        outCell.row = row;
        outCell.imageNo = parameter.imageNo;
        outCell.restoredTileValue = parameter.restoredTileValue;
        outCell.restoredMarkerType = parameter.restoredMarkerType;
        outCell.restoredMarkerParameter = parameter.restoredMarkerParameter;
        return true;
    }


    struct SwitchMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int requiredBatteryCount = 1;
        bool controlsLaserPower = false;
        SwitchPressMode pressMode = SwitchPressMode::Battery;
    };

    struct ElevatorMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int moveRangeTiles = 3;
        float widthTiles = 5.0f;
    };

    struct LaserSwitchMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int linkIdOverride = -1;
    };

    struct ShutterMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int moveRangeTiles = 3;
        int linkIdOverride = -1;
        bool useBossDefeatSignal = false;
        bool opensWhenUnpowered = false;
    };

    struct ProtectiveWallMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int durability = 2;
        int linkIdOverride = -1;
        int widthTiles = 1;
        int markerHeightTiles = 1;
        int heightTiles = 3;
    };

    struct BatteryGeneratorMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int spawnDirectionX = 1;
    };

    struct LinkedGimmickColor
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
    };

    struct LinkedGimmickSpawnConfig
    {
        float batterySwitchWidthTiles = 3.0f;
        float batterySwitchHeightTiles = 0.5f;
        float batterySwitchPressDepthRatio = 0.22f;
        float batterySwitchPressSpeed = 12.0f;
        float batterySwitchReleaseSpeed = 9.0f;
        float elevatorWidthTiles = 5.0f;
        float elevatorHeightTiles = 1.0f;
        float elevatorSpeedTilesPerSec = 2.5f;
        float elevatorTopPauseSeconds = 1.0f;
        float laserSwitchWidthTiles = 1.0f;
        float laserSwitchHeightTiles = 2.0f;
        float shutterWidthTiles = 1.0f;
        float shutterHeightTiles = 3.0f;
        float shutterSpeedTilesPerSec = 3.2f;
        float protectiveWallSpeedTilesPerSec = 4.0f;
        float batteryGeneratorWidthTiles = 3.0f;
        float batteryGeneratorHeightTiles = 2.0f;
        float batteryGeneratorCooldownSeconds = 3.0f;
        LinkedGimmickColor batterySwitchColor{ 0.92f, 0.26f, 0.20f };
        LinkedGimmickColor batteryGeneratorColor{ 0.32f, 0.32f, 0.32f };
        LinkedGimmickColor elevatorColor{ 0.42f, 0.46f, 0.52f };
        LinkedGimmickColor laserSwitchColor{ 0.96f, 0.86f, 0.20f };
        LinkedGimmickColor shutterColor{ 0.46f, 0.50f, 0.56f };
        LinkedGimmickColor protectiveWallColor{ 0.26f, 0.78f, 0.70f };
    };

    constexpr LinkedGimmickSpawnConfig kLinkedGimmickSpawnConfig{};

    void CollectLinkedGimmickMarkers(
        const TileMap& tileMap,
        float tileSize,
        std::vector<SwitchMarker>& outSwitchMarkers,
        std::vector<ElevatorMarker>& outElevatorMarkers,
        std::vector<LaserSwitchMarker>& outLaserSwitchMarkers,
        std::vector<ShutterMarker>& outShutterMarkers,
        std::vector<ProtectiveWallMarker>& outProtectiveWallMarkers,
        std::vector<BatteryGeneratorMarker>& outBatteryGeneratorMarkers)
    {
        outSwitchMarkers.clear();
        outElevatorMarkers.clear();
        outLaserSwitchMarkers.clear();
        outShutterMarkers.clear();
        outProtectiveWallMarkers.clear();
        outBatteryGeneratorMarkers.clear();

        const size_t estimatedMarkerCount =
            static_cast<size_t>((std::max)(0, tileMap.GetWidth())) *
            static_cast<size_t>((std::max)(0, tileMap.GetHeight()));
        outSwitchMarkers.reserve(estimatedMarkerCount / 32 + 1);
        outElevatorMarkers.reserve(estimatedMarkerCount / 32 + 1);
        outLaserSwitchMarkers.reserve(estimatedMarkerCount / 32 + 1);
        outShutterMarkers.reserve(estimatedMarkerCount / 32 + 1);
        outProtectiveWallMarkers.reserve(estimatedMarkerCount / 32 + 1);
        outBatteryGeneratorMarkers.reserve(estimatedMarkerCount / 32 + 1);

        for (int row = 0; row < tileMap.GetHeight(); ++row)
        {
            for (int column = 0; column < tileMap.GetWidth(); ++column)
            {
                const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(tileMap.GetMarker(column, row))));
                const float markerX = static_cast<float>(column) * tileSize;
                const float markerY = static_cast<float>(row) * tileSize;
                if (marker == 'K')
                {
                    const bool isPlayerSwitch = static_cast<char>(
                        std::toupper(static_cast<unsigned char>(
                            tileMap.GetMarker2(column, row)))) == '*';
                    outSwitchMarkers.push_back(SwitchMarker{
                        markerX,
                        markerY + tileSize * 0.5f,
                        isPlayerSwitch ? 1 : (std::max)(1, tileMap.GetMarkerParameter(column, row)),
                        false,
                        isPlayerSwitch ? SwitchPressMode::Player : SwitchPressMode::Battery });
                }
                else if (marker == 'L')
                {
                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    outElevatorMarkers.push_back(ElevatorMarker{
                        markerX,
                        markerY,
                        markerParameter > 0 ? markerParameter : 3,
                        kLinkedGimmickSpawnConfig.elevatorWidthTiles });
                }
                else if (marker == 'Q')
                {
                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    outElevatorMarkers.push_back(ElevatorMarker{
                        markerX,
                        markerY,
                        markerParameter > 0 ? markerParameter : 3,
                        4.0f });
                }
                else if (marker == 'O')
                {
                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    outLaserSwitchMarkers.push_back(LaserSwitchMarker{
                        markerX,
                        markerY,
                        markerParameter > 0 ? markerParameter : -1 });
                }
                else if (marker == 'X')
                {
                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    outSwitchMarkers.push_back(SwitchMarker{
                        markerX,
                        markerY + tileSize * 0.5f,
                        (std::max)(1, markerParameter),
                        true,
                        SwitchPressMode::Battery });
                }
                else if (marker == 'J')
                {
                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    ShutterMarker shutterMarker{};
                    shutterMarker.x = markerX;
                    shutterMarker.y = markerY;
                    // J marker parameter:
                    //   >0  : link id override. Pair with O using the same number (O1 -> J1).
                    //   <0  : inverse shutter. Pair with O using the absolute value (O1 -> J-1).
                    //   99  : boss defeat trigger enabled (link signal OR boss defeat)
                    if (markerParameter == 99)
                    {
                        shutterMarker.useBossDefeatSignal = true;
                    }
                    else if (markerParameter < 0)
                    {
                        shutterMarker.linkIdOverride = -markerParameter;
                        shutterMarker.opensWhenUnpowered = true;
                    }
                    else if (markerParameter > 0)
                    {
                        shutterMarker.linkIdOverride = markerParameter;
                    }
                    outShutterMarkers.push_back(shutterMarker);
                }
                else if (marker == '&')
                {
                    if ((column > 0 && tileMap.GetMarker(column - 1, row) == '&') ||
                        (row > 0 && tileMap.GetMarker(column, row - 1) == '&'))
                    {
                        continue;
                    }

                    int widthTiles = 1;
                    while (column + widthTiles < tileMap.GetWidth() &&
                        tileMap.GetMarker(column + widthTiles, row) == '&')
                    {
                        ++widthTiles;
                    }

                    int heightTiles = 1;
                    bool rowMatchesWallWidth = true;
                    while (row + heightTiles < tileMap.GetHeight() && rowMatchesWallWidth)
                    {
                        for (int wallColumnOffset = 0; wallColumnOffset < widthTiles; ++wallColumnOffset)
                        {
                            if (tileMap.GetMarker(column + wallColumnOffset, row + heightTiles) != '&')
                            {
                                rowMatchesWallWidth = false;
                                break;
                            }
                        }
                        if (rowMatchesWallWidth)
                        {
                            ++heightTiles;
                        }
                    }

                    const int markerParameter = tileMap.GetMarkerParameter(column, row);
                    const int wallLinkIdOverride = markerParameter < 0 ? -markerParameter : -1;
                    const int wallDurability = markerParameter > 0 ? markerParameter : 2;
                    constexpr int kDefaultProtectiveWallHeightTiles = 5;
                    const int wallHeightTiles = heightTiles > 1 ? heightTiles : kDefaultProtectiveWallHeightTiles;
                    outProtectiveWallMarkers.push_back(ProtectiveWallMarker{
                        markerX,
                        markerY,
                        wallDurability,
                        wallLinkIdOverride,
                        widthTiles,
                        heightTiles,
                        wallHeightTiles });
                }
                else if (marker == 'Y')
                {
                    const char marker2 = static_cast<char>(std::toupper(static_cast<unsigned char>(tileMap.GetMarker2(column, row))));
                    const int markerParameter2 = tileMap.GetMarkerParameter2(column, row);
                    if (marker2 != 'Y' || markerParameter2 == 0)
                    {
                        continue;
                    }

                    outBatteryGeneratorMarkers.push_back(BatteryGeneratorMarker{
                        markerX,
                        markerY,
                        markerParameter2 < 0 ? -1 : 1 });
                }
            }
        }
    }

    std::vector<int> BuildLaserSwitchLinkIds(const std::vector<LaserSwitchMarker>& laserSwitchMarkers)
    {
        std::vector<int> linkIds;
        linkIds.reserve(laserSwitchMarkers.size());
        for (int index = 0; index < static_cast<int>(laserSwitchMarkers.size()); ++index)
        {
            const LaserSwitchMarker& marker = laserSwitchMarkers[static_cast<size_t>(index)];
            linkIds.push_back(marker.linkIdOverride >= 0 ? marker.linkIdOverride : index);
        }
        return linkIds;
    }

    int ResolveShutterLinkId(
        const ShutterMarker& marker,
        int shutterIndex,
        const std::vector<LaserSwitchMarker>& laserSwitchMarkers,
        const std::vector<int>& laserSwitchLinkIds)
    {
        if (marker.linkIdOverride >= 0)
        {
            return marker.linkIdOverride;
        }

        if (laserSwitchMarkers.empty())
        {
            return shutterIndex;
        }

        int nearestIndex = 0;
        float nearestDistSq = std::numeric_limits<float>::max();
        for (int switchIndex = 0; switchIndex < static_cast<int>(laserSwitchMarkers.size()); ++switchIndex)
        {
            const LaserSwitchMarker& switchMarker = laserSwitchMarkers[static_cast<size_t>(switchIndex)];
            const float dx = switchMarker.x - marker.x;
            const float dy = switchMarker.y - marker.y;
            const float distSq = dx * dx + dy * dy;
            if (distSq < nearestDistSq)
            {
                nearestDistSq = distSq;
                nearestIndex = switchIndex;
            }
        }

        return laserSwitchLinkIds[static_cast<size_t>(nearestIndex)];
    }

    struct SepiaGroupSizing
    {
        float widthTiles = 1.0f;
        float heightTiles = 1.0f;
    };

    bool TryResolveSepiaTileSizing(
        char targetMarker,
        int restoredTileValue,
        SepiaGroupSizing& outSizing)
    {
        if (targetMarker == '<' && restoredTileValue == 11)
        {
            outSizing.widthTiles = 9.0f;
            outSizing.heightTiles = 5.0f;
            return true;
        }
        return false;
    }

    bool TryResolveSepiaGroupSizing(
        char targetMarker,
        char restoredMarkerType,
        SepiaGroupSizing& outSizing)
    {
        const char marker = static_cast<char>(
            std::toupper(static_cast<unsigned char>(restoredMarkerType)));

        const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
        struct SepiaGroupSizingRule
        {
            char marker;
            float widthTiles;
            float heightTiles;
        };

        const std::array<SepiaGroupSizingRule, 7> rules
        {
            SepiaGroupSizingRule{ 'M', 4.0f, 1.0f },
            SepiaGroupSizingRule{ 'S', 2.0f, 2.0f },
            SepiaGroupSizingRule{ 'K', cfg.batterySwitchWidthTiles, cfg.batterySwitchHeightTiles },
            SepiaGroupSizingRule{ 'X', cfg.batterySwitchWidthTiles, cfg.batterySwitchHeightTiles },
            SepiaGroupSizingRule{ 'L', cfg.elevatorWidthTiles, cfg.elevatorHeightTiles },
            SepiaGroupSizingRule{ 'Q', 4.0f, cfg.elevatorHeightTiles },
            SepiaGroupSizingRule{ 'O', cfg.laserSwitchWidthTiles, cfg.laserSwitchHeightTiles },
        };

        for (const SepiaGroupSizingRule& rule : rules)
        {
            if (marker == rule.marker)
            {
                outSizing.widthTiles = rule.widthTiles;
                outSizing.heightTiles = rule.heightTiles;
                return true;
            }
        }

        if (marker == 'J')
        {
            outSizing.widthTiles = cfg.shutterWidthTiles;
            outSizing.heightTiles = cfg.shutterHeightTiles;
            return true;
        }

        if (marker == '+' && targetMarker == '<')
        {
            outSizing.widthTiles = 4.0f;
            outSizing.heightTiles = cfg.elevatorHeightTiles;
            return true;
        }

        if (IsEnemyMarker(marker))
        {
            outSizing.widthTiles = 1.0f;
            outSizing.heightTiles = 1.0f;
            return true;
        }

        return false;
    }
}

void GameScene::SpawnBatterySwitchMarker(float x, float y, int requiredBatteryCount, bool controlsLaserPower, int linkId, float tileSize, SwitchPressMode pressMode)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    auto switchEntity = std::make_unique<Entity>();
    switchEntity->AddComponent<TagComponent>(kTagBatterySwitch);
    switchEntity->AddComponent<TransformComponent>(
        x,
        y,
        tileSize * cfg.batterySwitchWidthTiles,
        tileSize * cfg.batterySwitchHeightTiles);
    switchEntity->AddComponent<TintComponent>(
        cfg.batterySwitchColor.r,
        cfg.batterySwitchColor.g,
        cfg.batterySwitchColor.b,
        1.0f);
    switchEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    switchEntity->AddComponent<BatterySwitchComponent>(
        linkId,
        requiredBatteryCount,
        tileSize * cfg.batterySwitchPressDepthRatio,
        cfg.batterySwitchPressSpeed,
        cfg.batterySwitchReleaseSpeed,
        controlsLaserPower,
        pressMode);
    m_world.Spawn(std::move(switchEntity));
}

void GameScene::SpawnBatteryGeneratorMarker(float x, float y, int linkId, int spawnDirectionX, float tileSize)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    auto generatorEntity = std::make_unique<Entity>();
    generatorEntity->AddComponent<TagComponent>(kTagBatteryGenerator);
    generatorEntity->AddComponent<TransformComponent>(
        x,
        y,
        tileSize * cfg.batteryGeneratorWidthTiles,
        tileSize * cfg.batteryGeneratorHeightTiles);
    generatorEntity->AddComponent<TintComponent>(
        cfg.batteryGeneratorColor.r,
        cfg.batteryGeneratorColor.g,
        cfg.batteryGeneratorColor.b,
        1.0f);
    generatorEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    generatorEntity->AddComponent<BatteryGeneratorComponent>(
        linkId,
        cfg.batteryGeneratorCooldownSeconds,
        spawnDirectionX);
    m_world.Spawn(std::move(generatorEntity));
}

void GameScene::SpawnElevatorMarker(float x, float y, int moveRangeTiles, float widthTiles, int linkId, float tileSize)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    auto elevatorEntity = std::make_unique<Entity>();
    elevatorEntity->AddComponent<TagComponent>(kTagElevator);
    elevatorEntity->AddComponent<TransformComponent>(
        x,
        y,
        tileSize * widthTiles,
        tileSize * cfg.elevatorHeightTiles);
    elevatorEntity->AddComponent<TintComponent>(
        cfg.elevatorColor.r,
        cfg.elevatorColor.g,
        cfg.elevatorColor.b,
        1.0f);
    elevatorEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    elevatorEntity->AddComponent<ElevatorComponent>(
        linkId,
        tileSize * static_cast<float>(moveRangeTiles),
        tileSize * cfg.elevatorSpeedTilesPerSec,
        cfg.elevatorTopPauseSeconds);
    m_world.Spawn(std::move(elevatorEntity));
}

void GameScene::SpawnLaserSwitchMarker(float x, float y, int linkId, float tileSize)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    auto switchEntity = std::make_unique<Entity>();
    switchEntity->AddComponent<TagComponent>(kTagLaserSwitch);
    switchEntity->AddComponent<TransformComponent>(
        x,
        y,
        tileSize * cfg.laserSwitchWidthTiles,
        tileSize * cfg.laserSwitchHeightTiles);
    switchEntity->AddComponent<TintComponent>(
        cfg.laserSwitchColor.r,
        cfg.laserSwitchColor.g,
        cfg.laserSwitchColor.b,
        1.0f);
    switchEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    switchEntity->AddComponent<LaserSwitchComponent>(linkId);
    m_world.Spawn(std::move(switchEntity));
}

void GameScene::SpawnShutterMarker(
    float x,
    float y,
    int moveRangeTiles,
    int linkId,
    bool useBossDefeatSignal,
    bool opensWhenUnpowered,
    float tileSize)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    const int effectiveMoveRangeTiles = (std::max)(1, moveRangeTiles);
    auto shutterEntity = std::make_unique<Entity>();
    shutterEntity->AddComponent<TagComponent>(kTagShutter);
    shutterEntity->AddComponent<TransformComponent>(
        x,
        y,
        tileSize * cfg.shutterWidthTiles,
        tileSize * cfg.shutterHeightTiles);
    shutterEntity->AddComponent<TintComponent>(
        cfg.shutterColor.r,
        cfg.shutterColor.g,
        cfg.shutterColor.b,
        1.0f);
    shutterEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    shutterEntity->AddComponent<ShutterComponent>(
        linkId,
        tileSize * static_cast<float>(effectiveMoveRangeTiles),
        tileSize * cfg.shutterSpeedTilesPerSec,
        useBossDefeatSignal,
        opensWhenUnpowered);
    m_world.Spawn(std::move(shutterEntity));
}

void GameScene::SpawnProtectiveWallMarker(float x, float y, int durability, int linkId, int widthTiles, int markerHeightTiles, int heightTiles, float tileSize)
{
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;
    const float markerWidth = tileSize * static_cast<float>((std::max)(1, widthTiles));
    const float markerHeight = tileSize * static_cast<float>((std::max)(1, markerHeightTiles));
    const float wallWidth = tileSize * static_cast<float>((std::max)(1, widthTiles));
    const float wallHeight = tileSize * static_cast<float>((std::max)(1, heightTiles));
    const float wallX = x + markerWidth * 0.5f - wallWidth * 0.5f;
    const float wallY = y + markerHeight - tileSize - wallHeight;
    auto wallEntity = std::make_unique<Entity>();
    wallEntity->AddComponent<TagComponent>(kTagProtectiveWall);
    wallEntity->AddComponent<TransformComponent>(
        wallX,
        wallY,
        wallWidth,
        wallHeight);
    wallEntity->AddComponent<TintComponent>(
        cfg.protectiveWallColor.r,
        cfg.protectiveWallColor.g,
        cfg.protectiveWallColor.b,
        1.0f);
    wallEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
    wallEntity->AddComponent<ProtectiveWallComponent>(
        linkId,
        durability,
        wallHeight,
        tileSize * cfg.protectiveWallSpeedTilesPerSec,
        false);
    m_world.Spawn(std::move(wallEntity));
}

void GameScene::RefreshMarkerDrivenSystems()
{
    RefreshEnemiesFromMarkers();
    RefreshBatteriesFromMarkers();
    RefreshLogsFromMarkers();
    RefreshJumpPadsFromMarkers();
    ReflashFallingRockfromMarkers();
    RefreshMarkerLightsFromMarkers();
    RefreshStageLightsFromMarkers();
    RefreshLaserTurretsFromMarkers();
    RefreshLinkedGimmicksFromMarkers();
    RefreshDamageFootholdsFromMarkers();
	RefleshSepiaRubblesFromMarkers();
}

void GameScene::RefreshMarkerDrivenSystemsByMarkerChange(char before, char after)
{
    const char normalizedBefore = static_cast<char>(std::toupper(static_cast<unsigned char>(before)));
    const char normalizedAfter = static_cast<char>(std::toupper(static_cast<unsigned char>(after)));
    if (normalizedBefore == normalizedAfter)
    {
        return;
    }
    const auto markerChanged = [&](auto&& predicate)
    {
        return predicate(normalizedBefore) || predicate(normalizedAfter);
    };

    const bool enemyChanged = markerChanged(IsEnemyMarker);
    const bool batteryChanged = markerChanged(IsBatteryMarker);
    const bool logChanged = markerChanged(IsLogMarker);
    const bool jumpPadChanged = markerChanged(IsJumpPadMarker);
    const bool fallingRockChanged = markerChanged(IsFallingRockMarker);
    const bool markerLightChanged = markerChanged(IsMarkerLightMarker);
    const bool stageLightChanged = markerChanged(IsStageLightMarker);
    const bool linkedGimmickMarkerChanged =
        markerChanged(IsShutterOrLaserSwitchMarker) ||
        markerChanged(IsElevatorMarker) ||
        markerChanged(IsProtectiveWallMarker) ||
        markerChanged(IsBatteryGeneratorMarker);
    const bool laserTurretChanged = markerChanged(IsLaserTurretMarker);
    const bool damageFootholdChanged = markerChanged(IsDamageFootholdMarker);
	const bool sepiaRubbleChanged =
        markerChanged(IsSepiaRubbleMarker) ||
        markerChanged(IsSepiaBackgroundMarker);

    if (enemyChanged) RefreshEnemiesFromMarkers();
    if (batteryChanged) RefreshBatteriesFromMarkers();
    if (logChanged) RefreshLogsFromMarkers();
    if (jumpPadChanged) RefreshJumpPadsFromMarkers();
    if (fallingRockChanged) ReflashFallingRockfromMarkers();
    if (markerLightChanged) RefreshMarkerLightsFromMarkers();
    if (stageLightChanged) RefreshStageLightsFromMarkers();
    if (linkedGimmickMarkerChanged) RefreshLinkedGimmicksFromMarkers();
    if (laserTurretChanged) RefreshLaserTurretsFromMarkers();
    if (damageFootholdChanged) RefreshDamageFootholdsFromMarkers();
	if (sepiaRubbleChanged) RefleshSepiaRubblesFromMarkers();
}

void GameScene::RefreshEnemiesFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            if (entity->GetComponent<EnemyComponent>())
            {
                return true;
            }
            if (HasTag(*entity, "BossShield"))
            {
                return true;
            }
            return HasTag(*entity, kTagBullet);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            const float markerX = static_cast<float>(column) * tileSize;
            const float markerY = static_cast<float>(row) * tileSize;

            const auto placeEnemyAtMarker = [&](Entity& enemy)
            {
                if (auto* transform = enemy.GetComponent<TransformComponent>())
                {
                    transform->x = markerX + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(
                        transform->x,
                        transform->width * transform->scale,
                        transform->height * transform->scale,
                        spawnX,
                        spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
                    if (auto* enemyComponent = enemy.GetComponent<EnemyComponent>())
                    {
                        enemyComponent->spawnX = transform->x;
                        enemyComponent->spawnY = transform->y;
                        if (enemyComponent->GetArchetype() == EnemyArchetype::ShieldBoss)
                        {
                            enemyComponent->respawnEnabled = false;
                        }
                    }
                }
            };

            struct EnemySpawnRule
            {
                char marker;
                const char* prefabId;
                void (GameScene::*configure)(Entity&) = nullptr;
            };

            const std::array<EnemySpawnRule, 5> enemySpawnRules
            {
                EnemySpawnRule{ 'W', "sandbox_enemy_walker", &GameScene::ConfigureWalkerSpriteAnimation },
                EnemySpawnRule{ 'R', "sandbox_enemy_ranged", &GameScene::ConfigureRangedSpriteAnimation },
                EnemySpawnRule{ '$', "sandbox_enemy_charger", nullptr },
                EnemySpawnRule{ 'A', "sandbox_enemy_ghost", nullptr },
                EnemySpawnRule{ 'D', "sandbox_enemy_blaster_robot", nullptr },
            };

            const auto trySpawnRegularEnemy = [&](char spawnMarker) -> bool
            {
                for (const EnemySpawnRule& rule : enemySpawnRules)
                {
                    if (spawnMarker != rule.marker)
                    {
                        continue;
                    }

                    Entity& enemy = SpawnStagePrefab(prefabs, rule.prefabId, markerX, markerY);
                    if (rule.configure)
                    {
                        (this->*rule.configure)(enemy);
                    }
                    placeEnemyAtMarker(enemy);
                    return true;
                }

                return false;
            };

            const auto attachShieldToBoss = [&](Entity& boss)
            {
                auto* bossComp = boss.GetComponent<ShieldBossComponent>();
                auto* transform = boss.GetComponent<TransformComponent>();
                if (!bossComp || !transform || bossComp->shieldEntity)
                {
                    return;
                }
                constexpr float kShieldW = 48.0f;
                constexpr float kShieldH = 192.0f;
                constexpr float kShieldRaiseOffsetY = 24.0f;
                auto shieldEntity = std::make_unique<Entity>();
                shieldEntity->AddComponent<TagComponent>("BossShield");
                shieldEntity->AddComponent<TransformComponent>(
                    transform->x - kShieldW,
                    transform->y - kShieldRaiseOffsetY,
                    kShieldW,
                    kShieldH);
                shieldEntity->AddComponent<TintComponent>(
                    0.72f,
                    0.78f,
                    0.90f,
                    (!bossComp->combatStarted && !bossComp->appearAnimationFinished) ? 0.0f : 1.0f);
                shieldEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
                auto& shieldComp = shieldEntity->AddComponent<ShieldComponent>();
                shieldComp.attached = true;
                shieldComp.ownerBoss = &boss;
                shieldComp.contactDamage = 1;
                shieldComp.followOffsetX = -kShieldW;
                shieldComp.followOffsetY = 0.0f;
                bossComp->shieldEntity = shieldEntity.get();
                m_world.Spawn(std::move(shieldEntity));
            };

            if (trySpawnRegularEnemy(marker))
            {
                continue;
            }

            if (marker == 'N' || marker == '?')
            {
                if (m_flow.shieldBossDefeatedThisScene)
                {
                    continue;
                }
                Entity& boss = SpawnStagePrefab(prefabs, "sandbox_shield_boss", markerX, markerY);
                placeEnemyAtMarker(boss);
                attachShieldToBoss(boss);
            }
            else if (marker == '!')
            {
                Entity& boss = SpawnStagePrefab(prefabs, "sandbox_mid_boss2", markerX, markerY);
                placeEnemyAtMarker(boss);
            }
        }
    }
}

void GameScene::RefreshBatteriesFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagBattery);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'Y')
            {
                continue;
            }

            const char marker2 = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker2(column, row))));
            if(marker2 != '\0')
            {
                continue;
            }

            auto battery = std::make_unique<Entity>();
            battery->AddComponent<TagComponent>(kTagBattery);
            battery->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize,
                tileSize,
                tileSize);
            battery->AddComponent<TintComponent>(0.94f, 0.82f, 0.22f, 1.0f);
            battery->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            battery->AddComponent<BatteryComponent>(
                1900.0f,
                980.0f,
                260.0f,
                320.0f,
                1);
            if (m_lifecycle.darknessStageEnabled)
            {
                AddUnderBatteryGlow(*battery);
            }
            m_world.Spawn(std::move(battery));
        }
    }
}

void GameScene::RefreshLogsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagLog);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'M')
            {
                continue;
            }

            auto log = std::make_unique<Entity>();
            log->AddComponent<TagComponent>(kTagLog);
            log->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize,
                tileSize * 4.0f,
                tileSize);
            log->AddComponent<TintComponent>(0.54f, 0.34f, 0.16f, 1.0f);
            log->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            log->AddComponent<ImageOutlineColliderComponent>(
                std::vector<b2Vec2>{
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                    { 1.0f, 1.0f },
                    { 0.0f, 1.0f }},
                0.5f);
            log->AddComponent<BarrelComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                0.0f,
                0.0f,
                1,
                99999.0f,
                99999.0f);
            if (auto* barrel = log->GetComponent<BarrelComponent>())
            {
                barrel->active = true;
                barrel->respawnEnabled = false;
                barrel->respawnWhenOffscreen = false;
            }
            m_world.Spawn(std::move(log));
        }
    }
}

void GameScene::RefreshJumpPadsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagJumpPad);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    constexpr float kPi = 3.14159265f;
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'T')
            {
                continue;
            }

            auto jumpPad = std::make_unique<Entity>();
            jumpPad->AddComponent<TagComponent>(kTagJumpPad);
            jumpPad->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize,
                tileSize * 5.0f,
                tileSize * 2.0f);
            jumpPad->AddComponent<TintComponent>(0.0f, 0.0f, 1.0f, 1.0f);
            jumpPad->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            jumpPad->AddComponent<JumpPadComponent>(
                std::clamp(gJumpPadMaxTiltDegrees, 0.0f, 89.0f) * kPi / 180.0f,
                8.0f,
                5.5f,
                gPlayerJumpSpeed,
                1.8f,
                gPlayerJumpSpeed * 5.0f);
            m_world.Spawn(std::move(jumpPad));
        }
    }
}

void GameScene::RefreshMarkerLightsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagMarkerLight);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    constexpr float kDefaultRadiusTiles = 3.0f;
    constexpr float kIntensity = 1.0f;
    constexpr float kLightSizeRatio = 0.72f;
    constexpr float kRotationRadians = 0.78539816339f;

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'P' && marker != 'F')
            {
                continue;
            }

            const int markerParameter = m_tileMap.GetMarkerParameter(column, row);
            const int lightLinkId = markerParameter < 0 ? -markerParameter : -1;
            const float radiusTiles = markerParameter > 0
                ? static_cast<float>(markerParameter)
                : kDefaultRadiusTiles;
            const float objectSize = tileSize * kLightSizeRatio;
            const float objectOffset = (tileSize - objectSize) * 0.5f;

            auto light = std::make_unique<Entity>();
            light->AddComponent<TagComponent>(kTagMarkerLight);
            auto& transform = light->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize + objectOffset,
                static_cast<float>(row) * tileSize + objectOffset,
                objectSize,
                objectSize);
            transform.rotation = kRotationRadians;
            light->AddComponent<TintComponent>(1.0f, 0.92f, 0.24f, 1.0f);
            light->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            auto& markerLight = light->AddComponent<MarkerLightComponent>(
                tileSize * radiusTiles,
                kIntensity,
                lightLinkId);
            markerLight.activated = marker == 'P';
            m_world.Spawn(std::move(light));
        }
    }
}

void GameScene::RefreshStageLightsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagStageLight);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    constexpr float kFixtureHeightRatio = 0.58f;
    constexpr float kFixtureTopWidthRatio = 0.46f;
    constexpr float kBeamBottomWidthRatio = 1.16f;
    constexpr float kBeamFeatherTiles = 0.34f;
    constexpr float kIntensity = 1.0f;
    constexpr float kBeamGroundWidthLimitTiles = 6.0f;

    auto computeBeamLengthToGround = [&](float sourceX, float sourceY, float beamWidthWorld, float fallbackLengthWorld) -> float
    {
        const float tileSize = m_tileMap.GetTileSize();
        if (tileSize <= 0.0f || fallbackLengthWorld <= 0.0f || m_tileMap.GetWidth() <= 0 || m_tileMap.GetHeight() <= 0)
        {
            return fallbackLengthWorld;
        }

        const int startRow = std::clamp(
            static_cast<int>(sourceY / tileSize) + 1,
            0,
            m_tileMap.GetHeight() - 1);
        const float halfWidth = std::max(tileSize * 0.5f, beamWidthWorld * 0.5f);
        const int leftColumn = std::clamp(
            static_cast<int>((sourceX - halfWidth) / tileSize),
            0,
            m_tileMap.GetWidth() - 1);
        const int rightColumn = std::clamp(
            static_cast<int>(((sourceX + halfWidth) - 1.0f) / tileSize),
            0,
            m_tileMap.GetWidth() - 1);

        for (int column = leftColumn; column <= rightColumn; ++column)
        {
            for (int row = startRow; row < m_tileMap.GetHeight(); ++row)
            {
                if (!IsSolidTile(column, row) && !IsSlopeTile(column, row))
                {
                    continue;
                }

                float groundY = static_cast<float>(row) * tileSize;
                if (IsSlopeTile(column, row))
                {
                    float slopeSurfaceY = 0.0f;
                    if (GetSlopeSurfaceY(column, row, sourceX, slopeSurfaceY))
                    {
                        groundY = slopeSurfaceY;
                    }
                }

                return std::max(0.0f, groundY - sourceY);
            }
        }

        return fallbackLengthWorld;
    };

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (!IsStageLightMarker(m_tileMap.GetMarker(column, row)))
            {
                continue;
            }

            const StageLightSizing sizing = ResolveStageLightSizing(m_tileMap.GetMarkerParameter(column, row));
            const float objectWidth = tileSize * sizing.fixtureTiles;
            const float objectHeight = tileSize * kFixtureHeightRatio;
            const float objectX = static_cast<float>(column) * tileSize + (tileSize - objectWidth) * 0.5f;
            const float objectY = static_cast<float>(row) * tileSize + (tileSize - objectHeight) * 0.5f;
            const float beamSourceX = objectX + objectWidth * 0.5f;
            const float beamSourceY = objectY + objectHeight;
            const float fallbackLength = tileSize * sizing.lightTiles;
            const float beamLength = computeBeamLengthToGround(beamSourceX, beamSourceY, objectWidth, fallbackLength);

            auto stageLight = std::make_unique<Entity>();
            stageLight->AddComponent<TagComponent>(kTagStageLight);
            stageLight->AddComponent<TransformComponent>(objectX, objectY, objectWidth, objectHeight);
            stageLight->AddComponent<TintComponent>(1.0f, 0.88f, 0.30f, 1.0f);
            stageLight->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            stageLight->AddComponent<StageLightComponent>(
                true,
                kFixtureTopWidthRatio,
                beamLength,
                objectWidth,
                std::min(
                    tileSize * sizing.lightTiles * kBeamBottomWidthRatio,
                    tileSize * kBeamGroundWidthLimitTiles),
                tileSize * kBeamFeatherTiles,
                1.0f,
                0.88f,
                0.30f,
                kIntensity);
            m_world.Spawn(std::move(stageLight));
        }
    }
}

void GameScene::RefreshLaserTurretsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }

            return HasTag(*entity, kTagLaserTurret) || HasTag(*entity, kTagLaserBeam);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'U' && marker != 'Z')
            {
                continue;
            }

            const int markerParameter = m_tileMap.GetMarkerParameter(column, row);
            const bool vertical = marker == 'Z';
            const bool shootsLeft = marker == 'U' && markerParameter < 0;
            const bool requiresLaserPower = vertical
                ? markerParameter > 0
                : (markerParameter > 0 || markerParameter <= -2);
            const float turretX = static_cast<float>(column) * tileSize;
            const float turretY = static_cast<float>(row) * tileSize;
            const float turretWidth = vertical ? tileSize : tileSize * 3.0f;
            const float turretHeight = vertical ? tileSize * 3.0f : tileSize;
            const float beamThickness = tileSize * 0.2f;

            auto turret = std::make_unique<Entity>();
            turret->AddComponent<TagComponent>(kTagLaserTurret);
            turret->AddComponent<TransformComponent>(turretX, turretY, turretWidth, turretHeight);
            turret->AddComponent<TintComponent>(0.40f, 0.44f, 0.50f, 1.0f);
            turret->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            auto& turretComponent = turret->AddComponent<LaserTurretComponent>(
                beamThickness,
                1.0f,
                vertical,
                shootsLeft,
                requiresLaserPower);
            turretComponent.fireDirection = vertical
                ? LaserTurretFireDirection::Down
                : (shootsLeft ? LaserTurretFireDirection::Left : LaserTurretFireDirection::Right);
            turretComponent.vertical = vertical;
            turretComponent.shootsLeft = shootsLeft;
            turretComponent.fireToLeft = shootsLeft;
            m_world.Spawn(std::move(turret));

            auto beam = std::make_unique<Entity>();
            Entity* beamEntity = beam.get();
            beam->AddComponent<TagComponent>(kTagLaserBeam);
            beam->AddComponent<TransformComponent>(
                vertical ? turretX + (turretWidth - beamThickness) * 0.5f : (shootsLeft ? turretX : turretX + turretWidth),
                vertical ? turretY + turretHeight : turretY + (turretHeight - beamThickness) * 0.5f,
                vertical ? beamThickness : 0.0f,
                vertical ? 0.0f : beamThickness);
            beam->AddComponent<TintComponent>(1.0f, 0.24f, 0.24f, 0.86f);
            beam->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            beam->AddComponent<LaserBeamComponent>();
            turretComponent.beamEntity = beamEntity;
            turretComponent.beamOriginOffsetX = vertical ? (turretWidth - beamThickness) * 0.5f : (shootsLeft ? 0.0f : turretWidth);
            turretComponent.beamOriginOffsetY = vertical ? turretHeight : turretHeight * 0.5f;
            m_world.Spawn(std::move(beam));
        }
    }
}

void GameScene::RefreshLinkedGimmicksFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return entity->GetComponent<BatterySwitchComponent>() != nullptr ||
                entity->GetComponent<BatteryGeneratorComponent>() != nullptr ||
                entity->GetComponent<ElevatorComponent>() != nullptr ||
                entity->GetComponent<LaserSwitchComponent>() != nullptr ||
                entity->GetComponent<ShutterComponent>() != nullptr ||
                entity->GetComponent<ProtectiveWallComponent>() != nullptr;
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    std::vector<SwitchMarker> switchMarkers;
    std::vector<ElevatorMarker> elevatorMarkers;
    std::vector<LaserSwitchMarker> laserSwitchMarkers;
    std::vector<ShutterMarker> shutterMarkers;
    std::vector<ProtectiveWallMarker> protectiveWallMarkers;
    std::vector<BatteryGeneratorMarker> batteryGeneratorMarkers;
    CollectLinkedGimmickMarkers(
        m_tileMap,
        tileSize,
        switchMarkers,
        elevatorMarkers,
        laserSwitchMarkers,
        shutterMarkers,
        protectiveWallMarkers,
        batteryGeneratorMarkers);
    for (int index = 0; index < static_cast<int>(switchMarkers.size()); ++index)
    {
        const SwitchMarker& marker = switchMarkers[static_cast<size_t>(index)];
        SpawnBatterySwitchMarker(
            marker.x,
            marker.y,
            marker.requiredBatteryCount,
            marker.controlsLaserPower,
            index,
            tileSize,
            marker.pressMode);
    }

    for (int index = 0; index < static_cast<int>(elevatorMarkers.size()); ++index)
    {
        const ElevatorMarker& marker = elevatorMarkers[static_cast<size_t>(index)];
        SpawnElevatorMarker(
            marker.x,
            marker.y,
            marker.moveRangeTiles,
            marker.widthTiles,
            index,
            tileSize);
    }

    std::vector<int> batteryGeneratorSwitchLinkIds;
    batteryGeneratorSwitchLinkIds.reserve(switchMarkers.size());
    for (int index = 0; index < static_cast<int>(switchMarkers.size()); ++index)
    {
        const SwitchMarker& marker = switchMarkers[static_cast<size_t>(index)];
        if (!marker.controlsLaserPower)
        {
            batteryGeneratorSwitchLinkIds.push_back(index);
        }
    }

    for (int index = 0; index < static_cast<int>(batteryGeneratorMarkers.size()); ++index)
    {
        const BatteryGeneratorMarker& marker = batteryGeneratorMarkers[static_cast<size_t>(index)];
        const int linkId = index < static_cast<int>(batteryGeneratorSwitchLinkIds.size())
            ? batteryGeneratorSwitchLinkIds[static_cast<size_t>(index)]
            : index;
        SpawnBatteryGeneratorMarker(
            marker.x,
            marker.y,
            linkId,
            marker.spawnDirectionX,
            tileSize);
    }

    for (int index = 0; index < static_cast<int>(laserSwitchMarkers.size()); ++index)
    {
        const LaserSwitchMarker& marker = laserSwitchMarkers[static_cast<size_t>(index)];
        const int linkId = marker.linkIdOverride >= 0 ? marker.linkIdOverride : index;
        SpawnLaserSwitchMarker(marker.x, marker.y, linkId, tileSize);
    }

    const std::vector<int> laserSwitchLinkIds = BuildLaserSwitchLinkIds(laserSwitchMarkers);

    for (int index = 0; index < static_cast<int>(shutterMarkers.size()); ++index)
    {
        const ShutterMarker& marker = shutterMarkers[static_cast<size_t>(index)];
        const int linkId = ResolveShutterLinkId(
            marker,
            index,
            laserSwitchMarkers,
            laserSwitchLinkIds);
        SpawnShutterMarker(
            marker.x,
            marker.y,
            marker.moveRangeTiles,
            linkId,
            marker.useBossDefeatSignal,
            marker.opensWhenUnpowered,
            tileSize);
    }

    for (int index = 0; index < static_cast<int>(protectiveWallMarkers.size()); ++index)
    {
        const ProtectiveWallMarker& marker = protectiveWallMarkers[static_cast<size_t>(index)];
        const int effectiveLinkId = marker.linkIdOverride >= 0 ? marker.linkIdOverride : index;
        SpawnProtectiveWallMarker(
            marker.x,
            marker.y,
            marker.durability,
            effectiveLinkId,
            marker.widthTiles,
            marker.markerHeightTiles,
            marker.heightTiles,
            tileSize);
    }
}

void GameScene::RefreshProtectiveWallsFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return entity->GetComponent<ProtectiveWallComponent>() != nullptr;
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    std::vector<ProtectiveWallMarker> protectiveWallMarkers;
    std::vector<SwitchMarker> switchMarkers;
    std::vector<ElevatorMarker> elevatorMarkers;
    std::vector<LaserSwitchMarker> laserSwitchMarkers;
    std::vector<ShutterMarker> shutterMarkers;
    std::vector<BatteryGeneratorMarker> batteryGeneratorMarkers;
    CollectLinkedGimmickMarkers(
        m_tileMap,
        tileSize,
        switchMarkers,
        elevatorMarkers,
        laserSwitchMarkers,
        shutterMarkers,
        protectiveWallMarkers,
        batteryGeneratorMarkers);

    for (int index = 0; index < static_cast<int>(protectiveWallMarkers.size()); ++index)
    {
        const ProtectiveWallMarker& marker = protectiveWallMarkers[static_cast<size_t>(index)];
        const int effectiveLinkId = marker.linkIdOverride >= 0 ? marker.linkIdOverride : index;
        SpawnProtectiveWallMarker(
            marker.x,
            marker.y,
            marker.durability,
            effectiveLinkId,
            marker.widthTiles,
            marker.markerHeightTiles,
            marker.heightTiles,
            tileSize);
    }
}

void GameScene::RefreshDamageFootholdsFromMarkers()
{
    const int purpleTexture = m_assets.GetTexture("tile_value_3_purple");
    const int damageTexture = purpleTexture >= 0 ? purpleTexture : m_whiteTexture;
    const int damageSpikeTexture = m_assets.GetTexture("tile_value_h_damage");
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }

            return HasTag(*entity, kTagDamagePlatform) || HasTag(*entity, kTagDamagePlatformSpike);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (!IsDamagePlatformMarker(marker))
            {
                continue;
            }

            const float markerX = static_cast<float>(column) * tileSize;
            const float markerY = static_cast<float>(row) * tileSize;
            const int tileSpan = GetDamagePlatformTileSpanFromMarker(marker);
            for (int cellIndex = 0; cellIndex < tileSpan; ++cellIndex)
            {
                const float cellX = markerX + tileSize * static_cast<float>(cellIndex);

                auto damagePlatformBase = std::make_unique<Entity>();
                damagePlatformBase->AddComponent<TagComponent>(kTagDamagePlatform);
                damagePlatformBase->AddComponent<TransformComponent>(
                    cellX,
                    markerY + tileSize,
                    tileSize,
                    tileSize);
                damagePlatformBase->AddComponent<TintComponent>(0.66f, 0.12f, 0.94f, 1.0f);
                damagePlatformBase->AddComponent<SpriteRenderComponent>(damageTexture);
                damagePlatformBase->AddComponent<DamagePlatformComponent>(1);
                damagePlatformBase->AddComponent<ImageOutlineColliderComponent>(
                    std::vector<b2Vec2>{
                        { 0.0f, 0.0f },
                        { 1.0f, 0.0f },
                        { 1.0f, 1.0f },
                        { 0.0f, 1.0f }},
                    0.2f);
                damagePlatformBase->AddComponent<VanishOnCaptureComponent>(true);
                m_world.Spawn(std::move(damagePlatformBase));

                auto damagePlatformSpike = std::make_unique<Entity>();
                damagePlatformSpike->AddComponent<TagComponent>(kTagDamagePlatformSpike);
                damagePlatformSpike->AddComponent<TransformComponent>(
                    cellX,
                    markerY,
                    tileSize,
                    tileSize);
                damagePlatformSpike->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
                damagePlatformSpike->AddComponent<SpriteRenderComponent>(
                    damageSpikeTexture >= 0 ? damageSpikeTexture : damageTexture);
                damagePlatformSpike->AddComponent<GimmickComponent>(GimmickType::Hazard, true, false);
                damagePlatformSpike->AddComponent<SpikeStripComponent>(1);
                damagePlatformSpike->AddComponent<ImageOutlineColliderComponent>(
                    BuildDamagePlatformNormalizedOutline(1),
                    0.2f);
                damagePlatformSpike->AddComponent<VanishOnCaptureComponent>(true);
                m_world.Spawn(std::move(damagePlatformSpike));
            }
        }
    }
}

void GameScene::RefleshSepiaRubblesFromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }

            return HasTag(*entity, kTagSepiaRubble);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    const int mapWidth = m_tileMap.GetWidth();
    const int mapHeight = m_tileMap.GetHeight();

    if (mapWidth <= 0 || mapHeight <= 0)
    {
        return;
    }

    std::vector<bool> visited(
        static_cast<size_t>(mapWidth * mapHeight),
        false);

    const auto toIndex = [mapWidth](int column, int row) -> size_t
        {
            return static_cast<size_t>(row * mapWidth + column);
        };

    constexpr int kNeighborOffsets[4][2] =
    {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 },
    };

    for (int row = 0; row < mapHeight; ++row)
    {
        for (int column = 0; column < mapWidth; ++column)
        {
            const size_t startIndex = toIndex(column, row);
            if (visited[startIndex])
            {
                continue;
            }

            SepiaMarkerCell startCell;
            if (!TryGetSepiaMarkerCell(
                m_tileMap,
                column,
                row,
                startCell))
            {
                visited[startIndex] = true;
                continue;
            }

            const char targetMarker = m_tileMap.GetMarker(column, row);
            const int targetImageNo = startCell.imageNo;
            const int restoredTileValue = startCell.restoredTileValue;
            const char targetRestoredMarkerType = startCell.restoredMarkerType;
            const int targetRestoredMarkerParameter = startCell.restoredMarkerParameter;

            int minColumn = column;
            int minRow = row;
            int maxColumn = column;
            int maxRow = row;

            std::vector<SepiaMarkerCell> groupCells;
            std::vector<SepiaMarkerCell> stack;

            visited[startIndex] = true;
            stack.push_back(startCell);

            while (!stack.empty())
            {
                const SepiaMarkerCell current = stack.back();
                stack.pop_back();

                groupCells.push_back(current);

                minColumn = std::min(minColumn, current.column);
                minRow = std::min(minRow, current.row);
                maxColumn = std::max(maxColumn, current.column);
                maxRow = std::max(maxRow, current.row);

                for (const auto& offset : kNeighborOffsets)
                {
                    const int nextColumn = current.column + offset[0];
                    const int nextRow = current.row + offset[1];

                    if (nextColumn < 0 ||
                        nextRow < 0 ||
                        nextColumn >= mapWidth ||
                        nextRow >= mapHeight)
                    {
                        continue;
                    }

                    const char nextMarker = m_tileMap.GetMarker(nextColumn, nextRow);
                    const size_t nextIndex = toIndex(nextColumn, nextRow);
                    if (visited[nextIndex])
                    {
                        continue;
                    }

                    SepiaMarkerCell nextCell;
                    if (!TryGetSepiaMarkerCell(
                        m_tileMap,
                        nextColumn,
                        nextRow,
                        nextCell))
                    {
                        visited[nextIndex] = true;
                        continue;
                    }

                    if ((nextCell.imageNo != targetImageNo) ||
                        (nextCell.restoredMarkerType != targetRestoredMarkerType) ||
                        (nextCell.restoredMarkerParameter != targetRestoredMarkerParameter) ||
                        (nextMarker != targetMarker))
                    {
                        continue;
                    }
                    visited[nextIndex] = true;
                    stack.push_back(nextCell);
                }
            }

            if (groupCells.empty())
            {
                continue;
            }

            int groupMinColumn = minColumn;
            int groupMinRow = minRow;
            int groupMaxColumn = maxColumn;
            int groupMaxRow = maxRow;

            float groupWidthTiles =
                static_cast<float>(groupMaxColumn - groupMinColumn + 1);
            float groupHeightTiles =
                static_cast<float>(groupMaxRow - groupMinRow + 1);

            SepiaGroupSizing fixedSizing;
            if (TryResolveSepiaTileSizing(targetMarker, restoredTileValue, fixedSizing) ||
                (targetRestoredMarkerType != '\0' &&
                    TryResolveSepiaGroupSizing(targetMarker, targetRestoredMarkerType, fixedSizing)))
            {
                groupWidthTiles = fixedSizing.widthTiles;
                groupHeightTiles = fixedSizing.heightTiles;

                int fixedWidthTileCount = static_cast<int>(fixedSizing.widthTiles);
                if (static_cast<float>(fixedWidthTileCount) < fixedSizing.widthTiles)
                {
                    ++fixedWidthTileCount;
                }
                fixedWidthTileCount = (std::max)(1, fixedWidthTileCount);

                int fixedHeightTileCount = static_cast<int>(fixedSizing.heightTiles);
                if (static_cast<float>(fixedHeightTileCount) < fixedSizing.heightTiles)
                {
                    ++fixedHeightTileCount;
                }
                fixedHeightTileCount = (std::max)(1, fixedHeightTileCount);

                groupMaxColumn = groupMinColumn + fixedWidthTileCount - 1;
                groupMaxRow = groupMinRow + fixedHeightTileCount - 1;
            }

            float groupX = static_cast<float>(groupMinColumn) * tileSize;
            float groupY = static_cast<float>(groupMinRow) * tileSize;
            float groupWidth = groupWidthTiles * tileSize;
            float groupHeight = groupHeightTiles * tileSize;
            const int sepiaRubbleTextureId = m_assets.GetTexture("sepia_rubble");

            auto rubble = std::make_unique<Entity>();
            rubble->AddComponent<TagComponent>(kTagSepiaRubble);

            // 外接矩形サイズでTransformを作る（すでに groupX/Y/Width/Height は計算済み）
            rubble->AddComponent<TransformComponent>(groupX, groupY, groupWidth, groupHeight);

            rubble->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
            rubble->AddComponent<SpriteRenderComponent>(m_assets.GetTexture("sepia_rubble"));
            rubble->AddComponent<SepiaRubbleComponent>();


            auto& groupComp = rubble->AddComponent<SepiaRubbleGroupComponent>(
                targetMarker,
                targetImageNo,          // startCell.imageNo と同じ
                restoredTileValue,      // startCell.restoredTileValue
                targetRestoredMarkerType,
                targetRestoredMarkerParameter,
                groupMinColumn, groupMinRow, groupMaxColumn, groupMaxRow,
                false);

            groupComp.cellColumns.reserve(groupCells.size());
            groupComp.cellRows.reserve(groupCells.size());
            groupComp.cellRestoredTileValues.reserve(groupCells.size());
            groupComp.cellRestoredMarkerTypes.reserve(groupCells.size());
            groupComp.cellRestoredMarkerParameters.reserve(groupCells.size());
            for (const SepiaMarkerCell& cell : groupCells)
            {
                groupComp.cellColumns.push_back(cell.column);
                groupComp.cellRows.push_back(cell.row);
                groupComp.cellRestoredTileValues.push_back(cell.restoredTileValue);
                groupComp.cellRestoredMarkerTypes.push_back(cell.restoredMarkerType);
                groupComp.cellRestoredMarkerParameters.push_back(cell.restoredMarkerParameter);
            }

            m_world.Spawn(std::move(rubble));
        }
    }
}

void GameScene::ReflashFallingRockfromMarkers()
{
    m_world.EraseIf(
        [](const std::unique_ptr<Entity>& entity)
        {
            if (!entity)
            {
                return true;
            }
            return HasTag(*entity, kTagFallingRock);
        });

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            if (marker != 'S')
            {
                continue;
            }

            auto fallingRock = std::make_unique<Entity>();
            fallingRock->AddComponent<TagComponent>(kTagFallingRock);
            fallingRock->AddComponent<TransformComponent>(
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize,
                tileSize * 2.0f,
                tileSize * 2.0f);
            fallingRock->AddComponent<TintComponent>(0.6f, 0.6f, 0.85f, 1.0f);
            fallingRock->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            fallingRock->AddComponent<ImageOutlineColliderComponent>(
                std::vector<b2Vec2>{
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                    { 1.0f, 1.0f },
                    { 0.0f, 1.0f }},
                0.5f);
            fallingRock->AddComponent<FallingRockComponent>(
                gBarrelGravity,
                gBarrelMaxFallSpeed,
                gBarrelRollSpeed,
                gBarrelGroundFriction,
                gBarrelContactDamage,
                gBarrelBreakMinFallDistance,
                gBarrelBreakMinImpactSpeed);
            if (auto* barrel = fallingRock->GetComponent<FallingRockComponent>())
            {
                barrel->active = false;
                barrel->respawnEnabled = false;
                barrel->respawnWhenOffscreen = false;
                barrel->rubbleActive = false;
            }
            m_world.Spawn(std::move(fallingRock));
        }
    }
}
