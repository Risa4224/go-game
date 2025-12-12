// game.h
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include "group.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Game {
private:
    int black_captures = 0;
    int white_captures = 0;
    int consecutive_passes = 0;

    // Own the board safely
    std::unique_ptr<Board> board;

    PieceColor turn = BLACK;
    std::vector<PieceGroup> groups;

    // Fast mapping: cell -> group index, -1 if empty
    std::array<int, BOARD_CELLS> groupAt{};

    // Undo/redo timeline (snapshots)
    std::vector<Game> history;
    std::vector<Game> future;

    // IMPORTANT optimization:
    // - Main game instance records history (true)
    // - Snapshots / copies (used in AI search, history, redo stack) do NOT record history (false)
    bool m_enableHistory = true;


    // Simple ko reference: board position before the last move (used to detect immediate ko recapture),
    // maintained even when history recording is disabled (AI snapshots).
    Board m_koRefBoard{};
    bool  m_hasKoRef = false;

    // Endgame scoring support: mark dead stones/groups (for Japanese-style scoring).
    // 0 = alive/ignored, 1 = marked dead (removed for scoring and counted as captures for opponent).
    std::array<std::uint8_t, BOARD_CELLS> m_deadMark{};


    // internal helpers
    bool valid(int x, int y) const;
    int  processGroups(int x, int y, PieceColor c); // returns group index of placed stone
    int  checkCapturesAround(int x, int y, PieceColor placedColor);
    int  removeGroupByIndex(int groupIdx); // returns number of stones removed
    int  calcLiberties(int groupIdx) const;

    // territory (scoring)
    PieceColor getTerritoryOwner(int startX, int startY, int& territory_size, std::vector<std::uint8_t>& visited) const;

    // last move flags for UI
    int  m_lastCaptures     = 0;
    bool m_lastInvalid      = false;
    bool m_lastSuicide      = false;
    bool m_lastKoViolation  = false;
    bool m_lastKoThreat     = false;

public:
    ~Game() = default;
    Game(const Game& other);
    Game& operator=(const Game& other);
    explicit Game(Board* b);

    PieceColor getTurn() const { return turn; }
    PieceColor getPiece(int x, int y) const { return board->getPiece(x, y); }
    Board* getBoard() const { return board.get(); }

    PieceColor oppositeColor(PieceColor input) const;

    bool placeStone(int x, int y);
    bool pass();

    bool undo();
    bool redo();

    // Giữ signature cũ để không vỡ UI, nhưng logic thực dùng pass>=2.
    bool ended(int /*x*/ = 0, int /*y*/ = 0) { return consecutive_passes >= 2; }

    // simple ko check helper (so sánh với trạng thái trước đó)
    bool checkKO() const;


    // Dead-stone marking (optional UI feature for correct endgame scoring).
    // These do NOT affect gameplay legality; only used by calculateFinalScore().
    void clearDeadMarks();
    bool toggleDeadGroupAt(int x, int y);
    bool isDeadAt(int x, int y) const;

    std::pair<float, float> calculateFinalScore(float komi = 6.5f) const;

    void printDebug() const;

    // persistence
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);

    void rebuildGroupsFromBoard();

    // UI helpers
    int  getLastCaptures() const        { return m_lastCaptures; }
    bool lastMoveWasInvalid() const     { return m_lastInvalid; }
    bool lastMoveWasSuicide() const     { return m_lastSuicide; }
    bool lastMoveWasKoViolation() const { return m_lastKoViolation; }
    bool lastMoveCreatedKoThreat() const{ return m_lastKoThreat; }

    static inline const std::string SAVE_DIR = "saved_game";
    bool saveNamed(const std::string& name) const;
    bool saveToNewSlot(std::string& outFilename) const;
    static std::vector<std::string> listSaveFiles();
    bool loadNamed(const std::string& name);
};

#endif // GAME_H
