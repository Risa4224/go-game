#include "EndGameState.hpp"
#include <SFML/Window/Event.hpp>

namespace
{
    static sf::Vector2f centerOf(const sf::RectangleShape& r)
    {
        return r.getPosition() + r.getSize() * 0.5f;
    }

    static void centerText(sf::Text& t, sf::Vector2f pos)
    {
        auto b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
        t.setPosition(pos);
    }
}

EndGameState::EndGameState(std::shared_ptr<Context>& context, const std::string& msg)
    : m_context(context), m_msg(msg)
{
}

void EndGameState::Init()
{
    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winF((float)winSize.x, (float)winSize.y);

    // --- dim overlay (keep board visible) ---
    m_dim.setSize(winF);
    m_dim.setPosition({0.f, 0.f});
    m_dim.setFillColor(sf::Color(0, 0, 0, 90)); // lighter than PauseState so the board is still visible

    // --- mini panel ---
    const sf::Vector2f panelSize{420.f, 260.f};
    m_panel.setSize(panelSize);
    m_panel.setPosition({(winF.x - panelSize.x) * 0.5f, (winF.y - panelSize.y) * 0.5f});
    m_panel.setFillColor(sf::Color(245, 245, 245, 245));
    m_panel.setOutlineThickness(2.f);
    m_panel.setOutlineColor(sf::Color::Black);

    const auto& font = m_context->m_assets->GetFont(MAIN_FONT);

    // --- texts (SFML3: use emplace) ---
    m_titleText.emplace(font, "End Game", 30);
    m_titleText->setFillColor(sf::Color::Black);

    m_msgText.emplace(font, m_msg, 18);
    m_msgText->setFillColor(sf::Color::Black);

    m_restartText.emplace(font, "Restart", 20);
    m_restartText->setFillColor(sf::Color::Black);

    m_closeText.emplace(font, "Close", 20);
    m_closeText->setFillColor(sf::Color::Black);

    // position texts
    const sf::Vector2f panelPos = m_panel.getPosition();

    centerText(*m_titleText, {panelPos.x + panelSize.x * 0.5f, panelPos.y + 45.f});
    centerText(*m_msgText, {panelPos.x + panelSize.x * 0.5f, panelPos.y + 110.f});

    // --- buttons ---
    const sf::Vector2f btnSize{150.f, 44.f};
    const float btnY = panelPos.y + panelSize.y - 70.f;

    m_restartBtn.setSize(btnSize);
    m_restartBtn.setPosition({panelPos.x + 45.f, btnY});
    m_restartBtn.setFillColor(sf::Color(200, 200, 200));
    m_restartBtn.setOutlineThickness(1.f);
    m_restartBtn.setOutlineColor(sf::Color(120, 120, 120));

    m_closeBtn.setSize(btnSize);
    m_closeBtn.setPosition({panelPos.x + panelSize.x - 45.f - btnSize.x, btnY});
    m_closeBtn.setFillColor(sf::Color(200, 200, 200));
    m_closeBtn.setOutlineThickness(1.f);
    m_closeBtn.setOutlineColor(sf::Color(120, 120, 120));

    centerText(*m_restartText, centerOf(m_restartBtn));
    centerText(*m_closeText, centerOf(m_closeBtn));

    m_restartHover = false;
    m_closeHover = false;
}

void EndGameState::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
            return;
        }

        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->scancode == sf::Keyboard::Scancode::Escape)
            {
                m_context->m_states->PopCurrent();
                return;
            }
            if (key->scancode == sf::Keyboard::Scancode::Enter)
            {
                // Enter = Restart (common UX)
                m_context->m_requestBoardRestart = true;
                m_context->m_states->PopCurrent();
                return;
            }
        }
        else if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mp((float)mm->position.x, (float)mm->position.y);
            m_restartHover = m_restartBtn.getGlobalBounds().contains(mp);
            m_closeHover = m_closeBtn.getGlobalBounds().contains(mp);
        }
        else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mb->button != sf::Mouse::Button::Left)
                continue;

            sf::Vector2f mp((float)mb->position.x, (float)mb->position.y);

            if (m_restartBtn.getGlobalBounds().contains(mp))
            {
                m_context->m_requestBoardRestart = true;
                m_context->m_states->PopCurrent();
                return;
            }
            if (m_closeBtn.getGlobalBounds().contains(mp))
            {
                m_context->m_states->PopCurrent();
                return;
            }

            // Click outside panel = close (optional nice behavior)
            if (!m_panel.getGlobalBounds().contains(mp))
            {
                m_context->m_states->PopCurrent();
                return;
            }
        }
    }
}

void EndGameState::Update(sf::Time)
{
    // hover colors
    m_restartBtn.setFillColor(m_restartHover ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    m_closeBtn.setFillColor(m_closeHover ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
}

void EndGameState::Draw()
{
    // Draw a small overlay without hiding the board
    m_context->m_window->draw(m_dim);

    // tiny shadow
    sf::RectangleShape shadow = m_panel;
    shadow.move({3.f, 3.f});
    shadow.setFillColor(sf::Color(0, 0, 0, 90));
    shadow.setOutlineThickness(0.f);
    m_context->m_window->draw(shadow);

    m_context->m_window->draw(m_panel);

    if (m_titleText)
        m_context->m_window->draw(*m_titleText);
    if (m_msgText)
        m_context->m_window->draw(*m_msgText);

    m_context->m_window->draw(m_restartBtn);
    m_context->m_window->draw(m_closeBtn);

    if (m_restartText)
        m_context->m_window->draw(*m_restartText);
    if (m_closeText)
        m_context->m_window->draw(*m_closeText);
}
