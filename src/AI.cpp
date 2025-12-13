#include "AI.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <vector>
#include <utility>

namespace {
    constexpr int kBoardSize = 19;

    inline int idx(int x, int y) { return y * kBoardSize + x; }
    inline bool kinBounds(int x, int y) { return (0 <= x && x < kBoardSize && 0 <= y && y < kBoardSize); }

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

        g_seenLib.ensureSize(kBoardSize * kBoardSize);
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
                if (!kinBounds(nx, ny)) continue;

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
        for (int y = 0; y < kBoardSize; ++y)
            for (int x = 0; x < kBoardSize; ++x)
                if (game.getPiece(x, y) == NONE) ++empty;
        return empty;
    }

    int countStones(const Game& game) {
        int s = 0;
        for (int y = 0; y < kBoardSize; ++y)
            for (int x = 0; x < kBoardSize; ++x)
                if (game.getPiece(x, y) != NONE) ++s;
        return s;
    }

    // Node budget để đảm bảo "reasonable time" cho Hard/Medium.
    struct SearchBudget { int remaining = 0; };
    thread_local SearchBudget g_budget;
}

double GoAI::evaluateBoardHeuristic(const Game& game, PieceColor aiColor)
{
    const PieceColor oppColor = game.oppositeColor(aiColor);

    // ---- Group-based tactical / stability features ----
    g_seenStone.ensureSize(kBoardSize * kBoardSize);
    const int visitedTok = g_seenStone.nextToken();

    int aiStones = 0, oppStones = 0;
    int aiLib = 0, oppLib = 0;

    int aiAtariStones = 0;
    int oppAtariStones = 0;

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            PieceColor pc = game.getPiece(x, y);
            if (pc == NONE) continue;

            const int id = idx(x, y);
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

    // ---- Simple territory estimate (no dead-mark): flood-fill empty regions ----
    std::array<uint8_t, kBoardSize * kBoardSize> visited{};
    visited.fill(0);

    int aiTerr = 0, oppTerr = 0;

    static const int dx4[4] = {-1, 1, 0, 0};
    static const int dy4[4] = {0, 0, -1, 1};

    std::array<int, kBoardSize * kBoardSize> q{};
    std::array<int, kBoardSize * kBoardSize> region{};
    int qh = 0, qt = 0, rn = 0;

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (game.getPiece(x, y) != NONE) continue;
            const int startId = idx(x, y);
            if (visited[startId]) continue;

            bool touchesAI = false;
            bool touchesOpp = false;

            qh = qt = rn = 0;
            q[qt++] = startId;
            visited[startId] = 1;

            while (qh < qt) {
                const int id = q[qh++];
                region[rn++] = id;

                const int cx = id % kBoardSize;
                const int cy = id / kBoardSize;

                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx4[d];
                    const int ny = cy + dy4[d];
                    if (!kinBounds(nx, ny)) continue;

                    PieceColor p = game.getPiece(nx, ny);
                    if (p == NONE) {
                        const int nid = idx(nx, ny);
                        if (!visited[nid]) {
                            visited[nid] = 1;
                            q[qt++] = nid;
                        }
                    } else if (p == aiColor) {
                        touchesAI = true;
                    } else if (p == oppColor) {
                        touchesOpp = true;
                    }
                }
            }

            if (touchesAI && !touchesOpp) aiTerr += rn;
            else if (touchesOpp && !touchesAI) oppTerr += rn;
        }
    }

    // ---- Phase-aware weights ----
    const int emptyNow = countEmpty(game);
    const double phase = std::clamp((361.0 - emptyNow) / 361.0, 0.0, 1.0); // 0 early -> 1 late

    const double W_STONE = 0.65 + 0.35 * phase;   // stones matter more late
    const double W_LIB   = 0.30;                  // stability
    const double W_ATARI = 0.75;                  // tactical threats
    const double W_TERR  = 0.95;                  // territory estimate

    const double stoneDiff = (aiStones - oppStones) * W_STONE;
    const double libDiff   = (aiLib - oppLib) * W_LIB;
    const double atariDiff = (oppAtariStones - aiAtariStones) * W_ATARI;
    const double terrDiff  = (aiTerr - oppTerr) * W_TERR;

    // Slightly discourage "all-in atari trading" when very early: needs territory too.
    const double earlyStabilityBonus = (phase < 0.25) ? (0.10 * libDiff + 0.15 * terrDiff) : 0.0;

    return stoneDiff + libDiff + atariDiff + terrDiff + earlyStabilityBonus;
}


