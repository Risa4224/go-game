#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include <optional>
#include <string>

#include "GameApp.h"
#include "State.hpp"

class EndGameState : public Engine::State
{
public:
    EndGameState(std::shared_ptr<Context>& context, const std::string& msg);

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time dt) override;
    void Draw() override;

    // ✅ Allow the board (previous state) to still draw behind this mini overlay
    bool AllowDrawBelow() const override { return true; }

private:
    std::shared_ptr<Context> m_context;

    std::string m_msg;

    sf::RectangleShape m_dim;
    sf::RectangleShape m_panel;

    sf::RectangleShape m_restartBtn;
    sf::RectangleShape m_closeBtn;
    bool m_restartHover = false;
    bool m_closeHover = false;

    // ✅ SFML3-safe (no default ctor)
    std::optional<sf::Text> m_titleText;
    std::optional<sf::Text> m_msgText;
    std::optional<sf::Text> m_restartText;
    std::optional<sf::Text> m_closeText;
};
