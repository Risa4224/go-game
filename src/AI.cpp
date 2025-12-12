#include "AI.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <vector>
#include <utility>

namespace {
    constexpr int BOARD_SIZE = 19;

    inline int idx(int x, int y) { return y * BOARD_SIZE + x; }
    inline bool inBounds(int x, int y) { return (0 <= x && x < BOARD_SIZE && 0 <= y && y < BOARD_SIZE); }

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

    struct GroupInfo {
        PieceColor color = NONE;
        int stones = 0;
        int liberties = 0;
        bool atari = false;
    };

    struct StampBuf {
        std::vector<int> mark;
        int token = 1;

        void ensureSize(std::size_t n) {
            if (mark.size() != n) mark.assign(n, 0);
        }

        int nextToken() {
            if (token >= std::numeric_limits<int>::max() - 2) {
                std::fill(mark.begin(), mark.end(), 0);
                token = 1;
            }
            return ++token;
        }

        bool isMarked(int id, int tok) const { return mark[id] == tok; }
        void setMarked(int id, int tok) { mark[id] = tok; }
    };

    thread_local StampBuf g_seenStone;   // dùng cho "visited stones" trong evaluate
    thread_local StampBuf g_seenLib;     // dùng cho "seen liberties" theo từng group BFS
    thread_local StampBuf g_tmpVisited;  // dùng cho BFS cục bộ trong candidate scoring

    // BFS lấy thông tin group (stones + liberties) bắt đầu từ (sx, sy).
    // - visitedTok: dùng chung cho toàn bộ vòng quét để tránh đếm lại group.
    GroupInfo analyzeGroupFrom(const Game& game, int sx, int sy, int visitedTok)
    {
        GroupInfo info;
        PieceColor c = game.getPiece(sx, sy);
        if (c == NONE) return info;

        info.color = c;

        g_seenLib.ensureSize(BOARD_SIZE * BOARD_SIZE);
        const int libTok = g_seenLib.nextToken();

        std::queue<std::pair<int,int>> q;
        q.push({sx, sy});
        g_seenStone.setMarked(idx(sx, sy), visitedTok);

        static const int dx[4] = {-1, 1, 0, 0};
        static const int dy[4] = {0, 0, -1, 1};

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            info.stones++;

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx[dir];
                int ny = y + dy[dir];
                if (!inBounds(nx, ny)) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    int id = idx(nx, ny);
                    if (!g_seenLib.isMarked(id, libTok)) {
                        g_seenLib.setMarked(id, libTok);
                        info.liberties++;
                    }
                } else if (p == c) {
                    int nid = idx(nx, ny);
                    if (!g_seenStone.isMarked(nid, visitedTok)) {
                        g_seenStone.setMarked(nid, visitedTok);
                        q.push({nx, ny});
                    }
                }
            }
        }

        info.atari = (info.liberties == 1);
        return info;
    }

    int countEmpty(const Game& game) {
        int empty = 0;
        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                if (game.getPiece(x, y) == NONE) ++empty;
        return empty;
    }

    int countStones(const Game& game) {
        int s = 0;
        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                if (game.getPiece(x, y) != NONE) ++s;
        return s;
    }

    // Node budget để đảm bảo "reasonable time" cho Hard/Medium.
    struct SearchBudget { int remaining = 0; };
    thread_local SearchBudget g_budget;
}