std::vector<AIMove>  GoAI::generateCandidateMoves(const Game& game)
{
    struct ScoredMove { AIMove m; int s; };
    std::vector<ScoredMove> scored;

    // If board empty: play center (stable opening)
    bool anyStone = false;
    for (int y = 0; y < kBoardSize && !anyStone; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (game.getPiece(x, y) != NONE) { anyStone = true; break; }
        }
    }
    if (!anyStone) {
        const int c = kBoardSize / 2;
        return { AIMove(c, c, false) };
    }

    const PieceColor aiColor  = game.getTurn();
    const PieceColor oppColor = game.oppositeColor(aiColor);

    constexpr int R = 2;
    std::array<uint8_t, kBoardSize * kBoardSize> candMask{};
    candMask.fill(0);

    auto tryAdd = [&](int x, int y) {
        if (!kinBounds(x, y)) return;
        if (game.getPiece(x, y) != NONE) return;
        candMask[idx(x, y)] = 1;
    };

    // Local candidates: within manhattan radius R of any existing stone.
    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
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

    // Opening support: star points (19x19 fixed).
    if (stonesNow <= 10) {
        const std::pair<int,int> star[] = {
            {3,3}, {3,15}, {15,3}, {15,15},
            {3,9}, {9,3}, {9,15}, {15,9},
            {9,9}
        };
        for (auto [sx, sy] : star) tryAdd(sx, sy);
    }

    // Add a few "global" candidates so the AI can tenuki / play big points,
    // even when fights are local.
    // (We keep this small to avoid blowing up branching.)
    if (stonesNow >= 8) {
        struct GlobalCand { int pot; int x; int y; };
        std::vector<GlobalCand> globals;
        globals.reserve(128);

        auto centerBias = [&](int x, int y) {
            // 0..18-ish, gentle bias only
            int cb = 18 - (std::abs(x - 9) + std::abs(y - 9));
            return cb;
        };

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                const int id = idx(x, y);
                if (candMask[id]) continue;
                if (game.getPiece(x, y) != NONE) continue;

                int emptyAdj = 0;
                int aiAdj = 0;
                int oppAdj = 0;

                static const int dx4[4] = {-1, 1, 0, 0};
                static const int dy4[4] = {0, 0, -1, 1};

                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx4[d];
                    int ny = y + dy4[d];
                    if (!kinBounds(nx, ny)) continue;

                    PieceColor p = game.getPiece(nx, ny);
                    if (p == NONE) ++emptyAdj;
                    else if (p == aiColor) ++aiAdj;
                    else if (p == oppColor) ++oppAdj;
                }

                // Avoid obvious eye-fills as "global" suggestions.
                if (emptyAdj == 0 && oppAdj == 0 && aiAdj >= 3) continue;

                const int distEdge = std::min(std::min(x, y), std::min(18 - x, 18 - y));
                int pot = emptyAdj * 5 + distEdge * 2 + centerBias(x, y) / 2;

                globals.push_back({ pot, x, y });
            }
        }

        std::sort(globals.begin(), globals.end(),
                  [](const GlobalCand& a, const GlobalCand& b) { return a.pot > b.pot; });

        const int GLOBAL_ADD = 12;
        for (int i = 0; i < (int)globals.size() && i < GLOBAL_ADD; ++i) {
            tryAdd(globals[i].x, globals[i].y);
        }
    }

    // If somehow still empty, fall back to "all empties".
    int candCount = 0;
    for (auto v : candMask) candCount += (v != 0);
    if (candCount == 0) {
        for (int y = 0; y < kBoardSize; ++y)
            for (int x = 0; x < kBoardSize; ++x)
                if (game.getPiece(x, y) == NONE) candMask[idx(x, y)] = 1;
    }

    // Cache group stats for neighbor-based scoring:
    // - liberties: to detect atari
    // - stones: to scale capture/save bonuses
    std::array<int, kBoardSize * kBoardSize> libCache;
    std::array<int, kBoardSize * kBoardSize> sizeCache;
    libCache.fill(-1);
    sizeCache.fill(0);

    g_tmpVisited.ensureSize(kBoardSize * kBoardSize);
    g_seenLib.ensureSize(kBoardSize * kBoardSize);

    static const int dx4[4] = {-1, 1, 0, 0};
    static const int dy4[4] = {0, 0, -1, 1};

    std::vector<int> groupStones;
    groupStones.reserve(64);

    auto groupStatsCached = [&](int sx, int sy) -> std::pair<int,int> {
        if (!kinBounds(sx, sy)) return {0, 0};
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
            auto [cx, cy] = q.front();
            q.pop();

            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx4[d];
                int ny = cy + dy4[d];
                if (!kinBounds(nx, ny)) continue;

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

    scored.reserve(128);

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (!candMask[idx(x, y)]) continue;

            int score = 0;

            // Opening: mild bias toward star-ish points
            if (stonesNow <= 10) {
                if ((x == 3 || x == 15) && (y == 3 || y == 15)) score += 6; // 4-4
                if ((x == 3 || x == 15) && y == 9) score += 3;
                if ((y == 3 || y == 15) && x == 9) score += 3;
                if (x == 9 && y == 9) score += 4;
            }

            int emptyAdj = 0;
            int aiAdj = 0;
            int oppAdj = 0;
            bool wouldCapture = false;

            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d];
                int ny = y + dy4[d];
                if (!kinBounds(nx, ny)) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    ++emptyAdj;
                    score += 1;
                } else if (p == oppColor) {
                    ++oppAdj;
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) {
                        wouldCapture = true;
                        score += 30 + 3 * stones; // capture atari group
                    }
                } else if (p == aiColor) {
                    ++aiAdj;
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) score += 18 + 2 * stones; // save atari group
                    score += 3; // connection tendency
                }
            }

            // Global shape: prefer moves with more breathing room.
            score += emptyAdj * 2;

            // Penalize obvious self-atari / eye fill when not capturing.
            // (Not perfect, but reduces silly moves a lot.)
            if (!wouldCapture) {
                if (emptyAdj == 0 && oppAdj == 0 && aiAdj >= 3) score -= 80;  // filling own eye
                else if (emptyAdj <= 1 && oppAdj == 0) score -= 18;            // likely self-atari
            }

            // Small edge/corner penalty in very early game (avoid 1-1 / 2-1 junk)
            if (stonesNow <= 12) {
                if (x <= 1 || x >= 17 || y <= 1 || y >= 17) score -= 6;
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

    // Beam in search to keep time stable
    int limit = 40;
    if (maxDepth >= 3) limit = 28;
    if (depth >= maxDepth - 1) limit = 14;
    if ((int)moves.size() > limit) moves.resize(limit);

    // Consider PASS as a real action in late game so minimax can reason about ending.
    const int emptyNow = countEmpty(game);
    const bool allowPass = (emptyNow <= 40);
    if (allowPass) moves.push_back(AIMove(-1, -1, true));

    bool anyValidChild = false;

    if (maximizingPlayer) {
        double bestValue = -std::numeric_limits<double>::infinity();

        for (const AIMove& m : moves) {
            Game child = game;

            bool ok = false;
            bool finished = false;

            if (m.isPass) {
                finished = child.pass();
                ok = true;
            } else {
                ok = child.placeStone(m.x, m.y);
            }

            if (!ok) continue;
            anyValidChild = true;

            double val = finished
                ? evaluateBoardHeuristic(child, aiColor)
                : minimax(child, depth + 1, maxDepth, false, aiColor);

            // Discourage premature pass (even if allowed), unless very late.
            if (m.isPass && emptyNow > 25) val -= 0.8;

            if (val > bestValue) bestValue = val;
        }

        if (!anyValidChild) return evaluateBoardHeuristic(game, aiColor);
        return bestValue;
    } else {
        double bestValue = +std::numeric_limits<double>::infinity();

        for (const AIMove& m : moves) {
            Game child = game;

            bool ok = false;
            bool finished = false;

            if (m.isPass) {
                finished = child.pass();
                ok = true;
            } else {
                ok = child.placeStone(m.x, m.y);
            }

            if (!ok) continue;
            anyValidChild = true;

            double val = finished
                ? evaluateBoardHeuristic(child, aiColor)
                : minimax(child, depth + 1, maxDepth, true, aiColor);

            if (m.isPass && emptyNow > 25) val += 0.8; // opponent passing early is good for us

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

    const int emptyNow = countEmpty(game);
    const bool allowPass = (emptyNow <= 40);
    if (allowPass) moves.push_back(AIMove(-1, -1, true));

    bool anyValidChild = false;

    if (maximizingPlayer) {
        double bestValue = -std::numeric_limits<double>::infinity();

        for (const AIMove& m : moves) {
            Game child = game;

            bool ok = false;
            bool finished = false;

            if (m.isPass) {
                finished = child.pass();
                ok = true;
            } else {
                ok = child.placeStone(m.x, m.y);
            }

            if (!ok) continue;
            anyValidChild = true;

            double val = finished
                ? evaluateBoardHeuristic(child, aiColor)
                : minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, false, aiColor);

            if (m.isPass && emptyNow > 25) val -= 0.8;

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

            bool ok = false;
            bool finished = false;

            if (m.isPass) {
                finished = child.pass();
                ok = true;
            } else {
                ok = child.placeStone(m.x, m.y);
            }

            if (!ok) continue;
            anyValidChild = true;

            double val = finished
                ? evaluateBoardHeuristic(child, aiColor)
                : minimaxAlphaBeta(child, depth + 1, maxDepth, alpha, beta, true, aiColor);

            if (m.isPass && emptyNow > 25) val += 0.8;

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

AIMoveOutcome GoAI::playAIMoveWithOutcome(Game& game, AIDifficulty difficulty)
{
    // NOTE:
    // - In AI search we use Game copies (snapshots) for speed.
    // - We still ALWAYS validate moves on the real `game` when actually playing,
    //   so all rules (including KO / suicide / occupancy) are enforced.

    const PieceColor aiColor = game.getTurn();

    std::vector<AIMove> candidates = generateCandidateMoves(game);
    if (candidates.empty()) {
        std::cout << "AI cannot generate moves, PASS.\n";
        bool finished = game.pass();
        return { false, finished };
    }

    // EASY: pick a random legal move (validated on the real game).
    if (difficulty == AIDifficulty::EASY) {
        std::shuffle(candidates.begin(), candidates.end(), globalRng());
        for (const AIMove& m : candidates) {
            if (game.placeStone(m.x, m.y)) {
                std::cout << "AI plays at (" << m.x << ", " << m.y << ")\n";
                return { true, false };
            }
        }
        std::cout << "AI cannot find any legal move, PASS.\n";
        bool finished = game.pass();
        return { false, finished };
    }

    // Depth settings
    const int maxDepth = (difficulty == AIDifficulty::MEDIUM) ? 2 : 3;

    // Budget (keep time stable)
    g_budget.remaining = (difficulty == AIDifficulty::MEDIUM) ? 2500 : 12000;

    const bool useAlphaBeta = (difficulty == AIDifficulty::HARD);
    const int ROOT_LIMIT = (difficulty == AIDifficulty::HARD) ? 40 : 60;

    struct RootChoice { AIMove move; double score; };
    std::vector<RootChoice> root;
    root.reserve(ROOT_LIMIT);

    // Score root moves using cheap copy-based search.
    int considered = 0;
    for (const AIMove& m : candidates) {
        Game child = game; // copy ctor creates a snapshot (no history recording)
        if (!child.placeStone(m.x, m.y)) continue;

        ++considered;
        if (considered > ROOT_LIMIT) break;

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
            score = minimax(child, 1, maxDepth, false, aiColor);

            // small noise so Medium feels less deterministic
            std::uniform_real_distribution<double> noise(-0.6, 0.6);
            score += noise(globalRng());
        }

        root.push_back({ AIMove(m.x, m.y, false), score });
    }

    if (root.empty()) {
        std::cout << "AI cannot find any legal move (after scoring), PASS.\n";
        bool finished = game.pass();
        return { false, finished };
    }

    std::sort(root.begin(), root.end(),
              [](const RootChoice& a, const RootChoice& b) { return a.score > b.score; });

    // Consider PASS (late game): only when it's not clearly worse than the best found move.
    const int emptyNow = countEmpty(game);
    const bool lateGame = (emptyNow <= 60) || ((int)root.size() <= 10);
    if (lateGame) {
        const double passBias = (emptyNow <= 40) ? 0.0 : -1.5;
        const double passScore = evaluateBoardHeuristic(game, aiColor) + passBias;

        if (passScore >= root.front().score - 0.75) {
            std::cout << "AI chooses to PASS.\n";
            bool finished = game.pass();
        return { false, finished };
        }
    }

    // Try to actually play on the REAL game (so KO/invalid rules are enforced).
    if (difficulty == AIDifficulty::MEDIUM) {
        // Try a random pick among the top-K first, then fall back down the list.
        const int K = std::min<int>(3, (int)root.size());

        std::vector<int> order;
        order.reserve(root.size());
        for (int i = 0; i < (int)root.size(); ++i) order.push_back(i);

        // shuffle only top-K indices to keep it "medium" but not too random
        std::shuffle(order.begin(), order.begin() + K, globalRng());

        for (int idxChoice : order) {
            const AIMove& m = root[idxChoice].move;
            if (game.placeStone(m.x, m.y)) {
                std::cout << "AI plays at (" << m.x << ", " << m.y << ")\n";
                return { true, false };
            }
        }
    } else {
        // HARD: best-first
        for (const auto& rc : root) {
            const AIMove& m = rc.move;
            if (game.placeStone(m.x, m.y)) {
                std::cout << "AI plays at (" << m.x << ", " << m.y << ")\n";
                return { true, false };
            }
        }
    }

    // Last resort: try any candidate directly.
    std::shuffle(candidates.begin(), candidates.end(), globalRng());
    for (const AIMove& m : candidates) {
        if (game.placeStone(m.x, m.y)) {
            std::cout << "AI fallback move at (" << m.x << ", " << m.y << ")\n";
            return { true, false };
        }
    }

    std::cout << "AI cannot find any legal move, PASS.\n";
    bool finished = game.pass();
        return { false, finished };
}

bool GoAI::playAIMove(Game& game, AIDifficulty difficulty)
{
    return playAIMoveWithOutcome(game, difficulty).playedStone;
}


