#include "ai.h"
#include "bot_easy.h"
#include "bot_medium.h"

int getBotMove(char **b, int difficulty, char botSym, char oppSym) {
    if (difficulty == 1) {
        return getEasyMove(b);
    } 
    else if (difficulty == 2) {
        return getMediumMove(b, botSym, oppSym);
    }
    
    return getEasyMove(b);
}
