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
    enum class Screen
    {
        SelectMode,
        AiOptions
    };

    std::shared_ptr<Context> m_context;
    Screen m_screen{Screen::SelectMode};

    sf::RectangleShape m_2PlayersBox;
    sf::RectangleShape m_aiBox;
    sf::RectangleShape m_backBox;

    // --- AI Options UI ---
    sf::RectangleShape m_colorBlackBox;
    sf::RectangleShape m_colorWhiteBox;
    sf::RectangleShape m_diffEasyBox;
    sf::RectangleShape m_diffMediumBox;
    sf::RectangleShape m_diffHardBox;
    sf::RectangleShape m_startBox;
    sf::RectangleShape m_aiBackBox;

    std::optional<sf::Text> m_titleText;
    std::optional<sf::Text> m_2PlayersText;
    std::optional<sf::Text> m_aiText;
    std::optional<sf::Text> m_backText;

    std::optional<sf::Text> m_aiTitleText;
    std::optional<sf::Text> m_colorLabelText;
    std::optional<sf::Text> m_blackText;
    std::optional<sf::Text> m_whiteText;
    std::optional<sf::Text> m_diffLabelText;
    std::optional<sf::Text> m_easyText;
    std::optional<sf::Text> m_mediumText;
    std::optional<sf::Text> m_hardText;
    std::optional<sf::Text> m_startText;
    std::optional<sf::Text> m_aiBackText;

    bool m_2PlayersHovered{false};
    bool m_aiHovered{false};
    bool m_backHovered{false};

    bool m_blackHovered{false};
    bool m_whiteHovered{false};
    bool m_easyHovered{false};
    bool m_mediumHovered{false};
    bool m_hardHovered{false};
    bool m_startHovered{false};
    bool m_aiBackHovered{false};

    // AI selections
    bool m_humanPlaysBlack{true};
    int m_difficultyIndex{1}; // 0=EASY, 1=MEDIUM, 2=HARD

public:
    ModeSelection(std::shared_ptr<Context> &context);
    ~ModeSelection() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
