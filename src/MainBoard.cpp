#include "MainBoard.hpp"

#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/PrimitiveType.hpp> 

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "AI.h"
#include "PauseState.hpp"

namespace fs = std::filesystem;

namespace
{
    enum class SaveLoadMode
    {
        Save,
        Load
    };

    struct SaveLoadMenuState
    {
        bool open = false;
        SaveLoadMode mode = SaveLoadMode::Save;

        std::vector<std::string> files;
        int selected = -1;

        int scroll = 0;              // in rows
        int visibleRows = 10;
        float rowHeight = 28.f;

        // Visual layout constants
        sf::Vector2f panelSize = {520.f, 440.f};
        float btnWidth = 120.f;
        float btnHeight = 36.f;

        void refresh()
        {
            files = Game::listSaveFiles();
            if (files.empty())
            {
                selected = -1;
                scroll = 0;
                return;
            }

            if (selected < 0) selected = 0;
            if (selected >= static_cast<int>(files.size())) selected = static_cast<int>(files.size()) - 1;
            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(files.size()) - visibleRows));
        }

        void clampScroll()
        {
            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(files.size()) - visibleRows));
        }
    };

    SaveLoadMenuState g_saveLoad;

    // Best-effort deletion
// DEBUG VERSION: Best-effort deletion
    bool tryDeleteSaveFile(const std::string& filename)
    {
        std::error_code ec;
        bool deleted = false;
        
        // Define the list of paths/extensions to try
        // Adjust ".txt" or ".go" depending on what your Game::saveToFile uses!
        std::vector<fs::path> pathsToTry;
        
        // 1. Try exact name in current folder
        pathsToTry.push_back(fs::path(filename));
        // 2. Try exact name in "saves" folder
        pathsToTry.push_back(fs::path("saves") / filename);
        
        // 3. Try with .txt extension (if your game saves as .txt)
        pathsToTry.push_back(fs::path(filename).replace_extension(".txt"));
        pathsToTry.push_back((fs::path("saves") / filename).replace_extension(".txt"));

        // 4. Try with .go extension
        pathsToTry.push_back(fs::path(filename).replace_extension(".go"));
        pathsToTry.push_back((fs::path("saves") / filename).replace_extension(".go"));

        std::cout << "[Delete Debug] Request to delete: " << filename << "\n";

        for (const auto& p : pathsToTry)
        {
            // Check if exists first to avoid confusing error logs for non-existent guesses
            if (fs::exists(p, ec)) 
            {
                std::cout << "   Found file at: " << p << ". Deleting... ";
                if (fs::remove(p, ec)) 
                {
                    std::cout << "SUCCESS.\n";
                    deleted = true;
                    // Keep going? Usually we stop, but maybe you have a .meta file too? 
                    // Let's stop on first success for now.
                    return true; 
                }
                else
                {
                    std::cout << "FAILED. Error: " << ec.message() << "\n";
                }
            }
            else
            {
                // Uncomment this if you are really stuck to see where it looked
                // std::cout << "   Checked: " << p << " (Not found)\n";
            }
        }

        if (!deleted) {
            std::cout << "[Delete Debug] Could not find or delete any matching file.\n";
            std::cout << "               Current Working Directory is: " << fs::current_path() << "\n";
        }

        return deleted;
    }
} // namespace

MainBoard::MainBoard(std::shared_ptr<Context> &context)
    : m_context(context),
      m_boardBackground(),
      m_gridLines(),
      m_boardPixelSize(650.f),
      m_boardSize(19),
      m_cellSize(0.f),
      m_boardTopLeft(0.f, 0.f),
      m_stones(),
      m_undoButtonBox(), m_redoButtonBox(), m_passButtonBox(), m_pauseButtonBox(), m_saveButtonBox(), m_loadButtonBox(),
      m_undoHovered(false), m_redoHovered(false), m_passHovered(false), m_pauseHovered(false), m_saveHovered(false), m_loadHovered(false),
      m_game(std::make_unique<Game>(new Board())),
      m_placeSound(m_context->m_assets->GetSoundBuffer(STONEPLACE_SOUND)),
      m_passSound(m_context->m_assets->GetSoundBuffer(PASS_SOUND)),
      m_invalidSound(m_context->m_assets->GetSoundBuffer(INVALID_SOUND)),
      m_winSound(m_context->m_assets->GetSoundBuffer(WIN_SOUND))
{
    std::cout << "[MainBoard] ctor\n";
}

