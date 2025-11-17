#include "MainMenu.hpp"
#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
MainMenu::MainMenu(std::shared_ptr<Context> &context)
    :m_context(context),
    m_gameTitle(m_context->m_assets->GetFont(MAIN_FONT),"Go Game",60),
    m_playButton(m_context->m_assets->GetFont(MAIN_FONT),"Play",40),
    m_exitButton(m_context->m_assets->GetFont(MAIN_FONT),"Exit",40),
    m_settingsButton(m_context->m_assets->GetFont(MAIN_FONT),"Settings",40),
    m_isPlayButtonSelected(true),
    m_isPlayButtonPressed(false),
    m_isExitButtonSelected(false),
    m_isExitButtonPressed(false),
    m_isSettingButtonSelected(false),
    m_isSettingButtonPressed(false)
{
}

MainMenu::~MainMenu() {

}

void MainMenu::Init()
{
    //title
    m_gameTitle.setOrigin(m_gameTitle.getLocalBounds().getCenter());
    m_gameTitle.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2,static_cast<float>(m_context->m_window->getSize().y)/2-100});

    //play button

    m_playButton.setOrigin(m_playButton.getLocalBounds().getCenter());
    m_playButton.setPosition({static_cast<float>(m_context->m_window->getSize().x/2),static_cast<float>(m_context->m_window->getSize().y)/2-20});

    //setting button


    m_settingsButton.setOrigin(m_settingsButton.getLocalBounds().getCenter());
    m_settingsButton.setPosition({static_cast<float>(m_context->m_window->getSize().x/2),static_cast<float>(m_context->m_window->getSize().y)/2+60});

    //exit button

    m_exitButton.setOrigin(m_exitButton.getLocalBounds().getCenter());
    m_exitButton.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2,static_cast<float>(m_context->m_window->getSize().y)/2+140});
}
void MainMenu::ProcessInput()
{
    while(const std::optional event=m_context->m_window->pollEvent())
    { 
        if(event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if(keyPressed->scancode==sf::Keyboard::Scancode::Up)
            {
                if(!m_isPlayButtonSelected)
                {
                    if(m_isExitButtonSelected)
                    {
                        m_isExitButtonSelected=false;
                        m_isSettingButtonSelected=true;
                    }
                    else
                    {
                        m_isSettingButtonSelected=false;
                        m_isPlayButtonSelected=true;
                    }
                }
            }
            if(keyPressed->scancode==sf::Keyboard::Scancode::Down)
            {
                if(!m_isExitButtonSelected)
                {
                    if(m_isPlayButtonSelected)
                    {
                        m_isPlayButtonSelected=false;
                        m_isSettingButtonSelected=true;
                    }
                    else
                    {
                        m_isSettingButtonSelected=false;
                        m_isExitButtonSelected=true;
                    }
                }
            }
            if(keyPressed->scancode==sf::Keyboard::Scancode::Enter)
            {
                m_isExitButtonPressed=false;
                m_isPlayButtonPressed=false;
                m_isSettingButtonPressed=false;
                if(m_isPlayButtonSelected) m_isPlayButtonPressed=true;
                if(m_isExitButtonSelected) m_isExitButtonPressed=true;
                if(m_isSettingButtonSelected) m_isSettingButtonPressed=true;
            }
        }
    }
}
void MainMenu::Update(sf::Time deltaTime)
{
    if(m_isPlayButtonSelected)
    {
        m_playButton.setFillColor(sf::Color::Yellow);
        m_exitButton.setFillColor(sf::Color::White);
        m_settingsButton.setFillColor(sf::Color::White);
    }
    else if(m_isExitButtonSelected)
    {
        m_exitButton.setFillColor(sf::Color::Yellow);
        m_playButton.setFillColor(sf::Color::White);
        m_settingsButton.setFillColor(sf::Color::White);
    }
    else
    {
        m_settingsButton.setFillColor(sf::Color::Yellow);
        m_exitButton.setFillColor(sf::Color::White);
        m_playButton.setFillColor(sf::Color::White);
    }
    if(m_isPlayButtonPressed)
    {
        //Todo GO to Play State
        m_context->m_states->Add(std::make_unique<ModeSelection>(m_context),false);
        m_isPlayButtonPressed=false;
    }
    else if(m_isSettingButtonPressed)
    {
        //TOdo go to settings state
    }
    else if(m_isExitButtonPressed)
    {
        m_context->m_window->close();
    }
}
void MainMenu::Draw()
{
    m_context->m_window->clear({210,164,80});   
    m_context->m_window->draw(m_gameTitle);
    m_context->m_window->draw(m_exitButton);
    m_context->m_window->draw(m_playButton);
    m_context->m_window->draw(m_settingsButton);
    m_context->m_window->display();

}