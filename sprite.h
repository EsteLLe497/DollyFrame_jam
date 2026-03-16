#pragma once

#include <string>
#include <unordered_map>

void SpriteInitialize(void);
void SpriteFinalize(void);
void SpriteDraw(int textureID, float x, float y, float width, float height, float tx, float ty, float tw, float th, bool flipX, float rot);
inline void SpriteDraw(int textureID, float x, float y, float width, float height, float tx, float ty, float tw, float th, float rot = 0.0f)
{
    SpriteDraw(textureID, x, y, width, height, tx, ty, tw, th, false, rot);
}
void DrawTextFromSheet(int textureID, const std::wstring& text, float posX, float posY, float size);

struct FontIndex
{
    int x;
    int y;
};

inline const std::unordered_map<wchar_t, FontIndex> fontMap = {};
