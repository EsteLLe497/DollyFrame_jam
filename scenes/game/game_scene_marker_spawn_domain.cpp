#include "game_scene_internal.h"

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

    struct SwitchMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int requiredBatteryCount = 1;
        bool controlsLaserPower = false;
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
        int durability = 3;
        int widthTiles = 1;
        int markerHeightTiles = 1;
        int heightTiles = 3;
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
        LinkedGimmickColor batterySwitchColor{ 0.92f, 0.26f, 0.20f };
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
        std::vector<ProtectiveWallMarker>& outProtectiveWallMarkers)
    {
        outSwitchMarkers.clear();
        outElevatorMarkers.clear();
        outLaserSwitchMarkers.clear();
        outShutterMarkers.clear();
        outProtectiveWallMarkers.clear();

        for (int row = 0; row < tileMap.GetHeight(); ++row)
        {
            for (int column = 0; column < tileMap.GetWidth(); ++column)
            {
                const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(tileMap.GetMarker(column, row))));
                const float markerX = static_cast<float>(column) * tileSize;
                const float markerY = static_cast<float>(row) * tileSize;
                if (marker == 'K')
                {
                    outSwitchMarkers.push_back(SwitchMarker{
                        markerX,
                        markerY + tileSize * 0.5f,
                        (std::max)(1, tileMap.GetMarkerParameter(column, row)),
                        false });
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
                        true });
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
                else if (marker == '@')
                {
                    if ((column > 0 && tileMap.GetMarker(column - 1, row) == '@') ||
                        (row > 0 && tileMap.GetMarker(column, row - 1) == '@'))
                    {
                        continue;
                    }

                    int widthTiles = 1;
                    while (column + widthTiles < tileMap.GetWidth() &&
                        tileMap.GetMarker(column + widthTiles, row) == '@')
                    {
                        ++widthTiles;
                    }

                    int heightTiles = 1;
                    bool rowMatchesWallWidth = true;
                    while (row + heightTiles < tileMap.GetHeight() && rowMatchesWallWidth)
                    {
                        for (int wallColumnOffset = 0; wallColumnOffset < widthTiles; ++wallColumnOffset)
                        {
                            if (tileMap.GetMarker(column + wallColumnOffset, row + heightTiles) != '@')
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
                    const int wallHeightTiles = heightTiles > 1 ? heightTiles : 4;
                    outProtectiveWallMarkers.push_back(ProtectiveWallMarker{
                        markerX,
                        markerY,
                        markerParameter > 0 ? markerParameter : 3,
                        widthTiles,
                        heightTiles,
                        wallHeightTiles });
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
}

void GameScene::RefreshMarkerDrivenSystems()
{
    RefreshEnemiesFromMarkers();
    RefreshBatteriesFromMarkers();
    RefreshLogsFromMarkers();
    RefreshMarkerLightsFromMarkers();
    RefreshStageLightsFromMarkers();
    RefreshLaserTurretsFromMarkers();
    RefreshLinkedGimmicksFromMarkers();
    RefreshDamageFootholdsFromMarkers();
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
    const bool markerLightChanged = markerChanged(IsMarkerLightMarker);
    const bool stageLightChanged = markerChanged(IsStageLightMarker);
    const bool linkedGimmickMarkerChanged =
        markerChanged(IsShutterOrLaserSwitchMarker) ||
        markerChanged(IsElevatorMarker) ||
        markerChanged(IsProtectiveWallMarker);
    const bool laserTurretChanged = markerChanged(IsLaserTurretMarker);
    const bool damageFootholdChanged = markerChanged(IsDamageFootholdMarker);

    if (enemyChanged) RefreshEnemiesFromMarkers();
    if (batteryChanged) RefreshBatteriesFromMarkers();
    if (logChanged) RefreshLogsFromMarkers();
    if (markerLightChanged) RefreshMarkerLightsFromMarkers();
    if (stageLightChanged) RefreshStageLightsFromMarkers();
    if (linkedGimmickMarkerChanged) RefreshLinkedGimmicksFromMarkers();
    if (laserTurretChanged) RefreshLaserTurretsFromMarkers();
    if (damageFootholdChanged) RefreshDamageFootholdsFromMarkers();
}

void GameScene::RefreshEnemiesFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
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
            }),
        m_entities.end());

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
                    }
                }
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
                constexpr float kShieldH = 144.0f;
                auto shieldEntity = std::make_unique<Entity>();
                shieldEntity->AddComponent<TagComponent>("BossShield");
                shieldEntity->AddComponent<TransformComponent>(
                    transform->x - kShieldW,
                    transform->y,
                    kShieldW,
                    kShieldH);
                shieldEntity->AddComponent<TintComponent>(0.72f, 0.78f, 0.90f, 1.0f);
                shieldEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
                auto& shieldComp = shieldEntity->AddComponent<ShieldComponent>();
                shieldComp.attached = true;
                shieldComp.ownerBoss = &boss;
                shieldComp.contactDamage = 1;
                shieldComp.followOffsetX = -kShieldW;
                shieldComp.followOffsetY = 0.0f;
                bossComp->shieldEntity = shieldEntity.get();
                m_entities.push_back(std::move(shieldEntity));
            };

            if (marker == 'W')
            {
                Entity& enemy = SpawnStagePrefab(prefabs, "sandbox_enemy_walker", markerX, markerY);
                placeEnemyAtMarker(enemy);
            }
            else if (marker == 'R')
            {
                Entity& enemy = SpawnStagePrefab(prefabs, "sandbox_enemy_ranged", markerX, markerY);
                placeEnemyAtMarker(enemy);
            }
            else if (marker == 'N' || marker == '?')
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
			else if (marker == 'A')
            {
                Entity& enemy = SpawnStagePrefab(prefabs, "sandbox_enemy_ghost", markerX, markerY);
                placeEnemyAtMarker(enemy);
            }
            else if (marker == 'D')
            {
                Entity& enemy = SpawnStagePrefab(prefabs, "sandbox_enemy_blaster_robot", markerX, markerY);
                placeEnemyAtMarker(enemy);
            }
        }
    }
}

