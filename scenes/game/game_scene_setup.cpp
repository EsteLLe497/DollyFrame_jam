#include "pch.h"

#include "game_scene_internal.h"

#include "components_combat.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sstream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26495)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "prefab_factory.h"
#include "game_scene_camerawork.h"
#include "game_scene_player_visual_system.h"

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

    std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    bool IsForestStageMapPath(const std::string& mapPath)
    {
        std::filesystem::path path(mapPath);
        const std::string stem = ToLowerCopy(path.stem().string());
        return stem.find("forest") != std::string::npos;
    }

    std::string ResolveDefaultTileTextureKeyForMapPath(const std::string& mapCsvPath)
    {
        return "tile_forest_ground";
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
        return upper == 'W' || upper == 'R' || upper == 'N' || upper == '!' || upper == '?' || upper == '$' || upper == '%';
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

    nlohmann::json BuildMidBoss2TeleportSlotsJson(
        const std::array<MidBoss2Component::TeleportSlotConfig, 3>& slots)
    {
        nlohmann::json slotsJson = nlohmann::json::array();
        for (const auto& slot : slots)
        {
            slotsJson.push_back({
                { "center_grid_x", slot.centerGridX },
                { "height_offset_grid", slot.hoverHeightOffsetGrid },
            });
        }
        return slotsJson;
    }

    nlohmann::json BuildMidBoss2TuningJson(const MidBoss2Component::Params& params)
    {
        nlohmann::json root;
        root["boss2_hp"] = params.boss2Hp;
        root["boss2_width_grid"] = params.boss2WidthGrid;
        root["boss2_height_grid"] = params.boss2HeightGrid;
        root["spear_damage"] = params.spearDamage;
        root["spear_fade_time"] = params.spearFadeTime;
        root["spear_interval"] = params.spearInterval;
        root["spear_cooldown_after_landing"] = params.spearCooldownAfterLanding;
        root["spear_landing_pause_time"] = params.spearLandingPauseTime;
        root["spear_jump_height_grid"] = params.spearJumpHeightGrid;
        root["spear_jump_horizontal_grid"] = params.spearJumpHorizontalGrid;
        root["beam_charge_time"] = params.beamChargeTime;
        root["beam_damage_per_second"] = params.beamDamagePerSecond;
        root["beam_height_grid"] = params.beamHeightGrid;
        root["beam_cooldown_after_fire"] = params.beamCooldownAfterFire;
        root["teleport_hover_base_grid"] = params.teleportHoverBaseGrid;
        root["teleport_spark_count"] = params.teleportSparkCount;
        root["teleport_spark_min_size"] = params.teleportSparkMinSize;
        root["teleport_spark_max_size"] = params.teleportSparkMaxSize;
        root["teleport_spark_spread_scale"] = params.teleportSparkSpreadScale;
        root["teleport_spark_lifetime"] = params.teleportSparkLifetime;
        root["pasted_beam_damage_per_second"] = params.pastedBeamDamagePerSecond;
        root["left_teleport_slots"] = BuildMidBoss2TeleportSlotsJson(params.leftTeleportSlots);
        root["right_teleport_slots"] = BuildMidBoss2TeleportSlotsJson(params.rightTeleportSlots);
        return root;
    }

    void LoadMidBoss2TeleportSlotsJson(
        const nlohmann::json& slotsJson,
        std::array<MidBoss2Component::TeleportSlotConfig, 3>& slots)
    {
        if (!slotsJson.is_array())
        {
            return;
        }

        const size_t count = std::min(slots.size(), slotsJson.size());
        for (size_t index = 0; index < count; ++index)
        {
            const auto& slotJson = slotsJson[index];
            if (!slotJson.is_object())
            {
                continue;
            }

            auto& slot = slots[index];
            slot.centerGridX = slotJson.value<float>("center_grid_x", slot.centerGridX);
            slot.hoverHeightOffsetGrid = slotJson.value<float>("height_offset_grid", slot.hoverHeightOffsetGrid);
        }
    }

    void LoadMidBoss2TuningJson(const nlohmann::json& root, MidBoss2Component::Params& params)
    {
        if (!root.is_object())
        {
            return;
        }

        params.boss2Hp = root.value<int>("boss2_hp", params.boss2Hp);
        params.boss2WidthGrid = root.value<int>("boss2_width_grid", params.boss2WidthGrid);
        params.boss2HeightGrid = root.value<int>("boss2_height_grid", params.boss2HeightGrid);
        params.spearDamage = root.value<int>("spear_damage", params.spearDamage);
        params.spearFadeTime = root.value<float>("spear_fade_time", params.spearFadeTime);
        params.spearInterval = root.value<float>("spear_interval", params.spearInterval);
        params.spearCooldownAfterLanding = root.value<float>(
            "spear_cooldown_after_landing",
            params.spearCooldownAfterLanding);
        params.spearLandingPauseTime = root.value<float>("spear_landing_pause_time", params.spearLandingPauseTime);
        params.spearJumpHeightGrid = root.value<float>("spear_jump_height_grid", params.spearJumpHeightGrid);
        params.spearJumpHorizontalGrid = root.value<float>(
            "spear_jump_horizontal_grid",
            params.spearJumpHorizontalGrid);
        params.beamChargeTime = root.value<float>("beam_charge_time", params.beamChargeTime);
        params.beamDamagePerSecond = root.value<float>("beam_damage_per_second", params.beamDamagePerSecond);
        params.beamHeightGrid = root.value<float>("beam_height_grid", params.beamHeightGrid);
        params.beamCooldownAfterFire = root.value<float>("beam_cooldown_after_fire", params.beamCooldownAfterFire);
        params.teleportHoverBaseGrid = root.value<float>("teleport_hover_base_grid", params.teleportHoverBaseGrid);
        params.teleportSparkCount = root.value<int>("teleport_spark_count", params.teleportSparkCount);
        params.teleportSparkMinSize = root.value<float>("teleport_spark_min_size", params.teleportSparkMinSize);
        params.teleportSparkMaxSize = root.value<float>("teleport_spark_max_size", params.teleportSparkMaxSize);
        params.teleportSparkSpreadScale = root.value<float>("teleport_spark_spread_scale", params.teleportSparkSpreadScale);
        params.teleportSparkLifetime = root.value<float>("teleport_spark_lifetime", params.teleportSparkLifetime);
        params.pastedBeamDamagePerSecond = root.value<float>(
            "pasted_beam_damage_per_second",
            params.pastedBeamDamagePerSecond);

        if (const auto it = root.find("left_teleport_slots"); it != root.end())
        {
            LoadMidBoss2TeleportSlotsJson(*it, params.leftTeleportSlots);
        }
        if (const auto it = root.find("right_teleport_slots"); it != root.end())
        {
            LoadMidBoss2TeleportSlotsJson(*it, params.rightTeleportSlots);
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
        root["capture_frame_width_px"] = gCaptureFrameWidthPx;
        root["capture_frame_height_px"] = gCaptureFrameHeightPx;
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
        root["jump_pad_max_tilt_degrees"] = gJumpPadMaxTiltDegrees;
        root["mid_boss_2"] = BuildMidBoss2TuningJson(GetActiveGameScene()->Tuning().midBoss2Params);
        return root;
    }

#define UI_JSON_FIELD(name) root[#name] = value.name
#define UI_JSON_LOAD(name) value.name = root.value(#name, value.name)

#define DEFINE_UI_JSON(Type, Fields) \
    nlohmann::json ToJson(const Type& value) \
    { \
        nlohmann::json root; \
        Fields(UI_JSON_FIELD); \
        return root; \
    } \
    void FromJson(const nlohmann::json& root, Type& value) \
    { \
        if (!root.is_object()) return; \
        Fields(UI_JSON_LOAD); \
    }

#define CAPTURE_FINDER_FIELDS(F) F(scaleMin); F(scaleMax); F(scaleStep); F(zoomBlendResponse)
#define CAPTURE_OVERLAY_FIELDS(F) F(frameInset); F(cornerLength); F(cornerThickness); F(guideInset); F(frameBandThickness); F(vignetteEdge0); F(vignetteEdge1); F(vignetteEdge2); F(vignetteEdge3); F(vignetteBoost); F(warningPanelX); F(warningPanelY); F(warningPanelWidth); F(warningPanelHeight); F(warningTitleX); F(warningTitleY); F(warningCountX); F(warningCountY); F(warningTimerX); F(pulseInset)
#define TUTORIAL_FIELDS(F) F(dimAlpha); F(dialogueBoxX); F(dialogueBoxY); F(dialogueBoxWidth); F(dialogueBoxHeight); F(dialogueNameX); F(dialogueNameY); F(dialogueTextX); F(dialogueTextY); F(dialoguePromptX); F(dialoguePromptY); F(dialogueNameFontSize); F(dialogueTextFontSize); F(dialogueLineSpacing); F(dialoguePortraitX); F(dialoguePortraitY); F(dialoguePortraitSize); F(dialogueFadeDuration); F(dialogueCharactersPerSecond); F(dialogueBoxLayer); F(dialoguePortraitLayer); F(dialogueNameLayer); F(dialogueTextLayer); F(dialoguePromptLayer); F(frameX); F(frameY); F(frameWidth); F(frameHeight); F(headingX); F(headingY); F(headingWidth); F(headingHeight); F(titleX); F(titleY); F(contentImageX); F(contentImageY); F(contentImageWidth); F(contentImageHeight); F(bodyX); F(bodyY); F(bodyWidth); F(bodyLineSpacing); F(promptX); F(promptY); F(titleFontSize); F(bodyFontSize); F(promptFontSize); F(frameLayer); F(headingLayer); F(contentImageLayer); F(titleLayer); F(bodyLayer); F(promptLayer)
#define PHOTO_TRAY_FIELDS(F) F(slotStartX); F(slotStartY); F(slotWidth); F(slotHeight); F(slotGapX); F(previewPadding); F(previewScale); F(emptyTextX); F(emptyTextY); F(lockTextX); F(lockTextY); F(revealSpeed); F(revealThreshold)
#define PHOTO_PREVIEW_FIELDS(F) F(lifetime); F(cardWidth); F(cardHeight); F(cardRightMargin); F(cardStartYOffset); F(cardCruiseY); F(cardShadowOffset); F(cardOutlineOffset); F(frameInset); F(imageHeight); F(imageTopStripHeight); F(imageMiddleStripY); F(cardRiseEase); F(cardPauseStart); F(cardPauseEnd); F(cardPauseAmplitude); F(cardOvershootY); F(popScale); F(orbLaunchXOffset); F(orbLaunchYOffset); F(orbControl1YOffset); F(orbControl2YOffset); F(orbControl2XOffset)
#define HP_FIELDS(F) F(slotStartX); F(slotStartY); F(slotWidth); F(slotHeight); F(slotGapX); F(heartSize); F(heartYOffset); F(heartShadowOffsetX); F(heartShadowOffsetY); F(heartGlowExpand); F(heartLagGlowExpand); F(labelOffsetX); F(labelOffsetY); F(hpTextOffsetY); F(displayRiseSpeedDown); F(displayRiseSpeedUp); F(lagSpeed); F(flashDecaySpeed)
#define PARTS_FIELDS(F) F(panelWidth); F(panelHeight); F(marginRight); F(marginBottom); F(iconX); F(iconY); F(iconSize); F(iconInnerInset); F(labelX); F(labelY); F(valueY)
#define BOSS_FIELDS(F) F(panelWidth); F(barHeight); F(panelPadding); F(marginTop); F(panelExtraHeight); F(titleOffsetY); F(hpTextOffsetY)
#define ATTACK_FIELDS(F) F(panelX); F(panelY); F(panelSize); F(iconRadius); F(titleX); F(titleY); F(countRightOffset); F(countBottomOffset)
#define ESCAPE_FIELDS(F) F(panelWidth); F(panelHeight); F(rowStartOffset); F(rowHeight); F(rowPaddingX); F(rowBottomInset); F(titleX); F(titleY); F(helpY); F(rowTextX); F(rowTextY)
#define MERCHANT_FIELDS(F) F(panelWidth); F(panelHeight); F(rowHeight); F(listLeftOffset); F(listTopOffset); F(listRightOffset); F(detailLeftOffset); F(detailTopOffset); F(detailBottomOffset); F(promptHalfWidth); F(promptHeight); F(promptTextX); F(promptTextY); F(promptRiseOffsetY); F(promptPulseSpeed)
#define FILTER_FIELDS(F) F(panelWidth); F(panelHeight); F(marginRight); F(marginTop); F(swatchX); F(swatchY); F(swatchSize); F(titleX); F(titleY); F(effectY); F(hintX); F(hintY)
#define BATTERY_FIELDS(F) F(panelWidth); F(panelHeight); F(offsetY); F(tileOffsetMultiplier); F(iconSize); F(iconInnerInset); F(labelX); F(labelY)
#define GUIDE_FIELDS(F) F(x); F(yOffsetFromBottom)
#define EDITOR_FIELDS(F) F(panelLeft); F(panelTop); F(panelRight); F(panelBottom)

    DEFINE_UI_JSON(GameSceneUiCaptureFinderTuning, CAPTURE_FINDER_FIELDS)
    DEFINE_UI_JSON(GameSceneUiCaptureOverlayTuning, CAPTURE_OVERLAY_FIELDS)
    DEFINE_UI_JSON(GameSceneUiTutorialTuning, TUTORIAL_FIELDS)
    DEFINE_UI_JSON(GameSceneUiPhotoTrayTuning, PHOTO_TRAY_FIELDS)
    DEFINE_UI_JSON(GameSceneUiDevelopedPhotoPreviewTuning, PHOTO_PREVIEW_FIELDS)
    DEFINE_UI_JSON(GameSceneUiHpTuning, HP_FIELDS)
    DEFINE_UI_JSON(GameSceneUiPartsHudTuning, PARTS_FIELDS)
    DEFINE_UI_JSON(GameSceneUiBossHpTuning, BOSS_FIELDS)
    DEFINE_UI_JSON(GameSceneUiAttackCaptureTuning, ATTACK_FIELDS)
    DEFINE_UI_JSON(GameSceneUiEscapeMenuTuning, ESCAPE_FIELDS)
    DEFINE_UI_JSON(GameSceneUiMerchantTuning, MERCHANT_FIELDS)
    DEFINE_UI_JSON(GameSceneUiFilterPanelTuning, FILTER_FIELDS)
    DEFINE_UI_JSON(GameSceneUiBatteryCounterTuning, BATTERY_FIELDS)
    DEFINE_UI_JSON(GameSceneUiStageGuideTuning, GUIDE_FIELDS)
    DEFINE_UI_JSON(GameSceneUiMapEditorTuning, EDITOR_FIELDS)

#undef EDITOR_FIELDS
#undef GUIDE_FIELDS
#undef BATTERY_FIELDS
#undef FILTER_FIELDS
#undef MERCHANT_FIELDS
#undef ESCAPE_FIELDS
#undef ATTACK_FIELDS
#undef BOSS_FIELDS
#undef PARTS_FIELDS
#undef HP_FIELDS
#undef PHOTO_PREVIEW_FIELDS
#undef PHOTO_TRAY_FIELDS
#undef CAPTURE_OVERLAY_FIELDS
#undef CAPTURE_FINDER_FIELDS
#undef TUTORIAL_FIELDS
#undef DEFINE_UI_JSON
#undef UI_JSON_LOAD
#undef UI_JSON_FIELD

    nlohmann::json BuildUiTuningJson(const GameSceneUiState& ui)
    {
        nlohmann::json root;
        root["version"] = 1;
        root["capture_frame_width_px"] = gCaptureFrameWidthPx;
        root["capture_frame_height_px"] = gCaptureFrameHeightPx;
        root["capture_finder_scale"] = ui.captureFinderScale;
        root["camera_flash_enabled"] = ui.cameraFlash.enabled;
        root["capture_finder"] = ToJson(ui.tuning.captureFinder);
        root["capture_overlay"] = ToJson(ui.tuning.captureOverlay);
        root["tutorial"] = ToJson(ui.tuning.tutorial);
        root["photo_tray"] = ToJson(ui.tuning.photoTray);
        root["developed_photo_preview"] = ToJson(ui.tuning.developedPhotoPreview);
        root["hp"] = ToJson(ui.tuning.hp);
        root["parts_hud"] = ToJson(ui.tuning.partsHud);
        root["boss_hp"] = ToJson(ui.tuning.bossHp);
        root["attack_capture"] = ToJson(ui.tuning.attackCapture);
        root["escape_menu"] = ToJson(ui.tuning.escapeMenu);
        root["merchant"] = ToJson(ui.tuning.merchant);
        root["filter_panel"] = ToJson(ui.tuning.filterPanel);
        root["battery_counter"] = ToJson(ui.tuning.batteryCounter);
        root["stage_guide"] = ToJson(ui.tuning.stageGuide);
        root["map_editor"] = ToJson(ui.tuning.mapEditor);
        return root;
    }

    template<typename T>
    void LoadUiSection(const nlohmann::json& root, const char* key, T& value)
    {
        const auto it = root.find(key);
        if (it != root.end())
        {
            FromJson(*it, value);
        }
    }

    void ApplyUiTuningJson(const nlohmann::json& root, GameSceneUiState& ui)
    {
        if (!root.is_object())
        {
            return;
        }

        gCaptureFrameWidthPx = root.value("capture_frame_width_px", gCaptureFrameWidthPx);
        gCaptureFrameHeightPx = root.value("capture_frame_height_px", gCaptureFrameHeightPx);
        ui.captureFinderScale = root.value("capture_finder_scale", ui.captureFinderScale);
        ui.cameraFlash.enabled = root.value("camera_flash_enabled", ui.cameraFlash.enabled);
        LoadUiSection(root, "capture_finder", ui.tuning.captureFinder);
        LoadUiSection(root, "capture_overlay", ui.tuning.captureOverlay);
        LoadUiSection(root, "tutorial", ui.tuning.tutorial);
        LoadUiSection(root, "photo_tray", ui.tuning.photoTray);
        LoadUiSection(root, "developed_photo_preview", ui.tuning.developedPhotoPreview);
        LoadUiSection(root, "hp", ui.tuning.hp);
        LoadUiSection(root, "parts_hud", ui.tuning.partsHud);
        LoadUiSection(root, "boss_hp", ui.tuning.bossHp);
        LoadUiSection(root, "attack_capture", ui.tuning.attackCapture);
        LoadUiSection(root, "escape_menu", ui.tuning.escapeMenu);
        LoadUiSection(root, "merchant", ui.tuning.merchant);
        LoadUiSection(root, "filter_panel", ui.tuning.filterPanel);
        LoadUiSection(root, "battery_counter", ui.tuning.batteryCounter);
        LoadUiSection(root, "stage_guide", ui.tuning.stageGuide);
        LoadUiSection(root, "map_editor", ui.tuning.mapEditor);
    }
    struct BackgroundPartPlacement
    {
        const char* textureKey;
        float worldX;
        float worldY;
        float width;
        float height;
        float parallax; // 1.0 = カメラと完全に連動(通常の地形と同じ)
    };

    // 仮配置。あとでCSVマーカー化する前提で、ここに直接座標を書く。
    const BackgroundPartPlacement kBackgroundParts[] =
    {
        { "bg_parts_tree_01",  320.0f, 480.0f, 192.0f, 256.0f, 1.0f },
        { "bg_parts_rock_01",  860.0f, 620.0f, 128.0f,  96.0f, 1.0f },
        { "bg_parts_grass_01", 540.0f, 700.0f, 160.0f,  64.0f, 1.0f },
    };

}


void GameScene::RefreshStageRenderProfile()
{
    m_lifecycle.darknessStageEnabled = IsDarknessStageMapPath(m_lifecycle.currentMapCsvPath);
    m_lifecycle.forestStageEnabled = IsForestStageMapPath(m_lifecycle.currentMapCsvPath);
    if (m_lifecycle.forestStageEnabled)
    {
        DirectXSetPostProcessVignette(0.54f, 0.43f, 0.34f, 0.58f);
        return;
    }

    DirectXSetPostProcessVignette(0.08f, 0.72f, 0.72f, 0.70f);
    DirectXSetPostProcessPlayerLight(
        static_cast<float>(kVirtualScreenWidth) * 0.5f,
        static_cast<float>(kVirtualScreenHeight) * 0.5f,
        0.0f,
        120.0f,
        170.0f);
}

void GameScene::BuildCameraMarkers()
{

    m_camera.fixedRanges.clear();

    float tileSize = m_tileMap.GetTileSize();

    std::filesystem::path path(m_lifecycle.currentMapCsvPath);
    std::string stageName = path.stem().string();

    ////森林ステージ    
    //if (stageName == "forest")
    //{
    //    //カメラ1
    //    {
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(-2, 6, tileSize); // 左上タイル
    //        cameraRange.SetEndTiles(22, 18, tileSize);   // 右下タイル
    //        cameraRange.SetCameraNum(0);
    //        cameraRange.SetFollowPlayer(false);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    //カメラ2
    //    {
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(22, 6, tileSize);
    //        cameraRange.SetEndTiles(42, 19, tileSize);
    //        cameraRange.SetCameraNum(1);
    //        cameraRange.SetFollowPlayer(false);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    //カメラ3
    //    {
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(72, 2, tileSize);
    //        cameraRange.SetEndTiles(93, 28, tileSize);
    //        cameraRange.SetCameraNum(2);
    //        cameraRange.SetFollowPlayer(false);

    //        cameraRange.SetZoomWidth(2560.0f);
    //        cameraRange.SetZoomHeight(1440.0f);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    //カメラ4
    //    {
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(113, 4, tileSize);
    //        cameraRange.SetEndTiles(133, 21, tileSize);
    //        cameraRange.SetCameraNum(3);
    //        cameraRange.SetFollowPlayer(false);

    //        cameraRange.SetZoomHeight(1440.0f);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    //カメラ5
    //    {
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(131, 10, tileSize);
    //        cameraRange.SetEndTiles(148, 21, tileSize);
    //        cameraRange.SetCameraNum(4);
    //        cameraRange.SetFollowPlayer(false);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }
    //}

    ////地下ステージ
    //if (stageName == "under")
    //{
    //    {
    //        //カメラ1
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(0, 0, tileSize);
    //        cameraRange.SetEndTiles(19, 11, tileSize);
    //        cameraRange.SetCameraNum(0);
    //        cameraRange.SetFollowPlayer(false);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ2-1
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(19, 0, tileSize);
    //        cameraRange.SetEndTiles(30, 19, tileSize);
    //        cameraRange.SetCameraNum(1);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(10, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ2-2
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(19, 11, tileSize);
    //        cameraRange.SetEndTiles(30, 33, tileSize);
    //        cameraRange.SetCameraNum(2);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(10, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ3
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(30, 19, tileSize);
    //        cameraRange.SetEndTiles(47, 30, tileSize);
    //        cameraRange.SetCameraNum(3);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(3, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ4
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(67, 10, tileSize);
    //        cameraRange.SetEndTiles(82, 27, tileSize);
    //        cameraRange.SetCameraNum(4);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(-3, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ5
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(48, 21, tileSize);
    //        cameraRange.SetEndTiles(67, 27, tileSize);
    //        cameraRange.SetCameraNum(5);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(5, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ6
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(47, 25, tileSize);
    //        cameraRange.SetEndTiles(62, 34, tileSize);
    //        cameraRange.SetCameraNum(6);
    //        cameraRange.SetFollowPlayer(false);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ7
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(84, 25, tileSize);
    //        cameraRange.SetEndTiles(93, 32, tileSize);
    //        cameraRange.SetCameraNum(7);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(2, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //    {
    //        //カメラ8
    //        fixedCameraRange cameraRange;
    //        cameraRange.SetStartTiles(93, 14, tileSize);
    //        cameraRange.SetEndTiles(107, 32, tileSize);
    //        cameraRange.SetCameraNum(8);
    //        cameraRange.SetFollowPlayer(false);
    //        cameraRange.SetOffsetY(2, tileSize);

    //        m_camera.fixedRanges.push_back(cameraRange);
    //    }

    //}

}

void GameScene::ApplyTileTextureKey(const std::string& tileTextureKey)
{
    const std::string resolvedKey = tileTextureKey.empty()
        ? ResolveDefaultTileTextureKeyForMapPath(m_lifecycle.currentMapCsvPath)
        : tileTextureKey;

    int textureId = m_assets.GetTexture(resolvedKey);
    std::string finalKey = resolvedKey;
    if (textureId < 0 && finalKey != "tile_forest_ground")
    {
        finalKey = "tile_forest_ground";
        textureId = m_assets.GetTexture(finalKey);
    }
    if (textureId < 0 && finalKey != "tile_default")
    {
        finalKey = "tile_default";
        textureId = m_assets.GetTexture(finalKey);
    }
    if (textureId < 0)
    {
        textureId = m_whiteTexture;
    }

    m_tileTexture = textureId;
    m_tileTexture2 = m_assets.GetTexture("tile_value_2_blue");
    if (m_tileTexture2 < 0)
    {
        m_tileTexture2 = m_tileTexture;
    }
    m_tileTexture3 = m_assets.GetTexture("tile_value_3_purple");
    if (m_tileTexture3 < 0)
    {
        m_tileTexture3 = m_tileTexture;
    }
    m_lifecycle.currentTileTextureKey = finalKey;
    m_tileMap.SetTileTextureKey(finalKey);
}

std::string GameScene::ResolveDefaultTileTextureKeyForCurrentMap() const
{
    return ResolveDefaultTileTextureKeyForMapPath(m_lifecycle.currentMapCsvPath);
}

void GameScene::RefreshTileTextureForCurrentMap()
{
    const std::string tileTextureKey = m_tileMap.GetTileTextureKey().empty()
        ? ResolveDefaultTileTextureKeyForCurrentMap()
        : m_tileMap.GetTileTextureKey();
    ApplyTileTextureKey(tileTextureKey);
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
        gCaptureFrameWidthPx = root.value("capture_frame_width_px", gCaptureFrameWidthPx);
        gCaptureFrameHeightPx = root.value("capture_frame_height_px", gCaptureFrameHeightPx);
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
        gJumpPadMaxTiltDegrees = root.value("jump_pad_max_tilt_degrees", gJumpPadMaxTiltDegrees);

        if (const auto it = root.find("mid_boss_2"); it != root.end())
        {
            LoadMidBoss2TuningJson(*it, GetActiveGameScene()->Tuning().midBoss2Params);
        }
    }
}

