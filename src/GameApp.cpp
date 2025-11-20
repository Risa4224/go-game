#include "GameApp.h"
#include "MainMenu.hpp"
#include <SFML/Window.hpp>

GameApp::GameApp()
    : m_context(std::make_shared<Context>())
{

    m_context->m_window->create(
        sf::VideoMode({1000u, 800u}),
        "Go Game",
        sf::Style::Close);

    m_context->m_assets->AddFont(
        MAIN_FONT,
        "assets/fonts/Roboto-VariableFont_wdth,wght.ttf");

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

        m_context->m_window->clear(sf::Color(30, 30, 30));

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
