#include "AI.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <functional>
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
        // Thread-local RNG: safe when AI is called from a worker thread (VS AI)
        // and also from the main thread (e.g., Hint).
        thread_local std::mt19937 rng([] {
            const auto now = static_cast<std::uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            const auto tid = static_cast<std::uint64_t>(
                std::hash<std::thread::id>{}(std::this_thread::get_id()));

            std::seed_seq seq{
                static_cast<std::uint32_t>(now),
                static_cast<std::uint32_t>(now >> 32),
                static_cast<std::uint32_t>(tid),
                static_cast<std::uint32_t>(tid >> 32),
            };
            return std::mt19937(seq);
        }());

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


    // -------------------------------
    // Urgent move generator (HARD)
    // -------------------------------
    // Purpose:
    //  - Always consider forcing tactical moves first (capture/save/atari/cut/connect).
    //  - Keep the list small to avoid branching explosion.
    //
    // Notes:
    //  - Legality (ko/suicide, etc.) is still validated by Game::placeStone().
    //  - We score and deduplicate by board coordinate.
    struct UrgentScored {
        AIMove m;
        int score = 0;
    };

    static std::vector<AIMove> generateUrgentMoves(const Game& game)
    {
        constexpr int N = kBoardSize * kBoardSize;
        constexpr int kMaxUrgent = 24;

        std::array<int, N> best;
        best.fill(std::numeric_limits<int>::min());

        auto consider = [&](int x, int y, int s) {
            if (!kinBounds(x, y)) return;
            if (game.getPiece(x, y) != NONE) return;
            const int id = idx(x, y);
            if (s > best[id]) best[id] = s;
        };

        // Build group id map + (up to 2) liberties for every group in one scan.
        g_seenStone.ensureSize(N);
        g_seenLib.ensureSize(N);

        const int visitedTok = g_seenStone.nextToken();
        std::vector<int> groupId(N, -1);

        struct G {
            PieceColor color = NONE;
            int stones = 0;
            int liberties = 0;
            int libCount = 0;
            int libX[2] = { -1, -1 };
            int libY[2] = { -1, -1 };
        };

        std::vector<G> groups;
        groups.reserve(128);

        static const int dx4[4] = { -1, 1, 0, 0 };
        static const int dy4[4] = { 0, 0, -1, 1 };

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                const PieceColor c = game.getPiece(x, y);
                if (c == NONE) continue;

                const int id0 = idx(x, y);
                if (g_seenStone.isMarked(id0, visitedTok)) continue;

                const int groupIndex = (int)groups.size();

                G g;
                g.color = c;

                const int libTok = g_seenLib.nextToken();

                std::queue<std::pair<int, int>> q;
                q.push({ x, y });
                g_seenStone.setMarked(id0, visitedTok);
                groupId[id0] = groupIndex;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();

                    g.stones++;

                    for (int dir = 0; dir < 4; ++dir) {
                        const int nx = cx + dx4[dir];
                        const int ny = cy + dy4[dir];
                        if (!kinBounds(nx, ny)) continue;

                        const PieceColor p = game.getPiece(nx, ny);
                        const int nid = idx(nx, ny);

                        if (p == NONE) {
                            if (!g_seenLib.isMarked(nid, libTok)) {
                                g_seenLib.setMarked(nid, libTok);
                                g.liberties++;
                                if (g.libCount < 2) {
                                    g.libX[g.libCount] = nx;
                                    g.libY[g.libCount] = ny;
                                    g.libCount++;
                                }
                            }
                        } else if (p == c) {
                            if (!g_seenStone.isMarked(nid, visitedTok)) {
                                g_seenStone.setMarked(nid, visitedTok);
                                groupId[nid] = groupIndex;
                                q.push({ nx, ny });
                            }
                        }
                    }
                }

                groups.push_back(g);
            }
        }

        const PieceColor us = game.getTurn();
        const PieceColor them = (us == BLACK) ? WHITE : BLACK;

        // CAPTURE / SAVE / CREATE ATARI
        for (const G& g : groups) {
            if (g.libCount <= 0) continue;

            if (g.color == them) {
                // Immediate capture if opponent group is in atari.
                if (g.liberties == 1 && g.libCount == 1) {
                    consider(g.libX[0], g.libY[0], 100 + g.stones);
                }
                // Forcing atari attempt if opponent group has exactly 2 liberties.
                else if (g.liberties == 2 && g.libCount == 2) {
                    consider(g.libX[0], g.libY[0], 70 + g.stones);
                    consider(g.libX[1], g.libY[1], 70 + g.stones);
                }
            } else if (g.color == us) {
                // Save our atari group (simple defense: fill the last liberty).
                if (g.liberties == 1 && g.libCount == 1) {
                    consider(g.libX[0], g.libY[0], 95 + g.stones);
                }
            }
        }

        // SAVE BY CAPTURE (counter-capture)
        // If our group is in atari, sometimes the correct defense is to capture an adjacent enemy atari group.
        // This fixes many "AI defends wrong" situations (snapback / ko / throw-in).
        std::vector<uint8_t> usAtari(groups.size(), 0);
        for (int i = 0; i < (int)groups.size(); ++i) {
            if (groups[i].color == us && groups[i].liberties == 1 && groups[i].libCount == 1) usAtari[i] = 1;
        }

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                if (game.getPiece(x, y) != us) continue;

                const int gidUs = groupId[idx(x, y)];
                if (gidUs < 0 || !usAtari[gidUs]) continue;

                for (int dir = 0; dir < 4; ++dir) {
                    const int nx = x + dx4[dir];
                    const int ny = y + dy4[dir];
                    if (!kinBounds(nx, ny)) continue;
                    if (game.getPiece(nx, ny) != them) continue;

                    const int gidTh = groupId[idx(nx, ny)];
                    if (gidTh < 0) continue;

                    const G& eg = groups[gidTh];
                    if (eg.color == them && eg.liberties == 1 && eg.libCount == 1) {
                        // Higher than "save by filling last liberty", because it often wins immediately.
                        consider(eg.libX[0], eg.libY[0], 130 + 3 * eg.stones + groups[gidUs].stones);
                    }
                }
            }
        }

        // CUT / CONNECT (based on touching >=2 distinct neighboring groups)
        auto addUnique = [](int* arr, int& cnt, int gid) {
            for (int i = 0; i < cnt; ++i) if (arr[i] == gid) return;
            arr[cnt++] = gid;
        };

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                if (game.getPiece(x, y) != NONE) continue;

                int usIds[4]; int usCnt = 0;
                int thIds[4]; int thCnt = 0;

                for (int dir = 0; dir < 4; ++dir) {
                    const int nx = x + dx4[dir];
                    const int ny = y + dy4[dir];
                    if (!kinBounds(nx, ny)) continue;

                    const PieceColor p = game.getPiece(nx, ny);
                    if (p == NONE) continue;

                    const int gid = groupId[idx(nx, ny)];
                    if (gid < 0) continue;

                    if (p == us) addUnique(usIds, usCnt, gid);
                    else if (p == them) addUnique(thIds, thCnt, gid);
                }

                if (usCnt >= 2) consider(x, y, 60 + (usCnt - 2));
                if (thCnt >= 2) consider(x, y, 64 + (thCnt - 2));
            }
        }

        // Collect + sort
        std::vector<UrgentScored> tmp;
        tmp.reserve(64);

        for (int id = 0; id < N; ++id) {
            if (best[id] == std::numeric_limits<int>::min()) continue;
            const int x = id % kBoardSize;
            const int y = id / kBoardSize;
            tmp.push_back({ AIMove(x, y, false), best[id] });
        }

        std::sort(tmp.begin(), tmp.end(),
            [](const UrgentScored& a, const UrgentScored& b) { return a.score > b.score; });

        std::vector<AIMove> out;
        out.reserve(std::min<int>((int)tmp.size(), kMaxUrgent));
        for (int i = 0; i < (int)tmp.size() && i < kMaxUrgent; ++i) out.push_back(tmp[i].m);

        return out;
    }

    // Node budget để đảm bảo "reasonable time" cho Hard/Medium.
    struct SearchBudget { int remaining = 0; };
    thread_local SearchBudget g_budget;


// ---------------------------
// Time limit support (for MEDIUM/HARD)
// - We stop search when reaching a per-move deadline.
// - To keep overhead low, we check time only every N visited nodes.
// ---------------------------
thread_local bool g_useDeadline = false;
thread_local bool g_abortSearch = false;
thread_local std::chrono::steady_clock::time_point g_deadline;
thread_local std::uint64_t g_nodesVisited = 0;

