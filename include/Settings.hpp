#pragma once

#include <memory>

#include "GameApp.h"
#include "State.hpp"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Text.hpp>

class SettingsState : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::Text m_titleText;

    // --- Music (required) ---
    // Music volume (slider)
    sf::RectangleShape m_musicVolBox;
    sf::Text           m_musicVolLabel;
    sf::Text           m_musicVolPercent;

    float m_musicVol = 100.f;
    bool  m_musicVolBoxHovered = false;
    bool  m_musicVolHovered = false;
    bool  m_musicVolDragging = false;

    sf::RectangleShape m_musicVolTrack;
    sf::RectangleShape m_musicVolFill;
    sf::CircleShape    m_musicVolKnob;

    // Music track selection (click to cycle)
    sf::RectangleShape m_musicTrackBox;
    sf::Text           m_musicTrackLabel;
    sf::Text           m_musicTrackValue;
    bool               m_musicTrackHovered = false;

    // --- SFX (required) ---
    // SFX enable/disable (explicit toggle)
    sf::RectangleShape m_sfxToggleBox;
    sf::Text           m_sfxToggleLabel;
    sf::Text           m_sfxToggleValue;
    bool               m_sfxToggleHovered = false;

    // SFX volume (slider)
    sf::RectangleShape m_sfxVolBox;
    sf::Text           m_sfxVolLabel;
    sf::Text           m_sfxVolPercent;

    float m_sfxVol = 100.f;
    bool  m_sfxVolBoxHovered = false;
    bool  m_sfxVolHovered = false;
    bool  m_sfxVolDragging = false;

    sf::RectangleShape m_sfxVolTrack;
    sf::RectangleShape m_sfxVolFill;
    sf::CircleShape    m_sfxVolKnob;

    // Theme row
    sf::RectangleShape m_themeBox;
    sf::Text           m_themeLabel;
    sf::Text           m_themeValue;
    bool               m_themeHovered = false;

    // Stone theme row
    sf::RectangleShape m_stoneBox;
    sf::Text           m_stoneLabel;
    sf::Text           m_stoneValue;
    bool               m_stoneHovered = false;

    // Back button
    sf::RectangleShape m_backBox;
    sf::Text           m_backText;
    bool               m_backHovered = false;

private:
    float VolumeFromMouseX(float mouseX, const sf::RectangleShape& track) const;

    // Music
    void  ApplyMusicVolume(float volumePercent);
    void  LayoutMusicVolumeRow();

    void  LayoutMusicTrackRow();
    void  RefreshMusicTrackText();
    void  CycleMusicTrack();

    // SFX
    void  LayoutSfxToggleRow();
    void  RefreshSfxToggleText();
    void  ToggleSfxEnabled();

    void  ApplySfxVolume(float volumePercent);
    void  LayoutSfxVolumeRow();

    void  LayoutThemeRow();
    void  RefreshThemeText();

    void  LayoutStoneRow();
    void  RefreshStoneText();

public:
    explicit SettingsState(std::shared_ptr<Context>& context);
    ~SettingsState() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
