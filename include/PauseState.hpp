#pragma once

#include <memory>
#include <string>

#include "State.hpp"
#include "GameApp.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

class PauseState : public Engine::State
{
public:
    enum class Mode
    {
        Paused,
        GameOver
    };

private:
    std::shared_ptr<Context> m_context;
    Mode m_mode;
    std::string m_message;

    sf::RectangleShape m_overlay;
    sf::RectangleShape m_panel;

    sf::Text m_titleText;
    sf::Text m_messageText;

    sf::RectangleShape m_okButtonBox;
    sf::Text m_okText;
    bool m_okHovered;

public:
    PauseState(std::shared_ptr<Context> &context,
               Mode mode,
               const std::string &message = "");

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;

    bool AllowDrawBelow() const override { return true; }
};
