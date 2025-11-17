#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
#include <iostream>
ModeSelection::ModeSelection(std::shared_ptr<Context>& context)
:m_context{context},
m_2playersMode{{300.f,200.f}},
m_AiMode{{300.f,200.f}},
m_returnButton{64.f},
// m_2playersText(m_context->m_assets->GetFont(MAIN_FONT),"2 Players Mode",60),
// m_AiModeText(m_context->m_assets->GetFont(MAIN_FONT),"AI Mode",60)
// m_SelectMode(m_context->m_assets->GetFont(MAIN_FONT),"Select Gamemode",60),
m_is2PlayersSelected{true},
m_isAiSelected{false},
m_isReturnButtonSelected{false},
m_is2PlayersPressed{false},
m_isAiPressed{false},
m_isReturnButtonPressed{false}
{
}

ModeSelection::~ModeSelection() {

}

    void ModeSelection::Init()
    {
        //Two rectangles for mode selection
        m_2playersMode.setOrigin(m_2playersMode.getSize()/2.0f);
        m_2playersMode.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2-250,static_cast<float>(m_context->m_window->getSize().y)/2});
        m_2playersMode.setFillColor(sf::Color::Green);
        m_AiMode.setOrigin(m_AiMode.getSize()/2.0f);
        m_AiMode.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2+250,static_cast<float>(m_context->m_window->getSize().y)/2});
        m_AiMode.setFillColor(sf::Color::Red);
        m_returnButton.setFillColor(sf::Color::Blue);
        //Text for mode selection
        // m_SelectMode.setOrigin(m_SelectMode.getLocalBounds().getCenter());
        // m_SelectMode.setPosition({static_cast<float>(m_context->m_window->getSize().x/2),static_cast<float>(m_context->m_window->getSize().y)/2+200});
        // m_2playersText.setOrigin(m_2playersText.getLocalBounds().getCenter());
        // m_AiModeText.setOrigin(m_AiModeText.getLocalBounds().getCenter());
        // m_2playersText.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2-250,static_cast<float>(m_context->m_window->getSize().y)/2});
        // m_AiModeText.setPosition({m_AiMode.getPosition().x,m_AiMode.getPosition().y});
    }
    void ModeSelection::ProcessInput()
    {
        while(const std::optional event=m_context->m_window->pollEvent())
        { 
            if(event->is<sf::Event::Closed>())
            {
                m_context->m_window->close();
            }
            else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                // if(keyPressed->scancode==sf::Keyboard::Scancode::Escape)
                // {
                //     m_context->m_states->PopCurrent();
                //     std::cout<<"Trying to return to the main menu\n";
                // }
                if(keyPressed->scancode==sf::Keyboard::Scancode::Right)
                {
                    if(!m_isAiSelected) 
                    {
                        m_isAiSelected=true;
                        m_is2PlayersSelected=false;
                    }
                }
                if(keyPressed->scancode==sf::Keyboard::Scancode::Left)
                {
                    if(!m_is2PlayersSelected) 
                    {
                        m_isAiSelected=false;
                        m_is2PlayersSelected=true;
                    }
                }
            }
        }
    }
    void ModeSelection::Update(sf::Time deltaTime)
    {
        if(m_is2PlayersSelected)
        {
            m_2playersMode.setOutlineColor(sf::Color::Yellow);
            m_2playersMode.setOutlineThickness(10.f);
            m_AiMode.setOutlineColor(sf::Color::White);
            m_AiMode.setOutlineThickness(10.f);
        }
        else if(m_isAiSelected)
        {
            m_AiMode.setOutlineColor(sf::Color::Yellow);
            m_AiMode.setOutlineThickness(10.f);
            m_2playersMode.setOutlineColor(sf::Color::White);
            m_2playersMode.setOutlineThickness(10.f);
        }
    }
    void ModeSelection::Draw()
    {
    m_context->m_window->clear({210,164,80});   
    m_context->m_window->draw(m_2playersMode);
    m_context->m_window->draw(m_AiMode);
    m_context->m_window->draw(m_returnButton);
    // m_context->m_window->draw(m_2playersText);
    // m_context->m_window->draw(m_AiModeText);
    // m_context->m_window->draw(m_SelectMode);
    m_context->m_window->display();
    }