void GameScene::RefreshBatteriesFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }
                return HasTag(*entity, kTagBattery);
            }),
        m_entities.end());

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
            if (m_darknessStageEnabled)
            {
                AddUnderBatteryGlow(*battery);
            }
            m_entities.push_back(std::move(battery));
        }
    }
}

void GameScene::RefreshLogsFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }
                return HasTag(*entity, kTagLog);
            }),
        m_entities.end());

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
            m_entities.push_back(std::move(log));
        }
    }
}

void GameScene::RefreshMarkerLightsFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }
                return HasTag(*entity, kTagMarkerLight);
            }),
        m_entities.end());

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
                kIntensity);
            markerLight.activated = marker == 'F';
            m_entities.push_back(std::move(light));
        }
    }
}

void GameScene::RefreshStageLightsFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }
                return HasTag(*entity, kTagStageLight);
            }),
        m_entities.end());

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
            const float beamLength = tileSize * sizing.lightTiles;

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
                tileSize * sizing.lightTiles * kBeamBottomWidthRatio,
                tileSize * kBeamFeatherTiles,
                1.0f,
                0.88f,
                0.30f,
                kIntensity);
            m_entities.push_back(std::move(stageLight));
        }
    }
}

void GameScene::RefreshLaserTurretsFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }

                return HasTag(*entity, kTagLaserTurret) || HasTag(*entity, kTagLaserBeam);
            }),
        m_entities.end());

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
            m_entities.push_back(std::move(turret));

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
            m_entities.push_back(std::move(beam));
        }
    }
}

void GameScene::RefreshLinkedGimmicksFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }
                return entity->GetComponent<BatterySwitchComponent>() != nullptr ||
                    entity->GetComponent<ElevatorComponent>() != nullptr ||
                    entity->GetComponent<LaserSwitchComponent>() != nullptr ||
                    entity->GetComponent<ShutterComponent>() != nullptr ||
                    entity->GetComponent<ProtectiveWallComponent>() != nullptr;
            }),
        m_entities.end());

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }
    const LinkedGimmickSpawnConfig& cfg = kLinkedGimmickSpawnConfig;

    std::vector<SwitchMarker> switchMarkers;
    std::vector<ElevatorMarker> elevatorMarkers;
    std::vector<LaserSwitchMarker> laserSwitchMarkers;
    std::vector<ShutterMarker> shutterMarkers;
    std::vector<ProtectiveWallMarker> protectiveWallMarkers;
    CollectLinkedGimmickMarkers(
        m_tileMap,
        tileSize,
        switchMarkers,
        elevatorMarkers,
        laserSwitchMarkers,
        shutterMarkers,
        protectiveWallMarkers);

    const auto spawnBatterySwitch = [&](const SwitchMarker& marker, int linkId)
    {
        auto switchEntity = std::make_unique<Entity>();
        switchEntity->AddComponent<TagComponent>(kTagBatterySwitch);
        switchEntity->AddComponent<TransformComponent>(
            marker.x,
            marker.y,
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
            marker.requiredBatteryCount,
            tileSize * cfg.batterySwitchPressDepthRatio,
            cfg.batterySwitchPressSpeed,
            cfg.batterySwitchReleaseSpeed,
            marker.controlsLaserPower);
        m_entities.push_back(std::move(switchEntity));
    };

    const auto spawnElevator = [&](const ElevatorMarker& marker, int linkId)
    {
        auto elevatorEntity = std::make_unique<Entity>();
        elevatorEntity->AddComponent<TagComponent>(kTagElevator);
        elevatorEntity->AddComponent<TransformComponent>(
            marker.x,
            marker.y,
            tileSize * marker.widthTiles,
            tileSize * cfg.elevatorHeightTiles);
        elevatorEntity->AddComponent<TintComponent>(
            cfg.elevatorColor.r,
            cfg.elevatorColor.g,
            cfg.elevatorColor.b,
            1.0f);
        elevatorEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        elevatorEntity->AddComponent<ElevatorComponent>(
            linkId,
            tileSize * static_cast<float>(marker.moveRangeTiles),
            tileSize * cfg.elevatorSpeedTilesPerSec,
            cfg.elevatorTopPauseSeconds);
        m_entities.push_back(std::move(elevatorEntity));
    };

    const auto spawnLaserSwitch = [&](const LaserSwitchMarker& marker, int linkId)
    {
        auto switchEntity = std::make_unique<Entity>();
        switchEntity->AddComponent<TagComponent>(kTagLaserSwitch);
        switchEntity->AddComponent<TransformComponent>(
            marker.x,
            marker.y,
            tileSize * cfg.laserSwitchWidthTiles,
            tileSize * cfg.laserSwitchHeightTiles);
        switchEntity->AddComponent<TintComponent>(
            cfg.laserSwitchColor.r,
            cfg.laserSwitchColor.g,
            cfg.laserSwitchColor.b,
            1.0f);
        switchEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
        switchEntity->AddComponent<LaserSwitchComponent>(linkId);
        m_entities.push_back(std::move(switchEntity));
    };

    const auto spawnShutter = [&](const ShutterMarker& marker, int linkId)
    {
        const int moveRangeTiles = (std::max)(1, marker.moveRangeTiles);
        auto shutterEntity = std::make_unique<Entity>();
        shutterEntity->AddComponent<TagComponent>(kTagShutter);
        shutterEntity->AddComponent<TransformComponent>(
            marker.x,
            marker.y,
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
            tileSize * static_cast<float>(moveRangeTiles),
            tileSize * cfg.shutterSpeedTilesPerSec,
            marker.useBossDefeatSignal,
            marker.opensWhenUnpowered);
        m_entities.push_back(std::move(shutterEntity));
    };

    const auto spawnProtectiveWall = [&](const ProtectiveWallMarker& marker, int linkId)
    {
        const float markerWidth = tileSize * static_cast<float>((std::max)(1, marker.widthTiles));
        const float markerHeight = tileSize * static_cast<float>((std::max)(1, marker.markerHeightTiles));
        const float wallWidth = tileSize * static_cast<float>((std::max)(1, marker.widthTiles));
        const float wallHeight = tileSize * static_cast<float>((std::max)(1, marker.heightTiles));
        const float wallX = marker.x + markerWidth * 0.5f - wallWidth * 0.5f;
        const float wallY = marker.y + markerHeight * 0.5f - wallHeight * 0.5f;
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
            marker.durability,
            wallHeight,
            tileSize * cfg.protectiveWallSpeedTilesPerSec,
            false);
        m_entities.push_back(std::move(wallEntity));
    };

    for (int index = 0; index < static_cast<int>(switchMarkers.size()); ++index)
    {
        spawnBatterySwitch(switchMarkers[static_cast<size_t>(index)], index);
    }

    for (int index = 0; index < static_cast<int>(elevatorMarkers.size()); ++index)
    {
        spawnElevator(elevatorMarkers[static_cast<size_t>(index)], index);
    }

    for (int index = 0; index < static_cast<int>(laserSwitchMarkers.size()); ++index)
    {
        const LaserSwitchMarker& marker = laserSwitchMarkers[static_cast<size_t>(index)];
        const int linkId = marker.linkIdOverride >= 0 ? marker.linkIdOverride : index;
        spawnLaserSwitch(marker, linkId);
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
        spawnShutter(marker, linkId);
    }

    for (int index = 0; index < static_cast<int>(protectiveWallMarkers.size()); ++index)
    {
        spawnProtectiveWall(protectiveWallMarkers[static_cast<size_t>(index)], index);
    }
}

