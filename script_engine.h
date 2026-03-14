#pragma once

#include <string>

class EventBus;

namespace sol
{
    class state;
}

class ScriptEngine
{
public:
    ScriptEngine();
    ~ScriptEngine();

    bool Initialize();
    void Shutdown();
    void BindEventBus(EventBus& eventBus);
    bool LoadFile(const std::string& path);
    void SetNumber(const std::string& name, double value);
    double GetNumber(const std::string& name, double fallback = 0.0) const;
    bool CallUpdate(double deltaTime);

private:
    sol::state* m_lua;
};