namespace
{
    using json = nlohmann::json;

    template <typename Enum>
    int ToSavedEnum(Enum value)
    {
        return static_cast<int>(value);
    }

    template <typename Enum>
    Enum FromSavedEnum(const json& value, Enum fallback)
    {
        if (!value.is_number_integer())
        {
            return fallback;
        }

        return static_cast<Enum>(value.get<int>());
    }

    json SerializePhotoItem(const CapturedPhotoItem& item)
    {
        json root;
        root["textureId"] = item.textureId;
        root["role"] = ToSavedEnum(item.role);
        root["layer"] = ToSavedEnum(item.layer);
        root["origin"] = ToSavedEnum(item.origin);
        root["appliedTheme"] = ToSavedEnum(item.appliedTheme);
        root["relativeX"] = item.relativeX;
        root["relativeY"] = item.relativeY;
        root["width"] = item.width;
        root["height"] = item.height;
        root["sourceX"] = item.sourceX;
        root["sourceY"] = item.sourceY;
        root["sourceWidth"] = item.sourceWidth;
        root["sourceHeight"] = item.sourceHeight;
        root["tintR"] = item.tintR;
        root["tintG"] = item.tintG;
        root["tintB"] = item.tintB;
        root["tintA"] = item.tintA;
        root["sourceTileValue"] = item.sourceTileValue;
        root["damagePlatformTileSpan"] = item.damagePlatformTileSpan;
        root["spikeStripTileSpan"] = item.spikeStripTileSpan;
        root["sepiaRestoredTileValue"] = item.sepiaRestoredTileValue;
        root["sepiaRestoredMarkerObject"] = item.sepiaRestoredMarkerObject;
        root["rotation"] = item.rotation;
        root["flipX"] = item.flipX;
        root["vanishOnCapture"] = item.vanishOnCapture;
        root["enemyAttackPaste"] = item.enemyAttackPaste;
        root["attackCaptureCount"] = item.attackCaptureCount;
        root["gearNo"] = item.gearNo;
        root["spawnArchetype"] = ToSavedEnum(item.spawnArchetype);
        root["placementRuleGroup"] = ToSavedEnum(item.placementRuleGroup);
        root["projectileVelocityX"] = item.projectileVelocityX;
        root["projectileVelocityY"] = item.projectileVelocityY;
        root["projectileDamage"] = item.projectileDamage;
        root["spearProjectile"] = item.spearProjectile;
        root["spearStuck"] = item.spearStuck;
        root["spearDirectionX"] = item.spearDirectionX;
        root["spearDirectionY"] = item.spearDirectionY;
        root["spearTravelDistance"] = item.spearTravelDistance;
        root["laserBeamThickness"] = item.laserBeamThickness;
        root["laserDamagePerSecond"] = item.laserDamagePerSecond;
        root["laserEnemyKnockbackSpeed"] = item.laserEnemyKnockbackSpeed;
        root["lightRadius"] = item.lightRadius;
        root["lightIntensity"] = item.lightIntensity;
        root["collisionOutline"] = json::array();
        for (const auto& point : item.collisionOutline)
        {
            root["collisionOutline"].push_back({ {"x", point.x}, {"y", point.y} });
        }
        return root;
    }

