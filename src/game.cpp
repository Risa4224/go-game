#include "game.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
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
    groupAt.fill(-1);
    rebuildGroupsFromBoard(); // an toàn nếu board không rỗng
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
    // Simple ko: không được lặp lại vị trí của board trước đó (ngay trước lượt này).
    if (history.empty()) return false;
    return board->isEqual(*history.back().board);
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

    int removed = 0;
    for (int id : locs) {
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
    const auto groups_backup = groups;
    const auto groupAt_backup = groupAt;
    const int black_backup = black_captures;
    const int white_backup = white_captures;
    const int passes_backup = consecutive_passes;

    // do move
    board->setPiece(x, y, current_color);
    int placed_group = processGroups(x, y, current_color);
    int captures = checkCapturesAround(x, y, current_color);

    const bool is_suicide = (captures == 0 && calcLiberties(placed_group) == 0);
    const bool is_ko_violation = (m_enableHistory && !history.empty() && checkKO());

    if (is_suicide || is_ko_violation) {
        // rollback
        *board = board_backup;
        groups = groups_backup;
        groupAt = groupAt_backup;
        black_captures = black_backup;
        white_captures = white_backup;
        consecutive_passes = passes_backup;

        if (is_suicide) m_lastSuicide = true;
        if (is_ko_violation) m_lastKoViolation = true;
        return false;
    }

    // update captures
    if (current_color == BLACK) black_captures += captures;
    else if (current_color == WHITE) white_captures += captures;

    // record info for UI
    m_lastCaptures = captures;
    m_lastKoThreat = (captures == 1);

    // record history only for main game instance
    if (m_enableHistory) {
        // save PRE-move snapshot (board_backup, groups_backup, groupAt_backup, captures_backup, ...)
        Game snapshot(new Board(board_backup));
        snapshot.turn = current_color;
        snapshot.black_captures = black_backup;
        snapshot.white_captures = white_backup;
        snapshot.consecutive_passes = passes_backup;
        snapshot.groups = groups_backup;
        snapshot.groupAt = groupAt_backup;
        snapshot.history.clear();
        snapshot.future.clear();
        snapshot.m_enableHistory = false;

        history.push_back(std::move(snapshot));
        future.clear();
    }

    // next turn
    turn = oppositeColor(turn);
    consecutive_passes = 0;
    return true;
}

bool Game::pass() {
    if (m_enableHistory) {
        history.push_back(*this); // snapshot (copy ctor disables history)
        future.clear();
    }

    ++consecutive_passes;
    if (consecutive_passes >= 2) {
        return true; // game ended
    }

    turn = oppositeColor(turn);
    return false;
}

bool Game::undo() {
    if (history.empty()) return false;

    Game prevState = history.back();
    history.pop_back();

    future.push_back(*this);
    *this = prevState; // operator= keeps m_enableHistory of main game
    return true;
}

bool Game::redo() {
    if (future.empty()) return false;

    Game nextState = future.back();
    future.pop_back();

    history.push_back(*this);
    *this = nextState;
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
    std::vector<std::uint8_t> visited(BOARD_CELLS, 0);

    int black_territory = 0;
    int white_territory = 0;

    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (board->getPiece(x, y) != NONE) continue;

            int id = encodePos(x, y);
            if (visited[id]) continue;

            int territory_size = 0;
            PieceColor owner = getTerritoryOwner(x, y, territory_size, visited);

            if (owner == BLACK) black_territory += territory_size;
            else if (owner == WHITE) white_territory += territory_size;
        }
    }

    float black_score = static_cast<float>(black_territory + black_captures);
    float white_score = static_cast<float>(white_territory + white_captures) + komi;
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

    out << BOARD_SIZE << '\n';
    out << static_cast<int>(turn) << ' '
        << black_captures << ' '
        << white_captures << ' '
        << consecutive_passes << '\n';

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            PieceColor p = board->getPiece(x, y);
            char c = '.';
            if (p == BLACK) c = 'B';
            else if (p == WHITE) c = 'W';
            out << c;
        }
        out << '\n';
    }

    out << history.size() << '\n';
    for (const Game& st : history) {
        out << static_cast<int>(st.turn) << ' '
            << st.black_captures << ' '
            << st.white_captures << ' '
            << st.consecutive_passes << '\n';

        for (int y = 0; y < BOARD_SIZE; ++y) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                PieceColor p = st.board->getPiece(x, y);
                char c = '.';
                if (p == BLACK) c = 'B';
                else if (p == WHITE) c = 'W';
                out << c;
            }
            out << '\n';
        }
    }

    out << future.size() << '\n';
    for (const Game& st : future) {
        out << static_cast<int>(st.turn) << ' '
            << st.black_captures << ' '
            << st.white_captures << ' '
            << st.consecutive_passes << '\n';

        for (int y = 0; y < BOARD_SIZE; ++y) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                PieceColor p = st.board->getPiece(x, y);
                char c = '.';
                if (p == BLACK) c = 'B';
                else if (p == WHITE) c = 'W';
                out << c;
            }
            out << '\n';
        }
    }

    return true;
}

