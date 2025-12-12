#include "AI.h"

#include <algorithm>
#include <random>
#include <chrono>
#include <limits>
#include <iostream>
#include <queue>
#include <vector>
#include <cstdint>

namespace {
    constexpr int BOARD_SIZE = 19;

    // RNG dùng chung cho toàn bộ AI.
    std::mt19937& globalRng()
    {
        static std::mt19937 rng(
            static_cast<unsigned long>(
                std::chrono::steady_clock::now().time_since_epoch().count()
            )
        );
        return rng;
    }

    inline int idx(int x, int y) { return y * BOARD_SIZE + x; }

    struct GroupInfo {
        PieceColor color = NONE;
        int stones = 0;
        int liberties = 0;
        bool atari = false;
    };

    // BFS lấy thông tin group (stones + liberties) bắt đầu từ (sx, sy).
    // Chỉ dùng game.getPiece (không cần truy cập groups bên trong Game).
    GroupInfo analyzeGroupFrom(const Game& game, int sx, int sy,
                              std::vector<char>& visited)
    {
        GroupInfo info;
        PieceColor c = game.getPiece(sx, sy);
        if (c == NONE) return info;

        info.color = c;

        std::queue<std::pair<int,int>> q;
        q.push({sx, sy});
        visited[idx(sx, sy)] = 1;

        // Dùng mảng đánh dấu liberty để tránh đếm trùng
        std::vector<char> seenLib(BOARD_SIZE * BOARD_SIZE, 0);

        static const int dx[4] = {-1, 1, 0, 0};
        static const int dy[4] = {0, 0, -1, 1};

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();
            info.stones++;

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx[dir];
                int ny = y + dy[dir];
                if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    int id = idx(nx, ny);
                    if (!seenLib[id]) {
                        seenLib[id] = 1;
                        info.liberties++;
                    }
                } else if (p == c) {
                    int id = idx(nx, ny);
                    if (!visited[id]) {
                        visited[id] = 1;
                        q.push({nx, ny});
                    }
                }
            }
        }

        info.atari = (info.liberties == 1);
        return info;
    }

    // Node budget để đảm bảo "reasonable time" cho Hard.
    struct SearchBudget {
        int remaining = 0;
    };

    thread_local SearchBudget g_budget;
}

// ---------------------------------------------------------------------
// Heuristic theo report (Future Improvements - AI):
//  - Liberty count: thưởng nhiều liberties, phạt ít liberties.
//  - Capture potential / Atari: group địch ở atari -> tốt, group mình ở atari -> xấu.
//  - Local tactical focus (không cố gắng estimate territory dài hạn).
// ---------------------------------------------------------------------
double GoAI::evaluateBoardHeuristic(const Game& game, PieceColor aiColor)
{
    PieceColor oppColor = game.oppositeColor(aiColor);

    // Quét group để tránh đếm liberties bị lặp theo từng stone.
    std::vector<char> visited(BOARD_SIZE * BOARD_SIZE, 0);

    int aiStones = 0, oppStones = 0;
    int aiLib = 0, oppLib = 0;
    int aiAtari = 0, oppAtari = 0;

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) == NONE) continue;
            if (visited[idx(x,y)]) continue;

            GroupInfo g = analyzeGroupFrom(game, x, y, visited);
            if (g.color == aiColor) {
                aiStones += g.stones;
                aiLib += g.liberties;
                if (g.atari) aiAtari++;
            } else if (g.color == oppColor) {
                oppStones += g.stones;
                oppLib += g.liberties;
                if (g.atari) oppAtari++;
            }
        }
    }

    // Trọng số: bạn có thể tinh chỉnh nhanh ở đây.
    const double W_STONE = 1.0;   // material
    const double W_LIB   = 0.35;  // stability
    const double W_ATARI = 2.5;   // tactical threats

    double stoneDiff = (aiStones - oppStones) * W_STONE;
    double libDiff   = (aiLib - oppLib) * W_LIB;
    double atariDiff = (oppAtari - aiAtari) * W_ATARI;

    return stoneDiff + libDiff + atariDiff;
}

