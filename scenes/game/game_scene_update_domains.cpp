#include "pch.h"

#include "game_scene_internal.h"
#include "audio.h"
#include "DxLib.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "prefab_factory.h"

using namespace game_scene_detail;

namespace
{
    constexpr float kPhotoFocusTimeScale = 0.22f;
    constexpr float kCaptureFocusDuration = 0.8f;
    constexpr float kPlacementFocusDuration = 1.2f;
    constexpr float kStageTransitionFadeInDuration = 1.10f;
    constexpr float kCaptureFinderScaleMin = 1.0f;
    constexpr float kCaptureFinderScaleMax = 2.0f;
    constexpr float kCaptureFinderScaleStep = 0.1f;
    constexpr float kCaptureModeZoomResponse = 7.0f;
    constexpr int kEscapeMenuItemCount = 8;
    constexpr int kEscapeMenuPanelWidth = 560;
    constexpr int kEscapeMenuPanelHeight = 420;
    constexpr int kEscapeMenuRowStartOffset = 86;
    constexpr int kEscapeMenuRowHeight = 38;
    constexpr int kEscapeMenuRowPaddingX = 18;
    constexpr int kEscapeMenuRowBottomInset = 4;
    constexpr int kEditorTileMinValue = 0;
    constexpr int kEditorTileMaxValue = TileMap::kPitTileValue;
    constexpr float kEditorCameraPanSpeed = 900.0f;
    constexpr int kDefaultNewMapWidth = 64;
    constexpr int kDefaultNewMapHeight = 36;
    constexpr const char* kEditorMapOutputDir = "assets/maps/stages";

    std::string BuildEditorMapFilePath(const char* prefix)
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
#if defined(_WIN32)
        localtime_s(&localTime, &nowTime);
#else
        localtime_r(&nowTime, &localTime);
#endif

        std::ostringstream nameBuilder;
        nameBuilder
            << prefix
            << "_"
            << std::put_time(&localTime, "%Y%m%d_%H%M%S")
            << ".csv";
        return std::string(kEditorMapOutputDir) + "/" + nameBuilder.str();
    }

    bool WriteEmptyMapCsv(const std::string& path, int width, int height, int fillTileValue)
    {
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        std::filesystem::create_directories(kEditorMapOutputDir);

        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            return false;
        }

        for (int row = 0; row < height; ++row)
        {
            for (int column = 0; column < width; ++column)
            {
                if (column > 0)
                {
                    stream << ",";
                }
                stream << fillTileValue;
            }
            if (row + 1 < height)
            {
                stream << "\n";
            }
        }

        return true;
    }
}

void GameScene::UpdateCameraMode()
{
    const bool wasCameraMode = m_flow.cameraMode;
    m_flow.cameraMode = Input_IsActionDown(InputAction::HoldCamera);
    if (m_flow.cameraMode)
    {
        m_photo.placement.active = false;
        m_photo.placement.valid = false;
    }
    if (m_flow.cameraMode && !wasCameraMode)
    {
        ++m_flow.cameraModeSessionId;
    }
}

float GameScene::UpdatePhotoModes(float deltaTime)
{
    UpdateCameraMode();

    // 3迥ｶ諷具ｼ域聴蠖ｱ/驟咲ｽｮ/迴ｾ蜒上・繝ｬ繝薙Η繝ｼ・峨°繧峨√ヨ繝ｬ繧､陦ｨ遉ｺ縺ｨ繧ｹ繝ｭ繝ｼ貍泌・繧剃ｸ蜈・ｱｺ螳壹☆繧九・
    const bool placementHeld = !m_flow.cameraMode && m_photo.capture.hasPhoto && Input_IsActionDown(InputAction::HoldPlacement);
    const bool placementActive = placementHeld || m_photo.placement.active;
    const bool previewActive = m_photo.pendingStore.active && m_flow.developedPhotoPreviewRemaining > 0.0f;
    const bool previewOrbAttached =
        previewActive &&
        m_flow.developedPhotoPreviewRemaining <= 0.34f;
    const bool showPhotoTray = (previewActive && !previewOrbAttached) || m_flow.cameraMode || placementActive;
    const float trayTarget = showPhotoTray ? 1.0f : 0.0f;
    m_flow.photoTrayReveal += (trayTarget - m_flow.photoTrayReveal) * std::min(1.0f, deltaTime * 12.0f);
    if (showPhotoTray)
    {
        UpdatePhotoTraySelection();
    }
    const float captureZoomTarget = m_flow.cameraMode ? 1.0f : 0.0f;
    m_flow.captureModeZoomBlend += (captureZoomTarget - m_flow.captureModeZoomBlend) * std::min(1.0f, deltaTime * kCaptureModeZoomResponse);
    m_flow.captureSlowRemaining = m_flow.cameraMode ? kCaptureFocusDuration : 0.0f;
    m_flow.placementSlowRemaining = placementActive ? kPlacementFocusDuration : 0.0f;
    const bool slowForCapture = m_flow.cameraMode;
    const bool slowForPlacement = placementActive;
    // 繝輔か繝ｼ繧ｫ繧ｹ荳ｭ縺縺代ご繝ｼ繝蜈ｨ菴薙ｒ貂幃溘＆縺帙ｋ縲・
    return (slowForCapture || slowForPlacement)
        ? deltaTime * kPhotoFocusTimeScale
        : deltaTime;
}

