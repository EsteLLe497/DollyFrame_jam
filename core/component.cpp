#include "pch.h"

#include "component.h"

void Component::OnAttach(Entity& owner)
{
    m_owner = &owner;
}

void Component::Update(float)
{
}

void Component::Draw()
{
}

void Component::DrawDebugUI()
{
}
