// game.h
#ifndef GAME_H
#define GAME_H

#include "board.h"
#include <vector>
#include <iostream>
#include <set>
#include <string>
class Game {
private:
    int black_captures = 0;
    int white_captures = 0;
    int consecutive_passes = 0;
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
    PieceColor getTerritoryOwner(int x, int y, int& territory_size, std::vector<std::vector<bool>>& visited) const;
//UI NEEDED 
    int  m_lastCaptures     = 0;
    bool m_lastInvalid      = false;  // invalid because occupied/OOB
    bool m_lastSuicide      = false;
    bool m_lastKoViolation  = false;
    bool m_lastKoThreat     = false;  // simple heuristic: capture=1
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
    bool pass();
    std::pair<float, float> calculateFinalScore(float komi = 6.5f) const;
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    void rebuildGroupsFromBoard();


    //UI NEEDED
    int  getLastCaptures() const        { return m_lastCaptures; }
    bool lastMoveWasInvalid() const     { return m_lastInvalid; }
    bool lastMoveWasSuicide() const     { return m_lastSuicide; }
    bool lastMoveWasKoViolation() const { return m_lastKoViolation; }
    bool lastMoveCreatedKoThreat() const{ return m_lastKoThreat; }
};

#endif