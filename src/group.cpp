#include "group.h"
#include <cmath>

PieceGroup::PieceGroup(PieceColor col) : color(col) {}

PieceGroup::PieceGroup(int x, int y, PieceColor col) : color(col) {
    locations.push_back(encodePos(x, y));
}

int PieceGroup::getLocation(int index) const {
    if (index < 0 || index >= getSize()) return -1;
    return locations[static_cast<std::size_t>(index)];
}

bool PieceGroup::contains(int x, int y) const {
    const int id = encodePos(x, y);
    for (int v : locations) {
        if (v == id) return true;
    }
    return false;
}

void PieceGroup::addPiece(int x, int y) {
    const int id = encodePos(x, y);
    for (int v : locations) {
        if (v == id) return; // đã có
    }
    locations.push_back(id);
}

PieceGroup PieceGroup::combine(const PieceGroup& other) const {
    // Hai group disjoint (theo logic Game), nên có thể append thẳng để nhanh.
    PieceGroup result = other;
    result.locations.reserve(result.locations.size() + locations.size());
    result.locations.insert(result.locations.end(), locations.begin(), locations.end());
    return result;
}

bool PieceGroup::isConnected(int x, int y) const {
    for (int id : locations) {
        const int gx = decodeX(id);
        const int gy = decodeY(id);
        if (std::abs(gx - x) + std::abs(gy - y) == 1) return true;
    }
    return false;
}

void PieceGroup::printSelf() const {
    for (int id : locations) {
        std::cout << "(" << decodeX(id) << ", " << decodeY(id) << ") ";
    }
}