void GameScene::UpdateCaptureFinderZoomInput()
{
    if (!m_flow.cameraMode)
    {
        return;
    }

    int zoomDirection = 0;
    const int wheelDelta = GetMouseWheelRotVol();
    const bool dpadUpDown = Input_IsDpadUpDown();
    const bool dpadDownDown = Input_IsDpadDownDown();
    if (wheelDelta > 0 || dpadUpDown)
    {
        ++zoomDirection;
    }
    if (wheelDelta < 0 || dpadDownDown)
    {
        --zoomDirection;
    }

    // Reverse gamepad zoom mapping:
    // LB = zoom in, RB = zoom out.
    if (Input_IsLeftShoulderPressed())
    {
        ++zoomDirection;
    }
    else if (Input_IsRightShoulderPressed())
    {
        --zoomDirection;
    }

    if (zoomDirection != 0)
    {
        m_flow.captureFinderScale = std::clamp(
            m_flow.captureFinderScale + static_cast<float>(zoomDirection) * kCaptureFinderScaleStep,
            kCaptureFinderScaleMin,
            kCaptureFinderScaleMax);
    }
}

void GameScene::ProcessFilterInput()
{
    if (Input_IsActionPressed(InputAction::SelectFilterNone))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::None;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterHot))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Hot;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterCold))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Cold;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterInvert))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Invert;
    }
    if (Input_IsActionPressed(InputAction::SelectFilterSepia))
    {
        m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
    }
    if (Input_IsActionPressed(InputAction::CycleFilter))
    {
        m_photo.capture.selectedTheme = GetNextPhotoFilterTheme(m_photo.capture.selectedTheme);
    }

    const bool blockFilterChange = m_photo.placement.active || m_flow.cameraMode;
    if (!blockFilterChange)
    {
        if (Input_IsRightShoulderPressed())
        {
            m_photo.capture.selectedTheme = GetNextPhotoFilterTheme(m_photo.capture.selectedTheme);
        }
        else if (Input_IsLeftShoulderPressed())
        {
            switch (m_photo.capture.selectedTheme)
            {
            case PhotoFilterTheme::None:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Sepia;
                break;
            case PhotoFilterTheme::Hot:
                m_photo.capture.selectedTheme = PhotoFilterTheme::None;
                break;
            case PhotoFilterTheme::Cold:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Hot;
                break;
            case PhotoFilterTheme::Invert:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Cold;
                break;
            case PhotoFilterTheme::Sepia:
                m_photo.capture.selectedTheme = PhotoFilterTheme::Invert;
                break;
            }
        }
    }
}

void GameScene::UpdateTuningHotReload(float deltaTime)
{
    m_debug.tuningReloadTimer = std::max(0.0f, m_debug.tuningReloadTimer - deltaTime);
    if (m_debug.tuningReloadTimer > 0.0f)
    {
        return;
    }

    m_debug.tuningReloadTimer = 0.25f;
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
    if (!ec && (!m_debug.hasTuningFileWriteTime || writeTime != m_debug.tuningFileWriteTime))
    {
        m_debug.tuningFileWriteTime = writeTime;
        m_debug.hasTuningFileWriteTime = true;
        LoadTuningJsonFile();
    }
}

