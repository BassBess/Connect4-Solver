#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "defs.h"
#include "bot_hard.h"

// --- CONSTANTS & TRANSPOSITION TABLE ---
#define TT_SIZE 8388617 
#define MAX_SCORE 10000
#define MIN_SCORE -10000

typedef struct {
    uint64_t key;
    int16_t val;  // CHANGED: int8_t -> int16_t (to hold 10000)
    uint8_t flag; 
} Entry;

Entry TT[TT_SIZE];

// --- BITBOARD LOGIC ---
// CHANGED: long long -> uint64_t (Unsigned 64-bit integer) to fix warnings

uint64_t get_position(char **b, char p) {
    uint64_t pos = 0;
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++) {
            if (b[row][col] == p) {
                pos |= (1ULL << (col * 7 + (ROWS - 1 - row)));
            }
        }
    }
    return pos;
}

uint64_t get_mask(char **b) {
    uint64_t mask = 0;
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++) {
            if (b[row][col] != EMPTY) {
                mask |= (1ULL << (col * 7 + (ROWS - 1 - row)));
            }
        }
    }
    return mask;
}

int has_won(uint64_t pos) {
    uint64_t m = pos & (pos >> 1); 
    if (m & (m >> 2)) return 1;
    m = pos & (pos >> 7); 
    if (m & (m >> 14)) return 1;
    m = pos & (pos >> 6); 
    if (m & (m >> 12)) return 1;
    m = pos & (pos >> 8); 
    if (m & (m >> 16)) return 1;
    return 0;
}

// --- SOLVER ---

int columnOrder[7] = {3, 2, 4, 1, 5, 0, 6};

// CHANGED: arguments are now uint64_t
int negamax(uint64_t position, uint64_t mask, int alpha, int beta, int depth) {
    
    if (has_won(position ^ mask)) return -(MAX_SCORE); 

    if (mask == 0x1FFFFFFFFFFFF) return 0; 

    uint64_t key = position + mask + 0x12345; 
    int i = key % TT_SIZE;
    
    // FIXED: Checking types correctly now (uint64_t vs uint64_t)
    if (TT[i].key == key) {
        // FIXED: TT[i].val is now int16_t, so it can handle MAX_SCORE
        if (TT[i].flag == 0) return TT[i].val; // Exact value
    }

    if (depth == 0) return 0; 

    for (int x = 0; x < 7; x++) {
        int col = columnOrder[x];
        
        if ((mask & (1ULL << (col * 7 + 5))) == 0) {
            
            uint64_t next_move = (mask + (1ULL << (col * 7))) ^ mask;
            
            int score = -negamax(position ^ mask, mask | next_move, -beta, -alpha, depth - 1);

            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) return alpha; 
            }
        }
    }
    
    TT[i].key = key;
    TT[i].val = (int16_t)alpha;
    TT[i].flag = 0; 
    
    return alpha;
}

// --- MAIN WRAPPER ---

int getHardMove(char **b, char botSym, char oppSym) {
    
    (void)oppSym; // FIXED: Explicitly ignore this to silence the warning

    // 1. OPENING BOOK 
    if (b[ROWS-1][3] == EMPTY) return 4;

    // 2. CONVERT BOARD TO BITS
    uint64_t position = get_position(b, botSym);
    uint64_t mask = get_mask(b);
    
    int bestMove = -1;
    int bestScore = MIN_SCORE * 2;
    int alpha = MIN_SCORE * 2;
    int beta = MAX_SCORE * 2;
    
    int maxDepth = 12; 

    for (int x = 0; x < 7; x++) {
        int col = columnOrder[x];
        
        if ((mask & (1ULL << (col * 7 + 5))) == 0) {
            
            uint64_t next_move = (mask + (1ULL << (col * 7))) ^ mask;
            
            int score = -negamax(position ^ mask, mask | next_move, -beta, -alpha, maxDepth);

            if (score > bestScore) {
                bestScore = score;
                bestMove = col;
            }
            
            if (bestScore > alpha) alpha = bestScore;
        }
    }
    
    if (bestMove == -1) {
        for(int i=0; i<7; i++) if(b[0][i]==EMPTY) return i+1;
    }

    return bestMove + 1; 
}
