#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "defs.h"      
#include "bot_hard.h"

#define P_WIDTH 7
#define P_HEIGHT 6
#define MIN_SCORE -(P_WIDTH*P_HEIGHT)/2 + 3
#define MAX_SCORE (P_WIDTH*P_HEIGHT+1)/2 - 3
#define TT_SIZE 8388593 

typedef uint64_t bitboard_t;

typedef struct {
    bitboard_t current_position; 
    bitboard_t mask;
    int moves;
} Position;

typedef struct {
    uint64_t key;
    uint8_t val;
} TTEntry;

typedef struct {
    int move;
    int score;
} MoveEntry;

typedef struct {
    MoveEntry entries[P_WIDTH];
    int size;
} MoveSorter;

static TTEntry* transTable = NULL;
static int columnOrder[P_WIDTH];
static unsigned long long nodeCount = 0;

static int popcount(bitboard_t m) {
    return __builtin_popcountll(m);
}

static bitboard_t top_mask_col(int col) {
    return 1ULL << ((P_HEIGHT - 1) + col * (P_HEIGHT + 1));
}

static bitboard_t bottom_mask_col(int col) {
    return 1ULL << (col * (P_HEIGHT + 1));
}

static bitboard_t column_mask(int col) {
    return ((1ULL << P_HEIGHT) - 1) << (col * (P_HEIGHT + 1));
}

static bitboard_t get_bottom_mask() {
    bitboard_t m = 0;
    for(int c=0; c<P_WIDTH; c++) m |= bottom_mask_col(c);
    return m;
}

static bitboard_t get_board_mask() {
    bitboard_t m = 0;
    for(int c=0; c<P_WIDTH; c++) m |= column_mask(c);
    return m;
}

static bitboard_t compute_winning_position(bitboard_t position, bitboard_t mask) {
    bitboard_t r = (position << 1) & (position << 2) & (position << 3); 
    bitboard_t p = (position << (P_HEIGHT + 1)) & (position << 2 * (P_HEIGHT + 1));
    r |= p & (position << 3 * (P_HEIGHT + 1));
    r |= p & (position >> (P_HEIGHT + 1));
    p = (position >> (P_HEIGHT + 1)) & (position >> 2 * (P_HEIGHT + 1));
    r |= p & (position << (P_HEIGHT + 1));
    r |= p & (position >> 3 * (P_HEIGHT + 1));
    p = (position << P_HEIGHT) & (position << 2 * P_HEIGHT);
    r |= p & (position << 3 * P_HEIGHT);
    r |= p & (position >> P_HEIGHT);
    p = (position >> P_HEIGHT) & (position >> 2 * P_HEIGHT);
    r |= p & (position << P_HEIGHT);
    r |= p & (position >> 3 * P_HEIGHT);
    p = (position << (P_HEIGHT + 2)) & (position << 2 * (P_HEIGHT + 2)); 
    r |= p & (position << 3 * (P_HEIGHT + 2));
    r |= p & (position >> (P_HEIGHT + 2));
    p = (position >> (P_HEIGHT + 2)) & (position >> 2 * (P_HEIGHT + 2));
    r |= p & (position << (P_HEIGHT + 2));
    r |= p & (position >> 3 * (P_HEIGHT + 2));
    return r & (get_board_mask() ^ mask);
}

static void pos_play(Position *p, bitboard_t move) {
    p->current_position ^= p->mask;
    p->mask |= move;
    p->moves++;
}

static uint64_t pos_key(const Position *p) {
    return p->current_position + p->mask;
}

static int pos_can_win_next(const Position *p) {
    bitboard_t possible = (p->mask + get_bottom_mask()) & get_board_mask();
    return (compute_winning_position(p->current_position, p->mask) & possible) != 0;
}

static int pos_move_score(const Position *p, bitboard_t move) {
    return popcount(compute_winning_position(p->current_position | move, p->mask));
}

static bitboard_t pos_possible_non_losing_moves(const Position *p) {
    bitboard_t possible_mask = (p->mask + get_bottom_mask()) & get_board_mask();
    bitboard_t opponent_win = compute_winning_position(p->current_position ^ p->mask, p->mask);
    bitboard_t forced_moves = possible_mask & opponent_win;
    if(forced_moves) {
        if(forced_moves & (forced_moves - 1)) return 0; 
        else possible_mask = forced_moves; 
    }
    return possible_mask & ~(opponent_win >> 1); 
}

void init_hard_bot() {
    if (transTable) return; 
    
    transTable = (TTEntry*)calloc(TT_SIZE, sizeof(TTEntry));
    if (!transTable) {
        fprintf(stderr, "Fatal: Failed to allocate memory for Hard Bot.\n");
        exit(1);
    }
    
    for(int i = 0; i < P_WIDTH; i++) 
        columnOrder[i] = P_WIDTH/2 + (1-2*(i%2))*(i+1)/2;
}

static void tt_put(uint64_t key, uint8_t val) {
    int idx = key % TT_SIZE;
    transTable[idx].key = key;
    transTable[idx].val = val;
}

