#include "pch.h"

#include "game_scene_internal.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

#include "imgui_layer.h"

using namespace game_scene_detail;

void GameScene::DrawDebugUI()
{
    const ActiveGameSceneScope activeScene(*this);
    ImGuiLayer_SetFoundationOverlayVisible(!m_debug.hideNonPhotoUi);
    DrawUiAdjustmentWindow();
    if (m_debug.hideNonPhotoUi)
    {
        DrawTestPhotoPanel();
        return;
    }

    const auto toMidBoss2StateLabel = [](MidBoss2State state) -> const char*
    {
        switch (state)
        {
        case MidBoss2State::Idle: return "待機";
        case MidBoss2State::SpearJump: return "ワープ";
        case MidBoss2State::SpearThrow: return "攻撃";
        case MidBoss2State::SpearLanding: return "着地";
        case MidBoss2State::SpearCooldown: return "再配置";
        case MidBoss2State::BeamCharge: return "チャージ";
        case MidBoss2State::BeamFire: return "ビーム発射";
        case MidBoss2State::BeamCooldown: return "再配置";
        case MidBoss2State::Damaged: return "被弾";
        case MidBoss2State::Dead: return "撃破";
        default: return "不明";
        }
    };

    ImGui::Begin("ゲームシーン");
    ImGui::Text("2D 写真プラットフォーム試作");
    ImGui::Text("移動: A / D またはスティック");
    ImGui::Text("ジャンプ: W / Space / ゲームパッドA");
    ImGui::Text("回避: 左Shift / 右Shift");
    ImGui::Text("カメラ: 右クリック押しっぱなし");
    ImGui::Text("撮影: カメラモード中に左クリック");
    ImGui::Text("フィルター: C 切替  1 なし  2 暖色  3 寒色  4 反転  5 セピア");
    ImGui::Text("出現物生成: E 長押し");
    ImGui::Text("設置: F 反転  B 橋");
    ImGui::Text("ステージ: 左から順にギミックを解く");
    ImGui::Text("再開: R  タイトル: T");
    ImGui::Text("衝突デバッグ: F3 (%s)", m_debug.showCollisionDebug ? "オン" : "オフ");
    ImGui::Text("エンティティ数: %d", static_cast<int>(m_world.Entities().size()));
    ImGui::Text("CSV タイルマップ: %s", m_tileMap.IsLoaded() ? "読み込み済み" : "未読込");
    ImGui::Text("タイルマップサイズ: %d x %d (tile %.0f)",
        m_tileMap.GetWidth(),
        m_tileMap.GetHeight(),
        m_tileMap.GetTileSize());
    ImGui::Text("カメラX: %.1f / %.1f", m_flow.cameraX, std::max(0.0f, GetMapPixelWidth() - gCameraViewWidth));
    ImGui::Text("カメラY: %.1f / %.1f", m_flow.cameraY, std::max(0.0f, GetMapPixelHeight() - gCameraViewHeight));
    ImGui::Text("カメラY追従: %s", gCameraFollowY >= 0.5f ? "オン" : "オフ");
    bool followY = gCameraFollowY >= 0.5f;
    if (ImGui::Checkbox("カメラY追従を有効にする", &followY))
    {
        gCameraFollowY = followY ? 1.0f : 0.0f;
    }
    ImGui::Text("表示倍率: %.2f", GetViewScale());
    ImGui::Text("制限時間: なし");
    ImGui::Text("撮影済み写真: %s", m_photo.capture.hasPhoto ? "あり" : "なし");
    ImGui::Text("保存写真数: %d / 3",
        static_cast<int>(std::count_if(
            m_photo.savedCaptures.begin(),
            m_photo.savedCaptures.end(),
            [](const PhotoCaptureState& capture) { return capture.hasPhoto; })));
    ImGui::Text("選択スロット: %d", m_photo.selectedCaptureSlot + 1);
    ImGui::Text("現像プレビュー: %.2f", m_ui.developedPhotoPreviewRemaining);
    ImGui::Text("選択フィルター: %s", GetPhotoFilterThemeLabel(m_photo.capture.selectedTheme));
    ImGui::Text("撮影フィルター: %s", GetPhotoFilterThemeLabel(m_photo.capture.capturedTheme));
    ImGui::Text("生成コピー: %s", m_photo.groups.hasSpawnedCopy ? "あり" : "なし");
    ImGui::Text("コピーグループ: %d / 3", m_photo.groups.activeGroupCount);
    ImGui::Text("敵数: %d", m_flow.enemyCount);
    ImGui::Text("設置モード: %s", m_photo.placement.active ? "オン" : "オフ");
    ImGui::Text("マップエディター: %s (F4)", m_mapEditor.active ? "オン" : "オフ");
    ImGui::Text("設置反転: %s", m_photo.placement.flipX ? "オン" : "オフ");
    ImGui::Text("橋: %s", m_photo.placement.bridgeEnabled ? "オン" : "オフ");
    ImGui::Text("カメラモード: %s", m_flow.cameraMode ? "オン" : "オフ");
    ImGui::Text("集中スロー: %s", ((m_flow.cameraMode && m_flow.captureSlowRemaining > 0.0f) || ((m_photo.capture.hasPhoto && Input_IsActionDown(InputAction::HoldPlacement)) && m_flow.placementSlowRemaining > 0.0f)) ? "オン" : "オフ");
    ImGui::Text("撮影集中: %.2f", m_flow.captureSlowRemaining);
    ImGui::Text("設置集中: %.2f", m_flow.placementSlowRemaining);
    ImGui::Text("撮影フレームサイズ: %.0f x %.0f px", gCaptureFrameWidthPx, gCaptureFrameHeightPx);
    ImGui::Text("ゴール: %s", m_flow.goalUnlocked ? "解放" : "ロック");
    ImGui::Text("ゴール接触: %s", m_flow.playerTouchingTarget ? "接触" : "未接触");
    ImGui::Text("危険物接触: %s", m_flow.playerTouchingHazard ? "接触" : "未接触");
    ImGui::Checkbox("衝突デバッグを表示", &m_debug.showCollisionDebug);
    ImGui::Checkbox("HPダメージを有効にする", &m_debug.playerHealthDamageEnabled);

    if (ImGui::CollapsingHeader("ボリューム霧", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto& fog = m_tuning.volumetricFog;
        ImGui::Checkbox("有効##volumetricFog", &fog.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("範囲を表示##volumetricFog", &fog.showBounds);
        ImGui::TextUnformatted("位置と大きさは、現在のゲーム画面左上からのピクセル値です。");
        ImGui::DragFloat("X##volumetricFog", &fog.positionX, 1.0f, -1920.0f, 3840.0f, "%.1f px");
        ImGui::DragFloat("Y##volumetricFog", &fog.positionY, 1.0f, -1080.0f, 2160.0f, "%.1f px");
        ImGui::DragFloat("幅##volumetricFog", &fog.width, 1.0f, 1.0f, 3840.0f, "%.1f px");
        ImGui::DragFloat("高さ##volumetricFog", &fog.height, 1.0f, 1.0f, 2160.0f, "%.1f px");
        ImGui::SeparatorText("霧");
        ImGui::DragFloat("密度##volumetricFog", &fog.density, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("不透明度##volumetricFog", &fog.opacity, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("画面を覆う割合##volumetricFog", &fog.coverage, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("濃淡のばらつき##volumetricFog", &fog.variation, 0.01f, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat("模様スケール##volumetricFog", &fog.noiseScale, 0.01f, 0.1f, 6.0f, "%.2f");
        ImGui::DragFloat("流れる速さ##volumetricFog", &fog.driftSpeed, 0.001f, 0.001f, 0.20f, "%.3f");
        ImGui::SeparatorText("光源位置");
        ImGui::TextUnformatted("0～1が霧矩形内、負数や1以上で画面外の光源になります。");
        ImGui::DragFloat(
            "光源X##volumetricFog",
            &fog.lightPositionX,
            0.01f,
            -1.0f,
            2.0f,
            "%.2f");
        ImGui::DragFloat(
            "光源Y##volumetricFog",
            &fog.lightPositionY,
            0.01f,
            -1.0f,
            2.0f,
            "%.2f");
        ImGui::SeparatorText("ゴッドレイ");
        ImGui::DragFloat(
            "光条の強さ##volumetricFog",
            &fog.godRayIntensity,
            0.05f,
            0.0f,
            8.0f,
            "%.2f");
        ImGui::DragFloat(
            "光条の長さ##volumetricFog",
            &fog.godRayLength,
            0.01f,
            0.0f,
            1.5f,
            "%.2f");
        ImGui::DragFloat(
            "光条の減衰##volumetricFog",
            &fog.godRayDecay,
            0.005f,
            0.0f,
            1.0f,
            "%.3f");
        ImGui::DragFloat(
            "光条のくっきり度##volumetricFog",
            &fog.godRayContrast,
            0.02f,
            0.25f,
            4.0f,
            "%.2f");

        if (ImGui::Button("全面まばら霧プリセット##volumetricFog"))
        {
            fog.positionX = 0.0f;
            fog.positionY = 0.0f;
            fog.width = static_cast<float>(kVirtualScreenWidth);
            fog.height = static_cast<float>(kVirtualScreenHeight);
            fog.density = 0.48f;
            fog.opacity = 0.62f;
            fog.coverage = 0.88f;
            fog.variation = 0.72f;
            fog.noiseScale = 1.30f;
            fog.driftSpeed = 0.035f;
        }
        ImGui::SameLine();
        if (ImGui::Button("保存##volumetricFog"))
        {
            WriteTuningJsonFile();
            m_debug.saveStatusMessage = "ボリューム霧設定を assets/tuning.json に保存しました。";
            m_debug.saveStatusTimer = 3.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("再読込##volumetricFog"))
        {
            LoadTuningJsonFile();
            m_debug.saveStatusMessage = "ボリューム霧設定を再読込しました。";
            m_debug.saveStatusTimer = 3.0f;
        }
    }

    if (auto* player = FindEntityByTag(kTagPlayer))
    {
        if (auto* transform = player->GetComponent<TransformComponent>())
        {
            ImGui::Text("プレイヤー位置: %.1f, %.1f", transform->x, transform->y);
            if (m_flow.cameraMode)
            {
                float frameX = 0.0f;
                float frameY = 0.0f;
                float frameWidth = 0.0f;
                float frameHeight = 0.0f;
                GetCaptureFrameRect(*transform, frameX, frameY, frameWidth, frameHeight);
                ImGui::Text("撮影フレーム: %.1f, %.1f, %.1f, %.1f", frameX, frameY, frameWidth, frameHeight);
            }
        }
        ImGui::Text("接地: %s", m_player.grounded ? "あり" : "なし");
        ImGui::Text("速度: %.1f, %.1f", m_player.velocityX, m_player.velocityY);
        ImGui::Text("回避: %.2f / クールダウン: %.2f", m_player.dodgeRemaining, m_player.dodgeCooldownRemaining);
        ImGui::Text("コヨーテ: %.2f", m_player.coyoteTimeRemaining);
        if (auto* health = player->GetComponent<HealthComponent>())
        {
            ImGui::Text("プレイヤーHP: %d / %d", health->GetCurrentHealth(), health->GetMaxHealth());
        }
        if (auto* cooldown = player->GetComponent<DamageCooldownComponent>())
        {
            ImGui::Text("ダメージクールダウン: %.2f", cooldown->GetRemainingSeconds());
        }
    }

    ImGui::Text("このフレームのイベント: %d", static_cast<int>(m_eventBus.GetEvents().size()));
    ImGui::End();
    DrawMidBoss2DebugWindow();
    DrawProgressSavePanel();
    DrawTestPhotoPanel();
}

void GameScene::DrawMidBoss2DebugWindow()
{
    const auto toMidBoss2StateLabel = [](MidBoss2State state) -> const char*
    {
        switch (state)
        {
        case MidBoss2State::Idle: return "待機";
        case MidBoss2State::SpearJump: return "ワープ";
        case MidBoss2State::SpearThrow: return "攻撃";
        case MidBoss2State::SpearLanding: return "着地";
        case MidBoss2State::SpearCooldown: return "再配置";
        case MidBoss2State::BeamCharge: return "チャージ";
        case MidBoss2State::BeamFire: return "ビーム発射";
        case MidBoss2State::BeamCooldown: return "再配置";
        case MidBoss2State::Damaged: return "被弾";
        case MidBoss2State::Dead: return "撃破";
        default: return "不明";
        }
    };

    ImGui::SetNextWindowSize(ImVec2(520.0f, 700.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("ボス2"))
    {
        ImGui::End();
        return;
    }

    bool foundBoss = false;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        auto* boss = entity->GetComponent<MidBoss2Component>();
        auto* transform = entity->GetComponent<TransformComponent>();
        if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss2 || !transform)
        {
            continue;
        }

        foundBoss = true;

        ImGui::Text("ボス当たり判定: %.1f, %.1f, %.1f, %.1f",
            transform->x,
            transform->y,
            transform->width * transform->scale,
            transform->height * transform->scale);
        ImGui::Text("状態: %s", toMidBoss2StateLabel(boss->state));
        ImGui::Text("攻撃フロー: %d", boss->attackFlowStep);
        ImGui::Text("クールダウン残り: %.2f", boss->cooldownRemaining);
        ImGui::Text("撮影判定: %s", boss->captureWindowActive ? "あり" : "なし");
        ImGui::Text("槍の向き: %.2f, %.2f", boss->lastSpearDirX, boss->lastSpearDirY);
        ImGui::Text("最後のビーム側: %s", boss->lastBeamTeleportLeftSide ? "左" : "右");
        ImGui::Text("次の槍開始側: %s", boss->nextSpearStartLeftSide ? "左" : "右");

        ImGui::SeparatorText("戦闘パラメータ");
        ImGui::DragInt("槍ダメージ", &boss->params.spearDamage, 1.0f, 0, 999);
        ImGui::DragFloat("槍フェード時間", &boss->params.spearFadeTime, 0.05f, 0.05f, 10.0f, "%.2f");
        ImGui::DragFloat("槍間隔", &boss->params.spearInterval, 0.01f, 0.05f, 10.0f, "%.2f");
        ImGui::DragFloat("着地後クールダウン", &boss->params.spearCooldownAfterLanding, 0.05f, 0.05f, 20.0f, "%.2f");
        ImGui::DragFloat("着地停止時間", &boss->params.spearLandingPauseTime, 0.01f, 0.0f, 5.0f, "%.2f");
        ImGui::DragFloat("槍ジャンプ高さグリッド", &boss->params.spearJumpHeightGrid, 0.1f, 0.0f, 20.0f, "%.2f");
        ImGui::DragFloat("槍ジャンプ横グリッド", &boss->params.spearJumpHorizontalGrid, 0.1f, 0.0f, 40.0f, "%.2f");
        ImGui::DragFloat("ビーム準備時間", &boss->params.beamChargeTime, 0.05f, 0.05f, 20.0f, "%.2f");
        ImGui::DragFloat("ビーム秒間ダメージ", &boss->params.beamDamagePerSecond, 0.05f, 0.0f, 50.0f, "%.2f");
        ImGui::DragFloat("ビーム高さグリッド", &boss->params.beamHeightGrid, 0.05f, 0.5f, 20.0f, "%.2f");
        ImGui::DragFloat("発射後クールダウン", &boss->params.beamCooldownAfterFire, 0.05f, 0.05f, 20.0f, "%.2f");
        ImGui::SeparatorText("ワープ高さ");
        ImGui::TextUnformatted("値を小さくするとボスは画面下側に下がります。");
        ImGui::TextUnformatted("実際の高さ = 基準高さ + スロット補正。");
        ImGui::DragFloat("基準高さグリッド", &boss->params.teleportHoverBaseGrid, 0.1f, 0.0f, 20.0f, "%.2f");

        ImGui::SeparatorText("ワープエフェクト");
        ImGui::DragInt("スパーク数", &boss->params.teleportSparkCount, 1.0f, 0, 256);
        ImGui::DragFloat("最小粒子サイズ", &boss->params.teleportSparkMinSize, 0.05f, 0.1f, 12.0f, "%.2f");
        ImGui::DragFloat("最大粒子サイズ", &boss->params.teleportSparkMaxSize, 0.05f, 0.1f, 12.0f, "%.2f");
        ImGui::DragFloat("広がり倍率", &boss->params.teleportSparkSpreadScale, 0.05f, 0.0f, 8.0f, "%.2f");
        ImGui::DragFloat("粒子寿命", &boss->params.teleportSparkLifetime, 0.01f, 0.01f, 5.0f, "%.2f");

        ImGui::SeparatorText("ワープスロット");
        ImGui::Text("値はグリッド単位です。");
        const auto drawTeleportSlots = [&](const char* sectionLabel, std::array<MidBoss2Component::TeleportSlotConfig, 3>& slots)
        {
            ImGui::PushID(sectionLabel);
            ImGui::TextUnformatted(sectionLabel);
            for (int index = 0; index < static_cast<int>(slots.size()); ++index)
            {
                auto& slot = slots[static_cast<size_t>(index)];
                float actualHeightGrid = boss->params.teleportHoverBaseGrid + slot.hoverHeightOffsetGrid;

                ImGui::PushID(index);
                ImGui::Text("スロット %d", index + 1);
                ImGui::DragFloat("中心Xグリッド", &slot.centerGridX, 0.1f, 0.0f, 120.0f, "%.2f");
                if (ImGui::DragFloat("ワープ高さグリッド", &actualHeightGrid, 0.1f, 0.0f, 20.0f, "%.2f"))
                {
                    slot.hoverHeightOffsetGrid = actualHeightGrid - boss->params.teleportHoverBaseGrid;
                }
                ImGui::Text("基準からの高さ補正: %.2f", slot.hoverHeightOffsetGrid);
                ImGui::PopID();
            }
            ImGui::PopID();
        };
        if (ImGui::BeginTable("TeleportSlotsTable", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            drawTeleportSlots("左", boss->params.leftTeleportSlots);
            ImGui::TableNextColumn();
            drawTeleportSlots("右", boss->params.rightTeleportSlots);
            ImGui::EndTable();
        }
        ImGui::TextUnformatted("ワールド表示には実際のワープ枠が出ます。");
        ImGui::TextUnformatted("左 = シアン、右 = オレンジ、ビーム = 金色。");
        ImGui::TextUnformatted("金色 = ビームワープ。赤 = アリーナ境界で制限。");

        ImGui::SeparatorText("保存");
        ImGui::Text("ファイル: %s", kTuningFilePath);
        if (ImGui::Button("ボス2パラメータを保存"))
        {
            m_tuning.midBoss2Params = boss->params;
            ApplyMidBoss2TuningToActiveBosses();
            WriteTuningJsonFile();

            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
            if (!ec)
            {
                m_debug.tuningFileWriteTime = writeTime;
                m_debug.hasTuningFileWriteTime = true;
            }

            m_debug.saveStatusMessage = "ボス2パラメータを assets/tuning.json に保存しました。";
            m_debug.saveStatusTimer = 3.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("ボス2パラメータを再読込"))
        {
            LoadTuningJsonFile();
            ApplyMidBoss2TuningToActiveBosses();

            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
            if (!ec)
            {
                m_debug.tuningFileWriteTime = writeTime;
                m_debug.hasTuningFileWriteTime = true;
            }

            m_debug.saveStatusMessage = "assets/tuning.json からボス2パラメータを再読込しました。";
            m_debug.saveStatusTimer = 3.0f;
        }
        if (!m_debug.saveStatusMessage.empty())
        {
            ImGui::TextWrapped("%s", m_debug.saveStatusMessage.c_str());
        }

        if (boss->beamEntity)
        {
            if (const auto* beamTransform = boss->beamEntity->GetComponent<TransformComponent>())
            {
                ImGui::Text("ビーム当たり判定: %.1f, %.1f, %.1f, %.1f",
                    beamTransform->x,
                    beamTransform->y,
                    beamTransform->width * beamTransform->scale,
                    beamTransform->height * beamTransform->scale);
            }
        }
    }

    if (!foundBoss)
    {
        ImGui::TextUnformatted("このシーンに MidBoss2 は見つかりません。");
    }

    ImGui::End();
}

void GameScene::DrawProgressSavePanel()
{
    ImGui::SetNextWindowSize(ImVec2(360.0f, 170.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("セーブ"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("ファイル: %s", kGameProgressSavePath);
    if (ImGui::Button("今すぐ保存"))
    {
        SaveProgressState();
    }
    ImGui::SameLine();
    if (ImGui::Button("セーブを再読込"))
    {
        if (std::filesystem::exists(kGameProgressSavePath))
        {
            m_debug.saveStatusMessage = "セーブファイルを再読込しています...";
            m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
        }
        else
        {
            m_debug.saveStatusMessage = "セーブファイルが見つかりません。";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("セーブを削除"))
    {
        std::error_code ec;
        if (std::filesystem::remove(kGameProgressSavePath, ec) && !ec)
        {
            m_debug.saveStatusMessage = "セーブファイルを削除しました。";
        }
        else
        {
            m_debug.saveStatusMessage = "削除できるセーブファイルがありません。";
        }
    }

    if (!m_debug.saveStatusMessage.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_debug.saveStatusMessage.c_str());
    }

    ImGui::TextUnformatted("F5 保存 / F8 再読込");
    ImGui::End();
}