static inline void searchBeginTimeLimitMs(int ms) {
    g_abortSearch = false;
    g_nodesVisited = 0;
    g_useDeadline = (ms > 0);
    if (g_useDeadline) {
        g_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    }
}

static inline bool searchPollShouldAbort() {
    if (g_abortSearch) return true;
    if (g_budget.remaining <= 0) {
        g_abortSearch = true;
        return true;
    }
    if (!g_useDeadline) return false;

    // Check time only every 256 nodes.
    if ((g_nodesVisited & 0xFFu) == 0u) {
        if (std::chrono::steady_clock::now() >= g_deadline) {
            g_abortSearch = true;
            return true;
        }
    }
    return false;
}

static inline bool searchTimeUpNow() {
    if (!g_useDeadline) return false;
    if (std::chrono::steady_clock::now() >= g_deadline) {
        g_abortSearch = true;
        return true;
    }
    return false;
}


// ---------------------------
// Move ordering heuristics (HARD)
// - Killer moves: remember moves that caused beta cutoffs at each ply.
// - History heuristic: reward moves that often cause cutoffs (global within this search).
// These dramatically improve alpha-beta pruning depth without increasing branching.
// ---------------------------
constexpr int kMaxPly = 64; // safe upper bound for our shallow searches
thread_local std::array<int, kBoardSize * kBoardSize> g_history{};
thread_local std::array<AIMove, kMaxPly * 2> g_killer{}; // two killer moves per ply

static inline bool sameMove(const AIMove& a, const AIMove& b) {
    if (a.isPass != b.isPass) return false;
    if (a.isPass) return true;
    return a.x == b.x && a.y == b.y;
}

static inline void orderingNewSearch() {
    g_history.fill(0);
    for (auto& m : g_killer) m = AIMove(-1, -1, true);
}

static inline int moveHistoryScore(const AIMove& m) {
    if (m.isPass || m.x < 0) return std::numeric_limits<int>::min();
    return g_history[idx(m.x, m.y)];
}

static inline bool promoteMove(std::vector<AIMove>& moves, const AIMove& target, std::size_t toIndex) {
    if (target.isPass || target.x < 0) return false;
    for (std::size_t i = toIndex; i < moves.size(); ++i) {
        if (!moves[i].isPass && moves[i].x == target.x && moves[i].y == target.y) {
            std::swap(moves[toIndex], moves[i]);
            return true;
        }
    }
    return false;
}


    // ---------------------------
    // Territory estimate (Japanese-style): flood-fill empty regions and assign to the single bordering color.
    // ---------------------------
    thread_local StampBuf g_seenTerr;

    static std::pair<int,int> estimateTerritory(const Game& game) {
        g_seenTerr.ensureSize(kBoardSize * kBoardSize);
        const int tok = g_seenTerr.nextToken();

        int blackTerr = 0;
        int whiteTerr = 0;

        std::queue<std::pair<int,int>> q;
        static const int dx[4] = {-1, 1, 0, 0};
        static const int dy[4] = {0, 0, -1, 1};

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                if (game.getPiece(x, y) != NONE) continue;
                const int sid = idx(x, y);
                if (g_seenTerr.isMarked(sid, tok)) continue;

                bool touchesBlack = false;
                bool touchesWhite = false;
                int regionSize = 0;

                g_seenTerr.setMarked(sid, tok);
                q.push({x, y});

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    ++regionSize;

                    for (int k = 0; k < 4; ++k) {
                        int nx = cx + dx[k];
                        int ny = cy + dy[k];
                        if (!kinBounds(nx, ny)) continue;
                        PieceColor p = game.getPiece(nx, ny);
                        if (p == NONE) {
                            const int nid = idx(nx, ny);
                            if (!g_seenTerr.isMarked(nid, tok)) {
                                g_seenTerr.setMarked(nid, tok);
                                q.push({nx, ny});
                            }
                        } else if (p == BLACK) {
                            touchesBlack = true;
                        } else if (p == WHITE) {
                            touchesWhite = true;
                        }
                    }
                }

                if (touchesBlack && !touchesWhite) blackTerr += regionSize;
                else if (touchesWhite && !touchesBlack) whiteTerr += regionSize;
            }
        }
        return {blackTerr, whiteTerr};
    }

    static bool hasAnyAtariGroup(const Game& game) {
        g_seenStone.ensureSize(kBoardSize * kBoardSize);
        const int visitedTok = g_seenStone.nextToken();

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                if (game.getPiece(x, y) == NONE) continue;
                const int id = idx(x, y);
                if (g_seenStone.isMarked(id, visitedTok)) continue;

                GroupInfo gi = analyzeGroupFrom(game, x, y, visitedTok);
                if (gi.atari) return true;
            }
        }
        return false;
    }

