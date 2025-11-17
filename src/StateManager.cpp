#include "StateManager.hpp"

namespace Engine
{
    StateManager::StateManager()
    : m_add(false)
    , m_replace(false)
    , m_remove(false)
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
        // Xử lý remove state
        if (m_remove && !m_stateStack.empty())
        {
            m_stateStack.pop();
            if (!m_stateStack.empty())
            {
                m_stateStack.top()->Start();
            }
            m_remove = false;
        }

        // Xử lý add / replace
        if (m_add)
        {
            if (m_replace && !m_stateStack.empty())
            {
                m_stateStack.pop();
                m_replace = false;
            }

            if (!m_stateStack.empty())
            {
                m_stateStack.top()->Pause();
            }

            m_stateStack.push(std::move(m_newState));
            m_stateStack.top()->Init();
            m_stateStack.top()->Start();
            m_add = false;
        }
    }

    std::unique_ptr<State>& StateManager::getCurrent()
    {
        // KHÔNG ném exception nữa, nhưng bạn phải đảm bảo không gọi khi stack rỗng
        return m_stateStack.top();
    }

    bool StateManager::isEmpty() const
    {
        return m_stateStack.empty();
    }
}
