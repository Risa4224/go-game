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

// --- Music file locations ---
// IMPORTANT: Keep this in sync with GameApp.cpp so both use the same BGM tracks.
static std::string MusicFileForTrack(MusicTrack t)
{
    switch (t)
    {
        case MusicTrack::Classic: return "assets/audio/background.mp3";
        case MusicTrack::Ambient: return "assets/audio/background_2.mp3"; // Modern
        case MusicTrack::Retro:   return "assets/audio/background_2.mp3"; // legacy -> Modern
        case MusicTrack::Off:     return "";
        default:                  return "assets/audio/background.mp3";
    }
}

static const char* MusicTrackToString(MusicTrack t)
{
    switch (t)
    {
        case MusicTrack::Off:     return "Off";
        case MusicTrack::Classic: return "Classic";
        case MusicTrack::Ambient: return "Modern";
        case MusicTrack::Retro:   return "Modern"; // legacy
        default:                  return "Classic";
    }
}

} // namespace

SettingsState::SettingsState(std::shared_ptr<Context>& context)
    : m_context{context},
      m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Settings", 60),

      // Music
      m_musicVolLabel(m_context->m_assets->GetFont(MAIN_FONT), "Music Volume:", 36),
      m_musicVolPercent(m_context->m_assets->GetFont(MAIN_FONT), "100%", 36),
      m_musicTrackLabel(m_context->m_assets->GetFont(MAIN_FONT), "Music:", 40),
      m_musicTrackValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),

      // SFX
      m_sfxToggleLabel(m_context->m_assets->GetFont(MAIN_FONT), "SFX:", 40),
      m_sfxToggleValue(m_context->m_assets->GetFont(MAIN_FONT), "On", 40),
      m_sfxVolLabel(m_context->m_assets->GetFont(MAIN_FONT), "SFX Volume:", 36),
      m_sfxVolPercent(m_context->m_assets->GetFont(MAIN_FONT), "100%", 36),

      // Existing visuals
      m_themeLabel(m_context->m_assets->GetFont(MAIN_FONT), "Theme:", 40),
      m_themeValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),
      m_stoneLabel(m_context->m_assets->GetFont(MAIN_FONT), "Stones:", 40),
      m_stoneValue(m_context->m_assets->GetFont(MAIN_FONT), "Classic", 40),
      m_backText(m_context->m_assets->GetFont(MAIN_FONT), "Back", 36)
{
    // Boxes (default sizes; resized in Init())
    m_musicVolBox.setSize({420.f, kRowHeight});
    m_musicTrackBox.setSize({420.f, kRowHeight});
    m_sfxToggleBox.setSize({420.f, kRowHeight});
    m_sfxVolBox.setSize({420.f, kRowHeight});
    m_themeBox.setSize({420.f, kRowHeight});
    m_stoneBox.setSize({420.f, kRowHeight});
    m_backBox.setSize({200.f, 60.f});

    // Slider visuals (music)
    m_musicVolTrack.setSize({160.f, 8.f});
    m_musicVolTrack.setFillColor(sf::Color(150, 150, 150));

    m_musicVolFill.setSize({0.f, 8.f});
    m_musicVolFill.setFillColor(sf::Color(80, 80, 80));

    m_musicVolKnob.setRadius(10.f);
    m_musicVolKnob.setFillColor(sf::Color(245, 245, 245));
    m_musicVolKnob.setOutlineThickness(2.f);
    m_musicVolKnob.setOutlineColor(sf::Color(60, 60, 60));

    // Slider visuals (sfx)
    m_sfxVolTrack.setSize({160.f, 8.f});
    m_sfxVolTrack.setFillColor(sf::Color(150, 150, 150));

    m_sfxVolFill.setSize({0.f, 8.f});
    m_sfxVolFill.setFillColor(sf::Color(80, 80, 80));

    m_sfxVolKnob.setRadius(10.f);
    m_sfxVolKnob.setFillColor(sf::Color(245, 245, 245));
    m_sfxVolKnob.setOutlineThickness(2.f);
    m_sfxVolKnob.setOutlineColor(sf::Color(60, 60, 60));

    // Text color for light boxes
    m_musicVolLabel.setFillColor(sf::Color::Black);
    m_musicVolPercent.setFillColor(sf::Color::Black);
    m_musicTrackLabel.setFillColor(sf::Color::Black);
    m_musicTrackValue.setFillColor(sf::Color::Black);

    m_sfxToggleLabel.setFillColor(sf::Color::Black);
    m_sfxToggleValue.setFillColor(sf::Color::Black);
    m_sfxVolLabel.setFillColor(sf::Color::Black);
    m_sfxVolPercent.setFillColor(sf::Color::Black);

    m_themeLabel.setFillColor(sf::Color::Black);
    m_themeValue.setFillColor(sf::Color::Black);
    m_stoneLabel.setFillColor(sf::Color::Black);
    m_stoneValue.setFillColor(sf::Color::Black);
    m_backText.setFillColor(sf::Color::Black);

    // Title is usually on a dark background
    m_titleText.setFillColor(sf::Color::White);
}

