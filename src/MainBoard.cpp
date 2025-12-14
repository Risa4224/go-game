#include "MainBoard.hpp"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Window/Event.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <type_traits>
#include <string>
#include <vector>

#include "AI.h"
#include "EndGameState.hpp"
namespace fs = std::filesystem;

namespace
{
    static std::mutex g_gameStateMutex;

    // ---- Board visual tuning (match reference style) ----
    constexpr float kGridMargin = 28.f;          // padding inside the board for coords
    constexpr float kOuterBorderThickness = 2.f; // bold outer border around grid
    constexpr float kCoordFontSize = 16.f;

    // ---- Right-side panel + history layout constants ----
    // Wider panel so buttons + move list feel less cramped.
    constexpr float kSidePanelW = 210.f;
    constexpr float kSidePanelMargin = 8.f;
    constexpr float kSidePanelGap = 18.f;

    constexpr float kPanelTop = 24.f;
    // Increased to fit two extra buttons: "Show Moves" + "Hint".
    constexpr float kSidePanelH = 540.f; // MUST match Draw() side panel height

    // Button layout inside right panel
    constexpr float kSidePanelPad = 12.f;
    constexpr float kBtnH = 38.f;
    constexpr float kBtnGap = 10.f;

    constexpr float kBottomBarH = 30.f;
    constexpr float kHistoryMargin = 10.f;
    constexpr float kHistoryMinH = 140.f;

    // Go coordinates skip "I"
    std::string makeGoLetters(int n)
    {
        // Standard: A B C D E F G H J K L M N O P Q R S T ... (skip I)
        const std::string alphabet = "ABCDEFGHJKLMNOPQRSTUVWXYZ";
        if (n <= 0)
            return "";
        if ((int)alphabet.size() >= n)
            return alphabet.substr(0, n);

        // fallback for huge sizes
        std::string out;
        out.reserve(n);
        for (int i = 0; i < n; ++i)
        {
            char c = (i < (int)alphabet.size()) ? alphabet[i] : char('A' + (i % 26));
            if (c == 'I')
                c = 'J';
            out.push_back(c);
        }
        return out;
    }

    // (A,19) style where y=0 is top row -> 19
    std::string toGoCoord(int x, int y, int boardSize)
    {
        const std::string letters = makeGoLetters(boardSize);
        char col = (x >= 0 && x < boardSize && x < (int)letters.size()) ? letters[x] : '?';
        int row = boardSize - y;
        return "(" + std::string(1, col) + "," + std::to_string(row) + ")";
    }

    // Hoshi points index sets
    std::vector<int> hoshiIdx(int n)
    {
        if (n == 19)
            return {3, 9, 15};
        if (n == 13)
            return {3, 6, 9};
        if (n == 9)
            return {2, 4, 6};
        return {};
    }

    // AI move apply delay depends on difficulty (Easy: 0s, Medium: 1.2s, Hard: 3.2s)
    template <typename T>
    int toIntDifficulty(T v)
    {
        if constexpr (std::is_enum_v<T>)
            return static_cast<int>(v);
        else
            return static_cast<int>(v);
    }

    template <typename T>
    float aiDelaySecondsForDifficulty(T diff)
    {
        const int d = toIntDifficulty(diff);
        if (d <= 0)
            return 0.f;   // Easy
        if (d == 1)
            return 1.2f;  // Medium
        return 3.2f;      // Hard (and any higher)
    }


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

        int scroll = 0; // in rows
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

