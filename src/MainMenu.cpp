#include "MainMenu.hpp"
#include <SFML/Window/Event.hpp>
MainMenu::MainMenu(std::shared_ptr<Context> &context)
    :m_context(context),
    m_gameTitle(m_context->m_assets->GetFont(MAIN_FONT))
{
}

MainMenu::~MainMenu() {

}

void MainMenu::Init()
{
    m_gameTitle.setString("Go Game");
    m_gameTitle.setPosition({320,320});
}
void MainMenu::ProcessInput()
{
    while(const std::optional event=m_context->m_window->pollEvent())
    { 
        if(event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
        }
    }
}
void MainMenu::Update(sf::Time deltaTime)
{

}
void MainMenu::Draw()
{
    m_context->m_window->clear();   
    m_context->m_window->draw(m_gameTitle);
    m_context->m_window->display();

}