    CapturedPhotoItem DeserializePhotoItem(const json& root)
    {
        CapturedPhotoItem item;
        item.textureId = root.value("textureId", item.textureId);
        item.role = FromSavedEnum(root.value("role", 0), item.role);
        item.layer = FromSavedEnum(root.value("layer", 0), item.layer);
        item.origin = FromSavedEnum(root.value("origin", 0), item.origin);
        item.appliedTheme = FromSavedEnum(root.value("appliedTheme", 0), item.appliedTheme);
        item.relativeX = root.value("relativeX", item.relativeX);
        item.relativeY = root.value("relativeY", item.relativeY);
        item.width = root.value("width", item.width);
        item.height = root.value("height", item.height);
        item.sourceX = root.value("sourceX", item.sourceX);
        item.sourceY = root.value("sourceY", item.sourceY);
        item.sourceWidth = root.value("sourceWidth", item.sourceWidth);
        item.sourceHeight = root.value("sourceHeight", item.sourceHeight);
        item.tintR = root.value("tintR", item.tintR);
        item.tintG = root.value("tintG", item.tintG);
        item.tintB = root.value("tintB", item.tintB);
        item.tintA = root.value("tintA", item.tintA);
        item.sourceTileValue = root.value("sourceTileValue", item.sourceTileValue);
        item.damagePlatformTileSpan = root.value("damagePlatformTileSpan", item.damagePlatformTileSpan);
        item.spikeStripTileSpan = root.value("spikeStripTileSpan", item.spikeStripTileSpan);
        item.sepiaRestoredTileValue = root.value("sepiaRestoredTileValue", item.sepiaRestoredTileValue);
        item.sepiaRestoredMarkerObject = root.value("sepiaRestoredMarkerObject", item.sepiaRestoredMarkerObject);
        item.rotation = root.value("rotation", item.rotation);
        item.flipX = root.value("flipX", item.flipX);
        item.vanishOnCapture = root.value("vanishOnCapture", item.vanishOnCapture);
        item.enemyAttackPaste = root.value("enemyAttackPaste", item.enemyAttackPaste);
        item.attackCaptureCount = root.value("attackCaptureCount", item.attackCaptureCount);
        item.gearNo = root.value("gearNo", item.gearNo);
        item.spawnArchetype = FromSavedEnum(root.value("spawnArchetype", 0), item.spawnArchetype);
        item.placementRuleGroup = FromSavedEnum(root.value("placementRuleGroup", 0), item.placementRuleGroup);
        item.projectileVelocityX = root.value("projectileVelocityX", item.projectileVelocityX);
        item.projectileVelocityY = root.value("projectileVelocityY", item.projectileVelocityY);
        item.projectileDamage = root.value("projectileDamage", item.projectileDamage);
        item.spearProjectile = root.value("spearProjectile", item.spearProjectile);
        item.spearStuck = root.value("spearStuck", item.spearStuck);
        item.spearDirectionX = root.value("spearDirectionX", item.spearDirectionX);
        item.spearDirectionY = root.value("spearDirectionY", item.spearDirectionY);
        item.spearTravelDistance = root.value("spearTravelDistance", item.spearTravelDistance);
        item.laserBeamThickness = root.value("laserBeamThickness", item.laserBeamThickness);
        item.laserDamagePerSecond = root.value("laserDamagePerSecond", item.laserDamagePerSecond);
        item.laserEnemyKnockbackSpeed = root.value("laserEnemyKnockbackSpeed", item.laserEnemyKnockbackSpeed);
        item.lightRadius = root.value("lightRadius", item.lightRadius);
        item.lightIntensity = root.value("lightIntensity", item.lightIntensity);

        const auto outlineIt = root.find("collisionOutline");
        if (outlineIt != root.end() && outlineIt->is_array())
        {
            item.collisionOutline.clear();
            item.collisionOutline.reserve(outlineIt->size());
            for (const auto& pointRoot : *outlineIt)
            {
                CapturedPhotoItem::OutlinePoint point;
                point.x = pointRoot.value("x", 0.0f);
                point.y = pointRoot.value("y", 0.0f);
                item.collisionOutline.push_back(point);
            }
        }
        return item;
    }