double GoAI::evaluateBoardHeuristic(const Game& game, PieceColor aiColor)
{
    PieceColor oppColor = game.oppositeColor(aiColor);

    g_seenStone.ensureSize(BOARD_SIZE * BOARD_SIZE);
    const int visitedTok = g_seenStone.nextToken();

    int aiStones = 0, oppStones = 0;
    int aiLib = 0, oppLib = 0;

    int aiAtariStones = 0;
    int oppAtariStones = 0;

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) == NONE) continue;
            int id = idx(x, y);
            if (g_seenStone.isMarked(id, visitedTok)) continue;

            GroupInfo info = analyzeGroupFrom(game, x, y, visitedTok);

            if (info.color == aiColor) {
                aiStones += info.stones;
                aiLib += info.liberties;
                if (info.atari) aiAtariStones += info.stones;
            } else if (info.color == oppColor) {
                oppStones += info.stones;
                oppLib += info.liberties;
                if (info.atari) oppAtariStones += info.stones;
            }
        }
    }

    const double W_STONE = 1.0;   // material
    const double W_LIB   = 0.35;  // stability
    const double W_ATARI = 0.70;  // tactical threats (theo số stones ở atari)

    double stoneDiff = (aiStones - oppStones) * W_STONE;
    double libDiff   = (aiLib - oppLib) * W_LIB;
    double atariDiff = (oppAtariStones - aiAtariStones) * W_ATARI;

    return stoneDiff + libDiff + atariDiff;
}

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

    const PieceColor aiColor  = game.getTurn();
    const PieceColor oppColor = game.oppositeColor(aiColor);

    constexpr int R = 2;
    std::array<uint8_t, BOARD_SIZE * BOARD_SIZE> candMask{};
    candMask.fill(0);

    auto tryAdd = [&](int x, int y) {
        if (!inBounds(x, y)) return;
        if (game.getPiece(x, y) != NONE) return;
        candMask[idx(x, y)] = 1;
    };

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) == NONE) continue;
            for (int dy = -R; dy <= R; ++dy) {
                for (int dx = -R; dx <= R; ++dx) {
                    if (std::abs(dx) + std::abs(dy) > R) continue;
                    tryAdd(x + dx, y + dy);
                }
            }
        }
    }

    const int stonesNow = countStones(game);
    if (stonesNow <= 10) {
        const std::pair<int,int> star[] = {
            {3,3}, {3,15}, {15,3}, {15,15},
            {3,9}, {9,3}, {9,15}, {15,9},
            {9,9}
        };
        for (auto [sx, sy] : star) tryAdd(sx, sy);
    }

    int candCount = 0;
    for (auto v : candMask) candCount += (v != 0);
    if (candCount == 0) {
        for (int y = 0; y < BOARD_SIZE; ++y)
            for (int x = 0; x < BOARD_SIZE; ++x)
                if (game.getPiece(x, y) == NONE) candMask[idx(x,y)] = 1;
    }

    //    - liberties: để check atari
    //    - stones: để thưởng capture theo số quân có thể bắt/cứu
    std::array<int, BOARD_SIZE * BOARD_SIZE> libCache;
    std::array<int, BOARD_SIZE * BOARD_SIZE> sizeCache;
    libCache.fill(-1);
    sizeCache.fill(0);

    g_tmpVisited.ensureSize(BOARD_SIZE * BOARD_SIZE);
    g_seenLib.ensureSize(BOARD_SIZE * BOARD_SIZE);

    static const int dx4[4] = {-1, 1, 0, 0};
    static const int dy4[4] = {0, 0, -1, 1};

    std::vector<int> groupStones;
    groupStones.reserve(64);

    auto groupStatsCached = [&](int sx, int sy) -> std::pair<int,int> {
        if (!inBounds(sx, sy)) return {0, 0};
        int id0 = idx(sx, sy);
        if (libCache[id0] != -1) return {libCache[id0], sizeCache[id0]};

        PieceColor c = game.getPiece(sx, sy);
        if (c == NONE) { libCache[id0] = 0; sizeCache[id0] = 0; return {0, 0}; }

        const int vTok = g_tmpVisited.nextToken();
        const int lTok = g_seenLib.nextToken();

        std::queue<std::pair<int,int>> q;
        q.push({sx, sy});
        g_tmpVisited.setMarked(id0, vTok);

        groupStones.clear();
        groupStones.push_back(id0);

        int liberties = 0;

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx4[dir];
                int ny = y + dy4[dir];
                if (!inBounds(nx, ny)) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    int lid = idx(nx, ny);
                    if (!g_seenLib.isMarked(lid, lTok)) {
                        g_seenLib.setMarked(lid, lTok);
                        liberties++;
                    }
                } else if (p == c) {
                    int nid = idx(nx, ny);
                    if (!g_tmpVisited.isMarked(nid, vTok)) {
                        g_tmpVisited.setMarked(nid, vTok);
                        q.push({nx, ny});
                        groupStones.push_back(nid);
                    }
                }
            }
        }

        const int stones = (int)groupStones.size();
        for (int sid : groupStones) {
            libCache[sid] = liberties;
            sizeCache[sid] = stones;
        }
        return {liberties, stones};
    };


    scored.reserve(candCount);

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (!candMask[idx(x,y)]) continue;

            int score = 0;

            if (stonesNow <= 10) {
                if ((x == 3 || x == 15) && (y == 3 || y == 15)) score += 6; // 4-4
                if ((x == 3 || x == 15) && y == 9) score += 3;
                if ((y == 3 || y == 15) && x == 9) score += 3;
                if (x == 9 && y == 9) score += 4;
            }

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx4[dir];
                int ny = y + dy4[dir];
                if (!inBounds(nx, ny)) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    score += 1;
                } else if (p == oppColor) {
                    // nếu group địch đang ở atari và (x,y) kề nó => có khả năng là liberty cuối => bắt
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) score += 30 + 3 * stones;
                } else if (p == aiColor) {
                    // cứu group mình ở atari
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) score += 18 + 2 * stones;
                    // nối group
                    score += 3;
                }
            }

            scored.push_back({ AIMove(x, y, false), score });
        }
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredMove& a, const ScoredMove& b) { return a.s > b.s; });

    std::vector<AIMove> moves;
    moves.reserve(scored.size());
    for (auto& sm : scored) moves.push_back(sm.m);

    return moves;
}

