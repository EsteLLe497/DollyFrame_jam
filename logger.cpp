#include "logger.h"

#include <memory>
#include <string>
#include <vector>

#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

namespace
{
    std::shared_ptr<spdlog::logger> g_logger;
}

bool Logger::Initialize()
{
    if (g_logger)
    {
        return true;
    }

    try
    {
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("foundation.log", true));
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());

        g_logger = std::make_shared<spdlog::logger>("foundation", sinks.begin(), sinks.end());
        g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        g_logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(g_logger);
        spdlog::flush_on(spdlog::level::info);
        Info("Logger initialized");
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Logger::Shutdown()
{
    if (g_logger)
    {
        Info("Logger shutdown");
        spdlog::shutdown();
        g_logger.reset();
    }
}

void Logger::Info(std::string_view message)
{
    if (g_logger)
    {
        g_logger->info(std::string(message));
    }
}

void Logger::Warn(std::string_view message)
{
    if (g_logger)
    {
        g_logger->warn(std::string(message));
    }
}

void Logger::Error(std::string_view message)
{
    if (g_logger)
    {
        g_logger->error(std::string(message));
    }
}