// Quiescence trigger: beyond "has atari", also treat "capture threat / near-atari / snapback-ish"
// positions as tactically noisy. This reduces horizon effects without exploring quiet moves.
static bool isNoisyForQuiescence(const Game& game) {
    g_seenStone.ensureSize(kBoardSize * kBoardSize);
    const int visitedTok = g_seenStone.nextToken();

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (game.getPiece(x, y) == NONE) continue;
            const int id = idx(x, y);
            if (g_seenStone.isMarked(id, visitedTok)) continue;

            GroupInfo gi = analyzeGroupFrom(game, x, y, visitedTok);

            // Noisy if any group is already in atari, or has only 2 liberties (immediate atari/capture threats).
            if (gi.liberties <= 2) return true;
        }
    }
    return false;
}



    // ---------------------------
    // Transposition table (per computeAIMove call).
    // IMPORTANT: include ko-reference hash + turn in the key; otherwise ko positions will be mixed incorrectly.
    // ---------------------------
    enum class TTFlag : std::uint8_t { EXACT = 0, LOWER = 1, UPPER = 2 };

    struct TTEntry {
        std::uint64_t key = 0;
        std::uint16_t gen = 0;
        int depthRemaining = 0;  // how deep this entry is valid for
        double value = 0.0;
        TTFlag flag = TTFlag::EXACT;
        AIMove best = AIMove(-1, -1, true);
    };

    static inline std::uint64_t rotl64(std::uint64_t x, int r) {
        return (x << r) | (x >> (64 - r));
    }

    static inline std::uint64_t ttKey(const Game& g) {
        // IMPORTANT:
        // Board position alone is not sufficient for a stable TT in this project because:
        // - captures affect the score-aligned heuristic
        // - pass state affects end conditions (two consecutive passes) while the board hash is unchanged
        std::uint64_t h = g.getBoardHash();
        const std::uint64_t k = g.hasKoRef() ? g.getKoRefHash() : 0ULL;

        // ko reference + turn
        h ^= rotl64(k * 0x9e3779b97f4a7c15ULL, 17);
        h ^= (g.getTurn() == BLACK ? 0xBADC0FFEE0DDF00DULL : 0xC001D00DCAFEBEEFULL);
        h ^= (g.hasKoRef() ? 0xA5A5A5A5A5A5A5A5ULL : 0ULL);

        // captures (score-aligned eval depends on these)
        const std::uint64_t caps =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(g.getBlackCaptures()))      ) |
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(g.getWhiteCaptures())) << 32);
        h ^= rotl64(caps * 0xD6E8FEB86659FD93ULL, 31);

        // consecutive pass state:
        // we cannot read Game::consecutive_passes (private), so infer:
        // - ended() => 2 passes
        // - a single pass sets koRefHash == boardHash (see Game::pass), while a normal move sets koRefHash to previous board hash
        std::uint64_t passState = 0ULL; // 0,1,2
        if (g.ended()) passState = 2ULL;
        else if (g.hasKoRef() && g.getKoRefHash() == g.getBoardHash()) passState = 1ULL;
        h ^= rotl64((passState + 0x1234ULL) * 0x94D049BB133111EBULL, 11);

        return h;
    }


        // Fixed-size TT (direct-mapped) with generation tagging.
    // - Much faster and lower overhead than unordered_map.
    // - Replacement policy: keep the entry that is valid for deeper (>=) remaining depth.
    constexpr std::size_t kTTSize = 1u << 18; // 262,144 entries
    static std::array<TTEntry, kTTSize> g_tt{};
    static std::uint16_t g_ttGen = 1;

    static inline std::size_t ttIndex(std::uint64_t key) {
        return static_cast<std::size_t>(key) & (kTTSize - 1);
    }

    static inline void ttNewSearch() {
        ++g_ttGen;
        if (g_ttGen == 0) { // wrapped
            g_ttGen = 1;
            for (auto& e : g_tt) { e.key = 0; e.gen = 0; }
        }
    }

    static inline TTEntry* ttProbe(std::uint64_t key) {
        TTEntry& e = g_tt[ttIndex(key)];
        if (e.gen == g_ttGen && e.key == key) return &e;
        return nullptr;
    }

    static inline void ttStore(std::uint64_t key, int depthRemaining, double value, TTFlag flag, const AIMove& best) {
        TTEntry& e = g_tt[ttIndex(key)];
        const bool same = (e.gen == g_ttGen && e.key == key);
        if (!same || e.depthRemaining <= depthRemaining) {
            e.key = key;
            e.gen = g_ttGen;
            e.depthRemaining = depthRemaining;
            e.value = value;
            e.flag = flag;
            e.best = best;
        }
    }


    // ---------------------------
    // Negamax Alpha-Beta + Quiescence + PVS (internal)
    // Returns value in "color-space": (color * value_from_ai_perspective).
    // color = +1 when side-to-move == aiColor, -1 otherwise.
    // The public minimaxAlphaBetaQ() wrapper converts back to ai-space.
    // ---------------------------
    static double negamaxAlphaBetaQ_impl(Game game,
                                        int depth,
                                        int maxDepth,
                                        int qDepthLeft,
                                        double alpha,
                                        double beta,
                                        int color,
                                        PieceColor aiColor)
    {
        ++g_nodesVisited;
        if (searchPollShouldAbort()) {
            return (double)color * GoAI::evaluateBoardHeuristic(game, aiColor);
        }
        // Node budget is a safety net; time limit is the main control.
        g_budget.remaining--;
// Quiescence: if we're at the leaf but tactically unstable, extend by 1 ply (limited).
        int effectiveMaxDepth = maxDepth;
        bool usedQ = false;
        if (depth >= maxDepth) {
            if (qDepthLeft > 0 && isNoisyForQuiescence(game)) {
                effectiveMaxDepth = maxDepth + 1;
                usedQ = true;
            } else {
                return (double)color * GoAI::evaluateBoardHeuristic(game, aiColor);
            }
        }
        const int nextQ = qDepthLeft - (usedQ ? 1 : 0);
        const int depthRemaining = effectiveMaxDepth - depth;

        // --- TT lookup (values stored in color-space) ---
        const std::uint64_t key = ttKey(game);
        TTEntry* tte = ttProbe(key);
        if (tte && tte->depthRemaining >= depthRemaining) {
            const TTEntry& e = *tte;
            if (e.flag == TTFlag::EXACT) return e.value;
            if (e.flag == TTFlag::LOWER) alpha = std::max(alpha, e.value);
            else if (e.flag == TTFlag::UPPER) beta = std::min(beta, e.value);
            if (alpha >= beta) return e.value;
        }

        const double alphaOrig = alpha;
        const double betaOrig  = beta;

        // Generate moves (beam-limited).
        // IMPORTANT: always try urgent tactical moves first at EVERY node,
        // then fill the remainder with normal candidate moves (deduped).
        int limit = 30;
        if (depth >= effectiveMaxDepth - 1) limit = 14;
        if (usedQ) limit = std::min(limit, 12);

        std::vector<AIMove> moves;
        moves.reserve(limit);

        std::array<uint8_t, kBoardSize * kBoardSize> seen{};
        seen.fill(0);

        // 1) Urgent tactical moves first.
        std::vector<AIMove> urgent = generateUrgentMoves(game);
        for (const AIMove& m : urgent) {
            if (m.isPass || m.x < 0) continue;
            const int id = idx(m.x, m.y);
            if (!seen[id]) {
                moves.push_back(m);
                seen[id] = 1;
                if ((int)moves.size() >= limit) break;
            }
        }

        // In quiescence: ONLY explore urgent moves.
        if (usedQ) {
            if (moves.empty()) return (double)color * GoAI::evaluateBoardHeuristic(game, aiColor);
        } else if ((int)moves.size() < limit) {
            // 2) Fill the rest with normal candidates.
            std::vector<AIMove> normal = GoAI::generateCandidateMoves(game);
            for (const AIMove& m : normal) {
                if (m.isPass || m.x < 0) continue;
                const int id = idx(m.x, m.y);
                if (!seen[id]) {
                    moves.push_back(m);
                    seen[id] = 1;
                    if ((int)moves.size() >= limit) break;
                }
            }
        }

        // Move ordering: TT best move first if present.
        AIMove ttBest(-1, -1, true);
        if (tte) ttBest = tte->best;
        if (!ttBest.isPass && ttBest.x >= 0) {
            for (std::size_t i = 0; i < moves.size(); ++i) {
                if (!moves[i].isPass && moves[i].x == ttBest.x && moves[i].y == ttBest.y) {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }

        // Additional ordering: killer + history. Keep TT move in front if present.
        const int ply = depth; // ply from root (root children start at depth=1)
        std::size_t startIdx = 0;
        if (!ttBest.isPass && ttBest.x >= 0 && !moves.empty() &&
            !moves[0].isPass && moves[0].x == ttBest.x && moves[0].y == ttBest.y) {
            startIdx = 1;
        }

        if (ply >= 0 && ply < kMaxPly) {
            const AIMove& k1 = g_killer[ply * 2 + 0];
            const AIMove& k2 = g_killer[ply * 2 + 1];

            if (startIdx < moves.size()) {
                if (promoteMove(moves, k1, startIdx)) ++startIdx;
                if (startIdx < moves.size()) {
                    if (promoteMove(moves, k2, startIdx)) ++startIdx;
                }
            }

            if (startIdx + 1 < moves.size()) {
                std::stable_sort(moves.begin() + (long long)startIdx, moves.end(),
                    [](const AIMove& a, const AIMove& b) {
                        return moveHistoryScore(a) > moveHistoryScore(b);
                    });
            }
        }

        const bool quietNode = (!usedQ) && urgent.empty();

        const bool allowPass = (!usedQ) && (countEmpty(game) <= 40);
        constexpr double PASS_PENALTY = 0.15;

        auto recordCutoff = [&](const AIMove& m) {
            if (m.isPass || m.x < 0) return;
            const int id = idx(m.x, m.y);
            // Prefer deeper cutoffs.
            g_history[id] += depthRemaining * depthRemaining;

            if (ply >= 0 && ply < kMaxPly) {
                AIMove& k1 = g_killer[ply * 2 + 0];
                AIMove& k2 = g_killer[ply * 2 + 1];
                if (!sameMove(k1, m)) {
                    k2 = k1;
                    k1 = m;
                }
            }
        };

        double bestValue = -std::numeric_limits<double>::infinity();
        AIMove bestMove(-1, -1, true);
        bool anyValidChild = false;

        // PVS epsilon (small, but not too small to avoid re-searching due to floating noise).
        constexpr double kPvsEps = 0.01;

        bool searchedFirst = false;

        // Consider PASS first (late game).
        if (allowPass) {
            Game child = game;
            (void)child.pass();
            double score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                                  -beta, -alpha, -color, aiColor);
            // Discourage AI from passing too early (same behavior as before).
            if (color == +1) score -= PASS_PENALTY;

            anyValidChild = true;
            searchedFirst = true;

            if (score > bestValue) { bestValue = score; bestMove = AIMove(-1, -1, true); }
            alpha = std::max(alpha, bestValue);
        }

        // Search stone moves.
        for (std::size_t i = 0; i < moves.size(); ++i) {
    const AIMove& m = moves[i];

    // Late Move Pruning (very light):
    // In quiet nodes at sufficiently large remaining depth, the tail of a well-ordered move list
    // almost never matters. Keep it conservative to avoid tactical weakening.
    if (quietNode && depthRemaining >= 4 && i >= 18) break;

    Game child = game;
    if (!child.placeStone(m.x, m.y)) continue;
    anyValidChild = true;

    double score;

    const bool canPvs = searchedFirst && std::isfinite(alpha) && std::isfinite(beta);

    // Late Move Reductions (LMR):
    // Reduce depth a bit for late, quiet moves. If the reduced search beats alpha, re-search full depth.
    int reduction = 0;
    if (quietNode && depthRemaining >= 3) {
        if (i >= 8)  reduction = 1;
        if (i >= 14 && depthRemaining >= 4) reduction = 2;
    }

    if (!searchedFirst) {
        // First searched move: full window.
        score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                       -beta, -alpha, -color, aiColor);
        searchedFirst = true;
    } else if (canPvs) {
        if (reduction > 0) {
            const int reducedMax = std::max(depth + 1, effectiveMaxDepth - reduction);

            // Reduced-depth null-window search.
            score = -negamaxAlphaBetaQ_impl( child, depth + 1, reducedMax, nextQ,
                                           -(alpha + kPvsEps), -alpha, -color, aiColor);

            if (score > alpha) {
                // If promising, do normal PVS at full depth.
                score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                               -(alpha + kPvsEps), -alpha, -color, aiColor);

                if (score > alpha && score < beta) {
                    score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                                   -beta, -alpha, -color, aiColor);
                }
            }
        } else {
            // PVS: null-window search first.
            score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                           -(alpha + kPvsEps), -alpha, -color, aiColor);
            if (score > alpha && score < beta) {
                // Re-search with full window if it looks promising.
                score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                               -beta, -alpha, -color, aiColor);
            }
        }
    } else {
        // Fallback: full window.
        score = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                       -beta, -alpha, -color, aiColor);
    }

    if (score > bestValue) { bestValue = score; bestMove = m; bestMove.isPass = false; }
    alpha = std::max(alpha, bestValue);

    if (alpha >= beta) { recordCutoff(m); break; }
}

        // If no legal stone move and pass wasn't enabled, pass as a fallback.
        if (!anyValidChild) {
            if (usedQ) return (double)color * GoAI::evaluateBoardHeuristic(game, aiColor);

            Game child = game;
            (void)child.pass();
            bestValue = -negamaxAlphaBetaQ_impl( child, depth + 1, effectiveMaxDepth, nextQ,
                                               -beta, -alpha, -color, aiColor);
            if (color == +1) bestValue -= PASS_PENALTY;
            bestMove = AIMove(-1, -1, true);
        }

        // Store to TT
        TTFlag f;
        if (bestValue <= alphaOrig) f = TTFlag::UPPER;
        else if (bestValue >= betaOrig) f = TTFlag::LOWER;
        else f = TTFlag::EXACT;

        ttStore(key, depthRemaining, bestValue, f, bestMove);
        return bestValue;
    }

}

