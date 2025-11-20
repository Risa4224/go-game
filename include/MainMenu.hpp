#pragma once

#include <memory>
#include "State.hpp"
#include "GameApp.h"

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

class MainMenu : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::Text m_gameTitle;

    sf::RectangleShape m_playButtonBox;
    sf::RectangleShape m_settingsButtonBox;
    sf::RectangleShape m_exitButtonBox;

    sf::Text m_playButtonText;
    sf::Text m_settingsButtonText;
    sf::Text m_exitButtonText;

    bool m_isPlayButtonSelected;
    bool m_isSettingsButtonSelected;
    bool m_isExitButtonSelected;

    bool m_isPlayButtonPressed;
    bool m_isSettingsButtonPressed;
    bool m_isExitButtonPressed;

public:
    MainMenu(std::shared_ptr<Context> &context);
    ~MainMenu() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
