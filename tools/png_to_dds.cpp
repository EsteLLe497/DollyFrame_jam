// =========================================================
// ファイルの情報[png_to_dds.cpp]
//
// 制作者:Masatora Tanaka        日付:2026/06/17
// =========================================================
#include <Windows.h>
#include <DirectXTex.h>

#include <iostream>

// =========================================================
// PNGをDDSへ変換
// =========================================================
int wmain(int argc, wchar_t** argv)
{
    if (argc < 3)
    {
        std::wcerr << L"Usage: png_to_dds <input.png> <output.dds>\n";
        return 1;
    }

    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initResult))
    {
        std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << initResult << L"\n";
        return 1;
    }

    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;
    HRESULT result = DirectX::LoadFromWICFile(argv[1], DirectX::WIC_FLAGS_NONE, &metadata, image);
    if (FAILED(result))
    {
        std::wcerr << L"LoadFromWICFile failed: 0x" << std::hex << result << L"\n";
        CoUninitialize();
        return 1;
    }

    result = DirectX::SaveToDDSFile(
        image.GetImages(),
        image.GetImageCount(),
        metadata,
        DirectX::DDS_FLAGS_NONE,
        argv[2]);
    if (FAILED(result))
    {
        std::wcerr << L"SaveToDDSFile failed: 0x" << std::hex << result << L"\n";
        CoUninitialize();
        return 1;
    }

    CoUninitialize();
    return 0;
}
