#include "StateManager.hpp"
#include "State.hpp"

namespace Engine
{
    StateManager::StateManager()
        : m_add(false), m_replace(false), m_remove(false)
    {
    }

    StateManager::~StateManager() = default;

    void StateManager::Add(std::unique_ptr<State> toAdd, bool replace)
    {
        m_add = true;
        m_newState = std::move(toAdd);
        m_replace = replace;
    }

    void StateManager::PopCurrent()
    {
        m_remove = true;
    }

    void StateManager::ProcessStateChange()
    {

        if (m_remove && !m_stateStack.empty())
        {

            m_stateStack.pop_back();

            if (!m_stateStack.empty())
            {

                m_stateStack.back()->Start();
            }
            m_remove = false;
        }

        if (m_add)
        {
            if (m_replace && !m_stateStack.empty())
            {
                m_stateStack.pop_back();
                m_replace = false;
            }

            if (!m_stateStack.empty())
            {
                m_stateStack.back()->Pause();
            }

            m_stateStack.push_back(std::move(m_newState));

            m_stateStack.back()->Init();
            m_stateStack.back()->Start();

            m_add = false;
        }
    }

    std::unique_ptr<State> &StateManager::getCurrent()
    {

        return m_stateStack.back();
    }

    bool StateManager::isEmpty() const
    {
        return m_stateStack.empty();
    }
}
