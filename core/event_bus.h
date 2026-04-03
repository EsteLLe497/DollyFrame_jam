#pragma once

#include <string>
#include <vector>

class Entity;

enum class EventType
{
    ContactBegin,
    ContactEnd,
    PlaySoundRequest,
    SceneChangeRequested,
    ExitApplicationRequested,
    LogMessage,
};

struct Event
{
    EventType type = EventType::LogMessage;
    Entity* entityA = nullptr;
    Entity* entityB = nullptr;
    std::string name;
    float value0 = 0.0f;
    float value1 = 0.0f;
};

class EventBus
{
public:
    void Clear();
    void Publish(Event eventData);
    const std::vector<Event>& GetEvents() const;
    int Count(EventType type) const;

private:
    std::vector<Event> m_events;
};
