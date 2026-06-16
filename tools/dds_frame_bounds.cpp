// =========================================================
// ファイルの情報[dds_frame_bounds.cpp]
//
// 制作者:Masatora Tanaka        日付:2026/06/17
// =========================================================
#include <Windows.h>
#include <DirectXTex.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>

// =========================================================
// DDSの各フレームの非透明範囲を出力
// =========================================================
int wmain(int argc, wchar_t** argv)
{
    if (argc < 5)
    {
        std::wcerr << L"Usage: dds_frame_bounds <sheet.dds> <columns> <rows> <frameCount>\n";
        return 1;
    }

    const int columns = _wtoi(argv[2]);
    const int rows = _wtoi(argv[3]);
    const int frameCount = _wtoi(argv[4]);
    if (columns <= 0 || rows <= 0 || frameCount <= 0)
    {
        std::wcerr << L"Invalid grid arguments.\n";
        return 1;
    }

    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result))
    {
        std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << result << L"\n";
        return 1;
    }

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;
    result = DirectX::LoadFromDDSFile(argv[1], DirectX::DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(result))
    {
        std::wcerr << L"LoadFromDDSFile failed: 0x" << std::hex << result << L"\n";
        CoUninitialize();
        return 1;
    }

    DirectX::ScratchImage rgbaImage;
    result = DirectX::Convert(
        image.GetImages(),
        image.GetImageCount(),
        metadata,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DirectX::TEX_FILTER_DEFAULT,
        DirectX::TEX_THRESHOLD_DEFAULT,
        rgbaImage);
    if (FAILED(result))
    {
        std::wcerr << L"Convert failed: 0x" << std::hex << result << L"\n";
        CoUninitialize();
        return 1;
    }

    const DirectX::Image* rgba = rgbaImage.GetImage(0, 0, 0);
    if (!rgba || !rgba->pixels)
    {
        std::wcerr << L"No image pixels.\n";
        CoUninitialize();
        return 1;
    }

    const int cellWidth = static_cast<int>(rgba->width) / columns;
    const int cellHeight = static_cast<int>(rgba->height) / rows;
    std::cout << "frame,left,top,right,bottom,centerX,centerY,width,height\n";

    for (int frame = 0; frame < frameCount; ++frame)
    {
        const int frameColumn = frame % columns;
        const int frameRow = frame / columns;
        const int baseX = frameColumn * cellWidth;
        const int baseY = frameRow * cellHeight;

        int minX = std::numeric_limits<int>::max();
        int minY = std::numeric_limits<int>::max();
        int maxX = std::numeric_limits<int>::min();
        int maxY = std::numeric_limits<int>::min();

        for (int y = 0; y < cellHeight; ++y)
        {
            const auto* row = rgba->pixels + static_cast<size_t>(baseY + y) * rgba->rowPitch;
            for (int x = 0; x < cellWidth; ++x)
            {
                const uint8_t* pixel = row + static_cast<size_t>(baseX + x) * 4;
                if (pixel[3] <= 8)
                {
                    continue;
                }
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }

        if (minX > maxX || minY > maxY)
        {
            std::cout << frame << ",0,0,0,0,0,0,0,0\n";
            continue;
        }

        const int width = maxX - minX + 1;
        const int height = maxY - minY + 1;
        const float centerX = static_cast<float>(minX + maxX) * 0.5f;
        const float centerY = static_cast<float>(minY + maxY) * 0.5f;
        std::cout << frame << ','
            << minX << ',' << minY << ','
            << maxX << ',' << maxY << ','
            << centerX << ',' << centerY << ','
            << width << ',' << height << '\n';
    }

    CoUninitialize();
    return 0;
}
