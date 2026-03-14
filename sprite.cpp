#include "directX.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"

using namespace DirectX;

static ID3D11Buffer* g_VertexBuffer = nullptr;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texcoord;
};

void SpriteInitialize(void)
{
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    DirectXGetDevice()->CreateBuffer(&bd, nullptr, &g_VertexBuffer);
}

void SpriteFinalize(void)
{
    SAFE_RELEASE(g_VertexBuffer);
}

void SpriteDraw(int textureID, float x, float y, float width, float height, float tx, float ty, float tw, float th, float rot)
{
    auto* ctx = DirectXGetDeviceContext();
    if (!ctx || !g_VertexBuffer)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (FAILED(ctx->Map(g_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        return;
    }

    auto* v = static_cast<Vertex*>(msr.pData);
    v[0].position = { -width * 0.5f, -height * 0.5f, 0.0f };
    v[1].position = { width * 0.5f, -height * 0.5f, 0.0f };
    v[2].position = { -width * 0.5f, height * 0.5f, 0.0f };
    v[3].position = { width * 0.5f, height * 0.5f, 0.0f };

    v[0].texcoord = { tx, ty };
    v[1].texcoord = { tx + tw, ty };
    v[2].texcoord = { tx, ty + th };
    v[3].texcoord = { tx + tw, ty + th };
    ctx->Unmap(g_VertexBuffer, 0);

    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;

    Shader_Begin();
    Shader_BindSpriteTextures(textureID);
    Shader_SetTextureSize(static_cast<float>(TextureGetWidth(textureID)), static_cast<float>(TextureGetHeight(textureID)));

    XMMATRIX world = XMMatrixRotationZ(rot) * XMMatrixTranslation(cx, cy, 0.0f);
    XMMATRIX proj = XMMatrixOrthographicOffCenterLH(
        0.0f, static_cast<float>(SCREEN_WIDTH),
        static_cast<float>(SCREEN_HEIGHT), 0.0f,
        0.0f, 1.0f);

    Shader_SetMatrix(world * proj);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    ctx->IASetVertexBuffers(0, 1, &g_VertexBuffer, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
}

constexpr int FONT_COLS = 16;
constexpr int FONT_ROWS = 16;

static void DrawCharFromSheet(int textureID, int cellX, int cellY, float x, float y, float size)
{
    const float cellU = 1.0f / FONT_COLS;
    const float cellV = 1.0f / FONT_ROWS;
    const float u = cellX * cellU;
    const float v = cellY * cellV;
    SpriteDraw(textureID, x, y, size, size, u, v, cellU, cellV, 0.0f);
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
            DrawCharFromSheet(textureID, idx.x, idx.y, x, posY, size);
        }
        x += size * 0.9f;
    }
}
