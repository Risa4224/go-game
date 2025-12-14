#include "GameApp.h"
#include "MainMenu.hpp"
#include <SFML/Window.hpp>
#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <string>

namespace
{
static std::string MusicFileForTrack(MusicTrack t)
{
    switch (t)
    {
        case MusicTrack::Classic: return "assets/audio/background.mp3";
        case MusicTrack::Ambient: return "assets/audio/background_2.mp3";
        case MusicTrack::Retro:   return "assets/audio/background_2.mp3"; // legacy -> Modern
        case MusicTrack::Off:     return "";
        default:                  return "assets/audio/background.mp3";
    }
}

static void ApplyMusicSettings(Context& ctx)
{
    if (!ctx.m_music)
        return;

    const float vol = std::clamp(ctx.m_musicVolume, 0.f, 100.f);

    if (!ctx.m_musicEnabled || ctx.m_musicTrack == MusicTrack::Off || vol <= 0.f)
    {
        ctx.m_music->stop();
        return;
    }

    const std::string file = MusicFileForTrack(ctx.m_musicTrack);
    if (file.empty())
    {
        ctx.m_musicEnabled = false;
        ctx.m_music->stop();
        return;
    }

    if (!ctx.m_music->openFromFile(file))
    {
        ctx.m_musicEnabled = false;
        ctx.m_music->stop();
        return;
    }

    ctx.m_music->setLooping(true); // SFML 3
    ctx.m_music->setVolume(vol);
    ctx.m_music->play();
}
} // namespace

GameApp::GameApp()
    : m_context(std::make_shared<Context>())
{
    m_context->m_window->create(
        sf::VideoMode({1000u, 800u}),
        "Baduk",
        sf::Style::Close);
    sf::Image icon;
    if (icon.loadFromFile("assets/texture/game_icon.png")) // use your transparent PNG
    {
        m_context->m_window->setIcon(icon);
    }

    m_context->m_assets->AddFont(
        MAIN_FONT,
        "assets/fonts/Roboto-VariableFont_wdth,wght.ttf");
    m_context->m_assets->AddTexture(
        GAME_ICON,
        "assets/texture/game_icon.png", // <-- put your icon file here
        false);
// Background Music (single source of truth)
// Defaults (user can change these in Settings)
m_context->m_musicTrack   = MusicTrack::Classic;
m_context->m_musicVolume  = 100.f;
m_context->m_musicEnabled = true;

ApplyMusicSettings(*m_context);
{
        auto &assets = *m_context->m_assets;

        assets.AddSoundBuffer(STONEPLACE_SOUND, "assets/sfx/stone_place.mp3");
        assets.AddSoundBuffer(PASS_SOUND, "assets/sfx/pass.wav");
        assets.AddSoundBuffer(INVALID_SOUND, "assets/sfx/invalid.mp3");
        assets.AddSoundBuffer(WIN_SOUND, "assets/sfx/win.mp3");
    }
    m_context->m_states->Add(std::make_unique<MainMenu>(m_context), false);
}

GameApp::~GameApp() = default;

void GameApp::Run()
{
    sf::Clock clock;

    while (m_context->m_window->isOpen())
    {
        sf::Time dt = clock.restart();

        m_context->m_states->ProcessStateChange();

        if (m_context->m_states->isEmpty())
        {
            m_context->m_window->close();
            break;
        }

        auto &current = m_context->m_states->getCurrent();

        current->ProcessInput();
        current->Update(dt);

        auto &stack = m_context->m_states->getStack();
        if (stack.empty())
            continue;

        m_context->m_window->clear(sf::Color(120, 180, 120));

        int startIndex = static_cast<int>(stack.size()) - 1;
        while (startIndex > 0 && stack[startIndex]->AllowDrawBelow())
        {
            --startIndex;
        }

        for (int i = startIndex; i < static_cast<int>(stack.size()); ++i)
        {
            stack[i]->Draw();
        }

        m_context->m_window->display();
    }
}
