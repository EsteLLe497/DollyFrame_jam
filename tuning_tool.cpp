#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

namespace
{
    struct TuningField
    {
        const char* key;
        const char* label;
        double defaultValue;
    };

    constexpr TuningField kFields[] =
    {
        { "camera_view_width", "Camera Width", 1120.0 },
        { "camera_view_height", "Camera Height", 630.0 },
        { "move_speed", "Move Speed", 320.0 },
        { "jump_speed", "Jump Speed", -760.0 },
        { "gravity", "Gravity", 1900.0 },
        { "max_fall_speed", "Max Fall Speed", 980.0 },
        { "coyote_time", "Coyote Time", 0.1 },
        { "ground_snap_distance", "Ground Snap", 8.0 },
        { "capture_width_scale", "Capture Width", 1.85 },
        { "capture_height_scale", "Capture Height", 1.15 },
        { "pickup_time_bonus", "Pickup Bonus", 8.0 },
    };

    constexpr int kApplyButtonId = 1001;
    constexpr int kReloadButtonId = 1002;
    constexpr int kStatusLabelId = 1003;
    constexpr int kEditBaseId = 2000;

    std::array<HWND, std::size(kFields)> g_editControls{};

    std::filesystem::path GetTuningFilePath()
    {
        return std::filesystem::current_path() / "assets" / "tuning.json";
    }

    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), length);
        if (!wide.empty() && wide.back() == L'\0')
        {
            wide.pop_back();
        }
        return wide;
    }

    std::string ToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, utf8.data(), length, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
        {
            utf8.pop_back();
        }
        return utf8;
    }

    void SetStatus(HWND window, const wchar_t* text)
    {
        SetWindowTextW(GetDlgItem(window, kStatusLabelId), text);
    }

    bool LoadJson(nlohmann::json& root)
    {
        std::ifstream stream(GetTuningFilePath(), std::ios::binary);
        if (!stream.is_open())
        {
            root = nlohmann::json::object();
            for (const auto& field : kFields)
            {
                root[field.key] = field.defaultValue;
            }
            return false;
        }

        try
        {
            stream >> root;
        }
        catch (...)
        {
            root = nlohmann::json::object();
            for (const auto& field : kFields)
            {
                root[field.key] = field.defaultValue;
            }
            return false;
        }

        return true;
    }

    void PopulateControls(HWND window)
    {
        nlohmann::json root;
        const bool loaded = LoadJson(root);
        for (size_t index = 0; index < std::size(kFields); ++index)
        {
            const auto& field = kFields[index];
            const double value = root.value(field.key, field.defaultValue);
            const std::wstring text = ToWide(std::to_string(value));
            SetWindowTextW(g_editControls[index], text.c_str());
        }

        SetStatus(window, loaded ? L"Loaded assets/tuning.json" : L"Created defaults in memory");
    }

    bool SaveControls(HWND window)
    {
        nlohmann::json root;
        for (size_t index = 0; index < std::size(kFields); ++index)
        {
            wchar_t buffer[128]{};
            GetWindowTextW(g_editControls[index], buffer, static_cast<int>(std::size(buffer)));
            try
            {
                root[kFields[index].key] = std::stod(buffer);
            }
            catch (...)
            {
                SetStatus(window, L"Invalid number detected");
                return false;
            }
        }

        std::ofstream stream(GetTuningFilePath(), std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            SetStatus(window, L"Failed to open assets/tuning.json");
            return false;
        }

        stream << root.dump(2);
        SetStatus(window, L"Saved assets/tuning.json");
        return true;
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            int y = 16;
            for (size_t index = 0; index < std::size(kFields); ++index)
            {
                CreateWindowW(L"STATIC", ToWide(kFields[index].label).c_str(),
                    WS_CHILD | WS_VISIBLE,
                    16, y + 4, 140, 22,
                    window, nullptr, nullptr, nullptr);

                g_editControls[index] = CreateWindowW(
                    L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                    164, y, 140, 24,
                    window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEditBaseId + index)), nullptr, nullptr);
                SendMessageW(g_editControls[index], WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                y += 30;
            }

            HWND applyButton = CreateWindowW(L"BUTTON", L"Apply",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                16, y + 12, 100, 30,
                window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kApplyButtonId)), nullptr, nullptr);
            SendMessageW(applyButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            HWND reloadButton = CreateWindowW(L"BUTTON", L"Reload",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                128, y + 12, 100, 30,
                window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kReloadButtonId)), nullptr, nullptr);
            SendMessageW(reloadButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            HWND statusLabel = CreateWindowW(L"STATIC", L"",
                WS_CHILD | WS_VISIBLE,
                16, y + 52, 320, 22,
                window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStatusLabelId)), nullptr, nullptr);
            SendMessageW(statusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            PopulateControls(window);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case kApplyButtonId:
                SaveControls(window);
                return 0;
            case kReloadButtonId:
                PopulateControls(window);
                return 0;
            default:
                break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    const wchar_t* className = L"DollyFrameTuningToolWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&windowClass))
    {
        return 1;
    }

    HWND window = CreateWindowW(
        className,
        L"DollyFrame Tuning Tool",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        360, 450,
        nullptr, nullptr, instance, nullptr);
    if (!window)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
