#include <iostream>
#include "board.h"

using namespace std;

int main(){
    Board b;
    b.init();
    cout << "Initial Board State:\n";
    b.printSelf();
    cout << "Lets start the game!\n";
    return 0;
}