    json SerializePhotoCaptureState(const PhotoCaptureState& capture)
    {
        json root;
        root["hasPhoto"] = capture.hasPhoto;
        root["selectedTheme"] = ToSavedEnum(capture.selectedTheme);
        root["capturedTheme"] = ToSavedEnum(capture.capturedTheme);
        root["attackCaptureCount"] = capture.attackCaptureCount;
        root["containsEnemyAttackPaste"] = capture.containsEnemyAttackPaste;
        root["textureId"] = capture.textureId;
        root["width"] = capture.width;
        root["height"] = capture.height;
        root["sourceX"] = capture.sourceX;
        root["sourceY"] = capture.sourceY;
        root["sourceWidth"] = capture.sourceWidth;
        root["sourceHeight"] = capture.sourceHeight;
        root["tintR"] = capture.tintR;
        root["tintG"] = capture.tintG;
        root["tintB"] = capture.tintB;
        root["tintA"] = capture.tintA;
        root["items"] = json::array();
        for (const auto& item : capture.items)
        {
            root["items"].push_back(SerializePhotoItem(item));
        }
        return root;
    }

    PhotoCaptureState DeserializePhotoCaptureState(const json& root)
    {
        PhotoCaptureState capture;
        capture.hasPhoto = root.value("hasPhoto", capture.hasPhoto);
        capture.selectedTheme = FromSavedEnum(root.value("selectedTheme", 0), capture.selectedTheme);
        capture.capturedTheme = FromSavedEnum(root.value("capturedTheme", 0), capture.capturedTheme);
        capture.attackCaptureCount = root.value("attackCaptureCount", capture.attackCaptureCount);
        capture.containsEnemyAttackPaste = root.value("containsEnemyAttackPaste", capture.containsEnemyAttackPaste);
        capture.textureId = root.value("textureId", capture.textureId);
        capture.width = root.value("width", capture.width);
        capture.height = root.value("height", capture.height);
        capture.sourceX = root.value("sourceX", capture.sourceX);
        capture.sourceY = root.value("sourceY", capture.sourceY);
        capture.sourceWidth = root.value("sourceWidth", capture.sourceWidth);
        capture.sourceHeight = root.value("sourceHeight", capture.sourceHeight);
        capture.tintR = root.value("tintR", capture.tintR);
        capture.tintG = root.value("tintG", capture.tintG);
        capture.tintB = root.value("tintB", capture.tintB);
        capture.tintA = root.value("tintA", capture.tintA);
        capture.items.clear();
        const auto itemsIt = root.find("items");
        if (itemsIt != root.end() && itemsIt->is_array())
        {
            capture.items.reserve(itemsIt->size());
            for (const auto& itemRoot : *itemsIt)
            {
                capture.items.push_back(DeserializePhotoItem(itemRoot));
            }
        }
        return capture;
    }

    json SerializePhotoPlacementState(const PhotoPlacementState& placement)
    {
        json root;
        root["active"] = placement.active;
        root["valid"] = placement.valid;
        root["x"] = placement.x;
        root["y"] = placement.y;
        root["width"] = placement.width;
        root["height"] = placement.height;
        root["layer"] = ToSavedEnum(placement.layer);
        root["flipX"] = placement.flipX;
        root["bridgeEnabled"] = placement.bridgeEnabled;
        root["rotation"] = placement.rotation;
        root["sessionId"] = placement.sessionId;
        root["blockedByUi"] = placement.blockedByUi;
        root["draggingFromTray"] = placement.draggingFromTray;
        root["invalidFlashRemaining"] = placement.invalidFlashRemaining;
        root["confirmFlashRemaining"] = placement.confirmFlashRemaining;
        return root;
    }

    PhotoPlacementState DeserializePhotoPlacementState(const json& root)
    {
        PhotoPlacementState placement;
        placement.active = root.value("active", placement.active);
        placement.valid = root.value("valid", placement.valid);
        placement.x = root.value("x", placement.x);
        placement.y = root.value("y", placement.y);
        placement.width = root.value("width", placement.width);
        placement.height = root.value("height", placement.height);
        placement.layer = FromSavedEnum(root.value("layer", 0), placement.layer);
        placement.flipX = root.value("flipX", placement.flipX);
        placement.bridgeEnabled = root.value("bridgeEnabled", placement.bridgeEnabled);
        placement.rotation = root.value("rotation", placement.rotation);
        placement.sessionId = root.value("sessionId", placement.sessionId);
        placement.blockedByUi = root.value("blockedByUi", placement.blockedByUi);
        placement.draggingFromTray = root.value("draggingFromTray", placement.draggingFromTray);
        placement.invalidFlashRemaining = root.value("invalidFlashRemaining", placement.invalidFlashRemaining);
        placement.confirmFlashRemaining = root.value("confirmFlashRemaining", placement.confirmFlashRemaining);
        return placement;
    }

