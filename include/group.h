#ifndef GROUP_H
#define GROUP_H

#include "nonclass.h"
#include <vector>
#include <iostream>

class PieceGroup {
public:
    explicit PieceGroup(PieceColor col = NONE);
    PieceGroup(int x, int y, PieceColor col);

    int getSize() const { return static_cast<int>(locations.size()); }
    int getLocation(int index) const;

    bool contains(int x, int y) const;
    bool isConnected(int x, int y) const;
    void addPiece(int x, int y);

    void addEncodedUnchecked(int id) { locations.push_back(id); }
    void reserve(int n) { locations.reserve(n); }

    PieceGroup combine(const PieceGroup& other) const; 
    PieceColor getColor() const { return color; }

    const std::vector<int>& getLocations() const { return locations; }
    std::vector<int>&       getLocationsRef()    { return locations; }

    void printSelf() const;

private:
    std::vector<int> locations; // encoding: x*BOARD_SIZE + y
    PieceColor color = NONE;
};

#endif // GROUP_H
