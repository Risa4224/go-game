#include "MainBoard.hpp"
#include <SFML/Window/Event.hpp>
#include <cmath> // std::round

MainBoard::MainBoard(std::shared_ptr<Context>& context)
    : m_context{context}
    , m_boardBackground{}
    , m_gridLines{}
    , m_titleText(std::nullopt)
    , m_hintText(std::nullopt)
    , m_boardSize(19)
    , m_boardTopLeft{0.f, 0.f}
    , m_boardPixelSize(0.f)
    , m_cellSize(0.f)
    , m_boardState{}
    , m_stones{}
    , m_isBlackTurn(true)
{
}

MainBoard::~MainBoard() = default;

void MainBoard::Init()
{
    auto& font = m_context->m_assets->GetFont(MAIN_FONT);

    // ----- Title -----
    std::string modeStr =
        (m_context->m_gameMode == GameMode::TwoPlayers ?
            "Go Board - 2 Players" : "Go Board - AI Mode");

    m_titleText.emplace(font, modeStr, 40);
    m_hintText.emplace(font, "ESC: Back  |  L-Click: place stone", 20);

    const auto win = m_context->m_window->getSize();
    const float cx = static_cast<float>(win.x) * 0.5f;
    const float cy = static_cast<float>(win.y) * 0.5f;

    // layout title
    {
        auto tb = m_titleText->getLocalBounds();
        m_titleText->setOrigin(tb.getCenter());
        m_titleText->setPosition({cx, 60.f});
    }

    // layout hint
    {
        auto hb = m_hintText->getLocalBounds();
        m_hintText->setOrigin(hb.getCenter());
        m_hintText->setPosition({cx, static_cast<float>(win.y) - 50.f});
    }

    // ----- Bàn nền -----
    m_boardPixelSize = 650.f;

    m_boardBackground.setSize({m_boardPixelSize, m_boardPixelSize});
    auto bb = m_boardBackground.getLocalBounds();
    m_boardBackground.setOrigin(bb.getCenter());
    m_boardBackground.setPosition({cx, cy});
    m_boardBackground.setFillColor(sf::Color(210, 164, 80));
    m_boardBackground.setOutlineThickness(3.f);
    m_boardBackground.setOutlineColor(sf::Color::Black);

    // Góc trên trái vùng lưới
    m_boardTopLeft = {
        m_boardBackground.getPosition().x - m_boardPixelSize * 0.5f,
        m_boardBackground.getPosition().y - m_boardPixelSize * 0.5f
    };

    // khoảng cách giữa các đường
    m_cellSize = m_boardPixelSize / static_cast<float>(m_boardSize - 1);

    // ----- Sinh lưới -----
    m_gridLines.clear();

    // Vertical
    for (unsigned int i = 0; i < m_boardSize; ++i)
    {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        float x = m_boardTopLeft.x + i * m_cellSize;

        line[0].position = {x, m_boardTopLeft.y};
        line[1].position = {x, m_boardTopLeft.y + m_boardPixelSize};
        line[0].color = sf::Color::Black;
        line[1].color = sf::Color::Black;

        m_gridLines.push_back(line);
    }

    // Horizontal
    for (unsigned int j = 0; j < m_boardSize; ++j)
    {
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        float y = m_boardTopLeft.y + j * m_cellSize;

        line[0].position = {m_boardTopLeft.x, y};
        line[1].position = {m_boardTopLeft.x + m_boardPixelSize, y};
        line[0].color = sf::Color::Black;
        line[1].color = sf::Color::Black;

        m_gridLines.push_back(line);
    }

    // ----- Trạng thái bàn -----
    m_boardState.assign(m_boardSize, std::vector<int>(m_boardSize, 0));
    m_stones.clear();
    m_isBlackTurn = true;
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

        // ESC: back
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->scancode == sf::Keyboard::Scancode::Escape)
            {
                m_context->m_states->PopCurrent();
                return;
            }
        }

        // Click chuột trái để đặt quân
        if (const auto* mouseBtn = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mouseBtn->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePos{
                    static_cast<float>(mouseBtn->position.x),
                    static_cast<float>(mouseBtn->position.y)
                };

                // ngoài vùng bàn
                if (mousePos.x < m_boardTopLeft.x ||
                    mousePos.y < m_boardTopLeft.y ||
                    mousePos.x > m_boardTopLeft.x + m_boardPixelSize ||
                    mousePos.y > m_boardTopLeft.y + m_boardPixelSize)
                {
                    continue;
                }

                float relX = mousePos.x - m_boardTopLeft.x;
                float relY = mousePos.y - m_boardTopLeft.y;

                int ix = static_cast<int>(std::round(relX / m_cellSize));
                int iy = static_cast<int>(std::round(relY / m_cellSize));

                if (ix < 0) ix = 0;
                if (iy < 0) iy = 0;
                if (ix >= static_cast<int>(m_boardSize)) ix = m_boardSize - 1;
                if (iy >= static_cast<int>(m_boardSize)) iy = m_boardSize - 1;

                // đã có quân
                if (m_boardState[iy][ix] != 0)
                    continue;

                int colorCode = m_isBlackTurn ? 1 : 2;
                m_boardState[iy][ix] = colorCode;

                float radius = m_cellSize * 0.4f;
                sf::CircleShape stone(radius);
                stone.setOrigin({radius, radius});

                sf::Vector2f stonePos = {
                    m_boardTopLeft.x + ix * m_cellSize,
                    m_boardTopLeft.y + iy * m_cellSize
                };
                stone.setPosition(stonePos);

                if (m_isBlackTurn)
                {
                    stone.setFillColor(sf::Color::Black);
                    stone.setOutlineThickness(1.f);
                    stone.setOutlineColor(sf::Color(220, 220, 220));
                }
                else
                {
                    stone.setFillColor(sf::Color::White);
                    stone.setOutlineThickness(1.f);
                    stone.setOutlineColor(sf::Color::Black);
                }

                m_stones.push_back(stone);
                m_isBlackTurn = !m_isBlackTurn;
            }
        }
    }
}

void MainBoard::Update(sf::Time) { }

void MainBoard::Draw()
{
    m_context->m_window->clear(sf::Color(30, 30, 30));

    m_context->m_window->draw(m_boardBackground);

    for (auto& line : m_gridLines)
        m_context->m_window->draw(line);

    for (auto& stone : m_stones)
        m_context->m_window->draw(stone);

    if (m_titleText) m_context->m_window->draw(*m_titleText);
    if (m_hintText)  m_context->m_window->draw(*m_hintText);

    m_context->m_window->display();
}
