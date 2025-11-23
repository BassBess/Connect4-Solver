#include <stdlib.h>
#include "defs.h"
#include "bot_easy.h"

int getEasyMove(char **b) {
    int col;

    do {
        col = (rand() % COLS) + 1; 
    } while (b[0][col - 1] != EMPTY);
    
    return col;
}
