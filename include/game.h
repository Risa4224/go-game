// game.h
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include <vector>
#include <iostream>

class Game {
private:
    Board* board;
    PieceColor turn;
    std::vector<PieceGroup> groups; // Dữ liệu logic game
    PieceColor oppositeColor(PieceColor input) const;
    void processGroups(int x, int y, PieceColor c);
    int checkCaptures(int x, int y, PieceColor c);
    void removeGroup(int x, int y, int &a);
    int findGroupIndex(int x, int y);
    int calcLiberties(int x);
    bool valid(int x, int y) const;
public:
    Game(Board* b); 
    Game(const Game& other);
    Game& operator=(const Game& other);
    PieceColor getTurn() const { return turn; }
    bool placeStone(int x, int y); 
    bool ended(int x, int y);
    bool checkKO() const;
    void printDebug() const; 
};

#endif