#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
#include <iostream>

#include "AI.h"

ModeSelection::ModeSelection(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_2PlayersBox{{260.f, 120.f}}
    , m_aiBox{{260.f, 120.f}}
    , m_backBox{{260.f, 80.f}}

    , m_colorBlackBox{{240.f, 90.f}}
    , m_colorWhiteBox{{240.f, 90.f}}
    , m_diffEasyBox{{180.f, 80.f}}
    , m_diffMediumBox{{180.f, 80.f}}
    , m_diffHardBox{{180.f, 80.f}}
    , m_startBox{{320.f, 85.f}}
    , m_aiBackBox{{260.f, 70.f}}

    , m_titleText(std::nullopt)
    , m_2PlayersText(std::nullopt)
    , m_aiText(std::nullopt)
    , m_backText(std::nullopt)

    , m_aiTitleText(std::nullopt)
    , m_colorLabelText(std::nullopt)
    , m_blackText(std::nullopt)
    , m_whiteText(std::nullopt)
    , m_diffLabelText(std::nullopt)
    , m_easyText(std::nullopt)
    , m_mediumText(std::nullopt)
    , m_hardText(std::nullopt)
    , m_startText(std::nullopt)
    , m_aiBackText(std::nullopt)
{ }

ModeSelection::~ModeSelection() = default;

void ModeSelection::Init()
{
    auto& font = m_context->m_assets->GetFont(MAIN_FONT);

    m_titleText.emplace(font, "Select Game Mode", 42);
    m_2PlayersText.emplace(font, "2 Players", 32);
    m_aiText.emplace(font, "AI vs Player", 32);
    m_backText.emplace(font, "Back to Menu", 24);

    // --- AI Options labels ---
    m_aiTitleText.emplace(font, "AI Match Setup", 42);
    m_colorLabelText.emplace(font, "Choose your color", 26);
    m_blackText.emplace(font, "Play Black (First)", 22);
    m_whiteText.emplace(font, "Play White (Second)", 22);
    m_diffLabelText.emplace(font, "Difficulty", 26);
    m_easyText.emplace(font, "Easy", 22);
    m_mediumText.emplace(font, "Medium", 22);
    m_hardText.emplace(font, "Hard", 22);
    m_startText.emplace(font, "Start", 28);
    m_aiBackText.emplace(font, "Back", 22);

    const auto win = m_context->m_window->getSize();
    const float cx = static_cast<float>(win.x) * 0.5f;
    const float cy = static_cast<float>(win.y) * 0.5f;

    // --- Mode screen ---
    {
        auto b = m_titleText->getLocalBounds();
        m_titleText->setOrigin(b.getCenter());
        m_titleText->setPosition({cx, cy - 220.f});
    }

    {
        auto bounds = m_2PlayersBox.getLocalBounds();
        m_2PlayersBox.setOrigin(bounds.getCenter());
        m_2PlayersBox.setPosition({cx - 260.f, cy});
        m_2PlayersBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_2PlayersText->getLocalBounds();
        m_2PlayersText->setOrigin(tb.getCenter());
        m_2PlayersText->setPosition(m_2PlayersBox.getPosition());
        m_2PlayersText->setFillColor(sf::Color::Black);
    }

    {
        auto bounds = m_aiBox.getLocalBounds();
        m_aiBox.setOrigin(bounds.getCenter());
        m_aiBox.setPosition({cx + 260.f, cy});
        m_aiBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_aiText->getLocalBounds();
        m_aiText->setOrigin(tb.getCenter());
        m_aiText->setPosition(m_aiBox.getPosition());
        m_aiText->setFillColor(sf::Color::Black);
    }

    {
        auto bounds = m_backBox.getLocalBounds();
        m_backBox.setOrigin(bounds.getCenter());
        m_backBox.setPosition({cx, cy + 220.f});
        m_backBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_backText->getLocalBounds();
        m_backText->setOrigin(tb.getCenter());
        m_backText->setPosition(m_backBox.getPosition());
        m_backText->setFillColor(sf::Color::Black);
    }

    // --- AI Options layout ---
    {
        auto b = m_aiTitleText->getLocalBounds();
        m_aiTitleText->setOrigin(b.getCenter());
        m_aiTitleText->setPosition({cx, cy - 260.f});
    }

    {
        auto b = m_colorLabelText->getLocalBounds();
        m_colorLabelText->setOrigin(b.getCenter());
        m_colorLabelText->setPosition({cx, cy - 185.f});
    }

    // Color boxes
    {
        auto bounds = m_colorBlackBox.getLocalBounds();
        m_colorBlackBox.setOrigin(bounds.getCenter());
        m_colorBlackBox.setPosition({cx - 160.f, cy - 110.f});
        m_colorBlackBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_blackText->getLocalBounds();
        m_blackText->setOrigin(tb.getCenter());
        m_blackText->setPosition(m_colorBlackBox.getPosition());
        m_blackText->setFillColor(sf::Color::Black);
    }
    {
        auto bounds = m_colorWhiteBox.getLocalBounds();
        m_colorWhiteBox.setOrigin(bounds.getCenter());
        m_colorWhiteBox.setPosition({cx + 160.f, cy - 110.f});
        m_colorWhiteBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_whiteText->getLocalBounds();
        m_whiteText->setOrigin(tb.getCenter());
        m_whiteText->setPosition(m_colorWhiteBox.getPosition());
        m_whiteText->setFillColor(sf::Color::Black);
    }

    {
        auto b = m_diffLabelText->getLocalBounds();
        m_diffLabelText->setOrigin(b.getCenter());
        m_diffLabelText->setPosition({cx, cy - 15.f});
    }

    // Difficulty boxes
    {
        auto bounds = m_diffEasyBox.getLocalBounds();
        m_diffEasyBox.setOrigin(bounds.getCenter());
        m_diffEasyBox.setPosition({cx - 220.f, cy + 65.f});
        m_diffEasyBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_easyText->getLocalBounds();
        m_easyText->setOrigin(tb.getCenter());
        m_easyText->setPosition(m_diffEasyBox.getPosition());
        m_easyText->setFillColor(sf::Color::Black);
    }
    {
        auto bounds = m_diffMediumBox.getLocalBounds();
        m_diffMediumBox.setOrigin(bounds.getCenter());
        m_diffMediumBox.setPosition({cx, cy + 65.f});
        m_diffMediumBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_mediumText->getLocalBounds();
        m_mediumText->setOrigin(tb.getCenter());
        m_mediumText->setPosition(m_diffMediumBox.getPosition());
        m_mediumText->setFillColor(sf::Color::Black);
    }
    {
        auto bounds = m_diffHardBox.getLocalBounds();
        m_diffHardBox.setOrigin(bounds.getCenter());
        m_diffHardBox.setPosition({cx + 220.f, cy + 65.f});
        m_diffHardBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_hardText->getLocalBounds();
        m_hardText->setOrigin(tb.getCenter());
        m_hardText->setPosition(m_diffHardBox.getPosition());
        m_hardText->setFillColor(sf::Color::Black);
    }

    // Start / Back
    {
        auto bounds = m_startBox.getLocalBounds();
        m_startBox.setOrigin(bounds.getCenter());
        m_startBox.setPosition({cx, cy + 205.f});
        m_startBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_startText->getLocalBounds();
        m_startText->setOrigin(tb.getCenter());
        m_startText->setPosition(m_startBox.getPosition());
        m_startText->setFillColor(sf::Color::Black);
    }
    {
        auto bounds = m_aiBackBox.getLocalBounds();
        m_aiBackBox.setOrigin(bounds.getCenter());
        m_aiBackBox.setPosition({cx, cy + 295.f});
        m_aiBackBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_aiBackText->getLocalBounds();
        m_aiBackText->setOrigin(tb.getCenter());
        m_aiBackText->setPosition(m_aiBackBox.getPosition());
        m_aiBackText->setFillColor(sf::Color::Black);
    }
}

void ModeSelection::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
        }
        else if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->scancode == sf::Keyboard::Scancode::Escape)
            {
                if (m_screen == Screen::AiOptions) m_screen = Screen::SelectMode;
                else m_context->m_states->PopCurrent();
            }
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)
            };

            // reset all hover flags
            m_2PlayersHovered = m_aiHovered = m_backHovered = false;
            m_blackHovered = m_whiteHovered = false;
            m_easyHovered = m_mediumHovered = m_hardHovered = false;
            m_startHovered = m_aiBackHovered = false;

            if (m_screen == Screen::SelectMode)
            {
                m_2PlayersHovered = m_2PlayersBox.getGlobalBounds().contains(mousePos);
                m_aiHovered       = m_aiBox.getGlobalBounds().contains(mousePos);
                m_backHovered     = m_backBox.getGlobalBounds().contains(mousePos);
            }
            else
            {
                m_blackHovered  = m_colorBlackBox.getGlobalBounds().contains(mousePos);
                m_whiteHovered  = m_colorWhiteBox.getGlobalBounds().contains(mousePos);
                m_easyHovered   = m_diffEasyBox.getGlobalBounds().contains(mousePos);
                m_mediumHovered = m_diffMediumBox.getGlobalBounds().contains(mousePos);
                m_hardHovered   = m_diffHardBox.getGlobalBounds().contains(mousePos);
                m_startHovered  = m_startBox.getGlobalBounds().contains(mousePos);
                m_aiBackHovered = m_aiBackBox.getGlobalBounds().contains(mousePos);
            }
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y)
                };

                if (m_screen == Screen::SelectMode)
                {
                    if (m_2PlayersBox.getGlobalBounds().contains(mousePos))
                    {
                        m_context->m_gameMode = GameMode::TwoPlayers;
                        m_context->m_states->Add(std::make_unique<MainBoard>(m_context), false);
                    }
                    else if (m_aiBox.getGlobalBounds().contains(mousePos))
                    {
                        m_screen = Screen::AiOptions; // open AI setup
                    }
                    else if (m_backBox.getGlobalBounds().contains(mousePos))
                    {
                        m_context->m_states->PopCurrent();
                    }
                }
                else // AiOptions
                {
                    if (m_colorBlackBox.getGlobalBounds().contains(mousePos)) m_humanPlaysBlack = true;
                    else if (m_colorWhiteBox.getGlobalBounds().contains(mousePos)) m_humanPlaysBlack = false;
                    else if (m_diffEasyBox.getGlobalBounds().contains(mousePos)) m_difficultyIndex = 0;
                    else if (m_diffMediumBox.getGlobalBounds().contains(mousePos)) m_difficultyIndex = 1;
                    else if (m_diffHardBox.getGlobalBounds().contains(mousePos)) m_difficultyIndex = 2;
                    else if (m_startBox.getGlobalBounds().contains(mousePos))
                    {
                        m_context->m_gameMode = GameMode::AiVsPlayer;
                        m_context->m_humanPlaysBlack = m_humanPlaysBlack;
                        m_context->m_aiDifficulty = static_cast<AIDifficulty>(m_difficultyIndex);

                        m_context->m_states->Add(std::make_unique<MainBoard>(m_context), false);
                    }
                    else if (m_aiBackBox.getGlobalBounds().contains(mousePos))
                    {
                        m_screen = Screen::SelectMode;
                    }
                }
            }
        }
    }
}

