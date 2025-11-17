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

class MainBoard : public Engine::State
{
private:
    std::shared_ptr<Context> m_context;

    // Nền bàn cờ
    sf::RectangleShape m_boardBackground;
    // Các đường lưới
    std::vector<sf::VertexArray> m_gridLines;

    // Text (dùng optional vì sf::Text không có default ctor trong SFML 3)
    std::optional<sf::Text> m_titleText;
    std::optional<sf::Text> m_hintText;

    // Thông tin bàn cờ
    unsigned int m_boardSize;          // 19x19
    sf::Vector2f m_boardTopLeft;       // góc trên trái của vùng lưới
    float m_boardPixelSize;            // kích thước pixel của bàn (vuông)
    float m_cellSize;                  // khoảng cách giữa các đường lưới

    // Trạng thái quân cờ: 0 = trống, 1 = đen, 2 = trắng
    std::vector<std::vector<int>> m_boardState;
    std::vector<sf::CircleShape> m_stones;
    bool m_isBlackTurn;

public:
    MainBoard(std::shared_ptr<Context>& context);
    ~MainBoard() override;

    void Init() override;
    void ProcessInput() override;
    void Update(sf::Time deltaTime) override;
    void Draw() override;
};
