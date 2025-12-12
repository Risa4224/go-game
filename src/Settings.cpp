#include "Settings.hpp"

#include <SFML/Audio/SoundSource.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
constexpr float kRowHeight = 80.f;
constexpr float kRowGap    = 100.f; // distance between row centers
} // namespace

SettingsState::SettingsState(std::shared_ptr<Context>& context)
    : m_context{context},
      m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 60),
      m_volumeLabel(m_context->m_assets->GetFont(MAIN_FONT), "Volume:", 40),
      m_volumePercent(m_context->m_assets->GetFont(MAIN_FONT), "100%", 40),
      m_themeLabel(m_context->m_assets->GetFont(MAIN_FONT), "Theme:", 40),
      m_themeValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),
      m_backText(m_context->m_assets->GetFont(MAIN_FONT), "Back", 36)
{
    // Boxes
    m_volumeBox.setSize({420.f, kRowHeight});
    m_themeBox.setSize({420.f, kRowHeight});
    m_backBox.setSize({200.f, 60.f});

    // Slider visuals
    m_volumeTrack.setSize({160.f, 8.f});
    m_volumeTrack.setFillColor(sf::Color(150, 150, 150));

    m_volumeFill.setSize({0.f, 8.f});
    m_volumeFill.setFillColor(sf::Color(80, 80, 80));

    m_volumeKnob.setRadius(10.f);
    m_volumeKnob.setFillColor(sf::Color(245, 245, 245));
    m_volumeKnob.setOutlineThickness(2.f);
    m_volumeKnob.setOutlineColor(sf::Color(60, 60, 60));

    // Text color for light boxes
    m_volumeLabel.setFillColor(sf::Color::Black);
    m_volumePercent.setFillColor(sf::Color::Black);
    m_themeLabel.setFillColor(sf::Color::Black);
    m_themeValue.setFillColor(sf::Color::Black);
    m_backText.setFillColor(sf::Color::Black);

    // Title is usually on a dark background
    m_titleText.setFillColor(sf::Color::White);
}

SettingsState::~SettingsState() = default;

float SettingsState::VolumeFromMouseX(float mouseX) const
{
    const float left  = m_volumeTrack.getPosition().x;
    const float width = m_volumeTrack.getSize().x;
    if (width <= 0.f)
        return 0.f;

    const float t = std::clamp((mouseX - left) / width, 0.f, 1.f);
    return t * 100.f;
}

void SettingsState::LayoutVolumeRow()
{
    const float padding = 18.f;
    const float gap     = 14.f;

    const auto boxB = m_volumeBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    // Label (left, inside box)
    {
        const auto b = m_volumeLabel.getLocalBounds();
        m_volumeLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_volumeLabel.setPosition({left + padding, midY});
    }

    // Percent (right, inside box)
    {
        const auto b = m_volumePercent.getLocalBounds();
        m_volumePercent.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_volumePercent.setPosition({right - padding, midY});
    }

    // Free space for slider between label and percent
    const float sliderLeft  = m_volumeLabel.getGlobalBounds().position.x + m_volumeLabel.getGlobalBounds().size.x + gap;
    const float sliderRight = m_volumePercent.getGlobalBounds().position.x - gap;
    const float trackW      = std::max(90.f, sliderRight - sliderLeft);

    // Track
    m_volumeTrack.setSize({trackW, 8.f});
    m_volumeTrack.setOrigin({0.f, m_volumeTrack.getSize().y * 0.5f});
    m_volumeTrack.setPosition({sliderLeft, midY});

    // Fill + knob
    const float t = std::clamp(m_volume / 100.f, 0.f, 1.f);

    m_volumeFill.setSize({trackW * t, 8.f});
    m_volumeFill.setOrigin({0.f, m_volumeFill.getSize().y * 0.5f});
    m_volumeFill.setPosition({sliderLeft, midY});

    const float r = m_volumeKnob.getRadius();
    m_volumeKnob.setOrigin({r, r});
    m_volumeKnob.setPosition({sliderLeft + trackW * t, midY});
}

void SettingsState::LayoutThemeRow()
{
    const float padding = 18.f;

    const auto boxB = m_themeBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    // Label left
    {
        const auto b = m_themeLabel.getLocalBounds();
        m_themeLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_themeLabel.setPosition({left + padding, midY});
    }

    // Value right
    {
        const auto b = m_themeValue.getLocalBounds();
        m_themeValue.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_themeValue.setPosition({right - padding, midY});
    }
}

void SettingsState::RefreshThemeText()
{
    if (m_context->m_boardTheme == BoardTheme::Classic)
        m_themeValue.setString("Classic");
    else
        m_themeValue.setString("Dark");

    // String width changes => re-layout to keep right-aligned
    LayoutThemeRow();
}

void SettingsState::ApplyVolume(float volumePercent)
{
    m_volume = std::clamp(volumePercent, 0.f, 100.f);

    // Update UI text BEFORE layout (width affects available slider space)
    m_volumePercent.setString(std::to_string(static_cast<int>(m_volume + 0.5f)) + "%");

    // Update context state first (so other states can query it).
    m_context->m_musicEnabled = (m_volume > 0.f);

    if (m_context->m_music)
    {
        m_context->m_music->setVolume(m_context->m_musicEnabled ? m_volume : 0.f);

        if (m_context->m_musicEnabled)
        {
            if (m_context->m_music->getStatus() != sf::SoundSource::Status::Playing)
                m_context->m_music->play();
        }
        else
        {
            if (m_context->m_music->getStatus() == sf::SoundSource::Status::Playing)
                m_context->m_music->pause();
        }
    }

    LayoutVolumeRow();
}

