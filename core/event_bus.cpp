#include "pch.h"

#include "event_bus.h"

#include <algorithm>

void EventBus::Reserve(size_t count)
{
    m_events.reserve(count);
}

void EventBus::Clear()
{
    m_events.clear();
}

void EventBus::Publish(Event eventData)
{
    m_events.push_back(std::move(eventData));
}

const std::vector<Event>& EventBus::GetEvents() const
{
    return m_events;
}

int EventBus::Count(EventType type) const
{
    return static_cast<int>(std::count_if(m_events.begin(), m_events.end(), [type](const Event& eventData)
    {
        return eventData.type == type;
    }));
}
