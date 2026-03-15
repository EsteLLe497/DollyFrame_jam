#include "sprite.h"

#include <algorithm>
#include <cmath>

#include "DxLib.h"
#include "shader.h"
#include "texture.h"

namespace
{
    void DrawSpriteChunk(
        int graphHandle,
        int srcX,
        int srcY,
        int srcWidth,
        int srcHeight,
        float destX,
        float destY,
        float destWidth,
        float destHeight,
        float rot)
    {
        if (srcWidth <= 0 || srcHeight <= 0 || destWidth <= 0.0f || destHeight <= 0.0f)
        {
            return;
        }

        const double scaleX = static_cast<double>(destWidth) / static_cast<double>(srcWidth);
        const double scaleY = static_cast<double>(destHeight) / static_cast<double>(srcHeight);

        DrawRectRotaGraph3F(
            destX + destWidth * 0.5f,
            destY + destHeight * 0.5f,
            srcX,
            srcY,
            srcWidth,
            srcHeight,
            static_cast<float>(srcWidth) * 0.5f,
            static_cast<float>(srcHeight) * 0.5f,
            scaleX,
            scaleY,
            static_cast<double>(rot),
            graphHandle,
            TRUE);
    }

    void BuildSourceRect(
        int textureWidth,
        int textureHeight,
        float tx,
        float ty,
        float tw,
        float th,
        int& srcX,
        int& srcY,
        int& srcWidth,
        int& srcHeight)
    {
        srcX = std::clamp(static_cast<int>(tx * static_cast<float>(textureWidth)), 0, textureWidth - 1);
        srcY = std::clamp(static_cast<int>(ty * static_cast<float>(textureHeight)), 0, textureHeight - 1);
        srcWidth = (std::max)(1, (std::min)(textureWidth - srcX, static_cast<int>(tw * static_cast<float>(textureWidth))));
        srcHeight = (std::max)(1, (std::min)(textureHeight - srcY, static_cast<int>(th * static_cast<float>(textureHeight))));
    }
}

void SpriteInitialize(void)
{
}

void SpriteFinalize(void)
{
}