            if (selected < 0)
                selected = 0;
            if (selected >= static_cast<int>(files.size()))
                selected = static_cast<int>(files.size()) - 1;

            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(files.size()) - visibleRows));
        }

        void clampScroll()
        {
            scroll = std::clamp(scroll, 0, std::max(0, static_cast<int>(files.size()) - visibleRows));
        }
    };

    SaveLoadMenuState g_saveLoad;

    // End Game button on the right toolbar (kept in .cpp only)
    static sf::RectangleShape g_endGameButtonBox;
    static bool g_endGameHovered = false;

    // --- Show legal moves + Hint (kept in .cpp only) ---
    static sf::RectangleShape g_showMovesButtonBox;
    static sf::RectangleShape g_hintButtonBox;
    static bool g_showMovesHovered = false;
    static bool g_hintHovered = false;

    static bool g_showLegalMoves = false;
    static bool g_legalMovesDirty = true;
    static std::vector<sf::CircleShape> g_legalMoveMarkers;

    // Hint: computed once on demand and highlighted.
    static std::optional<AIMove> g_hintMove;
    static sf::Clock g_hintClock;
    static constexpr float kHintShowSeconds = 8.f;

    // Hint spam cooldown (also reduces chance of noticeable stutter)
    static sf::Clock g_hintCooldown;
    static constexpr float kHintCooldownSeconds = 0.5f;

    // helper: whenever board state changes, refresh markers and clear hint
    static void invalidateMoveVisuals()
    {
        g_legalMovesDirty = true;
        g_hintMove.reset();
    }

    // ---- Legal move check for highlighting ----
    // Your core API exposes legality through Game::placeStone(...).
    // For highlighting, we *must not* mutate the real game state, so we
    // clone the game and test the move on the clone.
    static bool isLegalCandidate(const Game *game, int x, int y)
    {
        if (!game)
            return false;

        const Board *b = game->getBoard();
        if (!b)
            return false;

        if (b->getPiece(x, y) != NONE)
            return false;

        // Accurate (handles suicide + ko + capture rules) without touching the real game.
        Game snapshot(*game);
        return snapshot.placeStone(x, y);
    }

    // Move history scrolling (0 = follow latest; >0 = scrolled up)
    static int g_histScroll = 0;

    static sf::FloatRect computeHistoryRect(sf::Vector2u winSize)
    {
        const float sidePanelX = (float)winSize.x - kSidePanelW - kSidePanelMargin;

        const float x = sidePanelX;
        const float y = kPanelTop + kSidePanelH + kHistoryMargin;

        const float w = kSidePanelW;
        // Use remaining vertical space under the toolbar (so it scales with window size).
        const float bottom = (float)winSize.y - kBottomBarH - 10.f;
        float h = bottom - y-20.f;
        h = std::max(kHistoryMinH, h);

        return sf::FloatRect({x, y}, {w, h});
    }

    // Robust save dir finder: tries a few likely places, then creates ./saved_game if missing.
    static fs::path findSaveDir()
    {
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);

        std::vector<fs::path> candidates = {
            cwd / "saved_game",
            cwd / "go-game" / "saved_game",
            cwd.parent_path() / "saved_game",
            cwd.parent_path() / "go-game" / "saved_game",
        };

        for (const auto &p : candidates)
        {
            if (fs::exists(p, ec) && fs::is_directory(p, ec))
                return p;
        }

        // create the most reasonable default: ./saved_game
        fs::path dir = cwd / "saved_game";
        fs::create_directories(dir, ec);
        std::cout << "[SaveDir] created: " << dir << " | err=" << ec.message() << "\n";
        return dir;
    }

    // Sidecar history file per save slot
    static fs::path historyPathForSave(const std::string &saveName)
    {
        const fs::path saveDir = findSaveDir();

        fs::path p = fs::path(saveName).filename();
        if (!p.has_extension())
            p.replace_extension(".go");

        fs::path hist = saveDir / p;
        hist.replace_extension(".hist");
        return hist;
    }

    static void writeHistoryFile(const std::string &saveName, const std::vector<std::string> &lines)
    {
        std::error_code ec;
        fs::create_directories(findSaveDir(), ec);

        std::ofstream out(historyPathForSave(saveName));
        if (!out)
            return;

        for (const auto &s : lines)
            out << s << "\n";
    }

    static bool readHistoryFile(const std::string &saveName, std::vector<std::string> &outLines)
    {
        outLines.clear();
        std::ifstream in(historyPathForSave(saveName));
        if (!in)
            return false;

        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty())
                outLines.push_back(line);
        }
        return true;
    }

    // Best-effort deletion (looks in saved_game)
    static bool tryDeleteSaveFile(const std::string &filename)
    {
        std::error_code ec;
        const fs::path saveDir = findSaveDir();

        fs::path in = fs::path(filename);
        fs::path inName = in.filename(); // strip any folder user might have included

        fs::path goName = inName;
        if (!goName.has_extension())
            goName.replace_extension(".go");

        fs::path txtName = goName;
        txtName.replace_extension(".txt");

        fs::path histName = goName;
        histName.replace_extension(".hist");

        std::vector<fs::path> toTry = {
            saveDir / inName,
            saveDir / goName,
            saveDir / txtName,
            saveDir / histName,

            // sometimes they are saved in CWD by mistake
            inName,
            goName,
            txtName,
            histName};

        std::sort(toTry.begin(), toTry.end());
        toTry.erase(std::unique(toTry.begin(), toTry.end()), toTry.end());

        std::cout << "[Delete Debug] Request to delete: " << filename << "\n";
        std::cout << "[Delete Debug] CWD: " << fs::current_path() << "\n";
        std::cout << "[Delete Debug] SaveDir chosen: " << saveDir << "\n";

        bool anyDeleted = false;

        for (const auto &p : toTry)
        {
            bool ex = fs::exists(p, ec);
            std::cout << "  Try: " << p << " | exists=" << (ex ? "yes" : "no") << "\n";
            if (ex)
            {
                if (fs::remove(p, ec))
                {
                    std::cout << "  -> DELETE OK: " << p << "\n";
                    anyDeleted = true;
                }
                else
                {
                    std::cout << "  -> DELETE FAIL: " << p << " | err=" << ec.message() << "\n";
                }
            }
        }

        if (!anyDeleted)
        {
            std::cout << "[Delete Debug] Could not delete. Listing files in: " << saveDir << "\n";
            if (fs::exists(saveDir, ec) && fs::is_directory(saveDir, ec))
            {
                for (const auto &e : fs::directory_iterator(saveDir, ec))
                {
                    if (!ec && e.is_regular_file())
                        std::cout << "   - " << e.path().filename().string() << "\n";
                }
            }
            else
            {
                std::cout << "[Delete Debug] SaveDir does not exist.\n";
            }
        }

        return anyDeleted;
    }

    static void clampHistoryScroll(int historySize, int maxLines)
    {
        const int maxScroll = std::max(0, historySize - maxLines);
        g_histScroll = std::clamp(g_histScroll, 0, maxScroll);
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
      m_undoButtonBox(), m_redoButtonBox(), m_passButtonBox(), m_restartButtonBox(), m_saveButtonBox(), m_loadButtonBox(),
      m_undoHovered(false), m_redoHovered(false), m_passHovered(false), m_restartHovered(false), m_saveHovered(false), m_loadHovered(false),
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
    cancelAIWorker();

    m_gameOver = false;

    std::cout << "[MainBoard] Init START\n";

    auto winSize = m_context->m_window->getSize();
    sf::Vector2f winSizeF(static_cast<float>(winSize.x),
                          static_cast<float>(winSize.y));

    // --- 1. Load Font & Setup Text ---
    auto &font = m_context->m_assets->GetFont(MAIN_FONT);

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

    // Reserve space for right side panel
    const float sidePanelX = winSizeF.x - kSidePanelW - kSidePanelMargin;
    const float boardAreaW = sidePanelX - kSidePanelGap;

    m_boardTopLeft.x = std::max(20.f, (boardAreaW - m_boardPixelSize) * 0.5f);
    m_boardTopLeft.y = (winSizeF.y - m_boardPixelSize) * 0.5f;
    m_boardBackground.setPosition(m_boardTopLeft);

    m_hasClassicTexture = m_boardTextureClassic.loadFromFile("assets/texture/light-wood.jpg");
    m_hasDarkTexture = m_boardTextureDark.loadFromFile("assets/texture/dark-stone.jpg");

    // grid is inset so coordinates fit inside board
    m_cellSize = (m_boardPixelSize - 2.f * kGridMargin) / static_cast<float>(m_boardSize - 1);

    buildGrid();

    // --- 3. UI Buttons Positioning ---
    // Right toolbar layout: full-width buttons, consistent spacing.
    float y = kPanelTop + 58.f; // leave space for a small "Controls" header in Draw()
    const float btnW = kSidePanelW - 2.f * kSidePanelPad;
    const float btnX = sidePanelX + kSidePanelPad;

    auto setupBtn = [&](sf::RectangleShape& box)
    {
        box.setSize({btnW, kBtnH});
        box.setPosition({btnX, y});
        box.setFillColor(sf::Color(200, 200, 200));
        box.setOutlineThickness(1.f);
        box.setOutlineColor(sf::Color(120, 120, 120));
        y += kBtnH + kBtnGap;
    };

    setupBtn(m_undoButtonBox);
    setupBtn(m_redoButtonBox);
    setupBtn(m_passButtonBox);
    setupBtn(m_restartButtonBox);
    setupBtn(m_saveButtonBox);
    setupBtn(m_loadButtonBox);

    // End Game button on toolbar
    setupBtn(g_endGameButtonBox);

    // Show legal moves + Hint
    setupBtn(g_showMovesButtonBox);
    setupBtn(g_hintButtonBox);

    if (!m_game)
        m_game = std::make_unique<Game>(new Board());

    loadStoneTexturesFromFiles();

    if (m_context)
        m_lastStoneTheme = m_context->m_stoneTheme;

    // IMPORTANT: clear history BEFORE AI move (so AI-first doesn't get wiped)
    m_moveHistory.clear();
    m_moveRedo.clear();
    g_histScroll = 0;

    invalidateMoveVisuals();

    rebuildStonesFromGame();
    maybeRunAITurn();

    std::cout << "[MainBoard] Init END\n";
}

void MainBoard::loadStoneTexturesFromFiles()
{
    auto tryLoad = [](sf::Texture &tex, bool &flag, const fs::path &path)
    {
        flag = tex.loadFromFile(path);
        if (flag)
            tex.setSmooth(true);
        else
            std::cout << "[MainBoard] Stone texture missing or failed to load: " << path.string() << "\n";
    };

    tryLoad(m_stoneTexClassicBlack, m_hasStoneTexClassicBlack, fs::path("assets/stones/classic_black.png"));
    tryLoad(m_stoneTexClassicWhite, m_hasStoneTexClassicWhite, fs::path("assets/stones/classic_white.png"));
    tryLoad(m_stoneTexSlateShellBlack, m_hasStoneTexSlateShellBlack, fs::path("assets/stones/slate_shell_black.png"));
    tryLoad(m_stoneTexSlateShellWhite, m_hasStoneTexSlateShellWhite, fs::path("assets/stones/slate_shell_white.png"));
    tryLoad(m_stoneTexGlassBlack, m_hasStoneTexGlassBlack, fs::path("assets/stones/glass_black.png"));
    tryLoad(m_stoneTexGlassWhite, m_hasStoneTexGlassWhite, fs::path("assets/stones/glass_white.png"));
}

const sf::Texture *MainBoard::getStoneTexture(StoneTheme theme, PieceColor c) const
{
    const bool wantBlack = (c == BLACK);

    switch (theme)
    {
    case StoneTheme::SlateShell:
        if (wantBlack)
            return m_hasStoneTexSlateShellBlack ? &m_stoneTexSlateShellBlack : nullptr;
        return m_hasStoneTexSlateShellWhite ? &m_stoneTexSlateShellWhite : nullptr;

    case StoneTheme::Glass:
        if (wantBlack)
            return m_hasStoneTexGlassBlack ? &m_stoneTexGlassBlack : nullptr;
        return m_hasStoneTexGlassWhite ? &m_stoneTexGlassWhite : nullptr;

    case StoneTheme::Classic:
    default:
        if (wantBlack)
            return m_hasStoneTexClassicBlack ? &m_stoneTexClassicBlack : nullptr;
        return m_hasStoneTexClassicWhite ? &m_stoneTexClassicWhite : nullptr;
    }
}

