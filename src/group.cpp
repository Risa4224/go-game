#include "group.h"
#include <cmath> 
#include <fstream>

using namespace std;

PieceGroup::PieceGroup(PieceColor col) {
    color = col;
}

PieceGroup::PieceGroup(int x, int y, PieceColor col) {
    color = col;
    locations.push_back(x * 19 + y);
}

PieceGroup::~PieceGroup() {
}

int PieceGroup::getSize() const {
    return locations.size();
}

int PieceGroup::getLocation(int index) const {
    if(index >= 0 && index < getSize()) {
        return locations[index];
    }
    else return -1;
}

bool PieceGroup::contains(int x, int y) const {
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if((x * 19 + y) == (*it)) return true;
    }
    return false;
}

void PieceGroup::addPiece(int x, int y) {
    if (!contains(x, y)) {
        locations.push_back(x * 19 + y);
    }
}

PieceGroup PieceGroup::combine(const PieceGroup& other) {
    PieceGroup result = other; 
    int tempx, tempy;
    
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        tempx = (*it)/19;
        tempy = (*it)%19;
        result.addPiece(tempx, tempy);
    }
    return result;
}

bool PieceGroup::isConnected(int x, int y) const {
    int group_x, group_y;
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        group_x = (*it)/19; 
        group_y = (*it)%19;

        if (std::abs(group_x - x) + std::abs(group_y - y) == 1) {
            return true;
        }
    }
    return false;
}

void PieceGroup::printSelf() const {
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        std::cout << "(" << (*it)/19 << ", " << (*it)%19 << ") ";
    }
}