static uint8_t tt_get(uint64_t key) {
    int idx = key % TT_SIZE;
    if(transTable[idx].key == key) return transTable[idx].val;
    return 0;
}


static int negamax(const Position *P, int alpha, int beta) {
    nodeCount++;
    
    bitboard_t possible = pos_possible_non_losing_moves(P);
    if (possible == 0) return -(P_WIDTH * P_HEIGHT - P->moves) / 2; 
    
    if (P->moves >= P_WIDTH * P_HEIGHT - 2) return 0; 

    int min = -(P_WIDTH * P_HEIGHT - 2 - P->moves) / 2;
    if (alpha < min) {
        alpha = min;
        if (alpha >= beta) return alpha;
    }

    int max = (P_WIDTH * P_HEIGHT - 1 - P->moves) / 2;
    if (beta > max) {
        beta = max;
        if (alpha >= beta) return beta;
    }

    uint64_t key = pos_key(P);
    uint8_t val = tt_get(key);
    if (val) {
        if (val > MAX_SCORE - MIN_SCORE + 1) {
            min = val + 2 * MIN_SCORE - MAX_SCORE - 2;
            if (alpha < min) {
                alpha = min;
                if (alpha >= beta) return alpha;
            }
        } else {
            max = val + MIN_SCORE - 1;
            if (beta > max) {
                beta = max;
                if (alpha >= beta) return beta;
            }
        }
    }

    MoveSorter moves; 
    moves.size = 0;
    
    for (int i = P_WIDTH - 1; i >= 0; i--) {
        int col = columnOrder[i];
        bitboard_t move = possible & column_mask(col);
        if (move) {
            int pos = moves.size++;
            int score = pos_move_score(P, move);
            for(; pos && moves.entries[pos - 1].score > score; --pos) 
                moves.entries[pos] = moves.entries[pos - 1];
            moves.entries[pos].move = move;
            moves.entries[pos].score = score;
        }
    }

    while (moves.size > 0) {
        bitboard_t next_move = (bitboard_t)moves.entries[--moves.size].move;
        Position P2 = *P;
        pos_play(&P2, next_move);
        int score = -negamax(&P2, -beta, -alpha);
        
        if (score >= beta) {
            tt_put(key, score + MAX_SCORE - 2 * MIN_SCORE + 2);
            return score;
        }
        if (score > alpha) alpha = score;
    }
    
    tt_put(key, alpha - MIN_SCORE + 1);
    return alpha;
}

static int solve_position(const Position *P) {
    nodeCount = 0;
    if (pos_can_win_next(P)) return (P_WIDTH * P_HEIGHT + 1 - P->moves) / 2;
    
    int min = -(P_WIDTH * P_HEIGHT - P->moves) / 2;
    int max = (P_WIDTH * P_HEIGHT + 1 - P->moves) / 2;
    
    while (min < max) {
        int med = min + (max - min) / 2;
        if (med <= 0 && min / 2 < med) med = min / 2;
        else if (med >= 0 && max / 2 > med) med = max / 2;
        
        int r = negamax(P, med, med + 1);
        if (r <= med) max = r;
        else min = r;
    }
    return min;
}





int getHardMove(char **b, char botSym) {
   
    if(!transTable) init_hard_bot();

    Position p = {0, 0, 0};
    
    for(int c = 0; c < P_WIDTH; c++) {
        for(int r = P_HEIGHT - 1; r >= 0; r--) {
            if(b[r][c] != EMPTY) {
                p.moves++;
                
                int bitRow = (P_HEIGHT - 1) - r; 
                int bitIdx = bitRow + c * (P_HEIGHT + 1);
                
                p.mask |= (1ULL << bitIdx);
                if(b[r][c] == botSym) {
                    p.current_position |= (1ULL << bitIdx);
                }
            }
        }
    }
    bitboard_t possible = (p.mask + get_bottom_mask()) & get_board_mask();
    for(int i=0; i<P_WIDTH; i++) {
        int col = columnOrder[i]; 
        bitboard_t move = (p.mask + bottom_mask_col(col)) & column_mask(col);
        
        if((possible & column_mask(col)) && move) {
             if(compute_winning_position(p.current_position, p.mask) & move) {
                 return col + 1; 
             }
        }
    }

    int bestCol = -1;
    int bestScore = -99999;

    for(int i = 0; i < P_WIDTH; i++) {
        int col = columnOrder[i]; 
        
        if((p.mask & top_mask_col(col)) == 0) {
            Position p2 = p;
            bitboard_t move = (p2.mask + bottom_mask_col(col)) & column_mask(col);
            
            pos_play(&p2, move); 
  
            int score = -solve_position(&p2);
            
            if(score > bestScore) {
                bestScore = score;
                bestCol = col;
            }
        }
    }

    if(bestCol == -1) {
        for(int c=0; c<P_WIDTH; c++) if(b[0][c] == EMPTY) return c+1;
        return 1;
    }

    return bestCol + 1;
}
