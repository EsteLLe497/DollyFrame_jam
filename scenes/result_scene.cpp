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
#include "DxLib.h"
#include "photo_log.h"
#include "game_scene_render_ui_helpers.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <tracy/Tracy.hpp>

namespace
{
    struct PrimaryOption
    {
        std::string label;
        std::string mapCsvPath;
    };

    // 最後に遊んだステージと終了理由から、リザルトの主選択肢を決める。
    // クリア時は次のステージ（森→廃墟→地下）、ゲームオーバー時は同じステージへの再挑戦。
    PrimaryOption BuildPrimaryOption(const GameSessionState& session)
    {
        const bool cleared = session.endReason == GameEndReason::GoalReached ||
            session.endReason == GameEndReason::BossDefeated;
        const std::string& lastMap = session.lastMapCsvPath.empty()
            ? session.startMapCsvPath
            : session.lastMapCsvPath;

        if (!cleared)
        {
            return { "同じステージに再挑戦", lastMap };
        }

        if (lastMap.find("forest") != std::string::npos)
        {
            return { "廃墟ステージへ進む", "assets/maps/stages/ruins_v2.csv" };
        }
        if (lastMap.find("ruins") != std::string::npos)
        {
            return { "地下ステージへ進む", "assets/maps/stages/under.csv" };
        }
        if (lastMap.find("under") != std::string::npos)
        {
            return { "最初のステージから遊びなおす", "assets/maps/stages/forest.csv" };
        }
        return { "もう一度遊ぶ", lastMap };
    }

    //const char* ToReasonLabel(GameEndReason reason)
    //{
    //    switch (reason)
    //    {
    //    case GameEndReason::GoalReached:
    //        return "クリア";
    //    case GameEndReason::TimeUp:
    //        return "時間切れ";
    //    case GameEndReason::HpZero:
    //        return "HP0";
    //    case GameEndReason::BossDefeated:
    //        return "ボス撃破";
    //    case GameEndReason::None:
    //    default:
    //        return "結果なし";
    //    }
    //}

    // 最後に遊んだステージ名から見出しに使う日本語ステージ名を決める。
    const char* GetStageDisplayName(const std::string& mapPath)
    {
        if (mapPath.find("forest") != std::string::npos) return "森林ステージ";
        if (mapPath.find("ruins") != std::string::npos) return "廃墟ステージ";
        if (mapPath.find("under") != std::string::npos) return "地下ステージ";
        return "ステージ";
    }

    struct FreeImagePlacement
    {
        const char* textureKey;
        float x;
        float y;
        float width;
        float height;
    };


    void DrawOutlinedString(int x, int y, const char* text, int textColor, int outlineColor)
    {
        DrawString(x - 2, y, text, outlineColor);
        DrawString(x + 2, y, text, outlineColor);
        DrawString(x, y - 2, text, outlineColor);
        DrawString(x, y + 2, text, outlineColor);
        DrawString(x - 1, y - 1, text, outlineColor);
        DrawString(x + 1, y - 1, text, outlineColor);
        DrawString(x - 1, y + 1, text, outlineColor);
        DrawString(x + 1, y + 1, text, outlineColor);
        DrawString(x, y, text, textColor);
    }

    void DrawCenteredOutlinedString(int centerX, int y, const char* text, int textColor, int outlineColor)
    {
        const int width = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawOutlinedString(centerX - width / 2, y, text, textColor, outlineColor);
    }

    void DrawMenuRow(int left, int top, int width, int height, const char* label, bool selected)
    {
        const int right = left + width;
        const int bottom = top + height;
        const int fillColor = selected ? GetColor(131, 92, 51) : GetColor(66, 44, 22);
        const int lineColor = selected ? GetColor(241, 205, 132) : GetColor(188, 148, 82);
        const int textColor = selected ? GetColor(255, 246, 230) : GetColor(236, 216, 182);

        DrawBox(left, top, right, bottom, fillColor, TRUE);
        DrawBox(left, top, right, bottom, lineColor, FALSE);
        if (selected)
        {
            DrawCircle(left + 18, top + height / 2, 5, GetColor(241, 205, 132), TRUE);
            DrawCircle(left + 18, top + height / 2, 2, GetColor(52, 30, 14), TRUE);
        }

        const int textWidth = GetDrawStringWidth(label, static_cast<int>(std::strlen(label)));
        const int textX = left + (width - textWidth) / 2 + 12;
        const int textY = top + (height - 16) / 2;
        DrawOutlinedString(textX, textY, label, textColor, GetColor(28, 16, 9));
    }

    constexpr int kMenuOptionCount = 2;
    constexpr const char* kBackToTitleLabel = "タイトルへ戻る";
    constexpr int kMenuRowWidth = 360;
    constexpr int kMenuRowHeight = 44;
    constexpr int kMenuRowGap = 14;
    constexpr int kMenuRowTop = 520;
    constexpr float kResultIntroDuration = 0.55f;
    constexpr int kResultFilmHeight = 132;
    constexpr int kResultFilmRailHeight = 28;
    constexpr int kResultFilmHoleWidth = 20;
    constexpr int kResultFilmHoleHeight = 16;
    constexpr int kResultFilmHoleGap = 34;
    constexpr int kResultFilmFrameWidth = 270;
    constexpr int kResultTitleFontSize = 76;  // 見出し（クリア/オーバー）の文字サイズ（大きく）
    constexpr int kResultTitleLineY0 = 380;   // 見出し1行目（英語）のY座標
    constexpr int kResultTitleLineY1 = 468;   // 見出し2行目（日本語）のY座標
    constexpr int kResultDetailLineY = 346;   // 「クリア」「時間切れ」等の詳細行のY座標

    float EaseOutCubic(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        const float invT = 1.0f - t;
        return 1.0f - invT * invT * invT;
    }

