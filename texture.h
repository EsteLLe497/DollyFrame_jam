#pragma once

#include <d3d11.h>
#include <string>

void TextureInitialize(ID3D11Device* device);
int TextureLoad(const std::wstring& texture_filename);
int TextureCreateSolidColor(int width, int height, unsigned int rgba);
int TextureCreateCheckerboard(int width, int height, unsigned int rgbaA, unsigned int rgbaB, int cellSize);
int TextureCreateDisc(int width, int height, unsigned int rgbaInner, unsigned int rgbaOuter, float innerRatio);
ID3D11ShaderResourceView* GetTexture(int id);
int TextureGetWidth(int id);
int TextureGetHeight(int id);
void TextureFinalize(void);
