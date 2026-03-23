#include "game_scene_internal.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "prefab_factory.h"

using namespace game_scene_detail;

namespace
{
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

namespace game_scene_detail
{
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
    m_photo = PhotoState{};
    m_flow = GameSceneFlowState{};
    m_player = GameScenePlayerState{};
    m_debug = GameSceneDebugState{};
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
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_tileTexture = resources.LoadTexture(L"assets\\texture\\block.png");
    m_tileMap.LoadFromCsv("assets/maps/stage01_1.csv", 48.0f);
    m_eventBus.Clear();
}

void GameScene::InitializeStageEntities()
{
    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
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
    const float tileSize = m_tileMap.GetTileSize();
    const float goalSize = tileSize * 2.0f;
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

    Entity& player = SpawnStagePrefab(
        prefabs,
        "sandbox_player",
        AlignToGrid(192.0f, tileSize),
        AlignToGrid(336.0f, tileSize));
    SetEntityTint(player, 0.30f, 0.82f, 0.98f);

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

    if (!hasBarrelMarker)
    {
        spawnRespawnableBarrel(
            AlignToGrid(432.0f, tileSize),
            AlignToGrid(240.0f, tileSize));
    }
    // 3/23’Ç‰ÁFCSVƒ}[ƒJ[‚©‚çWalker/Ranged“G‚ð¶¬(“c”Vãr)
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

   Entity& goal = SpawnStagePrefab(
        prefabs,
        "sandbox_goal",
        AlignToGrid(5500.0f, tileSize),
        AlignToGrid(240.0f, tileSize));
    SetEntityTint(goal, 0.62f, 0.30f, 0.24f);

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

    //Entity& hazardSource = SpawnStagePrefab(prefabs, "sandbox_hazard", AlignToGrid(1600.0f, tileSize), AlignToGrid(320.0f, tileSize));
    //SetEntityTint(hazardSource, 1.0f, 0.28f, 0.24f);

   // SpawnStagePrefab(prefabs, "sandbox_enemy_wide", AlignToGrid(760.0f, tileSize), AlignToGrid(248.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_tall", AlignToGrid(1470.0f, tileSize), AlignToGrid(230.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_walker", AlignToGrid(500.0f, tileSize), AlignToGrid(352.0f, tileSize));
    //SpawnStagePrefab(prefabs, "sandbox_enemy_ranged", AlignToGrid(900.0f, tileSize), AlignToGrid(352.0f, tileSize));
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

    m_entities.push_back(std::move(entity));
    return entityRef;
}
