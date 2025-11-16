#pragma once

#include <memory>
#include "State.hpp"
#include "GameApp.h"
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
class ModeSelection : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;
    sf::RectangleShape m_2playersMode;
    sf::RectangleShape m_AiMode;

public:
    ModeSelection(std::shared_ptr<Context>& context);
    ~ModeSelection();


    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;



};