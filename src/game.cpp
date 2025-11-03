// game.cpp (FILE MỚI)
#include "game.h"
#include <iostream>
#include <cmath> 

using namespace std;

Game::Game(Board* b) : board(b), turn(BLACK) {}

// Phương thức Logic Game (PRIVATE)
PieceColor Game::oppositeColor(PieceColor input) const {
    if(input == BLACK) return WHITE;
    else if(input == WHITE) return BLACK;
    else return NONE;
}

bool Game::valid(int x, int y) const{
    if(x < 0 || x >= 19 || y < 0 || y >= 19) return false;
    if(board->getPiece(x,y) != NONE) return false;
    return true;
}

void Game::processGroups(int x, int y, PieceColor c) {
    if(groups.size() <= 0) groups.push_back(PieceGroup(x, y, c));

    else {
        std::vector<int> connections; 

        for(unsigned int i = 0; i < groups.size(); ++i) {
            if(groups[i].getColor() == c && groups[i].isConnected(x, y)) {
                connections.push_back(i);
            }
        }

        if(connections.size() <= 0) groups.push_back(PieceGroup(x, y, c));

        else {
            PieceGroup temp = PieceGroup(c); 
            temp.addPiece(x,y); 

            for(std::vector<int>::iterator it = connections.begin(); it != connections.end(); ++it) {
                temp = groups[*it].combine(temp);
            }

            for(int i = connections.size()-1; i >= 0; --i) {
                groups.erase(groups.begin() + connections[i]);
            }

            groups.push_back(temp);
        }
    }
}

void Game::checkCaptures(int x, int y, PieceColor c) {
    std::cout << "Checking captures around (" << x << ", " << y << "):\n";
    if(board->getPiece(x-1,y) == oppositeColor(c)) {
        std::cout << x - 1 << ' ' << y << '\n';
        std::cout << findGroupIndex(x-1,y) << '\n';
        if(calcLiberties(x-1,y) == 0 && calcLiberties(findGroupIndex(x-1,y)) == 0){
            removeGroup(x-1,y);
        }
    }
    if(board->getPiece(x+1,y) == oppositeColor(c)) {
        std:: cout << x + 1 << ' ' << y << '\n';
        std:: cout << findGroupIndex(x+1,y) << '\n';
        if(calcLiberties(x+1,y) == 0 && calcLiberties(findGroupIndex(x+1,y)) == 0){
            removeGroup(x+1,y);
        }
    }
    if(board->getPiece(x,y-1) == oppositeColor(c)) {
        std:: cout << x << ' ' << y - 1 << '\n';       
        std:: cout << findGroupIndex(x,y-1) << '\n';
        if(calcLiberties(x,y-1) == 0 && calcLiberties(findGroupIndex(x,y-1)) == 0){
            removeGroup(x,y-1);
        }
    }
    if(board->getPiece(x,y+1) == oppositeColor(c)) {
        std:: cout << x << ' ' << y + 1 << '\n';
        std:: cout << findGroupIndex(x,y+1) << '\n';
        if(calcLiberties(x,y+1) == 0 && calcLiberties(findGroupIndex(x,y+1)) == 0){
            removeGroup(x,y+1);
        }
    }
}

int Game::findGroupIndex(int x, int y) {
    for(int i = 0; i < (int)groups.size(); ++i) {
        if(groups[i].contains(x, y)) {
            return i;
        }
    }
    return -1;
}

void Game::removeGroup(int x, int y) {
    int ind = -1;
    for(int i = 0; i < (int)groups.size(); ++i) {
        if(groups[i].contains(x, y)) {
            ind = i;
            break;
        }
    }

    if(ind == -1) {
        std::cout << "SOMETHING WENT WRONG: removeGroup failed to find group" << std::endl;
        return;
    }

    int tempx, tempy;
    for(int i = 0; i < groups[ind].getSize(); ++i) {
        int location = groups[ind].getLocation(i);
        tempx = location / 19;
        tempy = location % 19;
        std::cout << "Removing piece at (" << tempx << ", " << tempy << ")\n";
        board->removePiece(tempx, tempy); 
    }
    groups.erase(groups.begin() + ind);
}

// Cập nhật chữ ký: nhận tham chiếu const
int Game::calcLiberties(int id){ 
    int sum = 0;
    std::cout << id << '\n';
    for(int i = 0; i < groups[id].getSize(); ++i) {
        int location = groups[id].getLocation(i);
        std::cout << location / 19 << ' ' << location % 19 << '\n';
        sum += calcLiberties(location / 19, location % 19); 
        std::cout << "Current sum: " << sum << '\n';
    }
    std::cout << sum << '\n';
    return sum;
}

int Game::calcLiberties(int x, int y) const {
    int sum = 0;
    if(valid(x - 1, y)) sum += 1;
    if(valid(x + 1, y)) sum += 1;
    if(valid(x, y - 1)) sum += 1;
    if(valid(x, y + 1)) sum += 1;
    return sum;
}

bool Game::ended(int x, int y){
    if(x < 0 || y < 0) return true;
    return false;
}


// Phương thức chính: Xử lý nước đi VÀ logic game
bool Game::placeStone(int x, int y) {
    // 1. KIỂM TRA LUẬT CẤP ĐỘ BOARD (Trống và trong biên)
    if(!valid(x, y)){
        cout << "Invalid move: spot already occupied or out of bounds." << endl;
        return false;
    }
    
    PieceColor current_color = turn;

    // 2. THỰC HIỆN THAO TÁC CẤP ĐỘ BOARD (Đặt quân)
    board->setPiece(x, y, current_color); 

    // 3. THỰC HIỆN LOGIC GAME
    processGroups(x, y, current_color);
    checkCaptures(x, y, current_color);
    
    // Switch the turn (Logic game)
    turn = oppositeColor(turn);
    
    return true;
}

// Debug function
void Game::printDebug() const {
    cout << "--- GAME STATE ---" << endl;
    cout << "Current Turn: " << ((turn == BLACK) ? "BLACK (2)" : "WHITE (1)") << endl;
    cout << "Num groups: " << groups.size() << endl;
    
    board->printDebug();
    
    cout << "--- GROUP DETAILS ---" << endl;
    for(const auto& group : groups) {
        cout << "Color: " << group.getColor() << ", Size: " << group.getSize() << ", Locations: ";
        group.printSelf();
        cout << endl;
    }
    cout << "--------------------" << endl;
}