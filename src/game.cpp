#include "game.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <queue>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    // stamp để tránh std::set trong calcLiberties
    struct Stamp {
        std::array<int, BOARD_CELLS> mark{};
        int token = 1;
        int next() {
            if (token >= 1'000'000'000) { mark.fill(0); token = 1; }
            return ++token;
        }
    };
    thread_local Stamp g_seenLib;
    thread_local Stamp g_seenStone;

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

    static std::unordered_map<const Game*, Timeline> g_timeline;

    static Timeline* findTimeline(const Game* g) {
        auto it = g_timeline.find(g);
        if (it == g_timeline.end()) return nullptr;
        return &it->second;
    }

    static Timeline& ensureTimeline(const Game* g) {
        return g_timeline.try_emplace(g).first->second;
    }

    static bool isUnsignedIntToken(const std::string& s) {
        if (s.empty()) return false;
        for (unsigned char ch : s) {
            if (!std::isdigit(ch)) return false;
        }
        return true;
    }
    // ---------------------------
    // Zobrist hashing (fast ko)
    // ---------------------------
    static inline std::uint64_t splitmix64(std::uint64_t& x) {
        std::uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    static inline int zobristColorIndex(PieceColor c) {
        // Map {WHITE, BLACK} -> {0, 1}
        if (c == WHITE) return 0;
        if (c == BLACK) return 1;
        return -1;
    }

    static const std::array<std::array<std::uint64_t, 2>, BOARD_CELLS>& zobristTable() {
        static const auto table = [] {
            std::array<std::array<std::uint64_t, 2>, BOARD_CELLS> t{};
            // Fixed seed -> deterministic (important for save/load reproducibility)
            std::uint64_t seed = 0xC0FFEE1234ULL;
            for (int i = 0; i < BOARD_CELLS; ++i) {
                t[i][0] = splitmix64(seed); // WHITE
                t[i][1] = splitmix64(seed); // BLACK
            }
            return t;
        }();
        return table;
    }

    static std::uint64_t computeBoardHash(const Board& b) {
        const auto& Z = zobristTable();
        std::uint64_t h = 0;
        for (int y = 0; y < BOARD_SIZE; ++y) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                PieceColor p = b.getPiece(x, y);
                const int idx = zobristColorIndex(p);
                if (idx >= 0) {
                    h ^= Z[encodePos(x, y)][idx];
                }
            }
        }
        return h;
    }



}

Game::Game(const Game& other)
    : black_captures(other.black_captures),
      white_captures(other.white_captures),
      consecutive_passes(other.consecutive_passes),
      board(std::make_unique<Board>(*other.board)),
      turn(other.turn),
      groups(other.groups),
      groupAt(other.groupAt),
      // history & future intentionally NOT copied (snapshots)
      m_enableHistory(false),
      m_koRefBoard(other.m_koRefBoard),
      m_hasKoRef(other.m_hasKoRef),
      m_boardHash(other.m_boardHash),
      m_koRefHash(other.m_koRefHash),
      m_deadMark(other.m_deadMark),
      m_lastCaptures(other.m_lastCaptures),
      m_lastInvalid(other.m_lastInvalid),
      m_lastSuicide(other.m_lastSuicide),
      m_lastKoViolation(other.m_lastKoViolation),
      m_lastKoThreat(other.m_lastKoThreat)
{
}

Game& Game::operator=(const Game& other) {
    if (this == &other) return *this;

    black_captures = other.black_captures;
    white_captures = other.white_captures;
    consecutive_passes = other.consecutive_passes;

    board = std::make_unique<Board>(*other.board);
    turn = other.turn;
    groups = other.groups;
    groupAt = other.groupAt;
    m_koRefBoard = other.m_koRefBoard;
    m_hasKoRef = other.m_hasKoRef;
    m_boardHash = other.m_boardHash;
    m_koRefHash = other.m_koRefHash;
    m_deadMark = other.m_deadMark;

    // IMPORTANT: keep this->m_enableHistory unchanged (main game must keep recording)
    // history/future are not copied by design
    m_lastCaptures = other.m_lastCaptures;
    m_lastInvalid = other.m_lastInvalid;
    m_lastSuicide = other.m_lastSuicide;
    m_lastKoViolation = other.m_lastKoViolation;
    m_lastKoThreat = other.m_lastKoThreat;
    return *this;
}

