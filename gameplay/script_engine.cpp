#include "pch.h"

#include "script_engine.h"

#include <memory>

#include <sol/sol.hpp>

#include "event_bus.h"
#include "logger.h"

ScriptEngine::ScriptEngine()
    : m_lua(nullptr)
{
}

ScriptEngine::~ScriptEngine()
{
    Shutdown();
}

bool ScriptEngine::Initialize()
{
    if (m_lua)
    {
        return true;
    }

    m_lua = new sol::state();
    m_lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::package, sol::lib::table);
    Logger::Info("Lua scripting initialized");
    return true;
}

void ScriptEngine::Shutdown()
{
    if (m_lua)
    {
        Logger::Info("Lua scripting shutdown");
        delete m_lua;
        m_lua = nullptr;
    }
}

void ScriptEngine::BindEventBus(EventBus& eventBus)
{
    if (!m_lua)
    {
        return;
    }

    m_lua->set_function("log_message",
        [&eventBus](const std::string& message)
        {
            eventBus.Publish({ EventType::LogMessage, nullptr, nullptr, message, 0.0f, 0.0f });
        });

    m_lua->set_function("request_sound",
        [&eventBus](const std::string& cueName)
        {
            eventBus.Publish({ EventType::PlaySoundRequest, nullptr, nullptr, cueName, 0.0f, 0.0f });
        });

    m_lua->set_function("request_scene_change",
        [&eventBus](const std::string& sceneId)
        {
            eventBus.Publish({ EventType::SceneChangeRequested, nullptr, nullptr, sceneId, 0.0f, 0.0f });
        });
}

bool ScriptEngine::LoadFile(const std::string& path)
{
    if (!m_lua)
    {
        return false;
    }

    sol::load_result chunk = m_lua->load_file(path);
    if (!chunk.valid())
    {
        const sol::error err = chunk;
        Logger::Error(err.what());
        return false;
    }

    sol::protected_function_result result = chunk();
    if (!result.valid())
    {
        const sol::error err = result;
        Logger::Error(err.what());
        return false;
    }

    Logger::Info("Lua script loaded");
    return true;
}

void ScriptEngine::SetNumber(const std::string& name, double value)
{
    if (m_lua)
    {
        (*m_lua)[name] = value;
    }
}

double ScriptEngine::GetNumber(const std::string& name, double fallback) const
{
    if (!m_lua)
    {
        return fallback;
    }

    sol::object obj = (*m_lua)[name];
    if (!obj.is<double>())
    {
        return fallback;
    }
    return obj.as<double>();
}

bool ScriptEngine::CallUpdate(double deltaTime)
{
    if (!m_lua)
    {
        return false;
    }

    sol::protected_function update = (*m_lua)["update"];
    if (!update.valid())
    {
        return false;
    }

    sol::protected_function_result result = update(deltaTime);
    if (!result.valid())
    {
        const sol::error err = result;
        Logger::Error(err.what());
        return false;
    }

    return true;
}