void MainBoard::Init()
{
    std::cout << "[MainBoard] Init START\n";

    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winSizeF(static_cast<float>(winSize.x),
                          static_cast<float>(winSize.y));

    // --- 1. Load Font & Setup Text ---
    auto& font = m_context->m_assets->GetFont(MAIN_FONT);

    // Menu Overlay Texts
    m_menuTitleText.emplace(font, "Game Menu", 24);
    m_menuDeleteText.emplace(font, "Delete", 16);
    m_menuCancelText.emplace(font, "Cancel", 16);
    m_menuActionText.emplace(font, "Action", 16);
    m_fileListText.emplace(font, "", 18);

    // --- 2. Board Setup ---
    m_boardBackground.setSize({m_boardPixelSize, m_boardPixelSize});
    m_boardBackground.setFillColor({210, 164, 80});
    m_boardBackground.setOutlineThickness(2.f);
    m_boardBackground.setOutlineColor(sf::Color::Black);

    m_boardTopLeft.x = (winSizeF.x - m_boardPixelSize) * 0.5f;
    m_boardTopLeft.y = (winSizeF.y - m_boardPixelSize) * 0.5f;
    m_boardBackground.setPosition(m_boardTopLeft);

    m_hasClassicTexture = m_boardTextureClassic.loadFromFile("assets/texture/light-wood.jpg");
    m_hasDarkTexture = m_boardTextureDark.loadFromFile("assets/texture/dark-stone.jpg");

    m_cellSize = m_boardPixelSize / static_cast<float>(m_boardSize - 1);

    buildGrid();

    // --- 3. UI Buttons Positioning ---
    float btnX = winSizeF.x - 150.f;
    
    auto setupBtn = [&](sf::RectangleShape& box, float yPos) {
        box.setSize({100.f, 40.f});
        box.setFillColor(sf::Color(200,200,200));
        box.setPosition({btnX, yPos}); 
    };

    setupBtn(m_undoButtonBox,   40.f);
    setupBtn(m_redoButtonBox,   90.f);
    setupBtn(m_passButtonBox,  140.f);
    setupBtn(m_pauseButtonBox, 190.f);
    setupBtn(m_saveButtonBox,  240.f);
    setupBtn(m_loadButtonBox,  290.f);

    if (!m_game)
        m_game = std::make_unique<Game>(new Board());

    // Load stone textures once (optional: falls back to flat colors if files are missing).
    loadStoneTexturesFromFiles();

    // Cache the current stone theme so we can refresh visuals if the user changes it in Settings.
    if (m_context)
        m_lastStoneTheme = m_context->m_stoneTheme;

    rebuildStonesFromGame();
    maybeRunAITurn();

    std::cout << "[MainBoard] Init END\n";
}

void MainBoard::loadStoneTexturesFromFiles()
{
    // NOTE: Shapes keep a pointer to the texture (sf::Shape::setTexture takes a Texture*),
    // so the textures must live as long as the shapes do.
    // We'll try to load all theme variants; missing files are fine (we'll fallback to colors).

    auto tryLoad = [](sf::Texture& tex, bool& flag, const fs::path& path)
    {
        flag = tex.loadFromFile(path);
        if (flag)
            tex.setSmooth(true);
        else
            std::cout << "[MainBoard] Stone texture missing or failed to load: " << path.string() << "\n";
    };

    // Default expected layout (relative to working directory)
    //   assets/stones/
    //     classic_black.png, classic_white.png
    //     slate_shell_black.png, slate_shell_white.png
    //     glass_black.png,  glass_white.png
    tryLoad(m_stoneTexClassicBlack,     m_hasStoneTexClassicBlack,     fs::path("assets/stones/classic_black.png"));
    tryLoad(m_stoneTexClassicWhite,     m_hasStoneTexClassicWhite,     fs::path("assets/stones/classic_white.png"));
    tryLoad(m_stoneTexSlateShellBlack,  m_hasStoneTexSlateShellBlack,  fs::path("assets/stones/slate_shell_black.png"));
    tryLoad(m_stoneTexSlateShellWhite,  m_hasStoneTexSlateShellWhite,  fs::path("assets/stones/slate_shell_white.png"));
    tryLoad(m_stoneTexGlassBlack,       m_hasStoneTexGlassBlack,       fs::path("assets/stones/glass_black.png"));
    tryLoad(m_stoneTexGlassWhite,       m_hasStoneTexGlassWhite,       fs::path("assets/stones/glass_white.png"));
}

