#include "game_scene_internal.h"
#include "DxLib.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

using namespace game_scene_detail;

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

bool GameScene::OnCancelAction()
{
    if (m_mapEditor.active)
    {
        m_mapEditor.active = false;
        return true;
    }

    if (m_debug.showTuningPanel)
    {
        m_debug.showTuningPanel = false;
        return true;
    }

    m_debug.showEscapeMenu = !m_debug.showEscapeMenu;
    if (!m_debug.showEscapeMenu)
    {
        return true;
    }

    m_debug.escapeMenuSelection = 0;
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
    m_photo.placement.blockedByUi = false;
    return true;
}
