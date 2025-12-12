
#include "MainBoard.hpp"

#include <SFML/Window/Event.hpp>

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

        // simple double-click support
        sf::Clock clickClock;
        int lastClickedRow = -1;

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

    // Best-effort deletion without depending on Game::SAVE_DIR visibility.
    bool tryDeleteSaveFile(const std::string& filename)
    {
        std::error_code ec;

        // 1) try in "saves/" (matches Game::saveNamed/listSaveFiles in your pasted game.cpp)
        if (fs::remove(fs::path("saves") / filename, ec)) return true;

        // 2) try direct name
        ec.clear();
        if (fs::remove(fs::path(filename), ec)) return true;

        // 3) try add ".go" in both places
        const std::string withExt = (filename.size() >= 3 && filename.compare(filename.size()-3, 3, ".go")==0) ? filename : (filename + ".go");

        ec.clear();
        if (fs::remove(fs::path("saves") / withExt, ec)) return true;

        ec.clear();
        if (fs::remove(fs::path(withExt), ec)) return true;

        return false;
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
      m_undoButtonBox(),
      m_redoButtonBox(),
      m_passButtonBox(),
      m_pauseButtonBox(),
      m_saveButtonBox(),
      m_loadButtonBox(),
      m_undoHovered(false),
      m_redoHovered(false),
      m_passHovered(false),
      m_pauseHovered(false),
      m_saveHovered(false),
      m_loadHovered(false),
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

    m_boardBackground.setSize({m_boardPixelSize, m_boardPixelSize});
    m_boardBackground.setFillColor({210, 164, 80});
    m_boardBackground.setOutlineThickness(2.f);
    m_boardBackground.setOutlineColor(sf::Color::Black);

    m_boardTopLeft.x = (winSizeF.x - m_boardPixelSize) * 0.5f;
    m_boardTopLeft.y = (winSizeF.y - m_boardPixelSize) * 0.5f;
    m_boardBackground.setPosition(m_boardTopLeft);

    m_hasClassicTexture = m_boardTextureClassic.loadFromFile("assets/texture/light-wood.jpg");
    if (!m_hasClassicTexture)
    {
        std::cout << "[MainBoard] Warning: could not load assets/texture/light-wood.jpg\n";
    }

    m_hasDarkTexture = m_boardTextureDark.loadFromFile("assets/texture/dark-stone.jpg");
    if (!m_hasDarkTexture)
    {
        std::cout << "[MainBoard] Warning: could not load assets/texture/dark-stone.jpg\n";
    }

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

    m_saveButtonBox.setSize({100.f, 40.f});
    m_saveButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_saveButtonBox.setPosition({winSizeF.x - 150.f, 240.f});

    m_loadButtonBox.setSize({100.f, 40.f});
    m_loadButtonBox.setFillColor(sf::Color(200, 200, 200));
    m_loadButtonBox.setPosition({winSizeF.x - 150.f, 290.f});

    if (!m_game)
        m_game = std::make_unique<Game>(new Board());

    rebuildStonesFromGame();

    // If AI is Black (human chose White), let AI play first.
    maybeRunAITurn();

    std::cout << "[MainBoard] Init END\n";
}

bool MainBoard::isAIMode() const
{
    return m_context && (m_context->m_gameMode == GameMode::AiVsPlayer);
}

PieceColor MainBoard::humanColor() const
{
    if (!m_context)
        return BLACK;
    return (m_context->m_humanPlaysBlack ? BLACK : WHITE);
}

PieceColor MainBoard::aiColor() const
{
    return (humanColor() == BLACK) ? WHITE : BLACK;
}

void MainBoard::handleGameOver()
{
    if (!m_game)
        return;

    auto [blackScore, whiteScore] = m_game->calculateFinalScore();
    std::string msg;

    if (blackScore > whiteScore)
    {
        float diff = blackScore - whiteScore;
        msg = "Black wins by " + std::to_string(diff) +
              " points.\nBlack: " + std::to_string(blackScore) +
              " | White: " + std::to_string(whiteScore);
    }
    else if (whiteScore > blackScore)
    {
        float diff = whiteScore - blackScore;
        msg = "White wins by " + std::to_string(diff) +
              " points.\nBlack: " + std::to_string(blackScore) +
              " | White: " + std::to_string(whiteScore);
    }
    else
    {
        msg = "It's a draw!\nBoth players: " + std::to_string(blackScore);
    }

    setNotification(msg);
    m_winSound.play();

    m_context->m_states->Add(
        std::make_unique<PauseState>(m_context, PauseState::Mode::GameOver, msg),
        false);
}

