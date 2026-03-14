#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <unordered_map>

struct SpriteVertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 texcoord;
};

void SpriteInitialize(void);
void SpriteFinalize(void);
void SpriteDraw(int textureID, float x, float y, float width, float height, float tx, float ty, float tw, float th, float rot = 0.0f);
void DrawTextFromSheet(int textureID, const std::wstring& text, float posX, float posY, float size);

struct FontIndex
{
    int x;
    int y;
};

inline const std::unordered_map<wchar_t, FontIndex> fontMap = {};
