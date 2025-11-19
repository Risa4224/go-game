// board.h
#ifndef BOARD_H_INCLUDED
#define BOARD_H_INCLUDED

#include "nonclass.h"
#include "group.h" // Cần cho PieceGroup
#include <vector>

class Board {
public:
	Board();
	
	// Phương thức Cấp độ Board (Truy cập dữ liệu)
	PieceColor getPiece(int, int) const; // Thêm const
	void removePiece(int, int);
	// Đổi tên thành setPiece để thể hiện chỉ là thao tác cơ bản
    void setPiece(int, int, PieceColor); 
    
    // Thêm getter cho kích thước bàn cờ
    int getSize() const { return 19; } 
    
	void printDebug() const; // Thêm const
	
	bool isEqual(const Board& other) const; // So sánh hai bàn cờ, thêm const

private:
    // LOẠI BỎ: std::vector<PieceGroup> groups; (Chuyển sang Game)
    // LOẠI BỎ: PieceColor turn; (Chuyển sang Game)
	PieceColor board[19][19];
};


#endif // BOARD_H_INCLUDED