    // 撮影写真グリッド（右下3x3）のレイアウト。DrawCapturedPhotosGrid と
    // DrawPhotoRevealAnimation の両方から参照するのでファイルスコープに置く。
    constexpr int kPhotoGridColumns = 3;
    constexpr int kPhotoGridRows = 3;
    constexpr float kPhotoGridCellWidth = 180.0f;
    constexpr float kPhotoGridCellHeight = 120.0f;
    constexpr float kPhotoGridCellGapX = 12.0f;
    constexpr float kPhotoGridCellGapY = 12.0f;
    constexpr float kPhotoGridMarginRight = 40.0f + 112.0f;
    constexpr float kPhotoGridMarginBottom = 40.0f + 145.0f;
    constexpr float kPhotoGridPadding = 6.0f;

    struct PhotoCellRect
    {
        float x;
        float y;
        float width;
        float height;
    };

    PhotoCellRect ComputePhotoGridBounds()
    {
        const float gridWidth =
            static_cast<float>(kPhotoGridColumns) * kPhotoGridCellWidth +
            static_cast<float>(kPhotoGridColumns - 1) * kPhotoGridCellGapX;
        const float gridHeight =
            static_cast<float>(kPhotoGridRows) * kPhotoGridCellHeight +
            static_cast<float>(kPhotoGridRows - 1) * kPhotoGridCellGapY;
        const float gridStartX = static_cast<float>(SCREEN_WIDTH) - kPhotoGridMarginRight - gridWidth;
        const float gridStartY = static_cast<float>(SCREEN_HEIGHT) - kPhotoGridMarginBottom - gridHeight;
        return PhotoCellRect{ gridStartX, gridStartY, gridWidth, gridHeight };
    }

    PhotoCellRect ComputePhotoGridCellRect(int index)
    {
        const PhotoCellRect bounds = ComputePhotoGridBounds();
        const int column = index % kPhotoGridColumns;
        const int row = index / kPhotoGridColumns;
        return PhotoCellRect{
            bounds.x + static_cast<float>(column) * (kPhotoGridCellWidth + kPhotoGridCellGapX),
            bounds.y + static_cast<float>(row) * (kPhotoGridCellHeight + kPhotoGridCellGapY),
            kPhotoGridCellWidth,
            kPhotoGridCellHeight
        };
    }

    // 写真の演出タイミング。ここの数値だけ変えれば速さ・見え方を調整できます。
    // スライドインが終わった直後（kResultIntroDuration経過後）から1枚目の演出が始まります。
    constexpr float kPhotoRevealStagger = 0.28f;
    //constexpr float kPhotoRevealStartDelay = kResultIntroDuration;
    //constexpr float kPhotoRevealStagger = 0.28f;

    constexpr float kPhotoRevealPopDuration = 0.18f;
    constexpr float kPhotoRevealHoldDuration = 0.55f;
    constexpr float kPhotoRevealFlyDuration = 0.60f;
    constexpr float kPhotoRevealCenterWidth = 420.0f;
    constexpr float kPhotoRevealCenterHeight = 300.0f;
    constexpr float kPhotoRevealArcHeight = 160.0f;
    constexpr float kPhotoRevealScatterRangeX = 90.0f;  // 横方向のズレ幅（大きいほど散らばる）
    constexpr float kPhotoRevealScatterRangeY = 60.0f;  // 縦方向のズレ幅
   
    constexpr float kResultStatsRevealGap = 0.25f;     // 写真演出が終わってからの間
    constexpr float kResultStatsStagger = 0.35f;       // 時間→アイテム→ボタンの表示間隔
    constexpr float kResultStatsFadeDuration = 0.45f;  // フェードインにかかる時間
    constexpr float kResultStatsRiseDistance = 26.0f;  // フェードインと同時に下から上へ動く距離(px)
    constexpr int kResultStatsFontSize = 40;           // 時間・アイテム数の文字サイズ（大きく）
    constexpr int kResultItemLineY = 368;
    constexpr int kResultTimeLineY = 300;

    constexpr int kResultTitleCanvasWidth = 940;           // ← 760 から変更
    constexpr int kResultTitleCanvasHeight = 250;          // ← 200 から変更
    constexpr int kResultTitleLocalLineY0 = 55;            // ← 40 から変更
    constexpr int kResultTitleLocalLineY1 = 148;           // ← 108 から変更
    constexpr int kResultTitleCanvasPivotY = 101;          // ← 74 から変更
    constexpr float kResultTitleWorldPivotOffsetY = 27.0f; // ← 20.0f から変更
    constexpr float kResultTitleSpinDuration = 0.6f;      // 回転しながら現れる時間
    constexpr float kResultTitleSpinTurns = 1.0f;         // 現れるまでに何回転するか
    constexpr float kResultTitleSpinStartScale = 0.35f;   // 回転開始時の縮小率

    constexpr float kResultTitleHoldDuration = 2.5f;    // 中央での表示時間（回転が終わってからこの秒数だけ静止）
    constexpr float kResultTitleMoveDuration = 0.55f;   // 移動にかかる時間
    constexpr float kResultTitleMoveScale = 0.72f;      // 移動先での縮小率
    constexpr float kResultTitleMoveTopMargin = 150.0f;  // 写真グリッドとの間の余白

    constexpr float kVignetteInnerRadius = 230.0f;  // ← 260 から変更（少し狭く＝より強く見える）
    constexpr float kVignetteOuterRadius = 500.0f;  // ← 560 から変更
    constexpr int kVignetteMaxAlpha = 195;          // ← 130 から変更（もっと濃く）

    float GetPhotoRevealStartDelay()
    {
        return kResultIntroDuration + kResultTitleSpinDuration + kResultTitleHoldDuration + kResultTitleMoveDuration;
    }

