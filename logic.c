#include "defs.h"
#include "logic.h"

int checkWin(char **b, int row, int col, char p) {
    int count = 0;
    
    for (int i = 0; i < COLS; i++) {
        if (b[row][i] == p) {
            count++;
            if (count == 4) return 1;
        } else count = 0;
    }
    
    count = 0;
    for (int i = 0; i < ROWS; i++) {
        if (b[i][col] == p) {
            count++;
            if (count == 4) return 1;
        } else count = 0;
    }
    
    count = 0;
    int r = row, c = col;
    while (r > 0 && c > 0) { r--; c--; } 
    while (r < ROWS && c < COLS) {
        if (b[r][c] == p) {
            count++;
            if (count == 4) return 1;
        } else count = 0;
        r++; c++;
    }
    
    count = 0;
    r = row; c = col;
    while (r > 0 && c < COLS - 1) { r--; c++; } 
    while (r < ROWS && c >= 0) {
        if (b[r][c] == p) {
            count++;
            if (count == 4) return 1;
        } else count = 0;
        r++; c--;
    }
    
    return 0;
}

int isBoardFull(char **b) {
    for (int i = 0; i < COLS; i++)
        if (b[0][i] == EMPTY) return 0;
    return 1;
}

int canWin(char **b, int col, char s) {
    if (b[0][col-1] != EMPTY) return 0;
    
    int row = -1;
    for (int i = ROWS - 1; i >= 0; i--) {
        if (b[i][col-1] == EMPTY) {
            row = i;
            break;
        }
    }
    if (row == -1) return 0;
    
    b[row][col-1] = s;
    int wins = checkWin(b, row, col-1, s);
    b[row][col-1] = EMPTY; // Undo move
    
    return wins;
}