double GoAI::evaluateBoardHeuristic(const Game& game, PieceColor aiColor)
{
    // If the game already ended (two passes), use the real scoring for maximum accuracy.
    if (game.ended()) {
        auto [bs, ws] = game.calculateFinalScore(6.5f);
        double diff = static_cast<double>(bs - ws);
        return (aiColor == BLACK) ? diff : -diff;
    }

    // --- Tactical features (old heuristic, but down-weighted late game) ---
    PieceColor oppColor = game.oppositeColor(aiColor);

    g_seenStone.ensureSize(kBoardSize * kBoardSize);
    const int visitedTok = g_seenStone.nextToken();

    int aiStones = 0, oppStones = 0;
    int aiLib = 0, oppLib = 0;
    int aiAtariStones = 0;
    int oppAtariStones = 0;

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            PieceColor p = game.getPiece(x, y);
            if (p == NONE) continue;

            const int id = idx(x, y);
            if (g_seenStone.isMarked(id, visitedTok)) continue;

            GroupInfo gi = analyzeGroupFrom(game, x, y, visitedTok);
            if (gi.color == aiColor) {
                aiStones += gi.stones;
                aiLib    += gi.liberties;
                if (gi.atari) aiAtariStones += gi.stones;
            } else if (gi.color == oppColor) {
                oppStones += gi.stones;
                oppLib    += gi.liberties;
                if (gi.atari) oppAtariStones += gi.stones;
            }
        }
    }

    const double W_STONE = 1.0;
    const double W_LIB   = 0.35;
    const double W_ATARI = 0.70;

    double tactical =
        (aiStones - oppStones) * W_STONE +
        (aiLib    - oppLib)    * W_LIB +
        (oppAtariStones - aiAtariStones) * W_ATARI;

    // --- Score-aligned estimate (Japanese-style): territory + captures (+ komi to White) ---
    const int empty = countEmpty(game);
    const double phase = 1.0 - (double)empty / (kBoardSize * kBoardSize); // 0 early -> 1 late

    auto [bTerr, wTerr] = estimateTerritory(game);

    const int bCaps = game.getBlackCaptures();
    const int wCaps = game.getWhiteCaptures();
    constexpr double KOMI = 6.5;

    double baseDiffBlack =
        (double)(bTerr + bCaps) - (double)(wTerr + wCaps) - KOMI;

    double base = (aiColor == BLACK) ? baseDiffBlack : -baseDiffBlack;

    // Blend: early game rely more on tactical stability; late game rely more on score estimate.
    const double baseW = 0.25 + 0.75 * phase;
    const double tactW = 0.35 * (1.0 - phase);

    return baseW * base + tactW * tactical;
}


std::vector<AIMove> GoAI::generateCandidateMoves(const Game& game)
{
    struct ScoredMove { AIMove m; int s; };
    std::vector<ScoredMove> scored;

    // Nếu bàn trống: ưu tiên 4-4 (corner) thay vì tengen để AI nhìn "đúng Go" hơn.
bool anyStone = false;
for (int y = 0; y < kBoardSize && !anyStone; ++y) {
    for (int x = 0; x < kBoardSize; ++x) {
        if (game.getPiece(x, y) != NONE) { anyStone = true; break; }
    }
}
if (!anyStone) {
    static const std::pair<int,int> corners[] = {
        {3,3}, {3,15}, {15,3}, {15,15}
    };
    std::uniform_int_distribution<int> pick(0, 3);
    auto [x, y] = corners[pick(globalRng())];
    return { AIMove(x, y, false) };
}

const PieceColor aiColor  = game.getTurn();
    const PieceColor oppColor = game.oppositeColor(aiColor);

    const int stonesNow = countStones(game);

    // Dynamic local radius (candidate generator bottleneck):
    // opening R=3, mid R=2, late R=1.
    int R = 2;
    if (stonesNow <= 30) R = 3;
    else if (stonesNow <= 180) R = 2;
    else R = 1;
    std::array<uint8_t, kBoardSize * kBoardSize> candMask{};
    candMask.fill(0);

    auto tryAdd = [&](int x, int y) {
        if (!kinBounds(x, y)) return;
        if (game.getPiece(x, y) != NONE) return;
        candMask[idx(x, y)] = 1;
    };

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
        for (int y = 0; y < kBoardSize; ++y)
            for (int x = 0; x < kBoardSize; ++x)
                if (game.getPiece(x, y) == NONE) candMask[idx(x,y)] = 1;
    }

    // ---- Global candidates (tenuki / big points) ----
    // Ý tưởng: candidate hiện tại quá "local" (chỉ quanh nhóm đang giao tranh) -> AI hay bỏ lỡ nước lớn ở xa.
    // Ta thêm một ít điểm trống "xa quân" dựa trên khoảng cách Manhattan tới quân gần nhất.
    std::array<int16_t, kBoardSize * kBoardSize> distToStone;
    distToStone.fill(-1);

    std::array<int, kBoardSize * kBoardSize> qDist;
    int qhDist = 0, qtDist = 0;

    static const int ddx4[4] = {-1, 1, 0, 0};
    static const int ddy4[4] = {0, 0, -1, 1};

    int blackSt = 0, whiteSt = 0;

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            const PieceColor p0 = game.getPiece(x, y);
            if (p0 == NONE) continue;
            if (p0 == BLACK) ++blackSt;
            else if (p0 == WHITE) ++whiteSt;

            const int id = idx(x, y);
            distToStone[id] = 0;
            qDist[qtDist++] = id;
        }
    }

    while (qhDist < qtDist) {
        const int id = qDist[qhDist++];
        const int cx = id % kBoardSize;
        const int cy = id / kBoardSize;

        for (int k = 0; k < 4; ++k) {
            const int nx = cx + ddx4[k];
            const int ny = cy + ddy4[k];
            if (!kinBounds(nx, ny)) continue;
            const int nid = idx(nx, ny);
            if (distToStone[nid] != -1) continue;
            distToStone[nid] = static_cast<int16_t>(distToStone[id] + 1);
            qDist[qtDist++] = nid;
        }
    }