const sf::Texture* MainBoard::getStoneTexture(StoneTheme theme, PieceColor c) const
{
    const bool wantBlack = (c == BLACK);

    switch (theme)
    {
        case StoneTheme::SlateShell:
            if (wantBlack) return m_hasStoneTexSlateShellBlack ? &m_stoneTexSlateShellBlack : nullptr;
            return m_hasStoneTexSlateShellWhite ? &m_stoneTexSlateShellWhite : nullptr;

        case StoneTheme::Glass:
            if (wantBlack) return m_hasStoneTexGlassBlack ? &m_stoneTexGlassBlack : nullptr;
            return m_hasStoneTexGlassWhite ? &m_stoneTexGlassWhite : nullptr;

        case StoneTheme::Classic:
        default:
            if (wantBlack) return m_hasStoneTexClassicBlack ? &m_stoneTexClassicBlack : nullptr;
            return m_hasStoneTexClassicWhite ? &m_stoneTexClassicWhite : nullptr;
    }
}

void MainBoard::applyStoneVisual(sf::CircleShape& stone, PieceColor c) const
{
    // Fallback to classic if context is missing.
    const StoneTheme theme = m_context ? m_context->m_stoneTheme : StoneTheme::Classic;

    // If textures exist for this theme/color, prefer them.
    if (const sf::Texture* tex = getStoneTexture(theme, c))
    {
        // SFML shapes modulate the texture with fill color; keep it white to show texture unmodified.
        stone.setFillColor(sf::Color::White);
        stone.setTexture(tex, true);

        // Keep light outlines so stones still read clearly on both boards.
        switch (theme)
        {
            case StoneTheme::SlateShell:
                stone.setOutlineThickness(2.f);
                stone.setOutlineColor(sf::Color(120, 120, 120, 220));
                break;
            case StoneTheme::Glass:
                stone.setOutlineThickness(2.5f);
                stone.setOutlineColor(sf::Color(220, 220, 255, 160));
                break;
            case StoneTheme::Classic:
            default:
                stone.setOutlineThickness(1.5f);
                stone.setOutlineColor(sf::Color(30, 30, 30, 180));
                break;
        }
        return;
    }

    // No texture available => disable texturing and use the original color-based styling.
    stone.setTexture(nullptr, true);

    switch (theme)
    {
        case StoneTheme::SlateShell:
        {
            if (c == BLACK)
            {
                stone.setFillColor(sf::Color(28, 28, 28));
                stone.setOutlineThickness(2.f);
                stone.setOutlineColor(sf::Color(130, 130, 130));
            }
            else
            {
                stone.setFillColor(sf::Color(245, 241, 234));
                stone.setOutlineThickness(2.f);
                stone.setOutlineColor(sf::Color(130, 130, 130));
            }
        } break;

        case StoneTheme::Glass:
        {
            if (c == BLACK)
            {
                stone.setFillColor(sf::Color(8, 10, 18));
                stone.setOutlineThickness(3.f);
                stone.setOutlineColor(sf::Color(230, 230, 255, 180));
            }
            else
            {
                stone.setFillColor(sf::Color(252, 252, 255));
                stone.setOutlineThickness(2.5f);
                stone.setOutlineColor(sf::Color(60, 60, 80, 200));
            }
        } break;

        case StoneTheme::Classic:
        default:
        {
            if (c == BLACK)
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
        } break;
    }
}

bool MainBoard::isAIMode() const { return m_context && (m_context->m_gameMode == GameMode::AiVsPlayer); }
PieceColor MainBoard::humanColor() const { return (!m_context || m_context->m_humanPlaysBlack) ? BLACK : WHITE; }
PieceColor MainBoard::aiColor() const { return (humanColor() == BLACK) ? WHITE : BLACK; }

void MainBoard::handleGameOver() {
    if (!m_game) return;
    auto [b, w] = m_game->calculateFinalScore();
    std::string msg = (b > w) ? "Black wins!" : (w > b) ? "White wins!" : "Draw!";
    msg += " B:" + std::to_string(b) + " W:" + std::to_string(w);
    setNotification(msg);
    m_winSound.play();
    m_context->m_states->Add(std::make_unique<PauseState>(m_context, PauseState::Mode::GameOver, msg), false);
}