void MainBoard::applyStoneVisual(sf::CircleShape &stone, PieceColor c) const
{
    const StoneTheme theme = m_context ? m_context->m_stoneTheme : StoneTheme::Classic;

    if (const sf::Texture *tex = getStoneTexture(theme, c))
    {
        stone.setFillColor(sf::Color::White);
        stone.setTexture(tex, true);
        stone.setOutlineThickness(0.f);
        stone.setOutlineColor(sf::Color::Transparent);
        return;
    }

    stone.setTexture(nullptr, true);

    switch (theme)
    {
    case StoneTheme::SlateShell:
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
        break;

    case StoneTheme::Glass:
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
        break;

    case StoneTheme::Classic:
    default:
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
        break;
    }
}

bool MainBoard::isAIMode() const { return m_context && (m_context->m_gameMode == GameMode::AiVsPlayer); }
PieceColor MainBoard::humanColor() const { return (!m_context || m_context->m_humanPlaysBlack) ? BLACK : WHITE; }
PieceColor MainBoard::aiColor() const { return (humanColor() == BLACK) ? WHITE : BLACK; }

void MainBoard::handleGameOver()
{
    if (!m_game)
        return;

    // Prevent stacking multiple end-game modals.
    if (m_gameOver)
        return;

    // Stop any pending AI work so the UI doesn't apply extra moves after game end.
    cancelAIWorker();

    auto [b, w] = m_game->calculateFinalScore();
    std::string winnerLine = (b > w) ? "Black wins!" : (w > b) ? "White wins!" : "Draw!";
    std::string msg = winnerLine + " B:" + std::to_string(b) + " W:" + std::to_string(w);

    m_gameOver = true;
    setNotification(msg);
    m_winSound.play();

    // Show modal state with winner + final score.
    if (m_context && m_context->m_states)
        m_context->m_states->Add(std::make_unique<EndGameState>(m_context, msg), false);
}


MainBoard::~MainBoard()
{
    cancelAIWorker();
}

bool MainBoard::aiBusy() const
{
    if (!isAIMode() || !m_game)
        return false;
    // If it's AI's turn or we have a pending/result AI computation, we consider AI "busy".
    return (m_game->getTurn() == aiColor()) || m_aiThinking.load() || m_aiResultReady.load();
}

void MainBoard::cancelAIWorker()
{
    // Join worker if running.
    if (m_aiThread.joinable())
        m_aiThread.join();

    m_aiThinking.store(false);
    m_aiResultReady.store(false);
}

void MainBoard::queueAITurn(float delaySeconds)
{
    if (!isAIMode() || !m_game)
        return;

    // Only queue when it's AI's turn.
    if (m_game->getTurn() != aiColor())
        return;

    // Already working / already have a move ready.
    if (m_aiThinking.load() || m_aiResultReady.load())
        return;

    // Ensure previous worker is done (should already be, but keep it safe).
    if (m_aiThread.joinable())
        m_aiThread.join();

    m_aiDelaySeconds = delaySeconds;
    m_aiDelayClock.restart();

    m_aiThinking.store(true);
    m_aiResultReady.store(false);

    // IMPORTANT:
    // Snapshot the game so the AI thread never touches the live game.
    // This prevents race/crashes if the player clicks buttons fast (undo/restart/end-game, etc.)
    std::unique_ptr<Game> snapshot;
    {
        std::lock_guard<std::mutex> lk(g_gameStateMutex);
        snapshot = std::make_unique<Game>(*m_game);
    }

    const auto diff = m_context ? m_context->m_aiDifficulty : AIDifficulty::MEDIUM;

    m_aiThread = std::thread([this, snapshot = std::move(snapshot), diff]() mutable
    {
        AIMove mv = GoAI::computeAIMove(*snapshot, diff);

        {
            std::lock_guard<std::mutex> lk(m_aiMutex);
            m_aiResult = mv;
        }

        m_aiResultReady.store(true);
        m_aiThinking.store(false);
    });
}

void MainBoard::applyAIMove(const AIMove &mv)
{
    if (!m_game || !isAIMode() || m_game->getTurn() != aiColor())
        return;

    if (mv.isPass)
    {
        m_moveRedo.clear();
        std::string who = (aiColor() == BLACK ? "Black" : "White");
        m_moveHistory.push_back(who + " Pass");
        g_histScroll = 0;

        if (m_game->pass())
            handleGameOver();
        else
        {
            setNotification("AI Passed");
            rebuildStonesFromGame();
            m_passSound.play();
        }

        invalidateMoveVisuals();
        return;
    }

    if (m_game->placeStone(mv.x, mv.y))
    {
        m_moveRedo.clear();
        std::string who = (aiColor() == BLACK ? "Black" : "White");
        m_moveHistory.push_back(who + " " + toGoCoord(mv.x, mv.y, m_boardSize));
        g_histScroll = 0;

        m_placeSound.play();
        rebuildStonesFromGame();
        setNotification("AI Played " + toGoCoord(mv.x, mv.y, m_boardSize));

        invalidateMoveVisuals();
    }
    else
    {
        // Fallback: if AI somehow produced an invalid move, pass.
        if (m_game->pass())
            handleGameOver();
    }
}

void MainBoard::pollAITurn()
{
    if (!isAIMode() || !m_game)
        return;

    // If it's AI's turn and nothing is queued yet, queue it (works for AI-first scenarios too).
    if (m_game->getTurn() == aiColor() && !m_aiThinking.load() && !m_aiResultReady.load())
    {
        queueAITurn(aiDelaySecondsForDifficulty(m_context->m_aiDifficulty));
        return;
    }

    // If result is ready, wait until the delay has passed, then apply.
    if (m_game->getTurn() == aiColor() && m_aiResultReady.load())
    {
        if (m_aiDelayClock.getElapsedTime().asSeconds() < m_aiDelaySeconds)
            return;

        AIMove mv;
        {
            std::lock_guard<std::mutex> lk(m_aiMutex);
            mv = m_aiResult;
        }

        m_aiResultReady.store(false);

        if (m_aiThread.joinable())
            m_aiThread.join();

        applyAIMove(mv);
    }
}

void MainBoard::maybeRunAITurn()
{
    // Old synchronous behavior caused ~3s freeze.
    // Now we queue AI in background and apply after a difficulty-based delay in Update().
    queueAITurn(aiDelaySecondsForDifficulty(m_context->m_aiDifficulty));
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

    const sf::Vector2f gridStart = {m_boardTopLeft.x + kGridMargin, m_boardTopLeft.y + kGridMargin};
    const float gridSpan = m_cellSize * static_cast<float>(m_boardSize - 1);

    for (int i = 0; i < m_boardSize; ++i)
    {
        float x = gridStart.x + i * m_cellSize;
        m_gridLines.push_back({{x, gridStart.y}, sf::Color::Black});
        m_gridLines.push_back({{x, gridStart.y + gridSpan}, sf::Color::Black});
    }

    for (int j = 0; j < m_boardSize; ++j)
    {
        float y = gridStart.y + j * m_cellSize;
        m_gridLines.push_back({{gridStart.x, y}, sf::Color::Black});
        m_gridLines.push_back({{gridStart.x + gridSpan, y}, sf::Color::Black});
    }
}

