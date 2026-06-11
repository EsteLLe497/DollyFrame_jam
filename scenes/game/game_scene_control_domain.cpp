#include "pch.h"

#include "game_scene_internal.h"
#include "DxLib.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

using namespace game_scene_detail;

void GameScene::UpdateTuningHotReload(float deltaTime)
{
    const ActiveGameSceneScope activeScene(*this);
    if (!m_debug.showTuningPanel)
    {
        return;
    }

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
    if (Input_IsKeyPressed(VK_F9))
    {
        m_debug.sepiaFilmFilterDryRunEnabled = !m_debug.sepiaFilmFilterDryRunEnabled;
    }
    if (Input_IsKeyPressed('6'))
    {
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            auto* enemy = entity->GetComponent<EnemyComponent>();
            auto* boss = entity->GetComponent<MidBoss3Component>();
            if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss3)
            {
                continue;
            }

            boss->debugRequestedAttack = 1;
            m_eventBus.Publish({ EventType::LogMessage, entity.get(), nullptr, "MidBoss3 debug: attack 1", 0.0f, 0.0f });
            break;
        }
    }
    if (Input_IsKeyPressed('7'))
    {
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            auto* enemy = entity->GetComponent<EnemyComponent>();
            auto* boss = entity->GetComponent<MidBoss3Component>();
            if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss3)
            {
                continue;
            }

            boss->debugRequestedAttack = 2;
            m_eventBus.Publish({ EventType::LogMessage, entity.get(), nullptr, "MidBoss3 debug: attack 2", 0.0f, 0.0f });
            break;
        }
    }
    if (Input_IsKeyPressed('8'))
    {
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            auto* enemy = entity->GetComponent<EnemyComponent>();
            auto* boss = entity->GetComponent<MidBoss3Component>();
            if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss3)
            {
                continue;
            }

            boss->debugRequestedAttack = 3;
            m_eventBus.Publish({ EventType::LogMessage, entity.get(), nullptr, "MidBoss3 debug: attack 3", 0.0f, 0.0f });
            break;
        }
    }
    if (Input_IsKeyPressed('9'))
    {
        for (const auto& entity : m_world.Entities())
        {
            if (!entity)
            {
                continue;
            }

            auto* enemy = entity->GetComponent<EnemyComponent>();
            auto* boss = entity->GetComponent<MidBoss3Component>();
            if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss3)
            {
                continue;
            }

            boss->debugRequestedAttack = 4;
            m_eventBus.Publish({ EventType::LogMessage, entity.get(), nullptr, "MidBoss3 debug: attack 4", 0.0f, 0.0f });
            break;
        }
    }
}

bool GameScene::OnCancelAction()
{
    const ActiveGameSceneScope activeScene(*this);
    if (m_ui.merchantShopOpen)
    {
        m_ui.merchantShopOpen = false;
        m_debug.showEscapeMenu = false;
        m_ui.merchantMessage.clear();
        m_ui.merchantMessageTimer = 0.0f;
        return true;
    }

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