void ModeSelection::Update(sf::Time)
{
    auto apply = [](sf::RectangleShape& box, bool hovered, bool selected)
    {
        sf::Color c = hovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200);
        if (selected)
        {
            c = hovered ? sf::Color(185, 235, 185) : sf::Color(170, 220, 170);
            box.setOutlineThickness(4.f);
            box.setOutlineColor(sf::Color(80, 140, 80));
        }
        else
        {
            box.setOutlineThickness(0.f);
        }
        box.setFillColor(c);
    };

    if (m_screen == Screen::SelectMode)
    {
        apply(m_2PlayersBox, m_2PlayersHovered, false);
        apply(m_aiBox, m_aiHovered, false);
        apply(m_backBox, m_backHovered, false);
    }
    else
    {
        apply(m_colorBlackBox, m_blackHovered, m_humanPlaysBlack);
        apply(m_colorWhiteBox, m_whiteHovered, !m_humanPlaysBlack);

        apply(m_diffEasyBox, m_easyHovered, m_difficultyIndex == 0);
        apply(m_diffMediumBox, m_mediumHovered, m_difficultyIndex == 1);
        apply(m_diffHardBox, m_hardHovered, m_difficultyIndex == 2);

        apply(m_startBox, m_startHovered, false);
        apply(m_aiBackBox, m_aiBackHovered, false);
    }
}