void GameScene::HandleGlobalSceneShortcuts()
{
    if (Input_IsKeyPressed(VK_F4))
    {
        m_mapEditor.active = !m_mapEditor.active;
        m_mapEditor.brushTarget = GameSceneMapEditorState::BrushTarget::Tile;
        m_mapEditor.statusMessage.clear();
        m_mapEditor.statusMessageTimer = 0.0f;
        m_photo.placement.active = false;
        m_photo.placement.valid = false;
        m_flow.cameraMode = false;
        return;
    }

    if (Input_IsActionPressed(InputAction::ReturnToTitle))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::RestartScene))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::ToggleTuningPanel))
    {
        m_debug.showTuningPanel = !m_debug.showTuningPanel;
    }
    if (Input_IsActionPressed(InputAction::ToggleCollisionDebug))
    {
        m_debug.showCollisionDebug = !m_debug.showCollisionDebug;
    }
}

void GameScene::UpdateMapEditorInput(float deltaTime)
{
    UpdateMapEditorStatusMessage(deltaTime);
    if (!HandleMapEditorModeShortcuts())
    {
        return;
    }

    if (!m_tileMap.IsLoaded())
    {
        return;
    }

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    UpdateMapEditorCameraPan(deltaTime);
    UpdateMapEditorBrushSelection();
    HandleMapEditorFileShortcuts(tileSize);
    ApplyMapEditorMousePaint(tileSize);
}

void GameScene::UpdateMapEditorStatusMessage(float deltaTime)
{
    m_mapEditor.statusMessageTimer = std::max(0.0f, m_mapEditor.statusMessageTimer - deltaTime);
    if (m_mapEditor.statusMessageTimer <= 0.0f)
    {
        m_mapEditor.statusMessage.clear();
    }
}

bool GameScene::HandleMapEditorModeShortcuts()
{
    if (Input_IsKeyPressed(VK_F4))
    {
        m_mapEditor.active = false;
        return false;
    }

    if (Input_IsKeyPressed('M'))
    {
        m_mapEditor.brushTarget =
            m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile
            ? GameSceneMapEditorState::BrushTarget::Marker
            : GameSceneMapEditorState::BrushTarget::Tile;
        m_mapEditor.statusMessage =
            m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Marker
            ? "邱ｨ髮・ｯｾ雎｡: 繝槭・繧ｫ繝ｼ"
            : "邱ｨ髮・ｯｾ雎｡: 繧ｿ繧､繝ｫ";
        m_mapEditor.statusMessageTimer = 1.8f;
    }

    return true;
}

void GameScene::UpdateMapEditorCameraPan(float deltaTime)
{
    float panX = Input_GetAxis(InputAxis::MoveX);
    float panY = Input_GetAxis(InputAxis::MoveY);
    if (Input_IsActionDown(InputAction::MoveLeft))
    {
        panX -= 1.0f;
    }
    if (Input_IsActionDown(InputAction::MoveRight))
    {
        panX += 1.0f;
    }
    if (Input_IsActionDown(InputAction::MoveUp))
    {
        panY -= 1.0f;
    }
    if (Input_IsActionDown(InputAction::MoveDown))
    {
        panY += 1.0f;
    }
    panX = std::clamp(panX, -1.0f, 1.0f);
    panY = std::clamp(panY, -1.0f, 1.0f);
    const float maxCameraX = std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth);
    const float maxCameraY = std::max(0.0f, GetMapPixelHeight() - gCameraViewHeight);
    m_flow.cameraX = std::clamp(m_flow.cameraX + panX * kEditorCameraPanSpeed * deltaTime, 0.0f, maxCameraX);
    m_flow.cameraY = std::clamp(m_flow.cameraY + panY * kEditorCameraPanSpeed * deltaTime, 0.0f, maxCameraY);
}

