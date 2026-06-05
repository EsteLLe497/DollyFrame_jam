#include "pch.h"

#include "game_scene_internal.h"

#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sstream>

#include <nlohmann/json.hpp>

#include "prefab_factory.h"
#include "game_scene_camerawork.h"

using namespace game_scene_detail;

namespace
{
    constexpr const char* kStageTransitionCsvPath = "assets/maps/stage_transitions.csv";

    bool IsDarknessStageMapPath(const std::string& mapPath)
    {
        std::error_code ec;
        std::filesystem::path path(mapPath);
        std::string stem = path.stem().string();
        std::transform(
            stem.begin(),
            stem.end(),
            stem.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return stem == "under";
    }

    std::string Trim(const std::string& value)
    {
        const size_t start = value.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
        {
            return {};
        }
        const size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    std::vector<std::string> SplitCsvLine(const std::string& line)
    {
        std::vector<std::string> parts;
        std::stringstream stream(line);
        std::string cell;
        while (std::getline(stream, cell, ','))
        {
            parts.push_back(Trim(cell));
        }
        return parts;
    }

    bool IsEnemySpawnMarker(char marker)
    {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(marker)));
        return upper == 'W' || upper == 'R' || upper == 'N' || upper == '!' || upper == '?' || upper == '$' || upper == '%';
    }

    void LoadStageTransitionLinks()
    {
        gStageTransitionLinks.clear();

        std::ifstream stream(kStageTransitionCsvPath, std::ios::binary);
        if (!stream.is_open())
        {
            return;
        }

        std::string line;
        while (std::getline(stream, line))
        {
            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }

            const std::vector<std::string> cells = SplitCsvLine(trimmed);
            if (cells.size() < 3)
            {
                continue;
            }

            StageTransitionLink link;
            link.sourceMapCsv = cells[0];
            if (link.sourceMapCsv.empty())
            {
                link.sourceMapCsv = "*";
            }

            if (!cells[1].empty())
            {
                link.marker = static_cast<char>(std::toupper(static_cast<unsigned char>(cells[1][0])));
            }
            link.destinationMapCsv = cells[2];
            if (cells.size() >= 4 && !cells[3].empty())
            {
                link.spawnMarker = static_cast<char>(std::toupper(static_cast<unsigned char>(cells[3][0])));
            }

            if (link.marker == '\0' || link.destinationMapCsv.empty())
            {
                continue;
            }
            if (IsEnemySpawnMarker(link.marker))
            {
                std::ostringstream warning;
                warning
                    << "Stage transition marker '" << link.marker
                    << "' is reserved for enemy spawn. source='"
                    << link.sourceMapCsv
                    << "' destination='"
                    << link.destinationMapCsv
                    << "'";
                Logger::Warn(warning.str());
                continue;
            }

            gStageTransitionLinks.push_back(std::move(link));
        }
    }

    float AlignToGrid(float value, float gridSize)
    {
        return std::round(value / gridSize) * gridSize;
    }

    void SetEntityTint(Entity& entity, float r, float g, float b, float a = 1.0f)
    {
        if (auto* tint = entity.GetComponent<TintComponent>())
        {
            tint->r = r;
            tint->g = g;
            tint->b = b;
            tint->a = a;
        }
    }

    nlohmann::json BuildTuningJson()
    {
        nlohmann::json root;
        root["camera_view_width"] = gCameraViewWidth;
        root["camera_view_height"] = gCameraViewHeight;
        root["camera_follow_speed_x"] = gCameraFollowSpeedX;
        root["camera_follow_speed_y"] = gCameraFollowSpeedY;
        root["camera_follow_y"] = gCameraFollowY >= 0.5f;
        root["move_speed"] = gPlayerMoveSpeed;
        root["jump_speed"] = gPlayerJumpSpeed;
        root["gravity"] = gPlayerGravity;
        root["max_fall_speed"] = gPlayerMaxFallSpeed;
        root["dodge_speed"] = gPlayerDodgeSpeed;
        root["dodge_distance"] = gPlayerDodgeDistance;
        root["dodge_invincibility"] = gPlayerDodgeInvincibilitySeconds;
        root["dodge_cooldown"] = gPlayerDodgeCooldown;
        root["coyote_time"] = gCoyoteTimeSeconds;
        root["ground_snap_distance"] = gGroundSnapDistance;
        root["ground_step_up_height"] = gGroundStepUpHeight;
        root["capture_width_tiles"] = gCaptureWidthTiles;
        root["capture_height_tiles"] = gCaptureHeightTiles;
        root["capture_rapid_shot_limit"] = gCaptureRapidShotLimit;
        root["capture_rapid_window_seconds"] = gCaptureRapidWindowSeconds;
        root["capture_overheat_lock_seconds"] = gCaptureOverheatLockSeconds;
        root["printed_photo_padding_x"] = gPrintedPhotoPaddingX;
        root["printed_photo_padding_top"] = gPrintedPhotoPaddingTop;
        root["printed_photo_footer_height"] = gPrintedPhotoFooterHeight;
        root["printed_photo_min_width"] = gPrintedPhotoMinWidth;
        root["printed_photo_min_height"] = gPrintedPhotoMinHeight;
        root["printed_photo_matte_inset"] = gPrintedPhotoMatteInset;
        root["pickup_time_bonus"] = gPickupTimeBonus;
        return root;
    }

}

