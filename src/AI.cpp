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
        std::uint64_t h = g.getBoardHash();
        std::uint64_t k = g.hasKoRef() ? g.getKoRefHash() : 0ULL;
        h ^= rotl64(k * 0x9e3779b97f4a7c15ULL, 17);
        h ^= (g.getTurn() == BLACK ? 0xBADC0FFEE0DDF00DULL : 0xC001D00DCAFEBEEFULL);
        h ^= (g.hasKoRef() ? 0xA5A5A5A5A5A5A5A5ULL : 0ULL);
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

    constexpr int R = 2;
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

    for (int y = 0; y < kBoardSize; ++y) {
        for (int x = 0; x < kBoardSize; ++x) {
            if (game.getPiece(x, y) == NONE) continue;
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

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx4[dir];
                int ny = y + dy4[dir];
                if (!kinBounds(nx, ny)) continue;

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
    // Wrapper: Hard uses 1-ply quiescence extension in noisy positions.
    return minimaxAlphaBetaQ(std::move(game), depth, maxDepth, /*qDepthLeft=*/1, alpha, beta, maximizingPlayer, aiColor);
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
    if (g_budget.remaining <= 0) {
        return evaluateBoardHeuristic(game, aiColor);
    }
    g_budget.remaining--;

    // Quiescence: if we're at the leaf but the position is tactically unstable (atari exists),
    // extend by 1 ply (limited times).
    int effectiveMaxDepth = maxDepth;
    bool usedQ = false;
    if (depth >= maxDepth) {
        if (qDepthLeft > 0 && hasAnyAtariGroup(game)) {
            effectiveMaxDepth = maxDepth + 1;
            usedQ = true;
        } else {
            return evaluateBoardHeuristic(game, aiColor);
        }
    }
    const int nextQ = qDepthLeft - (usedQ ? 1 : 0);

    const int depthRemaining = effectiveMaxDepth - depth;

    // --- Transposition table lookup ---
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

    // Generate and limit candidates (beam) to keep search time reasonable.
    std::vector<AIMove> moves = usedQ ? generateUrgentMoves(game) : generateCandidateMoves(game);
    if (usedQ && moves.empty()) return evaluateBoardHeuristic(game, aiColor);

    int limit = 30;
    if (depth >= effectiveMaxDepth - 1) limit = 14;
    if (usedQ) limit = std::min(limit, 12);
    if ((int)moves.size() > limit) moves.resize(limit);

    // Move ordering: try TT best move first if present.
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

    const bool allowPass = (!usedQ) && (countEmpty(game) <= 40);
    constexpr double PASS_PENALTY = 0.15;

    bool anyValidChild = false;
    AIMove bestMove(-1, -1, true);

    if (maximizingPlayer) {
        double bestValue = -std::numeric_limits<double>::infinity();

        // Consider PASS (late game) and/or when TT suggests it.
        if (allowPass) {
            Game child = game;
            (void)child.pass();
            anyValidChild = true;
            double val = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, false, aiColor);
            val -= PASS_PENALTY;
            if (val > bestValue) { bestValue = val; bestMove = AIMove(-1, -1, true); }
            alpha = std::max(alpha, bestValue);
        }

        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, false, aiColor);
            if (val > bestValue) { bestValue = val; bestMove = m; bestMove.isPass = false; }

            alpha = std::max(alpha, bestValue);
            if (beta <= alpha) break;
        }

        // If no legal stone move and we didn't try pass earlier, pass as a fallback.
        if (!anyValidChild) {
            if (usedQ) return evaluateBoardHeuristic(game, aiColor);
Game child = game;
            (void)child.pass();
            bestValue = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, false, aiColor) - PASS_PENALTY;
            bestMove = AIMove(-1, -1, true);
            anyValidChild = true;
        }

        // Store to TT
TTFlag f;
if (bestValue <= alphaOrig) f = TTFlag::UPPER;
else if (bestValue >= betaOrig) f = TTFlag::LOWER;
else f = TTFlag::EXACT;

ttStore(key, depthRemaining, bestValue, f, bestMove);
return bestValue;
    } else {
        double bestValue = +std::numeric_limits<double>::infinity();

        // Consider PASS (late game)
        if (allowPass) {
            Game child = game;
            (void)child.pass();
            anyValidChild = true;
            double val = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, true, aiColor);
            // Do NOT penalize opponent pass; it can be correct when they are ahead.
            if (val < bestValue) { bestValue = val; bestMove = AIMove(-1, -1, true); }
            beta = std::min(beta, bestValue);
        }

        for (const AIMove& m : moves) {
            Game child = game;
            if (!child.placeStone(m.x, m.y)) continue;
            anyValidChild = true;

            double val = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, true, aiColor);
            if (val < bestValue) { bestValue = val; bestMove = m; bestMove.isPass = false; }

            beta = std::min(beta, bestValue);
            if (beta <= alpha) break;
        }
        if (!anyValidChild) {
            if (usedQ) return evaluateBoardHeuristic(game, aiColor);
            Game child = game;
            (void)child.pass();
            bestValue = minimaxAlphaBetaQ(child, depth + 1, effectiveMaxDepth, nextQ, alpha, beta, true, aiColor);
            bestMove = AIMove(-1, -1, true);
            anyValidChild = true;
        }
        TTFlag f;
if (bestValue <= alphaOrig) f = TTFlag::UPPER;
else if (bestValue >= betaOrig) f = TTFlag::LOWER;
else f = TTFlag::EXACT;

ttStore(key, depthRemaining, bestValue, f, bestMove);
return bestValue;
    }
}


AIMove GoAI::computeAIMove(const Game& game, AIDifficulty difficulty)
{
    const PieceColor aiColor = game.getTurn();

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

    // depth
    int maxDepth = (difficulty == AIDifficulty::MEDIUM) ? 2 : 3;

    // Budget
    g_budget.remaining = (difficulty == AIDifficulty::MEDIUM) ? 2500 : 12000;

    const bool useAlphaBeta = (difficulty == AIDifficulty::HARD);

    if (useAlphaBeta) {
        ttNewSearch();
    }

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