    json SerializePhotoGroupState(const PhotoGroupState& groups)
    {
        json root;
        root["hasSpawnedCopy"] = groups.hasSpawnedCopy;
        root["nextGroupId"] = groups.nextGroupId;
        root["activeGroupCount"] = groups.activeGroupCount;
        root["nextPasteOrder"] = groups.nextPasteOrder;
        return root;
    }

    PhotoGroupState DeserializePhotoGroupState(const json& root)
    {
        PhotoGroupState groups;
        groups.hasSpawnedCopy = root.value("hasSpawnedCopy", groups.hasSpawnedCopy);
        groups.nextGroupId = root.value("nextGroupId", groups.nextGroupId);
        groups.activeGroupCount = root.value("activeGroupCount", groups.activeGroupCount);
        groups.nextPasteOrder = root.value("nextPasteOrder", groups.nextPasteOrder);
        return groups;
    }

    json SerializePendingPhotoStoreState(const PendingPhotoStoreState& pendingStore)
    {
        json root;
        root["active"] = pendingStore.active;
        root["commitOnComplete"] = pendingStore.commitOnComplete;
        root["slotIndex"] = pendingStore.slotIndex;
        root["capture"] = SerializePhotoCaptureState(pendingStore.capture);
        return root;
    }

    PendingPhotoStoreState DeserializePendingPhotoStoreState(const json& root)
    {
        PendingPhotoStoreState pendingStore;
        pendingStore.active = root.value("active", pendingStore.active);
        pendingStore.commitOnComplete = root.value("commitOnComplete", pendingStore.commitOnComplete);
        pendingStore.slotIndex = root.value("slotIndex", pendingStore.slotIndex);
        const auto captureIt = root.find("capture");
        if (captureIt != root.end() && captureIt->is_object())
        {
            pendingStore.capture = DeserializePhotoCaptureState(*captureIt);
        }
        return pendingStore;
    }

    json SerializePhotoState(const PhotoState& photo)
    {
        json root;
        root["capture"] = SerializePhotoCaptureState(photo.capture);
        root["attackCapture"] = SerializePhotoCaptureState(photo.attackCapture);
        root["savedCaptures"] = json::array();
        for (const auto& capture : photo.savedCaptures)
        {
            root["savedCaptures"].push_back(SerializePhotoCaptureState(capture));
        }
        root["selectedCaptureSlot"] = photo.selectedCaptureSlot;
        root["nextCaptureSlot"] = photo.nextCaptureSlot;
        root["pendingStore"] = SerializePendingPhotoStoreState(photo.pendingStore);
        root["placement"] = SerializePhotoPlacementState(photo.placement);
        root["groups"] = SerializePhotoGroupState(photo.groups);
        return root;
    }

    PhotoState DeserializePhotoState(const json& root)
    {
        PhotoState photo;
        const auto captureIt = root.find("capture");
        if (captureIt != root.end() && captureIt->is_object())
        {
            photo.capture = DeserializePhotoCaptureState(*captureIt);
        }

        const auto attackCaptureIt = root.find("attackCapture");
        if (attackCaptureIt != root.end() && attackCaptureIt->is_object())
        {
            photo.attackCapture = DeserializePhotoCaptureState(*attackCaptureIt);
        }

        const auto savedCapturesIt = root.find("savedCaptures");
        if (savedCapturesIt != root.end() && savedCapturesIt->is_array())
        {
            const int limit = (std::min)(static_cast<int>(photo.savedCaptures.size()), static_cast<int>(savedCapturesIt->size()));
            for (int index = 0; index < limit; ++index)
            {
                photo.savedCaptures[static_cast<size_t>(index)] = DeserializePhotoCaptureState((*savedCapturesIt)[static_cast<size_t>(index)]);
            }
        }

        photo.selectedCaptureSlot = root.value("selectedCaptureSlot", photo.selectedCaptureSlot);
        photo.nextCaptureSlot = root.value("nextCaptureSlot", photo.nextCaptureSlot);

        const auto pendingStoreIt = root.find("pendingStore");
        if (pendingStoreIt != root.end() && pendingStoreIt->is_object())
        {
            photo.pendingStore = DeserializePendingPhotoStoreState(*pendingStoreIt);
        }

        const auto placementIt = root.find("placement");
        if (placementIt != root.end() && placementIt->is_object())
        {
            photo.placement = DeserializePhotoPlacementState(*placementIt);
        }

        const auto groupsIt = root.find("groups");
        if (groupsIt != root.end() && groupsIt->is_object())
        {
            photo.groups = DeserializePhotoGroupState(*groupsIt);
        }

        return photo;
    }
}

void GameScene::ResetSceneState()
{
    m_world.Clear();
    m_photo = PhotoState{};
    m_flow = GameSceneFlowState{};
    m_ui = GameSceneUiState{};
    m_player = GameScenePlayerState{};
    m_debug = GameSceneDebugState{};
    m_testPhotos = GameSceneTestPhotoState{};
    m_tutorial = GameSceneTutorialState{};
    m_mapEditor.active = false;
    m_mapEditor.brushTarget = GameSceneMapEditorState::BrushTarget::Tile;
    m_mapEditor.selectedTileValue = 1;
    m_mapEditor.selectedMarker = 'G';
    m_mapEditor.selectedMarkerParameter = 1;
    m_mapEditor.selectedStageLightTiles = 3;
    m_mapEditor.selectedStageLightFixtureTiles = 1;
    m_mapEditor.statusMessage.clear();
    m_mapEditor.statusMessageTimer = 0.0f;
    m_camera.transitionMarkers.clear();
    m_camera.fixedRanges.clear();
    m_camera.hasPreviousPlayerCameraProbe = false;
    m_camera.previousPlayerCameraProbeX = 0.0f;
    m_camera.previousPlayerCameraProbeY = 0.0f;
    m_camera.hasCameraSmoothedPlayerY = false;
    m_camera.cameraSmoothedPlayerCenterY = 0.0f;
    m_camera.cameraYRecenteringStrength = 0.0f;
    m_camera.floorCameraTransitionActive = false;
    m_camera.floorCameraTransitionElapsed = 0.0f;
    m_camera.floorCameraTransitionDuration = 1.10f;
    m_camera.floorCameraTransitionStartX = 0.0f;
    m_camera.floorCameraTransitionStartY = 0.0f;
    m_camera.floorCameraTransitionTargetX = 0.0f;
    m_camera.floorCameraTransitionTargetY = 0.0f;
    m_camera.cameraFixedLockActive = false;
    m_camera.cameraFixedLockStartX = 0.0f;
    m_camera.cameraFixedLockEndX = 0.0f;
    m_camera.cameraFixedLockX = 0.0f;
    m_camera.cameraFixedLockY = 0.0f;
    // ボス戦カメラの左右反転状態を、通常の右側構図へ戻します。
    m_camera.shieldBossCameraOffsetX = 0.0f;
    m_camera.shieldBossCameraOffsetY = 0.0f;
    m_camera.shieldBossCameraBaseY = 0.0f;
    m_camera.shieldBossDistanceZoomScale = 1.0f;
    m_camera.shieldBossSideChangeTimer = 0.0f;
    m_camera.shieldBossCameraSide = 1;
    m_camera.shieldBossPendingCameraSide = 1;
    m_camera.shieldBossZoomTier = 0;
    m_camera.shieldBossCameraBaseYInitialized = false;
    m_save = GameSceneSaveState{};
    m_lifecycle.hasPendingStageTransition = false;
    m_lifecycle.pendingStageTransitionMapCsv.clear();
    m_lifecycle.pendingStageTransitionSpawnMarker = '\0';
    m_lifecycle.pendingStageTransitionMarker = '\0';
    m_lifecycle.darknessStageEnabled = false;
    m_lifecycle.currentMapCsvPath = GameSession_GetStartMapCsvPath();
    m_lifecycle.lastStageTransitionMarker = '\0';
    m_lifecycle.currentTileTextureKey = "tile_forest_ground";
    m_lifecycle.shieldBossBgmCrossFadeStarted = false;
    m_flow.timeLimit = 60.0f;
    m_flow.timeRemaining = m_flow.timeLimit;
    m_debug.saveStatusMessage.clear();
    m_debug.saveStatusTimer = 0.0f;
}

void GameScene::LoadTuningState()
{
    const ActiveGameSceneScope activeScene(*this);
    LoadTuningJsonFile();
    LoadUiTuningState();
    loadTutorialData(1);
    std::error_code ec;
    const auto writeTime = std::filesystem::last_write_time(kTuningFilePath, ec);
    if (!ec)
    {
        m_debug.tuningFileWriteTime = writeTime;
        m_debug.hasTuningFileWriteTime = true;
    }
}

bool GameScene::SaveUiTuningState()
{
    std::ofstream stream(kUiTuningFilePath, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        m_debug.saveStatusMessage = "UI設定の保存に失敗しました。";
        m_debug.saveStatusTimer = 3.0f;
        Logger::Warn("Failed to open UI tuning file for writing.");
        return false;
    }

    try
    {
        stream << BuildUiTuningJson(m_ui).dump(2);
    }
    catch (...)
    {
        m_debug.saveStatusMessage = "UI設定の保存中にエラーが発生しました。";
        m_debug.saveStatusTimer = 3.0f;
        Logger::Warn("Failed to serialize UI tuning settings.");
        return false;
    }

    if (!stream.good())
    {
        m_debug.saveStatusMessage = "UI設定を最後まで書き込めませんでした。";
        m_debug.saveStatusTimer = 3.0f;
        Logger::Warn("Failed while writing UI tuning settings.");
        return false;
    }

    m_debug.saveStatusMessage = "UI設定を assets/ui_tuning.json に保存しました。";
    m_debug.saveStatusTimer = 3.0f;
    Logger::Info("Saved UI tuning settings to assets/ui_tuning.json");
    return true;
}

