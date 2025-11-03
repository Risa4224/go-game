// group.h
#ifndef GROUP_H
#define GROUP_H

#include "nonclass.h"
#include <iostream>
#include <vector>

class PieceGroup
{
public:
    PieceGroup(PieceColor);
    PieceGroup(int, int, PieceColor);
    ~PieceGroup();

    int getSize() const;
    int getLocation(int) const;
    bool contains(int, int) const;
    bool isConnected(int, int) const;
    void addPiece(int, int);
    PieceGroup combine(const PieceGroup&); 
    PieceColor getColor() const { return color; }
    void printSelf() const;
private:
    std::vector<int> locations;
    PieceColor color;
};

#endif // GROUP_H