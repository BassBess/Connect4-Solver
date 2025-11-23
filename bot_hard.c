#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "defs.h"
#include "bot_hard.h"

// --- CONSTANTS & TRANSPOSITION TABLE ---
#define TT_SIZE 8388617 // A large prime number for the cache size
#define MAX_SCORE 10000
#define MIN_SCORE -10000

// Entry in our cache
typedef struct {
    uint64_t key;
    int8_t val;
    uint8_t flag; // 0: Exact, 1: Lowerbound, 2: Upperbound
} Entry;

Entry TT[TT_SIZE];

// --- BITBOARD LOGIC ---
// A bitboard is a 64-bit integer.
// We need two: one for 'position' (current player) and one for 'mask' (all pieces).
// Connect 4 is 7 cols x 6 rows. We use 7 bits per column (6 + 1 buffer bit).

long long get_position(char **b, char p) {
    long long pos = 0;
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++) {
            if (b[row][col] == p) {
                // Calculate bit index: col * 7 + row
                pos |= (1ULL << (col * 7 + (ROWS - 1 - row)));
            }
        }
    }
    return pos;
}

long long get_mask(char **b) {
    long long mask = 0;
    for (int col = 0; col < COLS; col++) {
        for (int row = 0; row < ROWS; row++) {
            if (b[row][col] != EMPTY) {
                mask |= (1ULL << (col * 7 + (ROWS - 1 - row)));
            }
        }
    }
    return mask;
}

// Check for win using bitwise operations (Insanely fast)
int has_won(long long pos) {
    long long m = pos & (pos >> 1); // Horizontal
    if (m & (m >> 2)) return 1;
    m = pos & (pos >> 7); // Vertical
    if (m & (m >> 14)) return 1;
    m = pos & (pos >> 6); // Diagonal 1
    if (m & (m >> 12)) return 1;
    m = pos & (pos >> 8); // Diagonal 2
    if (m & (m >> 16)) return 1;
    return 0;
}

// --- SOLVER ---

// Column order: Center first (4), then 3, 5, 2, 6, 1, 7.
// This makes Alpha-Beta pruning much faster.
int columnOrder[7] = {3, 2, 4, 1, 5, 0, 6};

int negamax(long long position, long long mask, int alpha, int beta, int depth) {
    // Check if the previous player just made a winning move
    // Note: We check 'position ^ mask' which is the other player
    if (has_won(position ^ mask)) return -(MAX_SCORE); 

    if (mask == 0x1FFFFFFFFFFFF) return 0; // Draw (Board full)

    // Cache Key (Mask + Position identifies unique state)
    long long key = position + mask + 0x12345; // Simple mixing
    int i = key % TT_SIZE;
    
    // Check Cache (Transposition Table)
    // Only use cache if it's the exact same board state
    if (TT[i].key == key) {
        if (TT[i].val > MAX_SCORE - 100) { // Adjust for depth
            // Basic retrieval logic, simplified for this snippet
        }
    }

    if (depth == 0) return 0; // Reached depth limit

    for (int x = 0; x < 7; x++) {
        int col = columnOrder[x];
        // Can we play in this column? 
        // (mask + bottom_bit) & column_mask == 0 means column full
        if ((mask & (1ULL << (col * 7 + 5))) == 0) {
            
            long long next_move = (mask + (1ULL << (col * 7))) ^ mask;
            
            // Recursive call: negating the score (Minimax)
            int score = -negamax(position ^ mask, mask | next_move, -beta, -alpha, depth - 1);

            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) return alpha; // Pruning
            }
        }
    }
    
    // Store in Cache
    TT[i].key = key;
    TT[i].val = alpha;
    
    return alpha;
}

// --- OPENING BOOK & MAIN WRAPPER ---

int getHardMove(char **b, char botSym, char oppSym) {
    
    // 1. OPENING BOOK (The "Perfect" Start)
    // If board is empty, Center (Col 4) is mathematically solved to win.
    if (b[ROWS-1][3] == EMPTY) return 4;

    // If we are player 2 and they took center, we have specific optimal moves.
    // But for now, let's jump to the solver.

    // 2. CONVERT BOARD TO BITS
    long long position = get_position(b, botSym);
    long long mask = get_mask(b);
    
    int bestMove = -1;
    int bestScore = MIN_SCORE * 2;
    int alpha = MIN_SCORE * 2;
    int beta = MAX_SCORE * 2;
    
    // 3. ITERATIVE DEEPENING
    // In a real competition, you loop based on Time (e.g., "search for 1 second").
    // Here, we pick a high fixed depth (e.g., 10-12 plies). 
    // Standard Minimax does ~6-7. Bitboards allow 12+.
    int maxDepth = 12; 

    for (int x = 0; x < 7; x++) {
        int col = columnOrder[x];
        
        // Check if column is playable
        if ((mask & (1ULL << (col * 7 + 5))) == 0) {
            
            long long next_move = (mask + (1ULL << (col * 7))) ^ mask;
            
            // We make the move, then ask negamax "How good is this for the other guy?"
            // and negate it.
            int score = -negamax(position ^ mask, mask | next_move, -beta, -alpha, maxDepth);

            if (score > bestScore) {
                bestScore = score;
                bestMove = col;
            }
            
            // Alpha update
            if (bestScore > alpha) alpha = bestScore;
        }
    }
    
    // Fallback if something went wrong
    if (bestMove == -1) {
        for(int i=0; i<7; i++) if(b[0][i]==EMPTY) return i+1;
    }

    return bestMove + 1; // Convert 0-6 back to 1-7
}