// Per-color distance (influence proxy) for "big moves" on territory boundary / contested areas.
// We keep it simple: BFS Manhattan distance to nearest BLACK / WHITE stone.
std::array<int16_t, kBoardSize * kBoardSize> distToBlack;
std::array<int16_t, kBoardSize * kBoardSize> distToWhite;
distToBlack.fill(-1);
distToWhite.fill(-1);

auto buildDistColor = [&](PieceColor c, std::array<int16_t, kBoardSize * kBoardSize>& dist) {
    std::array<int, kBoardSize * kBoardSize> q;
    int qh = 0, qt = 0;

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (game.getPiece(x, y) != c) continue;
            const int id = idx(x, y);
            dist[id] = 0;
            q[qt++] = id;
        }
    }

    while (qh < qt) {
        const int id = q[qh++];
        const int cx = id % kBoardSize;
        const int cy = id / kBoardSize;

        for (int k = 0; k < 4; ++k) {
            const int nx = cx + ddx4[k];
            const int ny = cy + ddy4[k];
            if (!kinBounds(nx, ny)) continue;
            const int nid = idx(nx, ny);
            if (dist[nid] != -1) continue;
            dist[nid] = static_cast<int16_t>(dist[id] + 1);
            q[qt++] = nid;
        }
    }
};

if (blackSt > 0) buildDistColor(BLACK, distToBlack);
if (whiteSt > 0) buildDistColor(WHITE, distToWhite);

    // Nếu candidate set quá nhỏ/local, thêm một số điểm "global" dựa trên distToStone.
    if (candCount > 0 && candCount < 90) {
        int extraGlobal = 0;
        if (stonesNow <= 24) extraGlobal = 18;
        else if (stonesNow <= 60) extraGlobal = 12;
        else if (stonesNow <= 120) extraGlobal = 8;

        if (extraGlobal > 0) {
            struct Glob { int id; int score; };
            std::vector<Glob> glob;
            glob.reserve(kBoardSize * kBoardSize);

            const int c = kBoardSize / 2;
            for (int y = 0; y < kBoardSize; ++y) {
                for (int x = 0; x < kBoardSize; ++x) {
                    const int id = idx(x, y);
                    if (candMask[id]) continue;
                    if (game.getPiece(x, y) != NONE) continue;

                    int d = distToStone[id];
                    if (d < 0) d = 0;

                    const int centerDist = std::abs(x - c) + std::abs(y - c);
                    const int toEdge = std::min(std::min(x, y), std::min(kBoardSize - 1 - x, kBoardSize - 1 - y));

                    // "big point" bias: prefer far-from-stones, and also slightly prefer corners/sides and center over 2nd-line.
                    const int farScore = 8 * d;
                    const int centerBonus = std::max(-3, 6 - centerDist / 2);
                    // Prefer "big points" near corner frameworks (around 4-4 / 3-4 / 4-3),
                    // but strongly avoid 1st/2nd line in the opening.
                    const int cx1 = 3, cx2 = kBoardSize - 4;
                    const int cy1 = 3, cy2 = kBoardSize - 4;
                    const int cornerDist =
                        std::min(std::min(std::abs(x - cx1) + std::abs(y - cy1),
                                          std::abs(x - cx1) + std::abs(y - cy2)),
                                 std::min(std::abs(x - cx2) + std::abs(y - cy1),
                                          std::abs(x - cx2) + std::abs(y - cy2)));
                    const int cornerBonus = std::max(0, 10 - cornerDist);
                    
                    const int firstLinePenalty = (toEdge == 0 ? 12 : (toEdge == 1 ? 5 : 0));
                    const int score = farScore + centerBonus + cornerBonus - firstLinePenalty;

                    glob.push_back({ id, score });
                }
            }

            std::sort(glob.begin(), glob.end(), [](const Glob& a, const Glob& b) { return a.score > b.score; });

            const int take = std::min<int>(extraGlobal, (int)glob.size());
            for (int i = 0; i < take; ++i) {
                candMask[glob[i].id] = 1;
            }
        }
    }

// ---- Boundary / influence big moves (territory boundary / contested area) ----
// Add a small set of candidates near the "influence border" where distToBlack ~= distToWhite.
// This helps the AI notice big, strategic moves (tenuki into the main fight boundary)
// without exploding branching.
if (blackSt > 0 && whiteSt > 0) {
    int extraBoundary = 0;
    if (stonesNow <= 40) extraBoundary = 8;
    else if (stonesNow <= 160) extraBoundary = 12;
    else extraBoundary = 6;

    if (extraBoundary > 0) {
        struct Bound { int id; int score; };
        std::vector<Bound> bound;
        bound.reserve(kBoardSize * kBoardSize);

        for (int y = 0; y < kBoardSize; ++y) {
            for (int x = 0; x < kBoardSize; ++x) {
                const int id = idx(x, y);
                if (candMask[id]) continue;
                if (game.getPiece(x, y) != NONE) continue;

                const int dB = distToBlack[id];
                const int dW = distToWhite[id];
                if (dB < 0 || dW < 0) continue;

                const int diff = std::abs(dB - dW);
                const int md = std::min(dB, dW);

                // Prefer points not too far from both sides, and close to the boundary.
                if (md > 10) continue;

                int s = 40 - diff * 10 - md * 2;

                // Mild opening penalty for 1st/2nd line (avoid tiny endgame-ish moves too early).
                if (stonesNow <= 80) {
                    const int toEdge = std::min(std::min(x, y),
                                                std::min(kBoardSize - 1 - x, kBoardSize - 1 - y));
                    if (toEdge == 0) s -= 10;
                    else if (toEdge == 1) s -= 4;
                }

                if (s > 0) bound.push_back({ id, s });
            }
        }

        std::sort(bound.begin(), bound.end(),
                  [](const Bound& a, const Bound& b) { return a.score > b.score; });

        const int take = std::min<int>(extraBoundary, (int)bound.size());
        for (int i = 0; i < take; ++i) {
            candMask[bound[i].id] = 1;
        }
    }
}

    //    - liberties: để check atari
    //    - stones: để thưởng capture theo số quân có thể bắt/cứu
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
            auto [x, y] = q.front();
            q.pop();

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx4[dir];
                int ny = y + dy4[dir];
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


    // ------------------------------------------------------------
    // "Don't be stupid" filters (cheap anti-blunder heuristics)
    //  1) Avoid filling our own (simple) true eye unless it's a capture / urgent save.
    //  2) Penalize obvious self-atari (new group ends with 1 liberty) unless it's a direct save.
    //
    // Notes:
    //  - We keep these checks lightweight so Medium(1s)/Hard(3s) can still search deep.
    //  - We do NOT attempt full life&death; just filter the most common blunders.
    // ------------------------------------------------------------

    auto isSimpleTrueEye = [&](int x, int y) -> bool {
        // Conservative "true-eye" test:
        //   - All 4 orth neighbors are our stones or off-board
        //   - No opponent stones on diagonals (avoids false-eye cases)
        static const int dxD[4] = { -1, 1, -1, 1 };
        static const int dyD[4] = { -1, -1, 1, 1 };

        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx4[d];
            const int ny = y + dy4[d];
            if (!kinBounds(nx, ny)) continue; // edge counts as our "wall"
            if (game.getPiece(nx, ny) != aiColor) return false;
        }

        for (int d = 0; d < 4; ++d) {
            const int nx = x + dxD[d];
            const int ny = y + dyD[d];
            if (!kinBounds(nx, ny)) continue; // off-board diagonal is fine
            if (game.getPiece(nx, ny) == oppColor) return false;
        }
        return true;
    };

    auto virtualGroupLibertiesAfterPlace = [&](int sx, int sy) -> int {
        // Count liberties of the connected component if we place aiColor at (sx, sy),
        // without copying Game (treat (sx, sy) as occupied by aiColor).
        const int vTok = g_tmpVisited.nextToken();
        const int lTok = g_seenLib.nextToken();

        std::queue<std::pair<int, int>> q;
        q.push({ sx, sy });
        g_tmpVisited.setMarked(idx(sx, sy), vTok);

        int liberties = 0;

        while (!q.empty()) {
            auto [x, y] = q.front();
            q.pop();

            for (int dir = 0; dir < 4; ++dir) {
                const int nx = x + dx4[dir];
                const int ny = y + dy4[dir];
                if (!kinBounds(nx, ny)) continue;

                // (sx, sy) is the virtual stone -> never count it as a liberty.
                if (nx == sx && ny == sy) continue;

                const PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    const int lid = idx(nx, ny);
                    if (!g_seenLib.isMarked(lid, lTok)) {
                        g_seenLib.setMarked(lid, lTok);
                        liberties++;
                    }
                } else if (p == aiColor) {
                    const int nid = idx(nx, ny);
                    if (!g_tmpVisited.isMarked(nid, vTok)) {
                        g_tmpVisited.setMarked(nid, vTok);
                        q.push({ nx, ny });
                    }
                }
            }
        }

        return liberties;
    };

    
    scored.reserve(candCount);

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (!candMask[idx(x,y)]) continue;

            const int cid = idx(x, y);
            int score = 0;

            // Global expansion bias (opening/mid): reward "big points" away from current clusters.
            int d = distToStone[cid];
            if (d < 0) d = 0;
            if (stonesNow <= 120) {
                const int toEdge = std::min(std::min(x, y),
                                            std::min(kBoardSize - 1 - x, kBoardSize - 1 - y));
                int bonus = std::min(8, d);
                if (toEdge == 0) bonus -= 6;       // 1st line is usually bad early
                else if (toEdge == 1) bonus -= 3;  // 2nd line also often small
                score += bonus;
            }

            if (stonesNow <= 10) {
                if ((x == 3 || x == 15) && (y == 3 || y == 15)) score += 6; // 4-4
                if ((x == 3 || x == 15) && y == 9) score += 3;
                if ((y == 3 || y == 15) && x == 9) score += 3;
                if (x == 9 && y == 9) score += 4;
            }


