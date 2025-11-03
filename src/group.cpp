// group.cpp
#include "group.h"
#include <cmath> // Cần cho std::abs

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

// Thêm const
int PieceGroup::getSize() const {
    return locations.size();
}

// Thêm const
int PieceGroup::getLocation(int index) const {
    if(index >= 0 && index < getSize()) {
        return locations[index];
    }
    else return -1;
}

// Thêm const
bool PieceGroup::contains(int x, int y) const {
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if((x * 19 + y) == (*it)) return true;
    }
    return false;
}

void PieceGroup::addPiece(int x, int y) {
    // Nên kiểm tra trùng lặp
    if (!contains(x, y)) {
        locations.push_back(x * 19 + y);
    }
}

// Sửa lỗi tham số và đảm bảo logic combine chính xác
PieceGroup PieceGroup::combine(const PieceGroup& other) {
    PieceGroup result = other; // Bắt đầu bằng bản sao của 'other'
    int tempx, tempy;
    
    // Thêm các mảnh từ *this vào 'result'
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        tempx = (*it)/19;
        tempy = (*it)%19;
        result.addPiece(tempx, tempy);
    }
    return result;
}

// Thêm const và sửa lỗi logic tọa độ
bool PieceGroup::isConnected(int x, int y) const {
    int group_x, group_y;
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        // Tọa độ đã được sửa lại: x là /19, y là %19
        group_x = (*it)/19; 
        group_y = (*it)%19;

        // Kiểm tra 4 hướng: |x1 - x2| + |y1 - y2| == 1
        if (std::abs(group_x - x) + std::abs(group_y - y) == 1) {
            return true;
        }
    }
    return false;
}

// Thêm const
void PieceGroup::printSelf() const {
    for(std::vector<int>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        std::cout << "(" << (*it)/19 << ", " << (*it)%19 << ") ";
    }
}