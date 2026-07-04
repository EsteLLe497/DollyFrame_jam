// =========================================================
// ファイルの情報[forest_fog.cpp]
//
// 制作者:Masatora Tanaka		日付：2026/07/04
// =========================================================
#include "pch.h"

#include "forest_fog.h"

#include <algorithm>

#include "directX.h"
#include "DxLib.h"

// =========================================================
// グローバル変数
// =========================================================
namespace
{
    constexpr const char* kForestFogShaderPath = "assets/shaders/forest_fog_ps.cso";
    constexpr int kFogBufferWidth = kVirtualScreenWidth / 2;
    constexpr int kFogBufferHeight = kVirtualScreenHeight / 2;

    int g_forestFogPixelShader = -1;		// ボリューム霧ピクセルシェーダー
    int g_forestFogConstantBuffer = -1;	// ボリューム霧定数バッファ
    int g_forestFogRenderTarget = -1;	// 負荷軽減用の半解像度描画先

    // HLSLのcbufferと同じ16バイト境界を維持する。
    struct ForestFogShaderParams
    {
        float viewWidth;
        float viewHeight;
        float timeSeconds;
        float density;

        float cameraOffsetX;
        float cameraOffsetY;
        float noiseScale;
        float driftSpeed;

        float fogColorR;
        float fogColorG;
        float fogColorB;
        float opacity;

        float verticalStart;
        float verticalEnd;
        float edgeSoftness;
        float rayLength;

        float lightPositionX;
        float lightPositionY;
        float lightIntensity;
        float rayDecay;

        float lightColorR;
        float lightColorG;
        float lightColorB;
        float rayContrast;

        float coverage;
        float variation;
        float padding0;
        float padding1;
    };

    // =========================================================
    // 半解像度の霧描画用矩形を作成
    // =========================================================
    void drawFogQuad()
    {
        VERTEX2DSHADER vertices[4] = {};
        const COLOR_U8 white = GetColorU8(255, 255, 255, 255);
        const float width = static_cast<float>(kFogBufferWidth);
        const float height = static_cast<float>(kFogBufferHeight);

        vertices[0].pos = VGet(0.0f, 0.0f, 0.0f);
        vertices[0].rhw = 1.0f;
        vertices[0].dif = white;
        vertices[0].spc = white;
        vertices[0].u = 0.0f;
        vertices[0].v = 0.0f;

        vertices[1].pos = VGet(width, 0.0f, 0.0f);
        vertices[1].rhw = 1.0f;
        vertices[1].dif = white;
        vertices[1].spc = white;
        vertices[1].u = 1.0f;
        vertices[1].v = 0.0f;

        vertices[2].pos = VGet(0.0f, height, 0.0f);
        vertices[2].rhw = 1.0f;
        vertices[2].dif = white;
        vertices[2].spc = white;
        vertices[2].u = 0.0f;
        vertices[2].v = 1.0f;

        vertices[3].pos = VGet(width, height, 0.0f);
        vertices[3].rhw = 1.0f;
        vertices[3].dif = white;
        vertices[3].spc = white;
        vertices[3].u = 1.0f;
        vertices[3].v = 1.0f;

        DrawPrimitive2DToShader(vertices, 4, DX_PRIMTYPE_TRIANGLESTRIP);
    }

    // =========================================================
    // 定数バッファへ霧パラメータを書き込む
    // =========================================================
    bool updateShaderParams(const ForestFogParams& params)
    {
        auto* buffer =
            static_cast<ForestFogShaderParams*>(GetBufferShaderConstantBuffer(g_forestFogConstantBuffer));
        if (!buffer)
        {
            return false;
        }

        buffer->viewWidth = params.viewWidth;
        buffer->viewHeight = params.viewHeight;
        buffer->timeSeconds = params.timeSeconds;
        buffer->density = std::clamp(params.density, 0.0f, 1.0f);
        buffer->cameraOffsetX = params.cameraX * params.parallax;
        buffer->cameraOffsetY = params.cameraY * params.parallax;
        buffer->noiseScale = std::max(0.01f, params.noiseScale);
        buffer->driftSpeed = params.driftSpeed;
        buffer->fogColorR = std::clamp(params.fogColorR, 0.0f, 1.0f);
        buffer->fogColorG = std::clamp(params.fogColorG, 0.0f, 1.0f);
        buffer->fogColorB = std::clamp(params.fogColorB, 0.0f, 1.0f);
        buffer->opacity = std::clamp(params.opacity, 0.0f, 1.0f);
        buffer->verticalStart = std::clamp(params.verticalStart, 0.0f, 1.0f);
        buffer->verticalEnd = std::clamp(params.verticalEnd, buffer->verticalStart, 1.0f);
        buffer->edgeSoftness = std::max(0.001f, params.edgeSoftness);
        buffer->rayLength = std::clamp(params.rayLength, 0.0f, 1.5f);
        buffer->lightPositionX = params.lightPositionX;
        buffer->lightPositionY = params.lightPositionY;
        buffer->lightIntensity = std::max(0.0f, params.lightIntensity);
        buffer->rayDecay = std::clamp(params.rayDecay, 0.0f, 1.0f);
        buffer->lightColorR = std::clamp(params.lightColorR, 0.0f, 1.0f);
        buffer->lightColorG = std::clamp(params.lightColorG, 0.0f, 1.0f);
        buffer->lightColorB = std::clamp(params.lightColorB, 0.0f, 1.0f);
        buffer->rayContrast = std::clamp(params.rayContrast, 0.25f, 4.0f);
        buffer->coverage = std::clamp(params.coverage, 0.0f, 1.0f);
        buffer->variation = std::clamp(params.variation, 0.0f, 1.0f);
        buffer->padding0 = 0.0f;
        buffer->padding1 = 0.0f;

        UpdateShaderConstantBuffer(g_forestFogConstantBuffer);
        return true;
    }
}

