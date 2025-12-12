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

    // Volume row (slider)
    sf::RectangleShape m_volumeBox;
    sf::Text           m_volumeLabel;
    sf::Text           m_volumePercent;

    float m_volume = 100.f;
    bool  m_volumeBoxHovered = false;
    bool  m_volumeHovered = false;
    bool  m_volumeDragging = false;

    sf::RectangleShape m_volumeTrack;
    sf::RectangleShape m_volumeFill;
    sf::CircleShape    m_volumeKnob;

    // Theme row
    sf::RectangleShape m_themeBox;
    sf::Text           m_themeLabel;
    sf::Text           m_themeValue;
    bool               m_themeHovered = false;

    // Back button
    sf::RectangleShape m_backBox;
    sf::Text           m_backText;
    bool               m_backHovered = false;

private:
    float VolumeFromMouseX(float mouseX) const;
    void  ApplyVolume(float volumePercent);
    void  LayoutVolumeRow();

    void  LayoutThemeRow();
    void  RefreshThemeText();

public:
    explicit SettingsState(std::shared_ptr<Context>& context);
    ~SettingsState() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
