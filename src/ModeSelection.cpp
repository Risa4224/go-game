#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>

ModeSelection::ModeSelection(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_2PlayersBox({300.f, 200.f})
    , m_aiBox({300.f, 200.f})
    , m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Select game mode", 60)
    , m_2PlayersText(m_context->m_assets->GetFont(MAIN_FONT), "2 Players", 40)
    , m_aiText(m_context->m_assets->GetFont(MAIN_FONT), "AI Mode", 40)
    , m_returnText(m_context->m_assets->GetFont(MAIN_FONT), "Return to menu", 32)
{
}

ModeSelection::~ModeSelection() = default;

void ModeSelection::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;
    const float cy = static_cast<float>(winSize.y) * 0.5f;

    // Title
    {
        auto b = m_titleText.getLocalBounds();
        m_titleText.setOrigin(b.getCenter());
        m_titleText.setPosition({cx, cy - 220.f});
    }

    // 2 Players box
    {
        auto b = m_2PlayersBox.getLocalBounds();
        m_2PlayersBox.setOrigin(b.getCenter());
        m_2PlayersBox.setPosition({cx - 250.f, cy});
        m_2PlayersBox.setFillColor(sf::Color::Green);

        auto tb = m_2PlayersText.getLocalBounds();
        m_2PlayersText.setOrigin(tb.getCenter());
        m_2PlayersText.setPosition(m_2PlayersBox.getPosition());
        m_2PlayersText.setFillColor(sf::Color::Black);
    }

    // AI Mode box
    {
        auto b = m_aiBox.getLocalBounds();
        m_aiBox.setOrigin(b.getCenter());
        m_aiBox.setPosition({cx + 250.f, cy});
        m_aiBox.setFillColor(sf::Color::Red);

        auto tb = m_aiText.getLocalBounds();
        m_aiText.setOrigin(tb.getCenter());
        m_aiText.setPosition(m_aiBox.getPosition());
        m_aiText.setFillColor(sf::Color::Black);
    }

    // Return to menu
    {
        auto b = m_returnText.getLocalBounds();
        m_returnText.setOrigin(b.getCenter());
        m_returnText.setPosition({cx, cy + 230.f});
        m_returnText.setFillColor(sf::Color::White);
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
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
            {
                // Pop ModeSelection → quay về MainMenu
                m_context->m_states->PopCurrent();
            }
        }
    }
}

void ModeSelection::Update(sf::Time)
{
    // Chưa làm gì thêm (sau này có thể thêm chọn ô trái/phải, return…)
}

void ModeSelection::Draw()
{
    m_context->m_window->clear({210, 164, 80});

    m_context->m_window->draw(m_titleText);
    m_context->m_window->draw(m_2PlayersBox);
    m_context->m_window->draw(m_aiBox);
    m_context->m_window->draw(m_2PlayersText);
    m_context->m_window->draw(m_aiText);
    m_context->m_window->draw(m_returnText);

    m_context->m_window->display();
}
