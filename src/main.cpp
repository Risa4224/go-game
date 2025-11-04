// main.cpp
#include <iostream>
#include "board.h"
#include "game.h" // Cần include game.h

using namespace std;

int main(){
    // 1. Khởi tạo Board (Dữ liệu)
    Board gameBoard; 

    // 2. Khởi tạo Game (Logic), truyền Board vào
    Game gameLogic(&gameBoard);

    cout << "Go Game Initialized. BLACK goes first." << endl;
    // Vòng lặp game
    int x, y;
    int pre = 0;
    while (true) {
        PieceColor currentTurn = gameLogic.getTurn();
        char playerChar = (currentTurn == BLACK) ? 'B' : 'W';
        cout << "\n--- Turn: " << playerChar << " ---" << endl;
        
        gameLogic.printDebug(); // In trạng thái game và board

        cout << "Player " << playerChar << ", enter move (x y, or -1, -1 to pass turn): ";
        if (!(cin >> x >> y)) break;
        if(gameLogic.ended(x, y)) pre++;
        else pre = 0;
        if(pre == 2) break;
        // GameLogic::placeStone xử lý toàn bộ: kiểm tra luật, đặt quân, xử lý nhóm, bắt quân, chuyển lượt
        bool success = gameLogic.placeStone(x, y);

        if (!success) {
            cout << "Invalid move. Try again." << endl;
            // Không chuyển lượt, người chơi này phải thử lại.
        }
    }

    return 0;
}