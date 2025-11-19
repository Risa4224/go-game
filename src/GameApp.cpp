#include "GameApp.h"
#include "MainMenu.hpp"
#include <SFML/Window.hpp>

GameApp::GameApp()
    : m_context(std::make_shared<Context>())
{
    // Tạo cửa sổ
    m_context->m_window->create(
        sf::VideoMode({1000u, 800u}),
        "Go Game",
        sf::Style::Close
    );

    // Load font chính
    m_context->m_assets->AddFont(
        MAIN_FONT,
        "assets/fonts/Roboto-VariableFont_wdth,wght.ttf"
    );

    // Nhạc nền (nếu có file)
    if (m_context->m_music->openFromFile("assets/audio/background.mp3"))
    {
        m_context->m_music->setVolume(100.f);
        m_context->m_music->play();
        m_context->m_musicEnabled = true;
    }
    else
    {
        m_context->m_musicEnabled = false;
    }

    // Push MainMenu lần đầu (VERY IMPORTANT)
    m_context->m_states->Add(std::make_unique<MainMenu>(m_context), false);
}

GameApp::~GameApp() = default;

void GameApp::Run()
{
    sf::Clock clock;

    while (m_context->m_window->isOpen())
    {
        sf::Time dt = clock.restart();

        // Áp dụng các thay đổi push/pop state
        m_context->m_states->ProcessStateChange();

        // Nếu không còn state nào -> đóng game
        if (m_context->m_states->isEmpty())
        {
            m_context->m_window->close();
            break;
        }

        auto& currentState = m_context->m_states->getCurrent();

        currentState->ProcessInput();
        currentState->Update(dt);
        currentState->Draw();
    }
}