void MainBoard::rebuildStonesFromGame()
{
    m_stones.clear();
    if (!m_game || !m_game->getBoard())
        return;

    Board *board = m_game->getBoard();
    int size = board->getSize();

    const sf::Vector2f gridStart = {m_boardTopLeft.x + kGridMargin, m_boardTopLeft.y + kGridMargin};
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
            stone.setPosition({gridStart.x + x * m_cellSize, gridStart.y + y * m_cellSize});
            applyStoneVisual(stone, c);
            m_stones.push_back(stone);
        }
    }
}

void MainBoard::handleLeftClick(const sf::Vector2i &pixelPos)
{
    if (m_gameOver)
    {
        setNotification("Game over. Press Restart.");
        return;
    }

    sf::Vector2f posF((float)pixelPos.x, (float)pixelPos.y);
    if (!m_boardBackground.getGlobalBounds().contains(posF))
        return;

    const sf::Vector2f gridStart = {m_boardTopLeft.x + kGridMargin, m_boardTopLeft.y + kGridMargin};

    float localX = posF.x - gridStart.x;
    float localY = posF.y - gridStart.y;

    int ix = (int)(localX / m_cellSize + 0.5f);
    int iy = (int)(localY / m_cellSize + 0.5f);

    if (ix < 0 || ix >= m_boardSize || iy < 0 || iy >= m_boardSize || !m_game)
        return;

    if (isAIMode() && m_game->getTurn() != humanColor())
    {
        setNotification("Wait for AI");
        return;
    }

    if (m_game->placeStone(ix, iy))
    {
        m_placeSound.play();
        rebuildStonesFromGame();

        m_moveRedo.clear();
        std::string who = (m_game->getTurn() == WHITE ? "Black" : "White"); // turn advanced already
        std::string entry = who + " " + toGoCoord(ix, iy, m_boardSize);
        if (m_game->lastMoveCreatedKoThreat())
            entry += " (Ko)";
        m_moveHistory.push_back(entry);
        g_histScroll = 0;

        // Notification (same)
        std::string msg = who + " " + toGoCoord(ix, iy, m_boardSize);
        if (m_game->lastMoveCreatedKoThreat())
            msg += " (Ko)";

        setNotification(msg);

        invalidateMoveVisuals();
        maybeRunAITurn();
    }
    else
    {
        m_invalidSound.play();
        if (m_game->lastMoveWasKoViolation())
            setNotification("Invalid: Ko Violation");
        else if (m_game->lastMoveWasSuicide())
            setNotification("Invalid: Suicide");
        else
            setNotification("Invalid Move");
    }
}

void MainBoard::Update(sf::Time)
{
    if (m_context && m_context->m_stoneTheme != m_lastStoneTheme)
    {
        m_lastStoneTheme = m_context->m_stoneTheme;
        rebuildStonesFromGame();
    }

    if (m_context->m_requestBoardRestart)
    {
        m_context->m_requestBoardRestart = false;
        resetGame();
    }

    // Buttons
    auto setButtonColor = [&](sf::RectangleShape& box, bool hover,
                              sf::Color base = sf::Color(200, 200, 200),
                              sf::Color hov  = sf::Color(230, 230, 230))
    {
        box.setFillColor(hover ? hov : base);
    };

    auto setDangerButtonColor = [&](sf::RectangleShape& box, bool hover)
    {
        setButtonColor(box, hover, sf::Color(210, 110, 110), sf::Color(235, 135, 135));
    };

    auto setToggleButtonColor = [&](sf::RectangleShape& box, bool hover, bool active)
    {
        if (active)
            setButtonColor(box, hover, sf::Color(175, 220, 195), sf::Color(195, 240, 215));
        else
            setButtonColor(box, hover);
    };

    setButtonColor(m_undoButtonBox, m_undoHovered);
    setButtonColor(m_redoButtonBox, m_redoHovered);
    setButtonColor(m_passButtonBox, m_passHovered);
    setButtonColor(m_restartButtonBox, m_restartHovered);
    setButtonColor(m_saveButtonBox, m_saveHovered);
    setButtonColor(m_loadButtonBox, m_loadHovered);

    setDangerButtonColor(g_endGameButtonBox, g_endGameHovered);
    setToggleButtonColor(g_showMovesButtonBox, g_showMovesHovered, g_showLegalMoves);
    setButtonColor(g_hintButtonBox, g_hintHovered);

    if (m_showNotification)
    {
        if (m_notificationClock.getElapsedTime().asSeconds() > m_notificationDuration)
        {
            m_showNotification = false;
            m_notificationText.clear();
        }
    }

    pollAITurn();
}


