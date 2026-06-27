#include "pch.h"

#include "result_scene.h"

#include "directX.h"
#include "game_session.h"
#include "imgui.h"
#include "input.h"
#include "logger.h"
#include "resource_manager.h"
#include "shader.h"
#include "sprite.h"
#include <tracy/Tracy.hpp>

namespace
{
    const char* ToReasonLabel(GameEndReason reason)
    {
    switch (reason)
    {
    case GameEndReason::GoalReached:
            return "クリア";
        case GameEndReason::TimeUp:
            return "時間切れ";
        case GameEndReason::HpZero:
            return "HP0";
        case GameEndReason::None:
        default:
            return "結果なし";
        }
    }
}

ResultScene::ResultScene()
    : m_whiteTexture(-1)
    , m_blinkTimer(0.0f)
    , m_showPrompt(true)
{
}

const char* ResultScene::GetSceneId() const
{
    return "result";
}

void ResultScene::OnEnter(ResourceManager& resources)
{
    ZoneScoped;
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    m_eventBus.Clear();
    m_blinkTimer = 0.0f;
    m_showPrompt = true;
    Logger::Info("ResultScene entered");
}

void ResultScene::Update(float deltaTime)
{
    ZoneScoped;
    m_blinkTimer += deltaTime;
    if (m_blinkTimer >= 0.45f)
    {
        m_blinkTimer = 0.0f;
        m_showPrompt = !m_showPrompt;
    }

    if (Input_IsActionPressed(InputAction::Confirm))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }

    if (Input_IsActionPressed(InputAction::RestartScene))
    {
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
}

void ResultScene::Draw()
{
    DrawBackdrop();
}

void ResultScene::DrawDebugUI()
{
    const GameSessionState& session = GameSession_Get();
    ImGui::Begin("リザルト");
    ImGui::Text("試作版のリザルト画面です");
    ImGui::Text("Result: %s", ToReasonLabel(session.endReason));
    ImGui::Text("HP: %d / %d", session.currentHp, session.maxHp);
    ImGui::Text("残り時間: %.1f / %.1f", session.timeRemaining, session.timeLimit);
    ImGui::Text("Enter / Space / ゲームパッドA でタイトルへ戻る");
    ImGui::Text("R でゲームシーンをリトライ");
    ImGui::Text("プロンプト表示: %s", m_showPrompt ? "あり" : "なし");
    ImGui::End();
}

EventBus* ResultScene::GetEventBus()
{
    return &m_eventBus;
}

void ResultScene::DrawBackdrop() const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached;
    const float accentR = cleared ? 0.18f : 0.78f;
    const float accentG = cleared ? 0.62f : 0.24f;
    const float accentB = cleared ? 0.32f : 0.14f;

    Shader_SetTint(0.11f, 0.07f, 0.10f, 1.0f);
    SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(accentR, accentG, accentB, 1.0f);
    SpriteDraw(m_whiteTexture, 112.0f, 112.0f, static_cast<float>(SCREEN_WIDTH) - 224.0f, 20.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.18f, 0.10f, 0.16f, 1.0f);
    SpriteDraw(m_whiteTexture, 112.0f, 164.0f, static_cast<float>(SCREEN_WIDTH) - 224.0f, 220.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    Shader_SetTint(0.95f, 0.84f, 0.32f, 1.0f);
    SpriteDraw(m_whiteTexture, 152.0f, 206.0f, static_cast<float>(SCREEN_WIDTH) - 304.0f, 30.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    if (m_showPrompt)
    {
        Shader_SetTint(0.88f, 0.88f, 0.88f, 1.0f);
        SpriteDraw(m_whiteTexture, 192.0f, 430.0f, static_cast<float>(SCREEN_WIDTH) - 384.0f, 34.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}