void GameScene::RefreshStageRenderProfile()
{
    m_lifecycle.darknessStageEnabled = IsDarknessStageMapPath(m_lifecycle.currentMapCsvPath);
}

void GameScene::BuildCameraMarkers()
{

    m_camera.fixedRanges.clear();

    float tileSize = m_tileMap.GetTileSize();

    //繧ｫ繝｡繝ｩ1
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(-2, 6, tileSize); // 蟾ｦ荳翫ち繧､繝ｫ
        cameraRange.SetEndTiles(22, 18, tileSize);   // 蜿ｳ荳九ち繧､繝ｫ
        cameraRange.SetCameraNum(0);
        cameraRange.SetFollowPlayer(false);

        m_camera.fixedRanges.push_back(cameraRange);
    }

    //繧ｫ繝｡繝ｩ2
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(22, 6, tileSize);
        cameraRange.SetEndTiles(42, 19, tileSize);
        cameraRange.SetCameraNum(1);
        cameraRange.SetFollowPlayer(false);

        m_camera.fixedRanges.push_back(cameraRange);
    }

    //繧ｫ繝｡繝ｩ3
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(72, 2, tileSize);
        cameraRange.SetEndTiles(93, 28, tileSize);
        cameraRange.SetCameraNum(2);
        cameraRange.SetFollowPlayer(false);

        cameraRange.SetZoomWidth(2560.0f);
        cameraRange.SetZoomHeight(1440.0f);

        m_camera.fixedRanges.push_back(cameraRange);
    }

    //繧ｫ繝｡繝ｩ4
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(113, 4, tileSize);
        cameraRange.SetEndTiles(133, 21, tileSize);
        cameraRange.SetCameraNum(3);
        cameraRange.SetFollowPlayer(false);

        cameraRange.SetZoomHeight(1440.0f);

        m_camera.fixedRanges.push_back(cameraRange);
    }

    //繧ｫ繝｡繝ｩ5
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(131, 10, tileSize);
        cameraRange.SetEndTiles(148, 21, tileSize);
        cameraRange.SetCameraNum(4);
        cameraRange.SetFollowPlayer(false);

        m_camera.fixedRanges.push_back(cameraRange);
    }


}

namespace game_scene_detail
{
    constexpr float kDefaultCameraViewWidth = 1920.0f;
    constexpr float kDefaultCameraViewHeight = 1080.0f;

    void WriteTuningJsonFile()
    {
        std::ofstream stream(kTuningFilePath, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            return;
        }

        stream << BuildTuningJson().dump(2);
    }