void MainBoard::Draw()
{
    sf::RenderWindow &window = *m_context->m_window;
    auto winSize = window.getSize();

    // Theme logic
    sf::Color gridColor = sf::Color::Black;
    sf::Color boardColor = sf::Color(210, 164, 80);

    if (m_context->m_boardTheme == BoardTheme::Dark)
    {
        gridColor = sf::Color(200, 200, 200);
        boardColor = sf::Color(40, 40, 40);

        if (m_hasDarkTexture)
        {
            m_boardBackground.setTexture(&m_boardTextureDark);
            m_boardBackground.setFillColor(sf::Color::White);
        }
        else
        {
            m_boardBackground.setTexture(nullptr);
            m_boardBackground.setFillColor(boardColor);
        }
    }
    else
    {
        if (m_hasClassicTexture)
        {
            m_boardBackground.setTexture(&m_boardTextureClassic);
            m_boardBackground.setFillColor(sf::Color::White);
        }
        else
        {
            m_boardBackground.setTexture(nullptr);
            m_boardBackground.setFillColor(boardColor);
        }
    }

    for (auto &v : m_gridLines)
        v.color = gridColor;

    window.draw(m_boardBackground);

    const sf::Vector2f gridStart = {m_boardTopLeft.x + kGridMargin, m_boardTopLeft.y + kGridMargin};
    const float gridSpan = m_cellSize * static_cast<float>(m_boardSize - 1);

    // Outer grid border (bold like reference)
    sf::RectangleShape gridBorder({gridSpan, gridSpan});
    gridBorder.setPosition(gridStart);
    gridBorder.setFillColor(sf::Color::Transparent);
    gridBorder.setOutlineThickness(kOuterBorderThickness);
    gridBorder.setOutlineColor(gridColor);
    window.draw(gridBorder);

    // Grid lines
    if (!m_gridLines.empty())
        window.draw(m_gridLines.data(), m_gridLines.size(), sf::PrimitiveType::Lines);

    // Hoshi points
    {
        auto pts = hoshiIdx(m_boardSize);
        if (!pts.empty())
        {
            const float r = std::max(2.5f, m_cellSize * 0.08f);
            sf::CircleShape dot(r);
            dot.setOrigin({r, r});
            dot.setFillColor(gridColor);

            for (int ix : pts)
            {
                for (int iy : pts)
                {
                    dot.setPosition({gridStart.x + ix * m_cellSize, gridStart.y + iy * m_cellSize});
                    window.draw(dot);
                }
            }
        }
    }

    // Stone shadow only for Classic
    const StoneTheme stoneTheme = m_context ? m_context->m_stoneTheme : StoneTheme::Classic;
    const bool drawStoneShadow = (stoneTheme == StoneTheme::Classic);

    if (drawStoneShadow)
    {
        for (const auto &s : m_stones)
        {
            sf::CircleShape shadow = s;
            shadow.setTexture(nullptr, true);
            shadow.setFillColor(sf::Color(0, 0, 0, 60));
            shadow.setOutlineThickness(0.f);
            shadow.move({2.f, 2.f});
            window.draw(shadow);
        }
    }

    for (const auto &s : m_stones)
        window.draw(s);

    // ---- Show legal moves (toggle) ----
    if (g_showLegalMoves && m_game && m_game->getBoard())
    {
        if (g_legalMovesDirty)
        {
            g_legalMoveMarkers.clear();

            const int size = m_game->getBoard()->getSize();
            const float r = std::max(3.5f, m_cellSize * 0.12f);

            for (int x = 0; x < size; ++x)
            {
                for (int y = 0; y < size; ++y)
                {
                    if (!isLegalCandidate(m_game.get(), x, y))
                        continue;

                    sf::CircleShape c(r);
                    c.setOrigin({r, r});
                    c.setPosition({gridStart.x + x * m_cellSize, gridStart.y + y * m_cellSize});
                    // subtle "candidate" look
                    c.setFillColor(sf::Color(0, 255, 120, 55));
                    c.setOutlineThickness(1.4f);
                    c.setOutlineColor(sf::Color(0, 255, 120, 140));
                    g_legalMoveMarkers.push_back(c);
                }
            }

            g_legalMovesDirty = false;
        }

        for (const auto &m : g_legalMoveMarkers)
            window.draw(m);
    }
    else
    {
        // keep memory small when toggle is off
        if (!g_legalMoveMarkers.empty())
            g_legalMoveMarkers.clear();
    }

    // ---- Hint (one-shot AI suggestion highlight) ----
    if (g_hintMove && !g_hintMove->isPass && g_hintClock.getElapsedTime().asSeconds() <= kHintShowSeconds)
    {
        const float r = std::max(6.0f, m_cellSize * 0.18f);
        sf::CircleShape ring(r);
        ring.setOrigin({r, r});
        ring.setPosition({gridStart.x + g_hintMove->x * m_cellSize, gridStart.y + g_hintMove->y * m_cellSize});
        ring.setFillColor(sf::Color(255, 230, 120, 25));
        ring.setOutlineThickness(std::max(2.5f, m_cellSize * 0.06f));
        ring.setOutlineColor(sf::Color(255, 230, 120, 225));
        window.draw(ring);
    }

    // ---- Coordinates (Top+Bottom letters, Left+Right numbers) ----
    const auto &font = m_context->m_assets->GetFont(MAIN_FONT);

    const bool dark = (m_context->m_boardTheme == BoardTheme::Dark);
    const sf::Color coordColor = dark ? sf::Color(235, 235, 235) : sf::Color::Black;
    const sf::Color coordShadow = dark ? sf::Color(0, 0, 0, 140) : sf::Color(255, 255, 255, 140);

    auto drawTextNice = [&](const std::string &str, sf::Vector2f pos, unsigned int size)
    {
        sf::Text t(font, str, size);
        t.setFillColor(coordColor);

        sf::Text sh = t;
        sh.setFillColor(coordShadow);

        auto b = t.getLocalBounds();
        t.setOrigin({b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f});
        sh.setOrigin(t.getOrigin());

        sh.setPosition(pos + sf::Vector2f(1.f, 1.f));
        t.setPosition(pos);

        window.draw(sh);
        window.draw(t);
    };

    const std::string letters = makeGoLetters(m_boardSize);

    // top letters
    {
        const float y = m_boardTopLeft.y + kGridMargin * 0.45f;
        for (int i = 0; i < m_boardSize; ++i)
        {
            const float x = gridStart.x + i * m_cellSize;
            std::string s(1, (i < (int)letters.size() ? letters[i] : '?'));
            drawTextNice(s, {x, y}, (unsigned)kCoordFontSize);
        }
    }

    // bottom letters
    {
        const float y = m_boardTopLeft.y + m_boardPixelSize - kGridMargin * 0.45f;
        for (int i = 0; i < m_boardSize; ++i)
        {
            const float x = gridStart.x + i * m_cellSize;
            std::string s(1, (i < (int)letters.size() ? letters[i] : '?'));
            drawTextNice(s, {x, y}, (unsigned)kCoordFontSize);
        }
    }

    // left + right numbers (19 at top, 1 at bottom)
    {
        const float xL = m_boardTopLeft.x + kGridMargin * 0.45f;
        const float xR = m_boardTopLeft.x + m_boardPixelSize - kGridMargin * 0.45f;

        for (int j = 0; j < m_boardSize; ++j)
        {
            const float y = gridStart.y + j * m_cellSize;
            const int label = (m_boardSize - j);
            drawTextNice(std::to_string(label), {xL, y}, (unsigned)kCoordFontSize);
            drawTextNice(std::to_string(label), {xR, y}, (unsigned)kCoordFontSize);
        }
    }
    // ---- Side panel ----
    const float sidePanelX = (float)winSize.x - kSidePanelW - kSidePanelMargin;

    // subtle shadow
    sf::RectangleShape sideShadow({kSidePanelW, kSidePanelH});
    sideShadow.setPosition({sidePanelX + 3.f, kPanelTop + 3.f});
    sideShadow.setFillColor(sf::Color(0, 0, 0, 90));
    window.draw(sideShadow);

    sf::RectangleShape sidePanel;
    sidePanel.setSize({kSidePanelW, kSidePanelH});
    sidePanel.setFillColor(sf::Color(35, 35, 35, 235));
    sidePanel.setPosition({sidePanelX, kPanelTop});
    sidePanel.setOutlineThickness(1.f);
    sidePanel.setOutlineColor(sf::Color(95, 95, 95));
    window.draw(sidePanel);

    // header
    {
        sf::Text panelTitle(font, "Controls", 18);
        panelTitle.setFillColor(sf::Color(235, 235, 235));
        panelTitle.setStyle(sf::Text::Bold);
        panelTitle.setPosition({sidePanelX + kSidePanelPad, kPanelTop + 12.f});
        window.draw(panelTitle);
    }


    // ---- Buttons ----
    auto drawBtn = [&](sf::RectangleShape &box, const std::string &str)
    {
        window.draw(box);

        const sf::Color bg = box.getFillColor();
        const int lum = (int(bg.r) + int(bg.g) + int(bg.b)) / 3;
        const sf::Color fg = (lum < 140) ? sf::Color::White : sf::Color::Black;

        sf::Text txt(font, str, 18);
        txt.setFillColor(fg);
        txt.setStyle(sf::Text::Bold);

        sf::FloatRect b = txt.getLocalBounds();
        txt.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        sf::Vector2f c = box.getPosition() + box.getSize() / 2.f;
        txt.setPosition(c);
        window.draw(txt);
    };


    drawBtn(m_undoButtonBox, "Undo");
    drawBtn(m_redoButtonBox, "Redo");
    drawBtn(m_passButtonBox, "Pass");
    drawBtn(m_restartButtonBox, "Restart");
    drawBtn(m_saveButtonBox, "Save");
    drawBtn(m_loadButtonBox, "Load");
    drawBtn(g_endGameButtonBox, "End Game");

    drawBtn(g_showMovesButtonBox, g_showLegalMoves ? "Legal: ON" : "Legal: OFF");
    drawBtn(g_hintButtonBox, "Hint");

    // ---- Turn Panel ----
    if (m_game)
    {
        PieceColor current = m_game->getTurn();
        std::string turnStr = (current == BLACK ? "Turn: Black" : (current == WHITE ? "Turn: White" : "Turn: -"));

        sf::RectangleShape turnPanel({160.f, 40.f});
        turnPanel.setFillColor(sf::Color(30, 30, 30, 220));
        turnPanel.setOutlineThickness(1.f);
        turnPanel.setOutlineColor(sf::Color(80, 80, 80));

        // Anchor near the board (looks more intentional than hard-coded screen coords)
        const float tpX = m_boardTopLeft.x;
        const float tpY = std::max(12.f, m_boardTopLeft.y - 52.f);
        turnPanel.setPosition({tpX, tpY});
        window.draw(turnPanel);

        sf::Text turnText(font, turnStr, 18);
        turnText.setFillColor(sf::Color::White);
        turnText.setStyle(sf::Text::Bold);
        turnText.setPosition({tpX + 14.f, tpY + 9.f});
        window.draw(turnText);

        sf::CircleShape turnStone(10.f);
        turnStone.setOrigin({10.f, 10.f});
        applyStoneVisual(turnStone, current);
        turnStone.setPosition({tpX + 142.f, tpY + 20.f});
        window.draw(turnStone);
    }

    // ---- Bottom bar ----
    sf::RectangleShape bottomBar({(float)winSize.x, kBottomBarH});
    bottomBar.setPosition({0.f, (float)winSize.y - kBottomBarH});
    bottomBar.setFillColor(sf::Color(30, 30, 30, 230));
    window.draw(bottomBar);

    sf::Text hint(font, "ESC: Back | Click: Place | Z: Undo | Y: Redo | P: Pass | R: Restart | E: End | M/L: Legal | H: Hint", 16);
    hint.setFillColor(sf::Color::White);
    hint.setStyle(sf::Text::Bold);
    hint.setPosition({10.f, (float)winSize.y - kBottomBarH + 5.f});
    window.draw(hint);

    // ---- Notification bar ----
    if (m_showNotification && !m_notificationText.empty())
    {
        sf::RectangleShape notifBar({(float)winSize.x, 24.f});
        notifBar.setPosition({0.f, (float)winSize.y - kBottomBarH - 24.f});
        notifBar.setFillColor(sf::Color(50, 50, 50, 230));
        notifBar.setOutlineThickness(1.f);
        notifBar.setOutlineColor(sf::Color(200, 200, 0, 180));
        window.draw(notifBar);

        sf::Text notifText(font, m_notificationText, 16);
        notifText.setFillColor(sf::Color(255, 255, 180));
        notifText.setStyle(sf::Text::Bold);
        notifText.setPosition({10.f, (float)winSize.y - kBottomBarH - 24.f + 4.f});
        window.draw(notifText);
    }

    // ---- Move History (scrollable, under side panel) ----
    {
        sf::FloatRect rect = computeHistoryRect(winSize);

        sf::RectangleShape histPanel({rect.size.x, rect.size.y});
        histPanel.setPosition({rect.position.x, rect.position.y});
        histPanel.setFillColor(sf::Color(30, 30, 30, 220));
        histPanel.setOutlineThickness(1.f);
        histPanel.setOutlineColor(sf::Color(80, 80, 80));
        window.draw(histPanel);

        sf::Text title(font, "Moves", 16);
        title.setFillColor(sf::Color::White);
        title.setStyle(sf::Text::Bold);
        title.setPosition({rect.position.x + 10.f, rect.position.y + 8.f});
        window.draw(title);

        const float startY = rect.position.y + 34.f;
        const float lineH = 16.f;

        int maxLines = (int)((rect.size.y - 44.f) / lineH);
        maxLines = std::max(1, maxLines);

        clampHistoryScroll((int)m_moveHistory.size(), maxLines);

        // g_histScroll = how many lines from the bottom
        int start = std::max(0, (int)m_moveHistory.size() - maxLines - g_histScroll);
        int end = std::min((int)m_moveHistory.size(), start + maxLines);

        float lineY = startY;
        for (int i = start; i < end; ++i)
        {
            sf::Text line(font, std::to_string(i + 1) + ". " + m_moveHistory[i], 14);
            // highlight the last move only when we are at bottom (scroll=0) and it's visible
            bool isLast = (i == (int)m_moveHistory.size() - 1);
            line.setFillColor((isLast && g_histScroll == 0) ? sf::Color(255, 255, 180) : sf::Color(220, 220, 220));
            line.setPosition({rect.position.x + 10.f, lineY});
            window.draw(line);
            lineY += lineH;
        }

        if (m_moveHistory.empty())
        {
            sf::Text empty(font, "(no moves yet)", 14);
            empty.setFillColor(sf::Color(180, 180, 180));
            empty.setPosition({rect.position.x + 10.f, startY});
            window.draw(empty);
        }

        // small scroll hint
        if ((int)m_moveHistory.size() > maxLines)
        {
            sf::Text scrollHint(font, "Wheel: scroll", 12);
            scrollHint.setFillColor(sf::Color(160, 160, 160));
            scrollHint.setPosition({rect.position.x + 10.f, rect.position.y + rect.size.y - 18.f});
            window.draw(scrollHint);
        }
    }

    // ---- Save/Load overlay ----
    if (g_saveLoad.open)
    {
        sf::RectangleShape dimmer({(float)winSize.x, (float)winSize.y});
        dimmer.setFillColor(sf::Color(0, 0, 0, 150));
        window.draw(dimmer);

        sf::Vector2f pSize = g_saveLoad.panelSize;
        sf::Vector2f pPos(((float)winSize.x - pSize.x) / 2.f, ((float)winSize.y - pSize.y) / 2.f);
        sf::RectangleShape panel(pSize);
        panel.setPosition(pPos);
        panel.setFillColor(sf::Color(50, 50, 50));
        panel.setOutlineThickness(2.f);
        panel.setOutlineColor(sf::Color::White);
        window.draw(panel);

        if (m_menuTitleText)
        {
            m_menuTitleText->setString(g_saveLoad.mode == SaveLoadMode::Save ? "Save Game" : "Load Game");
            sf::FloatRect b = m_menuTitleText->getLocalBounds();
            m_menuTitleText->setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
            m_menuTitleText->setPosition({pPos.x + pSize.x / 2.f, pPos.y + 30.f});
            m_menuTitleText->setFillColor(sf::Color::White);
            window.draw(*m_menuTitleText);
        }

        float listY = pPos.y + 70.f;
        float listW = pSize.x - 40.f;
        sf::RectangleShape listBg({listW, g_saveLoad.rowHeight * g_saveLoad.visibleRows});
        listBg.setPosition({pPos.x + 20.f, listY});
        listBg.setFillColor(sf::Color(30, 30, 30));
        window.draw(listBg);

        if (m_fileListText)
        {
            m_fileListText->setFillColor(sf::Color::White);
            for (int i = 0; i < g_saveLoad.visibleRows; ++i)
            {
                int idx = g_saveLoad.scroll + i;
                if (idx >= (int)g_saveLoad.files.size())
                    break;

                if (idx == g_saveLoad.selected)
                {
                    sf::RectangleShape hl({listW, g_saveLoad.rowHeight});
                    hl.setPosition({pPos.x + 20.f, listY + i * g_saveLoad.rowHeight});
                    hl.setFillColor(sf::Color(100, 100, 150));
                    window.draw(hl);
                }

                m_fileListText->setString(g_saveLoad.files[idx]);
                m_fileListText->setOrigin({0, 0});
                m_fileListText->setPosition({pPos.x + 25.f, listY + i * g_saveLoad.rowHeight + 4.f});
                window.draw(*m_fileListText);
            }
        }

        sf::Text menuHint(font, "Hints: Click file to select | Enter = Action | Del = Delete | ESC to Close", 14);
        menuHint.setFillColor(sf::Color(200, 200, 200));
        menuHint.setPosition({pPos.x + 25.f, listY + g_saveLoad.rowHeight * g_saveLoad.visibleRows + 10.f});
        window.draw(menuHint);

        float btnY = pPos.y + pSize.y - 60.f;
        auto drawMenuBtn = [&](float x, std::optional<sf::Text> &txt, std::string label, sf::Color bg)
        {
            sf::RectangleShape r({g_saveLoad.btnWidth, g_saveLoad.btnHeight});
            r.setPosition({x, btnY});
            r.setFillColor(bg);
            window.draw(r);
            if (txt)
            {
                txt->setString(label);
                sf::FloatRect b = txt->getLocalBounds();
                txt->setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
                txt->setPosition({x + g_saveLoad.btnWidth / 2.f, btnY + g_saveLoad.btnHeight / 2.f});
                txt->setFillColor(sf::Color::Black);
                window.draw(*txt);
            }
        };

        drawMenuBtn(pPos.x + 20.f, m_menuDeleteText, "Delete", sf::Color(150, 50, 50));
        drawMenuBtn(pPos.x + (pSize.x - g_saveLoad.btnWidth) * 0.5f, m_menuCancelText, "Cancel", sf::Color(100, 100, 100));

        std::string act =
            (g_saveLoad.mode == SaveLoadMode::Save && g_saveLoad.selected >= 0) ? "Overwrite"
                                                                                : (g_saveLoad.mode == SaveLoadMode::Save ? "Save New" : "Load");

        drawMenuBtn(pPos.x + pSize.x - g_saveLoad.btnWidth - 20.f, m_menuActionText, act, sf::Color(50, 150, 50));
    }
}

