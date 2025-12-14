#ifndef NONCLASS_H_INCLUDED
#define NONCLASS_H_INCLUDED

#include <cstdint>

enum PieceColor : std::uint8_t { NONE = 0, WHITE = 1, BLACK = 2 };

inline constexpr int BOARD_SIZE = 19;
inline constexpr int BOARD_CELLS = BOARD_SIZE * BOARD_SIZE;

inline constexpr int encodePos(int x, int y) noexcept { return x * BOARD_SIZE + y; }
inline constexpr int decodeX(int id) noexcept { return id / BOARD_SIZE; }
inline constexpr int decodeY(int id) noexcept { return id % BOARD_SIZE; }

inline constexpr bool inBounds(int x, int y) noexcept {
    return (0 <= x && x < BOARD_SIZE && 0 <= y && y < BOARD_SIZE);
}

#endif // NONCLASS_H_INCLUDED