Game::Game(Board* b)
    : board(b), turn(BLACK)
{
    // Ensure clean initial state (important for load/replay)
    black_captures = 0;
    white_captures = 0;
    consecutive_passes = 0;

    history.clear();
    future.clear();

    groupAt.fill(-1);
    m_deadMark.fill(0);
    m_hasKoRef = false;
    m_koRefBoard = *board;
    m_boardHash = computeBoardHash(*board);
    m_koRefHash = computeBoardHash(m_koRefBoard);
    rebuildGroupsFromBoard(); // an toàn nếu board không rỗng

    // Initialize timeline (used by save/load + undo/redo without huge snapshots)
    auto& tl = ensureTimeline(this);
    tl.record = true;
    tl.moves.clear();
    tl.cursor = 0;

    tl.base.board = *board;
    tl.base.turn = turn;
    tl.base.black_captures = black_captures;
    tl.base.white_captures = white_captures;
    tl.base.consecutive_passes = consecutive_passes;
    tl.base.koRefBoard = m_koRefBoard;
    tl.base.hasKoRef = m_hasKoRef;
    tl.base.deadMark.fill(0);
}

PieceColor Game::oppositeColor(PieceColor input) const {
    if (input == BLACK) return WHITE;
    if (input == WHITE) return BLACK;
    return NONE;
}

bool Game::valid(int x, int y) const {
    return inBounds(x, y) && board->getPiece(x, y) == NONE;
}

bool Game::checkKO() const {
    // Simple ko (positional, 1-step): disallow a move that recreates the board position
    // from two plies ago (i.e., the board position before the last move).
    if (!m_hasKoRef) return false;

    // Fast path (O(1)) using Zobrist hash.
    if (m_boardHash != m_koRefHash) return false;

    // Collision guard (extremely unlikely): verify full board only when hashes match.
    return board->isEqual(m_koRefBoard);
}

void Game::clearDeadMarks() {
    m_deadMark.fill(0);
}

bool Game::isDeadAt(int x, int y) const {
    if (!inBounds(x, y)) return false;
    return m_deadMark[encodePos(x, y)] != 0;
}

bool Game::toggleDeadGroupAt(int x, int y) {
    if (!inBounds(x, y)) return false;
    PieceColor p = board->getPiece(x, y);
    if (p == NONE) return false;

    const int id = encodePos(x, y);
    const int gidx = groupAt[id];
    if (gidx < 0 || gidx >= (int)groups.size()) return false;

    const auto& locs = groups[gidx].getLocations();
    const std::uint8_t newVal = m_deadMark[id] ? 0 : 1;
    for (int sid : locs) {
        m_deadMark[sid] = newVal;
    }
    return true;
}


int Game::calcLiberties(int groupIdx) const {
    if (groupIdx < 0 || groupIdx >= (int)groups.size()) return 0;

    const auto& locs = groups[groupIdx].getLocations();
    const int tok = g_seenLib.next();

    int liberties = 0;
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};

    for (int id : locs) {
        int x = decodeX(id);
        int y = decodeY(id);
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (!inBounds(nx, ny)) continue;
            if (board->getPiece(nx, ny) != NONE) continue;

            int lid = encodePos(nx, ny);
            if (g_seenLib.mark[lid] != tok) {
                g_seenLib.mark[lid] = tok;
                ++liberties;
            }
        }
    }
    return liberties;
}

int Game::removeGroupByIndex(int groupIdx) {
    if (groupIdx < 0 || groupIdx >= (int)groups.size()) return 0;

    auto& g = groups[groupIdx];
    const auto& locs = g.getLocations();

    const int zidx = zobristColorIndex(g.getColor());
    const auto& Z = zobristTable();

    int removed = 0;
    for (int id : locs) {
        if (zidx >= 0) m_boardHash ^= Z[id][zidx];
        int x = decodeX(id);
        int y = decodeY(id);
        board->removePiece(x, y);
        groupAt[id] = -1;
        ++removed;
    }

    // swap-remove để giữ O(1)
    const int last = (int)groups.size() - 1;
    if (groupIdx != last) {
        groups[groupIdx] = std::move(groups[last]);
        // cập nhật mapping cho group bị swap vào
        for (int id : groups[groupIdx].getLocations()) {
            groupAt[id] = groupIdx;
        }
    }
    groups.pop_back();

    return removed;
}