void MainBoard::maybeRunAITurn()
{
    if (!isAIMode() || !m_game)
        return;

    if (m_game->getTurn() != aiColor())
        return;

    AIDifficulty diff = m_context->m_aiDifficulty;

    AIMove mv = GoAI::computeAIMove(*m_game, diff);

    if (mv.isPass)
    {
        bool finished = m_game->pass();
        m_passSound.play();
        rebuildStonesFromGame();
        setNotification("AI passed.");
        if (finished)
            handleGameOver();
        return;
    }

    bool placed = m_game->placeStone(mv.x, mv.y);
    if (!placed)
    {
        bool found = false;
        for (int y = 0; y < m_boardSize && !found; ++y)
        {
            for (int x = 0; x < m_boardSize && !found; ++x)
            {
                if (m_game->placeStone(x, y))
                {
                    mv.x = x;
                    mv.y = y;
                    mv.isPass = false;
                    found = true;
                }
            }
        }

        if (!found)
        {
            bool finished = m_game->pass();
            m_passSound.play();
            rebuildStonesFromGame();
            setNotification("AI passed.");
            if (finished)
                handleGameOver();
            return;
        }
    }

    m_placeSound.play();
    rebuildStonesFromGame();

    int caps = m_game->getLastCaptures();
    std::string msg = "AI played at (" + std::to_string(mv.x) + ", " + std::to_string(mv.y) + ").";

    if (caps > 0)
        msg += " Captured " + std::to_string(caps) + (caps == 1 ? " stone." : " stones.");

    if (m_game->lastMoveCreatedKoThreat())
        msg += " Ko threat: immediate recapture is forbidden.";

    setNotification(msg);
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

    // In AI mode, block clicks when it's not the human's turn.
    if (isAIMode() && m_game->getTurn() != humanColor())
    {
        setNotification("Please wait for AI to move.");
        return;
    }

    bool ok = m_game->placeStone(ix, iy);
    if (ok)
    {
        m_placeSound.play();
        rebuildStonesFromGame();

        int caps = m_game->getLastCaptures();

        PieceColor now = m_game->getTurn();
        PieceColor played = (now == BLACK ? WHITE : (now == WHITE ? BLACK : NONE));

        std::string playerName = (played == BLACK ? "Black" : played == WHITE ? "White" : "Player");
        std::string msg;

        if (caps > 0)
        {
            msg = playerName + " captured " + std::to_string(caps) + (caps == 1 ? " stone." : " stones.");
        }

        if (m_game->lastMoveCreatedKoThreat())
        {
            if (!msg.empty())
                msg += " ";
            msg += "Ko threat: immediate recapture is forbidden.";
        }

        if (!msg.empty())
            setNotification(msg);

        // In AI mode, let AI respond after the human moves.
        maybeRunAITurn();
    }
    else
    {
        m_invalidSound.play();
        std::string msg;
        if (m_game->lastMoveWasKoViolation())
            msg = "Invalid move: Ko rule violation. You cannot immediately retake.";
        else if (m_game->lastMoveWasSuicide())
            msg = "Invalid move: Suicide moves are not allowed.";
        else if (m_game->lastMoveWasInvalid())
            msg = "Invalid move: position out of bounds or already occupied.";
        else
            msg = "Invalid move.";

        setNotification(msg);
        std::cout << "[MainBoard] Invalid move at (" << ix << ", " << iy << ")\n";
    }
}

