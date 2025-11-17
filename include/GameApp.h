#pragma once

#include <memory>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Audio/Music.hpp>

#include "AssetMan.hpp"
#include "StateManager.hpp"

enum AssetID
{
    MAIN_FONT = 0
};

// ✨ GameMode enum cần đặt ở đây
enum class GameMode
{
    TwoPlayers,
    AiVsPlayer
};

struct Context
{
    std::unique_ptr<Engine::AssetMan>      m_assets;
    std::unique_ptr<Engine::StateManager>  m_states;
    std::unique_ptr<sf::RenderWindow>      m_window;
    std::unique_ptr<sf::Music>             m_music;

    bool      m_musicEnabled;
    GameMode  m_gameMode; 

    Context()
    {
        m_assets       = std::make_unique<Engine::AssetMan>();
        m_states       = std::make_unique<Engine::StateManager>();
        m_window       = std::make_unique<sf::RenderWindow>();
        m_music        = std::make_unique<sf::Music>();
        m_musicEnabled = true;
        m_gameMode     = GameMode::TwoPlayers;
    }
};