void MainBoard::maybeRunAITurn() {
    if (!isAIMode() || !m_game || m_game->getTurn() != aiColor()) return;
    AIMove mv = GoAI::computeAIMove(*m_game, m_context->m_aiDifficulty);
    if (mv.isPass) {
        if(m_game->pass()) handleGameOver();
        else { setNotification("AI Passed"); rebuildStonesFromGame(); m_passSound.play(); }
        return;
    }
    if (m_game->placeStone(mv.x, mv.y)) {
        m_placeSound.play(); rebuildStonesFromGame();
        setNotification("AI Played (" + std::to_string(mv.x) + "," + std::to_string(mv.y) + ")");
    } else {
        if(m_game->pass()) handleGameOver();
    }
}

void MainBoard::setNotification(const std::string &msg)
{
    m_notificationText = msg;
    m_showNotification = !msg.empty();
    m_notificationClock.restart();
}

void MainBoard::buildGrid()
{
    m_gridLines.clear();
    for (int i = 0; i < m_boardSize; ++i) {
        float x = m_boardTopLeft.x + i * m_cellSize;
        m_gridLines.push_back({{x, m_boardTopLeft.y}, sf::Color::Black});
        m_gridLines.push_back({{x, m_boardTopLeft.y + m_boardPixelSize}, sf::Color::Black});
    }
    for (int j = 0; j < m_boardSize; ++j) {
        float y = m_boardTopLeft.y + j * m_cellSize;
        m_gridLines.push_back({{m_boardTopLeft.x, y}, sf::Color::Black});
        m_gridLines.push_back({{m_boardTopLeft.x + m_boardPixelSize, y}, sf::Color::Black});
    }
}

void MainBoard::rebuildStonesFromGame()
{
    m_stones.clear();
    if (!m_game || !m_game->getBoard()) return;
    Board *board = m_game->getBoard();
    int size = board->getSize();
    float radius = m_cellSize * 0.4f;

    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            PieceColor c = board->getPiece(x, y);
            if (c == NONE) continue;
            sf::CircleShape stone(radius);
            stone.setOrigin({radius, radius});
            stone.setPosition({m_boardTopLeft.x + x * m_cellSize, m_boardTopLeft.y + y * m_cellSize});
            applyStoneVisual(stone, c);
            m_stones.push_back(stone);
        }
    }
}

void MainBoard::handleLeftClick(const sf::Vector2i &pixelPos)
{
    sf::Vector2f posF((float)pixelPos.x, (float)pixelPos.y);
    if (!m_boardBackground.getGlobalBounds().contains(posF)) return;

    float localX = posF.x - m_boardTopLeft.x;
    float localY = posF.y - m_boardTopLeft.y;
    int ix = (int)(localX / m_cellSize + 0.5f);
    int iy = (int)(localY / m_cellSize + 0.5f);

    if (ix < 0 || ix >= m_boardSize || iy < 0 || iy >= m_boardSize || !m_game) return;
    if (isAIMode() && m_game->getTurn() != humanColor()) { setNotification("Wait for AI"); return; }

    if (m_game->placeStone(ix, iy)) {
        m_placeSound.play();
        rebuildStonesFromGame();
        std::string msg = (m_game->getTurn() == WHITE ? "Black" : "White");
        msg += " (" + std::to_string(ix) + "," + std::to_string(iy) + ")";
        if (m_game->lastMoveCreatedKoThreat()) msg += " (Ko)";
        setNotification(msg);
        maybeRunAITurn();
    } else {
        m_invalidSound.play();
        if(m_game->lastMoveWasKoViolation()) setNotification("Invalid: Ko Violation");
        else if(m_game->lastMoveWasSuicide()) setNotification("Invalid: Suicide");
        else setNotification("Invalid Move");
    }
}

void MainBoard::Update(sf::Time)
{
    // If the user changed stone theme in Settings, update visuals immediately.
    if (m_context && m_context->m_stoneTheme != m_lastStoneTheme)
    {
        m_lastStoneTheme = m_context->m_stoneTheme;
        rebuildStonesFromGame();
    }

    if (m_context->m_requestBoardRestart) {
        m_context->m_requestBoardRestart = false;
        resetGame();
    }

    // Button Hover
    auto updateColor = [&](sf::RectangleShape& box, bool hover) {
        box.setFillColor(hover ? sf::Color(230, 230, 230) : sf::Color(200, 200, 200));
    };
    updateColor(m_undoButtonBox, m_undoHovered);
    updateColor(m_redoButtonBox, m_redoHovered);
    updateColor(m_passButtonBox, m_passHovered);
    updateColor(m_pauseButtonBox, m_pauseHovered);
    updateColor(m_saveButtonBox, m_saveHovered);
    updateColor(m_loadButtonBox, m_loadHovered);

    // Notification Timer
    if (m_showNotification) {
        if (m_notificationClock.getElapsedTime().asSeconds() > m_notificationDuration) {
            m_showNotification = false;
            m_notificationText.clear();
        }
    }
}

