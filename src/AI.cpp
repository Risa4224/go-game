#include "AI.h"
#include "game.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <limits>
#include <iostream>

namespace {
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

    constexpr int BOARD_SIZE = 19;
}

// ---------------------------------------------------------------------
// Heuristic: đếm số quân của AI - số quân của đối thủ (càng lớn càng tốt).
// ---------------------------------------------------------------------
double GoAI::evaluateBoardHeuristic(const Game& game, PieceColor aiColor)
{
    int aiStones  = 0;
    int oppStones = 0;

    PieceColor oppColor = game.oppositeColor(aiColor);

    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            PieceColor p = game.getPiece(x, y);
            if (p == aiColor)       ++aiStones;
            else if (p == oppColor) ++oppStones;
        }
    }

    return static_cast<double>(aiStones - oppStones);
}

// ---------------------------------------------------------------------
// Sinh các nước đi ứng viên cho trạng thái hiện tại.
// Nếu bàn trống -> chỉ return nước ở giữa.
// Ngược lại -> chỉ xét các ô trống kề (4 hướng) với quân đã có.
// ---------------------------------------------------------------------
std::vector<AIMove> GoAI::generateCandidateMoves(const Game& game)
{
    std::vector<AIMove> moves;
    bool anyStone = false;

    // Kiểm tra xem đã có quân nào trên bàn chưa
    for (int y = 0; y < BOARD_SIZE && !anyStone; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) != NONE) {
                anyStone = true;
                break;
            }
        }
    }

    // Bàn trống: đi ở giữa
    if (!anyStone) {
        int c = BOARD_SIZE / 2;
        moves.emplace_back(c, c, false);
        return moves;
    }

    auto hasNeighbourStone = [&game](int x, int y) -> bool {
        const int dx[4] = {-1, 1, 0, 0};
        const int dy[4] = {0, 0, -1, 1};
        for (int dir = 0; dir < 4; ++dir) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];
            if (nx < 0 || nx >= BOARD_SIZE || ny < 0 || ny >= BOARD_SIZE) continue;
            if (game.getPiece(nx, ny) != NONE) return true;
        }
        return false;
    };

    // Lấy các ô trống mà xung quanh (4 hướng) có quân
    for (int y = 0; y < BOARD_SIZE; ++y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            if (game.getPiece(x, y) != NONE) continue;
            if (hasNeighbourStone(x, y)) {
                moves.emplace_back(x, y, false);
            }
        }
    }

    // Fallback: không tìm được ô "gần quân" nào -> lấy tất cả ô trống
    if (moves.empty()) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                if (game.getPiece(x, y) == NONE) {
                    moves.emplace_back(x, y, false);
                }
            }
        }
    }

    return moves;
}

// ---------------------------------------------------------------------
// Minimax (không Alpha–Beta) - dùng cho mức Medium.
// Tham số Game game truyền theo GIÁ TRỊ => mỗi node có 1 bản copy riêng,
// đảm bảo không side-effect lên game thật.
// ---------------------------------------------------------------------
double GoAI::minimax(Game game,
                     int depth,
                     int maxDepth,
                     bool maximizingPlayer,
                     PieceColor aiColor)
{
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
        // Lượt của AI
        for (const AIMove& m : moves) {
            Game child = game;

            if (!child.placeStone(m.x, m.y)) {
                continue;
            }

            anyValidChild = true;

            double val = minimax(child,
                                 depth + 1,
                                 maxDepth,
                                 false, // lượt tiếp theo là đối thủ (minimizer)
                                 aiColor);

            if (val > bestValue) bestValue = val;
        }
    } else {
        // Lượt của đối thủ
        for (const AIMove& m : moves) {
            Game child = game;

            if (!child.placeStone(m.x, m.y)) {
                continue;
            }

            anyValidChild = true;

            double val = minimax(child,
                                 depth + 1,
                                 maxDepth,
                                 true, // lượt tiếp theo là AI (maximizer)
                                 aiColor);

            if (val < bestValue) bestValue = val;
        }
    }

    if (!anyValidChild) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    return bestValue;
}

