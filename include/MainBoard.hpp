#pragma once

#include <memory>
#include <vector>

#include "State.hpp"
#include "GameApp.h"
#include "game.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Vertex.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Text.hpp>

class MainBoard : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    sf::RectangleShape m_boardBackground;
    std::vector<sf::Vertex> m_gridLines;
    float m_boardPixelSize;
    int m_boardSize;
    float m_cellSize;
    sf::Vector2f m_boardTopLeft;

    std::vector<sf::CircleShape> m_stones;

    sf::RectangleShape m_undoButtonBox;
    sf::RectangleShape m_redoButtonBox;
    sf::RectangleShape m_passButtonBox;
    sf::RectangleShape m_pauseButtonBox;
    bool m_undoHovered;
    bool m_redoHovered;
    bool m_passHovered;
    bool m_pauseHovered;
    std::unique_ptr<Game> m_game;

    void buildGrid();
    void rebuildStonesFromGame();
    void handleLeftClick(const sf::Vector2i &pixelPos);
    void resetGame();

public:
    MainBoard(std::shared_ptr<Context> &context);
    ~MainBoard() override = default;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
