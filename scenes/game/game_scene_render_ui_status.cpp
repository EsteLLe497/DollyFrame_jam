#include "pch.h"

#include "game_scene_internal.h"
#include "game_scene_hp_ui_layout.h"
#include "game_scene_combat_common.h"

#include "DxLib.h"

using namespace game_scene_detail;

namespace
{
    constexpr const char* kCameraBodyTextureKey = "ui_camera_body";
    constexpr const char* kCameraNoFilterTextureKey = "ui_camera_nofilter";
    constexpr const char* kCameraFlashOffTextureKey = "ui_camera_flash_off";
    constexpr const char* kCameraFlashOnTextureKey = "ui_camera_flash_on";
    constexpr const char* kCameraHealTextureKey = "ui_camera_heal";
    constexpr const char* kHpTextureKey = "ui_hp";
    constexpr const char* kHpDamageTextureKey = "ui_hp_damage";
    constexpr const char* kPartsCounterPanelTextureKey = "ui_parts_counter_panel";
    constexpr const char* kPartsCounterDigitsTextureKey = "ui_parts_counter_digits";

    constexpr float kCameraHudX = 20.0f;
    constexpr float kCameraHudY = 20.0f;
    constexpr float kCameraHudWidth = 200.0f;
    constexpr float kCameraHudHeight = 140.0f;
    constexpr float kCameraSourceBodyWidth = 1980.0f;
    constexpr float kCameraSourceBodyHeight = 1350.0f;
    constexpr float kCameraSourceFlashX = 500.0f;
    constexpr float kCameraSourceFlashOffY = 0.0f;
    constexpr float kCameraSourceFlashOnY = -20.0f;
    constexpr float kCameraSourceFlashWidth = 980.0f;
    constexpr float kCameraSourceFlashOffHeight = 200.0f;
    constexpr float kCameraSourceFlashOnHeight = 280.0f;
    constexpr float kCameraFilterHudAnimationDuration = 0.86f;

