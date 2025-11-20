#include "PauseState.hpp"
#include <SFML/Window/Event.hpp>

PauseState::PauseState(std::shared_ptr<Context> &context, Mode mode, const std::string &message)
    : m_context{context}, m_mode{mode}, m_message{message}, m_overlay{}, m_panel{}, m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "", 40), m_messageText(m_context->m_assets->GetFont(MAIN_FONT), "", 22), m_okButtonBox{}, m_okText(m_context->m_assets->GetFont(MAIN_FONT), "", 26), m_okHovered(false)
{
}

void PauseState::Init()
{
    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winSizeF(static_cast<float>(winSize.x), static_cast<float>(winSize.y));

    m_overlay.setSize(winSizeF);
    m_overlay.setPosition({0.f, 0.f});
    m_overlay.setFillColor(sf::Color(0, 0, 0, 150));

    const sf::Vector2f panelSize{400.f, 250.f};
    m_panel.setSize(panelSize);
    auto panelBounds = m_panel.getLocalBounds();
    m_panel.setOrigin(panelBounds.getCenter());
    m_panel.setPosition({winSizeF.x * 0.5f, winSizeF.y * 0.5f});
    m_panel.setFillColor(sf::Color(240, 240, 240));
    m_panel.setOutlineThickness(2.f);
    m_panel.setOutlineColor(sf::Color::Black);

    std::string titleStr = (m_mode == Mode::Paused) ? "Paused" : "Game Over";
    m_titleText.setString(titleStr);
    auto titleBounds = m_titleText.getLocalBounds();
    m_titleText.setOrigin(titleBounds.getCenter());
    m_titleText.setPosition({m_panel.getPosition().x,
                             m_panel.getPosition().y - panelSize.y * 0.3f});
    m_titleText.setFillColor(sf::Color::Black);

    if (!m_message.empty())
    {
        m_messageText.setString(m_message);
        auto msgBounds = m_messageText.getLocalBounds();
        m_messageText.setOrigin(msgBounds.getCenter());
        m_messageText.setPosition({m_panel.getPosition().x,
                                   m_panel.getPosition().y - 10.f});
        m_messageText.setFillColor(sf::Color::Black);
    }

    m_okButtonBox.setSize({160.f, 45.f});
    auto okBounds = m_okButtonBox.getLocalBounds();
    m_okButtonBox.setOrigin(okBounds.getCenter());
    m_okButtonBox.setPosition({m_panel.getPosition().x,
                               m_panel.getPosition().y + panelSize.y * 0.25f});
    m_okButtonBox.setFillColor(sf::Color(200, 200, 200));

    std::string okLabel = (m_mode == Mode::Paused) ? "Resume" : "Restart";
    m_okText.setString(okLabel);
    auto okTextBounds = m_okText.getLocalBounds();
    m_okText.setOrigin(okTextBounds.getCenter());
    m_okText.setPosition(m_okButtonBox.getPosition());
    m_okText.setFillColor(sf::Color::Black);

    m_okHovered = false;
}

void PauseState::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
            return;
        }
        else if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
            {
                m_context->m_states->PopCurrent();
                return;
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::Enter)
            {
                if (m_mode == Mode::Paused)
                {

                    m_context->m_states->PopCurrent();
                }
                else
                {
                    m_context->m_requestBoardRestart = true;
                    m_context->m_states->PopCurrent();
                }
                return;
            }
        }
        else if (const auto *mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos(
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y));
            m_okHovered = m_okButtonBox.getGlobalBounds().contains(mousePos);
        }
        else if (const auto *mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePosF(
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y));

                if (m_okButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_mode == Mode::Paused)
                    {
                        m_context->m_states->PopCurrent();
                    }
                    else
                    {
                        m_context->m_requestBoardRestart = true;
                        m_context->m_states->PopCurrent();
                    }
                    return;
                }
            }
        }
    }
}

void PauseState::Update(sf::Time)
{
    m_okButtonBox.setFillColor(
        m_okHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
}

void PauseState::Draw()
{
    m_context->m_window->draw(m_overlay);
    m_context->m_window->draw(m_panel);
    m_context->m_window->draw(m_titleText);

    if (!m_message.empty())
        m_context->m_window->draw(m_messageText);

    m_context->m_window->draw(m_okButtonBox);
    m_context->m_window->draw(m_okText);
}
