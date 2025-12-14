#ifndef GAME_H
#define GAME_H

#include "board.h"
#include "group.h"
#include <array>
#include <cstddef>
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

    // IMPORTANT optimization:
    // - Main game instance records move timeline (undo/redo + save)
    // - Snapshots / copies (used in AI search) do NOT record the timeline
    bool m_enableHistory = true;


    // Undo/redo timeline (compact): base state + move list + cursor.
    // Stored as a member (NOT static/global) to avoid leaks and pointer-reuse bugs.
    struct MoveRec {
        PieceColor color = NONE;
        int x = -1;
        int y = -1;
        bool isPass = false;
    };

    struct BaseState {
        Board board;
        PieceColor turn = BLACK;
        int black_captures = 0;
        int white_captures = 0;
        int consecutive_passes = 0;
        Board koRefBoard;
        bool hasKoRef = false;
        std::array<std::uint8_t, BOARD_CELLS> deadMark{};
    };

    struct Timeline {
        BaseState base;
        std::vector<MoveRec> moves;
        std::size_t cursor = 0; // number of moves already applied
        bool record = true;     // disabled during replay
    };

    Timeline m_timeline;



    // Simple ko reference: board position before the last move (used to detect immediate ko recapture),
    // maintained even when history recording is disabled (AI snapshots).
    Board m_koRefBoard{};
    bool  m_hasKoRef = false;
    // Zobrist hashes for fast ko detection (and future transposition/TT).
    std::uint64_t m_boardHash = 0; // hash of current board position
    std::uint64_t m_koRefHash = 0; // hash of ko reference position (two plies ago)


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
    Board* getBoard() { return board.get(); }
    const Board* getBoard() const { return board.get(); }
    std::uint64_t getBoardHash() const { return m_boardHash; }
    std::uint64_t getKoRefHash() const { return m_koRefHash; }
    bool hasKoRef() const { return m_hasKoRef; }
    int getBlackCaptures() const { return black_captures; }
    int getWhiteCaptures() const { return white_captures; }

    PieceColor oppositeColor(PieceColor input) const;

    bool placeStone(int x, int y);
    bool pass();

    bool undo();
    bool redo();

    bool ended(int /*x*/ = 0, int /*y*/ = 0) const { return consecutive_passes >= 2; }

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