void MainBoard::ProcessInput()
{
    auto openSaveMenu = [&]() {
        g_saveLoad.open = true;
        g_saveLoad.mode = SaveLoadMode::Save;
        g_saveLoad.selected = -1;
        g_saveLoad.scroll = 0;
        g_saveLoad.refresh();
        g_saveLoad.clampScroll();
    };

    auto openLoadMenu = [&]() {
        g_saveLoad.open = true;
        g_saveLoad.mode = SaveLoadMode::Load;
        g_saveLoad.selected = -1;
        g_saveLoad.scroll = 0;
        g_saveLoad.refresh();
        g_saveLoad.clampScroll();
    };

    auto doLoadSelected = [&]() -> bool {
        if (!m_game) return false;
        if (g_saveLoad.selected < 0 || g_saveLoad.selected >= (int)g_saveLoad.files.size()) return false;

        const std::string& name = g_saveLoad.files[g_saveLoad.selected];
        if (m_game->loadNamed(name)) {
            rebuildStonesFromGame();
            setNotification("Loaded: " + name);
            maybeRunAITurn();
            return true;
        }
        setNotification("Failed to load: " + name);
        return false;
    };

    auto doSaveNew = [&]() -> bool {
        if (!m_game) return false;

        std::string outFile;
        if (m_game->saveToNewSlot(outFile)) {
            setNotification("Saved: " + outFile);
            g_saveLoad.refresh();
            auto it = std::find(g_saveLoad.files.begin(), g_saveLoad.files.end(), outFile);
            if (it != g_saveLoad.files.end())
                g_saveLoad.selected = (int)std::distance(g_saveLoad.files.begin(), it);
            g_saveLoad.clampScroll();
            return true;
        }
        setNotification("Failed to save.");
        return false;
    };

    auto doSaveOverwrite = [&]() -> bool {
        if (!m_game) return false;
        if (g_saveLoad.selected < 0 || g_saveLoad.selected >= (int)g_saveLoad.files.size()) return false;

        const std::string& name = g_saveLoad.files[g_saveLoad.selected];
        if (m_game->saveNamed(name)) {
            setNotification("Saved (overwrite): " + name);
            g_saveLoad.refresh();
            g_saveLoad.clampScroll();
            return true;
        }
        setNotification("Failed to save: " + name);
        return false;
    };

    auto doDeleteSelected = [&]() -> bool {
        if (g_saveLoad.selected < 0 || g_saveLoad.selected >= (int)g_saveLoad.files.size()) return false;

        const std::string name = g_saveLoad.files[g_saveLoad.selected];
        if (!tryDeleteSaveFile(name)) {
            setNotification("Failed to delete: " + name);
            return false;
        }

        setNotification("Deleted: " + name);
        g_saveLoad.refresh();

        if (g_saveLoad.files.empty()) {
            g_saveLoad.selected = -1;
            g_saveLoad.scroll = 0;
        } else {
            g_saveLoad.selected = std::min(g_saveLoad.selected, (int)g_saveLoad.files.size() - 1);
        }
        g_saveLoad.clampScroll();
        return true;
    };

    auto panelRect = [&]() -> sf::FloatRect {
        auto winSize = m_context->m_window->getSize();
        const sf::Vector2f winF((float)winSize.x, (float)winSize.y);
        const sf::Vector2f panelSize(520.f, 440.f);
        const sf::Vector2f panelPos((winF.x - panelSize.x) * 0.5f, (winF.y - panelSize.y) * 0.5f);
        return sf::FloatRect(panelPos, panelSize);
    };

    auto listRect = [&]() -> sf::FloatRect {
        const auto p = panelRect();
        const float listX = p.position.x + 20.f;
        const float listY = p.position.y + 70.f;
        const float listW = p.size.x - 40.f;
        const float listH = g_saveLoad.rowHeight * (float)g_saveLoad.visibleRows;
        return sf::FloatRect({listX, listY}, {listW, listH});
    };

    auto buttonRects = [&]() -> std::array<sf::FloatRect, 3> {
        const auto p = panelRect();
        const float btnY = p.position.y + p.size.y - 60.f;
        const float btnH = 36.f;
        const float btnW = 150.f;

        sf::FloatRect left ({p.position.x + 20.f,                 btnY}, {btnW, btnH}); // Delete
        sf::FloatRect mid  ({p.position.x + (p.size.x - btnW)*0.5f, btnY}, {btnW, btnH}); // Overwrite/Cancel
        sf::FloatRect right({p.position.x + p.size.x - btnW - 20.f, btnY}, {btnW, btnH}); // SaveNew/Load
        return {left, mid, right};
    };

    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>()) {
            m_context->m_window->close();
            return;
        }

        // =========================
        // SAVE/LOAD MENU (modal)
        // =========================
        if (g_saveLoad.open)
        {
            if (const auto* key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    g_saveLoad.open = false;
                    continue;
                }

                // Switch tab while open
                if (key->control && key->scancode == sf::Keyboard::Scancode::S) {
                    g_saveLoad.mode = SaveLoadMode::Save;
                    g_saveLoad.refresh();
                    g_saveLoad.clampScroll();
                    continue;
                }
                if (key->control && key->scancode == sf::Keyboard::Scancode::L) {
                    g_saveLoad.mode = SaveLoadMode::Load;
                    g_saveLoad.refresh();
                    g_saveLoad.clampScroll();
                    continue;
                }

                if (key->scancode == sf::Keyboard::Scancode::Up) {
                    if (!g_saveLoad.files.empty()) {
                        if (g_saveLoad.selected < 0) g_saveLoad.selected = 0;
                        g_saveLoad.selected = std::max(0, g_saveLoad.selected - 1);
                        if (g_saveLoad.selected < g_saveLoad.scroll)
                            g_saveLoad.scroll = g_saveLoad.selected;
                        g_saveLoad.clampScroll();
                    }
                    continue;
                }

                if (key->scancode == sf::Keyboard::Scancode::Down) {
                    if (!g_saveLoad.files.empty()) {
                        if (g_saveLoad.selected < 0) g_saveLoad.selected = 0;
                        g_saveLoad.selected = std::min((int)g_saveLoad.files.size() - 1, g_saveLoad.selected + 1);
                        if (g_saveLoad.selected >= g_saveLoad.scroll + g_saveLoad.visibleRows)
                            g_saveLoad.scroll = g_saveLoad.selected - g_saveLoad.visibleRows + 1;
                        g_saveLoad.clampScroll();
                    }
                    continue;
                }

                if (key->scancode == sf::Keyboard::Scancode::Delete) {
                    doDeleteSelected();
                    continue;
                }

                if (key->scancode == sf::Keyboard::Scancode::Enter ||
                    key->scancode == sf::Keyboard::Scancode::NumpadEnter)
                {
                    if (g_saveLoad.mode == SaveLoadMode::Load) {
                        if (doLoadSelected()) g_saveLoad.open = false;
                    } else {
                        // Save menu: Enter = Save New, Ctrl+Enter = Overwrite selected
                        if (key->control) doSaveOverwrite();
                        else doSaveNew();
                    }
                    continue;
                }
            }
            else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (!g_saveLoad.files.empty()) {
                    const int delta = (wheel->delta > 0.f) ? -1 : 1;
                    g_saveLoad.scroll += delta;
                    g_saveLoad.clampScroll();
                }
                continue;
            }
            else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                {
                    const sf::Vector2f mouse((float)mb->position.x, (float)mb->position.y);
                    const auto pRect = panelRect();

                    // Click outside => close
                    if (!pRect.contains(mouse)) {
                        g_saveLoad.open = false;
                        continue;
                    }

                    // Click list => select / double-click load
                    const auto lRect = listRect();
                    if (lRect.contains(mouse))
                    {
                        const float localY = mouse.y - lRect.position.y;
                        const int row = (int)(localY / g_saveLoad.rowHeight);
                        const int idx = g_saveLoad.scroll + row;

                        if (idx >= 0 && idx < (int)g_saveLoad.files.size())
                        {
                            const bool sameRow = (idx == g_saveLoad.lastClickedRow);
                            const bool fast = (g_saveLoad.clickClock.getElapsedTime().asMilliseconds() < 350);
                            g_saveLoad.lastClickedRow = idx;
                            g_saveLoad.clickClock.restart();

                            g_saveLoad.selected = idx;

                            if (g_saveLoad.mode == SaveLoadMode::Load && sameRow && fast) {
                                if (doLoadSelected()) g_saveLoad.open = false;
                            }
                        }
                        continue;
                    }

                    // Click buttons
                    const auto btns = buttonRects();
                    if (btns[0].contains(mouse)) { // Delete
                        doDeleteSelected();
                        continue;
                    }
                    if (btns[1].contains(mouse)) { // Overwrite/Cancel
                        if (g_saveLoad.mode == SaveLoadMode::Load) {
                            g_saveLoad.open = false; // Cancel
                        } else {
                            doSaveOverwrite();
                        }
                        continue;
                    }
                    if (btns[2].contains(mouse)) { // Save New / Load
                        if (g_saveLoad.mode == SaveLoadMode::Load) {
                            if (doLoadSelected()) g_saveLoad.open = false;
                        } else {
                            doSaveNew();
                        }
                        continue;
                    }
                }
                continue;
            }

            // Menu open => swallow everything else
            continue;
        }

        // =========================
        // NORMAL GAME INPUT
        // =========================
        if (const auto* key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->scancode == sf::Keyboard::Scancode::Escape) {
                m_context->m_states->PopCurrent();
                return;
            }

            // Save/Load hotkeys (linked to the same UI as the buttons)
            if (key->control && key->scancode == sf::Keyboard::Scancode::S) { openSaveMenu(); continue; }
            if (key->control && key->scancode == sf::Keyboard::Scancode::L) { openLoadMenu(); continue; }
            if (key->scancode == sf::Keyboard::Scancode::S) { openSaveMenu(); continue; }
            if (key->scancode == sf::Keyboard::Scancode::L) { openLoadMenu(); continue; }

            if (key->scancode == sf::Keyboard::Scancode::Z) {
                if (m_game && m_game->undo()) {
                    rebuildStonesFromGame();
                    maybeRunAITurn();
                }
                continue;
            }

            if (key->scancode == sf::Keyboard::Scancode::Y) {
                if (m_game && m_game->redo()) {
                    rebuildStonesFromGame();
                    maybeRunAITurn();
                }
                continue;
            }

            if (key->scancode == sf::Keyboard::Scancode::P) {
                if (m_game) {
                    if (isAIMode() && m_game->getTurn() != humanColor()) {
                        setNotification("Please wait for AI to move.");
                        continue;
                    }

                    m_passSound.play();
                    bool finished = m_game->pass();
                    rebuildStonesFromGame();

                    if (finished) handleGameOver();
                    else maybeRunAITurn();
                }
                continue;
            }
        }
        else if (const auto* mm = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mousePos((float)mm->position.x, (float)mm->position.y);
            m_undoHovered  = m_undoButtonBox.getGlobalBounds().contains(mousePos);
            m_redoHovered  = m_redoButtonBox.getGlobalBounds().contains(mousePos);
            m_passHovered  = m_passButtonBox.getGlobalBounds().contains(mousePos);
            m_pauseHovered = m_pauseButtonBox.getGlobalBounds().contains(mousePos);
            m_saveHovered  = m_saveButtonBox.getGlobalBounds().contains(mousePos);
            m_loadHovered  = m_loadButtonBox.getGlobalBounds().contains(mousePos);
        }
        else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mb->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mousePosF((float)mb->position.x, (float)mb->position.y);
                sf::Vector2i mousePos(mb->position.x, mb->position.y);

                if (m_undoButtonBox.getGlobalBounds().contains(mousePosF)) {
                    if (m_game && m_game->undo()) {
                        rebuildStonesFromGame();
                        maybeRunAITurn();
                    }
                    continue;
                }

                if (m_redoButtonBox.getGlobalBounds().contains(mousePosF)) {
                    if (m_game && m_game->redo()) {
                        rebuildStonesFromGame();
                        maybeRunAITurn();
                    }
                    continue;
                }

                if (m_passButtonBox.getGlobalBounds().contains(mousePosF)) {
                    if (m_game) {
                        if (isAIMode() && m_game->getTurn() != humanColor()) {
                            setNotification("Please wait for AI to move.");
                            continue;
                        }

                        m_passSound.play();
                        bool finished = m_game->pass();
                        rebuildStonesFromGame();

                        if (finished) handleGameOver();
                        else maybeRunAITurn();
                    }
                    continue;
                }

                if (m_pauseButtonBox.getGlobalBounds().contains(mousePosF)) {
                    m_context->m_states->Add(
                        std::make_unique<PauseState>(m_context, PauseState::Mode::Paused),
                        false);
                    continue;
                }

                // Save/Load buttons -> open menus (multi-file)
                if (m_saveButtonBox.getGlobalBounds().contains(mousePosF)) {
                    openSaveMenu();
                    continue;
                }

                if (m_loadButtonBox.getGlobalBounds().contains(mousePosF)) {
                    openLoadMenu();
                    continue;
                }

                handleLeftClick(mousePos);
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

