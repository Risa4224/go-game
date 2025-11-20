#pragma once

#include <memory>
#include <vector>
#include "State.hpp"

namespace Engine
{
    class StateManager
    {
    private:
        std::vector<std::unique_ptr<State>> m_stateStack;
        std::unique_ptr<State> m_newState;

        bool m_add;
        bool m_replace;
        bool m_remove;

    public:
        StateManager();
        ~StateManager();

        void Add(std::unique_ptr<State> toAdd, bool replace = false);
        void PopCurrent();
        void ProcessStateChange();

        std::unique_ptr<State> &getCurrent();
        bool isEmpty() const;
        std::vector<std::unique_ptr<State>> &getStack() { return m_stateStack; }
    };
}