// Territory boundary / contested influence bonus:
// reward points close to where BLACK/WHITE influence is balanced (distToBlack ~= distToWhite).
if (blackSt > 0 && whiteSt > 0) {
    const int dB = distToBlack[cid];
    const int dW = distToWhite[cid];
    if (dB >= 0 && dW >= 0) {
        const int diff = std::abs(dB - dW);
        const int md = std::min(dB, dW);
        if (md <= 9) {
            const int boundaryB = std::max(0, 14 - diff * 5) + std::max(0, 8 - md);
            score += boundaryB;
        }
    }
}

            int adjEmpty = 0;
            bool wouldCapture = false;
            bool directSave = false;

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx4[dir];
                int ny = y + dy4[dir];
                if (!kinBounds(nx, ny)) continue;

                PieceColor p = game.getPiece(nx, ny);
                if (p == NONE) {
                    adjEmpty++;
                    score += 1;
                } else if (p == oppColor) {
                    // nếu group địch đang ở atari và (x,y) kề nó => có khả năng là liberty cuối => bắt
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) {
                        wouldCapture = true;
                        score += 30 + 3 * stones;
                    }
                } else if (p == aiColor) {
                    // cứu group mình ở atari
                    auto [libs, stones] = groupStatsCached(nx, ny);
                    if (libs == 1) {
                        directSave = true;
                        score += 18 + 2 * stones;
                    }
                    // nối group
                    score += 3;
                }
            }

            // --- "Don't be stupid" filters ---
            // 1) Avoid filling our own true eye (most common blunder).
            // 2) Penalize obvious self-atari (new group ends with 1 liberty) unless it's a direct save.
            if (!wouldCapture) {
                if (isSimpleTrueEye(x, y)) {
                    score -= directSave ? 40 : 120;
                }

                // Only run the heavier self-atari check on risky points.
                if (adjEmpty <= 1) {
                    const int vLib = virtualGroupLibertiesAfterPlace(x, y);

                    // vLib == 0 => suicide (unless capture, which is excluded here).
                    if (vLib == 0) continue;

                    if (vLib == 1) {
                        // Keep it in the list (throw-ins exist), but rank it very low.
                        score -= directSave ? 20 : 70;
                    }
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
    // Wrapper: Hard uses 1-ply quiescence extension in noisy positions.
    return minimaxAlphaBetaQ(std::move(game), depth, maxDepth, /*qDepthLeft=*/2, alpha, beta, maximizingPlayer, aiColor);
}

double GoAI::minimaxAlphaBetaQ(Game game,
                               int depth,
                               int maxDepth,
                               int qDepthLeft,
                               double alpha,
                               double beta,
                               bool maximizingPlayer,
                               PieceColor aiColor)
{
    // Convert alpha/beta (ai-space) to side-to-move "color-space" for negamax.
    // For color=-1, the interval [alpha, beta] becomes [-beta, -alpha].
    const int color = maximizingPlayer ? +1 : -1;
    const double alphaC = (color == 1) ? alpha : -beta;
    const double betaC  = (color == 1) ? beta  : -alpha;

    const double vC = negamaxAlphaBetaQ_impl( std::move(game), depth, maxDepth, qDepthLeft, alphaC, betaC, color, aiColor);

    // Convert back to ai-space.
    return (double)color * vC;
}



AIMove GoAI::computeAIMove(const Game& game, AIDifficulty difficulty)
{
    const PieceColor aiColor = game.getTurn();
    const PieceColor oppColor = game.oppositeColor(aiColor);
    const int baseBlackCaptures = game.getBlackCaptures();
    const int baseWhiteCaptures = game.getWhiteCaptures();

    std::vector<AIMove> candidates;

    if (difficulty == AIDifficulty::HARD) {
        candidates = generateUrgentMoves(game);
        if (!candidates.empty()) {
            constexpr int EXTRA = 10;
            std::vector<AIMove> normal = generateCandidateMoves(game);
            std::array<uint8_t, kBoardSize * kBoardSize> seen{};
            seen.fill(0);
            for (const AIMove& um : candidates) {
                if (!um.isPass && um.x >= 0) seen[idx(um.x, um.y)] = 1;
            }
            int added = 0;
            for (const AIMove& nm : normal) {
                if (added >= EXTRA) break;
                if (nm.isPass || nm.x < 0) continue;
                const int id = idx(nm.x, nm.y);
                if (seen[id]) continue;
                seen[id] = 1;
                candidates.push_back(nm);
                ++added;
            }
        } else {
            candidates = generateCandidateMoves(game);
        }
    } else {
        candidates = generateCandidateMoves(game);
    }

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


    // ------------------------------------------------------------
    // Root anti-blunder post-filter
    // Urgent moves bypass generateCandidateMoves() scoring filters, so we
    // re-check a few cheap patterns at the root to avoid wasting beam slots
    // on obvious self-atari / filling true eyes.
    // ------------------------------------------------------------
    auto rootDidCapture = [&](const Game& after) -> bool {
        if (aiColor == BLACK) return after.getBlackCaptures() > baseBlackCaptures;
        return after.getWhiteCaptures() > baseWhiteCaptures;
    };

    auto rootIsSimpleTrueEye = [&](int x, int y) -> bool {
        // Conservative "true-eye" test:
        //   - All 4 orth neighbors are our stones or off-board
        //   - No opponent stones on diagonals (avoids common false-eye cases)
        static const int dx4_[4] = { -1, 1, 0, 0 };
        static const int dy4_[4] = { 0, 0, -1, 1 };
        static const int dxD[4]  = { -1, 1, -1, 1 };
        static const int dyD[4]  = { -1, -1, 1, 1 };

        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx4_[d];
            const int ny = y + dy4_[d];
            if (!kinBounds(nx, ny)) continue; // edge counts as our "wall"
            if (game.getPiece(nx, ny) != aiColor) return false;
        }
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dxD[d];
            const int ny = y + dyD[d];
            if (!kinBounds(nx, ny)) continue;
            if (game.getPiece(nx, ny) == oppColor) return false;
        }
        return true;
    };

    auto rootGroupLiberties = [&](const Game& g, int sx, int sy) -> int {
        if (!kinBounds(sx, sy)) return 0;
        if (g.getPiece(sx, sy) == NONE) return 0;

        g_seenStone.ensureSize(kBoardSize * kBoardSize);
        const int visitedTok = g_seenStone.nextToken();
        GroupInfo gi = analyzeGroupFrom(g, sx, sy, visitedTok);
        return gi.liberties;
    };

    auto rootIsDirectSave = [&](int x, int y) -> bool {
        // If this point is a liberty of any adjacent atari group (ours), it's a direct save.
        static const int dx4_[4] = { -1, 1, 0, 0 };
        static const int dy4_[4] = { 0, 0, -1, 1 };
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx4_[d];
            const int ny = y + dy4_[d];
            if (!kinBounds(nx, ny)) continue;
            if (game.getPiece(nx, ny) != aiColor) continue;
            if (rootGroupLiberties(game, nx, ny) == 1) return true;
        }
        return false;
    };