    // 見出し（STAGE CLEAR等）を回転描画するための、使い回しオフスクリーンを取得する。
    int GetResultTitleRenderTarget(int width, int height)
    {
        struct RenderTargetState
        {
            int handle = -1;
            int width = 0;
            int height = 0;
        };

        static RenderTargetState s_state;
        if (width <= 0 || height <= 0)
        {
            return -1;
        }

        if (s_state.handle >= 0 && (s_state.width != width || s_state.height != height))
        {
            DeleteGraph(s_state.handle);
            s_state.handle = -1;
            s_state.width = 0;
            s_state.height = 0;
        }

        if (s_state.handle < 0)
        {
            s_state.handle = MakeScreen(width, height, TRUE);
            s_state.width = width;
            s_state.height = height;
        }

        return s_state.handle;
    }

    struct ResultRevealTimeline
    {
        float photoComplete;
        float timeStat;
        float itemStat;
        float buttons;
    };

    // 撮影写真の演出がすべて終わるタイミングを計算する。
    float ComputePhotoRevealCompleteTime()
    {
        const int photoCount = PhotoLog_GetCount();
        const float startDelay = GetPhotoRevealStartDelay();
        if (photoCount <= 0)
        {
            return startDelay;
        }
        return startDelay
            + static_cast<float>(photoCount - 1) * kPhotoRevealStagger
            + kPhotoRevealHoldDuration
            + kPhotoRevealFlyDuration;
    }

    ResultRevealTimeline ComputeResultRevealTimeline()
    {
        ResultRevealTimeline timeline;
        timeline.photoComplete = ComputePhotoRevealCompleteTime();
        timeline.timeStat = timeline.photoComplete + kResultStatsRevealGap;
        timeline.itemStat = timeline.timeStat + kResultStatsStagger;
        timeline.buttons = timeline.itemStat + kResultStatsStagger;
        return timeline;
    }

    // revealAt を基準に 0.0〜1.0 のフェードイン進行度を返す。
    float ComputeFadeInProgress(float revealAt, float elapsedSeconds, float fadeDuration)
    {
        return std::clamp((elapsedSeconds - revealAt) / std::max(0.001f, fadeDuration), 0.0f, 1.0f);
    }
}

// index から毎回同じ値を返す簡易な疑似乱数（0.0〜1.0）。
float PseudoRandom01(int seed)
{
    unsigned int x = static_cast<unsigned int>(seed) * 2654435761u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return static_cast<float>(x % 10000u) / 10000.0f;
}

ResultScene::ResultScene()
    : m_whiteTexture(-1)
    , m_blinkTimer(0.0f)
    , m_introStartTimeMs(0)// ← ここが m_introTimer(0.0f) から変
    , m_showPrompt(true)
    , m_selectedOption(0)
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
    m_introStartTimeMs = GetNowCount();   // ← ここが m_introTimer = 0.0f; から変更
    m_showPrompt = true;
    m_selectedOption = 0;
    PrimaryOption primaryOption = BuildPrimaryOption(GameSession_Get());
    m_primaryOptionLabel = std::move(primaryOption.label);
    m_primaryOptionMapCsv = std::move(primaryOption.mapCsvPath);
    Logger::Info("ResultScene entered");
}

ResultScene::MenuOptionRect ResultScene::GetOptionRect(int index) const
{
    const int left = SCREEN_WIDTH / 2 - kMenuRowWidth / 2;
    const int top = kMenuRowTop + index * (kMenuRowHeight + kMenuRowGap);
    return { left, top, left + kMenuRowWidth, top + kMenuRowHeight };
}

void ResultScene::UpdateMenuInput()
{
    // キーボード / パッド操作
    if (Input_IsActionPressed(InputAction::MoveUp) || Input_IsDpadUpPressed())
    {
        m_selectedOption = (m_selectedOption + kMenuOptionCount - 1) % kMenuOptionCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }
    if (Input_IsActionPressed(InputAction::MoveDown) || Input_IsDpadDownPressed())
    {
        m_selectedOption = (m_selectedOption + 1) % kMenuOptionCount;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    // マウス操作: ホバーで選択、クリックで決定
    const int mouseX = Input_GetMouseX();
    const int mouseY = Input_GetMouseY();
    int hoveredOption = -1;
    for (int index = 0; index < kMenuOptionCount; ++index)
    {
        const MenuOptionRect rect = GetOptionRect(index);
        if (mouseX >= rect.left && mouseX <= rect.right && mouseY >= rect.top && mouseY <= rect.bottom)
        {
            hoveredOption = index;
            break;
        }
    }

    if (hoveredOption >= 0 && hoveredOption != m_selectedOption)
    {
        m_selectedOption = hoveredOption;
        m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_move", 0.0f, 0.0f });
    }

    const bool confirmPressed =
        Input_IsActionPressed(InputAction::Confirm) ||
        Input_IsActionPressed(InputAction::StartGame) ||
        Input_IsSouthButtonPressed();
    const bool mouseClickConfirm = hoveredOption >= 0 && Input_IsMouseLeftPressed();

    if (confirmPressed || mouseClickConfirm)
    {
        ConfirmSelection();
    }
}

void ResultScene::ConfirmSelection()
{
    m_eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, "ui_select", 0.0f, 0.0f });

    if (m_selectedOption == 0)
    {
        GameSession_SetStartMapCsvPath(m_primaryOptionMapCsv);
        GameSession_SetLoadSavedProgress(false);
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "game", 0.0f, 0.0f });
    }
    else
    {
        // タイトルへ戻る
        m_eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, "title", 0.0f, 0.0f });
    }
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

    if (GetIntroProgress() < 1.0f)
    {
        return;
    }

    // ボタンが完全にふわっと表示され終わるまでは操作を受け付けない。
    const float elapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;
    const ResultRevealTimeline timeline = ComputeResultRevealTimeline();
    if (elapsedSeconds < timeline.buttons + kResultStatsFadeDuration)
    {
        return;
    }

    UpdateMenuInput();
}

