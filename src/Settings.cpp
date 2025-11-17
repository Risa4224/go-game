#include "Settings.hpp"
#include <SFML/Window/Event.hpp>
#include <SFML/Audio/SoundSource.hpp> // cho Status nếu cần

SettingsState::SettingsState(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 60)
    , m_musicLabel(m_context->m_assets->GetFont(MAIN_FONT), "Music:", 40)
    , m_musicValue(m_context->m_assets->GetFont(MAIN_FONT), "On", 40)
    , m_backText(m_context->m_assets->GetFont(MAIN_FONT), "Back", 36)
    , m_musicHovered(false)
    , m_backHovered(false)
{
    m_musicBox.setSize({300.f, 80.f});
    m_backBox.setSize({200.f, 60.f});
}

SettingsState::~SettingsState() = default;

void SettingsState::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;
    const float cy = static_cast<float>(winSize.y) * 0.5f;

    // Title
    {
        auto b = m_titleText.getLocalBounds();
        m_titleText.setOrigin(b.getCenter());
        m_titleText.setPosition({cx, cy - 180.f});
    }

    // Music box + label + value
    {
        auto b = m_musicBox.getLocalBounds();
        m_musicBox.setOrigin(b.getCenter());
        m_musicBox.setPosition({cx, cy - 20.f});
        m_musicBox.setFillColor(sf::Color(200, 200, 200));

        auto lb = m_musicLabel.getLocalBounds();
        m_musicLabel.setOrigin(lb.getCenter());
        m_musicLabel.setPosition({cx - 80.f, cy - 20.f});

        auto vb = m_musicValue.getLocalBounds();
        m_musicValue.setOrigin(vb.getCenter());
        m_musicValue.setPosition({cx + 80.f, cy - 20.f});
    }

    // Back box + text
    {
        auto b = m_backBox.getLocalBounds();
        m_backBox.setOrigin(b.getCenter());
        m_backBox.setPosition({cx, cy + 120.f});
        m_backBox.setFillColor(sf::Color(200, 200, 200));

        auto tb = m_backText.getLocalBounds();
        m_backText.setOrigin(tb.getCenter());
        m_backText.setPosition(m_backBox.getPosition());
    }

    // Sync text "On/Off" với context
    if (m_context->m_musicEnabled)
        m_musicValue.setString("On");
    else
        m_musicValue.setString("Off");
}

void SettingsState::ProcessInput()
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
                // ESC: quay lại menu trước
                m_context->m_states->PopCurrent();
            }
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)
            };

            auto musicBounds = m_musicBox.getGlobalBounds();
            auto backBounds  = m_backBox.getGlobalBounds();

            m_musicHovered = musicBounds.contains(mousePos);
            m_backHovered  = backBounds.contains(mousePos);
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y)
                };

                auto musicBounds = m_musicBox.getGlobalBounds();
                auto backBounds  = m_backBox.getGlobalBounds();

                if (musicBounds.contains(mousePos))
                {
                    // Toggle âm nhạc
                    m_context->m_musicEnabled = !m_context->m_musicEnabled;

                    if (m_context->m_music)
                    {
                        if (m_context->m_musicEnabled)
                        {
                            m_context->m_music->setVolume(50.f);
                            if (m_context->m_music->getStatus() != sf::SoundSource::Status::Playing)
                                m_context->m_music->play();
                        }
                        else
                        {
                            m_context->m_music->setVolume(0.f);
                        }
                    }

                    // Cập nhật text
                    if (m_context->m_musicEnabled)
                        m_musicValue.setString("On");
                    else
                        m_musicValue.setString("Off");
                }
                else if (backBounds.contains(mousePos))
                {
                    // Quay lại MainMenu
                    m_context->m_states->PopCurrent();
                }
            }
        }
    }
}

void SettingsState::Update(sf::Time)
{
    // Highlight bằng màu
    if (m_musicHovered)
        m_musicBox.setFillColor(sf::Color(230, 230, 230));
    else
        m_musicBox.setFillColor(sf::Color(200, 200, 200));

    if (m_backHovered)
        m_backBox.setFillColor(sf::Color(230, 230, 230));
    else
        m_backBox.setFillColor(sf::Color(200, 200, 200));
}

void SettingsState::Draw()
{
    m_context->m_window->clear({210, 164, 80});

    m_context->m_window->draw(m_titleText);
    m_context->m_window->draw(m_musicBox);
    m_context->m_window->draw(m_backBox);
    m_context->m_window->draw(m_musicLabel);
    m_context->m_window->draw(m_musicValue);
    m_context->m_window->draw(m_backText);

    m_context->m_window->display();
}