// Merge all same-color groups adjacent to (x,y). Return group index containing (x,y).
int Game::processGroups(int x, int y, PieceColor c) {
    const int placedId = encodePos(x, y);

    // Find one adjacent same-color group as base (if any)
    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};

    int baseIdx = -1;
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        if (!inBounds(nx, ny)) continue;
        if (board->getPiece(nx, ny) != c) continue;
        int gi = groupAt[encodePos(nx, ny)];
        if (gi != -1) { baseIdx = gi; break; }
    }

    if (baseIdx == -1) {
        groups.emplace_back(x, y, c);
        baseIdx = (int)groups.size() - 1;
        groupAt[placedId] = baseIdx;
        return baseIdx;
    }

    // add stone to base group
    groups[baseIdx].addEncodedUnchecked(placedId);
    groupAt[placedId] = baseIdx;

    // merge other adjacent same-color groups into base
    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        if (!inBounds(nx, ny)) continue;
        if (board->getPiece(nx, ny) != c) continue;

        int gi = groupAt[encodePos(nx, ny)];
        if (gi == -1 || gi == baseIdx) continue;

        // append stones
        auto& A = groups[baseIdx].getLocationsRef();
        const auto& B = groups[gi].getLocations();
        A.reserve(A.size() + B.size());
        A.insert(A.end(), B.begin(), B.end());

        for (int id : B) groupAt[id] = baseIdx;

        // remove gi (swap-remove). baseIdx may move if it was at last.
        const int last = (int)groups.size() - 1;
        if (gi != last) {
            if (baseIdx == last) baseIdx = gi; // base group moved into gi
            groups[gi] = std::move(groups[last]);
            for (int id : groups[gi].getLocations()) groupAt[id] = gi;
        }
        groups.pop_back();
    }

    return baseIdx;
}

int Game::checkCapturesAround(int x, int y, PieceColor placedColor) {
    const PieceColor opp = oppositeColor(placedColor);
    int total = 0;

    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};

    for (int k = 0; k < 4; ++k) {
        int nx = x + dx[k];
        int ny = y + dy[k];
        if (!inBounds(nx, ny)) continue;
        if (board->getPiece(nx, ny) != opp) continue;

        int gi = groupAt[encodePos(nx, ny)];
        if (gi == -1) continue;

        if (calcLiberties(gi) == 0) {
            total += removeGroupByIndex(gi);
        }
    }
    return total;
}

