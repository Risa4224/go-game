#include "MainBoard.hpp"
#include <SFML/Window/Event.hpp>
#include <iostream>
#include "PauseState.hpp"
MainBoard::MainBoard(std::shared_ptr<Context> &context)
    : m_context(context), m_boardBackground(), m_gridLines(), m_boardPixelSize(650.f), m_boardSize(19), m_cellSize(0.f), m_boardTopLeft(0.f, 0.f), m_stones(), m_undoButtonBox(), m_redoButtonBox(), m_undoHovered(false), m_redoHovered(false), m_game(std::make_unique<Game>(new Board()))
{
    std::cout << "[MainBoard] ctor\n";
}

void MainBoard::Init()
{
    std::cout << "[MainBoard] Init START\n";

    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winSizeF(static_cast<float>(winSize.x),
                          static_cast<float>(winSize.y));

    m_boardBackground.setSize({m_boardPixelSize, m_boardPixelSize});
    m_boardBackground.setFillColor({210, 164, 80});
    m_boardBackground.setOutlineThickness(2.f);
    m_boardBackground.setOutlineColor(sf::Color::Black);

    m_boardTopLeft.x = (winSizeF.x - m_boardPixelSize) * 0.5f;
    m_boardTopLeft.y = (winSizeF.y - m_boardPixelSize) * 0.5f;
    m_boardBackground.setPosition(m_boardTopLeft);

    m_cellSize = m_boardPixelSize / static_cast<float>(m_boardSize - 1);

    buildGrid();

    m_undoButtonBox.setSize({100.f, 40.f});
    m_undoButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_undoButtonBox.setPosition({winSizeF.x - 150.f, 40.f});

    m_redoButtonBox.setSize({100.f, 40.f});
    m_redoButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_redoButtonBox.setPosition({winSizeF.x - 150.f, 90.f});

    m_passButtonBox.setSize({100.f, 40.f});
    m_passButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_passButtonBox.setPosition({winSizeF.x - 150.f, 140.f});

    m_pauseButtonBox.setSize({100.f, 40.f});
    m_pauseButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_pauseButtonBox.setPosition({winSizeF.x - 150.f, 190.f});

    if (!m_game)
        m_game = std::make_unique<Game>(new Board());

    rebuildStonesFromGame();

    std::cout << "[MainBoard] Init END\n";
}

void MainBoard::buildGrid()
{
    m_gridLines.clear();
    m_gridLines.reserve(static_cast<std::size_t>(m_boardSize * 4));

    for (int i = 0; i < m_boardSize; ++i)
    {
        float x = m_boardTopLeft.x + i * m_cellSize;

        sf::Vertex v1{};
        v1.position = {x, m_boardTopLeft.y};
        v1.color = sf::Color::Black;
        m_gridLines.push_back(v1);

        sf::Vertex v2{};
        v2.position = {x, m_boardTopLeft.y + m_boardPixelSize};
        v2.color = sf::Color::Black;
        m_gridLines.push_back(v2);
    }

    for (int j = 0; j < m_boardSize; ++j)
    {
        float y = m_boardTopLeft.y + j * m_cellSize;

        sf::Vertex v1{};
        v1.position = {m_boardTopLeft.x, y};
        v1.color = sf::Color::Black;
        m_gridLines.push_back(v1);

        sf::Vertex v2{};
        v2.position = {m_boardTopLeft.x + m_boardPixelSize, y};
        v2.color = sf::Color::Black;
        m_gridLines.push_back(v2);
    }
}

void MainBoard::rebuildStonesFromGame()
{
    m_stones.clear();

    if (!m_game)
        return;
    Board *board = m_game->getBoard();
    if (!board)
        return;

    int size = board->getSize();
    float radius = m_cellSize * 0.4f;

    for (int x = 0; x < size; ++x)
    {
        for (int y = 0; y < size; ++y)
        {
            PieceColor c = board->getPiece(x, y);
            if (c == NONE)
                continue;

            sf::CircleShape stone(radius);
            stone.setOrigin({radius, radius});

            float px = m_boardTopLeft.x + x * m_cellSize;
            float py = m_boardTopLeft.y + y * m_cellSize;
            stone.setPosition({px, py});

            if (c == BLACK)
            {
                stone.setFillColor(sf::Color::Black);
                stone.setOutlineThickness(1.f);
                stone.setOutlineColor(sf::Color(220, 220, 220));
            }
            else if (c == WHITE)
            {
                stone.setFillColor(sf::Color::White);
                stone.setOutlineThickness(1.f);
                stone.setOutlineColor(sf::Color::Black);
            }

            m_stones.push_back(stone);
        }
    }
}