float ResultScene::GetIntroProgress() const
{
    // Update() が呼ばれない間（アプリ側のシーン遷移フェード中）も Draw() 側で進行させたいので、
    // deltaTime の積算ではなく GetNowCount()（ミリ秒の壁時計）からの経過時間で計算する。
    const float elapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;
    return std::clamp(elapsedSeconds / kResultIntroDuration, 0.0f, 1.0f);
}

float ResultScene::GetIntroOffsetX() const
{
    return (1.0f - EaseOutCubic(GetIntroProgress())) * static_cast<float>(SCREEN_WIDTH);  // ← 中身をGetIntroProgress()呼び出しに変更
}

void ResultScene::Draw()
{
    const float offsetX = GetIntroOffsetX();
    if (offsetX > 0.5f)
    {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, GetColor(0, 0, 0), TRUE);
    }

    DrawBackdrop(offsetX);
    DrawFreeImages(offsetX);
    DrawCapturedPhotosGrid(offsetX);
    DrawPhotoRevealAnimation(offsetX);
    DrawMenu(offsetX);
    DrawResultFilmFrame();
}

//float ResultScene::GetIntroOffsetX() const
//{
//    const float progress = std::clamp(m_introTimer / kResultIntroDuration, 0.0f, 1.0f);
//    return (1.0f - EaseOutCubic(progress)) * static_cast<float>(SCREEN_WIDTH);
//}

