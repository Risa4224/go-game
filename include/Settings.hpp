#pragma once

#include <memory>
#include "State.hpp"
#include "GameApp.h"

#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

class SettingsState : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::Text m_titleText;
    sf::Text m_musicLabel;
    sf::Text m_musicValue;
    sf::Text m_backText;

    sf::RectangleShape m_musicBox;
    sf::RectangleShape m_backBox;

    bool m_musicHovered;
    bool m_backHovered;

public:
    SettingsState(std::shared_ptr<Context>& context);
    ~SettingsState() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