bool Game::placeStone(int x, int y) {
    // reset last-move info
    m_lastCaptures    = 0;
    m_lastInvalid     = false;
    m_lastSuicide     = false;
    m_lastKoViolation = false;
    m_lastKoThreat    = false;

    if (!valid(x, y)) {
        m_lastInvalid = true;
        return false;
    }

    const PieceColor current_color = turn;

    // backups for rollback
    const Board board_backup = *board;
    const Board koRef_backup = m_koRefBoard;
    const bool  hasKoRef_backup = m_hasKoRef;
    const std::uint64_t boardHash_backup = m_boardHash;
    const std::uint64_t koRefHash_backup = m_koRefHash;
    const auto  deadMark_backup = m_deadMark;
    const auto groups_backup = groups;
    const auto groupAt_backup = groupAt;
    const int black_backup = black_captures;
    const int white_backup = white_captures;
    const int passes_backup = consecutive_passes;

    // do move
    board->setPiece(x, y, current_color);
    {
        const int zidx = zobristColorIndex(current_color);
        if (zidx >= 0) m_boardHash ^= zobristTable()[encodePos(x, y)][zidx];
    }
    int placed_group = processGroups(x, y, current_color);
    int captures = checkCapturesAround(x, y, current_color);

    const bool is_suicide = (captures == 0 && calcLiberties(placed_group) == 0);
    const bool is_ko_violation = checkKO();

    if (is_suicide || is_ko_violation) {
        // rollback
        *board = board_backup;
        groups = groups_backup;
        groupAt = groupAt_backup;
        black_captures = black_backup;
        white_captures = white_backup;
        consecutive_passes = passes_backup;
        m_koRefBoard = koRef_backup;
        m_hasKoRef = hasKoRef_backup;
        m_boardHash = boardHash_backup;
        m_koRefHash = koRefHash_backup;
        m_deadMark = deadMark_backup;

        if (is_suicide) m_lastSuicide = true;
        if (is_ko_violation) m_lastKoViolation = true;
        return false;
    }

    // update captures
    if (current_color == BLACK) black_captures += captures;
    else if (current_color == WHITE) white_captures += captures;

    // record info for UI
    m_lastCaptures = captures;
    // UI hint: detect a real simple-ko (opponent immediate recapture is forbidden due to ko)
    m_lastKoThreat = false;
    if (captures == 1) {
        const PieceColor opp = oppositeColor(current_color);

        int cx = -1, cy = -1;
        for (int yy = 0; yy < BOARD_SIZE && cx == -1; ++yy) {
            for (int xx = 0; xx < BOARD_SIZE; ++xx) {
                if (xx == x && yy == y) continue;
                if (board_backup.getPiece(xx, yy) == opp && board->getPiece(xx, yy) == NONE) {
                    cx = xx;
                    cy = yy;
                    break;
                }
            }
        }

        if (cx != -1) {
            Game tmp(*this); // copy state after this move (copy ctor disables history)
            tmp.turn = opp;
            tmp.m_koRefBoard = board_backup;
            tmp.m_hasKoRef = true;
            tmp.m_koRefHash = boardHash_backup;

            const bool ok = tmp.placeStone(cx, cy);
            m_lastKoThreat = (!ok && tmp.m_lastKoViolation);
        }
    }

    // record history only for main game instance
    if (m_enableHistory) {
        if (auto* tl = findTimeline(this); tl && tl->record) {
            // If we undid some moves, drop the redo tail before recording a new move
            if (tl->cursor < tl->moves.size()) tl->moves.resize(tl->cursor);
            tl->moves.push_back(MoveRec{current_color, x, y, false});
            tl->cursor = tl->moves.size();
        }
    }

    // update ko reference for the next player (board position before this move)
    m_koRefBoard = board_backup;
    m_hasKoRef = true;
    m_koRefHash = boardHash_backup;

    // next turn
    turn = oppositeColor(turn);
    consecutive_passes = 0;
    return true;
}

bool Game::pass() {
    // reset last-move info
    m_lastCaptures    = 0;
    m_lastInvalid     = false;
    m_lastSuicide     = false;
    m_lastKoViolation = false;
    m_lastKoThreat    = false;

    if (m_enableHistory) {
        if (auto* tl = findTimeline(this); tl && tl->record) {
            if (tl->cursor < tl->moves.size()) tl->moves.resize(tl->cursor);
            tl->moves.push_back(MoveRec{turn, -1, -1, true});
            tl->cursor = tl->moves.size();
        }
    }

    // A pass counts as a move for ko purposes under simple-ko:
    // it advances the "two plies ago" reference.
    m_koRefBoard = *board;
    m_hasKoRef = true;
    m_koRefHash = m_boardHash;

    ++consecutive_passes;
    if (consecutive_passes >= 2) {
        return true; // game ended
    }

    turn = oppositeColor(turn);
    return false;
}

