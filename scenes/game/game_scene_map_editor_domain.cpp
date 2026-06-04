#include "pch.h"

#include "game_scene_internal.h"
#include "DxLib.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

using namespace game_scene_detail;

namespace
{
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
            ? "編集対象: マーカー"
            : "編集対象: タイル";
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
    const float maxCameraY = std::max(0.0f, GetMapPixelHeight() - gCameraViewHeight+ 48.0f * 5);
    m_flow.cameraX = std::clamp(m_flow.cameraX + panX * kEditorCameraPanSpeed * deltaTime, 0.0f, maxCameraX);
    m_flow.cameraY = std::clamp(m_flow.cameraY + panY * kEditorCameraPanSpeed * deltaTime, 0.0f, maxCameraY);
}

void GameScene::UpdateMapEditorBrushSelection()
{
    const auto forEachPressedDigit = [&](auto&& onPressed)
    {
        for (int digit = 0; digit <= 9; ++digit)
        {
            if (Input_IsKeyPressed('0' + digit))
            {
                onPressed(digit);
            }
        }
    };

    if (m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Tile)
    {
        forEachPressedDigit([&](int digit) { m_mapEditor.selectedTileValue = digit; });
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

    const char markerBeforeSelection = m_mapEditor.selectedMarker;
    forEachPressedDigit([&](int digit) { m_mapEditor.selectedMarker = PresetIndexToMarker(digit); });
    const auto applyMarkerHotkey = [&](int keyCode, char marker)
    {
        if (Input_IsKeyPressed(keyCode))
        {
            m_mapEditor.selectedMarker = marker;
        }
    };
    constexpr std::array<std::pair<int, char>, 12> kMarkerHotkeys = {{
        { VK_F10, 'M' },
        { VK_F11, 'Y' },
        { VK_F12, '!' },
        { 'H', 'H' },
        { 'I', 'I' },
        { 'J', 'J' },
        { 'K', 'K' },
        { 'L', 'L' },
        { 'N', '?' },
        { 'O', 'O' },
        { 'P', '*' },
        { 'U', 'U' },
    }};
    for (const auto& [keyCode, marker] : kMarkerHotkeys)
    {
        applyMarkerHotkey(keyCode, marker);
    }

    if (m_mapEditor.selectedMarker == '@')
    {
        if (Input_IsKeyPressed('C'))
        {
            m_mapEditor.selectedStageLightTiles = std::max(1, m_mapEditor.selectedStageLightTiles - 1);
        }
        if (Input_IsKeyPressed('V'))
        {
            m_mapEditor.selectedStageLightTiles = std::min(9, m_mapEditor.selectedStageLightTiles + 1);
        }
        if (Input_IsKeyPressed('Z'))
        {
            m_mapEditor.selectedStageLightFixtureTiles = std::max(1, m_mapEditor.selectedStageLightFixtureTiles - 1);
        }
        if (Input_IsKeyPressed('X'))
        {
            m_mapEditor.selectedStageLightFixtureTiles = std::min(4, m_mapEditor.selectedStageLightFixtureTiles + 1);
        }
    }
    else if (IsParameterizedEditorMarker(m_mapEditor.selectedMarker))
    {
        if (Input_IsKeyPressed('C'))
        {
            m_mapEditor.selectedMarkerParameter =
                NormalizeEditorMarkerParameter(m_mapEditor.selectedMarker, m_mapEditor.selectedMarkerParameter - 1);
        }
        if (Input_IsKeyPressed('V'))
        {
            m_mapEditor.selectedMarkerParameter =
                NormalizeEditorMarkerParameter(m_mapEditor.selectedMarker, m_mapEditor.selectedMarkerParameter + 1);
        }
        if (Input_IsKeyPressed('Z'))
        {
            m_mapEditor.selectedMarkerParameter =
                NormalizeEditorMarkerParameter(m_mapEditor.selectedMarker, -m_mapEditor.selectedMarkerParameter);
        }
        if (Input_IsKeyPressed('X'))
        {
            m_mapEditor.selectedMarkerParameter =
                NormalizeEditorMarkerParameter(m_mapEditor.selectedMarker, 0);
        }
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

    if (m_mapEditor.selectedMarker != markerBeforeSelection &&
        IsParameterizedEditorMarker(m_mapEditor.selectedMarker))
    {
        m_mapEditor.selectedMarkerParameter =
            NormalizeEditorMarkerParameter(m_mapEditor.selectedMarker, m_mapEditor.selectedMarkerParameter);
    }
}

void GameScene::HandleMapEditorFileShortcuts(float tileSize)
{
    if (Input_IsKeyPressed(VK_F5))
    {
        if (m_tileMap.SaveToCsv(m_lifecycle.currentMapCsvPath))
        {
            m_mapEditor.statusMessage = "保存しました: " + m_lifecycle.currentMapCsvPath;
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
        if (m_tileMap.LoadFromCsv(m_lifecycle.currentMapCsvPath, tileSize))
        {
            BuildCameraMarkers();
            RefreshMarkerDrivenSystems();
            m_mapEditor.statusMessage = "CSVを再読込しました";
            m_mapEditor.statusMessageTimer = 2.4f;
        }
        else
        {
            m_mapEditor.statusMessage = "再読込に失敗しました";
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
            m_lifecycle.currentMapCsvPath = newMapPath;
            BuildCameraMarkers();
            RefreshMarkerDrivenSystems();
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
            m_lifecycle.currentMapCsvPath = duplicatedMapPath;
            m_mapEditor.statusMessage = "別名保存しました: " + duplicatedMapPath;
            m_mapEditor.statusMessageTimer = 3.0f;
        }
        else
        {
            m_mapEditor.statusMessage = "別名保存に失敗しました";
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
            const int beforeParameter = m_tileMap.GetMarkerParameter(column, row);
            int markerParameter = 0;
            if (m_mapEditor.selectedMarker == '@')
            {
                markerParameter = EncodeStageLightMarkerParameter(
                    m_mapEditor.selectedStageLightTiles,
                    m_mapEditor.selectedStageLightFixtureTiles);
            }
            else if (IsParameterizedEditorMarker(m_mapEditor.selectedMarker))
            {
                markerParameter = NormalizeEditorMarkerParameter(
                    m_mapEditor.selectedMarker,
                    m_mapEditor.selectedMarkerParameter);
            }
            m_tileMap.SetMarker(column, row, m_mapEditor.selectedMarker, markerParameter);
            const char after = static_cast<char>(std::toupper(static_cast<unsigned char>(m_mapEditor.selectedMarker)));
            RefreshMarkerDrivenSystemsByMarkerChange(before, after);
            if (before == after && beforeParameter != markerParameter)
            {
                switch (after)
                {
                case 'P':
                case 'F':
                    RefreshMarkerLightsFromMarkers();
                    break;
                case '@':
                    RefreshStageLightsFromMarkers();
                    break;
                case 'U':
                case 'Z':
                    RefreshLaserTurretsFromMarkers();
                    break;
                case 'K':
                case 'L':
                case 'Q':
                case 'J':
                case 'O':
                case 'X':
                    RefreshLinkedGimmicksFromMarkers();
                    break;
                default:
                    break;
                }
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
            RefreshMarkerDrivenSystemsByMarkerChange(before, '\0');
        }
    }
}