void ResultScene::DrawMenu(float offsetX) const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;
    const int drawOffsetX = static_cast<int>(std::round(offsetX));
    const int previousFontSize = GetFontSize();

    // 大見出し（ステージ名入り、回転しながら現れたあと右上へ移動する）
    {
        char line0[32] = {};
        char line1[64] = {};
        if (cleared)
        {
            const std::string& lastMap = session.lastMapCsvPath.empty()
                ? session.startMapCsvPath
                : session.lastMapCsvPath;
            const char* stageName = GetStageDisplayName(lastMap);
            std::snprintf(line0, sizeof(line0), "STAGE CLEAR");
            std::snprintf(line1, sizeof(line1), "%sクリア", stageName);
        }
        else
        {
            std::snprintf(line0, sizeof(line0), "GAME OVER");
            std::snprintf(line1, sizeof(line1), "ゲームオーバー");
        }

        const int titleRenderTarget = GetResultTitleRenderTarget(kResultTitleCanvasWidth, kResultTitleCanvasHeight);
        if (titleRenderTarget >= 0)
        {
            const int previousDrawScreen = GetDrawScreen();
            SetDrawScreen(titleRenderTarget);
            ClearDrawScreen();

            SetFontSize(kResultTitleFontSize);
            DrawCenteredOutlinedString(kResultTitleCanvasWidth / 2, kResultTitleLocalLineY0, line0, GetColor(255, 244, 220), GetColor(52, 30, 14));
            DrawCenteredOutlinedString(kResultTitleCanvasWidth / 2, kResultTitleLocalLineY1, line1, GetColor(255, 226, 164), GetColor(28, 16, 9));
            SetFontSize(previousFontSize);

            SetDrawScreen(previousDrawScreen);

            const float titleElapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;

            // 回転の動作は今まで通り。
            const float titleSpinT = ComputeFadeInProgress(kResultIntroDuration, titleElapsedSeconds, kResultTitleSpinDuration);
            const float titleSpinEased = EaseOutCubic(titleSpinT);

            const double titleAngle = static_cast<double>((1.0f - titleSpinEased) * kResultTitleSpinTurns * 6.2831853072f);
            const float baseScale = std::lerp(kResultTitleSpinStartScale, 1.0f, titleSpinEased);

            // 回転が終わったあと、写真グリッドの上あたりへ移動＆縮小する。
// 回転が終わったあと、中央で少し静止してから、写真グリッドの上あたりへ移動＆縮小する。
            const float titleSpinEndTime = kResultIntroDuration + kResultTitleSpinDuration;
            const float titleMoveStartTime = titleSpinEndTime + kResultTitleHoldDuration;
            const float titleMoveT = ComputeFadeInProgress(titleMoveStartTime, titleElapsedSeconds, kResultTitleMoveDuration);
            const float titleMoveEased = EaseOutCubic(titleMoveT);

            const float titleWorldCenterX = static_cast<float>(SCREEN_WIDTH) / 2.0f + offsetX;
            const float titleWorldCenterY = static_cast<float>(kResultTitleLineY0 + kResultTitleLineY1) / 2.0f + kResultTitleWorldPivotOffsetY;

            const PhotoCellRect gridBounds = ComputePhotoGridBounds();
            const float titleTargetCenterX = gridBounds.x + gridBounds.width * 0.5f + offsetX;
            const float titleTargetCenterY = gridBounds.y - kResultTitleMoveTopMargin;

            const float currentCenterX = std::lerp(titleWorldCenterX, titleTargetCenterX, titleMoveEased);
            const float currentCenterY = std::lerp(titleWorldCenterY, titleTargetCenterY, titleMoveEased);
            const double currentScale = static_cast<double>(std::lerp(baseScale, kResultTitleMoveScale, titleMoveEased));

            // ビネット：CLEARが中央にいる間だけ、周囲を暗くする。移動し始めたらフェードアウト。
            const float vignetteAlphaFactor = std::clamp(titleSpinEased, 0.0f, 1.0f) * (1.0f - titleMoveEased);
            if (vignetteAlphaFactor > 0.001f)
            {
                DrawResultVignette(
                    titleWorldCenterX,
                    titleWorldCenterY,
                    static_cast<int>(std::round(static_cast<float>(kVignetteMaxAlpha) * vignetteAlphaFactor)));
            }

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
            DrawRotaGraph3F(
                currentCenterX,
                currentCenterY,
                static_cast<float>(kResultTitleCanvasWidth) / 2.0f,
                static_cast<float>(kResultTitleCanvasPivotY),
                currentScale,
                currentScale,
                titleAngle,
                titleRenderTarget,
                TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
    }

    char detail[64] = {};
    //std::snprintf(detail, sizeof(detail), "%s", ToReasonLabel(session.endReason));
    DrawCenteredOutlinedString(SCREEN_WIDTH / 2 + drawOffsetX, kResultDetailLineY, detail, GetColor(242, 226, 194), GetColor(28, 16, 9));

    // 写真の演出が終わったあと、時間 → アイテム数 → ボタンの順にふわっと表示する。
    const float elapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;
    const ResultRevealTimeline timeline = ComputeResultRevealTimeline();
    const float timeStatT = ComputeFadeInProgress(timeline.timeStat, elapsedSeconds, kResultStatsFadeDuration);
    const float itemStatT = ComputeFadeInProgress(timeline.itemStat, elapsedSeconds, kResultStatsFadeDuration);
    const float buttonsT = ComputeFadeInProgress(timeline.buttons, elapsedSeconds, kResultStatsFadeDuration);

    // クリアタイム（上）
    if (timeStatT > 0.0f)
    {
        char timeLine[64] = {};
        const int clearMinutes = static_cast<int>(session.clearTimeSeconds) / 60;
        const int clearSeconds = static_cast<int>(session.clearTimeSeconds) % 60;
        std::snprintf(timeLine, sizeof(timeLine), "クリアタイム  %02d:%02d", clearMinutes, clearSeconds);

        SetFontSize(kResultStatsFontSize);
        const int riseOffset = static_cast<int>(std::round((1.0f - timeStatT) * kResultStatsRiseDistance));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(255.0f * timeStatT)));
        DrawCenteredOutlinedString(
            SCREEN_WIDTH / 2 + drawOffsetX,
            kResultTimeLineY + riseOffset,
            timeLine,
            GetColor(255, 236, 196),
            GetColor(28, 16, 9));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        SetFontSize(previousFontSize);
    }

    // 取得アイテム数（下）
    if (itemStatT > 0.0f)
    {
        char itemLine[64] = {};
        std::snprintf(itemLine, sizeof(itemLine), "取得アイテム  %d個", session.partsCollectedTotal);

        SetFontSize(kResultStatsFontSize);
        const int riseOffset = static_cast<int>(std::round((1.0f - itemStatT) * kResultStatsRiseDistance));
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(255.0f * itemStatT)));
        DrawCenteredOutlinedString(
            SCREEN_WIDTH / 2 + drawOffsetX,
            kResultItemLineY + riseOffset,
            itemLine,
            GetColor(255, 236, 196),
            GetColor(28, 16, 9));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        SetFontSize(previousFontSize);
    }

    // 選択肢（写真の演出が終わったあとにふわっと表示）
    if (buttonsT > 0.0f)
    {
        const int riseOffset = static_cast<int>(std::round((1.0f - buttonsT) * kResultStatsRiseDistance));
        const int alpha = static_cast<int>(std::round(255.0f * buttonsT));
        const char* menuLabels[kMenuOptionCount] = { m_primaryOptionLabel.c_str(), kBackToTitleLabel };

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, alpha);
        for (int index = 0; index < kMenuOptionCount; ++index)
        {
            const MenuOptionRect rect = GetOptionRect(index);
            DrawMenuRow(rect.left + drawOffsetX, rect.top + riseOffset, 
                kMenuRowWidth, kMenuRowHeight, menuLabels[index], m_selectedOption == index);
        }

        const int hintColor = m_showPrompt ? GetColor(252, 238, 214) : GetColor(168, 140, 104);
        DrawCenteredOutlinedString(
            SCREEN_WIDTH / 2 + drawOffsetX,
            kMenuRowTop + kMenuOptionCount * (kMenuRowHeight + kMenuRowGap) + 30 + riseOffset,
            "上下キー・マウス: 選択   Enter/Space/A/クリック: 決定",
            hintColor,
            GetColor(28, 16, 9));
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }
}