// ---------------------------------------------------------------------
// Sinh nước đi ứng viên + move ordering (theo report):
//  - chỉ xét ô trống gần quân (branching factor thấp)
//  - ưu tiên capture ngay (địch ở atari và ô hiện tại kề group đó)
//  - ưu tiên cứu group mình đang ở atari
//  - ưu tiên ô có nhiều hàng xóm (khu vực đang giao tranh)
// ---------------------------------------------------------------------
std::vector<AIMove> GoAI::generateCandidateMoves(const Game& game)
{
    struct ScoredMove { AIMove m; int s; };
    std::vector<ScoredMove> scored;

    // Nếu bàn trống: đánh giữa
    bool anyStone = false;
    for (int y = 0; y < BOARD_SIZE && !anyStone; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) != NONE) { anyStone = true; break; }
        }
    }
    if (!anyStone) {
        int c = BOARD_SIZE / 2;
        return { AIMove(c, c, false) };
    }

    PieceColor aiColor = game.getTurn();
    PieceColor oppColor = game.oppositeColor(aiColor);

    static const int dx[4] = {-1, 1, 0, 0};
    static const int dy[4] = {0, 0, -1, 1};

    // Cache liberties theo group cho trạng thái hiện tại (tránh BFS lặp lại).
    std::vector<int> libCache(BOARD_SIZE * BOARD_SIZE, -1);

    auto groupLibertiesCached = [&](int sx, int sy) -> int {
        int id0 = idx(sx, sy);
        if (id0 < 0 || id0 >= (int)libCache.size()) return 0;
        if (libCache[id0] != -1) return libCache[id0];

        PieceColor c = game.getPiece(sx, sy);
        if (c == NONE) { libCache[id0] = 0; return 0; }

        std::queue<std::pair<int,int>> q;
        q.push({sx, sy});

        std::vector<char> visitedLocal(BOARD_SIZE * BOARD_SIZE, 0);
        visitedLocal[id0] = 1;

        std::vector<char> seenLib(BOARD_SIZE * BOARD_SIZE, 0);
        int liberties = 0;

        std::vector<int> stonesInGroup;
        stonesInGroup.push_back(id0);

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx[dir];
                int ny = y + dy[dir];
                if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    int lid = idx(nx, ny);
                    if (!seenLib[lid]) { seenLib[lid] = 1; liberties++; }
                } else if (p == c) {
                    int sid = idx(nx, ny);
                    if (!visitedLocal[sid]) {
                        visitedLocal[sid] = 1;
                        q.push({nx, ny});
                        stonesInGroup.push_back(sid);
                    }
                }
            }
        }

        // Gán liberties cho toàn bộ stone trong group để cache dùng lại
        for (int sid : stonesInGroup) {
            libCache[sid] = liberties;
        }
        return liberties;
    };

    auto hasNeighbourStone = [&](int x, int y) -> bool {
        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];
            if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;
            if (game.getPiece(nx, ny) != NONE) return true;
        }
        return false;
    };

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) != NONE) continue;
            if (!hasNeighbourStone(x, y)) continue;

            int score = 0;

            // Ưu tiên khu vực có nhiều tương tác
            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx[dir];
                int ny = y + dy[dir];
                if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) continue;

                score += 2; // gần quân => đáng xét hơn

                if (p == oppColor) {
                    // nếu group địch đang ở atari, và (x,y) kề nó => có khả năng là liberty cuối => bắt ngay
                    if (groupLibertiesCached(nx, ny) == 1) score += 30;
                } else if (p == aiColor) {
                    // cứu group mình ở atari
                    if (groupLibertiesCached(nx, ny) == 1) score += 18;
                    // nối group
                    score += 3;
                }
            }

            scored.push_back({ AIMove(x, y, false), score });
        }
    }

    // Fallback: nếu vì lý do nào đó không có nước (hiếm), thêm tất cả ô trống.
    if (scored.empty()) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                if (game.getPiece(x, y) == NONE) {
                    scored.push_back({ AIMove(x, y, false), 0 });
                }
            }
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredMove& a, const ScoredMove& b) { return a.s > b.s; });

    std::vector<AIMove> moves;
    moves.reserve(scored.size());
    for (auto& sm : scored) moves.push_back(sm.m);
    return moves;
}

// ---------------------------------------------------------------------
// Minimax (Medium) - shallow depth + có "noise" nhẹ để tạo sai sót hợp lý.
// ---------------------------------------------------------------------
double GoAI::minimax(Game game,
                     int depth,
                     int maxDepth,
                     bool maximizingPlayer,
                     PieceColor aiColor)
{
    if (g_budget.remaining <= 0) {
        return evaluateBoardHeuristic(game, aiColor);
    }
    g_budget.remaining--;

    if (depth >= maxDepth) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    std::vector<AIMove> moves = generateCandidateMoves(game);
    if (moves.empty()) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    double bestValue = maximizingPlayer
        ? -std::numeric_limits<double>::infinity()
        :  std::numeric_limits<double>::infinity();

    bool anyValidChild = false;

    if (maximizingPlayer) {
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimax(child, depth + 1, maxDepth, false, aiColor);
            if (val > bestValue) bestValue = val;
        }
    } else {
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimax(child, depth + 1, maxDepth, true, aiColor);
            if (val < bestValue) bestValue = val;
        }
    }

    if (!anyValidChild) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    return bestValue;
}