    void LoadTuningJsonFile()
    {
        std::ifstream stream(kTuningFilePath, std::ios::binary);
        if (!stream.is_open())
        {
            WriteTuningJsonFile();
            return;
        }

        nlohmann::json root;
        try
        {
            stream >> root;
        }
        catch (...)
        {
            return;
        }

        gCameraViewWidth = root.value("camera_view_width", gCameraViewWidth);
        gCameraViewHeight = root.value("camera_view_height", gCameraViewHeight);
        gCameraFollowSpeedX = root.value("camera_follow_speed_x", gCameraFollowSpeedX);
        gCameraFollowSpeedY = root.value("camera_follow_speed_y", gCameraFollowSpeedY);
        gCameraFollowY = root.value("camera_follow_y", gCameraFollowY >= 0.5f) ? 1.0f : 0.0f;
        gPlayerMoveSpeed = root.value("move_speed", gPlayerMoveSpeed);
        gPlayerJumpSpeed = root.value("jump_speed", gPlayerJumpSpeed);
        gPlayerGravity = root.value("gravity", gPlayerGravity);
        gPlayerMaxFallSpeed = root.value("max_fall_speed", gPlayerMaxFallSpeed);
        gPlayerDodgeSpeed = root.value("dodge_speed", gPlayerDodgeSpeed);
        gPlayerDodgeDistance = root.value("dodge_distance", gPlayerDodgeDistance);
        gPlayerDodgeInvincibilitySeconds = root.value("dodge_invincibility", gPlayerDodgeInvincibilitySeconds);
        gPlayerDodgeCooldown = root.value("dodge_cooldown", gPlayerDodgeCooldown);
        gCoyoteTimeSeconds = root.value("coyote_time", gCoyoteTimeSeconds);
        gGroundSnapDistance = root.value("ground_snap_distance", gGroundSnapDistance);
        gGroundStepUpHeight = root.value("ground_step_up_height", gGroundStepUpHeight);
        gCaptureWidthTiles = root.value("capture_width_tiles", gCaptureWidthTiles);
        gCaptureHeightTiles = root.value("capture_height_tiles", gCaptureHeightTiles);
        gCaptureRapidShotLimit = root.value("capture_rapid_shot_limit", gCaptureRapidShotLimit);
        gCaptureRapidWindowSeconds = root.value("capture_rapid_window_seconds", gCaptureRapidWindowSeconds);
        gCaptureOverheatLockSeconds = root.value("capture_overheat_lock_seconds", gCaptureOverheatLockSeconds);
        gPrintedPhotoPaddingX = root.value("printed_photo_padding_x", gPrintedPhotoPaddingX);
        gPrintedPhotoPaddingTop = root.value("printed_photo_padding_top", gPrintedPhotoPaddingTop);
        gPrintedPhotoFooterHeight = root.value("printed_photo_footer_height", gPrintedPhotoFooterHeight);
        gPrintedPhotoMinWidth = root.value("printed_photo_min_width", gPrintedPhotoMinWidth);
        gPrintedPhotoMinHeight = root.value("printed_photo_min_height", gPrintedPhotoMinHeight);
        gPrintedPhotoMatteInset = root.value("printed_photo_matte_inset", gPrintedPhotoMatteInset);
        gPickupTimeBonus = root.value("pickup_time_bonus", gPickupTimeBonus);
    }
}

