#pragma once

#include <memory>
#include "State.hpp"
#include "GameApp.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

class ModeSelection : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::RectangleShape m_2PlayersBox;
    sf::RectangleShape m_aiBox;

    sf::Text m_titleText;
    sf::Text m_2PlayersText;
    sf::Text m_aiText;
    sf::Text m_returnText;

public:
    ModeSelection(std::shared_ptr<Context>& context);
    ~ModeSelection() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