void ModeSelection::Draw()
{
    if (m_screen == Screen::SelectMode)
    {
        if (m_titleText)    m_context->m_window->draw(*m_titleText);
        m_context->m_window->draw(m_2PlayersBox);
        m_context->m_window->draw(m_aiBox);
        m_context->m_window->draw(m_backBox);
        if (m_2PlayersText) m_context->m_window->draw(*m_2PlayersText);
        if (m_aiText)       m_context->m_window->draw(*m_aiText);
        if (m_backText)     m_context->m_window->draw(*m_backText);
    }
    else
    {
        if (m_aiTitleText)    m_context->m_window->draw(*m_aiTitleText);
        if (m_colorLabelText) m_context->m_window->draw(*m_colorLabelText);
        if (m_diffLabelText)  m_context->m_window->draw(*m_diffLabelText);

        m_context->m_window->draw(m_colorBlackBox);
        m_context->m_window->draw(m_colorWhiteBox);
        m_context->m_window->draw(m_diffEasyBox);
        m_context->m_window->draw(m_diffMediumBox);
        m_context->m_window->draw(m_diffHardBox);
        m_context->m_window->draw(m_startBox);
        m_context->m_window->draw(m_aiBackBox);

        if (m_blackText)   m_context->m_window->draw(*m_blackText);
        if (m_whiteText)   m_context->m_window->draw(*m_whiteText);
        if (m_easyText)    m_context->m_window->draw(*m_easyText);
        if (m_mediumText)  m_context->m_window->draw(*m_mediumText);
        if (m_hardText)    m_context->m_window->draw(*m_hardText);
        if (m_startText)   m_context->m_window->draw(*m_startText);
        if (m_aiBackText)  m_context->m_window->draw(*m_aiBackText);
    }
}
