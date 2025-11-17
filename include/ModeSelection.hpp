#pragma once

#include <memory>
#include "State.hpp"
#include "GameApp.h"
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/CircleShape.hpp>
class ModeSelection : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;
    sf::RectangleShape m_2playersMode;
    sf::RectangleShape m_AiMode;
    sf::CircleShape m_returnButton;
    // sf::Text m_2playersText;
    // sf::Text m_AiModeText;
    // sf::Text m_SelectMode;
    bool m_is2PlayersSelected;
    bool m_isAiSelected;
    bool m_isReturnButtonSelected;
    bool m_is2PlayersPressed;
    bool m_isAiPressed;
    bool m_isReturnButtonPressed;
public:
    ModeSelection(std::shared_ptr<Context>& context);
    ~ModeSelection();


    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;



};