void MainBoard::ProcessInput()
{
    auto openMenu = [&](SaveLoadMode m)
    {
        g_saveLoad.open = true;
        g_saveLoad.mode = m;
        g_saveLoad.refresh();
        g_saveLoad.clampScroll();
    };

    auto doDelete = [&]()
    {
        if (g_saveLoad.selected < 0 || g_saveLoad.selected >= (int)g_saveLoad.files.size())
        {
            setNotification("No file selected.");
            return;
        }

        std::string fname = g_saveLoad.files[g_saveLoad.selected];
        if (tryDeleteSaveFile(fname))
            setNotification("Deleted: " + fname);
        else
            setNotification("Failed to delete " + fname);

        g_saveLoad.refresh();
    };

    auto doAction = [&]()
    {
        if (g_saveLoad.mode == SaveLoadMode::Load)
        {
            if (g_saveLoad.selected >= 0 && g_saveLoad.selected < (int)g_saveLoad.files.size())
            {
                if (m_game->loadNamed(g_saveLoad.files[g_saveLoad.selected]))
                {
                    rebuildStonesFromGame();
                    
                    m_gameOver = false;
m_moveRedo.clear();

                    // Load history from sidecar .hist
                    if (!readHistoryFile(g_saveLoad.files[g_saveLoad.selected], m_moveHistory))
                    {
                        m_moveHistory.clear();
                        m_moveHistory.push_back("Loaded: " + g_saveLoad.files[g_saveLoad.selected]);
                    }

                    g_histScroll = 0;

                    invalidateMoveVisuals();

                    setNotification("Loaded: " + g_saveLoad.files[g_saveLoad.selected]);
                    g_saveLoad.open = false;

                    // If AI is to play after loading, let it play and be logged
                    maybeRunAITurn();
                }
            }
            else
            {
                setNotification("No file selected to load.");
            }
        }
        else
        {
            if (g_saveLoad.selected >= 0 && g_saveLoad.selected < (int)g_saveLoad.files.size())
            {
                m_game->saveNamed(g_saveLoad.files[g_saveLoad.selected]);
                writeHistoryFile(g_saveLoad.files[g_saveLoad.selected], m_moveHistory);

                setNotification("Overwritten: " + g_saveLoad.files[g_saveLoad.selected]);
                g_saveLoad.open = false;
            }
            else
            {
                std::string n;
                m_game->saveToNewSlot(n);
                writeHistoryFile(n, m_moveHistory);

                setNotification("Saved New: " + n);
                g_saveLoad.open = false;
            }
        }
    };

    while (const std::optional event = m_context->m_window->pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_context->m_window->close();
            return;
        }

        // --- MENU INPUT ---
        if (g_saveLoad.open)
        {
            if (const auto *key = event->getIf<sf::Event::KeyPressed>())
            {
                if (key->scancode == sf::Keyboard::Scancode::Escape)
                    g_saveLoad.open = false;
                else if (key->scancode == sf::Keyboard::Scancode::Up)
                {
                    g_saveLoad.selected = std::max(0, g_saveLoad.selected - 1);
                    if (g_saveLoad.selected < g_saveLoad.scroll)
                        g_saveLoad.scroll = g_saveLoad.selected;
                }
                else if (key->scancode == sf::Keyboard::Scancode::Down && !g_saveLoad.files.empty())
                {
                    g_saveLoad.selected = std::min((int)g_saveLoad.files.size() - 1, g_saveLoad.selected + 1);
                    if (g_saveLoad.selected >= g_saveLoad.scroll + g_saveLoad.visibleRows)
                        g_saveLoad.scroll++;
                }
                else if (key->scancode == sf::Keyboard::Scancode::Delete)
                    doDelete();
                else if (key->scancode == sf::Keyboard::Scancode::Enter)
                    doAction();
            }
            else if (const auto *wheel = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (!g_saveLoad.files.empty())
                {
                    g_saveLoad.scroll -= (int)wheel->delta;
                    g_saveLoad.clampScroll();
                }
            }
            else if (const auto *mb = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mb->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f mPos((float)mb->position.x, (float)mb->position.y);
                    auto winSize = m_context->m_window->getSize();
                    sf::Vector2f pSize = g_saveLoad.panelSize;
                    sf::Vector2f pPos(((float)winSize.x - pSize.x) / 2.f, ((float)winSize.y - pSize.y) / 2.f);

                    sf::FloatRect panelRect({pPos.x, pPos.y}, {pSize.x, pSize.y});
                    if (!panelRect.contains(mPos))
                    {
                        g_saveLoad.open = false;
                        continue;
                    }

                    float listY = pPos.y + 70.f;
                    float listH = g_saveLoad.rowHeight * g_saveLoad.visibleRows;
                    sf::FloatRect listRect({pPos.x + 20.f, listY}, {pSize.x - 40.f, listH});

                    if (listRect.contains(mPos))
                    {
                        int clickedRow = (int)((mPos.y - listY) / g_saveLoad.rowHeight);
                        int idx = g_saveLoad.scroll + clickedRow;
                        if (idx >= 0 && idx < (int)g_saveLoad.files.size())
                            g_saveLoad.selected = idx;
                        else
                            g_saveLoad.selected = -1;
                    }

                    float btnY = pPos.y + pSize.y - 60.f;
                    sf::FloatRect delRect({pPos.x + 20.f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});
                    sf::FloatRect canRect({pPos.x + (pSize.x - g_saveLoad.btnWidth) * 0.5f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});
                    sf::FloatRect actRect({pPos.x + pSize.x - g_saveLoad.btnWidth - 20.f, btnY}, {g_saveLoad.btnWidth, g_saveLoad.btnHeight});

                    if (delRect.contains(mPos))
                        doDelete();
                    else if (canRect.contains(mPos))
                        g_saveLoad.open = false;
                    else if (actRect.contains(mPos))
                        doAction();
                }
            }

            continue;
        }

        // --- GAME INPUT ---
        if (const auto *key = event->getIf<sf::Event::KeyPressed>())
        {
            if (key->scancode == sf::Keyboard::Scancode::Escape)
            {
                m_context->m_states->PopCurrent();
                return;
            }
            if (key->scancode == sf::Keyboard::Scancode::Z)
            {
                if (m_game->undo())
                {
                    rebuildStonesFromGame();
                    if (!m_moveHistory.empty())
                    {
                        m_moveRedo.push_back(m_moveHistory.back());
                        m_moveHistory.pop_back();
                    }
                    // keep scroll clamped

                    invalidateMoveVisuals();
                }
            }
            if (key->scancode == sf::Keyboard::Scancode::Y)
            {
                if (m_game->redo())
                {
                    rebuildStonesFromGame();
                    if (!m_moveRedo.empty())
                    {
                        m_moveHistory.push_back(m_moveRedo.back());
                        m_moveRedo.pop_back();
                    }

                    invalidateMoveVisuals();
                }
            }
            if (key->scancode == sf::Keyboard::Scancode::P)
            {
                if (m_gameOver) { setNotification("Game over. Press Restart."); continue; }

                m_passSound.play();
                if (m_game->pass())
                    handleGameOver();
                else
                    maybeRunAITurn();

                m_moveRedo.clear();
                // because turn already advanced after pass(), the passer is the opposite
                std::string who = (m_game->getTurn() == BLACK ? "White" : "Black");
                m_moveHistory.push_back(who + " Pass");
                g_histScroll = 0;

                invalidateMoveVisuals();
            }

            
            if (key->scancode == sf::Keyboard::Scancode::R)
            {
                resetGame();
                setNotification("Restarted");
            }
// Toggle "Show legal moves" (requested feature)
            // L = Legal moves, keep M as a backup shortcut.
            if (key->scancode == sf::Keyboard::Scancode::L || key->scancode == sf::Keyboard::Scancode::M)
            {
                g_showLegalMoves = !g_showLegalMoves;
                g_legalMovesDirty = true;
                setNotification(g_showLegalMoves ? "Legal moves: ON" : "Legal moves: OFF");
            }
            if (key->scancode == sf::Keyboard::Scancode::H)
            {
                // Simple anti-spam cooldown (0.5s)
                if (g_hintCooldown.getElapsedTime().asSeconds() < kHintCooldownSeconds)
                {
                    // ignore fast repeats
                    continue;
                }
                g_hintCooldown.restart();

                if (aiBusy())
                {
                    setNotification("Hint: AI is thinking...");
                }
                else if (isAIMode() && m_game->getTurn() != humanColor())
                {
                    setNotification("Hint: waiting for AI...");
                }
                else
                {
                    // Keep hint cheap: always use MEDIUM.
                    setNotification("Hint: thinking...");
                    g_hintMove = GoAI::computeAIMove(*m_game, AIDifficulty::MEDIUM);
                    g_hintClock.restart();

                    if (g_hintMove && g_hintMove->isPass)
                        setNotification("Hint: Pass");
                    else if (g_hintMove)
                        setNotification("Hint: " + toGoCoord(g_hintMove->x, g_hintMove->y, m_boardSize));
                    else
                        setNotification("Hint: (no move)");
                }
            }
            if (key->scancode == sf::Keyboard::Scancode::E)
            {
                handleGameOver();
            }
        }
        else if (const auto *wheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            // History panel scroll (only when cursor is over history panel)
            sf::Vector2f mPos((float)wheel->position.x, (float)wheel->position.y);
            sf::FloatRect histRect = computeHistoryRect(m_context->m_window->getSize());

            if (histRect.contains(mPos))
            {
                // Wheel up (delta>0) -> older moves -> increase scroll
                int d = (int)std::round(wheel->delta);
                g_histScroll += d;

                // clamp based on current visible lines estimate
                // (use same as Draw's line calc)
                const float lineH = 16.f;
                int maxLines = (int)((histRect.size.y - 44.f) / lineH);
                maxLines = std::max(1, maxLines);
                clampHistoryScroll((int)m_moveHistory.size(), maxLines);
            }
        }
        else if (const auto *mm = event->getIf<sf::Event::MouseMoved>())
        {
            sf::Vector2f mp((float)mm->position.x, (float)mm->position.y);
            m_undoHovered = m_undoButtonBox.getGlobalBounds().contains(mp);
            m_redoHovered = m_redoButtonBox.getGlobalBounds().contains(mp);
            m_passHovered = m_passButtonBox.getGlobalBounds().contains(mp);
            m_restartHovered = m_restartButtonBox.getGlobalBounds().contains(mp);
            m_saveHovered = m_saveButtonBox.getGlobalBounds().contains(mp);
            m_loadHovered = m_loadButtonBox.getGlobalBounds().contains(mp);

            g_endGameHovered = g_endGameButtonBox.getGlobalBounds().contains(mp);
            g_showMovesHovered = g_showMovesButtonBox.getGlobalBounds().contains(mp);
            g_hintHovered = g_hintButtonBox.getGlobalBounds().contains(mp);
        }
        else if (const auto *mb = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (mb->button == sf::Mouse::Button::Left)
            {
                sf::Vector2f mp((float)mb->position.x, (float)mb->position.y);

                if (m_undoButtonBox.getGlobalBounds().contains(mp))
                {
                    if (m_game->undo())
                    {
                        rebuildStonesFromGame();
                        if (!m_moveHistory.empty())
                        {
                            m_moveRedo.push_back(m_moveHistory.back());
                            m_moveHistory.pop_back();
                        }

                        invalidateMoveVisuals();
                    }
                }
                else if (m_redoButtonBox.getGlobalBounds().contains(mp))
                {
                    if (m_game->redo())
                    {
                        rebuildStonesFromGame();
                        if (!m_moveRedo.empty())
                        {
                            m_moveHistory.push_back(m_moveRedo.back());
                            m_moveRedo.pop_back();
                        }

                        invalidateMoveVisuals();
                    }
                }
                else if (m_passButtonBox.getGlobalBounds().contains(mp))
                {
                    if (m_gameOver) { setNotification("Game over. Press Restart."); continue; }

                    m_passSound.play();
                    if (m_game->pass())
                        handleGameOver();
                    else
                        maybeRunAITurn();

                    m_moveRedo.clear();
                    std::string who = (m_game->getTurn() == BLACK ? "White" : "Black");
                    m_moveHistory.push_back(who + " Pass");
                    g_histScroll = 0;

                    invalidateMoveVisuals();
                }
                else if (m_restartButtonBox.getGlobalBounds().contains(mp))
                {
                    resetGame();
                    setNotification("Restarted");
                }
                else if (m_saveButtonBox.getGlobalBounds().contains(mp))
                {
                    openMenu(SaveLoadMode::Save);
                }
                else if (m_loadButtonBox.getGlobalBounds().contains(mp))
                {
                    openMenu(SaveLoadMode::Load);
                }
                else if (g_endGameButtonBox.getGlobalBounds().contains(mp))
                {
                    handleGameOver();
                }
                else if (g_showMovesButtonBox.getGlobalBounds().contains(mp))
                {
                    g_showLegalMoves = !g_showLegalMoves;
                    g_legalMovesDirty = true;
                    setNotification(g_showLegalMoves ? "Legal moves: ON" : "Legal moves: OFF");
                }
                else if (g_hintButtonBox.getGlobalBounds().contains(mp))
                {
                    // Same cooldown as keyboard (prevents spam / stutter)
                    if (g_hintCooldown.getElapsedTime().asSeconds() < kHintCooldownSeconds)
                        continue;
                    g_hintCooldown.restart();

                    if (aiBusy())
                    {
                        setNotification("Hint: AI is thinking...");
                    }
                    else if (isAIMode() && m_game->getTurn() != humanColor())
                    {
                        setNotification("Hint: waiting for AI...");
                    }
                    else
                    {
                        setNotification("Hint: thinking...");
                        g_hintMove = GoAI::computeAIMove(*m_game, AIDifficulty::MEDIUM);
                        g_hintClock.restart();

                        if (g_hintMove && g_hintMove->isPass)
                            setNotification("Hint: Pass");
                        else if (g_hintMove)
                            setNotification("Hint: " + toGoCoord(g_hintMove->x, g_hintMove->y, m_boardSize));
                        else
                            setNotification("Hint: (no move)");
                    }
                }
                else
                {
                    handleLeftClick({mb->position.x, mb->position.y});
                }
            }
        }
    }
}

void MainBoard::resetGame()
{
    cancelAIWorker();

    m_gameOver = false;

    m_game = std::make_unique<Game>(new Board());
    m_stones.clear();
    rebuildStonesFromGame();

    m_moveHistory.clear();
    m_moveRedo.clear();
    g_histScroll = 0;

    invalidateMoveVisuals();

    maybeRunAITurn();
}