void SettingsState::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;
    const float cy = static_cast<float>(winSize.y) * 0.5f;

    // Title
    {
        const auto b = m_titleText.getLocalBounds();
        m_titleText.setOrigin(b.getCenter());
        m_titleText.setPosition({cx, cy - 180.f});
    }

    // Volume row
    {
        const auto b = m_volumeBox.getLocalBounds();
        m_volumeBox.setOrigin(b.getCenter());
        m_volumeBox.setPosition({cx, cy - 20.f});
        m_volumeBox.setFillColor(sf::Color(200, 200, 200));
        m_volumeBox.setOutlineThickness(2.f);
        m_volumeBox.setOutlineColor(sf::Color(170, 170, 170));

        // Initialize from current music volume (or old on/off flag)
        float startVol = 100.f;
        if (m_context->m_music)
            startVol = m_context->m_music->getVolume();
        else
            startVol = m_context->m_musicEnabled ? 100.f : 0.f;

        ApplyVolume(startVol);
    }

    // Theme row
    {
        const auto b = m_themeBox.getLocalBounds();
        m_themeBox.setOrigin(b.getCenter());
        m_themeBox.setPosition({cx, cy - 20.f + kRowGap});
        m_themeBox.setFillColor(sf::Color(200, 200, 200));

        // First layout with the default string, then refresh to match current context
        LayoutThemeRow();
        RefreshThemeText();
    }

    // Back button
    {
        const auto b = m_backBox.getLocalBounds();
        m_backBox.setOrigin(b.getCenter());
        m_backBox.setPosition({cx, cy - 20.f + 2.f * kRowGap});
        m_backBox.setFillColor(sf::Color(200, 200, 200));

        const auto tb = m_backText.getLocalBounds();
        m_backText.setOrigin(tb.getCenter());
        m_backText.setPosition(m_backBox.getPosition());
    }
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
                m_context->m_states->PopCurrent();
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            const sf::Vector2f mousePos{
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)};

            m_volumeBoxHovered = m_volumeBox.getGlobalBounds().contains(mousePos);
            m_themeHovered     = m_themeBox.getGlobalBounds().contains(mousePos);
            m_backHovered      = m_backBox.getGlobalBounds().contains(mousePos);

            // Slider hover: knob OR an expanded track hitbox (easier to grab)
            auto knobBounds  = m_volumeKnob.getGlobalBounds();
            auto trackBounds = m_volumeTrack.getGlobalBounds();
            trackBounds.position.y -= 18.f;
            trackBounds.size.y += 36.f;
            m_volumeHovered = knobBounds.contains(mousePos) || trackBounds.contains(mousePos);

            if (m_volumeDragging)
                ApplyVolume(VolumeFromMouseX(mousePos.x));
        }
        else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            const sf::Vector2f mousePos{
                static_cast<float>(wheel->position.x),
                static_cast<float>(wheel->position.y)};

            // Scroll only when you're on the volume row
            if (m_volumeBox.getGlobalBounds().contains(mousePos))
                ApplyVolume(m_volume + wheel->delta * 5.f);
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button != sf::Mouse::Button::Left)
                continue;

            const sf::Vector2f mousePos{
                static_cast<float>(mouseBtn->position.x),
                static_cast<float>(mouseBtn->position.y)};

            // Slider: click anywhere on track/knob to set, then drag.
            auto knobBounds  = m_volumeKnob.getGlobalBounds();
            auto trackBounds = m_volumeTrack.getGlobalBounds();
            trackBounds.position.y -= 18.f;
            trackBounds.size.y += 36.f;

            if (knobBounds.contains(mousePos) || trackBounds.contains(mousePos))
            {
                m_volumeDragging = true;
                ApplyVolume(VolumeFromMouseX(mousePos.x));
            }
            else if (m_themeBox.getGlobalBounds().contains(mousePos))
            {
                m_context->m_boardTheme =
                    (m_context->m_boardTheme == BoardTheme::Classic) ? BoardTheme::Dark : BoardTheme::Classic;
                RefreshThemeText();
            }
            else if (m_backBox.getGlobalBounds().contains(mousePos))
            {
                m_context->m_states->PopCurrent();
            }
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonReleased>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
                m_volumeDragging = false;
        }
    }
}

void SettingsState::Update(sf::Time)
{
    m_volumeBox.setFillColor(m_volumeBoxHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    m_themeBox.setFillColor(m_themeHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    m_backBox.setFillColor(m_backHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));

    m_volumeKnob.setFillColor((m_volumeHovered || m_volumeDragging) ? sf::Color(255, 255, 255) : sf::Color(245, 245, 245));
}

void SettingsState::Draw()
{
    m_context->m_window->draw(m_titleText);

    // Volume row
    m_context->m_window->draw(m_volumeBox);
    m_context->m_window->draw(m_volumeLabel);
    m_context->m_window->draw(m_volumeTrack);
    m_context->m_window->draw(m_volumeFill);
    m_context->m_window->draw(m_volumeKnob);
    m_context->m_window->draw(m_volumePercent);

    // Theme row
    m_context->m_window->draw(m_themeBox);
    m_context->m_window->draw(m_themeLabel);
    m_context->m_window->draw(m_themeValue);

    // Back
    m_context->m_window->draw(m_backBox);
    m_context->m_window->draw(m_backText);
}
