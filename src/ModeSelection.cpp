#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
#include <iostream>

ModeSelection::ModeSelection(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_2PlayersBox{{260.f, 120.f}}
    , m_aiBox{{260.f, 120.f}}
    , m_backBox{{260.f, 80.f}}
    , m_titleText(std::nullopt)
    , m_2PlayersText(std::nullopt)
    , m_aiText(std::nullopt)
    , m_backText(std::nullopt)
{ }

ModeSelection::~ModeSelection() = default;

void ModeSelection::Init()
{
    auto& font = m_context->m_assets->GetFont(MAIN_FONT);

    m_titleText.emplace(font, "Select Game Mode", 42);
    m_2PlayersText.emplace(font, "2 Players", 32);
    m_aiText.emplace(font, "AI vs Player", 32);
    m_backText.emplace(font, "Back to Menu", 24);

    const auto win = m_context->m_window->getSize();
    const float cx = static_cast<float>(win.x) * 0.5f;
    const float cy = static_cast<float>(win.y) * 0.5f;

    
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
                m_context->m_states->PopCurrent();
            }
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)
            };

            auto twoBounds  = m_2PlayersBox.getGlobalBounds();
            auto aiBounds   = m_aiBox.getGlobalBounds();
            auto backBounds = m_backBox.getGlobalBounds();

            m_2PlayersHovered = twoBounds.contains(mousePos);
            m_aiHovered       = aiBounds.contains(mousePos);
            m_backHovered     = backBounds.contains(mousePos);
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y)
                };

                auto twoBounds  = m_2PlayersBox.getGlobalBounds();
                auto aiBounds   = m_aiBox.getGlobalBounds();
                auto backBounds = m_backBox.getGlobalBounds();

                if (twoBounds.contains(mousePos))
                {
                    m_context->m_gameMode = GameMode::TwoPlayers;
                    m_context->m_states->Add(std::make_unique<MainBoard>(m_context), false);
                    std::cout<<"2 Players State added\n";
                }
                else if (aiBounds.contains(mousePos))
                {
                    m_context->m_gameMode = GameMode::AiVsPlayer;
                    m_context->m_states->Add(std::make_unique<MainBoard>(m_context), false);
                }
                else if (backBounds.contains(mousePos))
                {
                    m_context->m_states->PopCurrent();
                }
            }
        }
    }
}

void ModeSelection::Update(sf::Time)
{
    m_2PlayersBox.setFillColor(m_2PlayersHovered ? sf::Color(230, 230, 230)
                                                 : sf::Color(200, 200, 200));
    m_aiBox.setFillColor(m_aiHovered ? sf::Color(230, 230, 230)
                                     : sf::Color(200, 200, 200));
    m_backBox.setFillColor(m_backHovered ? sf::Color(230, 230, 230)
                                         : sf::Color(200, 200, 200));
}

void ModeSelection::Draw()
{

    if (m_titleText)    m_context->m_window->draw(*m_titleText);
    m_context->m_window->draw(m_2PlayersBox);
    m_context->m_window->draw(m_aiBox);
    m_context->m_window->draw(m_backBox);
    if (m_2PlayersText) m_context->m_window->draw(*m_2PlayersText);
    if (m_aiText)       m_context->m_window->draw(*m_aiText);
    if (m_backText)     m_context->m_window->draw(*m_backText);

}