bool GameScene::LoadUiTuningState()
{
    std::ifstream stream(kUiTuningFilePath, std::ios::binary);
    if (!stream.is_open())
    {
        m_debug.saveStatusMessage = "UI設定ファイルはまだありません。";
        m_debug.saveStatusTimer = 3.0f;
        return false;
    }

    nlohmann::json root;
    try
    {
        stream >> root;
        ApplyUiTuningJson(root, m_ui);
    }
    catch (...)
    {
        m_debug.saveStatusMessage = "UI設定ファイルの読込に失敗しました。";
        m_debug.saveStatusTimer = 3.0f;
        Logger::Warn("Failed to load UI tuning settings.");
        return false;
    }

    m_debug.saveStatusMessage = "UI設定を assets/ui_tuning.json から読み込みました。";
    m_debug.saveStatusTimer = 3.0f;
    Logger::Info("Loaded UI tuning settings from assets/ui_tuning.json");
    return true;
}

bool GameScene::LoadProgressStateFromDisk()
{
    m_save = GameSceneSaveState{};
    std::ifstream stream(kGameProgressSavePath, std::ios::binary);
    if (!stream.is_open())
    {
        m_debug.saveStatusMessage = "No save file found.";
        m_debug.saveStatusTimer = 3.0f;
        return false;
    }

    nlohmann::json root;
    try
    {
        stream >> root;
    }
    catch (...)
    {
        m_debug.saveStatusMessage = "Save file is invalid.";
        m_debug.saveStatusTimer = 3.0f;
        return false;
    }

    const int version = root.value("version", 0);
    if (version != 1)
    {
        m_debug.saveStatusMessage = "Save file version is not supported.";
        m_debug.saveStatusTimer = 3.0f;
        return false;
    }

    m_save.hasData = true;
    m_save.mapCsvPath = root.value("mapCsvPath", m_lifecycle.currentMapCsvPath);
    const bool hadPersistedCheckpoint = root.value("hasCheckpoint", false);
    // チェックポイントは起動中だけ有効とし、過去バージョンの保存値は復元しない。
    m_save.hasCheckpoint = false;
    m_save.activeCheckpointId = -1;
    m_save.stageStartX = root.value("stageStartX", 0.0f);
    m_save.stageStartY = root.value("stageStartY", 0.0f);
    m_save.respawnX = root.value("respawnX", 0.0f);
    m_save.respawnY = root.value("respawnY", 0.0f);
    m_save.playerX = root.value("playerX", 0.0f);
    m_save.playerY = root.value("playerY", 0.0f);
    m_save.cameraX = root.value("cameraX", 0.0f);
    m_save.cameraY = root.value("cameraY", 0.0f);
    m_save.sessionMaxHp = root.value("sessionMaxHp", 3);
    m_save.sessionCurrentHp = root.value("sessionCurrentHp", m_save.sessionMaxHp);
    m_save.sessionParts = root.value("sessionParts", 0);
    m_save.sessionPhotoStorageSlots = root.value("sessionPhotoStorageSlots", 2);
    m_save.sessionHasRecoveryFilter = root.value("sessionHasRecoveryFilter", false);
    m_save.cameraTutorialCompleted = root.value("cameraTutorialCompleted", false);
    m_save.completedTutorialNumbers = root.value(
        "completedTutorialNumbers",
        std::vector<int>{});
    if (m_save.cameraTutorialCompleted &&
        std::find(
            m_save.completedTutorialNumbers.begin(),
            m_save.completedTutorialNumbers.end(),
            1) == m_save.completedTutorialNumbers.end())
    {
        // 旧形式の完了フラグをチュートリアル1番へ移行します。
        m_save.completedTutorialNumbers.push_back(1);
    }
    m_save.sessionTimeLimit = root.value("sessionTimeLimit", 60.0f);
    m_save.sessionTimeRemaining = root.value("sessionTimeRemaining", m_save.sessionTimeLimit);
    const auto photoIt = root.find("photo");
    if (photoIt != root.end() && photoIt->is_object())
    {
        m_save.photo = DeserializePhotoState(*photoIt);
    }
    if (hadPersistedCheckpoint)
    {
        // 古いチェックポイント地点から即座に再起動しないよう、開始地点へ移行する。
        m_save.respawnX = m_save.stageStartX;
        m_save.respawnY = m_save.stageStartY;
        m_save.playerX = m_save.stageStartX;
        m_save.playerY = m_save.stageStartY;
    }

    m_lifecycle.currentMapCsvPath = m_save.mapCsvPath;
    m_debug.saveStatusMessage = "Loaded save file.";
    m_debug.saveStatusTimer = 3.0f;
    Logger::Info("Loaded save file from " + std::string(kGameProgressSavePath));
    return true;
}

bool GameScene::SaveProgressState()
{
    m_save.hasData = true;
    m_save.mapCsvPath = m_lifecycle.currentMapCsvPath;
    // チェックポイントはゲーム終了時に破棄し、次回はステージ開始地点を使う。
    m_save.hasCheckpoint = false;
    m_save.activeCheckpointId = -1;
    m_save.stageStartX = m_flow.stageStartX;
    m_save.stageStartY = m_flow.stageStartY;
    m_save.respawnX = m_flow.stageStartX;
    m_save.respawnY = m_flow.stageStartY;
    m_save.cameraX = m_flow.cameraX;
    m_save.cameraY = m_flow.cameraY;
    m_save.photo = m_photo;

    if (const Entity* player = FindEntityByTag(kTagPlayer))
    {
        if (const auto* transform = player->GetComponent<TransformComponent>())
        {
            m_save.playerX = transform->x;
            m_save.playerY = transform->y;
        }

        if (const auto* health = player->GetComponent<HealthComponent>())
        {
            m_save.sessionMaxHp = health->GetMaxHealth();
            m_save.sessionCurrentHp = health->GetCurrentHealth();
        }
    }

    const GameSessionState& session = GameSession_Get();
    m_save.sessionMaxHp = session.maxHp;
    m_save.sessionCurrentHp = session.currentHp;
    m_save.sessionParts = session.parts;
    m_save.sessionPhotoStorageSlots = session.photoStorageSlots;
    m_save.sessionHasRecoveryFilter = session.hasRecoveryFilter;
    m_save.cameraTutorialCompleted = session.cameraTutorialCompleted;
    m_save.completedTutorialNumbers = session.completedTutorialNumbers;
    m_save.sessionTimeLimit = session.timeLimit;
    m_save.sessionTimeRemaining = session.timeRemaining;

    nlohmann::json root;
    root["version"] = 1;
    root["mapCsvPath"] = m_save.mapCsvPath;
    root["hasCheckpoint"] = m_save.hasCheckpoint;
    root["activeCheckpointId"] = m_save.activeCheckpointId;
    root["stageStartX"] = m_save.stageStartX;
    root["stageStartY"] = m_save.stageStartY;
    root["respawnX"] = m_save.respawnX;
    root["respawnY"] = m_save.respawnY;
    root["playerX"] = m_save.playerX;
    root["playerY"] = m_save.playerY;
    root["cameraX"] = m_save.cameraX;
    root["cameraY"] = m_save.cameraY;
    root["sessionMaxHp"] = m_save.sessionMaxHp;
    root["sessionCurrentHp"] = m_save.sessionCurrentHp;
    root["sessionParts"] = m_save.sessionParts;
    root["sessionPhotoStorageSlots"] = m_save.sessionPhotoStorageSlots;
    root["sessionHasRecoveryFilter"] = m_save.sessionHasRecoveryFilter;
    root["cameraTutorialCompleted"] = m_save.cameraTutorialCompleted;
    root["completedTutorialNumbers"] = m_save.completedTutorialNumbers;
    root["sessionTimeLimit"] = m_save.sessionTimeLimit;
    root["sessionTimeRemaining"] = m_save.sessionTimeRemaining;
    root["photo"] = SerializePhotoState(m_save.photo);

    std::ofstream stream(kGameProgressSavePath, std::ios::binary | std::ios::trunc);
    if (!stream.is_open())
    {
        m_debug.saveStatusMessage = "Failed to open save file for writing.";
        m_debug.saveStatusTimer = 3.0f;
        return false;
    }

    stream << root.dump(2);
    m_debug.saveStatusMessage = "Saved progress.";
    m_debug.saveStatusTimer = 3.0f;
    Logger::Info("Saved progress to " + std::string(kGameProgressSavePath));
    return true;
}

