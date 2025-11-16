#include "GameApp.h"
#include "MainMenu.hpp"
#include <SFML/Window.hpp>

GameApp::GameApp() : m_context(std::make_shared<Context>())
{
    m_context->m_window->create(sf::VideoMode({640,640}),"Go Game",sf::Style::Close);
    m_context->m_assets->AddFont(MAIN_FONT,"assets/fonts/Roboto-VariableFont_wdth,wght.ttf");
    m_context->m_states->Add(std::make_unique<MainMenu>(m_context));
}
GameApp::~GameApp()
{

}

void GameApp::Run()
{

    sf::Clock clock;
    sf::Time timeSinceLastFrame=sf::Time::Zero;



    while(m_context->m_window->isOpen())
    {
        timeSinceLastFrame+= clock.restart();

        while(timeSinceLastFrame>TIME_PER_FRAME)
        {
            timeSinceLastFrame-=TIME_PER_FRAME;
        }
        
            m_context->m_states->ProcessStateChange();
            m_context->m_states->getCurrent()->ProcessInput();
            m_context->m_states->getCurrent()->Update(TIME_PER_FRAME);
            m_context->m_states->getCurrent()->Draw();
    }
}