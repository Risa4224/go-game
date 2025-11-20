#pragma once

#include <SFML/System/Time.hpp>

namespace Engine
{
    class State
    {
    public:
        State() = default;
        virtual ~State() = default;

        virtual void Init() = 0;
        virtual void ProcessInput() = 0;
        virtual void Update(sf::Time deltaTime) = 0;
        virtual void Draw() = 0;

                virtual void Pause() {}
        virtual void Start() {}

        virtual bool AllowDrawBelow() const { return false; }
    };
}