// ------------------------------
// SFX enable/disable row
// ------------------------------

void SettingsState::LayoutSfxToggleRow()
{
    const float padding = 18.f;

    const auto boxB = m_sfxToggleBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    {
        const auto b = m_sfxToggleLabel.getLocalBounds();
        m_sfxToggleLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_sfxToggleLabel.setPosition({left + padding, midY});
    }

    {
        const auto b = m_sfxToggleValue.getLocalBounds();
        m_sfxToggleValue.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_sfxToggleValue.setPosition({right - padding, midY});
    }
}

void SettingsState::RefreshSfxToggleText()
{
    m_sfxToggleValue.setString(m_context->m_sfxEnabled ? "On" : "Off");
    LayoutSfxToggleRow();
}

void SettingsState::ToggleSfxEnabled()
{
    m_context->m_sfxEnabled = !m_context->m_sfxEnabled;

    // If enabling from zero, give it a sensible default so users actually hear something.
    if (m_context->m_sfxEnabled && m_context->m_sfxVolume <= 0.f)
    {
        m_context->m_sfxVolume = 50.f;
        m_sfxVol = 50.f;
        m_sfxVolPercent.setString("50%");
        LayoutSfxVolumeRow();
    }

    RefreshSfxToggleText();
}

SettingsState::~SettingsState() = default;

float SettingsState::VolumeFromMouseX(float mouseX, const sf::RectangleShape& track) const
{
    const float left  = track.getPosition().x;
    const float width = track.getSize().x;
    if (width <= 0.f)
        return 0.f;

    const float t = std::clamp((mouseX - left) / width, 0.f, 1.f);
    return t * 100.f;
}

// ------------------------------
// Music volume slider
// ------------------------------

void SettingsState::LayoutMusicVolumeRow()
{
    const float padding = 18.f;
    const float gap     = 14.f;

    const auto boxB = m_musicVolBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    // Label (left, inside box)
    {
        const auto b = m_musicVolLabel.getLocalBounds();
        m_musicVolLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_musicVolLabel.setPosition({left + padding, midY});
    }

    // Percent (right, inside box)
    {
        const auto b = m_musicVolPercent.getLocalBounds();
        m_musicVolPercent.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_musicVolPercent.setPosition({right - padding, midY});
    }

    const float sliderLeft  = m_musicVolLabel.getGlobalBounds().position.x + m_musicVolLabel.getGlobalBounds().size.x + gap;
    const float sliderRight = m_musicVolPercent.getGlobalBounds().position.x - gap;
    const float trackW      = std::max(90.f, sliderRight - sliderLeft);

    m_musicVolTrack.setSize({trackW, 8.f});
    m_musicVolTrack.setOrigin({0.f, m_musicVolTrack.getSize().y * 0.5f});
    m_musicVolTrack.setPosition({sliderLeft, midY});

    const float t = std::clamp(m_musicVol / 100.f, 0.f, 1.f);

    m_musicVolFill.setSize({trackW * t, 8.f});
    m_musicVolFill.setOrigin({0.f, m_musicVolFill.getSize().y * 0.5f});
    m_musicVolFill.setPosition({sliderLeft, midY});

    const float r = m_musicVolKnob.getRadius();
    m_musicVolKnob.setOrigin({r, r});
    m_musicVolKnob.setPosition({sliderLeft + trackW * t, midY});
}

