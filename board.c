#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "board.h"

char **createBoard() {
    char **b = malloc(ROWS * sizeof(char *));
    for (int i = 0; i < ROWS; i++) {
        b[i] = malloc(COLS * sizeof(char));
        for (int j = 0; j < COLS; j++)
            b[i][j] = EMPTY;
    }
    return b;
}

void freeBoard(char **b) {
    for (int i = 0; i < ROWS; i++) {
        free(b[i]);
    }
    free(b);
}

void printBoard(char **b) {
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++)
            printf("%c ", b[i][j]);
        printf("\n");
    }
    printf("1 2 3 4 5 6 7\n");
}

int placePiece(char **b, int col, char p, int *row_placed) {
    col--; // Convert 1-based to 0-based
    if (b[0][col] != EMPTY) {
        printf("Column full. Try another.\n");
        return 0;
    }
    for (int i = ROWS - 1; i >= 0; i--) {
        if (b[i][col] == EMPTY) {
            b[i][col] = p;
            if (row_placed)
                *row_placed = i;
            return 1;
        }
    }
    return 0;
}
