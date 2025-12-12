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

struct AIMove {
    int x;
    int y;
    bool isPass; 

    AIMove(int xx = -1, int yy = -1, bool pass = true)
        : x(xx), y(yy), isPass(pass) {}
};

class GoAI {
public:
    static AIMove computeAIMove(const Game& game, AIDifficulty difficulty);

    static bool playAIMove(Game& game, AIDifficulty difficulty);

private:
    static double evaluateBoardHeuristic(const Game& game, PieceColor aiColor);

    static std::vector<AIMove> generateCandidateMoves(const Game& game);

    static double minimax(Game game,
                          int depth,
                          int maxDepth,
                          bool maximizingPlayer,
                          PieceColor aiColor);

    static double minimaxAlphaBeta(Game game,
                                   int depth,
                                   int maxDepth,
                                   double alpha,
                                   double beta,
                                   bool maximizingPlayer,
                                   PieceColor aiColor);
};
