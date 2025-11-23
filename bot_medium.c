#include "defs.h"
#include "logic.h"      
#include "bot_medium.h"

int getMediumMove(char **b, char botSym, char oppSym) {
 
    for (int col = 1; col <= COLS; col++) {
        if (canWin(b, col, botSym)) return col;
    }
    
  
    for (int col = 1; col <= COLS; col++) {
        if (canWin(b, col, oppSym)) return col;
    }

    int centers[] = {4, 3, 5, 2, 6, 1, 7};
    for (int i = 0; i < COLS; i++) {
        int col = centers[i];
        if (b[0][col-1] == EMPTY) return col;
    }


    return 1;
}