void GameScene::UpdateMapEditorBrushSelection()
{
    if (m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile)
    {
        for (int digit = 0; digit <= 9; ++digit)
        {
            if (Input_IsKeyPressed('0' + digit))
            {
                m_mapEditor.selectedTileValue = digit;
            }
        }
        if (Input_IsKeyPressed(VK_F9))
        {
            m_mapEditor.selectedTileValue = TileMap::kPitTileValue;
        }
        if (Input_IsKeyPressed('Q'))
        {
            m_mapEditor.selectedTileValue = std::max(kEditorTileMinValue, m_mapEditor.selectedTileValue - 1);
        }
        if (Input_IsKeyPressed('E'))
        {
            m_mapEditor.selectedTileValue = std::min(kEditorTileMaxValue, m_mapEditor.selectedTileValue + 1);
        }
        return;
    }

    for (int digit = 0; digit <= 9; ++digit)
    {
        if (Input_IsKeyPressed('0' + digit))
        {
            m_mapEditor.selectedMarker = PresetIndexToMarker(digit);
        }
    }
    if (Input_IsKeyPressed(VK_F10))
    {
        m_mapEditor.selectedMarker = 'M';
    }
    if (Input_IsKeyPressed(VK_F11))
    {
        m_mapEditor.selectedMarker = 'Y';
    }
    if (Input_IsKeyPressed(VK_F12))
    {
        m_mapEditor.selectedMarker = 'N';
    }
    if (Input_IsKeyPressed('H'))
    {
        m_mapEditor.selectedMarker = 'H';
    }
    if (Input_IsKeyPressed('I'))
    {
        m_mapEditor.selectedMarker = 'I';
    }
    if (Input_IsKeyPressed('K'))
    {
        m_mapEditor.selectedMarker = 'K';
    }
    if (Input_IsKeyPressed('L'))
    {
        m_mapEditor.selectedMarker = 'L';
    }

    int markerIndex = MarkerToPresetIndex(m_mapEditor.selectedMarker);
    if (Input_IsKeyPressed('Q'))
    {
        markerIndex = (markerIndex + kMarkerPresetCount - 1) % kMarkerPresetCount;
        m_mapEditor.selectedMarker = PresetIndexToMarker(markerIndex);
    }
    if (Input_IsKeyPressed('E'))
    {
        markerIndex = (markerIndex + 1) % kMarkerPresetCount;
        m_mapEditor.selectedMarker = PresetIndexToMarker(markerIndex);
    }
}

