#include "GameApp.h"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window/Event.hpp>

GameApp::GameApp() : m_context(std::make_shared<Context>())
{
    m_context->m_window->create(sf::VideoMode({200,200}),"Go Game",sf::Style::Close);
    //to do 
    // Add first state to m_states here.
}
GameApp::~GameApp()
{

}

void GameApp::Run()
{
    sf::CircleShape shape(100.f);
    shape.setFillColor(sf::Color::Green);
    while(m_context->m_window->isOpen())
    {
        while(const std::optional event=m_context->m_window->pollEvent())
        { 
            if(event->is<sf::Event::Closed>())
            {
                m_context->m_window->close();
            }
        }

        m_context->m_window->clear();   
        m_context->m_window->draw(shape);
        m_context->m_window->display();
    }
}