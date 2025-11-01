#include <iostream>
#include "board.h"

using namespace std;

void Board::init(){
    for(int i=0; i<19; i++){
        for(int j=0; j<19; j++){
            state[i][j] = 0;
        }
    }
}

void Board::printSelf(){
    for(int i=0; i<19; i++){
        for(int j=0; j<19; j++){
            cout << state[i][j] << " ";
        }
        cout << endl;
    }
}
