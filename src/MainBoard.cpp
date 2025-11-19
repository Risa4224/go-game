#include "MainBoard.hpp"
#include <SFML/Window/Event.hpp>
#include <iostream>

MainBoard::MainBoard(std::shared_ptr<Context>& context)
    : m_context(context)
    , m_boardBackground()
    , m_gridLines()
    , m_boardPixelSize(650.f)
    , m_boardSize(19)
    , m_cellSize(0.f)
    , m_boardTopLeft(0.f, 0.f)
    , m_stones()
    // Text: SFML 3.0.0 – dùng (font, string, size)
    // , m_titleText(m_context->m_assets->GetFont(MAIN_FONT), "Go Game", 40)
    // , m_hintText(m_context->m_assets->GetFont(MAIN_FONT),
    //              "ESC: Back  |  Left Click: place stone  |  Undo: Z / button  |  Redo: Y / button",
    //              18)
    // , m_undoText(m_context->m_assets->GetFont(MAIN_FONT), "Undo", 20)
    // , m_redoText(m_context->m_assets->GetFont(MAIN_FONT), "Redo", 20)
    , m_undoButtonBox()
    , m_redoButtonBox()
    , m_undoHovered(false)
    , m_redoHovered(false)
    , m_game(std::make_unique<Game>(new Board())) // Game own Board*
{
    std::cout << "[MainBoard] ctor\n";
}

void MainBoard::Init()
{
    std::cout << "[MainBoard] Init START\n";

    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winSizeF(static_cast<float>(winSize.x),
                          static_cast<float>(winSize.y));

    // --- Bàn gỗ ---
    m_boardBackground.setSize({ m_boardPixelSize, m_boardPixelSize });
    m_boardBackground.setFillColor({ 210, 164, 80 });
    m_boardBackground.setOutlineThickness(2.f);
    m_boardBackground.setOutlineColor(sf::Color::Black);

    m_boardTopLeft.x = (winSizeF.x - m_boardPixelSize) * 0.5f;
    m_boardTopLeft.y = (winSizeF.y - m_boardPixelSize) * 0.5f;
    m_boardBackground.setPosition(m_boardTopLeft);

    m_cellSize = m_boardPixelSize / static_cast<float>(m_boardSize - 1);

    // --- Vẽ lưới ---
    buildGrid();

    // --- Title (căn tương đối, không dùng getLocalBounds để tránh phức tạp) ---
    // m_titleText.setFillColor(sf::Color::Black);
    // m_titleText.setPosition(
    //     {m_boardTopLeft.x + m_boardPixelSize * 0.5f - 80.f,
    //     m_boardTopLeft.y - 50.f}
    // );

    // --- Hint ---
    // m_hintText.setFillColor(sf::Color::Black);
    // m_hintText.setPosition(
    //     {40.f,
    //     winSizeF.y - 40.f}
    // );

    // --- Undo / Redo buttons ---
    m_undoButtonBox.setSize({ 100.f, 40.f });
    m_undoButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_undoButtonBox.setPosition({ winSizeF.x - 150.f, 40.f });

    m_redoButtonBox.setSize({ 100.f, 40.f });
    m_redoButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_redoButtonBox.setPosition({ winSizeF.x - 150.f, 90.f });

    m_passButtonBox.setSize({ 100.f, 40.f });
    m_passButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_passButtonBox.setPosition({ winSizeF.x - 150.f, 140.f });
    // Text cho nút (căn tương đối đơn giản)
    // m_undoText.setFillColor(sf::Color::Black);
    // m_undoText.setPosition(
    //     {m_undoButtonBox.getPosition().x + 20.f,
    //     m_undoButtonBox.getPosition().y + 8.f}
    // );

    // m_redoText.setFillColor(sf::Color::Black);
    // m_redoText.setPosition(
    //     {m_redoButtonBox.getPosition().x + 20.f,
    //     m_redoButtonBox.getPosition().y + 8.f}
    // );

    // --- Reset game logic & stones ---
    if (!m_game)
        m_game = std::make_unique<Game>(new Board());
    // Game đang ở trạng thái ban đầu (board rỗng)
    rebuildStonesFromGame();

    std::cout << "[MainBoard] Init END\n";
}

