#ifndef BOARD_H_INCLUDED
#define BOARD_H_INCLUDED


struct Board{
	int state[19][19];
	void init();
	void printSelf();
};

#endif // BOARD_H_INCLUDED
