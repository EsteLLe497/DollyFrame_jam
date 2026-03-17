#pragma once

#include <string_view>

class Logger
{
public:
    static bool Initialize();
    static void Shutdown();
    static void Info(std::string_view message);
    static void Warn(std::string_view message);
    static void Error(std::string_view message);
};
