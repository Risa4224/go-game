#include "Game.hpp"
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Window/Event.hpp>
Game::Game() {
    m_context-> m_window->create(sf::VideoMode({200,200}),"SFML works", sf::Style::Close);
    //Todo
    //Add first state to m_states here
}

Game::~Game() {

}
/*
void Game::Run()
{
    sf::CircleShape shape(100.f);
    shape.setFillColor(sf::Color::Green);
    while(m_context->m_window->isOpen())
    {
        sf::Event event;
        while(m_context->m_window->pollEvent(event))
        {

        }
    }
}
*/