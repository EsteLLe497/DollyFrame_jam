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
        return upper == 'W' || upper == 'R' || upper == 'N' || upper == '!' || upper == '?' || upper == '$';
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
    m_darknessStageEnabled = IsDarknessStageMapPath(gCurrentMapCsvPath);
}

void GameScene::BuildCameraMarkers()
{
    m_cameraFixedRanges.clear();

    float tileSize = m_tileMap.GetTileSize();

    //カメラ1
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(-2, 6, tileSize); // 左上タイル
        cameraRange.SetEndTiles(22, 18, tileSize);   // 右下タイル
        cameraRange.SetCameraNum(0);
        cameraRange.SetFollowPlayer(false);

        m_fixedRanges.push_back(cameraRange);
    }

    //カメラ2
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(22, 6, tileSize);
        cameraRange.SetEndTiles(42, 19, tileSize);
        cameraRange.SetCameraNum(1);
        cameraRange.SetFollowPlayer(false);

        m_fixedRanges.push_back(cameraRange);
    }

    //カメラ3
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(72, 2, tileSize);
        cameraRange.SetEndTiles(93, 28, tileSize);
        cameraRange.SetCameraNum(2);
        cameraRange.SetFollowPlayer(false);

        cameraRange.SetZoomWidth(2560.0f);
        cameraRange.SetZoomHeight(1440.0f);

        m_fixedRanges.push_back(cameraRange);
    }

    //カメラ4
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(113, 4, tileSize);
        cameraRange.SetEndTiles(133, 21, tileSize);
        cameraRange.SetCameraNum(3);
        cameraRange.SetFollowPlayer(false);

        cameraRange.SetZoomHeight(1440.0f);

        m_fixedRanges.push_back(cameraRange);
    }

    //カメラ5
    {
        fixedCameraRange cameraRange;
        cameraRange.SetStartTiles(131, 10, tileSize);
        cameraRange.SetEndTiles(148, 21, tileSize);
        cameraRange.SetCameraNum(4);
        cameraRange.SetFollowPlayer(false);

        m_fixedRanges.push_back(cameraRange);
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
    m_entities.clear();
    m_pendingEntities.clear();
    m_photo = PhotoState{};
    m_flow = GameSceneFlowState{};
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
    m_cameraTransitionMarkers.clear();
    m_cameraFixedRanges.clear();
    m_hasPreviousPlayerCameraProbe = false;
    m_previousPlayerCameraProbeX = 0.0f;
    m_previousPlayerCameraProbeY = 0.0f;
    m_hasCameraSmoothedPlayerY = false;
    m_cameraSmoothedPlayerCenterY = 0.0f;
    m_floorCameraTransitionActive = false;
    m_floorCameraTransitionElapsed = 0.0f;
    m_floorCameraTransitionDuration = 1.10f;
    m_floorCameraTransitionStartX = 0.0f;
    m_floorCameraTransitionStartY = 0.0f;
    m_floorCameraTransitionTargetX = 0.0f;
    m_floorCameraTransitionTargetY = 0.0f;
    m_cameraFixedLockActive = false;
    m_cameraFixedLockStartX = 0.0f;
    m_cameraFixedLockEndX = 0.0f;
    m_cameraFixedLockX = 0.0f;
    m_cameraFixedLockY = 0.0f;
    m_hasPendingStageTransition = false;
    m_pendingStageTransitionMapCsv.clear();
    m_pendingStageTransitionSpawnMarker = '\0';
    m_pendingStageTransitionMarker = '\0';
    m_darknessStageEnabled = false;
    gCurrentMapCsvPath = "assets/maps/stages/ruins1.csv";
    gLastStageTransitionMarker = '\0';
    m_flow.timeLimit = 60.0f;
    m_flow.timeRemaining = m_flow.timeLimit;
}

void GameScene::LoadTuningState()
{
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
    m_tileMap.LoadFromCsv(gCurrentMapCsvPath, 48.0f);
    RefreshStageRenderProfile();
    gCameraViewWidth = kDefaultCameraViewWidth;
    gCameraViewHeight = kDefaultCameraViewHeight;
    m_eventBus.Clear();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
}