void GameScene::ApplyLoadedProgressState()
{
    if (!m_save.hasData)
    {
        return;
    }

    m_photo = m_save.photo;
    m_photo.pendingStore = PendingPhotoStoreState{};
    m_photo.placement.active = false;
    m_photo.placement.valid = false;
    m_photo.placement.blockedByUi = false;
    m_photo.placement.draggingFromTray = false;
    m_flow.hasCheckpoint = m_save.hasCheckpoint;
    m_flow.activeCheckpointId = m_save.activeCheckpointId;
    m_flow.stageStartX = m_save.stageStartX;
    m_flow.stageStartY = m_save.stageStartY;
    m_flow.respawnX = m_save.respawnX;
    m_flow.respawnY = m_save.respawnY;
    m_flow.cameraX = m_save.cameraX;
    m_flow.cameraY = m_save.cameraY;
    m_flow.timeLimit = m_save.sessionTimeLimit;
    m_flow.timeRemaining = m_save.sessionTimeRemaining;

    GameSession_Reset(m_save.sessionMaxHp, m_save.sessionTimeLimit);
    GameSession_SetCurrentHp(m_save.sessionCurrentHp);
    GameSession_AddParts(m_save.sessionParts);
    GameSession_SetPhotoStorageSlots(m_save.sessionPhotoStorageSlots);
    GameSession_SetRecoveryFilterOwned(m_save.sessionHasRecoveryFilter);
    gameSessionSetCompletedTutorialNumbers(m_save.completedTutorialNumbers);
    GameSession_SetTimeRemaining(m_save.sessionTimeRemaining);

    Entity* player = FindEntityByTag(kTagPlayer);
    if (player)
    {
        if (auto* transform = player->GetComponent<TransformComponent>())
        {
            transform->x = m_save.playerX;
            transform->y = m_save.playerY;
        }

        if (auto* health = player->GetComponent<HealthComponent>())
        {
            health->SetCurrentHealth(m_save.sessionCurrentHp);
        }

        game_scene_player_visual_system::ResetSpriteAnimationToIdle(m_player, *player);
    }

    for (Entity* entity : m_world.EntitiesByTag(EntityTag::Checkpoint))
    {
        if (!entity)
        {
            continue;
        }

        auto* checkpoint = entity->GetComponent<CheckpointComponent>();
        if (!checkpoint)
        {
            continue;
        }

        const bool shouldBeActive = m_save.hasCheckpoint && checkpoint->checkpointId == m_save.activeCheckpointId;
        checkpoint->activated = shouldBeActive;
    }

    m_ui.hpUiInitialized = false;
    m_ui.hpLastRaw = -1;
}

void GameScene::InitializeStageResources(ResourceManager& resources)
{
    LoadStageTransitionLinks();
    m_assets.LoadDefaults(resources);
    m_whiteTexture = m_assets.GetTexture("white");
    InitializeTestPhotoResources(resources);
    m_tileMap.LoadFromCsv(m_lifecycle.currentMapCsvPath, 48.0f);
    RefreshTileTextureForCurrentMap();
    const size_t mapCellCount =
        static_cast<size_t>((std::max)(0, m_tileMap.GetWidth())) *
        static_cast<size_t>((std::max)(0, m_tileMap.GetHeight()));
    m_world.Reserve(128 + mapCellCount / 8, 64);
    RefreshStageRenderProfile();
    gCameraViewWidth = kDefaultCameraViewWidth;
    gCameraViewHeight = kDefaultCameraViewHeight;
    m_eventBus.Reserve(128);
    m_eventBus.Clear();
    m_physicsWorld.Initialize(0.0f, 0.0f, m_eventBus);
}