bool Game::undo() {
    auto* tl = findTimeline(this);
    if (!tl) return false;
    if (tl->cursor == 0) return false;

    --tl->cursor;

    const bool oldRecord = tl->record;
    tl->record = false;

    // Reset to base state, then replay moves up to the cursor.
    *board = tl->base.board;
    turn = tl->base.turn;
    black_captures = tl->base.black_captures;
    white_captures = tl->base.white_captures;
    consecutive_passes = tl->base.consecutive_passes;
    m_deadMark = tl->base.deadMark;
    m_koRefBoard = tl->base.koRefBoard;
    m_hasKoRef = tl->base.hasKoRef;
    m_boardHash = computeBoardHash(*board);
    m_koRefHash = computeBoardHash(m_koRefBoard);

    // We no longer use snapshot history/future (kept for UI compatibility).
    history.clear();
    future.clear();

    groups.clear();
    groupAt.fill(-1);
    rebuildGroupsFromBoard();

    // reset last-move info
    m_lastCaptures    = 0;
    m_lastInvalid     = false;
    m_lastSuicide     = false;
    m_lastKoViolation = false;
    m_lastKoThreat    = false;

    for (std::size_t i = 0; i < tl->cursor; ++i) {
        const auto& mv = tl->moves[i];
        turn = mv.color;
        if (mv.isPass) {
            (void)pass();
        } else {
            if (!placeStone(mv.x, mv.y)) {
                tl->record = oldRecord;
                return false;
            }
        }
    }

    tl->record = oldRecord;
    return true;
}

bool Game::redo() {
    auto* tl = findTimeline(this);
    if (!tl) return false;
    if (tl->cursor >= tl->moves.size()) return false;

    ++tl->cursor;

    const bool oldRecord = tl->record;
    tl->record = false;

    // Reset to base state, then replay moves up to the cursor.
    *board = tl->base.board;
    turn = tl->base.turn;
    black_captures = tl->base.black_captures;
    white_captures = tl->base.white_captures;
    consecutive_passes = tl->base.consecutive_passes;
    m_deadMark = tl->base.deadMark;
    m_koRefBoard = tl->base.koRefBoard;
    m_hasKoRef = tl->base.hasKoRef;
    m_boardHash = computeBoardHash(*board);
    m_koRefHash = computeBoardHash(m_koRefBoard);

    history.clear();
    future.clear();

    groups.clear();
    groupAt.fill(-1);
    rebuildGroupsFromBoard();

    // reset last-move info
    m_lastCaptures    = 0;
    m_lastInvalid     = false;
    m_lastSuicide     = false;
    m_lastKoViolation = false;
    m_lastKoThreat    = false;

    for (std::size_t i = 0; i < tl->cursor; ++i) {
        const auto& mv = tl->moves[i];
        turn = mv.color;
        if (mv.isPass) {
            (void)pass();
        } else {
            if (!placeStone(mv.x, mv.y)) {
                tl->record = oldRecord;
                return false;
            }
        }
    }

    tl->record = oldRecord;
    return true;
}

PieceColor Game::getTerritoryOwner(int startX, int startY, int& territory_size, std::vector<std::uint8_t>& visited) const {
    std::queue<int> q;
    const int startId = encodePos(startX, startY);
    q.push(startId);
    visited[startId] = 1;

    territory_size = 0;
    bool touchesBlack = false;
    bool touchesWhite = false;

    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};

    while (!q.empty()) {
        int id = q.front();
        q.pop();

        int x = decodeX(id);
        int y = decodeY(id);
        ++territory_size;

        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k];
            int ny = y + dy[k];
            if (!inBounds(nx, ny)) continue;

            PieceColor c = board->getPiece(nx, ny);
            if (c == NONE) {
                int nid = encodePos(nx, ny);
                if (!visited[nid]) {
                    visited[nid] = 1;
                    q.push(nid);
                }
            } else if (c == BLACK) {
                touchesBlack = true;
            } else if (c == WHITE) {
                touchesWhite = true;
            }
        }
    }

    if (touchesBlack && !touchesWhite) return BLACK;
    if (touchesWhite && !touchesBlack) return WHITE;
    return NONE;
}