// Time budget per move (no UI dependency).
// - HARD: up to 3 seconds
// - MEDIUM: up to 1 second
int timeLimitMs = 0;
if (difficulty == AIDifficulty::MEDIUM) timeLimitMs = 1000;
else if (difficulty == AIDifficulty::HARD) timeLimitMs = 3000;

// Initialize the per-move deadline + reset abort flag/counters.
searchBeginTimeLimitMs(timeLimitMs);

// Depth cap (search will stop early if time runs out).
int maxDepth = (difficulty == AIDifficulty::MEDIUM) ? 4 : 6;
const int emptyNowForDepth = countEmpty(game);

// Deeper reading where it matters:
// - tactical positions (atari / near-atari / capture threats)
// - later game where branching is naturally smaller.
const bool tacticalRoot = hasAnyAtariGroup(game) || isNoisyForQuiescence(game);

if (difficulty == AIDifficulty::HARD) {
    if (tacticalRoot || emptyNowForDepth <= 140) maxDepth = std::max(maxDepth, 7);
    if (emptyNowForDepth <=  90) maxDepth = std::max(maxDepth, 8);
    if (emptyNowForDepth <=  50) maxDepth = std::max(maxDepth, 9);
    maxDepth = std::min(maxDepth, 9);
} else { // MEDIUM
    if (tacticalRoot || emptyNowForDepth <= 120) maxDepth = std::max(maxDepth, 5);
    if (emptyNowForDepth <=  70) maxDepth = std::max(maxDepth, 6);
    maxDepth = std::min(maxDepth, 6);
}

// Node budget as a safety net; time limit is the main control.
if (difficulty == AIDifficulty::MEDIUM) g_budget.remaining = 80000000;
else g_budget.remaining = 200000000;


    // HARD: Iterative deepening + aspiration (root) + PVS inside negamax.
    // This improves alpha-beta pruning depth significantly while keeping time reasonable.
    if (difficulty == AIDifficulty::HARD) {
        ttNewSearch();
        orderingNewSearch();

        const int ROOT_LIMIT = (maxDepth >= 4 ? 28 : 40);

        // Build a legal root move list (beam-limited).
        // Root-level anti-blunder demotion so urgent moves don't bypass the
        // generateCandidateMoves() "don't be stupid" filters.
        std::vector<AIMove> rootMovesGood;
        std::vector<AIMove> rootMovesBad;
        rootMovesGood.reserve(ROOT_LIMIT);
        rootMovesBad.reserve(ROOT_LIMIT);

        for (const AIMove& m : candidates) {
            Game test = game;
            if (!test.placeStone(m.x, m.y)) continue;

            bool demote = false;
            const bool didCapture = rootDidCapture(test);

            if (!didCapture) {
                // Cheap adjacency features on the pre-move board.
                int adjEmpty = 0;
                bool hasOwnAdj = false;
                static const int dx4_[4] = { -1, 1, 0, 0 };
                static const int dy4_[4] = { 0, 0, -1, 1 };
                for (int d = 0; d < 4; ++d) {
                    const int nx = m.x + dx4_[d];
                    const int ny = m.y + dy4_[d];
                    if (!kinBounds(nx, ny)) continue;
                    const PieceColor p = game.getPiece(nx, ny);
                    if (p == NONE) adjEmpty++;
                    else if (p == aiColor) hasOwnAdj = true;
                }

                bool directSave = false;

                // 1) Filling a true eye is almost always bad unless saving an atari group.
                if (rootIsSimpleTrueEye(m.x, m.y)) {
                    directSave = hasOwnAdj ? rootIsDirectSave(m.x, m.y) : false;
                    if (!directSave) demote = true;
                }

                // 2) Obvious self-atari (new group ends in atari) is usually bad unless direct save.
                if (!demote && adjEmpty <= 1) {
                    const int libsNew = rootGroupLiberties(test, m.x, m.y);
                    if (libsNew == 1) {
                        if (!directSave) directSave = hasOwnAdj ? rootIsDirectSave(m.x, m.y) : false;
                        if (!directSave) demote = true;
                    }
                }
            }

            const AIMove pushed(m.x, m.y, false);
            if (!demote) {
                if ((int)rootMovesGood.size() < ROOT_LIMIT) {
                    // If we already have demoted moves filling the beam, replace one.
                    if ((int)(rootMovesGood.size() + rootMovesBad.size()) >= ROOT_LIMIT && !rootMovesBad.empty()) {
                        rootMovesBad.pop_back();
                    }
                    rootMovesGood.push_back(pushed);
                }
            } else {
                if ((int)(rootMovesGood.size() + rootMovesBad.size()) < ROOT_LIMIT) {
                    rootMovesBad.push_back(pushed);
                }
            }

            if ((int)rootMovesGood.size() >= ROOT_LIMIT) break;
        }

        std::vector<AIMove> rootMoves;
        rootMoves.reserve(ROOT_LIMIT);
        for (const auto& m : rootMovesGood) rootMoves.push_back(m);
        for (const auto& m : rootMovesBad) {
            if ((int)rootMoves.size() >= ROOT_LIMIT) break;
            rootMoves.push_back(m);
        }

if (rootMoves.empty()) return AIMove(-1, -1, true);

        struct RootChoice { AIMove move; double score; };

        AIMove bestMove = rootMoves.front();
        double bestScore = -std::numeric_limits<double>::infinity();

        bool havePrev = false;
        double prevScore = 0.0;
        constexpr double kRootPvsEps = 0.01;

        std::vector<RootChoice> lastScores;
        lastScores.reserve(rootMoves.size());

        for (int d = 1; d <= maxDepth; ++d) {
            if (g_abortSearch || searchTimeUpNow()) break;

            // Aspiration window around previous depth's best (ai-space).
            double a0 = -std::numeric_limits<double>::infinity();
            double b0 = +std::numeric_limits<double>::infinity();
            if (havePrev) {
                const double delta = 2.5 + 0.8 * d; // safe window; widen as depth increases
                a0 = prevScore - delta;
                b0 = prevScore + delta;
            }

            double iterBestScore = -std::numeric_limits<double>::infinity();
            AIMove iterBestMove = rootMoves.front();

            bool accepted = false;
            for (int attempt = 0; attempt < 2 && !accepted; ++attempt) {
                double alpha = a0;
                double beta  = b0;

                iterBestScore = -std::numeric_limits<double>::infinity();
                iterBestMove = rootMoves.front();
                lastScores.clear();

                bool first = true;

                for (const AIMove& m : rootMoves) {
                    if (g_abortSearch || searchTimeUpNow()) break;

                    Game child = game;
                    if (!child.placeStone(m.x, m.y)) continue;

                    double score;
                    if (!first && std::isfinite(alpha) && std::isfinite(beta)) {
                        // Root PVS: null-window first, re-search full if promising.
                        score = minimaxAlphaBeta(child, 1, d, alpha, alpha + kRootPvsEps, false, aiColor);
                        if (score > alpha && score < beta) {
                            score = minimaxAlphaBeta(child, 1, d, alpha, beta, false, aiColor);
                        }
                    } else {
                        score = minimaxAlphaBeta(child, 1, d, alpha, beta, false, aiColor);
                    }

                    lastScores.push_back({ m, score });

                    if (score > iterBestScore) {
                        iterBestScore = score;
                        iterBestMove = m;
                    }

                    alpha = std::max(alpha, iterBestScore);
                    first = false;

                    // If we hit the aspiration beta, this is a fail-high; we will widen window below.
                    if (alpha >= beta) break;
                }

                if (!havePrev || (iterBestScore > a0 && iterBestScore < b0) || attempt == 1) {
                    accepted = true;
                } else {
                    // Widen to full window and retry once.
                    a0 = -std::numeric_limits<double>::infinity();
                    b0 = +std::numeric_limits<double>::infinity();
                }
            }

            // Reorder root moves for the next iteration (best-first).
            if (!lastScores.empty()) {
                std::stable_sort(lastScores.begin(), lastScores.end(),
                                 [](const RootChoice& A, const RootChoice& B) { return A.score > B.score; });

                rootMoves.clear();
                for (const auto& rc : lastScores) rootMoves.push_back(rc.move);

                // Ensure PV move is first.
                if (!rootMoves.empty() && !(rootMoves[0].x == iterBestMove.x && rootMoves[0].y == iterBestMove.y)) {
                    for (std::size_t i = 0; i < rootMoves.size(); ++i) {
                        if (rootMoves[i].x == iterBestMove.x && rootMoves[i].y == iterBestMove.y) {
                            std::swap(rootMoves[0], rootMoves[i]);
                            break;
                        }
                    }
                }
            }

            bestMove = iterBestMove;
            bestScore = iterBestScore;
            prevScore = iterBestScore;
            havePrev = true;

            // If budget is getting tight, stop early.
            if (g_abortSearch || searchTimeUpNow()) break;
        }

        // Late-game pass decision (kept from your previous logic).
        const int emptyNow = countEmpty(game);
        const bool lateGame = (emptyNow <= 60);

        if (lateGame) {
            const double passBias = (emptyNow <= 40) ? 0.0 : -1.5;
            double passScore = evaluateBoardHeuristic(game, aiColor) + passBias;

            if (passScore >= bestScore - 0.75) {
                return AIMove(-1, -1, true);
            }
        }

        return bestMove;
    }

    
