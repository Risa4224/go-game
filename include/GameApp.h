#pragma once

#include <memory>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Audio/Music.hpp>

#include "AssetMan.hpp"
#include "StateManager.hpp"
#include "Ai.h"

// ID của asset (font, texture, v.v.)
enum AssetID
{
    MAIN_FONT = 0,
    STONEPLACE_SOUND = 1,
    PASS_SOUND = 2,
    INVALID_SOUND = 3,
    WIN_SOUND = 4,
    GAME_ICON = 5
};

enum class MusicTrack
{
    Off,
    Classic,
    Ambient,
    Retro
};
enum class SfxTheme
{
    Off,
    Classic,
    Arcade,
    Zen
};

// Chế độ chơi
enum class GameMode
{
    TwoPlayers,
    AiVsPlayer
};
enum class BoardTheme
{
    Classic,
    Dark
};
enum class StoneTheme
{
    Classic,
    SlateShell,
    Glass
};
// Ngữ cảnh dùng chung cho mọi State
struct Context
{
    std::unique_ptr<Engine::AssetMan> m_assets;
    std::unique_ptr<Engine::StateManager> m_states;
    std::unique_ptr<sf::RenderWindow> m_window;
    std::unique_ptr<sf::Music> m_music;

    bool m_musicEnabled;
    GameMode m_gameMode;
    bool m_requestBoardRestart = false;
    BoardTheme m_boardTheme = BoardTheme::Classic;
    StoneTheme m_stoneTheme = StoneTheme::Classic;
    AIDifficulty m_aiDifficulty = AIDifficulty::MEDIUM;
    // Music
    // Music
    float m_musicVolume = 100.f;
    MusicTrack m_musicTrack = MusicTrack::Classic; // enum của bạn
    // (m_music) bạn đã có thì giữ, chưa có thì cần sf::Music* / unique_ptr<sf::Music>

    // SFX
    float m_sfxVolume = 100.f;
    bool m_sfxEnabled = true;

    bool m_humanPlaysBlack{true};
    Context()
    {
        m_assets = std::make_unique<Engine::AssetMan>();
        m_states = std::make_unique<Engine::StateManager>();
        m_window = std::make_unique<sf::RenderWindow>();
        m_music = std::make_unique<sf::Music>();
        m_musicEnabled = true;
        m_gameMode = GameMode::TwoPlayers;
        m_requestBoardRestart = false;
    }
};

// Lớp GameApp – vòng lặp game chính
class GameApp
{
private:
    std::shared_ptr<Context> m_context;

public:
    GameApp();
    ~GameApp();

    void Run();
};