void MainBoard::handleLeftClick(const sf::Vector2i &pixelPos)
{
    sf::Vector2f posF(static_cast<float>(pixelPos.x),
                      static_cast<float>(pixelPos.y));

    if (!m_boardBackground.getGlobalBounds().contains(posF))
        return;

    float localX = posF.x - m_boardTopLeft.x;
    float localY = posF.y - m_boardTopLeft.y;

    float fx = localX / m_cellSize;
    float fy = localY / m_cellSize;
    int ix = static_cast<int>(fx + 0.5f);
    int iy = static_cast<int>(fy + 0.5f);

    if (ix < 0 || ix >= m_boardSize || iy < 0 || iy >= m_boardSize)
        return;

    if (!m_game)
        return;

    bool ok = m_game->placeStone(ix, iy);
    if (ok)
    {
        rebuildStonesFromGame();
    }
    else
    {
        std::cout << "[MainBoard] Invalid move at (" << ix << ", " << iy << ")\n";
    }
}

void MainBoard::ProcessInput()
{
    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
            return;
        }
        else if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
            {

                std::cout << "Trying to return to menu\n";
                m_context->m_states->PopCurrent();
                std::cout << "Popped current State\n";
                return;
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::Z)
            {

                if (m_game && m_game->undo())
                {
                    rebuildStonesFromGame();
                }
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::Y)
            {

                if (m_game && m_game->redo())
                {
                    rebuildStonesFromGame();
                }
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::P)
            {

                if (m_game && m_game->pass())
                {
                    rebuildStonesFromGame();
                }
            }
        }
        else if (const auto *mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos(
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y));

            m_undoHovered = m_undoButtonBox.getGlobalBounds().contains(mousePos);
            m_redoHovered = m_redoButtonBox.getGlobalBounds().contains(mousePos);
            m_passHovered = m_passButtonBox.getGlobalBounds().contains(mousePos);
        }
        else if (const auto *mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePosF(
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y));
                sf::Vector2i mousePos(mouseBtn->position.x, mouseBtn->position.y);

                if (m_undoButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game && m_game->undo())
                    {
                        rebuildStonesFromGame();
                    }
                    continue;
                }

                if (m_redoButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game && m_game->redo())
                    {
                        rebuildStonesFromGame();
                    }
                    continue;
                }

                if (m_passButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game)
                    {
                        bool finished = m_game->pass();
                        rebuildStonesFromGame();

                        if (finished)
                        {

                            m_game->calculateFinalScore();

                            m_context->m_states->Add(
                                std::make_unique<PauseState>(
                                    m_context,
                                    PauseState::Mode::GameOver,
                                    "Game Over"),
                                false);
                        }
                    }
                    continue;
                }

                if (m_pauseButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    m_context->m_states->Add(
                        std::make_unique<PauseState>(m_context, PauseState::Mode::Paused),
                        false);
                }

                handleLeftClick(mousePos);
            }
        }
    }
}

void MainBoard::Update(sf::Time)
{
    if (m_context->m_requestBoardRestart)
    {
        m_context->m_requestBoardRestart = false;
        resetGame();
    }

    m_undoButtonBox.setFillColor(
        m_undoHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    m_redoButtonBox.setFillColor(
        m_redoHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    m_passButtonBox.setFillColor(
        m_passHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
}

void MainBoard::Draw()
{

    m_context->m_window->draw(m_boardBackground);

    if (!m_gridLines.empty())
    {
        m_context->m_window->draw(
            m_gridLines.data(),
            m_gridLines.size(),
            sf::PrimitiveType::Lines);
    }

    for (const auto &stone : m_stones)
        m_context->m_window->draw(stone);

    m_context->m_window->draw(m_undoButtonBox);
    m_context->m_window->draw(m_redoButtonBox);
    m_context->m_window->draw(m_passButtonBox);
    m_context->m_window->draw(m_pauseButtonBox);
}

void MainBoard::resetGame()
{
    m_game = std::make_unique<Game>(new Board());
    m_stones.clear();
    rebuildStonesFromGame();
}