std::pair<float, float> Game::calculateFinalScore(float komi) const {
    // Apply optional dead-stone removal on a temporary board for scoring.
    Board scoringBoard = *board;
    int black_caps = black_captures;
    int white_caps = white_captures;

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            const int id = encodePos(x, y);
            if (!m_deadMark[id]) continue;

            PieceColor p = scoringBoard.getPiece(x, y);
            if (p == BLACK) {
                ++white_caps;
                scoringBoard.setPiece(x, y, NONE);
            } else if (p == WHITE) {
                ++black_caps;
                scoringBoard.setPiece(x, y, NONE);
            }
        }
    }

    std::vector<std::uint8_t> visited(BOARD_CELLS, 0);

    auto getOwner = [&](int startX, int startY, int& territory_size) -> PieceColor {
        std::queue<int> q;
        const int startId = encodePos(startX, startY);
        q.push(startId);
        visited[startId] = 1;

        territory_size = 0;
        bool touchesBlack = false;
        bool touchesWhite = false;

        static const int dx[4] = {-1, 1, 0, 0};
        static const int dy[4] = {0, 0, -1, 1};

        while (!q.empty()) {
            int id = q.front();
            q.pop();

            const int x = decodeX(id);
            const int y = decodeY(id);
            ++territory_size;

            for (int k = 0; k < 4; ++k) {
                const int nx = x + dx[k];
                const int ny = y + dy[k];
                if (!inBounds(nx, ny)) continue;

                PieceColor p = scoringBoard.getPiece(nx, ny);
                if (p == NONE) {
                    const int nid = encodePos(nx, ny);
                    if (!visited[nid]) {
                        visited[nid] = 1;
                        q.push(nid);
                    }
                } else if (p == BLACK) {
                    touchesBlack = true;
                } else if (p == WHITE) {
                    touchesWhite = true;
                }
            }
        }

        if (touchesBlack && !touchesWhite) return BLACK;
        if (touchesWhite && !touchesBlack) return WHITE;
        return NONE;
    };

    int black_territory = 0;
    int white_territory = 0;

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (scoringBoard.getPiece(x, y) != NONE) continue;

            const int id = encodePos(x, y);
            if (visited[id]) continue;

            int territory_size = 0;
            PieceColor owner = getOwner(x, y, territory_size);

            if (owner == BLACK) black_territory += territory_size;
            else if (owner == WHITE) white_territory += territory_size;
        }
    }

    float black_score = static_cast<float>(black_territory + black_caps);
    float white_score = static_cast<float>(white_territory + white_caps) + komi;
    return {black_score, white_score};
}



void Game::printDebug() const {
    std::cout << "--- GAME STATE ---\n";
    std::cout << "Turn: " << ((turn == BLACK) ? "BLACK" : "WHITE") << "\n";
    std::cout << "Groups: " << groups.size() << "\n";
    std::cout << "Captures B/W: " << black_captures << " / " << white_captures << "\n";
    board->printDebug();
}

bool Game::saveToFile(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    // V2 format: base state + move list + cursor (for undo/redo)
    out << "GO_SAVE_V2\n";
    out << BOARD_SIZE << '\n';

    const Timeline* tl = findTimeline(this);

    // If timeline is missing (shouldn't happen for the main game), fall back to current state.
    BaseState base;
    std::size_t cursor = 0;
    std::size_t nMoves = 0;

    if (tl) {
        base = tl->base;
        cursor = tl->cursor;
        nMoves = tl->moves.size();
    } else {
        base.board = *board;
        base.turn = turn;
        base.black_captures = black_captures;
        base.white_captures = white_captures;
        base.consecutive_passes = consecutive_passes;
        base.koRefBoard = *board;
        base.hasKoRef = false;
        base.deadMark.fill(0);
    }

    out << static_cast<int>(base.turn) << ' '
        << base.black_captures << ' '
        << base.white_captures << ' '
        << base.consecutive_passes << '\n';

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            PieceColor p = base.board.getPiece(x, y);
            char c = '.';
            if (p == BLACK) c = 'B';
            else if (p == WHITE) c = 'W';
            out << c;
        }
        out << '\n';
    }

    out << cursor << ' ' << nMoves << '\n';

    if (tl) {
        for (const auto& mv : tl->moves) {
            const char cc = (mv.color == BLACK) ? 'B' : 'W';
            if (mv.isPass) {
                out << cc << " P\n";
            } else {
                out << cc << ' ' << mv.x << ' ' << mv.y << '\n';
            }
        }
    }

    return true;
}