// Minimax (Medium) với cắt candidate để giảm branching
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

    // Beam trong search để giữ thời gian ổn định
    int limit = 40;
    if (maxDepth >= 3) limit = 28;
    if (depth >= maxDepth - 1) limit = 14;
    if ((int)moves.size() > limit) moves.resize(limit);

    bool anyValidChild = false;

    if (maximizingPlayer) {
        double bestValue = -std::numeric_limits<double>::infinity();
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimax(child, depth + 1, maxDepth, false, aiColor);
            if (val > bestValue) bestValue = val;
        }
        if (!anyValidChild) return evaluateBoardHeuristic(game, aiColor);
        return bestValue;
    } else {
        double bestValue = +std::numeric_limits<double>::infinity();
        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimax(child, depth + 1, maxDepth, true, aiColor);
            if (val < bestValue) bestValue = val;
        }
        if (!anyValidChild) return evaluateBoardHeuristic(game, aiColor);
        return bestValue;
    }
}

// Minimax + Alpha–Beta (Hard) + beam limit
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

    int limit = 30;
    if (depth >= maxDepth - 1) limit = 14;
    if ((int)moves.size() > limit) moves.resize(limit);

    bool anyValidChild = false;

    if (maximizingPlayer) {
        double bestValue = -std::numeric_limits<double>::infinity();

        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, false, aiColor);

            if (val > bestValue) bestValue = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }

        if (!anyValidChild) return evaluateBoardHeuristic(game, aiColor);
        return bestValue;
    } else {
        double bestValue = +std::numeric_limits<double>::infinity();

        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, true, aiColor);

            if (val < bestValue) bestValue = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }

        if (!anyValidChild) return evaluateBoardHeuristic(game, aiColor);
        return bestValue;
    }
}

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

    // depth
    int maxDepth = (difficulty == AIDifficulty::MEDIUM) ? 2 : 3;

    // Budget
    g_budget.remaining = (difficulty == AIDifficulty::MEDIUM) ? 2500 : 12000;

    const bool useAlphaBeta = (difficulty == AIDifficulty::HARD);

    // Root beam
    const int ROOT_LIMIT = (difficulty == AIDifficulty::HARD) ? 40 : 60;

    struct RootChoice { AIMove move; double score; };
    std::vector<RootChoice> root;
    root.reserve(ROOT_LIMIT);

    int legalCount = 0;
    for (const AIMove& m : candidates) {
        Game child = game;
        if (!child.placeStone(m.x, m.y)) continue;

        legalCount++;
        if (legalCount > ROOT_LIMIT) break;

        double score;
        if (useAlphaBeta) {
            score = minimaxAlphaBeta(child,
                                     1,
                                     maxDepth,
                                     -std::numeric_limits<double>::infinity(),
                                     +std::numeric_limits<double>::infinity(),
                                     false,
                                     aiColor);
        } else {
            score = minimax(child,
                            1,
                            maxDepth,
                            false,
                            aiColor);

            std::uniform_real_distribution<double> noise(-0.6, 0.6);
            score += noise(globalRng());
        }

        root.push_back({ AIMove(m.x, m.y, false), score });
    }

    if (root.empty()) return AIMove(-1, -1, true);

    std::sort(root.begin(), root.end(), [](const RootChoice& a, const RootChoice& b) {
        return a.score > b.score;
    });

    const int emptyNow = countEmpty(game);
    const bool lateGame = (emptyNow <= 60) || (root.size() <= 10);

    if (lateGame) {
        const double passBias = (emptyNow <= 40) ? 0.0 : -1.5;
        double passScore = evaluateBoardHeuristic(game, aiColor) + passBias;

        if (passScore >= root.front().score - 0.75) {
            return AIMove(-1, -1, true);
        }
    }

    if (difficulty == AIDifficulty::MEDIUM) {
        int k = std::min<int>(3, (int)root.size());
        std::uniform_int_distribution<int> pick(0, k - 1);
        return root[pick(globalRng())].move;
    }

    return root.front().move;
}

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