void SettingsState::ApplyMusicVolume(float volumePercent)
{
    m_musicVol = std::clamp(volumePercent, 0.f, 100.f);
    m_musicVolPercent.setString(std::to_string(static_cast<int>(m_musicVol + 0.5f)) + "%");

    // Persist to Context (add these fields in Context)
    m_context->m_musicVolume = m_musicVol;

    const bool musicOn = (m_context->m_musicTrack != MusicTrack::Off);
    m_context->m_musicEnabled = musicOn;

    if (m_context->m_music)
    {
        m_context->m_music->setVolume(musicOn ? m_musicVol : 0.f);

        const bool shouldPlay = musicOn && (m_musicVol > 0.f);

        if (shouldPlay)
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

    LayoutMusicVolumeRow();
}

// ------------------------------
// Music track row
// ------------------------------

void SettingsState::LayoutMusicTrackRow()
{
    const float padding = 18.f;

    const auto boxB = m_musicTrackBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    {
        const auto b = m_musicTrackLabel.getLocalBounds();
        m_musicTrackLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_musicTrackLabel.setPosition({left + padding, midY});
    }

    {
        const auto b = m_musicTrackValue.getLocalBounds();
        m_musicTrackValue.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_musicTrackValue.setPosition({right - padding, midY});
    }
}

void SettingsState::RefreshMusicTrackText()
{
    m_musicTrackValue.setString(MusicTrackToString(m_context->m_musicTrack));
    LayoutMusicTrackRow();
}

void SettingsState::CycleMusicTrack()
{
    // Only 2 themes: Classic + Modern (stored as MusicTrack::Ambient for compatibility)
    switch (m_context->m_musicTrack)
    {
        case MusicTrack::Off:     m_context->m_musicTrack = MusicTrack::Classic; break;
        case MusicTrack::Classic: m_context->m_musicTrack = MusicTrack::Ambient; break; // Modern
        case MusicTrack::Ambient: m_context->m_musicTrack = MusicTrack::Off; break;
        // If some older save/config ever sets Retro, fold it back into the 2-theme cycle:
        case MusicTrack::Retro:   m_context->m_musicTrack = MusicTrack::Off; break;
        default:                  m_context->m_musicTrack = MusicTrack::Classic; break;
    }

    RefreshMusicTrackText();

    const bool musicOn = (m_context->m_musicTrack != MusicTrack::Off);
    m_context->m_musicEnabled = musicOn;

    if (!m_context->m_music)
        return;

    if (!musicOn)
    {
        if (m_context->m_music->getStatus() == sf::SoundSource::Status::Playing)
            m_context->m_music->pause();
        return;
    }

    // Try to load the selected track. Update MusicFileForTrack() above.
    const std::string file = MusicFileForTrack(m_context->m_musicTrack);
    if (!file.empty())
    {
        m_context->m_music->stop();
        (void)m_context->m_music->openFromFile(file);
        // SFML 3: looping is controlled via SoundStream::setLooping (sf::Music inherits SoundStream)
        m_context->m_music->setLooping(true);
    }

    ApplyMusicVolume(m_musicVol);
}

// ------------------------------
// SFX toggle row (On/Off)
// ------------------------------

// ------------------------------
// SFX volume slider
// ------------------------------

void SettingsState::LayoutSfxVolumeRow()
{
    const float padding = 18.f;
    const float gap     = 14.f;

    const auto boxB = m_sfxVolBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    {
        const auto b = m_sfxVolLabel.getLocalBounds();
        m_sfxVolLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_sfxVolLabel.setPosition({left + padding, midY});
    }

    {
        const auto b = m_sfxVolPercent.getLocalBounds();
        m_sfxVolPercent.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_sfxVolPercent.setPosition({right - padding, midY});
    }

    const float sliderLeft  = m_sfxVolLabel.getGlobalBounds().position.x + m_sfxVolLabel.getGlobalBounds().size.x + gap;
    const float sliderRight = m_sfxVolPercent.getGlobalBounds().position.x - gap;
    const float trackW      = std::max(90.f, sliderRight - sliderLeft);

    m_sfxVolTrack.setSize({trackW, 8.f});
    m_sfxVolTrack.setOrigin({0.f, m_sfxVolTrack.getSize().y * 0.5f});
    m_sfxVolTrack.setPosition({sliderLeft, midY});

    const float t = std::clamp(m_sfxVol / 100.f, 0.f, 1.f);

    m_sfxVolFill.setSize({trackW * t, 8.f});
    m_sfxVolFill.setOrigin({0.f, m_sfxVolFill.getSize().y * 0.5f});
    m_sfxVolFill.setPosition({sliderLeft, midY});

    const float r = m_sfxVolKnob.getRadius();
    m_sfxVolKnob.setOrigin({r, r});
    m_sfxVolKnob.setPosition({sliderLeft + trackW * t, midY});
}

void SettingsState::ApplySfxVolume(float volumePercent)
{
    m_sfxVol = std::clamp(volumePercent, 0.f, 100.f);
    m_sfxVolPercent.setString(std::to_string(static_cast<int>(m_sfxVol + 0.5f)) + "%");

    // Persist to Context (add these fields in Context)
    m_context->m_sfxVolume = m_sfxVol;

    // Explicit toggle: slider can also auto-disable at 0 and auto-enable when > 0.
    if (m_sfxVol <= 0.f)
        m_context->m_sfxEnabled = false;
    else
        m_context->m_sfxEnabled = true;

    RefreshSfxToggleText();

    LayoutSfxVolumeRow();
}

// ------------------------------
// Existing theme/stone rows
// ------------------------------

void SettingsState::LayoutThemeRow()
{
    const float padding = 18.f;

    const auto boxB = m_themeBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    {
        const auto b = m_themeLabel.getLocalBounds();
        m_themeLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_themeLabel.setPosition({left + padding, midY});
    }

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

    LayoutThemeRow();
}

void SettingsState::LayoutStoneRow()
{
    const float padding = 18.f;

    const auto boxB = m_stoneBox.getGlobalBounds();
    const float left  = boxB.position.x;
    const float right = boxB.position.x + boxB.size.x;
    const float midY  = boxB.position.y + boxB.size.y * 0.5f;

    {
        const auto b = m_stoneLabel.getLocalBounds();
        m_stoneLabel.setOrigin({b.position.x, b.position.y + b.size.y * 0.5f});
        m_stoneLabel.setPosition({left + padding, midY});
    }

    {
        const auto b = m_stoneValue.getLocalBounds();
        m_stoneValue.setOrigin({b.position.x + b.size.x, b.position.y + b.size.y * 0.5f});
        m_stoneValue.setPosition({right - padding, midY});
    }
}

void SettingsState::RefreshStoneText()
{
    switch (m_context->m_stoneTheme)
    {
        case StoneTheme::Classic:    m_stoneValue.setString("Classic"); break;
        case StoneTheme::SlateShell: m_stoneValue.setString("Slate/Shell"); break;
        case StoneTheme::Glass:      m_stoneValue.setString("Glass"); break;
        default:                     m_stoneValue.setString("Classic"); break;
    }

    LayoutStoneRow();
}

// ------------------------------
// State lifecycle
// ------------------------------

void SettingsState::Init()
{
    const auto winSize = m_context->m_window->getSize();
    const float cx = static_cast<float>(winSize.x) * 0.5f;

    const float rowW = std::clamp(winSize.x * 0.52f, 440.f, 600.f);

    m_musicVolBox.setSize({rowW, kRowHeight});
    m_musicTrackBox.setSize({rowW, kRowHeight});
    m_sfxToggleBox.setSize({rowW, kRowHeight});
    m_sfxVolBox.setSize({rowW, kRowHeight});
    m_themeBox.setSize({rowW, kRowHeight});
    m_stoneBox.setSize({rowW, kRowHeight});
    m_backBox.setSize({std::min(260.f, rowW * 0.55f), 64.f});

    // Title
    {
        const auto b = m_titleText.getLocalBounds();
        m_titleText.setOrigin(b.getCenter());
        m_titleText.setPosition({cx, winSize.y * 0.12f});
    }

    const float startY = m_titleText.getPosition().y + 80.f;

    auto styleRowBox = [](sf::RectangleShape& box)
    {
        box.setFillColor(sf::Color(230, 230, 230));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(sf::Color(255, 255, 255, 140));
    };

    // 0) Music Volume
    {
        const auto b = m_musicVolBox.getLocalBounds();
        m_musicVolBox.setOrigin(b.getCenter());
        m_musicVolBox.setPosition({cx, startY + 0.f * kRowGap});
        styleRowBox(m_musicVolBox);

        float startVol = 100.f;
        if (m_context->m_music)
            startVol = m_context->m_music->getVolume();
        else
            startVol = m_context->m_musicVolume;

        ApplyMusicVolume(startVol);
    }

    // 1) Music Track
    {
        const auto b = m_musicTrackBox.getLocalBounds();
        m_musicTrackBox.setOrigin(b.getCenter());
        m_musicTrackBox.setPosition({cx, startY + 1.f * kRowGap});
        styleRowBox(m_musicTrackBox);

        LayoutMusicTrackRow();
        RefreshMusicTrackText();
    }

    // 2) SFX Toggle
    {
        const auto b = m_sfxToggleBox.getLocalBounds();
        m_sfxToggleBox.setOrigin(b.getCenter());
        m_sfxToggleBox.setPosition({cx, startY + 2.f * kRowGap});
        styleRowBox(m_sfxToggleBox);

        RefreshSfxToggleText();
    }

    // 3) SFX Volume
    {
        const auto b = m_sfxVolBox.getLocalBounds();
        m_sfxVolBox.setOrigin(b.getCenter());
        m_sfxVolBox.setPosition({cx, startY + 3.f * kRowGap});
        styleRowBox(m_sfxVolBox);

        ApplySfxVolume(m_context->m_sfxVolume);
    }

    // 4) Board Theme
    {
        const auto b = m_themeBox.getLocalBounds();
        m_themeBox.setOrigin(b.getCenter());
        m_themeBox.setPosition({cx, startY + 4.f * kRowGap});
        styleRowBox(m_themeBox);

        LayoutThemeRow();
        RefreshThemeText();
    }

    // 5) Stone Theme
    {
        const auto b = m_stoneBox.getLocalBounds();
        m_stoneBox.setOrigin(b.getCenter());
        m_stoneBox.setPosition({cx, startY + 5.f * kRowGap});
        styleRowBox(m_stoneBox);

        LayoutStoneRow();
        RefreshStoneText();
    }

    // 6) Back
    {
        const auto b = m_backBox.getLocalBounds();
        m_backBox.setOrigin(b.getCenter());
        m_backBox.setPosition({cx, startY + 6.f * kRowGap});
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

            m_musicVolBoxHovered   = m_musicVolBox.getGlobalBounds().contains(mousePos);
            m_musicTrackHovered    = m_musicTrackBox.getGlobalBounds().contains(mousePos);
            m_sfxToggleHovered     = m_sfxToggleBox.getGlobalBounds().contains(mousePos);
            m_sfxVolBoxHovered     = m_sfxVolBox.getGlobalBounds().contains(mousePos);
            m_themeHovered         = m_themeBox.getGlobalBounds().contains(mousePos);
            m_stoneHovered         = m_stoneBox.getGlobalBounds().contains(mousePos);
            m_backHovered          = m_backBox.getGlobalBounds().contains(mousePos);

            // --- Music slider hover ---
            {
                auto knobBounds  = m_musicVolKnob.getGlobalBounds();
                auto trackBounds = m_musicVolTrack.getGlobalBounds();
                trackBounds.position.y -= 18.f;
                trackBounds.size.y += 36.f;
                m_musicVolHovered = knobBounds.contains(mousePos) || trackBounds.contains(mousePos);

                if (m_musicVolDragging)
                    ApplyMusicVolume(VolumeFromMouseX(mousePos.x, m_musicVolTrack));
            }

            // --- SFX slider hover ---
            {
                auto knobBounds  = m_sfxVolKnob.getGlobalBounds();
                auto trackBounds = m_sfxVolTrack.getGlobalBounds();
                trackBounds.position.y -= 18.f;
                trackBounds.size.y += 36.f;
                m_sfxVolHovered = knobBounds.contains(mousePos) || trackBounds.contains(mousePos);

                if (m_sfxVolDragging)
                    ApplySfxVolume(VolumeFromMouseX(mousePos.x, m_sfxVolTrack));
            }
        }
        else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            const sf::Vector2f mousePos{
                static_cast<float>(wheel->position.x),
                static_cast<float>(wheel->position.y)};

            if (m_musicVolBox.getGlobalBounds().contains(mousePos))
                ApplyMusicVolume(m_musicVol + wheel->delta * 5.f);
            else if (m_sfxVolBox.getGlobalBounds().contains(mousePos))
                ApplySfxVolume(m_sfxVol + wheel->delta * 5.f);
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button != sf::Mouse::Button::Left)
                continue;

            const sf::Vector2f mousePos{
                static_cast<float>(mouseBtn->position.x),
                static_cast<float>(mouseBtn->position.y)};

            // Music slider: click anywhere on track/knob to set, then drag.
            {
                auto knobBounds  = m_musicVolKnob.getGlobalBounds();
                auto trackBounds = m_musicVolTrack.getGlobalBounds();
                trackBounds.position.y -= 18.f;
                trackBounds.size.y += 36.f;
                if (knobBounds.contains(mousePos) || trackBounds.contains(mousePos))
                {
                    m_musicVolDragging = true;
                    ApplyMusicVolume(VolumeFromMouseX(mousePos.x, m_musicVolTrack));
                    continue;
                }
            }

            // SFX slider
            {
                auto knobBounds  = m_sfxVolKnob.getGlobalBounds();
                auto trackBounds = m_sfxVolTrack.getGlobalBounds();
                trackBounds.position.y -= 18.f;
                trackBounds.size.y += 36.f;
                if (knobBounds.contains(mousePos) || trackBounds.contains(mousePos))
                {
                    m_sfxVolDragging = true;
                    ApplySfxVolume(VolumeFromMouseX(mousePos.x, m_sfxVolTrack));
                    continue;
                }
            }

            // Click-to-cycle rows
            if (m_musicTrackBox.getGlobalBounds().contains(mousePos))
            {
                CycleMusicTrack();
            }
            else if (m_sfxToggleBox.getGlobalBounds().contains(mousePos))
            {
                ToggleSfxEnabled();
            }
            else if (m_themeBox.getGlobalBounds().contains(mousePos))
            {
                m_context->m_boardTheme =
                    (m_context->m_boardTheme == BoardTheme::Classic) ? BoardTheme::Dark : BoardTheme::Classic;
                RefreshThemeText();
            }
            else if (m_stoneBox.getGlobalBounds().contains(mousePos))
            {
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
            {
                m_musicVolDragging = false;
                m_sfxVolDragging   = false;
            }
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

    applyHover(m_musicVolBox,   m_musicVolBoxHovered);
    applyHover(m_musicTrackBox, m_musicTrackHovered);
    applyHover(m_sfxToggleBox,  m_sfxToggleHovered);
    applyHover(m_sfxVolBox,     m_sfxVolBoxHovered);
    applyHover(m_themeBox,      m_themeHovered);
    applyHover(m_stoneBox,      m_stoneHovered);
    applyHover(m_backBox,       m_backHovered);

    m_musicVolKnob.setFillColor((m_musicVolHovered || m_musicVolDragging)
        ? sf::Color(255, 255, 255)
        : sf::Color(245, 245, 245));

    m_sfxVolKnob.setFillColor((m_sfxVolHovered || m_sfxVolDragging)
        ? sf::Color(255, 255, 255)
        : sf::Color(245, 245, 245));
}

void SettingsState::Draw()
{
    auto drawShadow = [&](const sf::RectangleShape& box)
    {
        sf::RectangleShape sh = box;
        sh.setFillColor(sf::Color(0, 0, 0, 40));
        sh.setOutlineThickness(0.f);
        sh.move({0.f, 5.f});
        m_context->m_window->draw(sh);
    };

    // Title
    m_context->m_window->draw(m_titleText);

    // Music Volume
    drawShadow(m_musicVolBox);
    m_context->m_window->draw(m_musicVolBox);
    m_context->m_window->draw(m_musicVolLabel);
    m_context->m_window->draw(m_musicVolTrack);
    m_context->m_window->draw(m_musicVolFill);
    m_context->m_window->draw(m_musicVolKnob);
    m_context->m_window->draw(m_musicVolPercent);

    // Music Track
    drawShadow(m_musicTrackBox);
    m_context->m_window->draw(m_musicTrackBox);
    m_context->m_window->draw(m_musicTrackLabel);
    m_context->m_window->draw(m_musicTrackValue);

    // SFX Toggle
    drawShadow(m_sfxToggleBox);
    m_context->m_window->draw(m_sfxToggleBox);
    m_context->m_window->draw(m_sfxToggleLabel);
    m_context->m_window->draw(m_sfxToggleValue);

    // SFX Volume
    drawShadow(m_sfxVolBox);
    m_context->m_window->draw(m_sfxVolBox);
    m_context->m_window->draw(m_sfxVolLabel);
    m_context->m_window->draw(m_sfxVolTrack);
    m_context->m_window->draw(m_sfxVolFill);
    m_context->m_window->draw(m_sfxVolKnob);
    m_context->m_window->draw(m_sfxVolPercent);

    // Board Theme
    drawShadow(m_themeBox);
    m_context->m_window->draw(m_themeBox);
    m_context->m_window->draw(m_themeLabel);
    m_context->m_window->draw(m_themeValue);

    // Stone Theme
    drawShadow(m_stoneBox);
    m_context->m_window->draw(m_stoneBox);
    m_context->m_window->draw(m_stoneLabel);
    m_context->m_window->draw(m_stoneValue);

    // Back
    drawShadow(m_backBox);
    m_context->m_window->draw(m_backBox);
    m_context->m_window->draw(m_backText);
}