bool Game::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;

    std::string head;
    if (!(in >> head)) return false;

    // ---------------------------
    // New compact format (V2)
    // ---------------------------
    if (head == "GO_SAVE_V2") {
        int boardSize = 0;
        if (!(in >> boardSize) || boardSize != BOARD_SIZE) return false;

        int baseTurnInt = 0;
        int baseBlack = 0, baseWhite = 0, basePass = 0;
        if (!(in >> baseTurnInt >> baseBlack >> baseWhite >> basePass)) return false;

        std::string line;
        std::getline(in, line); // consume newline

        Board baseBoard;
        baseBoard.clear();
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (!std::getline(in, line)) return false;
            if (static_cast<int>(line.size()) < BOARD_SIZE) return false;
            for (int x = 0; x < BOARD_SIZE; ++x) {
                char c = line[x];
                if (c == 'B') baseBoard.setPiece(x, y, BLACK);
                else if (c == 'W') baseBoard.setPiece(x, y, WHITE);
                else baseBoard.setPiece(x, y, NONE);
            }
        }

        std::size_t cursor = 0, nMoves = 0;
        if (!(in >> cursor >> nMoves)) return false;
        if (cursor > nMoves) return false;

        std::vector<MoveRec> moves;
        moves.reserve(nMoves);

        for (std::size_t i = 0; i < nMoves; ++i) {
            char colChar = 0;
            if (!(in >> colChar)) return false;
            PieceColor col = (colChar == 'B') ? BLACK : WHITE;

            std::string tok;
            if (!(in >> tok)) return false;

            if (tok == "P") {
                moves.push_back(MoveRec{col, -1, -1, true});
            } else {
                int x = 0;
                int y = 0;
                try { x = std::stoi(tok); } catch (...) { return false; }
                if (!(in >> y)) return false;
                moves.push_back(MoveRec{col, x, y, false});
            }
        }

        auto& tl = ensureTimeline(this);
        tl.record = true;
        tl.moves = std::move(moves);
        tl.cursor = cursor;

        tl.base.board = baseBoard;
        tl.base.turn = static_cast<PieceColor>(baseTurnInt);
        tl.base.black_captures = baseBlack;
        tl.base.white_captures = baseWhite;
        tl.base.consecutive_passes = basePass;
        tl.base.deadMark.fill(0);
        tl.base.koRefBoard = baseBoard;
        tl.base.hasKoRef = false;

        // Replay to cursor (without recording)
        const bool oldRecord = tl.record;
        tl.record = false;

        *board = tl.base.board;
        turn = tl.base.turn;
        black_captures = tl.base.black_captures;
        white_captures = tl.base.white_captures;
        consecutive_passes = tl.base.consecutive_passes;
        m_deadMark = tl.base.deadMark;
        m_koRefBoard = tl.base.koRefBoard;
        m_hasKoRef = tl.base.hasKoRef;
        m_boardHash = computeBoardHash(*board);
        m_koRefHash = computeBoardHash(m_koRefBoard);

        history.clear();
        future.clear();
        groups.clear();
        groupAt.fill(-1);
        rebuildGroupsFromBoard();

        // reset last-move info
        m_lastCaptures    = 0;
        m_lastInvalid     = false;
        m_lastSuicide     = false;
        m_lastKoViolation = false;
        m_lastKoThreat    = false;

        for (std::size_t i = 0; i < tl.cursor; ++i) {
            const auto& mv = tl.moves[i];
            turn = mv.color;
            if (mv.isPass) {
                (void)pass();
            } else {
                if (!placeStone(mv.x, mv.y)) {
                    tl.record = oldRecord;
                    return false;
                }
            }
        }

        tl.record = oldRecord;
        return true;
    }

    // ---------------------------
    // Legacy format (V1): load current board only and ignore snapshot history/future.
    // This keeps the file compatible, while avoiding huge memory usage.
    // ---------------------------
    if (!isUnsignedIntToken(head)) return false;

    int boardSize = 0;
    try { boardSize = std::stoi(head); } catch (...) { return false; }
    if (boardSize != BOARD_SIZE) return false;

    int turnInt = 0;
    if (!(in >> turnInt >> black_captures >> white_captures >> consecutive_passes)) return false;
    turn = static_cast<PieceColor>(turnInt);

    std::string line;
    std::getline(in, line); // consume newline

    board->clear();
    for (int y = 0; y < BOARD_SIZE; ++y) {
        if (!std::getline(in, line)) return false;
        if (static_cast<int>(line.size()) < BOARD_SIZE) return false;
        for (int x = 0; x < BOARD_SIZE; ++x) {
            char c = line[x];
            if (c == 'B') board->setPiece(x, y, BLACK);
            else if (c == 'W') board->setPiece(x, y, WHITE);
            else board->setPiece(x, y, NONE);
        }
    }

    history.clear();
    future.clear();
    m_deadMark.fill(0);
    m_hasKoRef = false;
    m_koRefBoard = *board;
    m_boardHash = computeBoardHash(*board);
    m_koRefHash = computeBoardHash(m_koRefBoard);
    rebuildGroupsFromBoard();

    // Reset timeline base to the loaded state (no undo/redo history for legacy files).
    {
        auto& tl = ensureTimeline(this);
        tl.record = true;
        tl.moves.clear();
        tl.cursor = 0;
        tl.base.board = *board;
        tl.base.turn = turn;
        tl.base.black_captures = black_captures;
        tl.base.white_captures = white_captures;
        tl.base.consecutive_passes = consecutive_passes;
        tl.base.deadMark.fill(0);
        tl.base.koRefBoard = *board;
        tl.base.hasKoRef = false;
    }

    return true;
}