void ResultScene::DrawResultFilmFrame() const
{
    const float progress = GetIntroProgress();   // ← ここが std::clamp(m_introTimer / kResultIntroDuration, 0.0f, 1.0f) から変更
    const float eased = EaseOutCubic(progress);
    const int screenW = SCREEN_WIDTH;
    const int screenH = SCREEN_HEIGHT;
    const int filmLength = screenW + 520;
    const float startLeft = static_cast<float>(screenW + 160);
    const float stoppedLeft = -220.0f;
    const int filmLeft = static_cast<int>(std::round(startLeft + (stoppedLeft - startLeft) * eased));
    const int topFilmY = 36;
    const int bottomFilmY = screenH - 36 - kResultFilmHeight;
    const int filmColor = GetColor(12, 12, 12);
    const int frameColor = GetColor(248, 248, 248);
    const int dividerColor = GetColor(10, 10, 10);
    const int holeColor = GetColor(248, 248, 248);

    const auto drawFilmStrip = [&](int y, int phaseOffset)
        {
            const int left = filmLeft + phaseOffset;
            const int right = left + filmLength;
            if (right < -120 || left > screenW + 120)
            {
                return;
            }

            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 248);
            DrawBox(left, y, right, y + kResultFilmHeight, filmColor, TRUE);

            const int holePhase = static_cast<int>(std::round((1.0f - eased) * 260.0f)) % kResultFilmHoleGap;
            const int firstHoleX = left + 18 + holePhase;
            for (int x = firstHoleX - kResultFilmHoleGap * 2; x < right + 
                kResultFilmHoleGap; x += kResultFilmHoleGap)
            {
                DrawBox(x, y + 6, x + kResultFilmHoleWidth, y + 6 + 
                    kResultFilmHoleHeight, holeColor, TRUE);
                DrawBox(x, y + kResultFilmHeight - 6 - 
                    kResultFilmHoleHeight, x + kResultFilmHoleWidth, y + 
                    kResultFilmHeight - 6, holeColor, TRUE);
            }

            const int frameTop = y + kResultFilmRailHeight + 4;
            const int frameBottom = y + kResultFilmHeight - kResultFilmRailHeight - 4;
            for (int frameX = left; frameX < right; frameX += kResultFilmFrameWidth)
            {
                DrawBox(frameX + 8, frameTop + 6, frameX + 
                    kResultFilmFrameWidth - 8, frameBottom - 6, frameColor, TRUE);
                DrawBox(frameX + kResultFilmFrameWidth - 3, frameTop, frameX + 
                    kResultFilmFrameWidth + 3, frameBottom, dividerColor, TRUE);
            }

            DrawBox(left, y + kResultFilmRailHeight, right, y + 
                kResultFilmRailHeight + 4, dividerColor, TRUE);
            DrawBox(left, y + kResultFilmHeight - 
                kResultFilmRailHeight - 4, right, y + kResultFilmHeight - 
                kResultFilmRailHeight, dividerColor, TRUE);
        };

    drawFilmStrip(topFilmY, 0);
    drawFilmStrip(bottomFilmY, 180);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void ResultScene::DrawDebugUI()
{
    const GameSessionState& session = GameSession_Get();
    ImGui::Begin("リザルト");
    ImGui::Text("試作版のリザルト画面です");
    ImGui::Text("karihaikei texture id: %d", m_assets.GetTexture("karihaikei"));
    //ImGui::Text("Result: %s", ToReasonLabel(session.endReason));
    ImGui::Text("HP: %d / %d", session.currentHp, session.maxHp);
    ImGui::Text("残り時間: %.1f / %.1f", session.timeRemaining, session.timeLimit);
    ImGui::Text("選択中: %s", m_selectedOption == 0 ? m_primaryOptionLabel.c_str() : kBackToTitleLabel);
    ImGui::Text("主選択肢の遷移先: %s", m_primaryOptionMapCsv.c_str());
    ImGui::Text("上下キー/マウスホバーで選択、Enter/Space/A/クリックで決定");
    ImGui::Text("プロンプト表示: %s", m_showPrompt ? "あり" : "なし");
    ImGui::Text("取得アイテム(累計): %d", session.partsCollectedTotal);
    ImGui::Text("クリアタイム: %.1f 秒", session.clearTimeSeconds);
    ImGui::End();
}

EventBus* ResultScene::GetEventBus()
{
    return &m_eventBus;
}

