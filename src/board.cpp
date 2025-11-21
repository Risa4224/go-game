// board.cpp
#include <iostream>
#include "board.h"
#include "nonclass.h" 
#include <fstream>


using namespace std;


Board::Board() {
    for(int i = 0; i < 19; ++i) {
        for(int j = 0; j < 19; ++j) {
            board[i][j] = NONE;
        }
    }
}

PieceColor Board::getPiece(int x, int y) const {
    if (x < 0 || x >= 19 || y < 0 || y >= 19) return NONE;
    return board[x][y];
}


// empties the location on the board
void Board::removePiece(int x, int y) {
    if (x >= 0 && x < 19 && y >= 0 && y < 19) {
        board[x][y] = NONE;
    }
}

void Board::setPiece(int x, int y, PieceColor c) {
    if (x >= 0 && x < 19 && y >= 0 && y < 19) {
        board[x][y] = c;
    }
}

bool Board::isEqual(const Board& other) const {
    for (int i = 0; i < 19; ++i) {
        for (int j = 0; j < 19; ++j) {
            // So sánh từng ô
            if (this->board[i][j] != other.board[i][j]) {
                return false; 
            }
        }
    }
    return true; 
}


// debug functions
void Board::printDebug() const {
    std::cout << "--- BOARD STATE ---" << std::endl;
    for(int i = 0; i < 19; ++i) {
        for(int j = 0; j < 19; ++j) {
            std::cout << getPiece(i, j) << " ";
        }
        std::cout << std::endl;
    }
}