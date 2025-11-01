#include <iostream>
#include "board.h"

using namespace std;

int main(){
    Board b;
    b.placePiece(0, 0, BLACK);
    b.placePiece(1, 1, WHITE);
    b.printSelf();
    return 0;
}