void MainBoard::Draw()
{
    sf::RenderWindow& window = *m_context->m_window;
    auto winSize = window.getSize();

    // 1. Theme Logic
    sf::Color gridColor = sf::Color::Black;
    sf::Color boardColor = sf::Color(210, 164, 80);

    if (m_context->m_boardTheme == BoardTheme::Dark) {
        gridColor = sf::Color(200, 200, 200);
        boardColor = sf::Color(40, 40, 40);
        if (m_hasDarkTexture) {
            m_boardBackground.setTexture(&m_boardTextureDark);
            m_boardBackground.setFillColor(sf::Color::White);
        } else {
            m_boardBackground.setTexture(nullptr);
            m_boardBackground.setFillColor(boardColor);
        }
    } else {
        if (m_hasClassicTexture) {
            m_boardBackground.setTexture(&m_boardTextureClassic);
            m_boardBackground.setFillColor(sf::Color::White);
        } else {
            m_boardBackground.setTexture(nullptr);
            m_boardBackground.setFillColor(boardColor);
        }
    }

    for (auto& v : m_gridLines) v.color = gridColor;

    window.draw(m_boardBackground);
    if (!m_gridLines.empty()) {
        window.draw(m_gridLines.data(), m_gridLines.size(), sf::PrimitiveType::Lines);
    }
    for (const auto& s : m_stones) window.draw(s);

    // 2. Draw Side Panel
    sf::RectangleShape sidePanel;
    sidePanel.setSize({160.f, 340.f});
    sidePanel.setFillColor(sf::Color(40, 40, 40, 220));
    sidePanel.setPosition({(float)winSize.x - 170.f, 30.f});
    sidePanel.setOutlineThickness(1.f);
    sidePanel.setOutlineColor(sf::Color(80, 80, 80));
    window.draw(sidePanel);

    // 3. Draw Buttons & Text
    const auto& font = m_context->m_assets->GetFont(MAIN_FONT);
    auto drawBtn = [&](sf::RectangleShape& box, const std::string& str) {
        window.draw(box);
        sf::Text txt(font, str, 18);
        txt.setFillColor(sf::Color::Black);
        sf::FloatRect b = txt.getLocalBounds();
        txt.setOrigin({b.position.x + b.size.x/2.f, b.position.y + b.size.y/2.f});
        sf::Vector2f c = box.getPosition() + box.getSize()/2.f;
        txt.setPosition(c);
        window.draw(txt);
    };

    drawBtn(m_undoButtonBox, "Undo");
    drawBtn(m_redoButtonBox, "Redo");
    drawBtn(m_passButtonBox, "Pass");
    drawBtn(m_pauseButtonBox, "Pause");
    drawBtn(m_saveButtonBox, "Save");
    drawBtn(m_loadButtonBox, "Load");

    // 4. Draw Turn Panel
    if (m_game) {
        PieceColor current = m_game->getTurn();
        std::string turnStr = (current == BLACK ? "Turn: Black" : (current == WHITE ? "Turn: White" : "Turn: -"));
        
        sf::RectangleShape turnPanel({160.f, 40.f});
        turnPanel.setFillColor(sf::Color(30, 30, 30, 220));
        turnPanel.setOutlineThickness(1.f);
        turnPanel.setOutlineColor(sf::Color(80, 80, 80));
        turnPanel.setPosition({20.f, 20.f});
        window.draw(turnPanel);

        sf::Text turnText(font, turnStr, 18);
        turnText.setFillColor(sf::Color::White);
        turnText.setStyle(sf::Text::Bold);
        turnText.setPosition({35.f, 25.f});
        window.draw(turnText);

        sf::CircleShape turnStone(10.f);
        turnStone.setOrigin({10.f, 10.f});
        applyStoneVisual(turnStone, current);
        turnStone.setPosition({160.f, 40.f});
        window.draw(turnStone);
    }

    // 5. Draw Bottom Hints
    sf::RectangleShape bottomBar({(float)winSize.x, 30.f});
    bottomBar.setPosition({0.f, (float)winSize.y - 30.f});
    bottomBar.setFillColor(sf::Color(30, 30, 30, 230));
    window.draw(bottomBar);

    sf::Text hint(font, "ESC: Back | Click: Place | Z: Undo | Y: Redo | P: Pass", 16);
    hint.setFillColor(sf::Color::White);
    hint.setStyle(sf::Text::Bold);
    hint.setPosition({10.f, (float)winSize.y - 30.f + 5.f});
    window.draw(hint);

    // 6. Draw Notification Bar
    if (m_showNotification && !m_notificationText.empty())
    {
        sf::RectangleShape notifBar({(float)winSize.x, 24.f});
        notifBar.setPosition({0.f, (float)winSize.y - 30.f - 24.f});
        notifBar.setFillColor(sf::Color(50, 50, 50, 230));
        notifBar.setOutlineThickness(1.f);
        notifBar.setOutlineColor(sf::Color(200, 200, 0, 180));
        window.draw(notifBar);

        sf::Text notifText(font, m_notificationText, 16);
        notifText.setFillColor(sf::Color(255, 255, 180));
        notifText.setStyle(sf::Text::Bold);
        notifText.setPosition({10.f, (float)winSize.y - 30.f - 24.f + 4.f});
        window.draw(notifText);
    }

    // 7. Draw Save/Load Overlay
    if (g_saveLoad.open)
    {
        sf::RectangleShape dimmer({(float)winSize.x, (float)winSize.y});
        dimmer.setFillColor(sf::Color(0,0,0,150));
        window.draw(dimmer);

        sf::Vector2f pSize = g_saveLoad.panelSize;
        sf::Vector2f pPos(((float)winSize.x - pSize.x)/2.f, ((float)winSize.y - pSize.y)/2.f);
        sf::RectangleShape panel(pSize);
        panel.setPosition(pPos);
        panel.setFillColor(sf::Color(50,50,50));
        panel.setOutlineThickness(2.f);
        panel.setOutlineColor(sf::Color::White);
        window.draw(panel);

        // Header
        if(m_menuTitleText) {
            m_menuTitleText->setString(g_saveLoad.mode == SaveLoadMode::Save ? "Save Game" : "Load Game");
            sf::FloatRect b = m_menuTitleText->getLocalBounds();
            m_menuTitleText->setOrigin({b.position.x + b.size.x/2.f, b.position.y + b.size.y/2.f});
            m_menuTitleText->setPosition({pPos.x + pSize.x/2.f, pPos.y + 30.f});
            m_menuTitleText->setFillColor(sf::Color::White);
            window.draw(*m_menuTitleText);
        }

        // File List
        float listY = pPos.y + 70.f;
        float listW = pSize.x - 40.f;
        sf::RectangleShape listBg({listW, g_saveLoad.rowHeight * g_saveLoad.visibleRows});
        listBg.setPosition({pPos.x+20.f, listY});
        listBg.setFillColor(sf::Color(30,30,30));
        window.draw(listBg);

        if(m_fileListText) {
            m_fileListText->setFillColor(sf::Color::White);
            for(int i=0; i<g_saveLoad.visibleRows; ++i) {
                int idx = g_saveLoad.scroll + i;
                if(idx >= (int)g_saveLoad.files.size()) break;
                
                if(idx == g_saveLoad.selected) {
                    sf::RectangleShape hl({listW, g_saveLoad.rowHeight});
                    hl.setPosition({pPos.x+20.f, listY + i*g_saveLoad.rowHeight});
                    hl.setFillColor(sf::Color(100,100,150));
                    window.draw(hl);
                }

                m_fileListText->setString(g_saveLoad.files[idx]);
                m_fileListText->setOrigin({0,0});
                m_fileListText->setPosition({pPos.x+25.f, listY + i*g_saveLoad.rowHeight + 4.f});
                window.draw(*m_fileListText);
            }
        }

        // --- HINT TEXT ---
        sf::Text menuHint(font, "Hints: Click file to select | Double-click to Action | ESC to Close", 14);
        menuHint.setFillColor(sf::Color(200, 200, 200));
        menuHint.setPosition({pPos.x + 25.f, listY + g_saveLoad.rowHeight * g_saveLoad.visibleRows + 10.f});
        window.draw(menuHint);

        // Buttons
        float btnY = pPos.y + pSize.y - 60.f;
        auto drawMenuBtn = [&](float x, std::optional<sf::Text>& txt, std::string label, sf::Color bg) {
            sf::RectangleShape r({g_saveLoad.btnWidth, g_saveLoad.btnHeight});
            r.setPosition({x, btnY});
            r.setFillColor(bg);
            window.draw(r);
            if (txt) {
                txt->setString(label);
                sf::FloatRect b = txt->getLocalBounds();
                txt->setOrigin({b.position.x + b.size.x/2.f, b.position.y + b.size.y/2.f});
                txt->setPosition({x + g_saveLoad.btnWidth/2.f, btnY + g_saveLoad.btnHeight/2.f});
                txt->setFillColor(sf::Color::Black);
                window.draw(*txt);
            }
        };

        drawMenuBtn(pPos.x + 20.f, m_menuDeleteText, "Delete", sf::Color(150, 50, 50));
        drawMenuBtn(pPos.x + (pSize.x - g_saveLoad.btnWidth)*0.5f, m_menuCancelText, "Cancel", sf::Color(100, 100, 100));
        
        std::string act = (g_saveLoad.mode == SaveLoadMode::Save && g_saveLoad.selected >= 0) ? "Overwrite" : 
                          (g_saveLoad.mode == SaveLoadMode::Save ? "Save New" : "Load");
        drawMenuBtn(pPos.x + pSize.x - g_saveLoad.btnWidth - 20.f, m_menuActionText, act, sf::Color(50, 150, 50));
    }
}

