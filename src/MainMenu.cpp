#include "MainMenu.hpp"
#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
#include "Settings.hpp"

MainMenu::MainMenu(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_gameTitle(m_context->m_assets->GetFont(MAIN_FONT), "Go Game", 64)
    , m_playButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Play", 40)
    , m_settingsButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 40)
    , m_exitButtonText(m_context->m_assets->GetFont(MAIN_FONT), "Exit", 40)
    , m_isPlayButtonSelected(false)
    , m_isSettingsButtonSelected(false)
    , m_isExitButtonSelected(false)
    , m_isPlayButtonPressed(false)
    , m_isSettingsButtonPressed(false)
    , m_isExitButtonPressed(false)
{
    m_playButtonBox.setSize({300.f, 80.f});
    m_settingsButtonBox.setSize({300.f, 80.f});
    m_exitButtonBox.setSize({300.f, 80.f});
}

MainMenu::~MainMenu() = default;

void MainMenu::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;
    const float cy = static_cast<float>(winSize.y) * 0.5f;

    {
        auto bounds = m_gameTitle.getLocalBounds();
        m_gameTitle.setOrigin(bounds.getCenter());
        m_gameTitle.setPosition({cx, cy - 180.f});
    }

    {
        auto bounds = m_playButtonBox.getLocalBounds();
        m_playButtonBox.setOrigin(bounds.getCenter());
        m_playButtonBox.setPosition({cx, cy - 40.f});
        m_playButtonBox.setFillColor(sf::Color(200, 200, 200));

        auto tBounds = m_playButtonText.getLocalBounds();
        m_playButtonText.setOrigin(tBounds.getCenter());
        m_playButtonText.setPosition(m_playButtonBox.getPosition());
        m_playButtonText.setFillColor(sf::Color::White);
    }

    {
        auto bounds = m_settingsButtonBox.getLocalBounds();
        m_settingsButtonBox.setOrigin(bounds.getCenter());
        m_settingsButtonBox.setPosition({cx, cy + 50.f});
        m_settingsButtonBox.setFillColor(sf::Color(200, 200, 200));

        auto tBounds = m_settingsButtonText.getLocalBounds();
        m_settingsButtonText.setOrigin(tBounds.getCenter());
        m_settingsButtonText.setPosition(m_settingsButtonBox.getPosition());
        m_settingsButtonText.setFillColor(sf::Color::White);
    }

    {
        auto bounds = m_exitButtonBox.getLocalBounds();
        m_exitButtonBox.setOrigin(bounds.getCenter());
        m_exitButtonBox.setPosition({cx, cy + 140.f});
        m_exitButtonBox.setFillColor(sf::Color(200, 200, 200));

        auto tBounds = m_exitButtonText.getLocalBounds();
        m_exitButtonText.setOrigin(tBounds.getCenter());
        m_exitButtonText.setPosition(m_exitButtonBox.getPosition());
        m_exitButtonText.setFillColor(sf::Color::White);
    }
}

void MainMenu::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)
            };

            auto playBounds     = m_playButtonBox.getGlobalBounds();
            auto settingsBounds = m_settingsButtonBox.getGlobalBounds();
            auto exitBounds     = m_exitButtonBox.getGlobalBounds();

            // reset selection
            m_isPlayButtonSelected     = false;
            m_isSettingsButtonSelected = false;
            m_isExitButtonSelected     = false;

            if (playBounds.contains(mousePos))
            {
                m_isPlayButtonSelected = true;
            }
            else if (settingsBounds.contains(mousePos))
            {
                m_isSettingsButtonSelected = true;
            }
            else if (exitBounds.contains(mousePos))
            {
                m_isExitButtonSelected = true;
            }
        }
        else if (const auto* mouseButton = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseButton->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseButton->position.x),
                    static_cast<float>(mouseButton->position.y)
                };

                auto playBounds     = m_playButtonBox.getGlobalBounds();
                auto settingsBounds = m_settingsButtonBox.getGlobalBounds();
                auto exitBounds     = m_exitButtonBox.getGlobalBounds();

                m_isPlayButtonPressed     = false;
                m_isSettingsButtonPressed = false;
                m_isExitButtonPressed     = false;

                if (playBounds.contains(mousePos))
                {
                    m_isPlayButtonPressed = true;
                }
                else if (settingsBounds.contains(mousePos))
                {
                    m_isSettingsButtonPressed = true;
                }
                else if (exitBounds.contains(mousePos))
                {
                    m_isExitButtonPressed = true;
                }
            }
        }
    }
}

void MainMenu::Update(sf::Time)
{
    if (m_isPlayButtonSelected)
    {
        m_playButtonText.setFillColor(sf::Color::Yellow);
        m_settingsButtonText.setFillColor(sf::Color::White);
        m_exitButtonText.setFillColor(sf::Color::White);
    }
    else if (m_isSettingsButtonSelected)
    {
        m_playButtonText.setFillColor(sf::Color::White);
        m_settingsButtonText.setFillColor(sf::Color::Yellow);
        m_exitButtonText.setFillColor(sf::Color::White);
    }
    else if (m_isExitButtonSelected)
    {
        m_playButtonText.setFillColor(sf::Color::White);
        m_settingsButtonText.setFillColor(sf::Color::White);
        m_exitButtonText.setFillColor(sf::Color::Yellow);
    }
    else
    {
        m_playButtonText.setFillColor(sf::Color::White);
        m_settingsButtonText.setFillColor(sf::Color::White);
        m_exitButtonText.setFillColor(sf::Color::White);
    }
    if (m_isPlayButtonPressed)
    {
        m_context->m_states->Add(std::make_unique<ModeSelection>(m_context), false);
        m_isPlayButtonPressed = false;
    }

    if (m_isSettingsButtonPressed)
    {
        m_context->m_states->Add(std::make_unique<SettingsState>(m_context), false);
        m_isSettingsButtonPressed = false;
    }

    if (m_isExitButtonPressed)
    {
        m_context->m_window->close();
        m_isExitButtonPressed = false;
    }
}

void MainMenu::Draw()
{
    m_context->m_window->clear({210, 164, 80});

    m_context->m_window->draw(m_gameTitle);

    m_context->m_window->draw(m_playButtonBox);
    m_context->m_window->draw(m_settingsButtonBox);
    m_context->m_window->draw(m_exitButtonBox);

    m_context->m_window->draw(m_playButtonText);
    m_context->m_window->draw(m_settingsButtonText);
    m_context->m_window->draw(m_exitButtonText);

    m_context->m_window->display();
}