void SpriteDraw(int textureID, float x, float y, float width, float height, float tx, float ty, float tw, float th, float rot)
{
    const int graphHandle = TextureGetGraphHandle(textureID);
    if (graphHandle < 0)
    {
        return;
    }

    const int textureWidth = TextureGetWidth(textureID);
    const int textureHeight = TextureGetHeight(textureID);
    if (textureWidth <= 0 || textureHeight <= 0)
    {
        return;
    }

    float scrollU = 0.0f;
    float scrollV = 0.0f;
    Shader_GetUVScroll(scrollU, scrollV);

    int srcX = 0;
    int srcY = 0;
    int srcWidth = 0;
    int srcHeight = 0;
    BuildSourceRect(textureWidth, textureHeight, tx + scrollU, ty + scrollV, tw, th, srcX, srcY, srcWidth, srcHeight);

    int tintR = 255;
    int tintG = 255;
    int tintB = 255;
    int tintA = 255;
    Shader_GetTintBytes(tintR, tintG, tintB, tintA);

    const ShaderEffect2D effect = Shader_GetCurrentEffect();
    const int blendMode = Shader_GetCurrentBlendMode() == ShaderBlendMode2D::Additive ? DX_BLENDMODE_ADD : DX_BLENDMODE_ALPHA;
    SetDrawBlendMode(blendMode, tintA);
    SetDrawBright(tintR, tintG, tintB);

    if (effect == ShaderEffect2D::Distortion && std::fabs(rot) < 0.001f)
    {
        float strengthU = 0.0f;
        float strengthV = 0.0f;
        float time = 0.0f;
        float tintStrength = 0.0f;
        Shader_GetDistortion(strengthU, strengthV, time, tintStrength);

        constexpr int kStripCount = 6;
        const float stripHeight = height / static_cast<float>(kStripCount);
        const int stripSrcHeight = (std::max)(1, srcHeight / kStripCount);
        for (int i = 0; i < kStripCount; ++i)
        {
            const float phase = time * 3.2f + static_cast<float>(i) * 0.75f;
            const float offsetX = std::sinf(phase) * strengthU * width * 7.0f;
            const float offsetY = std::cosf(phase * 0.8f) * strengthV * height * 4.0f;
            const int currentSrcY = (std::min)(textureHeight - 1, srcY + stripSrcHeight * i);
            const int currentSrcHeight = i == kStripCount - 1 ? (srcY + srcHeight - currentSrcY) : stripSrcHeight;

            DrawSpriteChunk(
                graphHandle,
                srcX,
                currentSrcY,
                srcWidth,
                currentSrcHeight,
                x + offsetX,
                y + stripHeight * static_cast<float>(i) + offsetY,
                width,
                i == kStripCount - 1 ? (height - stripHeight * static_cast<float>(i)) : stripHeight,
                0.0f);
        }

        if (tintStrength > 0.0f)
        {
            const int overlayAlpha = std::clamp(static_cast<int>(tintStrength * 80.0f), 0, 255);
            SetDrawBlendMode(DX_BLENDMODE_ADD, overlayAlpha);
            DrawSpriteChunk(graphHandle, srcX, srcY, srcWidth, srcHeight, x, y, width, height, 0.0f);
        }
    }
    else if (effect == ShaderEffect2D::Parallax && std::fabs(rot) < 0.001f)
    {
        float backSpeed = 0.0f;
        float frontSpeed = 0.0f;
        float mixRatio = 0.0f;
        float time = 0.0f;
        Shader_GetParallax(backSpeed, frontSpeed, mixRatio, time);

        const float backOffset = std::fmod(backSpeed * time, 1.0f);
        const float frontOffset = std::fmod(frontSpeed * time, 1.0f);
        const int baseAlpha = tintA;
        const int backAlpha = std::clamp(static_cast<int>(baseAlpha * (0.45f + (1.0f - mixRatio) * 0.25f)), 0, 255);
        const int frontAlpha = std::clamp(static_cast<int>(baseAlpha * (0.55f + mixRatio * 0.35f)), 0, 255);

        int backSrcX = 0;
        int backSrcY = 0;
        int backSrcWidth = 0;
        int backSrcHeight = 0;
        BuildSourceRect(textureWidth, textureHeight, tx + backOffset, ty, tw, th, backSrcX, backSrcY, backSrcWidth, backSrcHeight);

        int frontSrcX = 0;
        int frontSrcY = 0;
        int frontSrcWidth = 0;
        int frontSrcHeight = 0;
        BuildSourceRect(textureWidth, textureHeight, tx + frontOffset, ty, tw, th, frontSrcX, frontSrcY, frontSrcWidth, frontSrcHeight);

        SetDrawBlendMode(blendMode, backAlpha);
        DrawSpriteChunk(graphHandle, backSrcX, backSrcY, backSrcWidth, backSrcHeight, x, y, width, height, 0.0f);

        SetDrawBlendMode(blendMode, frontAlpha);
        DrawSpriteChunk(graphHandle, frontSrcX, frontSrcY, frontSrcWidth, frontSrcHeight, x + width * 0.02f, y, width, height, 0.0f);
    }
    else
    {
        DrawSpriteChunk(graphHandle, srcX, srcY, srcWidth, srcHeight, x, y, width, height, rot);
    }

    SetDrawBright(255, 255, 255);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
}

void DrawTextFromSheet(int textureID, const std::wstring& text, float posX, float posY, float size)
{
    float x = posX;
    for (wchar_t c : text)
    {
        auto it = fontMap.find(c);
        if (it != fontMap.end())
        {
            const FontIndex idx = it->second;
            const float cellU = 1.0f / 16.0f;
            const float cellV = 1.0f / 16.0f;
            SpriteDraw(textureID, x, posY, size, size, idx.x * cellU, idx.y * cellV, cellU, cellV, 0.0f);
        }
        x += size * 0.9f;
    }
}