void GameScene::RefreshDamageFootholdsFromMarkers()
{
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](const std::unique_ptr<Entity>& entity)
            {
                if (!entity)
                {
                    return true;
                }

                return HasTag(*entity, kTagDamagePlatform) || HasTag(*entity, kTagDamagePlatformSpike);
            }),
        m_entities.end());

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

            auto damagePlatformBase = std::make_unique<Entity>();
            damagePlatformBase->AddComponent<TagComponent>(kTagDamagePlatform);
            damagePlatformBase->AddComponent<TransformComponent>(
                markerX,
                markerY + tileSize,
                tileSize * static_cast<float>(tileSpan),
                tileSize);
            damagePlatformBase->AddComponent<TintComponent>(0.66f, 0.12f, 0.94f, 1.0f);
            damagePlatformBase->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            damagePlatformBase->AddComponent<ImageOutlineColliderComponent>(
                std::vector<b2Vec2>{
                    { 0.0f, 0.0f },
                    { 1.0f, 0.0f },
                    { 1.0f, 1.0f },
                    { 0.0f, 1.0f }},
                0.2f);
            damagePlatformBase->AddComponent<VanishOnCaptureComponent>(true);
            m_entities.push_back(std::move(damagePlatformBase));

            auto damagePlatformSpike = std::make_unique<Entity>();
            damagePlatformSpike->AddComponent<TagComponent>(kTagDamagePlatformSpike);
            damagePlatformSpike->AddComponent<TransformComponent>(
                markerX,
                markerY,
                tileSize * static_cast<float>(tileSpan),
                tileSize);
            damagePlatformSpike->AddComponent<TintComponent>(0.86f, 0.16f, 0.18f, 1.0f);
            damagePlatformSpike->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            damagePlatformSpike->AddComponent<GimmickComponent>(GimmickType::Hazard, true, false);
            damagePlatformSpike->AddComponent<SpikeStripComponent>(tileSpan);
            damagePlatformSpike->AddComponent<VanishOnCaptureComponent>(true);
            m_entities.push_back(std::move(damagePlatformSpike));
        }
    }
}
