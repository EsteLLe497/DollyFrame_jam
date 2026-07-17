#pragma once

#include <string>

void TextureInitialize(void* device);
int TextureLoad(const std::wstring& texture_filename, bool asyncLoad = false);
int TextureCreateSolidColor(int width, int height, unsigned int rgba);
int TextureCreateCheckerboard(int width, int height, unsigned int rgbaA, unsigned int rgbaB, int cellSize);
int TextureCreateDisc(int width, int height, unsigned int rgbaInner, unsigned int rgbaOuter, float innerRatio);
void* GetTexture(int id);
int TextureGetGraphHandle(int id);
int TextureGetWidth(int id);
int TextureGetHeight(int id);
void TextureFinalize(void);
