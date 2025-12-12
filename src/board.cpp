// board.cpp
#include "board.h"
#include <iostream>

Board::Board() {
    cells.fill(NONE);
}

PieceColor Board::getPiece(int x, int y) const {
    if (!inBounds(x, y)) return NONE;
    return cells[encodePos(x, y)];
}

void Board::setPiece(int x, int y, PieceColor c) {
    if (!inBounds(x, y)) return;
    cells[encodePos(x, y)] = c;
}

void Board::removePiece(int x, int y) {
    if (!inBounds(x, y)) return;
    cells[encodePos(x, y)] = NONE;
}

void Board::clear() {
    cells.fill(NONE);
}

void Board::printDebug() const {
    std::cout << "--- BOARD STATE ---\n";
    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            std::cout << static_cast<int>(getPiece(x, y)) << ' ';
        }
        std::cout << '\n';
    }
}
