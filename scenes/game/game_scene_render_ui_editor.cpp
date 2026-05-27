#include "pch.h"

#include "game_scene_internal.h"

#include <cctype>

#include "DxLib.h"

using namespace game_scene_detail;

void GameScene::DrawMapEditorOverlay() const
{
    if (!m_mapEditor.active)
    {
        return;
    }

    const float viewScale = GetViewScale();
    const float tileSize = m_tileMap.GetTileSize();
    if (viewScale <= 0.0f || tileSize <= 0.0f)
    {
        return;
    }

    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const float worldX = m_flow.cameraX + (static_cast<float>(mouseX) - GetViewOriginX()) / viewScale;
    const float worldY = m_flow.cameraY + (static_cast<float>(mouseY) - GetViewOriginY()) / viewScale;
    const int column = static_cast<int>(std::floor(worldX / tileSize));
    const int row = static_cast<int>(std::floor(worldY / tileSize));
    int hoveredTileValue = 0;
    char hoveredMarkerValue = '\0';
    int hoveredMarkerParameter = 0;
    const bool markerMode = m_mapEditor.brushTarget == GameSceneMapEditorState::BrushTarget::Marker;
    const int cursorOuterColor = markerMode ? GetColor(116, 220, 255) : GetColor(255, 225, 120);
    const int cursorInnerColor = markerMode ? GetColor(72, 168, 255) : GetColor(255, 170, 80);
    if (column >= 0 && row >= 0 && column < m_tileMap.GetWidth() && row < m_tileMap.GetHeight())
    {
        hoveredTileValue = m_tileMap.GetTile(column, row);
        hoveredMarkerValue = m_tileMap.GetMarker(column, row);
        hoveredMarkerParameter = m_tileMap.GetMarkerParameter(column, row);
        const int left = static_cast<int>(std::round(GetViewOriginX() + (static_cast<float>(column) * tileSize - m_flow.cameraX) * viewScale));
        const int top = static_cast<int>(std::round(GetViewOriginY() + (static_cast<float>(row) * tileSize - m_flow.cameraY) * viewScale));
        const int right = static_cast<int>(std::round(GetViewOriginX() + ((static_cast<float>(column) + 1.0f) * tileSize - m_flow.cameraX) * viewScale));
        const int bottom = static_cast<int>(std::round(GetViewOriginY() + ((static_cast<float>(row) + 1.0f) * tileSize - m_flow.cameraY) * viewScale));
        DrawBox(left, top, right, bottom, cursorOuterColor, FALSE);
        DrawBox(left + 1, top + 1, right - 1, bottom - 1, cursorInnerColor, FALSE);
    }

    const int panelLeft = 22;
    const int panelTop = 22;
    const int panelRight = 560;
    const int panelBottom = 286;
    const char selectedMarker = m_mapEditor.selectedMarker;
    const char markerLabel = selectedMarker == '\0' ? '-' : selectedMarker;
    const char hoveredMarkerLabel = hoveredMarkerValue == '\0' ? '-' : static_cast<char>(std::toupper(static_cast<unsigned char>(hoveredMarkerValue)));
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 206);
    DrawBox(panelLeft, panelTop, panelRight, panelBottom, GetColor(16, 22, 30), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(panelLeft, panelTop, panelRight, panelBottom, GetColor(198, 214, 232), FALSE);

    DrawString(panelLeft + 16, panelTop + 14, "マップエディター", GetColor(244, 250, 255));
    DrawBox(panelLeft + 330, panelTop + 10, panelRight - 14, panelTop + 34, markerMode ? GetColor(28, 78, 134) : GetColor(96, 72, 24), TRUE);
    DrawBox(panelLeft + 330, panelTop + 10, panelRight - 14, panelTop + 34, markerMode ? GetColor(116, 220, 255) : GetColor(255, 220, 120), FALSE);
    DrawString(panelLeft + 342, panelTop + 15, markerMode ? "MARKER MODE" : "TILE MODE", GetColor(244, 250, 255));
    DrawString(panelLeft + 16, panelTop + 38, "F4: 閉じる  M: タイル/マーカー切替  WASD/十字: カメラ移動", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 58, "左ドラッグ: 塗る  右ドラッグ: 消す", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 78, "タイル: 0-9 / Q,E / F9(10)", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 96, "マーカー: 0(None),1(G),2(S),3(E),4(T),5(W),6(R),7(B),8(V),9(C),F10(M Log),F11(Y),N(? Boss1),F12(! Boss2),H,I,J,K,L,O,U,Q,E", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 114, "@照明: C/V 光の長さ  Z/X 本体横幅", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 132, "P/F: 正数=半径  負数=番号  Z 符号反転  X 初期値", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 150, "&: 正数=耐久  負数=番号", GetColor(168, 192, 220));
    DrawString(panelLeft + 16, panelTop + 168, "F5: 保存  F6: CSV再読込  F7: 新規作成  F8: 別名保存", GetColor(168, 192, 220));
    DrawFormatString(
        panelLeft + 16,
        panelTop + 174,
        markerMode ? GetColor(180, 238, 255) : GetColor(255, 236, 160),
        "編集モード: %s",
        markerMode ? "マーカー" : "タイル");
    DrawFormatString(
        panelLeft + 16,
        panelTop + 194,
        GetColor(255, 236, 160),
        "選択タイル: %d  /  選択マーカー: %c  /  param:%d  /  @光:%d 本体:%d",
        m_mapEditor.selectedTileValue,
        markerLabel,
        m_mapEditor.selectedMarkerParameter,
        m_mapEditor.selectedStageLightTiles,
        m_mapEditor.selectedStageLightFixtureTiles);
    DrawFormatString(
        panelLeft + 16,
        panelTop + 214,
        GetColor(255, 236, 160),
        "カーソル: (%d,%d)  tile=%d marker=%c param=%d",
        column,
        row,
        hoveredTileValue,
        hoveredMarkerLabel,
        hoveredMarkerParameter);
    DrawFormatString(
        panelLeft + 16,
        panelTop + 234,
        GetColor(220, 230, 244),
        "現在マップ: %s",
        GetMapDisplayName(gCurrentMapCsvPath).c_str());
    if (!m_mapEditor.statusMessage.empty())
    {
        DrawString(panelLeft + 16, panelTop + 258, m_mapEditor.statusMessage.c_str(), GetColor(142, 236, 166));
    }
}