// ---------------------------------------------------------------------
// Minimax + Alpha–Beta (Hard) + node budget + move ordering.
// ---------------------------------------------------------------------
double GoAI::minimaxAlphaBeta(Game game,
                              int depth,
                              int maxDepth,
                              double alpha,
                              double beta,
                              bool maximizingPlayer,
                              PieceColor aiColor)
{
    if (g_budget.remaining <= 0) {
        return evaluateBoardHeuristic(game, aiColor);
    }
    g_budget.remaining--;

    if (depth >= maxDepth) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    std::vector<AIMove> moves = generateCandidateMoves(game);
    if (moves.empty()) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    double bestValue = maximizingPlayer
        ? -std::numeric_limits<double>::infinity()
        :  std::numeric_limits<double>::infinity();

    bool anyValidChild = false;

    if (maximizingPlayer) {
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, false, aiColor);

            if (val > bestValue) bestValue = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }
    } else {
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, true, aiColor);

            if (val < bestValue) bestValue = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }
    }

    if (!anyValidChild) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    return bestValue;
}

// ---------------------------------------------------------------------
// Tính nước đi tốt nhất nhưng KHÔNG đặt quân vào game thật.
// ---------------------------------------------------------------------
AIMove GoAI::computeAIMove(const Game& game, AIDifficulty difficulty)
{
    const PieceColor aiColor = game.getTurn();

    std::vector<AIMove> candidates = generateCandidateMoves(game);
    if (candidates.empty()) return AIMove(-1, -1, true);

    // EASY: random hợp lệ
    if (difficulty == AIDifficulty::EASY) {
        std::mt19937& rng = globalRng();
        std::shuffle(candidates.begin(), candidates.end(), rng);

        for (const AIMove& m : candidates) {
            Game test = game;
            if (test.placeStone(m.x, m.y)) return AIMove(m.x, m.y, false);
        }
        return AIMove(-1, -1, true);
    }

    // MEDIUM / HARD: depth + budget
    int maxDepth = (difficulty == AIDifficulty::MEDIUM) ? 2 : 3;

    // Budget: Hard cho nhiều node hơn.
    g_budget.remaining = (difficulty == AIDifficulty::MEDIUM) ? 2500 : 12000;

    const bool useAlphaBeta = (difficulty == AIDifficulty::HARD);

    struct RootChoice {
        AIMove move;
        double score;
    };
    std::vector<RootChoice> root;

    for (const AIMove& m : candidates) {
        Game child = game;
        if (!child.placeStone(m.x, m.y)) continue;

        double score;
        if (useAlphaBeta) {
            score = minimaxAlphaBeta(child,
                                     /*depth=*/1,
                                     maxDepth,
                                     -std::numeric_limits<double>::infinity(),
                                     +std::numeric_limits<double>::infinity(),
                                     /*maximizingPlayer=*/false,
                                     aiColor);
        } else {
            score = minimax(child,
                            /*depth=*/1,
                            maxDepth,
                            /*maximizingPlayer=*/false,
                            aiColor);

            // Medium: thêm noise nhỏ để "có sai sót"
            std::uniform_real_distribution<double> noise(-0.6, 0.6);
            score += noise(globalRng());
        }

        root.push_back({ AIMove(m.x, m.y, false), score });
    }

    if (root.empty()) return AIMove(-1, -1, true);

    std::sort(root.begin(), root.end(), [](const RootChoice& a, const RootChoice& b) {
        return a.score > b.score;
    });

    if (difficulty == AIDifficulty::MEDIUM) {
        // Medium: pick ngẫu nhiên trong top 2-3 (nếu có) để "không hoàn hảo"
        int k = std::min<int>(3, (int)root.size());
        std::uniform_int_distribution<int> pick(0, k - 1);
        return root[pick(globalRng())].move;
    }

    // Hard: luôn pick tốt nhất
    return root.front().move;
}

// ---------------------------------------------------------------------
// Gọi AI và đặt quân luôn lên game thật.
// ---------------------------------------------------------------------
bool GoAI::playAIMove(Game& game, AIDifficulty difficulty)
{
    AIMove move = computeAIMove(game, difficulty);

    if (move.isPass) {
        std::cout << "AI chooses to PASS.\n";
        game.pass();
        return false;
    }

    if (game.placeStone(move.x, move.y)) {
        std::cout << "AI plays at (" << move.x << ", " << move.y << ")\n";
        return true;
    }

    // fallback nếu nước chọn bị KO/history làm invalid trên game thật
    std::cout << "AI's chosen move is invalid on real game. Trying fallback...\n";

    std::vector<AIMove> fallback = generateCandidateMoves(game);
    std::shuffle(fallback.begin(), fallback.end(), globalRng());

    for (const AIMove& m : fallback) {
        if (game.placeStone(m.x, m.y)) {
            std::cout << "AI fallback move at (" << m.x << ", " << m.y << ")\n";
            return true;
        }
    }

    std::cout << "AI cannot find any legal move, PASS.\n";
    game.pass();
    return false;
}