// Vẽ lưới
void MainBoard::buildGrid()
{
    m_gridLines.clear();
    m_gridLines.reserve(static_cast<std::size_t>(m_boardSize * 4));

    // Vertical lines
    for (int i = 0; i < m_boardSize; ++i)
    {
        float x = m_boardTopLeft.x + i * m_cellSize;

        sf::Vertex v1{};
        v1.position = { x, m_boardTopLeft.y };
        v1.color    = sf::Color::Black;
        m_gridLines.push_back(v1);

        sf::Vertex v2{};
        v2.position = { x, m_boardTopLeft.y + m_boardPixelSize };
        v2.color    = sf::Color::Black;
        m_gridLines.push_back(v2);
    }

    // Horizontal lines
    for (int j = 0; j < m_boardSize; ++j)
    {
        float y = m_boardTopLeft.y + j * m_cellSize;

        sf::Vertex v1{};
        v1.position = { m_boardTopLeft.x, y };
        v1.color    = sf::Color::Black;
        m_gridLines.push_back(v1);

        sf::Vertex v2{};
        v2.position = { m_boardTopLeft.x + m_boardPixelSize, y };
        v2.color    = sf::Color::Black;
        m_gridLines.push_back(v2);
    }
}

// Tạo lại các quân cờ để vẽ từ Game::Board
void MainBoard::rebuildStonesFromGame()
{
    m_stones.clear();

    if (!m_game) return;
    Board* board = m_game->getBoard();
    if (!board) return;

    int size = board->getSize();
    float radius = m_cellSize * 0.4f;

    for (int x = 0; x < size; ++x)
    {
        for (int y = 0; y < size; ++y)
        {
            PieceColor c = board->getPiece(x, y);
            if (c == NONE) continue;

            sf::CircleShape stone(radius);
            stone.setOrigin({ radius, radius });

            float px = m_boardTopLeft.x + x * m_cellSize;
            float py = m_boardTopLeft.y + y * m_cellSize;
            stone.setPosition({ px, py });

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

// Xử lý click trái: chuyển pixel → tọa độ lưới → gọi Game::placeStone
void MainBoard::handleLeftClick(const sf::Vector2i& pixelPos)
{
    sf::Vector2f posF(static_cast<float>(pixelPos.x),
                      static_cast<float>(pixelPos.y));

    // Ngoài vùng bàn cờ thì bỏ
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

    if (!m_game) return;

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
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
            {
                // ESC từ MainBoard: pop state này → quay lại ModeSelection
                m_context->m_states->PopCurrent();
                return;
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::Z)
            {
                // Undo bằng phím
                if (m_game && m_game->undo())
                {
                    rebuildStonesFromGame();
                }
            }
            else if (keyPressed->scancode == sf::Keyboard::Scancode::Y)
            {
                // Redo bằng phím
                if (m_game && m_game->redo())
                {
                    rebuildStonesFromGame();
                }
            }
        }
        else if (const auto* mouseMoved = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos(
                static_cast<float>(mouseMoved->position.x),
                static_cast<float>(mouseMoved->position.y)
            );

            m_undoHovered = m_undoButtonBox.getGlobalBounds().contains(mousePos);
            m_redoHovered = m_redoButtonBox.getGlobalBounds().contains(mousePos);
            m_passHovered = m_passButtonBox.getGlobalBounds().contains(mousePos);
        }
        else if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePosF(
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y)
                );
                sf::Vector2i mousePos(mouseBtn->position.x, mouseBtn->position.y);

                // Click Undo
                if (m_undoButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game && m_game->undo())
                    {
                        rebuildStonesFromGame();
                    }
                    continue;
                }

                // Click Redo
                if (m_redoButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game && m_game->redo())
                    {
                        rebuildStonesFromGame();
                    }
                    continue;
                }

                //Click Pass
                if (m_redoButtonBox.getGlobalBounds().contains(mousePosF))
                {
                    if (m_game && m_game->pass())
                    {
                        rebuildStonesFromGame();
                    }
                    continue;
                }
                // Click lên bàn cờ → đặt quân
                handleLeftClick(mousePos);
            }
        }
    }
}

void MainBoard::Update(sf::Time)
{
    // Update màu hover cho nút
    m_undoButtonBox.setFillColor(
        m_undoHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200)
    );
    m_redoButtonBox.setFillColor(
        m_redoHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200)
    );
    m_passButtonBox.setFillColor(
    m_passHovered ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200)
    );
}

void MainBoard::Draw()
{
    m_context->m_window->clear(sf::Color(30, 30, 30));

    // Bàn nền
    m_context->m_window->draw(m_boardBackground);

    // Lưới
    if (!m_gridLines.empty())
    {
        m_context->m_window->draw(
            m_gridLines.data(),
            m_gridLines.size(),
            sf::PrimitiveType::Lines
        );
    }

    // Quân cờ từ logic Game
    for (const auto& stone : m_stones)
        m_context->m_window->draw(stone);

    // Nút Undo / Redo
    m_context->m_window->draw(m_undoButtonBox);
    m_context->m_window->draw(m_redoButtonBox);
    m_context->m_window->draw(m_passButtonBox);
    // m_context->m_window->draw(m_undoText);
    // m_context->m_window->draw(m_redoText);

    // // Text Title + Hint
    // m_context->m_window->draw(m_titleText);
    // m_context->m_window->draw(m_hintText);

    m_context->m_window->display();
}