void GameScene::InitializeStageEntities()
{
    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
    const bool isDebugStageMap = gCurrentMapCsvPath == "assets/maps/stage_a.csv";
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

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != 'G')
            {
                continue;
            }

            goalX = static_cast<float>(column) * tileSize;
            goalY = static_cast<float>(row) * tileSize;
            goalMarkerFound = true;
        }
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
    bool playerSpawnMarkerFound = false;
    for (int row = 0; row < m_tileMap.GetHeight() && !playerSpawnMarkerFound; ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != '*')
            {
                continue;
            }

            playerSpawnX = static_cast<float>(column) * tileSize;
            playerSpawnY = static_cast<float>(row) * tileSize;
            playerSpawnMarkerFound = true;
            break;
        }
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

    bool hasBarrelMarker = false;
    for (int row = 0; row < m_tileMap.GetHeight() && !hasBarrelMarker; ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) == 'B')
            {
                hasBarrelMarker = true;
                break;
            }
        }
    }

    if (isDebugStageMap && !hasBarrelMarker)
    {
        spawnRespawnableBarrel(
            AlignToGrid(432.0f, tileSize),
            AlignToGrid(240.0f, tileSize));
    }
    // Spawn walker/ranged enemies from CSV markers.
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = m_tileMap.GetMarker(column, row);
            if (marker == 'W') // Walker
            {
                Entity& enemy = SpawnStagePrefab(
                    prefabs,
                    "sandbox_enemy_walker",
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize);
                ConfigureWalkerSpriteAnimation(enemy);
                if (auto* transform = enemy.GetComponent<TransformComponent>())
                {
                    transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
                }
            }
            else if (marker == 'R') // Ranged
            {
                Entity& enemy = SpawnStagePrefab(
                    prefabs,
                    "sandbox_enemy_ranged",
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize);
                ConfigureRangedSpriteAnimation(enemy);
                if (auto* transform = enemy.GetComponent<TransformComponent>())
                {
                    transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
                }
            }
            else if (marker == '$') // Charger
            {
                Entity& enemy = SpawnStagePrefab(
                    prefabs,
                    "sandbox_enemy_charger",
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize);
                if (auto* transform = enemy.GetComponent<TransformComponent>())
                {
                    transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
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
                if (auto* transform = boss.GetComponent<TransformComponent>())
                {
                    transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
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
                if (auto* transform = boss.GetComponent<TransformComponent>())
                {
                    transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                    float spawnX = transform->x;
                    float spawnY = transform->y;
                    if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
                    {
                        transform->x = spawnX;
                        transform->y = spawnY;
                    }
                    SnapEnemyToGround(*transform);
                    if (auto* enemy = boss.GetComponent<EnemyComponent>())
                    {
                        enemy->spawnX = transform->x;
                        enemy->spawnY = transform->y;
                    }
                }
            }
            else if (marker == 'A') // ゴースト
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
            else if (marker == 'D') // ブラロボ
            {
                Entity& enemy = SpawnStagePrefab(
                    prefabs,
                    "sandbox_enemy_blaster_robot",
                    static_cast<float>(column) * tileSize,
                    static_cast<float>(row) * tileSize);
                if (auto* transform = enemy.GetComponent<TransformComponent>())
                {
                    // FindSpawnPositionを使わずCSVの座標をそのまま使う
                    transform->x = static_cast<float>(column) * tileSize;
                    transform->y = static_cast<float>(row) * tileSize;
                    if (auto* enemyComp = enemy.GetComponent<EnemyComponent>())
                    {
                        enemyComp->spawnX = transform->x;
                        enemyComp->spawnY = transform->y;
                    }
                    if (auto* blasterRobot = enemy.GetComponent<BlasterRobotComponent>())
                    {
                        // 天井判定：マーカーの上のタイルが壁なら天井設置
                        if (row > 0 && m_tileMap.GetTile(column, row - 1) > 0)
                        {
                            blasterRobot->mountedOnCeiling = true;
                        }
                    }
                }
            }
        }
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != 'Y')
            {
                continue;
            }

            auto battery = std::make_unique<Entity>();
            battery->AddComponent<TagComponent>(kTagBattery);
            battery->AddComponent<TransformComponent>(
                AlignToGrid(static_cast<float>(column) * tileSize, tileSize),
                AlignToGrid(static_cast<float>(row) * tileSize, tileSize),
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
            m_entities.push_back(std::move(battery));
        }
    }

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != 'B')
            {
                continue;
            }

            spawnRespawnableBarrel(
                AlignToGrid(static_cast<float>(column) * tileSize, tileSize),
                AlignToGrid(static_cast<float>(row) * tileSize, tileSize));
        }
    }

    RefreshLogsFromMarkers();
    RefreshMarkerLightsFromMarkers();
    RefreshStageLightsFromMarkers();

    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != 'V')
            {
                continue;
            }

            Entity& vanishObject = SpawnStagePrefab(
                prefabs,
                "sandbox_vanish_object",
                AlignToGrid(static_cast<float>(column) * tileSize, tileSize),
                AlignToGrid(static_cast<float>(row) * tileSize, tileSize));
            vanishObject.AddComponent<PhotoCopyRoleComponent>(PhotoCopyRole::Solid);
            vanishObject.AddComponent<PhotoCopyLayerComponent>(PhotoCopyLayer::Foreground);
            vanishObject.AddComponent<PhotoCopyOriginComponent>(PhotoCopyOrigin::Generic);
            vanishObject.AddComponent<PhotoCopyEffectComponent>(PhotoFilterTheme::None);
            vanishObject.AddComponent<VanishOnCaptureComponent>(true);
        }
    }

    int checkpointId = 0;
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            if (m_tileMap.GetMarker(column, row) != 'C')
            {
                continue;
            }

            const float checkpointX = AlignToGrid(static_cast<float>(column) * tileSize, tileSize);
            const float checkpointY = AlignToGrid(static_cast<float>(row) * tileSize - tileSize, tileSize);
            Entity& checkpoint = SpawnStagePrefab(
                prefabs,
                "sandbox_checkpoint",
                checkpointX,
                checkpointY);
            checkpoint.AddComponent<CheckpointComponent>(checkpointId, checkpointX, checkpointY);
            ++checkpointId;
        }
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

    bool hasDamageFootholdMarker = false;
    for (int row = 0; row < m_tileMap.GetHeight() && !hasDamageFootholdMarker; ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = m_tileMap.GetMarker(column, row);
            if (IsDamagePlatformMarker(marker))
            {
                hasDamageFootholdMarker = true;
                break;
            }
        }
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

    m_entities.push_back(std::move(entity));
    return entityRef;
}

