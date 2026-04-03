#include "game_scene_internal.h"
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
    constexpr int kEscapeMenuItemCount = 7;
    constexpr int kEscapeMenuPanelWidth = 560;
    constexpr int kEscapeMenuPanelHeight = 360;
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
    constexpr int kMarkerPresetCount = 11;

    int MarkerToPresetIndex(char marker)
    {
        switch (static_cast<char>(std::toupper(static_cast<unsigned char>(marker))))
        {
        case 'G':
            return 1;
        case 'S':
            return 2;
        case 'E':
            return 3;
        case 'T':
            return 4;
        case 'W':
            return 5;
        case 'R':
            return 6;
        case 'B':
            return 7;
        case 'V':
            return 8;
        case 'C':
            return 9;
        case 'M':
            return 10;
        default:
            return 0;
        }
    }

    char PresetIndexToMarker(int index)
    {
        switch (index)
        {
        case 1:
            return 'G';
        case 2:
            return 'S';
        case 3:
            return 'E';
        case 4:
            return 'T';
        case 5:
            return 'W';
        case 6:
            return 'R';
        case 7:
            return 'B';
        case 8:
            return 'V';
        case 9:
            return 'C';
        case 10:
            return 'M';
        default:
            return '\0';
        }
    }

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

    // 3状態（撮影/配置/現像プレビュー）から、トレイ表示とスロー演出を一元決定する。
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
    // フォーカス中だけゲーム全体を減速させる。
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
    m_mapEditor.statusMessageTimer = std::max(0.0f, m_mapEditor.statusMessageTimer - deltaTime);
    if (m_mapEditor.statusMessageTimer <= 0.0f)
    {
        m_mapEditor.statusMessage.clear();
    }

    if (Input_IsKeyPressed(VK_F4))
    {
        m_mapEditor.active = false;
        return;
    }

    if (Input_IsKeyPressed('M'))
    {
        m_mapEditor.brushTarget =
            m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile
            ? GameSceneMapEditorState::BrushTarget::Marker
            : GameSceneMapEditorState::BrushTarget::Tile;
        m_mapEditor.statusMessage =
            m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Marker
            ? "編集対象: マーカー"
            : "編集対象: タイル";
        m_mapEditor.statusMessageTimer = 1.8f;
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

    // エディターモード中はカメラを直接パンして編集対象を移動する。
    // キーボード（WASD/矢印）とゲームパッド軸の両方を扱う。
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
    }
    else
    {
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

    if (Input_IsKeyPressed(VK_F5))
    {
        if (m_tileMap.SaveToCsv(gCurrentMapCsvPath))
        {
            m_mapEditor.statusMessage = "保存しました: " + gCurrentMapCsvPath;
            m_mapEditor.statusMessageTimer = 2.4f;
        }
        else
        {
            m_mapEditor.statusMessage = "保存に失敗しました";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }

    if (Input_IsKeyPressed(VK_F6))
    {
        if (m_tileMap.LoadFromCsv(gCurrentMapCsvPath, tileSize))
        {
            BuildCameraMarkers();
            RefreshEnemiesFromMarkers();
            m_mapEditor.statusMessage = "CSVを再読み込みしました";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
        else
        {
            m_mapEditor.statusMessage = "再読み込みに失敗しました";
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
            BuildCameraMarkers();
            RefreshEnemiesFromMarkers();
            m_flow.cameraX = 0.0f;
            m_flow.cameraY = 0.0f;
            m_mapEditor.statusMessage = "新規マップを作成: " + newMapPath;
            m_mapEditor.statusMessageTimer = 3.0f;
        }
        else
        {
            m_mapEditor.statusMessage = "新規マップ作成に失敗しました";
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
            m_mapEditor.statusMessage = "別名保存しました: " + duplicatedMapPath;
            m_mapEditor.statusMessageTimer = 3.0f;
        }
        else
        {
            m_mapEditor.statusMessage = "別名保存に失敗しました";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
    }

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
            const bool touchesEnemyMarker =
                before == 'W' || before == 'R' || before == 'M' ||
                after == 'W' || after == 'R' || after == 'M';
            if (touchesEnemyMarker && before != after)
            {
                RefreshEnemiesFromMarkers();
            }
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
            if (before == 'W' || before == 'R' || before == 'M')
            {
                RefreshEnemiesFromMarkers();
            }
        }
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
            else if (marker == 'M')
            {
                Entity& boss = SpawnStagePrefab(prefabs, "sandbox_shield_boss", markerX, markerY);
                placeEnemyAtMarker(boss);
            }
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
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        break;
    case 5:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
        break;
    case 6:
        m_debug.showEscapeMenu = false;
        m_eventBus.Publish({ EventType::ExitApplicationRequested, nullptr, nullptr, "", 0.0f, 0.0f });
        break;
    default:
        break;
    }
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
    m_flow.pitRestartFadeInTimer = std::max(0.0f, m_flow.pitRestartFadeInTimer - deltaTime);
    m_flow.stageTransitionFadeInTimer = std::max(0.0f, m_flow.stageTransitionFadeInTimer - deltaTime);
    const bool previewWasActive = m_flow.developedPhotoPreviewRemaining > 0.0f;
    m_flow.developedPhotoPreviewRemaining = std::max(0.0f, m_flow.developedPhotoPreviewRemaining - deltaTime);
    if (previewWasActive && m_flow.developedPhotoPreviewRemaining <= 0.0f)
    {
        CommitPendingCapturedPhoto();
    }
    m_flow.pickupPulse += gameplayDeltaTime;
}

void GameScene::RunGameplayFrame(float gameplayDeltaTime)
{
    UpdatePlayer(gameplayDeltaTime);
    HandlePhotoCapture();
    HandlePhotoSpawn();
    UpdateBarrels(gameplayDeltaTime);
    UpdateEnemies();
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