void GameScene::InitializeStageEntities()
{
    PrefabFactory prefabs(m_assets, m_physicsWorld, m_eventBus);
    const bool isDebugStageMap = m_lifecycle.currentMapCsvPath == "assets/maps/stage_a.csv";
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
    const std::vector<TileMarker> stageMarkers = CollectTileMarkers(m_tileMap);

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'G')
        {
            continue;
        }

        goalX = static_cast<float>(stageMarker.column) * tileSize;
        goalY = static_cast<float>(stageMarker.row) * tileSize;
        goalMarkerFound = true;
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
    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != '*')
        {
            continue;
        }

        playerSpawnX = static_cast<float>(stageMarker.column) * tileSize;
        playerSpawnY = static_cast<float>(stageMarker.row) * tileSize;
        break;
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

    const bool hasBarrelMarker = std::any_of(
        stageMarkers.begin(),
        stageMarkers.end(),
        [](const TileMarker& stageMarker)
        {
            return stageMarker.marker == 'B';
        });

    if (isDebugStageMap && !hasBarrelMarker)
    {
        spawnRespawnableBarrel(
            AlignToGrid(432.0f, tileSize),
            AlignToGrid(240.0f, tileSize));
    }

    const auto placeGroundedEnemyAtMarker = [&](Entity& enemy, int column, int row) -> TransformComponent*
    {
        auto* transform = enemy.GetComponent<TransformComponent>();
        if (!transform)
        {
            return nullptr;
        }

        transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
        float spawnX = transform->x;
        float spawnY = transform->y;
        if (FindSpawnPosition(transform->x, transform->width * transform->scale, transform->height * transform->scale, spawnX, spawnY))
        {
            transform->x = spawnX;
            transform->y = spawnY;
        }
        SnapEnemyToGround(*transform);
        return transform;
    };

    const auto spawnMidBoss3Fists = [&](PrefabFactory& prefabs, Entity& boss)
    {
        auto* bossTransform = boss.GetComponent<TransformComponent>();
        auto* bossComp = boss.GetComponent<MidBoss3Component>();
        if (!bossTransform || !bossComp)
        {
            return;
        }

        bossComp->fistEntities.clear();
        const float offsets[4][2] = {
            { tileSize * 1.0f, -tileSize * 3.0f },
            { tileSize * 1.0f,  tileSize * 5.0f },
            { tileSize * 5.0f, -tileSize * 2.0f },
            { tileSize * 5.0f,  tileSize * 4.5f },
        };

        for (int index = 0; index < 4; ++index)
        {
            Entity& fist = SpawnStagePrefab(
                prefabs,
                "sandbox_mid_boss3_fist",
                bossTransform->x + offsets[index][0],
                bossTransform->y + offsets[index][1]);
            auto& fistComp = fist.AddComponent<MidBoss3FistComponent>();
            fistComp.ownerBoss = &boss;
            fistComp.fistIndex = index;
            fistComp.baseOffsetX = offsets[index][0];
            fistComp.baseOffsetY = offsets[index][1];
            fistComp.idlePhase = static_cast<float>(index) * 1.35f;
            bossComp->fistEntities.push_back(&fist);
        }
    };

    // Spawn walker/ranged enemies from CSV markers.
    for (const TileMarker& stageMarker : stageMarkers)
    {
        const int column = stageMarker.column;
        const int row = stageMarker.row;
        const char marker = stageMarker.marker;
        if (marker == 'W') // Walker
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_walker",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            ConfigureWalkerSpriteAnimation(enemy);
            placeGroundedEnemyAtMarker(enemy, column, row);
        }
        else if (marker == 'R') // Ranged
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_ranged",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            ConfigureRangedSpriteAnimation(enemy);
            placeGroundedEnemyAtMarker(enemy, column, row);
        }
        else if (marker == '$') // Charger
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_charger",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = placeGroundedEnemyAtMarker(enemy, column, row))
            {
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
            if (auto* transform = placeGroundedEnemyAtMarker(boss, column, row))
            {
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                    enemy->respawnEnabled = false;
                }

                if (auto* bossComp = boss.GetComponent<ShieldBossComponent>())
                {
                    constexpr float kShieldW = 48.0f;
                    constexpr float kShieldH = 192.0f;
                    constexpr float kShieldRaiseOffsetY = 24.0f;
                    auto shieldEntity = std::make_unique<Entity>();
                    shieldEntity->AddComponent<TagComponent>("BossShield");
                    shieldEntity->AddComponent<TransformComponent>(
                        transform->x - kShieldW,
                        transform->y - kShieldRaiseOffsetY,
                        kShieldW,
                        kShieldH);
                    shieldEntity->AddComponent<TintComponent>(
                        0.72f,
                        0.78f,
                        0.90f,
                        (!bossComp->combatStarted && !bossComp->appearAnimationFinished) ? 0.0f : 1.0f);
                    shieldEntity->AddComponent<SpriteRenderComponent>(m_whiteTexture);
                    auto& shieldComp = shieldEntity->AddComponent<ShieldComponent>();
                    shieldComp.attached = true;
                    shieldComp.ownerBoss = &boss;
                    shieldComp.contactDamage = 1;
                    shieldComp.followOffsetX = -kShieldW;
                    shieldComp.followOffsetY = 0.0f;
                    bossComp->shieldEntity = shieldEntity.get();
                    m_world.Spawn(std::move(shieldEntity));
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
            if (auto* transform = placeGroundedEnemyAtMarker(boss, column, row))
            {
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                }
            }
        }
        else if (marker == '%') // MidBoss3
        {
            Entity& boss = SpawnStagePrefab(
                prefabs,
                "sandbox_mid_boss3",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = boss.GetComponent<TransformComponent>())
            {
                transform->x = static_cast<float>(column) * tileSize + (tileSize - transform->width * transform->scale) * 0.5f;
                transform->y = static_cast<float>(row) * tileSize + (tileSize - transform->height * transform->scale) * 0.5f;
                transform->y += tileSize * 2.0f;
                if (auto* enemy = boss.GetComponent<EnemyComponent>())
                {
                    enemy->spawnX = transform->x;
                    enemy->spawnY = transform->y;
                    enemy->respawnEnabled = false;
                }
                if (auto* bossComp = boss.GetComponent<MidBoss3Component>())
                {
                    bossComp->homeX = transform->x;
                    bossComp->homeY = transform->y;
                    bossComp->initializedHome = true;
                    bossComp->introWaitingForTrigger = true;
                    bossComp->introStarted = false;
                    bossComp->introFinished = false;
                    bossComp->introGroundInitialized = false;
                    bossComp->introTimer = 0.0f;
                    bossComp->introFloatHomeX = transform->x;
                    bossComp->introFloatHomeY = transform->y;
                    bossComp->introTriggerX = transform->x - tileSize * 7.0f;
                }
                spawnMidBoss3Fists(prefabs, boss);
            }
        }
        else if (marker == 'A') // 繧ｴ繝ｼ繧ｹ繝・
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
        else if (marker == 'D') // 繝悶Λ繝ｭ繝・
        {
            Entity& enemy = SpawnStagePrefab(
                prefabs,
                "sandbox_enemy_blaster_robot",
                static_cast<float>(column) * tileSize,
                static_cast<float>(row) * tileSize);
            if (auto* transform = enemy.GetComponent<TransformComponent>())
            {
                // FindSpawnPosition繧剃ｽｿ繧上★CSV縺ｮ蠎ｧ讓吶ｒ縺昴・縺ｾ縺ｾ菴ｿ縺・
                transform->x = static_cast<float>(column) * tileSize;
                transform->y = static_cast<float>(row) * tileSize;
                if (auto* enemyComp = enemy.GetComponent<EnemyComponent>())
                {
                    enemyComp->spawnX = transform->x;
                    enemyComp->spawnY = transform->y;
                }
                if (auto* blasterRobot = enemy.GetComponent<BlasterRobotComponent>())
                {
                    // 螟ｩ莠募愛螳夲ｼ壹・繝ｼ繧ｫ繝ｼ縺ｮ荳翫・繧ｿ繧､繝ｫ縺悟｣√↑繧牙､ｩ莠戊ｨｭ鄂ｮ
                    if (row > 0 && m_tileMap.GetTile(column, row - 1) > 0)
                    {
                        blasterRobot->mountedOnCeiling = true;
                    }
                }
            }
        }
        else if (marker == ';')
        {
            constexpr float kMerchantSignAspect = 401.0f / 1172.0f;
            const float merchantX = static_cast<float>(column) * tileSize;
            const float merchantY = static_cast<float>(row) * tileSize;
            const float merchantSize = tileSize * 4.0f;
            const float signWidth = tileSize * 1.8f;
            const float signHeight = signWidth * kMerchantSignAspect;
            auto merchant = std::make_unique<Entity>();
            merchant->AddComponent<TagComponent>(EntityTag::Merchant);
            merchant->AddComponent<TransformComponent>(
                merchantX,
                merchantY,
                merchantSize,
                merchantSize);
            const int merchantTexture = m_assets.GetTexture("merchant_sign");
            merchant->AddComponent<MerchantComponent>();
            m_world.Spawn(std::move(merchant));

            auto sign = std::make_unique<Entity>();
            sign->AddComponent<TransformComponent>(
                merchantX + (merchantSize - signWidth) * 0.5f,
                merchantY - signHeight - tileSize * 0.15f,
                signWidth,
                signHeight);
            sign->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
            sign->AddComponent<SpriteRenderComponent>(merchantTexture >= 0 ? merchantTexture : m_whiteTexture);
            m_world.Spawn(std::move(sign));
        }
    }

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'Y')
        {
            continue;
        }

        const char marker2 = static_cast<char>(std::toupper(static_cast<unsigned char>(
            m_tileMap.GetMarker2(stageMarker.column, stageMarker.row))));
        if (marker2 != '\0')
        {
            continue;
        }

        auto battery = std::make_unique<Entity>();
        battery->AddComponent<TagComponent>(kTagBattery);
        battery->AddComponent<TransformComponent>(
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize),
            tileSize,
            tileSize);
        const int batteryTexture = m_assets.GetTexture("tile_value_battery");
        battery->AddComponent<TintComponent>(1.0f, 1.0f, 1.0f, 1.0f);
        battery->AddComponent<SpriteRenderComponent>(batteryTexture >= 0 ? batteryTexture : m_whiteTexture);
        battery->AddComponent<BatteryComponent>(
            1900.0f,
            980.0f,
            260.0f,
            320.0f,
            1);
        if (m_lifecycle.darknessStageEnabled)
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
        m_world.Spawn(std::move(battery));
    }

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'B')
        {
            continue;
        }

        spawnRespawnableBarrel(
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize));
    }

    RefreshLogsFromMarkers();
    RefreshJumpPadsFromMarkers();
    ReflashFallingRockfromMarkers();
    RefreshHangingGravityObjectsFromMarkers();
    RefreshMarkerLightsFromMarkers();
    RefreshStageLightsFromMarkers();

    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'V')
        {
            continue;
        }

        Entity& vanishObject = SpawnStagePrefab(
            prefabs,
            "sandbox_vanish_object",
            AlignToGrid(static_cast<float>(stageMarker.column) * tileSize, tileSize),
            AlignToGrid(static_cast<float>(stageMarker.row) * tileSize, tileSize));
        vanishObject.AddComponent<PhotoCopyRoleComponent>(PhotoCopyRole::Solid);
        vanishObject.AddComponent<PhotoCopyLayerComponent>(PhotoCopyLayer::Foreground);
        vanishObject.AddComponent<PhotoCopyOriginComponent>(PhotoCopyOrigin::Generic);
        vanishObject.AddComponent<PhotoCopyEffectComponent>(PhotoFilterTheme::None);
        vanishObject.AddComponent<VanishOnCaptureComponent>(true);
    }

    // 同じ列に複数ある場合は、地面に近い一番下のマーカーを復帰地点として採用する。
    std::vector<const TileMarker*> checkpointMarkersByColumn(
        static_cast<size_t>((std::max)(0, m_tileMap.GetWidth())),
        nullptr);
    for (const TileMarker& stageMarker : stageMarkers)
    {
        if (stageMarker.marker != 'C')
        {
            continue;
        }

        const size_t columnIndex = static_cast<size_t>(stageMarker.column);
        const TileMarker* currentMarker = checkpointMarkersByColumn[columnIndex];
        if (!currentMarker || stageMarker.row > currentMarker->row)
        {
            checkpointMarkersByColumn[columnIndex] = &stageMarker;
        }
    }

    int checkpointId = 0;
    const float checkpointTriggerHeight = (std::max)(tileSize, GetMapPixelHeight());
    for (const TileMarker* stageMarker : checkpointMarkersByColumn)
    {
        if (!stageMarker)
        {
            continue;
        }

        // 判定は縦列全体、復帰地点は実際のCマーカー位置として分離する。
        const float checkpointX = AlignToGrid(static_cast<float>(stageMarker->column) * tileSize, tileSize);
        const float checkpointY = AlignToGrid(static_cast<float>(stageMarker->row) * tileSize - tileSize, tileSize);
        Entity& checkpoint = SpawnStagePrefab(
            prefabs,
            "sandbox_checkpoint",
            checkpointX,
            0.0f);
        if (auto* transform = checkpoint.GetComponent<TransformComponent>())
        {
            transform->x = checkpointX;
            transform->y = 0.0f;
            transform->width = tileSize;
            transform->height = checkpointTriggerHeight;
            transform->rotation = 0.0f;
            transform->scale = 1.0f;
        }
        SetEntityTint(checkpoint, 1.0f, 1.0f, 1.0f, 0.0f);
        if (auto* light = checkpoint.GetComponent<FlickerLightComponent>())
        {
            light->intensity = 0.0f;
            light->godRayEnabled = false;
            light->godRayIntensity = 0.0f;
        }
        checkpoint.AddComponent<CheckpointComponent>(checkpointId, checkpointX, checkpointY);
        ++checkpointId;
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

    RefreshDamageFootholdsFromMarkers();
    RefreshConveyorBeltsFromMarkers();
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

    // Choose backdrop keys from m_lifecycle.currentMapCsvPath and cache texture IDs
    auto ResolveBackdropKeysForMap = [](const std::string& mapPath) -> std::pair<std::string, std::string>
    {
        std::string stem;
        try { stem = std::filesystem::path(mapPath).stem().string(); } catch (...) { stem = mapPath; }
        std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        // CSV 蜷阪↓ "forest" 縺ｾ縺溘・ "ruins" 繧貞性繧√※隴伜挨縺励※縺・ｋ縺ｨ縺ｮ縺薙→縺ｪ縺ｮ縺ｧ縺昴ｌ縺ｫ蜷医ｏ縺帙ｋ
		if (stem.find("ruins") != std::string::npos) return { "ruins_bg", "ruins_fg" };//繝槭ャ繝励・CSV繝輔ぃ繧､繝ｫ蜷阪↓ "ruins" 繧貞性繧蝣ｴ蜷医・蟒・｢溘・閭梧勹縺ｨ蜑肴勹繧剃ｽｿ逕ｨ
        if (stem.find("forest") != std::string::npos) return { "forest_bg", "forest_fg" };        // 繝・ヵ繧ｩ繝ｫ繝・
        return { "forest_bg", "forest_fg" };
    };

    {
        const auto keys = ResolveBackdropKeysForMap(m_lifecycle.currentMapCsvPath);
        m_camera.backdropTextureId = m_assets.GetTexture(keys.first);
        m_camera.backdropTexture1Id = m_assets.GetTexture(keys.second);
        // manifest 未登録なら既存キーにフォールバック

        if (m_camera.backdropTextureId < 0) m_camera.backdropTextureId = m_assets.GetTexture("forest_bg");
        if (m_camera.backdropTexture1Id < 0) m_camera.backdropTexture1Id = m_assets.GetTexture("forest_fg");
    }
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
    if (auto* midBoss2 = entityRef.GetComponent<MidBoss2Component>())
    {
        midBoss2->params = m_tuning.midBoss2Params;
    }

    m_world.Spawn(std::move(entity));
    return entityRef;
}

void GameScene::ApplyMidBoss2TuningToActiveBosses()
{
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        auto* enemy = entity->GetComponent<EnemyComponent>();
        auto* boss = entity->GetComponent<MidBoss2Component>();
        if (!enemy || !boss || enemy->GetArchetype() != EnemyArchetype::MidBoss2)
        {
            continue;
        }

        boss->params = m_tuning.midBoss2Params;
    }
}

