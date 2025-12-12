#ifndef NONCLASS_H_INCLUDED
#define NONCLASS_H_INCLUDED

#include <cstdint>

enum PieceColor : std::uint8_t { NONE = 0, WHITE = 1, BLACK = 2 };

// Project hiện tại chỉ dùng 19x19. Gom tất cả magic number vào đây để đồng bộ.
inline constexpr int BOARD_SIZE = 19;
inline constexpr int BOARD_CELLS = BOARD_SIZE * BOARD_SIZE;

// Encoding mà code hiện tại đang dùng: id = x*BOARD_SIZE + y (x là "row index" trong board[x][y])
inline constexpr int encodePos(int x, int y) noexcept { return x * BOARD_SIZE + y; }
inline constexpr int decodeX(int id) noexcept { return id / BOARD_SIZE; }
inline constexpr int decodeY(int id) noexcept { return id % BOARD_SIZE; }

inline constexpr bool inBounds(int x, int y) noexcept {
    return (0 <= x && x < BOARD_SIZE && 0 <= y && y < BOARD_SIZE);
}

#endif // NONCLASS_H_INCLUDED