void MainBoard::ProcessInput()
{
    auto openMenu = [&](SaveLoadMode m) {
        g_saveLoad.open = true; g_saveLoad.mode = m; g_saveLoad.refresh(); g_saveLoad.clampScroll();
    };

    // Helper Actions
    auto doDelete = [&]() {
        if(g_saveLoad.selected < 0 || g_saveLoad.selected >= (int)g_saveLoad.files.size()) {
            setNotification("No file selected.");
            return;
        }
        std::string fname = g_saveLoad.files[g_saveLoad.selected];
        if (tryDeleteSaveFile(fname)) {
            setNotification("Deleted: " + fname);
        } else {
            setNotification("Failed to delete " + fname);
        }
        g_saveLoad.refresh();
    };

    auto doAction = [&]() {
        if (g_saveLoad.mode == SaveLoadMode::Load) {
            if (g_saveLoad.selected >= 0 && g_saveLoad.selected < (int)g_saveLoad.files.size()) {
                if(m_game->loadNamed(g_saveLoad.files[g_saveLoad.selected])) {
                    rebuildStonesFromGame(); setNotification("Loaded: " + g_saveLoad.files[g_saveLoad.selected]);
                    g_saveLoad.open = false;
                }
            } else { setNotification("No file selected to load."); }
        } else { // Save
            if (g_saveLoad.selected >= 0 && g_saveLoad.selected < (int)g_saveLoad.files.size()) {
                m_game->saveNamed(g_saveLoad.files[g_saveLoad.selected]);
                setNotification("Overwritten: " + g_saveLoad.files[g_saveLoad.selected]);
                g_saveLoad.open = false;
            } else {
                std::string n; m_game->saveToNewSlot(n);
                setNotification("Saved New: " + n);
                g_saveLoad.open = false;
            }
        }
    };

    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>()) { m_context->m_window->close(); return; }

        // --- MENU INPUT (Overlay) ---
        if (g_saveLoad.open) 
        {
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) g_saveLoad.open = false;
                else if (key->scancode == sf::Keyboard::Scancode::Up) {
                    g_saveLoad.selected = std::max(0, g_saveLoad.selected - 1);
                    if(g_saveLoad.selected < g_saveLoad.scroll) g_saveLoad.scroll = g_saveLoad.selected;
                }
                else if (key->scancode == sf::Keyboard::Scancode::Down && !g_saveLoad.files.empty()) {
                    g_saveLoad.selected = std::min((int)g_saveLoad.files.size()-1, g_saveLoad.selected + 1);
                    if(g_saveLoad.selected >= g_saveLoad.scroll + g_saveLoad.visibleRows) g_saveLoad.scroll++;
                }
                else if (key->scancode == sf::Keyboard::Scancode::Delete) doDelete();
                else if (key->scancode == sf::Keyboard::Scancode::Enter) doAction();
            }
            // Mouse Wheel for List
            else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (!g_saveLoad.files.empty()) {
                    g_saveLoad.scroll -= (int)wheel->delta;
                    g_saveLoad.clampScroll();
                }
            }
            // Mouse Click Hit Testing
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mPos((float)mb->position.x, (float)mb->position.y);
                    auto winSize = m_context->m_window->getSize();
                    sf::Vector2f pSize = g_saveLoad.panelSize;
                    sf::Vector2f pPos(((float)winSize.x - pSize.x)/2.f, ((float)winSize.y - pSize.y)/2.f);

                    // Check if clicked outside panel
                    sf::FloatRect panelRect({pPos.x, pPos.y}, {pSize.x, pSize.y});
                    if (!panelRect.contains(mPos)) {
                        g_saveLoad.open = false;
                        continue;
                    }

                    // Check List Click
                    float listY = pPos.y + 70.f;
                    float listH = g_saveLoad.rowHeight * g_saveLoad.visibleRows;
                    sf::FloatRect listRect({pPos.x + 20.f, listY}, {pSize.x - 40.f, listH});
                    
                    if (listRect.contains(mPos)) {
                        int clickedRow = (int)((mPos.y - listY) / g_saveLoad.rowHeight);
                        int idx = g_saveLoad.scroll + clickedRow;
                        if (idx >= 0 && idx < (int)g_saveLoad.files.size()) {
                            g_saveLoad.selected = idx;
                        } else {
                            // clicked empty space in list
                            g_saveLoad.selected = -1;
                        }
                    }

                    // Check Buttons
                    float btnY = pPos.y + pSize.y - 60.f;
                    sf::FloatRect delRect({pPos.x + 20.f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});
                    sf::FloatRect canRect({pPos.x + (pSize.x - g_saveLoad.btnWidth)*0.5f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});
                    sf::FloatRect actRect({pPos.x + pSize.x - g_saveLoad.btnWidth - 20.f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});

                    if (delRect.contains(mPos)) doDelete();
                    else if (canRect.contains(mPos)) g_saveLoad.open = false;
                    else if (actRect.contains(mPos)) doAction();
                }
            }
            continue; // Swallow input
        }

        // --- GAME INPUT (Normal) ---
        if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->scancode == sf::Keyboard::Scancode::Escape) { m_context->m_states->PopCurrent(); return; }
            if (key->scancode == sf::Keyboard::Scancode::Z) { if(m_game->undo()) rebuildStonesFromGame(); }
            if (key->scancode == sf::Keyboard::Scancode::Y) { if(m_game->redo()) rebuildStonesFromGame(); }
            if (key->scancode == sf::Keyboard::Scancode::P) { 
                m_passSound.play(); if(m_game->pass()) handleGameOver(); else maybeRunAITurn(); 
            }
        }
        else if (const auto* mm = event->getIf<sf::Event::MouseMoved>()) {
            sf::Vector2f mp((float)mm->position.x, (float)mm->position.y);
            m_undoHovered = m_undoButtonBox.getGlobalBounds().contains(mp);
            m_redoHovered = m_redoButtonBox.getGlobalBounds().contains(mp);
            m_passHovered = m_passButtonBox.getGlobalBounds().contains(mp);
            m_pauseHovered = m_pauseButtonBox.getGlobalBounds().contains(mp);
            m_saveHovered = m_saveButtonBox.getGlobalBounds().contains(mp);
            m_loadHovered = m_loadButtonBox.getGlobalBounds().contains(mp);
        }
        else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mb->button == sf::Mouse::Button::Left) {
                sf::Vector2f mp((float)mb->position.x, (float)mb->position.y);
                if (m_undoButtonBox.getGlobalBounds().contains(mp)) { if(m_game->undo()) rebuildStonesFromGame(); }
                else if (m_redoButtonBox.getGlobalBounds().contains(mp)) { if(m_game->redo()) rebuildStonesFromGame(); }
                else if (m_passButtonBox.getGlobalBounds().contains(mp)) { 
                    m_passSound.play(); if(m_game->pass()) handleGameOver(); else maybeRunAITurn(); 
                }
                else if (m_pauseButtonBox.getGlobalBounds().contains(mp)) m_context->m_states->Add(std::make_unique<PauseState>(m_context, PauseState::Mode::Paused), false);
                
                else if (m_saveButtonBox.getGlobalBounds().contains(mp)) openMenu(SaveLoadMode::Save);
                else if (m_loadButtonBox.getGlobalBounds().contains(mp)) openMenu(SaveLoadMode::Load);
                
                else handleLeftClick({mb->position.x, mb->position.y});
            }
        }
    }
}

void MainBoard::resetGame()
{
    m_game = std::make_unique<Game>(new Board());
    m_stones.clear();
    rebuildStonesFromGame();
    maybeRunAITurn();
}