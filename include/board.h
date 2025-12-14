#ifndef BOARD_H_INCLUDED
#define BOARD_H_INCLUDED

#include "nonclass.h"
#include <array>

class Board {
public:
    Board();

    PieceColor getPiece(int x, int y) const;
    void setPiece(int x, int y, PieceColor c);
    void removePiece(int x, int y);

    // Size cố định 19
    int getSize() const { return BOARD_SIZE; }

    void clear();
    void printDebug() const;

    bool isEqual(const Board& other) const { return cells == other.cells; }

private:
    std::array<PieceColor, BOARD_CELLS> cells;
};

#endif // BOARD_H_INCLUDED
