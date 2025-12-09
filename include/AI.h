#pragma once

#include <vector>
#include "nonclass.h"
#include "game.h"

// Mức độ khó của AI.
enum class AIDifficulty {
    EASY = 0,
    MEDIUM = 1,
    HARD = 2
};

// Một nước đi của AI.
struct AIMove {
    int x;
    int y;
    bool isPass; // true = AI chọn pass

    AIMove(int xx = -1, int yy = -1, bool pass = true)
        : x(xx), y(yy), isPass(pass) {}
};

// Lớp AI độc lập với UI, hoạt động trên lớp Game (core logic).
class GoAI {
public:
    // Tính nước đi tốt nhất nhưng KHÔNG đặt quân vào game.
    static AIMove computeAIMove(const Game& game, AIDifficulty difficulty);

    // Tính nước đi và đặt quân luôn lên game.
    // Trả về true nếu AI thực sự đặt được 1 nước, false nếu chỉ pass.
    static bool playAIMove(Game& game, AIDifficulty difficulty);

private:
    // --------- Các hàm trợ giúp ---------
    // Đánh giá bàn cờ ở trạng thái hiện tại, dương = lợi cho aiColor.
    static double evaluateBoardHeuristic(const Game& game, PieceColor aiColor);

    // Sinh các nước đi ứng viên (chỉ xét vị trí trống gần quân hiện có).
    static std::vector<AIMove> generateCandidateMoves(const Game& game);

    // Minimax (không Alpha–Beta) dùng cho mức Medium.
    static double minimax(Game game,
                          int depth,
                          int maxDepth,
                          bool maximizingPlayer,
                          PieceColor aiColor);

    // Minimax + Alpha–Beta dùng cho mức Hard.
    static double minimaxAlphaBeta(Game game,
                                   int depth,
                                   int maxDepth,
                                   double alpha,
                                   double beta,
                                   bool maximizingPlayer,
                                   PieceColor aiColor);
};