// MEDIUM: use the same alpha-beta engine (but shallower + tighter beam) and respect the 1s deadline.
// This makes MEDIUM much stronger than pure minimax while still clearly weaker than HARD.
ttNewSearch();
orderingNewSearch();

const int ROOT_LIMIT = 70;

// Build a legal root move list (beam-limited).
// Root-level anti-blunder demotion so urgent moves don't bypass the
// generateCandidateMoves() "don't be stupid" filters.
std::vector<AIMove> rootMovesGood;
std::vector<AIMove> rootMovesBad;
rootMovesGood.reserve(ROOT_LIMIT);
rootMovesBad.reserve(ROOT_LIMIT);

for (const AIMove& m : candidates) {
    if (g_abortSearch || searchTimeUpNow()) break;

    Game test = game;
    if (!test.placeStone(m.x, m.y)) continue;

    bool demote = false;
    const bool didCapture = rootDidCapture(test);

    if (!didCapture) {
        // Cheap adjacency features on the pre-move board.
        int adjEmpty = 0;
        bool hasOwnAdj = false;
        static const int dx4_[4] = { -1, 1, 0, 0 };
        static const int dy4_[4] = { 0, 0, -1, 1 };
        for (int d = 0; d < 4; ++d) {
            const int nx = m.x + dx4_[d];
            const int ny = m.y + dy4_[d];
            if (!kinBounds(nx, ny)) continue;
            const PieceColor p = game.getPiece(nx, ny);
            if (p == NONE) adjEmpty++;
            else if (p == aiColor) hasOwnAdj = true;
        }

        bool directSave = false;

        // 1) Filling a true eye is almost always bad unless saving an atari group.
        if (rootIsSimpleTrueEye(m.x, m.y)) {
            directSave = hasOwnAdj ? rootIsDirectSave(m.x, m.y) : false;
            if (!directSave) demote = true;
        }

        // 2) Obvious self-atari (new group ends in atari) is usually bad unless direct save.
        if (!demote && adjEmpty <= 1) {
            const int libsNew = rootGroupLiberties(test, m.x, m.y);
            if (libsNew == 1) {
                if (!directSave) directSave = hasOwnAdj ? rootIsDirectSave(m.x, m.y) : false;
                if (!directSave) demote = true;
            }
        }
    }

    const AIMove pushed(m.x, m.y, false);
    if (!demote) {
        if ((int)rootMovesGood.size() < ROOT_LIMIT) {
            // If we already have demoted moves filling the beam, replace one.
            if ((int)(rootMovesGood.size() + rootMovesBad.size()) >= ROOT_LIMIT && !rootMovesBad.empty()) {
                rootMovesBad.pop_back();
            }
            rootMovesGood.push_back(pushed);
        }
    } else {
        if ((int)(rootMovesGood.size() + rootMovesBad.size()) < ROOT_LIMIT) {
            rootMovesBad.push_back(pushed);
        }
    }

    if ((int)rootMovesGood.size() >= ROOT_LIMIT) break;
}

std::vector<AIMove> rootMoves;
rootMoves.reserve(ROOT_LIMIT);
for (const auto& m : rootMovesGood) rootMoves.push_back(m);
for (const auto& m : rootMovesBad) {
    if ((int)rootMoves.size() >= ROOT_LIMIT) break;
    rootMoves.push_back(m);
}

if (rootMoves.empty()) return AIMove(-1, -1, true);

AIMove bestMove = rootMoves.front();
double bestScore = -std::numeric_limits<double>::infinity();

bool havePrev = false;
double prevScore = 0.0;
constexpr double kRootPvsEps = 0.01;

struct RootChoice { AIMove move; double score; };
std::vector<RootChoice> lastScores;
lastScores.reserve(rootMoves.size());

const int qDepthLeft = 1;

for (int d = 1; d <= maxDepth; ++d) {
    if (g_abortSearch || searchTimeUpNow()) break;

    // Aspiration window around previous depth's best.
    double a0 = -std::numeric_limits<double>::infinity();
    double b0 = +std::numeric_limits<double>::infinity();
    if (havePrev) {
        const double delta = 3.0 + 1.0 * d;
        a0 = prevScore - delta;
        b0 = prevScore + delta;
    }

    double iterBestScore = -std::numeric_limits<double>::infinity();
    AIMove iterBestMove = rootMoves.front();

    bool accepted = false;
    for (int attempt = 0; attempt < 2 && !accepted; ++attempt) {
        double alpha = a0;
        double beta  = b0;

        iterBestScore = -std::numeric_limits<double>::infinity();
        iterBestMove = rootMoves.front();
        lastScores.clear();

        bool first = true;

        for (const AIMove& m : rootMoves) {
            if (g_abortSearch || searchTimeUpNow()) break;

            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;

            double score;
            if (first) {
                score = minimaxAlphaBetaQ(child, 1, d, qDepthLeft, alpha, beta, false, aiColor);
                first = false;
            } else {
                // Root PVS: null-window first
                score = minimaxAlphaBetaQ(child, 1, d, qDepthLeft, alpha, alpha + kRootPvsEps, false, aiColor);
                if (score > alpha) {
                    score = minimaxAlphaBetaQ(child, 1, d, qDepthLeft, alpha, beta, false, aiColor);
                }
            }

            lastScores.push_back({ m, score });

            if (score > iterBestScore) {
                iterBestScore = score;
                iterBestMove = m;
            }

            alpha = std::max(alpha, iterBestScore);
            if (alpha >= beta) break;
        }

        if (g_abortSearch) break;

        // Accept if inside aspiration window, otherwise widen.
        if (!havePrev || (iterBestScore >= a0 && iterBestScore <= b0) || attempt == 1) {
            accepted = true;
        } else {
            a0 = -std::numeric_limits<double>::infinity();
            b0 = +std::numeric_limits<double>::infinity();
        }
    }

    if (g_abortSearch) break;

    if (accepted) {
        bestMove = iterBestMove;
        bestScore = iterBestScore;
        prevScore = iterBestScore;
        havePrev = true;
    }

    // Reorder root moves for the next iteration (best-first).
    if (!lastScores.empty()) {
        std::stable_sort(lastScores.begin(), lastScores.end(),
                         [](const RootChoice& A, const RootChoice& B) { return A.score > B.score; });

        rootMoves.clear();
        for (const auto& rc : lastScores) rootMoves.push_back(rc.move);

        // Ensure PV move is first.
        if (!rootMoves.empty() && !(rootMoves[0].x == bestMove.x && rootMoves[0].y == bestMove.y)) {
            for (std::size_t i = 0; i < rootMoves.size(); ++i) {
                if (rootMoves[i].x == bestMove.x && rootMoves[i].y == bestMove.y) {
                    std::swap(rootMoves[0], rootMoves[i]);
                    break;
                }
            }
        }
    }
}

// Late-game pass decision (kept from your previous logic).
const int emptyNow = countEmpty(game);
const bool lateGame = (emptyNow <= 60);

if (lateGame && std::isfinite(bestScore)) {
    const double passBias = (emptyNow <= 40) ? 0.0 : -1.5;
    double passScore = evaluateBoardHeuristic(game, aiColor) + passBias;

    if (passScore >= bestScore - 0.75) {
        return AIMove(-1, -1, true);
    }
}

return bestMove;
}
