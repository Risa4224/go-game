#include "ModeSelection.hpp"
#include <SFML/Window/Event.hpp>
ModeSelection::ModeSelection(std::shared_ptr<Context>& context)
:m_context{context},
m_2playersMode{{300.f,200.f}},
m_AiMode{{300.f,200.f}}
{

}

ModeSelection::~ModeSelection() {

}

    void ModeSelection::Init()
    {
        m_2playersMode.setOrigin(m_2playersMode.getGeometricCenter());
        m_2playersMode.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2-300,static_cast<float>(m_context->m_window->getSize().y)/2});
        m_2playersMode.setFillColor(sf::Color::Green);
        m_AiMode.setOrigin(m_2playersMode.getGeometricCenter());
        m_AiMode.setPosition({static_cast<float>(m_context->m_window->getSize().x)/2+300,static_cast<float>(m_context->m_window->getSize().y)/2});
        m_2playersMode.setFillColor(sf::Color::Red);
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
                if(keyPressed->scancode==sf::Keyboard::Scancode::Escape)
                {
                    m_context->m_window->close();
                }
            }
        }
    }
    void ModeSelection::Update(sf::Time deltaTime)
    {

    }
    void ModeSelection::Draw()
    {
    m_context->m_window->clear({210,164,80});   
    m_context->m_window->draw(m_2playersMode);
    m_context->m_window->draw(m_AiMode);
    m_context->m_window->display();
    }