// =========================================================
// ボリューム霧シェーダー初期化
// =========================================================
bool forestFog::initialize()
{
    finalize();

    g_forestFogPixelShader = LoadPixelShader(kForestFogShaderPath);
    if (g_forestFogPixelShader < 0)
    {
        return false;
    }

    g_forestFogConstantBuffer = CreateShaderConstantBuffer(sizeof(ForestFogShaderParams));
    if (g_forestFogConstantBuffer < 0)
    {
        finalize();
        return false;
    }

    // 霧だけを保持して合成できるよう、透過対応の半解像度画面を一度だけ確保する。
    const int previousAlphaFlag = GetDrawValidAlphaChannelGraphCreateFlag();
    SetDrawValidAlphaChannelGraphCreateFlag(TRUE);
    g_forestFogRenderTarget = MakeScreen(kFogBufferWidth, kFogBufferHeight, TRUE);
    SetDrawValidAlphaChannelGraphCreateFlag(previousAlphaFlag);
    if (g_forestFogRenderTarget < 0)
    {
        finalize();
        return false;
    }

    return true;
}

// =========================================================
// ボリューム霧シェーダー終了
// =========================================================
void forestFog::finalize()
{
    if (g_forestFogRenderTarget >= 0)
    {
        DeleteGraph(g_forestFogRenderTarget);
        g_forestFogRenderTarget = -1;
    }

    if (g_forestFogConstantBuffer >= 0)
    {
        DeleteShaderConstantBuffer(g_forestFogConstantBuffer);
        g_forestFogConstantBuffer = -1;
    }

    if (g_forestFogPixelShader >= 0)
    {
        DeleteShader(g_forestFogPixelShader);
        g_forestFogPixelShader = -1;
    }
}

// =========================================================
// ボリューム霧シェーダー使用可能判定
// =========================================================
bool forestFog::isReady()
{
    return
        g_forestFogPixelShader >= 0 &&
        g_forestFogConstantBuffer >= 0 &&
        g_forestFogRenderTarget >= 0;
}

// =========================================================
// ボリューム霧描画
// =========================================================
void forestFog::draw(const ForestFogParams& params)
{
    if (!params.enabled ||
        !isReady() ||
        params.viewWidth <= 0.0f ||
        params.viewHeight <= 0.0f ||
        !updateShaderParams(params))
    {
        return;
    }

    const int previousDrawScreen = GetDrawScreen();
    const int previousDrawMode = GetDrawMode();
    int previousBlendMode = DX_BLENDMODE_NOBLEND;
    int previousBlendParam = 0;
    GetDrawBlendMode(&previousBlendMode, &previousBlendParam);

    // レイ積分は半解像度で行い、GPU負荷と一時VRAMを約4分の1へ抑える。
    SetDrawScreen(g_forestFogRenderTarget);
    ClearDrawScreen();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    SetShaderConstantBuffer(g_forestFogConstantBuffer, DX_SHADERTYPE_PIXEL, 0);
    SetUseTextureToShader(0, -1);
    SetUseVertexShader(-1);
    SetUsePixelShader(g_forestFogPixelShader);
    drawFogQuad();

    SetUsePixelShader(-1);
    SetUseTextureToShader(0, -1);
    SetDrawScreen(previousDrawScreen);

    // バイリニア拡大により、低解像度化で生じる段差も同時に滑らかにする。
    SetDrawMode(DX_DRAWMODE_BILINEAR);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    DrawExtendGraphF(
        params.viewX,
        params.viewY,
        params.viewX + params.viewWidth,
        params.viewY + params.viewHeight,
        g_forestFogRenderTarget,
        TRUE);

    SetDrawMode(previousDrawMode);
    SetDrawBlendMode(previousBlendMode, previousBlendParam);
}
