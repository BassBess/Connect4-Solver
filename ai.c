#include "ai.h"
#include "bot_easy.h"
#include "bot_medium.h"
#include "bot_hard.h" 


int getBotMove(char **b, int difficulty, char botSym, char oppSym) {
    if (difficulty == 1) return getEasyMove(b);
    if (difficulty == 2) return getMediumMove(b, botSym, oppSym);
    if (difficulty == 3) return getHardMove(b, botSym, oppSym);
    return getEasyMove(b);
}
