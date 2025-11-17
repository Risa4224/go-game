#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <SFML/System/Time.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Text.hpp>

#include "State.hpp"
#include "GameApp.h"

#include "game.h"      // 🔥 dùng Game
#include "nonclass.h"  // để có PieceColor

class MainBoard : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    // Bàn cờ
    sf::RectangleShape              m_boardBackground;
    std::vector<sf::VertexArray>    m_gridLines;

    // Text
    std::optional<sf::Text>         m_titleText;
    std::optional<sf::Text>         m_hintText;

    // Thông tin lưới
    unsigned int                    m_boardSize;      // 19x19
    sf::Vector2f                    m_boardTopLeft;
    float                           m_boardPixelSize;
    float                           m_cellSize;

    // 🔥 Logic game
    std::unique_ptr<Game>           m_game;

    // Các hình tròn để vẽ quân cờ
    std::vector<sf::CircleShape>    m_stones;

    // helper: dựng lại quân cờ từ Board
    void rebuildStones();

public:
    MainBoard(std::shared_ptr<Context>& context);
    ~MainBoard() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