void GameScene::HandleMapEditorFileShortcuts(float tileSize)
{
    if (Input_IsKeyPressed(VK_F5))
    {
        if (m_tileMap.SaveToCsv(gCurrentMapCsvPath))
        {
            m_mapEditor.statusMessage = "菫晏ｭ倥＠縺ｾ縺励◆: " + gCurrentMapCsvPath;
            m_mapEditor.statusMessageTimer = 2.4f;
        }
        else
        {
            m_mapEditor.statusMessage = "菫晏ｭ倥↓螟ｱ謨励＠縺ｾ縺励◆";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }

    if (Input_IsKeyPressed(VK_F6))
    {
        if (m_tileMap.LoadFromCsv(gCurrentMapCsvPath, tileSize))
        {
            RefreshStageRenderProfile();
            BuildCameraMarkers();
            RefreshMarkerDrivenSystems();
            m_mapEditor.statusMessage = "CSV繧貞・隱ｭ縺ｿ霎ｼ縺ｿ縺励∪縺励◆";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
        else
        {
            m_mapEditor.statusMessage = "蜀崎ｪｭ縺ｿ霎ｼ縺ｿ縺ｫ螟ｱ謨励＠縺ｾ縺励◆";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }

    if (Input_IsKeyPressed(VK_F7))
    {
        const int newWidth = m_tileMap.GetWidth() > 0 ? m_tileMap.GetWidth() : kDefaultNewMapWidth;
        const int newHeight = m_tileMap.GetHeight() > 0 ? m_tileMap.GetHeight() : kDefaultNewMapHeight;
        const std::string newMapPath = BuildEditorMapFilePath("new_map");
        if (WriteEmptyMapCsv(newMapPath, newWidth, newHeight, 0) &&
            m_tileMap.LoadFromCsv(newMapPath, tileSize))
        {
            gCurrentMapCsvPath = newMapPath;
            RefreshStageRenderProfile();
            BuildCameraMarkers();
            RefreshMarkerDrivenSystems();
            m_flow.cameraX = 0.0f;
            m_flow.cameraY = 0.0f;
            m_mapEditor.statusMessage = "譁ｰ隕上・繝・・繧剃ｽ懈・: " + newMapPath;
            m_mapEditor.statusMessageTimer = 3.0f;
        }
        else
        {
            m_mapEditor.statusMessage = "譁ｰ隕上・繝・・菴懈・縺ｫ螟ｱ謨励＠縺ｾ縺励◆";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }

    if (Input_IsKeyPressed(VK_F8))
    {
        const std::string duplicatedMapPath = BuildEditorMapFilePath("copy_map");
        std::filesystem::create_directories(kEditorMapOutputDir);
        if (m_tileMap.SaveToCsv(duplicatedMapPath))
        {
            gCurrentMapCsvPath = duplicatedMapPath;
            RefreshStageRenderProfile();
            m_mapEditor.statusMessage = "蛻･蜷堺ｿ晏ｭ倥＠縺ｾ縺励◆: " + duplicatedMapPath;
            m_mapEditor.statusMessageTimer = 3.0f;
        }
        else
        {
            m_mapEditor.statusMessage = "蛻･蜷堺ｿ晏ｭ倥↓螟ｱ謨励＠縺ｾ縺励◆";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }
}

void GameScene::ApplyMapEditorMousePaint(float tileSize)
{
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const float viewScale = GetViewScale();
    if (viewScale <= 0.0f)
    {
        return;
    }
    const float worldX = m_flow.cameraX + (static_cast<float>(mouseX) - GetViewOriginX()) / viewScale;
    const float worldY = m_flow.cameraY + (static_cast<float>(mouseY) - GetViewOriginY()) / viewScale;
    const int column = static_cast<int>(std::floor(worldX / tileSize));
    const int row = static_cast<int>(std::floor(worldY / tileSize));
    const bool insideMap = column >= 0 && row >= 0 && column < m_tileMap.GetWidth() && row < m_tileMap.GetHeight();
    if (!insideMap)
    {
        return;
    }

    const int mouseButtons = GetMouseInput();
    if ((mouseButtons & MOUSE_INPUT_LEFT) != 0)
    {
        if (m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile)
        {
            m_tileMap.SetTile(column, row, m_mapEditor.selectedTileValue);
        }
        else
        {
            const char before = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            m_tileMap.SetMarker(column, row, m_mapEditor.selectedMarker);
            const char after = static_cast<char>(std::toupper(static_cast<unsigned char>(m_mapEditor.selectedMarker)));
            RefreshMarkerDrivenSystemsByMarkerChange(before, after);
        }
    }
    if ((mouseButtons & MOUSE_INPUT_RIGHT) != 0)
    {
        if (m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile)
        {
            m_tileMap.SetTile(column, row, 0);
        }
        else
        {
            const char before = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            m_tileMap.SetMarker(column, row, '\0');
            RefreshMarkerDrivenSystemsByMarkerChange(before, '\0');
        }
    }
}

void GameScene::RefreshMarkerDrivenSystems()
{
    RefreshEnemiesFromMarkers();
    RefreshBatteriesFromMarkers();
    RefreshLogsFromMarkers();
    RefreshElevatorGimmicksFromMarkers();
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

    if (IsEnemyMarker(normalizedBefore) || IsEnemyMarker(normalizedAfter))
    {
        RefreshEnemiesFromMarkers();
    }
    if (IsBatteryMarker(normalizedBefore) || IsBatteryMarker(normalizedAfter))
    {
        RefreshBatteriesFromMarkers();
    }
    if (IsLogMarker(normalizedBefore) || IsLogMarker(normalizedAfter))
    {
        RefreshLogsFromMarkers();
    }
    if (IsElevatorMarker(normalizedBefore) || IsElevatorMarker(normalizedAfter))
    {
        RefreshElevatorGimmicksFromMarkers();
    }
    if (IsDamageFootholdMarker(normalizedBefore) || IsDamageFootholdMarker(normalizedAfter))
    {
        RefreshDamageFootholdsFromMarkers();
    }
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
                        if (enemyComponent->GetArchetype() == EnemyArchetype::ShieldBoss)
                        {
                            enemyComponent->respawnEnabled = false;
                        }
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
                ConfigureWalkerSpriteAnimation(enemy);
                placeEnemyAtMarker(enemy);
            }
            else if (marker == 'R')
            {
                Entity& enemy = SpawnStagePrefab(prefabs, "sandbox_enemy_ranged", markerX, markerY);
                ConfigureRangedSpriteAnimation(enemy);
                placeEnemyAtMarker(enemy);
            }
            else if (marker == 'N')
            {
                if (m_flow.shieldBossDefeatedThisScene)
                {
                    continue;
                }
                Entity& boss = SpawnStagePrefab(prefabs, "sandbox_shield_boss", markerX, markerY);
                placeEnemyAtMarker(boss);
                attachShieldToBoss(boss);
            }
            else if (marker == 'F')
            {
                Entity& boss = SpawnStagePrefab(prefabs, "sandbox_mid_boss2", markerX, markerY);
                placeEnemyAtMarker(boss);
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

void GameScene::RefreshElevatorGimmicksFromMarkers()
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
                    entity->GetComponent<ElevatorComponent>() != nullptr;
            }),
        m_entities.end());

    const float tileSize = m_tileMap.GetTileSize();
    if (tileSize <= 0.0f)
    {
        return;
    }

    struct SwitchMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int requiredBatteryCount = 1;
    };

    struct ElevatorMarker
    {
        float x = 0.0f;
        float y = 0.0f;
        int moveRangeTiles = 3;
    };

    std::vector<SwitchMarker> switchMarkers;
    std::vector<ElevatorMarker> elevatorMarkers;
    for (int row = 0; row < m_tileMap.GetHeight(); ++row)
    {
        for (int column = 0; column < m_tileMap.GetWidth(); ++column)
        {
            const char marker = static_cast<char>(std::toupper(static_cast<unsigned char>(m_tileMap.GetMarker(column, row))));
            const float markerX = static_cast<float>(column) * tileSize;
            const float markerY = static_cast<float>(row) * tileSize;
            if (marker == 'K')
            {
                switchMarkers.push_back(SwitchMarker{
                    markerX,
                    markerY + tileSize * 0.5f,
                    (std::max)(1, m_tileMap.GetMarkerParameter(column, row)) });
            }
            else if (marker == 'L')
            {
                const int markerParameter = m_tileMap.GetMarkerParameter(column, row);
                elevatorMarkers.push_back(ElevatorMarker{
                    markerX,
                    markerY,
                    markerParameter > 0 ? markerParameter : 3 });
            }
        }
    }

    const int pairCount = static_cast<int>((std::max)(switchMarkers.size(), elevatorMarkers.size()));
    for (int index = 0; index < pairCount; ++index)
    {
        if (index < static_cast<int>(switchMarkers.size()))
        {
            const SwitchMarker& switchMarker = switchMarkers[static_cast<size_t>(index)];
            auto switchEntity = std::make_unique<Entity>();
            switchEntity->AddComponent<TagComponent>(kTagBatterySwitch);
            switchEntity->AddComponent<TransformComponent>(
                switchMarker.x,
                switchMarker.y,
                tileSize * 3.0f,
                tileSize * 0.5f);
            switchEntity->AddComponent<TintComponent>(0.92f, 0.26f, 0.20f, 1.0f);
            switchEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            switchEntity->AddComponent<BatterySwitchComponent>(
                index,
                switchMarker.requiredBatteryCount,
                tileSize * 0.22f,
                12.0f,
                9.0f);
            m_entities.push_back(std::move(switchEntity));
        }

        if (index < static_cast<int>(elevatorMarkers.size()))
        {
            const ElevatorMarker& elevatorMarker = elevatorMarkers[static_cast<size_t>(index)];
            auto elevatorEntity = std::make_unique<Entity>();
            elevatorEntity->AddComponent<TagComponent>(kTagElevator);
            elevatorEntity->AddComponent<TransformComponent>(
                elevatorMarker.x,
                elevatorMarker.y,
                tileSize * 5.0f,
                tileSize);
            elevatorEntity->AddComponent<TintComponent>(0.42f, 0.46f, 0.52f, 1.0f);
            elevatorEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
            elevatorEntity->AddComponent<ElevatorComponent>(
                index,
                tileSize * static_cast<float>(elevatorMarker.moveRangeTiles),
                tileSize * 2.5f,
                1.0f);
            m_entities.push_back(std::move(elevatorEntity));
        }
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

void GameScene::UpdateEscapeMenuInput()
{
    const int panelLeft = (SCREEN_WIDTH - kEscapeMenuPanelWidth) / 2;
    const int panelTop = (SCREEN_HEIGHT - kEscapeMenuPanelHeight) / 2;
    const int rowStartY = panelTop + kEscapeMenuRowStartOffset;
    const int rowLeft = panelLeft + kEscapeMenuRowPaddingX;
    const int rowRight = panelLeft + kEscapeMenuPanelWidth - kEscapeMenuRowPaddingX;

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    for (int index = 0; index < kEscapeMenuItemCount; ++index)
    {
        const int rowTop = rowStartY + index * kEscapeMenuRowHeight;
        const int rowBottom = rowTop + kEscapeMenuRowHeight - kEscapeMenuRowBottomInset;
        if (mouseX >= rowLeft && mouseX <= rowRight && mouseY >= rowTop && mouseY <= rowBottom)
        {
            m_debug.escapeMenuSelection = index;
            break;
        }
    }

    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + kEscapeMenuItemCount - 1) % kEscapeMenuItemCount;
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_debug.escapeMenuSelection = (m_debug.escapeMenuSelection + 1) % kEscapeMenuItemCount;
    }

    const bool toggleLeft = Input_IsActionPressed(InputAction::MoveLeft);
    const bool toggleRight = Input_IsActionPressed(InputAction::MoveRight);
    if (toggleLeft || toggleRight)
    {
        switch (m_debug.escapeMenuSelection)
        {
        case 1:
            m_debug.effectPlacementPulseEnabled = !m_debug.effectPlacementPulseEnabled;
            break;
        case 2:
            m_debug.effectPasteStickEnabled = !m_debug.effectPasteStickEnabled;
            break;
        case 3:
            m_debug.effectPasteRingEnabled = !m_debug.effectPasteRingEnabled;
            break;
        case 4:
            ToggleEscapeMenuBgm();
            break;
        default:
            break;
        }
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsSouthButtonPressed() ||
        Input_IsMouseLeftPressed();
    if (!confirmPressed)
    {
        return;
    }

    switch (m_debug.escapeMenuSelection)
    {
    case 0:
        m_debug.showEscapeMenu = false;
        break;
    case 1:
        m_debug.effectPlacementPulseEnabled = !m_debug.effectPlacementPulseEnabled;
        break;
    case 2:
        m_debug.effectPasteStickEnabled = !m_debug.effectPasteStickEnabled;
        break;
    case 3:
        m_debug.effectPasteRingEnabled = !m_debug.effectPasteRingEnabled;
        break;
    case 4:
        ToggleEscapeMenuBgm();
        break;
    case 5:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        break;
    case 6:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
        break;
    case 7:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    default:
        break;
    }
}

void GameScene::ToggleEscapeMenuBgm()
{
    if (m_debug.bgmEnabled)
    {
        const float currentVolume = Audio_GetMasterVolume();
        if (currentVolume > 0.001f)
        {
            m_debug.bgmRestoreVolume = currentVolume;
        }
        Audio_SetMasterVolume(0.0f);
        m_debug.bgmEnabled = false;
        return;
    }

    const float restoreVolume = m_debug.bgmRestoreVolume > 0.001f ? m_debug.bgmRestoreVolume : 0.6f;
    Audio_SetMasterVolume(restoreVolume);
    m_debug.bgmEnabled = true;
}

bool GameScene::UpdatePitRestartFlow(float deltaTime)
{
    if (!m_flow.pitRestartActive)
    {
        return false;
    }

    m_flow.pitRestartTimer = std::max(0.0f, m_flow.pitRestartTimer - deltaTime);
    if (m_flow.pitRestartTimer > 0.0f)
    {
        return true;
    }

    Entity* player = FindEntityByTag(kTagPlayer);
    if (player)
    {
        RespawnPlayer(*player);
    }
    else
    {
        m_flow.pitRestartActive = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    return true;
}

bool GameScene::UpdateStageTransitionFlow(float deltaTime)
{
    if (!m_flow.stageTransitionActive)
    {
        return false;
    }

    m_flow.stageTransitionTimer = std::max(0.0f, m_flow.stageTransitionTimer - deltaTime);
    if (m_flow.stageTransitionTimer > 0.0f)
    {
        return true;
    }

    const bool transitioned = m_hasPendingStageTransition &&
        ExecuteStageTransition(
            m_pendingStageTransitionMapCsv,
            m_pendingStageTransitionSpawnMarker,
            m_pendingStageTransitionMarker);
    m_hasPendingStageTransition = false;
    m_pendingStageTransitionMapCsv.clear();
    m_pendingStageTransitionSpawnMarker = '\0';
    m_pendingStageTransitionMarker = '\0';
    m_flow.stageTransitionActive = false;
    m_flow.stageTransitionTimer = 0.0f;
    m_flow.stageTransitionFadeInTimer = transitioned ? kStageTransitionFadeInDuration : 0.0f;
    return true;
}

void GameScene::UpdateFrameTimers(float deltaTime, float gameplayDeltaTime, float effectiveGameplayDeltaTime)
{
    m_player.coyoteTimeRemaining = std::max(0.0f, m_player.coyoteTimeRemaining - effectiveGameplayDeltaTime);
    m_flow.shutterFlashRemaining = std::max(0.0f, m_flow.shutterFlashRemaining - deltaTime);
    m_flow.cameraFlash.pulseRemaining = std::max(0.0f, m_flow.cameraFlash.pulseRemaining - deltaTime);
    m_flow.pitRestartFadeInTimer = std::max(0.0f, m_flow.pitRestartFadeInTimer - deltaTime);
    m_flow.stageTransitionFadeInTimer = std::max(0.0f, m_flow.stageTransitionFadeInTimer - deltaTime);
    const bool previewWasActive = m_flow.developedPhotoPreviewRemaining > 0.0f;
    m_flow.developedPhotoPreviewRemaining = std::max(0.0f, m_flow.developedPhotoPreviewRemaining - deltaTime);
    if (previewWasActive && m_flow.developedPhotoPreviewRemaining <= 0.0f)
    {
        CommitPendingCapturedPhoto();
    }
    m_flow.pickupPulse += gameplayDeltaTime;

    // HP繝舌・貍泌・縺ｮ譖ｴ譁ｰ: 螳櫞P縺ｨ縺ｯ蛻･縺ｫ陦ｨ遉ｺ逕ｨ豈皮紫繧定｣憺俣縺吶ｋ縲・
    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* health = player->GetComponent<HealthComponent>())
        {
            const int maxHp = (std::max)(1, health->GetMaxHealth());
            const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
            const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);

            if (!m_flow.hpUiInitialized)
            {
                m_flow.hpDisplayRatio = targetRatio;
                m_flow.hpDamageLagRatio = targetRatio;
                m_flow.hpDamageFlash = 0.0f;
                m_flow.hpLastRaw = currentHp;
                m_flow.hpUiInitialized = true;
            }
            else
            {
                if (m_flow.hpLastRaw >= 0 && currentHp < m_flow.hpLastRaw)
                {
                    m_flow.hpDamageFlash = 1.0f;
                }
                m_flow.hpLastRaw = currentHp;

                const float displaySpeed = targetRatio < m_flow.hpDisplayRatio ? 10.0f : 14.0f;
                m_flow.hpDisplayRatio += (targetRatio - m_flow.hpDisplayRatio) * std::min(1.0f, deltaTime * displaySpeed);
                m_flow.hpDisplayRatio = std::clamp(m_flow.hpDisplayRatio, 0.0f, 1.0f);

                if (m_flow.hpDamageLagRatio < m_flow.hpDisplayRatio)
                {
                    m_flow.hpDamageLagRatio = m_flow.hpDisplayRatio;
                }
                else
                {
                    const float lagSpeed = 2.4f;
                    m_flow.hpDamageLagRatio += (m_flow.hpDisplayRatio - m_flow.hpDamageLagRatio) * std::min(1.0f, deltaTime * lagSpeed);
                    m_flow.hpDamageLagRatio = std::clamp(m_flow.hpDamageLagRatio, 0.0f, 1.0f);
                }
            }
        }
    }

    m_flow.hpDamageFlash = std::max(0.0f, m_flow.hpDamageFlash - deltaTime * 4.5f);
}

void GameScene::RunGameplayFrame(float gameplayDeltaTime)
{
    UpdatePlayer(gameplayDeltaTime);
    HandlePhotoCapture();
    HandlePhotoSpawn();
    UpdateBarrels(gameplayDeltaTime);
    UpdateBatteries(gameplayDeltaTime);
    UpdateElevatorGimmicks(gameplayDeltaTime);
    UpdateEnemies();
    UpdateShields(gameplayDeltaTime);
    UpdateBullets();
    UpdateDropItems(); // Legacy update order: drop item step
    UpdateGoalVisual(gameplayDeltaTime);
    HandleWorldInteractions();
    RemoveDefeatedEnemies();
    UpdateEffects(gameplayDeltaTime);

    // Flush entities queued during gameplay update
    for (auto& entity : m_pendingEntities)
    {
        m_entities.push_back(std::move(entity));
    }
    m_pendingEntities.clear();
}