// ---------------------------------------------------------------------
// Minimax + Alpha–Beta - dùng cho mức Hard.
// Cũng truyền Game theo giá trị để cô lập mọi thay đổi.
// ---------------------------------------------------------------------
double GoAI::minimaxAlphaBeta(Game game,
                               int depth,
                               int maxDepth,
                               double alpha,
                               double beta,
                               bool maximizingPlayer,
                               PieceColor aiColor)
{
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
        // Node của AI
        for (const AIMove& m : moves) {
            Game child = game;

            if (!child.placeStone(m.x, m.y)) {
                continue;
            }

            anyValidChild = true;

            double val = minimaxAlphaBeta(child,
                                          depth + 1,
                                          maxDepth,
                                          alpha,
                                          beta,
                                          false, // lượt tiếp theo là đối thủ (minimizer)
                                          aiColor);

            if (val > bestValue) bestValue = val;
            if (val > alpha)      alpha     = val;

            if (beta <= alpha) break; // cắt tỉa
        }
    } else {
        // Node của đối thủ
        for (const AIMove& m : moves) {
            Game child = game;

            if (!child.placeStone(m.x, m.y)) {
                continue;
            }

            anyValidChild = true;

            double val = minimaxAlphaBeta(child,
                                          depth + 1,
                                          maxDepth,
                                          alpha,
                                          beta,
                                          true, // lượt tiếp theo là AI (maximizer)
                                          aiColor);

            if (val < bestValue) bestValue = val;
            if (val < beta)       beta     = val;

            if (beta <= alpha) break; // cắt tỉa
        }
    }

    if (!anyValidChild) {
        return evaluateBoardHeuristic(game, aiColor);
    }

    return bestValue;
}

// ---------------------------------------------------------------------
// Tính nước đi tốt nhất nhưng KHÔNG đặt quân vào game.
// ---------------------------------------------------------------------
AIMove GoAI::computeAIMove(const Game& game, AIDifficulty difficulty)
{
    // AI luôn chơi với màu của lượt hiện tại
    const PieceColor aiColor = game.getTurn();

    std::vector<AIMove> candidates = generateCandidateMoves(game);
    if (candidates.empty()) {
        // Không có nước khả thi -> pass
        return AIMove(-1, -1, true);
    }

    // ===================== EASY: RANDOM MOVE =====================
    if (difficulty == AIDifficulty::EASY) {
        std::mt19937& rng = globalRng();
        std::shuffle(candidates.begin(), candidates.end(), rng);

        // Chọn nước đầu tiên mà đặt được trên một bản copy (tránh suicide / out-of-bounds)
        for (const AIMove& m : candidates) {
            Game test = game;
            if (test.placeStone(m.x, m.y)) {
                return AIMove(m.x, m.y, false);
            }
        }

        // Nếu tất cả candidate đều invalid -> pass
        return AIMove(-1, -1, true);
    }

    // ===================== MEDIUM / HARD: MINIMAX =====================
    int  maxDepth;
    bool useAlphaBeta;

    if (difficulty == AIDifficulty::MEDIUM) {
        maxDepth     = 2;     // AI -> Opponent
        useAlphaBeta = false;
    } else {
        maxDepth     = 3;     // Hard: đi sâu hơn
        useAlphaBeta = true;
    }

    AIMove bestMove(-1, -1, true);
    double bestScore = -std::numeric_limits<double>::infinity();

    for (const AIMove& m : candidates) {
        Game child = game;

        if (!child.placeStone(m.x, m.y)) {
            continue; // ko hợp lệ trên state copy
        }

        double score;
        if (useAlphaBeta) {
            score = minimaxAlphaBeta(child,
                                     /*depth=*/1,
                                     maxDepth,
                                     -std::numeric_limits<double>::infinity(),
                                     +std::numeric_limits<double>::infinity(),
                                     /*maximizingPlayer=*/false, // đối thủ
                                     aiColor);
        } else {
            score = minimax(child,
                            /*depth=*/1,
                            maxDepth,
                            /*maximizingPlayer=*/false, // đối thủ
                            aiColor);
        }

        if (score > bestScore || bestMove.isPass) {
            bestScore = score;
            bestMove  = AIMove(m.x, m.y, false);
        }
    }

    if (bestMove.isPass) {
        return AIMove(-1, -1, true);
    }

    return bestMove;
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

    // Thử đặt quân đã chọn lên game thật
    if (game.placeStone(move.x, move.y)) {
        std::cout << "AI plays at (" << move.x << ", " << move.y << ")\n";
        return true;
    }

    // Nếu nước tốt nhất bị từ chối (thường do Ko vì state copy không có history),
    // ta thử các nước khác như fallback đơn giản.
    std::cout << "AI's chosen move is invalid on real game. Trying fallback...\n";

    std::vector<AIMove> fallback = generateCandidateMoves(game);
    std::mt19937& rng = globalRng();
    std::shuffle(fallback.begin(), fallback.end(), rng);

    for (const AIMove& m : fallback) {
        if (game.placeStone(m.x, m.y)) {
            std::cout << "AI fallback move at (" << m.x << ", " << m.y << ")\n";
            return true;
        }
    }

    // Nếu không có nước nào đặt được -> pass
    std::cout << "AI cannot find any legal move, PASS.\n";
    game.pass();
    return false;
}