bool Game::loadFromFile(const std::string& filename) {
    std::ifstream in(filename);
    if (!in.is_open()) return false;

    int boardSize = 0;
    if (!(in >> boardSize) || boardSize != BOARD_SIZE) return false;

    int turnInt = 0;
    if (!(in >> turnInt >> black_captures >> white_captures >> consecutive_passes)) return false;
    turn = static_cast<PieceColor>(turnInt);

    std::string line;
    std::getline(in, line); // newline

    // read current board
    board->clear();
    for (int y = 0; y < BOARD_SIZE; ++y) {
        if (!std::getline(in, line) || (int)line.size() < BOARD_SIZE) return false;
        for (int x = 0; x < BOARD_SIZE; ++x) {
            char c = line[x];
            PieceColor p = NONE;
            if (c == 'B') p = BLACK;
            else if (c == 'W') p = WHITE;
            board->setPiece(x, y, p);
        }
    }

    rebuildGroupsFromBoard();

    std::size_t historySize = 0;
    if (!(in >> historySize)) { history.clear(); future.clear(); return true; }
    std::getline(in, line);

    history.clear();
    history.reserve(historySize);

    for (std::size_t i = 0; i < historySize; ++i) {
        int hTurnInt = 0, hBlack = 0, hWhite = 0, hPass = 0;
        if (!(in >> hTurnInt >> hBlack >> hWhite >> hPass)) return false;
        std::getline(in, line);

        Board* snapBoard = new Board();
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (!std::getline(in, line) || (int)line.size() < BOARD_SIZE) { delete snapBoard; return false; }
            for (int x = 0; x < BOARD_SIZE; ++x) {
                char c = line[x];
                PieceColor p = NONE;
                if (c == 'B') p = BLACK;
                else if (c == 'W') p = WHITE;
                snapBoard->setPiece(x, y, p);
            }
        }

        Game snapshot(snapBoard);
        snapshot.turn = static_cast<PieceColor>(hTurnInt);
        snapshot.black_captures = hBlack;
        snapshot.white_captures = hWhite;
        snapshot.consecutive_passes = hPass;
        snapshot.history.clear();
        snapshot.future.clear();
        snapshot.m_enableHistory = false;
        snapshot.rebuildGroupsFromBoard();

        history.push_back(std::move(snapshot));
    }

    std::size_t futureSize = 0;
    if (!(in >> futureSize)) { future.clear(); return true; }
    std::getline(in, line);

    future.clear();
    future.reserve(futureSize);

    for (std::size_t i = 0; i < futureSize; ++i) {
        int fTurnInt = 0, fBlack = 0, fWhite = 0, fPass = 0;
        if (!(in >> fTurnInt >> fBlack >> fWhite >> fPass)) return false;
        std::getline(in, line);

        Board* snapBoard = new Board();
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (!std::getline(in, line) || (int)line.size() < BOARD_SIZE) { delete snapBoard; return false; }
            for (int x = 0; x < BOARD_SIZE; ++x) {
                char c = line[x];
                PieceColor p = NONE;
                if (c == 'B') p = BLACK;
                else if (c == 'W') p = WHITE;
                snapBoard->setPiece(x, y, p);
            }
        }

        Game snapshot(snapBoard);
        snapshot.turn = static_cast<PieceColor>(fTurnInt);
        snapshot.black_captures = fBlack;
        snapshot.white_captures = fWhite;
        snapshot.consecutive_passes = fPass;
        snapshot.history.clear();
        snapshot.future.clear();
        snapshot.m_enableHistory = false;
        snapshot.rebuildGroupsFromBoard();

        future.push_back(std::move(snapshot));
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
