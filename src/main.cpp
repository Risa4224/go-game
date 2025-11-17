// main.cpp
#include <iostream>
#include "board.h"
#include "game.h" // Cần include game.h
//#include "GameApp.h"

using namespace std;

int main(){
    Board* gameBoard = new Board(); 

    // 2. Khởi tạo Game (Game logic giờ sở hữu con trỏ này)
    Game gameLogic(gameBoard);

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
        if(x == -2 && y == -2){
            gameLogic.undo();
        }
        else if(x == -3 && y == -3){
            gameLogic.redo();
        }
        else{
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
    }
    cout << "Game Over!" << endl;
    gameLogic.calculateFinalScore(6.5f); // Tính điểm cuối cùng với komi 6.5
    //cout << gameLogic.cal();
    //UI is down here
    //GameApp game;
    //game.Run();
    return 0;
}