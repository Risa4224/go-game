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
    void checkCaptures(int x, int y, PieceColor c);
    void removeGroup(int x, int y);
    int findGroupIndex(int x, int y);
    int calcLiberties(int x);
    int calcLiberties(int x, int y) const;
    bool valid(int x, int y) const;
public:
    Game(Board* b); 
    PieceColor getTurn() const { return turn; }
    bool placeStone(int x, int y); 
    bool ended(int x, int y);
    bool checkKO() const;
    void printDebug() const; 
};

#endif