void GameScene::ResetSceneState()
{
    m_world.Clear();
    m_photo = PhotoState{};
    m_flow = GameSceneFlowState{};
    m_ui = GameSceneUiState{};
    m_player = GameScenePlayerState{};
    m_debug = GameSceneDebugState{};
    m_mapEditor.active = false;
    m_mapEditor.brushTarget = GameSceneMapEditorState::BrushTarget::Tile;
    m_mapEditor.selectedTileValue = 1;
    m_mapEditor.selectedMarker = 'G';
    m_mapEditor.selectedMarkerParameter = 1;
    m_mapEditor.selectedStageLightTiles = 3;
    m_mapEditor.selectedStageLightFixtureTiles = 1;
    m_mapEditor.statusMessage.clear();
    m_mapEditor.statusMessageTimer = 0.0f;
    m_tuning = GameSceneTuningState{};
    m_camera.transitionMarkers.clear();

    m_camera.hasPreviousPlayerCameraProbe = false;
    m_camera.previousPlayerCameraProbeX = 0.0f;
    m_camera.previousPlayerCameraProbeY = 0.0f;
    m_camera.hasCameraSmoothedPlayerY = false;
    m_camera.cameraSmoothedPlayerCenterY = 0.0f;
    m_camera.floorCameraTransitionActive = false;
    m_camera.floorCameraTransitionElapsed = 0.0f;
    m_camera.floorCameraTransitionDuration = 1.10f;
    m_camera.floorCameraTransitionStartX = 0.0f;
    m_camera.floorCameraTransitionStartY = 0.0f;
    m_camera.floorCameraTransitionTargetX = 0.0f;
    m_camera.floorCameraTransitionTargetY = 0.0f;
    m_camera.cameraFixedLockActive = false;
    m_camera.cameraFixedLockStartX = 0.0f;
    m_camera.cameraFixedLockEndX = 0.0f;
    m_camera.cameraFixedLockX = 0.0f;
    m_camera.cameraFixedLockY = 0.0f;
    m_lifecycle.hasPendingStageTransition = false;
    m_lifecycle.pendingStageTransitionMapCsv.clear();
    m_lifecycle.pendingStageTransitionSpawnMarker = '\0';
    m_lifecycle.pendingStageTransitionMarker = '\0';
    m_lifecycle.darknessStageEnabled = false;
    m_lifecycle.currentMapCsvPath = "assets/maps/stages/ruins_boss.csv";
    m_lifecycle.lastStageTransitionMarker = '\0';
    m_render.shakeOffsetX = 0.0f;
    m_render.shakeOffsetY = 0.0f;
    m_render.viewScaleMultiplier = 1.0f;
    m_render.zoomAnchorScreenCenter = false;
    m_render.zoomAnchorX = static_cast<float>(SCREEN_WIDTH) * 0.5f;
    m_render.zoomAnchorY = static_cast<float>(SCREEN_HEIGHT) * 0.5f;
    m_flow.timeLimit = 60.0f;
    m_flow.timeRemaining = m_flow.timeLimit;
}

void GameScene::LoadTuningState()
{
    const ActiveGameSceneScope activeScene(*this);
    LoadTuningJsonFile();
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
    if (!ec)
    {
        m_debug.tuningFileWriteTime = writeTime;
        m_debug.hasTuningFileWriteTime = true;
    }
}

void GameScene::InitializeStageResources(ResourceManager& resources)
{
    LoadStageTransitionLinks();
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_tileTexture = resources.LoadTexture(L"assets\\texture\\block.png");
    m_tileMap.LoadFromCsv(m_lifecycle.currentMapCsvPath, 48.0f);
    const size_t mapCellCount =
        static_cast<size_t>((std::max)(0, m_tileMap.GetWidth())) *
        static_cast<size_t>((std::max)(0, m_tileMap.GetHeight()));
    m_world.Reserve(128 + mapCellCount / 8, 64);
    RefreshStageRenderProfile();
    gCameraViewWidth = kDefaultCameraViewWidth;
    gCameraViewHeight = kDefaultCameraViewHeight;
    m_eventBus.Reserve(128);
    m_eventBus.Clear();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
}

