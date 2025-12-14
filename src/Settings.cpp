#include "Settings.hpp"

#include <SFML/Audio/SoundSource.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
#include <optional>
#include <string>

namespace
{
constexpr float kRowHeight = 72.f;
constexpr float kRowGap    = 92.f; // distance between row centers
} // namespace

SettingsState::SettingsState(std::shared_ptr<Context>& context)
    : m_context{context},
      m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 60),
      m_volumeLabel(m_context->m_assets->GetFont(MAIN_FONT), "Volume:", 40),
      m_volumePercent(m_context->m_assets->GetFont(MAIN_FONT), "100%", 40),
      m_themeLabel(m_context->m_assets->GetFont(MAIN_FONT), "Theme:", 40),
      m_themeValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),
      m_stoneLabel(m_context->m_assets->GetFont(MAIN_FONT), "Stones:", 40),
      m_stoneValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),
      m_backText(m_context->m_assets->GetFont(MAIN_FONT), "Back", 36)
{
    // Boxes
    m_volumeBox.setSize({420.f, kRowHeight});
    m_themeBox.setSize({420.f, kRowHeight});
    m_stoneBox.setSize({420.f, kRowHeight});
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
    m_stoneLabel.setFillColor(sf::Color::Black);
    m_stoneValue.setFillColor(sf::Color::Black);
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

void SettingsState::LayoutStoneRow()
{
    const float padding = 18.f;

    const auto boxB = m_stoneBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    // Label left
    {
        const auto b = m_stoneLabel.getLocalBounds();
        m_stoneLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_stoneLabel.setPosition({left + padding, midY});
    }

    // Value right
    {
        const auto b = m_stoneValue.getLocalBounds();
        m_stoneValue.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_stoneValue.setPosition({right - padding, midY});
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

void SettingsState::RefreshStoneText()
{
    // Requires Context::m_stoneTheme + enum class StoneTheme in GameApp.h
    switch (m_context->m_stoneTheme)
    {
        case StoneTheme::Classic:    m_stoneValue.setString("Classic"); break;
        case StoneTheme::SlateShell: m_stoneValue.setString("Slate/Shell"); break;
        case StoneTheme::Glass:      m_stoneValue.setString("Glass"); break;
        default:                     m_stoneValue.setString("Classic"); break;
    }

    // String width changes => re-layout to keep right-aligned
    LayoutStoneRow();
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

    // Responsive widths (don’t look tiny or huge on different windows)
    const float rowW = std::clamp(winSize.x * 0.52f, 440.f, 560.f);

    m_volumeBox.setSize({rowW, kRowHeight});
    m_themeBox.setSize({rowW, kRowHeight});
    m_stoneBox.setSize({rowW, kRowHeight});
    m_backBox.setSize({std::min(260.f, rowW * 0.55f), 64.f});

    // Title
    {
        const auto b = m_titleText.getLocalBounds();
        m_titleText.setOrigin(b.getCenter());
        m_titleText.setPosition({cx, winSize.y * 0.18f});
    }

    // Base Y for first row (under title)
    const float startY = m_titleText.getPosition().y + 135.f;

    auto styleRowBox = [](sf::RectangleShape& box)
    {
        box.setFillColor(sf::Color(230, 230, 230));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(sf::Color(255, 255, 255, 140));
    };

    // Volume row
    {
        const auto b = m_volumeBox.getLocalBounds();
        m_volumeBox.setOrigin(b.getCenter());
        m_volumeBox.setPosition({cx, startY});
        styleRowBox(m_volumeBox);

        float startVol = 100.f;
        if (m_context->m_music)
            startVol = m_context->m_music->getVolume();
        else
            startVol = m_context->m_musicEnabled ? 100.f : 0.f;

        ApplyVolume(startVol); // also calls LayoutVolumeRow()
    }

    // Theme row
    {
        const auto b = m_themeBox.getLocalBounds();
        m_themeBox.setOrigin(b.getCenter());
        m_themeBox.setPosition({cx, startY + 1.f * kRowGap});
        styleRowBox(m_themeBox);

        LayoutThemeRow();
        RefreshThemeText();
    }

    // Stone row
    {
        const auto b = m_stoneBox.getLocalBounds();
        m_stoneBox.setOrigin(b.getCenter());
        m_stoneBox.setPosition({cx, startY + 2.f * kRowGap});
        styleRowBox(m_stoneBox);

        LayoutStoneRow();
        RefreshStoneText();
    }

    // Back button
    {
        const auto b = m_backBox.getLocalBounds();
        m_backBox.setOrigin(b.getCenter());
        m_backBox.setPosition({cx, startY + 3.f * kRowGap});
        styleRowBox(m_backBox);

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
            m_stoneHovered     = m_stoneBox.getGlobalBounds().contains(mousePos);
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
            else if (m_stoneBox.getGlobalBounds().contains(mousePos))
            {
                // Cycle stone themes
                switch (m_context->m_stoneTheme)
                {
                    case StoneTheme::Classic:    m_context->m_stoneTheme = StoneTheme::SlateShell; break;
                    case StoneTheme::SlateShell: m_context->m_stoneTheme = StoneTheme::Glass; break;
                    case StoneTheme::Glass:      m_context->m_stoneTheme = StoneTheme::Classic; break;
                    default:                     m_context->m_stoneTheme = StoneTheme::Classic; break;
                }
                RefreshStoneText();
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
    const sf::Color boxNormal(230, 230, 230);
    const sf::Color boxHover(245, 245, 245);
    const sf::Color outlineNormal(255, 255, 255, 140);
    const sf::Color outlineHover(255, 255, 255, 220);

    auto applyHover = [&](sf::RectangleShape& box, bool hovered)
    {
        box.setFillColor(hovered ? boxHover : boxNormal);
        box.setOutlineColor(hovered ? outlineHover : outlineNormal);
    };

    applyHover(m_volumeBox, m_volumeBoxHovered);
    applyHover(m_themeBox,  m_themeHovered);
    applyHover(m_stoneBox,  m_stoneHovered);
    applyHover(m_backBox,   m_backHovered);

    // Slider knob highlight
    m_volumeKnob.setFillColor((m_volumeHovered || m_volumeDragging)
        ? sf::Color(255, 255, 255)
        : sf::Color(245, 245, 245));
}

void SettingsState::Draw()
{
    // Small helper: draw a soft shadow behind a box
    auto drawShadow = [&](const sf::RectangleShape& box)
    {
        sf::RectangleShape sh = box;
        sh.setFillColor(sf::Color(0, 0, 0, 40)); // shadow alpha
        sh.setOutlineThickness(0.f);
        sh.move({0.f, 5.f}); // slight drop
        m_context->m_window->draw(sh);
    };

    // Title
    m_context->m_window->draw(m_titleText);

    // ----- Volume row -----
    drawShadow(m_volumeBox);
    m_context->m_window->draw(m_volumeBox);
    m_context->m_window->draw(m_volumeLabel);
    m_context->m_window->draw(m_volumeTrack);
    m_context->m_window->draw(m_volumeFill);
    m_context->m_window->draw(m_volumeKnob);
    m_context->m_window->draw(m_volumePercent);

    // ----- Theme row -----
    drawShadow(m_themeBox);
    m_context->m_window->draw(m_themeBox);
    m_context->m_window->draw(m_themeLabel);
    m_context->m_window->draw(m_themeValue);

    // ----- Stone theme row -----
    drawShadow(m_stoneBox);
    m_context->m_window->draw(m_stoneBox);
    m_context->m_window->draw(m_stoneLabel);
    m_context->m_window->draw(m_stoneValue);

    // ----- Back -----
    drawShadow(m_backBox);
    m_context->m_window->draw(m_backBox);
    m_context->m_window->draw(m_backText);
}