void ResultScene::DrawBackdrop(float offsetX) const
{
    const GameSessionState& session = GameSession_Get();
    const bool cleared = session.endReason == GameEndReason::GoalReached ||
        session.endReason == GameEndReason::BossDefeated;

    // ここに追加: 一番奥の背景画像
    const int backgroundTexture = m_assets.GetTexture("karihaikei");
    if (backgroundTexture >= 0)
    {
        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        SpriteDraw(
            backgroundTexture,
            offsetX,
            0.0f,
            static_cast<float>(SCREEN_WIDTH),
            static_cast<float>(SCREEN_HEIGHT),
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }

    const float accentR = cleared ? 0.18f : 0.78f;
    const float accentG = cleared ? 0.62f : 0.24f;
    const float accentB = cleared ? 0.32f : 0.14f;
    //Shader_SetTint(0.11f, 0.07f, 0.10f, 1.0f);
    //SpriteDraw(m_whiteTexture, 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), 
    // static_cast<float>(SCREEN_HEIGHT), 0.0f, 0.0f, 1.0f, 1.0f);
    //if (m_showPrompt)
    //{
    //    Shader_SetTint(0.88f, 0.88f, 0.88f, 1.0f);
    //    SpriteDraw(m_whiteTexture, 192.0f + offsetX, 430.0f, static_cast<float>
    // (SCREEN_WIDTH) - 384.0f, 34.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    //}
    Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
}

void ResultScene::DrawFreeImages(float offsetX) const
{
    struct FreeImagePlacement
    {
        const char* textureKey;
        float x;
        float y;
        float width;
        float height;
    };

    constexpr float kDateImageHeight = 1400.0f; // ここだけ変えれば全体のサイズが変わる
    constexpr float kDateImageWidth = kDateImageHeight * (2160.0f / 3840.0f);

    const FreeImagePlacement placements[] =
    {
        { "date", 40.0f, -250.0f, kDateImageWidth, kDateImageHeight },
        //{ "kuria", static_cast<float>(SCREEN_WIDTH) - 300.0f, 450.0f, 220.0f, 160.0f },
    };

    for (const auto& placement : placements)
    {
        const int textureId = m_assets.GetTexture(placement.textureKey);
        if (textureId < 0)
        {
            continue; // テクスチャが見つからない場合は描画しない
        }

        Shader_SetTint(1.0f, 1.0f, 1.0f, 1.0f);
        SpriteDraw(
            textureId,
            placement.x + offsetX,
            placement.y,
            placement.width,
            placement.height,
            0.0f,
            0.0f,
            1.0f,
            1.0f);
    }
}
void ResultScene::DrawCapturedPhotosGrid(float offsetX) const
{
    const int photoCount = PhotoLog_GetCount();
    const float elapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;
    const float landedAt = GetPhotoRevealStartDelay() + kPhotoRevealHoldDuration + kPhotoRevealFlyDuration;

    for (int index = 0; index < kPhotoGridColumns * kPhotoGridRows; ++index)
    {
        const PhotoCellRect cell = ComputePhotoGridCellRect(index);
        const float cellX = cell.x + offsetX;
        const float cellY = cell.y;
        DrawBox(
            static_cast<int>(cellX),
            static_cast<int>(cellY),
            static_cast<int>(cellX + cell.width),
            static_cast<int>(cellY + cell.height),
            GetColor(60, 50, 40),
            TRUE);
        DrawBox(
            static_cast<int>(cellX),
            static_cast<int>(cellY),
            static_cast<int>(cellX + cell.width),
            static_cast<int>(cellY + cell.height),
            GetColor(200, 180, 140),
            FALSE);

        if (index >= photoCount)
        {
            continue; // 未撮影スロットは枠だけ表示
        }

        // まだ中央でのポップイン〜飛翔演出中の写真は DrawPhotoRevealAnimation 側で描画する。
        // ここでは着地済みのものだけ静的に描画する。
        const float thisLandedAt = landedAt + static_cast<float>(index) * kPhotoRevealStagger;
        if (elapsedSeconds < thisLandedAt)
        {
            continue;
        }

        const PhotoCaptureState& capture = PhotoLog_GetEntry(index);
        if (!capture.hasPhoto || capture.items.empty())
        {
            continue;
        }

        const float innerX = cellX + kPhotoGridPadding;
        const float innerY = cellY + kPhotoGridPadding;
        const float innerWidth = cell.width - kPhotoGridPadding * 2.0f;
        const float innerHeight = cell.height - kPhotoGridPadding * 2.0f;
        const float scale = std::min(
            innerWidth / std::max(1.0f, capture.width),
            innerHeight / std::max(1.0f, capture.height));
        const float contentX = innerX + (innerWidth - capture.width * scale) * 0.5f;
        const float contentY = innerY + (innerHeight - capture.height * scale) * 0.5f;

        for (const auto& item : capture.items)
        {
            game_scene_detail::DrawCapturedPreviewItem(
                m_whiteTexture,
                item,
                contentX + item.relativeX * scale,
                contentY + item.relativeY * scale,
                item.width * scale,
                item.height * scale,
                1.0f);
        }
    }
}

void ResultScene::DrawPhotoRevealAnimation(float offsetX) const
{
    const int photoCount = PhotoLog_GetCount();
    if (photoCount <= 0)
    {
        return;
    }

    const float elapsedSeconds = static_cast<float>(GetNowCount() - m_introStartTimeMs) * 0.001f;
    const float baseCenterX = static_cast<float>(SCREEN_WIDTH) * 0.5f + offsetX;
    const float baseCenterY = static_cast<float>(SCREEN_HEIGHT) * 0.46f;
    const float landedAt = kPhotoRevealHoldDuration + kPhotoRevealFlyDuration;

    for (int index = 0; index < photoCount; ++index)
    {
        const float localT = elapsedSeconds - GetPhotoRevealStartDelay() 
            - static_cast<float>(index) * kPhotoRevealStagger;
        if (localT < 0.0f || localT >= landedAt)
        {
            continue;
        }

        const PhotoCaptureState& capture = PhotoLog_GetEntry(index);
        if (!capture.hasPhoto || capture.items.empty())
        {
            continue;
        }

        const float scatterX = (PseudoRandom01(index * 2 + 11) - 0.5f) * kPhotoRevealScatterRangeX;
        const float scatterY = (PseudoRandom01(index * 2 + 37) - 0.5f) * kPhotoRevealScatterRangeY;
        const float centerX = baseCenterX + scatterX;
        const float centerY = baseCenterY + scatterY;

        const PhotoCellRect targetCell = ComputePhotoGridCellRect(index);
        const float targetCenterX = targetCell.x + targetCell.width * 0.5f + offsetX;
        const float targetCenterY = targetCell.y + targetCell.height * 0.5f;

        float boxCenterX = centerX;
        float boxCenterY = centerY;
        float boxWidth = kPhotoRevealCenterWidth;
        float boxHeight = kPhotoRevealCenterHeight;
        float alpha = 1.0f;

        if (localT < kPhotoRevealPopDuration)
        {
            const float popT = std::clamp(localT / kPhotoRevealPopDuration, 0.0f, 1.0f);
            const float eased = popT * popT * (3.0f - 2.0f * popT);
            boxWidth = kPhotoRevealCenterWidth * eased;
            boxHeight = kPhotoRevealCenterHeight * eased;
            alpha = eased;
        }
        else if (localT < kPhotoRevealHoldDuration)
        {
        }
        else
        {
            const float flyT = std::clamp((localT - kPhotoRevealHoldDuration) / kPhotoRevealFlyDuration, 0.0f, 1.0f);
            const float eased = flyT * flyT * (3.0f - 2.0f * flyT);
            const float invT = 1.0f - eased;

            const float controlX = (centerX + targetCenterX) * 0.5f;
            const float controlY = std::min(centerY, targetCenterY) - kPhotoRevealArcHeight;

            boxCenterX = invT * invT * centerX + 2.0f * invT * eased * controlX + eased * eased * targetCenterX;
            boxCenterY = invT * invT * centerY + 2.0f * invT * eased * controlY + eased * eased * targetCenterY;
            boxWidth = std::lerp(kPhotoRevealCenterWidth, targetCell.width, eased);
            boxHeight = std::lerp(kPhotoRevealCenterHeight, targetCell.height, eased);
        }

        const float drawX = boxCenterX - boxWidth * 0.5f;
        const float drawY = boxCenterY - boxHeight * 0.5f;

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, static_cast<int>(std::round(255.0f * alpha)));
        DrawBox(
            static_cast<int>(std::round(drawX)),
            static_cast<int>(std::round(drawY)),
            static_cast<int>(std::round(drawX + boxWidth)),
            static_cast<int>(std::round(drawY + boxHeight)),
            GetColor(60, 50, 40),
            TRUE);
        DrawBox(
            static_cast<int>(std::round(drawX)),
            static_cast<int>(std::round(drawY)),
            static_cast<int>(std::round(drawX + boxWidth)),
            static_cast<int>(std::round(drawY + boxHeight)),
            GetColor(230, 210, 170),
            FALSE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);

        const float innerPadding = kPhotoGridPadding * (boxWidth / targetCell.width);
        const float innerX = drawX + innerPadding;
        const float innerY = drawY + innerPadding;
        const float innerWidth = boxWidth - innerPadding * 2.0f;
        const float innerHeight = boxHeight - innerPadding * 2.0f;
        const float scale = std::min(
            innerWidth / std::max(1.0f, capture.width),
            innerHeight / std::max(1.0f, capture.height));
        const float contentX = innerX + (innerWidth - capture.width * scale) * 0.5f;
        const float contentY = innerY + (innerHeight - capture.height * scale) * 0.5f;

        for (const auto& item : capture.items)
        {
            game_scene_detail::DrawCapturedPreviewItem(
                m_whiteTexture,
                item,
                contentX + item.relativeX * scale,
                contentY + item.relativeY * scale,
                item.width * scale,
                item.height * scale,
                alpha);
        }
    }
}

