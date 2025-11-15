#pragma once

#include <memory>

#include <SFML/Graphics/RenderWindow.hpp>

#include <AssetMan.hpp>
#include <StateManager.hpp>

struct Context
{
    std::unique_ptr<Engine::AssetMan> m_assets;
    std::unique_ptr<Engine::StateManager> m_states;
    std::unique_ptr<sf::RenderWindow> m_window;

    Context()
    {
        m_assets=std::make_unique<Engine::AssetMan>();
        m_states=std::make_unique<Engine::StateManager>();
        m_window=std::make_unique<sf::RenderWindow>();
    }
};
class Game {
private:
    std::shared_ptr<Context> m_context;
    const sf::Time TIME_PER_SECOND =sf::seconds(1.f/60.f);
public:
    Game();
    ~Game();
    void Run();
};