    float EaseOutCubic(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    float EaseInOut(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    PhotoFilterTheme NormalizeCameraHudTheme(PhotoFilterTheme theme)
    {
        switch (theme)
        {
        case PhotoFilterTheme::Cold:
        case PhotoFilterTheme::Sepia:
            return theme;
        case PhotoFilterTheme::None:
        case PhotoFilterTheme::Hot:
        case PhotoFilterTheme::Invert:
        default:
            return PhotoFilterTheme::None;
        }
    }

    void DrawSepiaHudFilmEffect(float x, float y, float width, float height)
    {
        const int left = static_cast<int>(std::round(x));
        const int top = static_cast<int>(std::round(y));
        const int right = static_cast<int>(std::round(x + width));
        const int bottom = static_cast<int>(std::round(y + height));
        if (right <= left || bottom <= top)
        {
            return;
        }

        const int frame = GetNowCount() / 33;
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 96);
        DrawBox(left, top, right, bottom, GetColor(176, 135, 42), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 72);
        DrawBox(left, top, right, bottom, GetColor(238, 202, 142), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 86);
        for (int i = 0; i < 7; ++i)
        {
            const int lineX = left + ((frame * 5 + i * 31) % std::max(1, right - left));
            DrawLine(lineX, top + 6, lineX - 8, bottom - 8, GetColor(98, 72, 38));
        }
        for (int i = 0; i < 18; ++i)
        {
            const int dotX = left + ((frame * 11 + i * 23) % std::max(1, right - left));
            const int dotY = top + ((frame * 7 + i * 19) % std::max(1, bottom - top));
            DrawCircle(dotX, dotY, 1 + (i % 2), GetColor(255, 250, 222), TRUE);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void DrawCameraFilterSheet(PhotoFilterTheme theme, float x, float y, float width, float height, float alpha)
    {
        const PhotoFilterTheme normalizedTheme = NormalizeCameraHudTheme(theme);
        if (normalizedTheme == PhotoFilterTheme::None || alpha <= 0.0f)
        {
            return;
        }

        const int blendAlpha = static_cast<int>(std::round(std::clamp(alpha, 0.0f, 1.0f) * 180.0f));
        const int color = normalizedTheme == PhotoFilterTheme::Sepia
            ? GetColor(205, 178, 68)
            : GetColor(108, 255, 162);
        const int lineColor = normalizedTheme == PhotoFilterTheme::Sepia
            ? GetColor(98, 72, 38)
            : GetColor(28, 142, 82);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, blendAlpha);
        DrawBoxAA(x, y, x + width, y + height, color, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(alpha * 120.0f)));
        for (int i = 0; i < 7; ++i)
        {
            const float lineY = y + 12.0f + i * (height - 24.0f) / 6.0f;
            DrawLineAA(x + 14.0f, lineY, x + width - 14.0f, lineY + (i % 2 == 0 ? 4.0f : -3.0f), lineColor, 1.5f);
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    unsigned int GetAttackCaptureIconColor(CapturedSpawnArchetype archetype)
    {
        switch (archetype)
        {
        case CapturedSpawnArchetype::WalkerMelee:
            return GetColor(255, 94, 42);
        case CapturedSpawnArchetype::Projectile:
            return GetColor(255, 214, 72);
        case CapturedSpawnArchetype::ShieldRushBurst:
        case CapturedSpawnArchetype::ShieldJumpBurst:
        case CapturedSpawnArchetype::ShieldNormal:
            return GetColor(86, 156, 255);
        case CapturedSpawnArchetype::MidBoss3FistAttack:
            return GetColor(255, 136, 54);
        case CapturedSpawnArchetype::MidBoss3DrillAttack:
            return GetColor(255, 196, 76);
        case CapturedSpawnArchetype::LaserTurret:
            return GetColor(82, 168, 255);
        default:
            return GetColor(210, 86, 255);
        }
    }

    int GetAttackCaptureCount(const PhotoCaptureState& capture)
    {
        if (capture.attackCaptureCount > 0)
        {
            return capture.attackCaptureCount;
        }

        const int countedItems = static_cast<int>(std::count_if(
            capture.items.begin(),
            capture.items.end(),
            [](const CapturedPhotoItem& item)
            {
                return item.enemyAttackPaste;
            }));
        if (countedItems > 0)
        {
            return countedItems;
        }

        return capture.hasPhoto && capture.containsEnemyAttackPaste ? 1 : 0;
    }

    CapturedSpawnArchetype GetPrimaryAttackCaptureArchetype(const PhotoCaptureState& capture)
    {
        for (const auto& item : capture.items)
        {
            if (item.enemyAttackPaste)
            {
                return item.spawnArchetype;
            }
        }

        return CapturedSpawnArchetype::None;
    }

    void DrawParallelogramBarSegment(
        float left,
        float right,
        float y,
        float height,
        float slant,
        unsigned int color)
    {
        DrawQuadrangleAA(
            left + slant,
            y,
            right + slant,
            y,
            right,
            y + height,
            left,
            y + height,
            color,
            TRUE);
    }

    void DrawBossHpParallelogramGauge(
        const GameSceneUiBossHpTuning& bossUi,
        int currentHp,
        int maxHp,
        float scale = 1.0f)
    {
        const float safeScale = std::max(0.1f, scale);
        const float width = bossUi.panelWidth * safeScale;
        const float height = bossUi.barHeight * safeScale;
        const float slant = std::max(1.0f, bossUi.panelPadding * safeScale);
        const float marginRight = std::max(0.0f, bossUi.panelExtraHeight);
        const float marginBottom = std::max(0.0f, bossUi.marginTop);
        const float x = static_cast<float>(SCREEN_WIDTH) - width - slant - marginRight;
        const float y = static_cast<float>(SCREEN_HEIGHT) - height - marginBottom;
        const int segments = std::clamp(maxHp, 1, 12);
        const float segmentWidth = width / static_cast<float>(segments);
        const float fillRatio = std::clamp(static_cast<float>(currentHp) / static_cast<float>(std::max(1, maxHp)), 0.0f, 1.0f);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 188);
        DrawParallelogramBarSegment(x - 5.0f, x + width + 5.0f, y - 6.0f, height + 12.0f, slant, GetColor(1, 6, 34));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        for (int i = 0; i < segments; ++i)
        {
            const float left = x + static_cast<float>(i) * segmentWidth + 1.0f;
            const float right = x + static_cast<float>(i + 1) * segmentWidth - 1.0f;
            const float segmentStart = static_cast<float>(i) / static_cast<float>(segments);
            const float segmentEnd = static_cast<float>(i + 1) / static_cast<float>(segments);
            const float segmentFill = std::clamp((fillRatio - segmentStart) / (segmentEnd - segmentStart), 0.0f, 1.0f);

            DrawParallelogramBarSegment(left, right, y, height, slant, GetColor(31, 36, 58));
            if (segmentFill > 0.0f)
            {
                const float fillRight = std::lerp(left, right, segmentFill);
                DrawParallelogramBarSegment(left, fillRight, y, height, slant, GetColor(248, 250, 255));
            }
        }

        for (int i = 1; i < segments; ++i)
        {
            const float lineX = x + static_cast<float>(i) * segmentWidth;
            DrawLineAA(lineX + slant, y + 1.0f, lineX, y + height - 1.0f, GetColor(118, 126, 150), 1.3f);
        }

        DrawLineAA(x + slant, y, x + width + slant, y, GetColor(210, 220, 242), 1.2f);
        DrawLineAA(x + width + slant, y, x + width, y + height, GetColor(210, 220, 242), 1.2f);
        DrawLineAA(x + width, y + height, x, y + height, GetColor(210, 220, 242), 1.2f);
        DrawLineAA(x, y + height, x + slant, y, GetColor(210, 220, 242), 1.2f);
    }

    void UpdateBossHpUiState(
        GameSceneBossHpUiState& state,
        int currentHp,
        int maxHp,
        float deltaTime,
        bool keepVisibleAtZero = false)
    {
        constexpr float kRevealSpeed = 2.35f;
        constexpr float kDisplayDropSpeed = 18.0f;
        constexpr float kDisplayRecoverSpeed = 10.0f;
        constexpr float kDamageLagSpeed = 1.55f;
        constexpr float kFlashDecaySpeed = 3.8f;

        const int safeMaxHp = std::max(1, maxHp);
        const int safeCurrentHp = std::clamp(currentHp, 0, safeMaxHp);
        const float targetRatio = static_cast<float>(safeCurrentHp) / static_cast<float>(safeMaxHp);

        if (!state.initialized || state.lastMax != safeMaxHp)
        {
            state.displayRatio = targetRatio;
            state.damageLagRatio = targetRatio;
            state.flash = 0.0f;
            state.reveal = 0.0f;
            state.lastRaw = safeCurrentHp;
            state.lastMax = safeMaxHp;
            state.initialized = true;
        }
        else
        {
            if (state.lastRaw >= 0 && safeCurrentHp < state.lastRaw)
            {
                state.flash = 1.0f;
            }
            state.lastRaw = safeCurrentHp;

            const float displaySpeed = targetRatio < state.displayRatio
                ? kDisplayDropSpeed
                : kDisplayRecoverSpeed;
            state.displayRatio += (targetRatio - state.displayRatio) * std::min(1.0f, deltaTime * displaySpeed);
            state.displayRatio = std::clamp(state.displayRatio, 0.0f, 1.0f);

            if (state.damageLagRatio < state.displayRatio)
            {
                state.damageLagRatio = state.displayRatio;
            }
            else
            {
                state.damageLagRatio += (state.displayRatio - state.damageLagRatio) * std::min(1.0f, deltaTime * kDamageLagSpeed);
                state.damageLagRatio = std::clamp(state.damageLagRatio, 0.0f, 1.0f);
            }
        }

        state.visible = safeCurrentHp > 0 || keepVisibleAtZero;
        state.reveal += (1.0f - state.reveal) * std::min(1.0f, deltaTime * kRevealSpeed);
        state.flash = std::max(0.0f, state.flash - deltaTime * kFlashDecaySpeed);
    }

    void DrawEnderInspiredBossHpGauge(const GameSceneUiBossHpTuning& bossUi, const GameSceneBossHpUiState& state)
    {
        if (!state.initialized || !state.visible || state.reveal <= 0.01f)
        {
            return;
        }

        const float revealT = EaseOutCubic(state.reveal);
        const float frameAlphaScale = std::clamp(state.reveal * 4.0f, 0.0f, 1.0f);
        const float fillIntroT = EaseOutCubic(std::clamp((state.reveal - 0.22f) / 0.78f, 0.0f, 1.0f));
        const float width = bossUi.panelWidth;
        const float height = bossUi.barHeight;
        constexpr float kOrnamentWidth = 42.0f;
        const float x = static_cast<float>(SCREEN_WIDTH) - width - bossUi.panelExtraHeight - kOrnamentWidth;
        const float baseY = static_cast<float>(SCREEN_HEIGHT) - height - bossUi.marginTop;
        const float y = baseY + (1.0f - revealT) * 28.0f;
        const float alphaScale = std::clamp(revealT, 0.0f, 1.0f);
        const int baseAlpha = static_cast<int>(std::round(210.0f * frameAlphaScale));
        const int lineAlpha = static_cast<int>(std::round(185.0f * frameAlphaScale));
        const int fillAlpha = static_cast<int>(std::round(235.0f * alphaScale));
        const float introFillWidth = width * fillIntroT;
        const float displayWidth = std::min(width * std::clamp(state.displayRatio, 0.0f, 1.0f), introFillWidth);
        const float lagWidth = std::min(width * std::clamp(state.damageLagRatio, 0.0f, 1.0f), introFillWidth);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(118.0f * frameAlphaScale)));
        DrawBoxAA(x - 20.0f, y - 14.0f, x + width + 20.0f, y + height + 18.0f, GetColor(0, 0, 0), TRUE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, baseAlpha);
        DrawBoxAA(x, y, x + width, y + height, GetColor(20, 5, 8), TRUE);
        DrawBoxAA(x, y + height * 0.55f, x + width, y + height, GetColor(36, 8, 12), TRUE);

        if (lagWidth > displayWidth + 0.5f)
        {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(190.0f * alphaScale)));
            DrawBoxAA(x, y + 1.0f, x + lagWidth, y + height - 1.0f, GetColor(248, 78, 58), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(92.0f * alphaScale)));
            DrawBoxAA(x, y + 1.0f, x + lagWidth, y + height * 0.45f, GetColor(255, 184, 150), TRUE);
        }

        if (displayWidth > 0.5f)
        {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, fillAlpha);
            DrawBoxAA(x, y + 1.0f, x + displayWidth, y + height - 1.0f, GetColor(172, 18, 32), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(128.0f * alphaScale)));
            DrawBoxAA(x, y + 2.0f, x + displayWidth, y + height * 0.42f, GetColor(255, 112, 104), TRUE);
        }

        if (fillIntroT > 0.01f && fillIntroT < 0.995f)
        {
            const float sweepX = x + introFillWidth;
            SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(150.0f * alphaScale)));
            DrawLineAA(sweepX, y - 3.0f, sweepX + 8.0f, y + height + 3.0f, GetColor(255, 210, 176), 2.0f);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(78.0f * alphaScale)));
            DrawBoxAA(std::max(x, sweepX - 42.0f), y + 1.0f, sweepX, y + height - 1.0f, GetColor(255, 128, 96), TRUE);
        }

        if (state.flash > 0.0f)
        {
            SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(120.0f * state.flash * alphaScale)));
            DrawBoxAA(x, y - 4.0f, x + width, y + height + 4.0f, GetColor(255, 90, 82), TRUE);
        }

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, lineAlpha);
        DrawLineAA(x - 42.0f, y - 8.0f, x + width + 42.0f, y - 8.0f, GetColor(244, 166, 150), 1.4f);
        DrawLineAA(x - 18.0f, y + height + 8.0f, x + width + 18.0f, y + height + 8.0f, GetColor(166, 58, 58), 1.2f);
        DrawBoxAA(x - 1.0f, y - 1.0f, x + width + 1.0f, y + height + 1.0f, GetColor(238, 164, 150), FALSE);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(170.0f * frameAlphaScale)));
        DrawCircleAA(x - 32.0f, y + height * 0.5f, 5.0f, 18, GetColor(238, 162, 146), TRUE);
        DrawCircleAA(x + width + 32.0f, y + height * 0.5f, 5.0f, 18, GetColor(238, 162, 146), TRUE);
        DrawLineAA(x - 28.0f, y + height * 0.5f, x - 8.0f, y + height * 0.5f, GetColor(238, 162, 146), 1.2f);
        DrawLineAA(x + width + 8.0f, y + height * 0.5f, x + width + 28.0f, y + height * 0.5f, GetColor(238, 162, 146), 1.2f);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

