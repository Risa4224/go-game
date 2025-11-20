#pragma once

#include <memory>
#include <optional>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include "State.hpp"
#include "GameApp.h"
#include "MainBoard.hpp"

class ModeSelection : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::RectangleShape m_2PlayersBox;
    sf::RectangleShape m_aiBox;
    sf::RectangleShape m_backBox;

    std::optional<sf::Text> m_titleText;
    std::optional<sf::Text> m_2PlayersText;
    std::optional<sf::Text> m_aiText;
    std::optional<sf::Text> m_backText;

    bool m_2PlayersHovered{false};
    bool m_aiHovered{false};
    bool m_backHovered{false};

public:
    ModeSelection(std::shared_ptr<Context> &context);
    ~ModeSelection() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
