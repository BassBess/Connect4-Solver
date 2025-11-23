#ifndef BOARD_H
#define BOARD_H

char **createBoard();
void freeBoard(char **b);
void printBoard(char **b);
int placePiece(char **b, int col, char p, int *row_placed);

#endif