void GameScene::DrawCameraStatusHud() const
{
    const int bodyTexture = m_assets.GetTexture(kCameraBodyTextureKey);
    const int noFilterTexture = m_assets.GetTexture(kCameraNoFilterTextureKey);
    const int flashOffTexture = m_assets.GetTexture(kCameraFlashOffTextureKey);
    const int flashOnTexture = m_assets.GetTexture(kCameraFlashOnTextureKey);
    const int healCameraTexture = m_assets.GetTexture(kCameraHealTextureKey);
    const bool recoveryFilterOwned = GameSession_Get().hasRecoveryFilter;
    if (bodyTexture < 0)
    {
        return;
    }

    const auto drawCamera = [&](PhotoFilterTheme theme, float x, float y, float alpha, bool drawFlashPart)
    {
        if (alpha <= 0.0f)
        {
            return;
        }

        const PhotoFilterTheme normalizedTheme = NormalizeCameraHudTheme(theme);
        const bool drawHealCamera = normalizedTheme == PhotoFilterTheme::Cold && recoveryFilterOwned && healCameraTexture >= 0;
        const bool drawSepiaCamera = normalizedTheme == PhotoFilterTheme::Sepia && noFilterTexture >= 0;
        const int baseTexture = drawHealCamera
            ? healCameraTexture
            : drawSepiaCamera
                ? noFilterTexture
                : bodyTexture;

        if (drawSepiaCamera)
        {
            DrawSepiaHudFilmEffect(x + 14.0f, y + 18.0f, kCameraHudWidth - 28.0f, kCameraHudHeight - 34.0f);
        }

        Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        Shader_SetTint(1.0f, 1.0f, 1.0f, std::clamp(alpha, 0.0f, 1.0f));
        SpriteDraw(
            baseTexture,
            x,
            y,
            kCameraHudWidth,
            kCameraHudHeight,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        if (!drawHealCamera && !drawSepiaCamera && drawFlashPart && m_ui.cameraFlash.unlocked)
        {
            const bool flashActive =
                m_ui.cameraFlash.enabled &&
                m_ui.cameraFlash.pulseRemaining > 0.0f &&
                flashOnTexture >= 0;
            const int flashTexture = flashActive ? flashOnTexture : flashOffTexture;
            if (flashTexture >= 0)
            {
                const float flashSourceY = flashActive ? kCameraSourceFlashOnY : kCameraSourceFlashOffY;
                const float flashSourceHeight = flashActive ? kCameraSourceFlashOnHeight : kCameraSourceFlashOffHeight;
                const float flashX = x + kCameraHudWidth * (kCameraSourceFlashX / kCameraSourceBodyWidth);
                const float flashY = y + kCameraHudHeight * (flashSourceY / kCameraSourceBodyHeight);
                const float flashWidth = kCameraHudWidth * (kCameraSourceFlashWidth / kCameraSourceBodyWidth);
                const float flashHeight = kCameraHudHeight * (flashSourceHeight / kCameraSourceBodyHeight);
                SpriteDraw(
                    flashTexture,
                    flashX,
                    flashY,
                    flashWidth,
                    flashHeight,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f);
            }
        }
    };

    const float animationT = std::clamp(
        m_ui.cameraFilterAnimationElapsed / kCameraFilterHudAnimationDuration,
        0.0f,
        1.0f);
    const bool animating = animationT < 1.0f;
    if (!animating)
    {
        drawCamera(m_photo.capture.selectedTheme, kCameraHudX, kCameraHudY, 1.0f, true);
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        return;
    }

    const PhotoFilterTheme fromTheme = NormalizeCameraHudTheme(m_ui.cameraFilterAnimationFrom);
    const PhotoFilterTheme toTheme = NormalizeCameraHudTheme(m_ui.cameraFilterAnimationTo);
    const bool clearingFilter = fromTheme != PhotoFilterTheme::None && toTheme == PhotoFilterTheme::None;
    const float cameraDownT = EaseInOut(std::min(animationT / 0.34f, 1.0f));
    const float cameraReturnT = EaseInOut(std::clamp((animationT - 0.34f) / 0.28f, 0.0f, 1.0f));
    const float cameraYOffset = 96.0f * (1.0f - cameraReturnT) * cameraDownT;

    const float sheetWidth = kCameraHudWidth * 0.82f;
    const float sheetHeight = kCameraHudHeight * 0.60f;
    const float sheetTargetX = kCameraHudX + kCameraHudWidth * 0.08f;
    const float sheetTargetY = kCameraHudY + 8.0f;

    if (clearingFilter)
    {
        const bool switched = animationT >= 0.52f;
        const PhotoFilterTheme visibleTheme = switched ? PhotoFilterTheme::None : fromTheme;
        const float sheetRiseT = EaseInOut(std::min(animationT / 0.42f, 1.0f));
        const float sheetExitT = EaseOutCubic(std::clamp((animationT - 0.42f) / 0.42f, 0.0f, 1.0f));
        const float sheetBaseY = sheetTargetY + 36.0f;
        const float sheetTopY = sheetTargetY - 8.0f;
        const float sheetX = sheetTargetX;
        const float sheetY = std::lerp(
            std::lerp(sheetBaseY, sheetTopY, sheetRiseT),
            -sheetHeight - 12.0f,
            sheetExitT);
        const float sheetAlpha = animationT < 0.84f ? 1.0f : 1.0f - std::clamp((animationT - 0.84f) / 0.16f, 0.0f, 1.0f);
        DrawCameraFilterSheet(fromTheme, sheetX, sheetY, sheetWidth, sheetHeight, sheetAlpha);
        drawCamera(visibleTheme, kCameraHudX, kCameraHudY + cameraYOffset, 1.0f, true);
    }
    else
    {
        const bool switched = animationT >= 0.62f;
        const PhotoFilterTheme visibleTheme = switched ? toTheme : fromTheme;
        const float sheetInT = EaseOutCubic(std::min(animationT / 0.34f, 1.0f));
        const float sheetDropT = EaseInOut(std::clamp((animationT - 0.45f) / 0.30f, 0.0f, 1.0f));
        const float sheetX = std::lerp(-sheetWidth - 12.0f, sheetTargetX, sheetInT);
        const float sheetY = sheetTargetY + sheetDropT * 36.0f;
        const float sheetAlpha = animationT < 0.80f ? 1.0f : 1.0f - std::clamp((animationT - 0.80f) / 0.20f, 0.0f, 1.0f);
        DrawCameraFilterSheet(toTheme, sheetX, sheetY, sheetWidth, sheetHeight, sheetAlpha);
        drawCamera(visibleTheme, kCameraHudX, kCameraHudY + cameraYOffset, 1.0f, true);
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::DrawSepiaUnlockOverlay() const
{
    if (!m_ui.sepiaUnlockOverlayOpen)
    {
        return;
    }

    const int bodyTexture = m_assets.GetTexture(kCameraBodyTextureKey);
    const int noFilterTexture = m_assets.GetTexture(kCameraNoFilterTextureKey);
    if (bodyTexture < 0)
    {
        return;
    }

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 146);
    DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    constexpr float kModalCameraWidth = 320.0f;
    constexpr float kModalCameraHeight = 224.0f;
    const float cameraX = static_cast<float>(SCREEN_WIDTH) * 0.5f - kModalCameraWidth * 0.5f;
    const float cameraY = static_cast<float>(SCREEN_HEIGHT) * 0.5f - 170.0f;
    const float sheetWidth = kModalCameraWidth * 0.82f;
    const float sheetHeight = kModalCameraHeight * 0.60f;
    const float sheetTargetX = cameraX + kModalCameraWidth * 0.08f;
    const float sheetTargetY = cameraY + 12.0f;

    const auto drawCamera = [&](PhotoFilterTheme theme, float x, float y, float alpha)
    {
        const bool drawSepiaCamera = NormalizeCameraHudTheme(theme) == PhotoFilterTheme::Sepia && noFilterTexture >= 0;
        const int baseTexture = drawSepiaCamera ? noFilterTexture : bodyTexture;
        if (drawSepiaCamera)
        {
            DrawSepiaHudFilmEffect(x + 22.0f, y + 29.0f, kModalCameraWidth - 44.0f, kModalCameraHeight - 54.0f);
        }
        Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        Shader_SetTint(1.0f, 1.0f, 1.0f, std::clamp(alpha, 0.0f, 1.0f));
        SpriteDraw(baseTexture, x, y, kModalCameraWidth, kModalCameraHeight, 0.0f, 0.0f, 1.0f, 1.0f);
        Shader_ResetStyle();
    };

    constexpr float kLoopSeconds = 2.55f;
    constexpr float kInsertSeconds = 0.86f;
    constexpr float kHoldSeconds = 0.54f;
    constexpr float kClearSeconds = 0.86f;
    const float loopT = std::fmod(std::max(0.0f, m_ui.sepiaUnlockOverlayTimer), kLoopSeconds);

    if (loopT < kInsertSeconds)
    {
        const float animationT = std::clamp(loopT / kInsertSeconds, 0.0f, 1.0f);
        const bool switched = animationT >= 0.62f;
        const PhotoFilterTheme visibleTheme = switched ? PhotoFilterTheme::Sepia : PhotoFilterTheme::None;
        const float cameraDownT = EaseInOut(std::min(animationT / 0.34f, 1.0f));
        const float cameraReturnT = EaseInOut(std::clamp((animationT - 0.34f) / 0.28f, 0.0f, 1.0f));
        const float cameraYOffset = 154.0f * (1.0f - cameraReturnT) * cameraDownT;
        const float sheetInT = EaseOutCubic(std::min(animationT / 0.34f, 1.0f));
        const float sheetDropT = EaseInOut(std::clamp((animationT - 0.45f) / 0.30f, 0.0f, 1.0f));
        const float sheetX = std::lerp(-sheetWidth - 24.0f, sheetTargetX, sheetInT);
        const float sheetY = sheetTargetY + sheetDropT * 58.0f;
        const float sheetAlpha = animationT < 0.80f ? 1.0f : 1.0f - std::clamp((animationT - 0.80f) / 0.20f, 0.0f, 1.0f);
        DrawCameraFilterSheet(PhotoFilterTheme::Sepia, sheetX, sheetY, sheetWidth, sheetHeight, sheetAlpha);
        drawCamera(visibleTheme, cameraX, cameraY + cameraYOffset, 1.0f);
    }
    else if (loopT < kInsertSeconds + kHoldSeconds)
    {
        drawCamera(PhotoFilterTheme::Sepia, cameraX, cameraY, 1.0f);
    }
    else
    {
        const float animationT = std::clamp((loopT - kInsertSeconds - kHoldSeconds) / kClearSeconds, 0.0f, 1.0f);
        const bool switched = animationT >= 0.52f;
        const PhotoFilterTheme visibleTheme = switched ? PhotoFilterTheme::None : PhotoFilterTheme::Sepia;
        const float cameraDownT = EaseInOut(std::min(animationT / 0.34f, 1.0f));
        const float cameraReturnT = EaseInOut(std::clamp((animationT - 0.34f) / 0.28f, 0.0f, 1.0f));
        const float cameraYOffset = 154.0f * (1.0f - cameraReturnT) * cameraDownT;
        const float sheetRiseT = EaseInOut(std::min(animationT / 0.42f, 1.0f));
        const float sheetExitT = EaseOutCubic(std::clamp((animationT - 0.42f) / 0.42f, 0.0f, 1.0f));
        const float sheetY = std::lerp(std::lerp(sheetTargetY + 58.0f, sheetTargetY - 10.0f, sheetRiseT), -sheetHeight - 24.0f, sheetExitT);
        const float sheetAlpha = animationT < 0.84f ? 1.0f : 1.0f - std::clamp((animationT - 0.84f) / 0.16f, 0.0f, 1.0f);
        DrawCameraFilterSheet(PhotoFilterTheme::Sepia, sheetTargetX, sheetY, sheetWidth, sheetHeight, sheetAlpha);
        drawCamera(visibleTheme, cameraX, cameraY + cameraYOffset, 1.0f);
    }

    const auto drawCenteredOutlinedText = [](int left, int top, int right, int bottom, const char* text, int fontSize, unsigned int color, unsigned int outlineColor)
    {
        const int previousFontSize = GetFontSize();
        SetFontSize(std::max(8, fontSize));
        const int textWidth = GetDrawStringWidth(text, -1);
        const int textHeight = GetFontSize();
        const int textX = left + (right - left - textWidth) / 2;
        const int textY = top + (bottom - top - textHeight) / 2 - 1;
        DrawString(textX + 2, textY + 2, text, GetColor(76, 44, 22));
        DrawString(textX - 1, textY, text, outlineColor);
        DrawString(textX + 1, textY, text, outlineColor);
        DrawString(textX, textY - 1, text, outlineColor);
        DrawString(textX, textY + 1, text, outlineColor);
        DrawString(textX, textY, text, color);
        SetFontSize(previousFontSize);
    };

    const int labelW = 470;
    const int labelH = 72;
    const int labelLeft = SCREEN_WIDTH / 2 - labelW / 2;
    const int labelTop = SCREEN_HEIGHT / 2 + 68;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 118);
    DrawBox(labelLeft + 8, labelTop + 8, labelLeft + labelW + 8, labelTop + labelH + 8, GetColor(18, 8, 2), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(labelLeft, labelTop, labelLeft + labelW, labelTop + labelH, GetColor(96, 56, 22), TRUE);
    DrawBox(labelLeft + 5, labelTop + 5, labelLeft + labelW - 5, labelTop + labelH - 5, GetColor(255, 217, 166), TRUE);
    DrawBox(labelLeft + 10, labelTop + 10, labelLeft + labelW - 10, labelTop + labelH / 2, GetColor(255, 236, 198), TRUE);
    DrawBox(labelLeft, labelTop, labelLeft + labelW, labelTop + labelH, GetColor(255, 238, 196), FALSE);
    drawCenteredOutlinedText(labelLeft, labelTop, labelLeft + labelW, labelTop + labelH, "セピアフィルター", 34, GetColor(76, 48, 32), GetColor(255, 238, 198));

    constexpr int kCloseWidth = 260;
    constexpr int kCloseHeight = 74;
    const int closeLeft = SCREEN_WIDTH / 2 - kCloseWidth / 2;
    const int closeTop = SCREEN_HEIGHT / 2 + 200;
    const int closeRight = closeLeft + kCloseWidth;
    const int closeBottom = closeTop + kCloseHeight;
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    const bool hover = mouseX >= closeLeft && mouseX <= closeRight && mouseY >= closeTop && mouseY <= closeBottom;
    const int radius = kCloseHeight / 2;
    const int leftCenterX = closeLeft + radius;
    const int rightCenterX = closeRight - radius;
    const int centerY = closeTop + radius;
    const unsigned int edgeColor = hover ? GetColor(126, 74, 32) : GetColor(92, 56, 28);
    const unsigned int fillColor = hover ? GetColor(255, 220, 166) : GetColor(232, 184, 118);
    const unsigned int topLight = hover ? GetColor(255, 241, 202) : GetColor(250, 213, 156);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
    DrawBox(leftCenterX + 7, closeTop + 8, rightCenterX + 7, closeBottom + 8, GetColor(18, 8, 2), TRUE);
    DrawOval(leftCenterX + 7, centerY + 8, radius, radius, GetColor(18, 8, 2), TRUE);
    DrawOval(rightCenterX + 7, centerY + 8, radius, radius, GetColor(18, 8, 2), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawBox(leftCenterX, closeTop, rightCenterX, closeBottom, edgeColor, TRUE);
    DrawOval(leftCenterX, centerY, radius, radius, edgeColor, TRUE);
    DrawOval(rightCenterX, centerY, radius, radius, edgeColor, TRUE);
    DrawBox(leftCenterX, closeTop + 5, rightCenterX, closeBottom - 5, fillColor, TRUE);
    DrawOval(leftCenterX, centerY, radius - 5, radius - 5, fillColor, TRUE);
    DrawOval(rightCenterX, centerY, radius - 5, radius - 5, fillColor, TRUE);
    DrawBox(leftCenterX - 2, closeTop + 9, rightCenterX + 2, closeTop + 29, topLight, TRUE);
    DrawLine(closeLeft + 30, closeBottom - 10, closeRight - 30, closeBottom - 10, GetColor(154, 92, 42), 2);
    drawCenteredOutlinedText(closeLeft, closeTop, closeRight, closeBottom, "閉じる", 32, GetColor(68, 42, 24), GetColor(255, 235, 198));

    Shader_ResetStyle();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
void GameScene::DrawPlayerHpBar() const
{
    const Entity* player = FindEntityByTag(kTagPlayer);
    if (!player) return;

    const auto* health = player->GetComponent<HealthComponent>();
    if (!health) return;

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
    const float targetRatio = static_cast<float>(currentHp) / static_cast<float>(maxHp);
    const float displayRatio = m_ui.hpUiInitialized ? m_ui.hpDisplayRatio : targetRatio;
    const float lagRatio = m_ui.hpUiInitialized ? m_ui.hpDamageLagRatio : targetRatio;
    const float flash = m_ui.hpDamageFlash;
    const int hpTexture = m_assets.GetTexture(kHpTextureKey);
    const int hpDamageTexture = m_assets.GetTexture(kHpDamageTextureKey);
    if (hpTexture < 0)
    {
        return;
    }

    const auto& hpUi = m_ui.tuning.hp;
    const int activeSlotCount = (std::min)(maxHp, 5);
    const float displayHp = std::clamp(displayRatio * static_cast<float>(maxHp), 0.0f, static_cast<float>(maxHp));
    const float lagHp = std::clamp(lagRatio * static_cast<float>(maxHp), 0.0f, static_cast<float>(maxHp));

    Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
    for (int slotIndex = 0; slotIndex < activeSlotCount; ++slotIndex)
    {
        const UiLayoutRect slot = MakeHpSlotRect(m_ui.tuning, slotIndex);
        const float slotFill = std::clamp(displayHp - static_cast<float>(slotIndex), 0.0f, 1.0f);
        const float slotLag = std::clamp(lagHp - static_cast<float>(slotIndex), 0.0f, 1.0f);
        const bool filled = currentHp > slotIndex;
        const int heartTexture = filled || hpDamageTexture < 0 ? hpTexture : hpDamageTexture;
        const float heartX = slot.x + (slot.width - hpUi.heartSize) * 0.5f;
        const float heartY = slot.y + (slot.height - hpUi.heartSize) * 0.5f + hpUi.heartYOffset;

        Shader_SetTint(0.0f, 0.0f, 0.0f, filled ? 0.24f : 0.18f);
        SpriteDraw(
            heartTexture,
            heartX + hpUi.heartShadowOffsetX,
            heartY + hpUi.heartShadowOffsetY,
            hpUi.heartSize,
            hpUi.heartSize,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        if (filled)
        {
            Shader_SetTint(
                1.0f - 0.03f * (1.0f - slotFill),
                0.42f + 0.14f * slotFill,
                0.45f + 0.12f * slotFill,
                0.96f + 0.04f * slotFill);
        }
        else
        {
            Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        }
        SpriteDraw(
            heartTexture,
            heartX,
            heartY,
            hpUi.heartSize,
            hpUi.heartSize,
            0.0f,
            0.0f,
            1.0f,
            1.0f);

        if (filled)
        {
            Shader_SetBlendMode(ShaderBlendMode2D::Additive);
            Shader_SetTint(1.0f, 0.82f, 0.88f, 0.10f + 0.10f * slotFill);
            SpriteDraw(
                hpTexture,
                heartX - hpUi.heartGlowExpand * 0.5f,
                heartY - hpUi.heartGlowExpand * 0.5f,
                hpUi.heartSize + hpUi.heartGlowExpand,
                hpUi.heartSize + hpUi.heartGlowExpand,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        }

        if (filled && slotLag > slotFill)
        {
            Shader_SetBlendMode(ShaderBlendMode2D::Additive);
            Shader_SetTint(1.0f, 0.42f, 0.48f, 0.12f + 0.30f * (slotLag - slotFill));
            SpriteDraw(
                hpTexture,
                heartX - hpUi.heartLagGlowExpand * 0.5f,
                heartY - hpUi.heartLagGlowExpand * 0.5f,
                hpUi.heartSize + hpUi.heartLagGlowExpand,
                hpUi.heartSize + hpUi.heartLagGlowExpand,
                0.0f,
                0.0f,
                1.0f,
                1.0f);
            Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
        }

    }

    if (flash > 0.0f)
    {
        Shader_SetBlendMode(ShaderBlendMode2D::Additive);
        Shader_SetTint(1.0f, 0.82f, 0.84f, 0.10f + 0.18f * flash);
        for (int slotIndex = 0; slotIndex < activeSlotCount; ++slotIndex)
        {
            const UiLayoutRect slot = MakeHpSlotRect(m_ui.tuning, slotIndex);
            if (currentHp > slotIndex)
            {
                SpriteDraw(
                    hpTexture,
                    slot.x,
                    slot.y,
                    slot.width,
                    slot.height,
                    0.0f,
                    0.0f,
                    1.0f,
                    1.0f);
            }
        }
        Shader_SetBlendMode(ShaderBlendMode2D::Alpha);
    }

    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void GameScene::DrawPartsHud() const
{
    const GameSessionState& session = GameSession_Get();
    const float alpha = std::clamp(m_ui.partsHudAlpha, 0.0f, 1.0f);
    if (alpha <= 0.0f)
    {
        return;
    }

    const auto& partsUi = m_ui.tuning.partsHud;
    const float panelX = static_cast<float>(SCREEN_WIDTH) - partsUi.panelWidth - partsUi.marginRight;
    const float panelY = partsUi.marginTop;
    const int panelTexture = m_assets.GetTexture(kPartsCounterPanelTextureKey);
    const int digitsTexture = m_assets.GetTexture(kPartsCounterDigitsTextureKey);

    Shader_ResetStyle();
    Shader_SetTint(1.0f, 1.0f, 1.0f, alpha);
    if (panelTexture >= 0)
    {
        SpriteDraw(
            panelTexture,
            panelX,
            panelY,
            partsUi.panelWidth,
            partsUi.panelHeight,
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }

    if (digitsTexture >= 0)
    {
        constexpr int kDigitCount = 4;
        constexpr float kDigitSourceWidth = 0.1f;
        constexpr float kDigitStartXRatio = 0.344f;
        constexpr float kDigitStartYRatio = 0.106f;
        constexpr float kDigitStepXRatio = 0.139f;
        constexpr float kDigitWidthRatio = 0.118f;
        constexpr float kDigitHeightRatio = 0.780f;

        const int clampedParts = std::clamp(session.parts, 0, 9999);
        int divisor = 1000;
        for (int index = 0; index < kDigitCount; ++index)
        {
            const int digit = (clampedParts / divisor) % 10;
            divisor /= 10;
            const float digitX = panelX + partsUi.panelWidth * (kDigitStartXRatio + kDigitStepXRatio * static_cast<float>(index));
            const float digitY = panelY + partsUi.panelHeight * kDigitStartYRatio;
            const float digitW = partsUi.panelWidth * kDigitWidthRatio;
            const float digitH = partsUi.panelHeight * kDigitHeightRatio;
            SpriteDraw(
                digitsTexture,
                digitX,
                digitY,
                digitW,
                digitH,
                static_cast<float>(digit) * kDigitSourceWidth,
                0.0f,
                kDigitSourceWidth,
                1.0f);
        }
    }

    Shader_ResetStyle();
}

void GameScene::DrawShieldBossHpBar() const
{
    const Entity* bossEntity = nullptr;
    bool keepVisibleForDeathMotion = false;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated() ||
            enemy->GetArchetype() != EnemyArchetype::ShieldBoss)
        {
            continue;
        }

        const auto* boss = entity->GetComponent<ShieldBossComponent>();
        if (!boss || !boss->combatStarted || !boss->appearAnimationFinished ||
            boss->introDropActive || boss->appearAnimationActive ||
            boss->roarAnimationActive || boss->deathAnimationFinished)
        {
            continue;
        }

        bossEntity = entity.get();
        keepVisibleForDeathMotion = boss->deathAnimationActive;
        break;
    }

    if (!bossEntity)
    {
        return;
    }

    const auto* health = bossEntity->GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
    if (currentHp <= 0 && !keepVisibleForDeathMotion)
    {
        return;
    }

    UpdateBossHpUiState(
        m_ui.bossHp,
        currentHp,
        maxHp,
        std::max(0.0f, m_flow.lastDeltaTime),
        keepVisibleForDeathMotion);
    DrawEnderInspiredBossHpGauge(m_ui.tuning.bossHp, m_ui.bossHp);
}

void GameScene::DrawMidBoss2HpBar() const
{
    const Entity* bossEntity = nullptr;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated())
        {
            continue;
        }
        const auto* boss = entity->GetComponent<MidBoss2Component>();
        if (!boss || boss->state == MidBoss2State::Dead)
        {
            continue;
        }
        bossEntity = entity.get();
        break;
    }

    if (!bossEntity)
    {
        return;
    }

    const auto* health = bossEntity->GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
    if (currentHp <= 0)
    {
        return;
    }

    UpdateBossHpUiState(m_ui.bossHp, currentHp, maxHp, std::max(0.0f, m_flow.lastDeltaTime));
    DrawEnderInspiredBossHpGauge(m_ui.tuning.bossHp, m_ui.bossHp);
    return;

    const auto& bossUi = m_ui.tuning.bossHp;
    const float panelWidth = bossUi.panelWidth + bossUi.panelPadding * 2.0f;
    const float panelHeight = bossUi.barHeight + bossUi.panelExtraHeight;
    const float panelX = static_cast<float>(SCREEN_WIDTH) * 0.5f - panelWidth * 0.5f;
    const float panelY = bossUi.marginTop;
    const float barX = panelX + bossUi.panelPadding;
    const float barY = panelY + bossUi.panelPadding;
    const float ratio = std::clamp(static_cast<float>(currentHp) / static_cast<float>(maxHp), 0.0f, 1.0f);
    const auto* boss = bossEntity->GetComponent<MidBoss2Component>();
    const bool beamPressureState = boss &&
        (boss->state == MidBoss2State::BeamCharge || boss->state == MidBoss2State::BeamFire);
    const bool spearPressureState = boss &&
        (boss->state == MidBoss2State::SpearJump || boss->state == MidBoss2State::SpearThrow);
    const char* phaseLabel = boss ? game_scene_combat_system::ToMidBoss2StateLabel(boss->state) : "不明";
    if (boss)
    {
        switch (boss->state)
        {
        case MidBoss2State::Idle:
            phaseLabel = "待機";
            break;
        case MidBoss2State::SpearJump:
            phaseLabel = "ワープ";
            break;
        case MidBoss2State::SpearThrow:
            phaseLabel = "攻撃";
            break;
        case MidBoss2State::SpearLanding:
            phaseLabel = "着地";
            break;
        case MidBoss2State::SpearCooldown:
            phaseLabel = "再配置";
            break;
        case MidBoss2State::BeamCharge:
            phaseLabel = "チャージ";
            break;
        case MidBoss2State::BeamFire:
            phaseLabel = "ビーム発射";
            break;
        case MidBoss2State::BeamCooldown:
            phaseLabel = "再配置";
            break;
        case MidBoss2State::Damaged:
            phaseLabel = "硬直";
            break;
        default:
            break;
        }
    }
    const float phasePulse = beamPressureState
        ? 0.72f + 0.28f * std::sin(static_cast<float>(GetNowCount()) * 0.020f)
        : (spearPressureState ? 0.82f + 0.18f * std::sin(static_cast<float>(GetNowCount()) * 0.014f) : 1.0f);
    const unsigned int phaseColor = beamPressureState
        ? GetColor(255, 172, 84)
        : (spearPressureState ? GetColor(122, 224, 255) : GetColor(208, 224, 240));

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(14, 18, 28),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(190, 224, 244),
        FALSE);

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(32, 40, 54),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth * ratio)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(74, 170, 248),
        TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth * ratio)),
        static_cast<int>(std::round(barY + bossUi.barHeight * 0.45f)),
        GetColor(255, 255, 255),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    for (int i = 1; i < maxHp; ++i)
    {
        const float x = barX + bossUi.panelWidth * (static_cast<float>(i) / static_cast<float>(maxHp));
        DrawLine(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + 2.0f)),
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + bossUi.barHeight - 2.0f)),
            GetColor(54, 68, 82));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(214, 238, 250),
        FALSE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(beamPressureState ? 180.0f * phasePulse : 110.0f)));
    DrawLine(
        static_cast<int>(std::round(panelX + 1.0f)),
        static_cast<int>(std::round(panelY + 28.0f)),
        static_cast<int>(std::round(panelX + panelWidth - 1.0f)),
        static_cast<int>(std::round(panelY + 28.0f)),
        phaseColor);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY - 18.0f)),
        "BOSS2",
        GetColor(220, 236, 248));
    DrawFormatString(
        static_cast<int>(std::round(barX + 176.0f)),
        static_cast<int>(std::round(barY - 18.0f)),
        phaseColor,
        "STATE %s",
        phaseLabel);
    if (beamPressureState)
    {
        SetDrawBlendMode(DX_BLENDMODE_ADD, static_cast<int>(std::round(120.0f + 100.0f * phasePulse)));
        DrawString(
            static_cast<int>(std::round(barX + bossUi.panelWidth - 86.0f)),
            static_cast<int>(std::round(barY - 18.0f)),
            "ALERT",
            GetColor(255, 124, 76));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
    DrawFormatString(
        static_cast<int>(std::round(barX + bossUi.panelWidth * 0.5f) - 34.0f),
        static_cast<int>(std::round(barY + bossUi.hpTextOffsetY)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);
}
void GameScene::DrawMidBoss3HpBar() const
{
    const Entity* bossEntity = nullptr;
    bool keepVisibleForDeathMotion = false;
    for (const auto& entity : m_world.Entities())
    {
        if (!entity)
        {
            continue;
        }

        const auto* enemy = entity->GetComponent<EnemyComponent>();
        if (!enemy || !enemy->IsEnabled() || enemy->IsDefeated())
        {
            continue;
        }
        const auto* boss = entity->GetComponent<MidBoss3Component>();
        if (!boss || !boss->introFinished || boss->deathAnimationFinished)
        {
            continue;
        }
        bossEntity = entity.get();
        keepVisibleForDeathMotion = boss->deathAnimationActive;
        break;
    }

    if (!bossEntity)
    {
        return;
    }

    const auto* health = bossEntity->GetComponent<HealthComponent>();
    if (!health)
    {
        return;
    }

    const int maxHp = (std::max)(1, health->GetMaxHealth());
    const int currentHp = std::clamp(health->GetCurrentHealth(), 0, maxHp);
    if (currentHp <= 0 && !keepVisibleForDeathMotion)
    {
        return;
    }

    UpdateBossHpUiState(
        m_ui.bossHp,
        currentHp,
        maxHp,
        std::max(0.0f, m_flow.lastDeltaTime),
        keepVisibleForDeathMotion);
    DrawEnderInspiredBossHpGauge(m_ui.tuning.bossHp, m_ui.bossHp);
    return;

    const auto& bossUi = m_ui.tuning.bossHp;
    const float panelWidth = bossUi.panelWidth + bossUi.panelPadding * 2.0f;
    const float panelHeight = bossUi.barHeight + bossUi.panelExtraHeight;
    const float panelX = static_cast<float>(SCREEN_WIDTH) * 0.5f - panelWidth * 0.5f;
    const float panelY = bossUi.marginTop;
    const float barX = panelX + bossUi.panelPadding;
    const float barY = panelY + bossUi.panelPadding;
    const float ratio = std::clamp(static_cast<float>(currentHp) / static_cast<float>(maxHp), 0.0f, 1.0f);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 204);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(28, 12, 14),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(panelX)),
        static_cast<int>(std::round(panelY)),
        static_cast<int>(std::round(panelX + panelWidth)),
        static_cast<int>(std::round(panelY + panelHeight)),
        GetColor(232, 196, 196),
        FALSE);

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(48, 26, 30),
        TRUE);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth * ratio)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(218, 42, 48),
        TRUE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 84);
    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth * ratio)),
        static_cast<int>(std::round(barY + bossUi.barHeight * 0.45f)),
        GetColor(255, 232, 232),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

    for (int i = 1; i < maxHp; ++i)
    {
        const float x = barX + bossUi.panelWidth * (static_cast<float>(i) / static_cast<float>(maxHp));
        DrawLine(
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + 2.0f)),
            static_cast<int>(std::round(x)),
            static_cast<int>(std::round(barY + bossUi.barHeight - 2.0f)),
            GetColor(72, 30, 34));
    }

    DrawBox(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY)),
        static_cast<int>(std::round(barX + bossUi.panelWidth)),
        static_cast<int>(std::round(barY + bossUi.barHeight)),
        GetColor(246, 220, 220),
        FALSE);
    DrawString(
        static_cast<int>(std::round(barX)),
        static_cast<int>(std::round(barY + bossUi.titleOffsetY)),
        "BOSS",
        GetColor(248, 196, 196));
    DrawFormatString(
        static_cast<int>(std::round(barX + bossUi.panelWidth * 0.5f) - 34.0f),
        static_cast<int>(std::round(barY + bossUi.hpTextOffsetY)),
        GetColor(255, 255, 255),
        "HP %d / %d",
        currentHp,
        maxHp);
}