void GameScene::InitializeStageEntities()
{
    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
    const bool isDebugStageMap = m_lifecycle.currentMapCsvPath == "assets/maps/stage_a.csv";
    const auto spawnRespawnableBarrel = [&](float x, float y)
    {
        Entity& barrel = SpawnStagePrefab(prefabs, "sandbox_barrel", x, y);
        if (auto* barrelComponent = barrel.GetComponent<BarrelComponent>())
        {
            barrelComponent->respawnWhenOffscreen = true;
            if (auto* transform = barrel.GetComponent<TransformComponent>())
            {
                barrelComponent->spawnX = transform->x;
                barrelComponent->spawnY = transform->y;
            }
            else
            {
                barrelComponent->spawnX = x;
                barrelComponent->spawnY = y;
            }
        }
    };

    float goalX = GetMapPixelWidth() - 120.0f;
    float goalY = 248.0f;
    bool goalMarkerFound = false;
    const float tileSize = m_tileMap.GetTileSize();
    const float goalSize = tileSize;
    const std::vector<TileMarker> stageMarkers = CollectTileMarkers(m_tileMap);

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'G')
        {
            continue;
        }

        goalX = static_cast<float>(stageMarker.column) * tileSize;
        goalY = static_cast<float>(stageMarker.row) * tileSize;
        goalMarkerFound = true;
    }

    if (!goalMarkerFound)
    {
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (!IsGoalTile(column, row))
            {
                continue;
            }

            goalX = static_cast<float>(column) * tileSize;
            goalY = static_cast<float>(row + 1) * tileSize - goalSize;
            row = m_tileMap.GetHeight();
            break;
        }
    }
    }

    float playerSpawnX = AlignToGrid(192.0f, tileSize);
    float playerSpawnY = AlignToGrid(336.0f, tileSize);
    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != '*')
        {
            continue;
        }

        playerSpawnX = static_cast<float>(stageMarker.column) * tileSize;
        playerSpawnY = static_cast<float>(stageMarker.row) * tileSize;
        break;
    }

    Entity& player = SpawnStagePrefab(
        prefabs,
        "sandbox_player",
        playerSpawnX,
        playerSpawnY);
    SetEntityTint(player, 1.0f, 1.0f, 1.0f, 1.0f);
    if (auto* playerTransform = player.GetComponent<TransformComponent>())
    {
        m_flow.stageStartX = playerTransform->x;
        m_flow.stageStartY = playerTransform->y;
        m_flow.respawnX = playerTransform->x;
        m_flow.respawnY = playerTransform->y;
    }
    else
    {
        m_flow.stageStartX = AlignToGrid(192.0f, tileSize);
        m_flow.stageStartY = AlignToGrid(336.0f, tileSize);
        m_flow.respawnX = m_flow.stageStartX;
        m_flow.respawnY = m_flow.stageStartY;
    }
    m_flow.hasCheckpoint = false;
    m_flow.activeCheckpointId = -1;

    const bool hasBarrelMarker = std::any_of(
        stageMarkers.begin(),
        stageMarkers.end(),
        [](const TileMarker& stageMarker)
        {
            return stageMarker.marker == 'B';
        });

    if (isDebugStageMap && !hasBarrelMarker)
    {
        spawnRespawnableBarrel(
            AlignToGrid(432.0f, tileSize),
            AlignToGrid(240.0f, tileSize));
    }

    const auto placeGroundedEnemyAtMarker = [&](Entity& enemy, int column, int row) -> TransformComponent*
    {
        auto* transform = enemy.GetComponent<TransformComponent>();
        if (!transform)
        {
            return nullptr;
        }

        transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
        float spawnX = transform->x;
        float spawnY = transform->y;
        if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
        {
            transform->x = spawnX;
            transform->y = spawnY;
        }
        SnapEnemyToGround(*transform);
        return transform;
    };

    const auto spawnMidBoss3Fists = [&](PrefabFactory& prefabs, Entity& boss)
    {
        auto* bossTransform = boss.GetComponent<TransformComponent>();
        auto* bossComp = boss.GetComponent<MidBoss3Component>();
        if (!bossTransform || !bossComp)
        {
            return;
        }

        bossComp->fistEntities.clear();
        const float offsets[4][2] = {
            { tileSize * 1.0f, -tileSize * 3.0f },
            { tileSize * 1.0f,  tileSize * 5.0f },
            { tileSize * 5.0f, -tileSize * 2.0f },
            { tileSize * 5.0f,  tileSize * 4.5f },
        };

        for (int index = 0; index < 4; ++index)
        {
            Entity& fist = SpawnStagePrefab(
                prefabs,
                "sandbox_mid_boss3_fist",
                bossTransform->x + offsets[index][0],
                bossTransform->y + offsets[index][1]);
            auto& fistComp = fist.AddComponent<MidBoss3FistComponent>();
            fistComp.ownerBoss = &boss;
            fistComp.fistIndex = index;
            fistComp.baseOffsetX = offsets[index][0];
            fistComp.baseOffsetY = offsets[index][1];
            fistComp.idlePhase = static_cast<float>(index) * 1.35f;
            bossComp->fistEntities.push_back(&fist);
        }
    };

    // Spawn walker/ranged enemies from CSV markers.
    for (const TileMarker& stageMarker : stageMarkers)
    {
        const int column = stageMarker.column;
        const int row = stageMarker.row;
        const char marker = stageMarker.marker;
        if (marker == 'W') // Walker
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_walker",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            ConfigureWalkerSpriteAnimation(enemy);
            placeGroundedEnemyAtMarker(enemy, column, row);
        }
        else if (marker == 'R') // Ranged
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_ranged",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            ConfigureRangedSpriteAnimation(enemy);
            placeGroundedEnemyAtMarker(enemy, column, row);
        }
        else if (marker == '$') // Charger
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_charger",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = placeGroundedEnemyAtMarker(enemy, column, row))
            {
                if (auto* enemyComp = enemy.GetComponent<EnemyComponent>())
                {
                    enemyComp->spawnX = transform->x;
                    enemyComp->spawnY = transform->y;
                }
            }
        }
        else if (marker == 'N' || marker == '?') // ShieldBoss
        {
            if (m_flow.shieldBossDefeatedThisScene)
            {
                continue;
            }
            Entity& boss = SpawnStagePrefab(
                prefabs,
                "sandbox_shield_boss",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = placeGroundedEnemyAtMarker(boss, column, row))
            {
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                    enemy->respawnEnabled = false;
                }

                if (auto* bossComp = boss.GetComponent<ShieldBossComponent>())
                {
                    constexpr float kShieldW = 48.0f;
                    constexpr float kShieldH = 144.0f;
                    auto shieldEntity = std::make_unique<Entity>();
                    shieldEntity->AddComponent<TagComponent>("BossShield");
                    shieldEntity->AddComponent<TransformComponent>(
                        transform->x - kShieldW,
                        transform->y,
                        kShieldW,
                        kShieldH);
                    shieldEntity->AddComponent<TintComponent>(
                        0.72f,
                        0.78f,
                        0.90f,
                        bossComp->appearAnimationActive ? 0.0f : 1.0f);
                    shieldEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
                    auto& shieldComp = shieldEntity->AddComponent<ShieldComponent>();
                    shieldComp.attached = true;
                    shieldComp.ownerBoss = &boss;
                    shieldComp.contactDamage = 1;
                    shieldComp.followOffsetX = -kShieldW;
                    shieldComp.followOffsetY = 0.0f;
                    bossComp->shieldEntity = shieldEntity.get();
                    m_world.Spawn(std::move(shieldEntity));
                }
            }
        }
        else if (marker == '!') // MidBoss2
        {
            Entity& boss = SpawnStagePrefab(
                prefabs,
                "sandbox_mid_boss2",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = placeGroundedEnemyAtMarker(boss, column, row))
            {
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                }
            }
        }
        else if (marker == '%') // MidBoss3
        {
            Entity& boss = SpawnStagePrefab(
                prefabs,
                "sandbox_mid_boss3",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = boss.GetComponent<TransformComponent>())
            {
                transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                transform->y = static_cast<float>(row) * tileSize + (tileSize - transform->height * transform->scale) * 0.5f;
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                    enemy->respawnEnabled = false;
                }
                if (auto* bossComp = boss.GetComponent<MidBoss3Component>())
                {
                    bossComp->homeX = transform->x;
                    bossComp->homeY = transform->y;
                    bossComp->initializedHome = true;
                }
                spawnMidBoss3Fists(prefabs, boss);
            }
        }
        else if (marker == 'A') // 繧ｴ繝ｼ繧ｹ繝・
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_ghost",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = enemy.GetComponent<TransformComponent>())
            {
                if (auto* enemyComp = enemy.GetComponent<EnemyComponent>())
                {
                    enemyComp->spawnX = transform->x;
                    enemyComp->spawnY = transform->y;
                }
            }
        }
        else if (marker == 'D') // 繝悶Λ繝ｭ繝・
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_blaster_robot",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = enemy.GetComponent<TransformComponent>())
            {
                // FindSpawnPosition繧剃ｽｿ繧上★CSV縺ｮ蠎ｧ讓吶ｒ縺昴・縺ｾ縺ｾ菴ｿ縺・
                transform->x = static_cast<float>(column) * tileSize;
                transform->y = static_cast<float>(row) * tileSize;
                if (auto* enemyComp = enemy.GetComponent<EnemyComponent>())
                {
                    enemyComp->spawnX = transform->x;
                    enemyComp->spawnY = transform->y;
                }
                if (auto* blasterRobot = enemy.GetComponent<BlasterRobotComponent>())
                {
                    // 螟ｩ莠募愛螳夲ｼ壹・繝ｼ繧ｫ繝ｼ縺ｮ荳翫・繧ｿ繧､繝ｫ縺悟｣√↑繧牙､ｩ莠戊ｨｭ鄂ｮ
                    if (row > 0 && m_tileMap.GetTile(column, row - 1) > 0)
                    {
                        blasterRobot->mountedOnCeiling = true;
                    }
                }
            }
        }
    }

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'Y')
        {
            continue;
        }

        auto battery = std::make_unique<Entity>();
        battery->AddComponent<TagComponent>(kTagBattery);
        battery->AddComponent<TransformComponent>(
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize),
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
            battery->AddComponent<FlickerLightComponent>(
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
        m_world.Spawn(std::move(battery));
    }

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'B')
        {
            continue;
        }

        spawnRespawnableBarrel(
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize));
    }

    RefreshLogsFromMarkers();
    RefreshMarkerLightsFromMarkers();
    RefreshStageLightsFromMarkers();

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'V')
        {
            continue;
        }

        Entity& vanishObject = SpawnStagePrefab(
            prefabs,
            "sandbox_vanish_object",
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize));
        vanishObject.AddComponent<PhotoCopyRoleComponent>(PhotoCopyRole::Solid);
        vanishObject.AddComponent<PhotoCopyLayerComponent>(PhotoCopyLayer::Foreground);
        vanishObject.AddComponent<PhotoCopyOriginComponent>(PhotoCopyOrigin::Generic);
        vanishObject.AddComponent<PhotoCopyEffectComponent>(PhotoFilterTheme::None);
        vanishObject.AddComponent<VanishOnCaptureComponent>(true);
    }

    int checkpointId = 0;
    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'C')
        {
            continue;
        }

        const float checkpointX = AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize);
        const float checkpointY = AlignToGrid(static_cast<float>(stageMarker.row) * tileSize - tileSize, tileSize);
        Entity& checkpoint = SpawnStagePrefab(
            prefabs,
            "sandbox_checkpoint",
            checkpointX,
            checkpointY);
        checkpoint.AddComponent<CheckpointComponent>(checkpointId, checkpointX, checkpointY);
        ++checkpointId;
    }

    Entity& goal = SpawnStagePrefab(
        prefabs,
        "sandbox_goal",
        AlignToGrid(goalX, tileSize),
        AlignToGrid(goalY, tileSize));
    SetEntityTint(goal, 0.62f, 0.30f, 0.24f);
    m_flow.goalUnlocked = true;
    m_flow.goalUnlockedBySwitch = true;

    if (isDebugStageMap)
    {
        Entity& star = SpawnStagePrefab(
            prefabs,
            "star_outline",
            AlignToGrid(720.0f, tileSize),
            AlignToGrid(336.0f, tileSize));
        SetEntityTint(star, 1.0f, 1.0f, 1.0f, 1.0f);

        Entity& apple = SpawnStagePrefab(
            prefabs,
            "apple_outline",
            AlignToGrid(940.0f, tileSize),
            AlignToGrid(336.0f, tileSize));
        SetEntityTint(apple, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    RefreshDamageFootholdsFromMarkers();
	RefleshSepiaRubblesFromMarkers();
 //   Entity& photoSourceA = SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(80.0f, tileSize), AlignToGrid(160.0f, tileSize)); 
	//SetEntityTint(photoSourceA, 0.96f, 0.68f, 0.18f);
 //   Entity& photoSourceB= SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(1360.0f, tileSize), AlignToGrid(240.0f, tileSize)); 
 //   SetEntityTint(photoSourceB, 0.96f, 0.68f, 0.18f);

 //   Entity& photoSourceC = SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(1400.0f, tileSize), AlignToGrid(240.0f, tileSize));
 //   SetEntityTint(photoSourceC, 0.96f, 0.68f, 0.18f);

    //Entity& shadowSource = SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(920.0f, tileSize), AlignToGrid(320.0f, tileSize));
    ////SetEntityTint(shadowSource, 0.08f, 0.08f, 0.10f);

    //Entity& flipSourceA = SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(1220.0f, tileSize), AlignToGrid(288.0f, tileSize));
    ////SetEntityTint(flipSourceA, 0.96f, 0.68f, 0.18f);

    //Entity& flipSourceB = SpawnStagePrefab(prefabs, "sandbox_photo_source", AlignToGrid(1300.0f, tileSize), AlignToGrid(352.0f, tileSize));
    ////SetEntityTint(flipSourceB, 0.96f, 0.68f, 0.18f);

   // SpawnStagePrefab(prefabs, "sandbox_enemy_wide", AlignToGrid(760.0f, tileSize), AlignToGrid(248.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_tall", AlignToGrid(1470.0f, tileSize), AlignToGrid(230.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_walker", AlignToGrid(500.0f, tileSize), AlignToGrid(352.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_ranged", AlignToGrid(900.0f, tileSize), AlignToGrid(352.0f, tileSize));

    RefreshLinkedGimmicksFromMarkers();
    RefreshLaserTurretsFromMarkers();
    BuildCameraMarkers();

    // Choose backdrop keys from m_lifecycle.currentMapCsvPath and cache texture IDs
    auto ResolveBackdropKeysForMap = [](const std::string& mapPath) -> std::pair<std::string, std::string>
    {
        std::string stem;
        try { stem = std::filesystem::path(mapPath).stem().string(); } catch (...) { stem = mapPath; }
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        // CSV 蜷阪↓ "forest" 縺ｾ縺溘・ "ruins" 繧貞性繧√※隴伜挨縺励※縺・ｋ縺ｨ縺ｮ縺薙→縺ｪ縺ｮ縺ｧ縺昴ｌ縺ｫ蜷医ｏ縺帙ｋ
		if (stem.find("ruins") != std::string::npos) return { "ruins_bg", "ruins_fg" };//繝槭ャ繝励・CSV繝輔ぃ繧､繝ｫ蜷阪↓ "ruins" 繧貞性繧蝣ｴ蜷医・蟒・｢溘・閭梧勹縺ｨ蜑肴勹繧剃ｽｿ逕ｨ
        if (stem.find("forest") != std::string::npos) return { "sinrin10", "sinrin11" };
        // 繝・ヵ繧ｩ繝ｫ繝・
        return { "sinrin10", "sinrin11" };
    };

    {
        const auto keys = ResolveBackdropKeysForMap(m_lifecycle.currentMapCsvPath);
        m_camera.backdropTextureId = m_assets.GetTexture(keys.first);
        m_camera.backdropTexture1Id = m_assets.GetTexture(keys.second);
        // manifest 譛ｪ逋ｻ骭ｲ縺ｪ繧画里蟄倥く繝ｼ縺ｫ繝輔か繝ｼ繝ｫ繝舌ャ繧ｯ
        if (m_camera.backdropTextureId < 0) m_camera.backdropTextureId = m_assets.GetTexture("sinrin10");
        if (m_camera.backdropTexture1Id < 0) m_camera.backdropTexture1Id = m_assets.GetTexture("sinrin11");
    }
}

Entity& GameScene::SpawnStagePrefab(PrefabFactory& prefabs, const char* prefabId, float x, float y)
{
    auto entity = prefabs.Create(prefabId);
    if (!entity)
    {
        throw std::runtime_error(std::string("Missing prefab: ") + prefabId);
    }

    Entity& entityRef = *entity;
    if (auto* transform = entityRef.GetComponent<TransformComponent>())
    {
        transform->x = x;
        transform->y = y;
    }
    if (auto* barrel = entityRef.GetComponent<BarrelComponent>())
    {
        barrel->spawnX = x;
        barrel->spawnY = y;
    }
    if (auto* enemyMover = entityRef.GetComponent<EnemyMoverComponent>())
    {
        enemyMover->SetOrigin(x, y);
    }

    if (auto* enemy = entityRef.GetComponent<EnemyComponent>())
    {
        enemy->spawnX = x;
        enemy->spawnY = y;
    }

    m_world.Spawn(std::move(entity));
    return entityRef;
}

