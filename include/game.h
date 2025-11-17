// game.h
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include <vector>
#include <iostream>
#include <set>

class Game {
private:
    int black_captures = 0;
    int white_captures = 0;
    Board* board;
    PieceColor turn;
    std::vector<PieceGroup> groups; // Dữ liệu logic game
    std::vector<Game> history; // Lưu lịch sử để kiểm tra KO
    std::vector<Game> future;  // Lưu lịch sử tương lai để hỗ trợ redo
    PieceColor oppositeColor(PieceColor input) const;
    void processGroups(int x, int y, PieceColor c);
    int checkCaptures(int x, int y, PieceColor c);
    void removeGroup(int x, int y, int &a);
    int findGroupIndex(int x, int y);
    int calcLiberties(int x);
    bool valid(int x, int y) const;
    PieceColor getTerritoryOwner(int x, int y, std::set<int>& visited) const;
public:
    ~Game();
    Game(const Game& other);
    Game& operator=(const Game& other);
    Game(Board* b); 
    PieceColor getTurn() const { return turn; }
    Board* getBoard() const { return board; }
    bool placeStone(int x, int y); 
    bool ended(int x, int y);
    bool checkKO() const;
    void printDebug() const; 
    bool redo();
    bool undo();
    void calculateFinalScore(float komi = 6.5f) const;
};

#endif