void ResultScene::DrawResultVignette(float centerX, float centerY, int alpha) const
{
    if (alpha <= 0)
    {
        return;
    }

    const float left = 0.0f;
    const float top = 0.0f;
    const float right = static_cast<float>(SCREEN_WIDTH);
    const float bottom = static_cast<float>(SCREEN_HEIGHT);
    const float outerRadius = kVignetteOuterRadius;
    const float innerRadius = kVignetteInnerRadius;
    const float outerRadiusSq = outerRadius * outerRadius;
    const float innerRadiusSq = innerRadius * innerRadius;
    constexpr int kStripeHeight = 4;
    constexpr int kSoftBandSegments = 14;
    const int color = GetColor(0, 0, 0);

    const auto drawRect = [&](float x0, float y0, float x1, float y1, int rectAlpha)
        {
            const int rectLeft = static_cast<int>(std::floor(std::min(x0, x1)));
            const int rectTop = static_cast<int>(std::floor(std::min(y0, y1)));
            const int rectRight = static_cast<int>(std::ceil(std::max(x0, x1)));
            const int rectBottom = static_cast<int>(std::ceil(std::max(y0, y1)));
            if (rectRight <= rectLeft || rectBottom <= rectTop || rectAlpha <= 0)
            {
                return;
            }
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, std::clamp(rectAlpha, 0, 255));
            DrawBox(rectLeft, rectTop, rectRight, rectBottom, color, TRUE);
        };

    drawRect(left, top, right, centerY - outerRadius, alpha);
    drawRect(left, centerY + outerRadius, right, bottom, alpha);

    const int bandStartY = std::max(static_cast<int>(top), static_cast<int>(std::floor(centerY - outerRadius)));
    const int bandEndY = std::min(static_cast<int>(bottom), static_cast<int>(std::ceil(centerY + outerRadius)));
    for (int bandTop = bandStartY; bandTop < bandEndY; bandTop += kStripeHeight)
    {
        const int bandBottom = std::min(bandEndY, bandTop + kStripeHeight);
        const float bandCenterY = (static_cast<float>(bandTop) + static_cast<float>(bandBottom)) * 0.5f;
        const float dy = std::fabs(bandCenterY - centerY);
        if (dy >= outerRadius)
        {
            drawRect(left, static_cast<float>(bandTop), right, static_cast<float>(bandBottom), alpha);
            continue;
        }

        const float outerDx = std::sqrt(std::max(0.0f, outerRadiusSq - dy * dy));
        const float innerDx = dy < innerRadius ? std::sqrt(std::max(0.0f, innerRadiusSq - dy * dy)) : 0.0f;

        drawRect(left, static_cast<float>(bandTop), centerX - outerDx, static_cast<float>(bandBottom), alpha);
        drawRect(centerX + outerDx, static_cast<float>(bandTop), right, static_cast<float>(bandBottom), alpha);

        const float softWidth = std::max(0.0f, outerDx - innerDx);
        if (softWidth <= 0.5f)
        {
            continue;
        }

        for (int segmentIndex = 0; segmentIndex < kSoftBandSegments; ++segmentIndex)
        {
            const float t0 = static_cast<float>(segmentIndex) / static_cast<float>(kSoftBandSegments);
            const float t1 = static_cast<float>(segmentIndex + 1) / static_cast<float>(kSoftBandSegments);
            const float dx0 = innerDx + softWidth * t0;
            const float dx1 = innerDx + softWidth * t1;
            const float dxMid = (dx0 + dx1) * 0.5f;
            const float radiusAtMid = std::sqrt(dxMid * dxMid + dy * dy);
            const float normalized = std::clamp((radiusAtMid - innerRadius) / (outerRadius - innerRadius), 0.0f, 1.0f);
            const float eased = normalized * normalized * (3.0f - 2.0f * normalized);
            const int bandAlpha = static_cast<int>(std::round(eased * static_cast<float>(alpha)));
            if (bandAlpha <= 0)
            {
                continue;
            }
            drawRect(centerX - dx1, static_cast<float>(bandTop), centerX - dx0, static_cast<float>(bandBottom), bandAlpha);
            drawRect(centerX + dx0, static_cast<float>(bandTop), centerX + dx1, static_cast<float>(bandBottom), bandAlpha);
        }
    }

    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
}