void GameScene::DrawAttackCaptureSlot() const
{
    if (!m_photo.attackCapture.hasPhoto || !m_photo.attackCapture.containsEnemyAttackPaste)
    {
        return;
    }

    const auto& attackUi = m_ui.tuning.attackCapture;
    const float centerX = attackUi.panelX + attackUi.panelSize * 0.5f;
    const float centerY = attackUi.panelY + attackUi.panelSize * 0.5f + attackUi.titleY;
    const CapturedSpawnArchetype archetype = GetPrimaryAttackCaptureArchetype(m_photo.attackCapture);
    const unsigned int iconColor = GetAttackCaptureIconColor(archetype);

    // Attack captures use a replaceable icon slot; the colored circle is a temporary asset stand-in.
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 210);
    DrawBox(
        static_cast<int>(std::round(attackUi.panelX)),
        static_cast<int>(std::round(attackUi.panelY)),
        static_cast<int>(std::round(attackUi.panelX + attackUi.panelSize)),
        static_cast<int>(std::round(attackUi.panelY + attackUi.panelSize)),
        GetColor(12, 18, 26),
        TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawBox(
        static_cast<int>(std::round(attackUi.panelX)),
        static_cast<int>(std::round(attackUi.panelY)),
        static_cast<int>(std::round(attackUi.panelX + attackUi.panelSize)),
        static_cast<int>(std::round(attackUi.panelY + attackUi.panelSize)),
        GetColor(224, 232, 242),
        FALSE);

    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 80);
    DrawCircleAA(centerX, centerY, attackUi.iconRadius + 14.0f, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ADD, 128);
    DrawCircleAA(centerX, centerY, attackUi.iconRadius + 6.0f, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 245);
    DrawCircleAA(centerX, centerY, attackUi.iconRadius, 64, iconColor, TRUE);
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    DrawCircleAA(centerX, centerY, attackUi.iconRadius, 64, GetColor(255, 246, 226), FALSE, 2.0f);

    DrawString(
        static_cast<int>(std::round(attackUi.panelX + attackUi.titleX)),
        static_cast<int>(std::round(attackUi.panelY + attackUi.titleY)),
        "ATK",
        GetColor(240, 226, 196));

    const int attackCount = GetAttackCaptureCount(m_photo.attackCapture);
    if (attackCount > 0)
    {
        DrawFormatString(
            static_cast<int>(std::round(attackUi.panelX + attackUi.panelSize - attackUi.countRightOffset)),
            static_cast<int>(std::round(attackUi.panelY + attackUi.panelSize - attackUi.countBottomOffset)),
            GetColor(30, 36, 44),
            "x %d",
            attackCount);
    }
}