bool Game::saveNamed(const std::string& name) const {
    std::error_code ec;
    fs::path dir(Game::SAVE_DIR);
    fs::create_directories(dir, ec);

    fs::path path = dir / name;
    if (!path.has_extension()) path.replace_extension(".go");

    return saveToFile(path.string());
}

bool Game::loadNamed(const std::string& name) {
    fs::path path(Game::SAVE_DIR);
    path /= name;
    if (!path.has_extension()) path.replace_extension(".go");
    return loadFromFile(path.string());
}

bool Game::saveToNewSlot(std::string& outFilename) const {
    std::error_code ec;
    fs::path dir(Game::SAVE_DIR);
    fs::create_directories(dir, ec);

    int maxIndex = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string stem = entry.path().stem().string();
        if (stem.rfind("save_", 0) == 0) {
            try {
                int idx = std::stoi(stem.substr(5));
                maxIndex = std::max(maxIndex, idx);
            } catch (...) {}
        }
    }

    int newIndex = maxIndex + 1;
    std::ostringstream oss;
    oss << "save_" << std::setfill('0') << std::setw(3) << newIndex;

    fs::path path = dir / (oss.str() + ".go");
    outFilename = path.filename().string();

    if (!saveToFile(path.string())) { outFilename.clear(); return false; }
    return true;
}

std::vector<std::string> Game::listSaveFiles() {
    std::vector<std::string> result;
    std::error_code ec;
    fs::path dir(Game::SAVE_DIR);

    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return result;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".go" || ext == ".txt" || ext == ".sav") {
            result.push_back(entry.path().filename().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void Game::rebuildGroupsFromBoard() {
    groups.clear();
    groupAt.fill(-1);

    // BFS build groups in O(N)
    std::queue<int> q;
    const int tok = g_seenStone.next();

    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (board->getPiece(x, y) == NONE) continue;

            int id = encodePos(x, y);
            if (g_seenStone.mark[id] == tok) continue;

            const PieceColor c = board->getPiece(x, y);

            PieceGroup g(c);
            g.reserve(32);

            g_seenStone.mark[id] = tok;
            q.push(id);

            while (!q.empty()) {
                int cur = q.front(); q.pop();
                g.addEncodedUnchecked(cur);

                int cx = decodeX(cur);
                int cy = decodeY(cur);

                static const int dx[4] = {-1, 1, 0, 0};
                static const int dy[4] = {0, 0, -1, 1};

                for (int k = 0; k < 4; ++k) {
                    int nx = cx + dx[k];
                    int ny = cy + dy[k];
                    if (!inBounds(nx, ny)) continue;
                    if (board->getPiece(nx, ny) != c) continue;

                    int nid = encodePos(nx, ny);
                    if (g_seenStone.mark[nid] == tok) continue;
                    g_seenStone.mark[nid] = tok;
                    q.push(nid);
                }
            }

            groups.push_back(std::move(g));
            int gi = (int)groups.size() - 1;
            for (int sid : groups[gi].getLocations()) groupAt[sid] = gi;
        }
    }
}