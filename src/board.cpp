// board.cpp
#include <iostream>
#include "board.h"
#include "nonclass.h" // Cần nonclass.h
#include <fstream>


using namespace std;


Board::Board() {
    for(int i = 0; i < 19; ++i) {
        for(int j = 0; j < 19; ++j) {
            board[i][j] = NONE;
        }
    }
    // LOẠI BỎ: turn = BLACK;
}

// getPiece đã sửa, thêm const và check biên
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


// debug functions
void Board::printDebug() const {
    for(int i = 0; i < 19; ++i) {
        for(int j = 0; j < 19; ++j) {
            std::cout << getPiece(i, j) << " ";
        }
        std::cout << std::endl;
    }
}
