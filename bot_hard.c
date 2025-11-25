#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "defs.h"
#include "bot_hard.h"

#define WIDTH 7
#define HEIGHT 6
#define MIN_SCORE (-(WIDTH*HEIGHT)/2 + 3)
#define MAX_SCORE ((WIDTH*HEIGHT+1)/2 - 3)

#define TT_SIZE (1 << 23)
#define TT_MASK (TT_SIZE - 1)

typedef uint64_t bitboard_t;

typedef struct {
    bitboard_t current_position;
    bitboard_t mask;
    unsigned int moves;
} Position;

typedef struct {
    uint64_t *keys; 
    uint8_t *values;
} TranspositionTable;

typedef struct {
    uint32_t *keys;
    uint8_t *values;
    size_t size;
    int max_depth;
} OpeningBook;

typedef struct {
    bitboard_t move;
    int score;
} MoveEntry;

typedef struct {
    MoveEntry entries[WIDTH];
    int size;
} MoveSorter;

static TranspositionTable TT = {NULL, NULL};
static OpeningBook BOOK = {NULL, NULL, 0, -1};
static int column_order[WIDTH];
static unsigned long long node_count = 0;
static bitboard_t BOTTOM_MASK = 0;
static bitboard_t BOARD_MASK = 0;

static inline bitboard_t column_mask(int col) {
    return ((UINT64_C(1) << HEIGHT) - 1) << (col * (HEIGHT + 1));
}

static inline bitboard_t bottom_mask_col(int col) {
    return UINT64_C(1) << (col * (HEIGHT + 1));
}

static inline bitboard_t top_mask_col(int col) {
    return UINT64_C(1) << ((HEIGHT - 1) + col * (HEIGHT + 1));
}

static unsigned int popcount(bitboard_t m) {
    return __builtin_popcountll(m);
}

static bitboard_t compute_winning_position(bitboard_t position, bitboard_t mask) {
    bitboard_t r = 0;
    bitboard_t p;
    
    r = (position << 1) & (position << 2) & (position << 3);
    
    p = (position << (HEIGHT + 1)) & (position << (2 * (HEIGHT + 1)));
    r |= p & (position << (3 * (HEIGHT + 1)));
    r |= p & (position >> (HEIGHT + 1));
    p = (position >> (HEIGHT + 1)) & (position >> (2 * (HEIGHT + 1)));
    r |= p & (position << (HEIGHT + 1));
    r |= p & (position >> (3 * (HEIGHT + 1)));
    
    p = (position << HEIGHT) & (position << (2 * HEIGHT));
    r |= p & (position << (3 * HEIGHT));
    r |= p & (position >> HEIGHT);
    p = (position >> HEIGHT) & (position >> (2 * HEIGHT));
    r |= p & (position << HEIGHT);
    r |= p & (position >> (3 * HEIGHT));
    
    p = (position << (HEIGHT + 2)) & (position << (2 * (HEIGHT + 2)));
    r |= p & (position << 3 * (HEIGHT + 2));
    r |= p & (position >> (HEIGHT + 2));
    p = (position >> (HEIGHT + 2)) & (position >> (2 * (HEIGHT + 2)));
    r |= p & (position << (HEIGHT + 2));
    r |= p & (position >> (3 * (HEIGHT + 2)));
    
    return r & (BOARD_MASK ^ mask);
}

static bool can_play(const Position *p, int col) {
    return (p->mask & top_mask_col(col)) == 0;
}

static void play(Position *p, bitboard_t move) {
    p->current_position ^= p->mask;
    p->mask |= move;
    p->moves++;
}

static void play_col(Position *p, int col) {
    play(p, (p->mask + bottom_mask_col(col)) & column_mask(col));
}

static bitboard_t key(const Position *p) {
    return p->current_position + p->mask;
}

static bitboard_t winning_position(const Position *p) {
    return compute_winning_position(p->current_position, p->mask);
}

static bitboard_t opponent_winning_position(const Position *p) {
    return compute_winning_position(p->current_position ^ p->mask, p->mask);
}

static bitboard_t possible(const Position *p) {
    return (p->mask + BOTTOM_MASK) & BOARD_MASK;
}

static bool can_win_next(const Position *p) {
    return winning_position(p) & possible(p);
}

static bitboard_t possible_non_losing_moves(const Position *p) {
    bitboard_t possible_mask = possible(p);
    bitboard_t opponent_win = opponent_winning_position(p);
    bitboard_t forced_moves = possible_mask & opponent_win;
    
    if (forced_moves) {
        if (forced_moves & (forced_moves - 1)) {
            return 0;
        }
        possible_mask = forced_moves;
    }
    
    return possible_mask & ~(opponent_win >> 1);
}

static int move_score(const Position *p, bitboard_t move) {
    return popcount(compute_winning_position(p->current_position | move, p->mask));
}

static void partial_key3(uint64_t *k, const Position *p, int col) {
    bitboard_t pos = UINT64_C(1) << (col * (HEIGHT + 1));
    while (pos & p->mask) {
        *k *= 3;
        if (pos & p->current_position) *k += 1;
        else *k += 2;
        pos <<= 1;
    }
    *k *= 3;
}

static uint64_t key3(const Position *p) {
    uint64_t key_forward = 0;
    for (int i = 0; i < WIDTH; i++) {
        partial_key3(&key_forward, p, i);
    }
    
    uint64_t key_reverse = 0;
    for (int i = WIDTH - 1; i >= 0; i--) {
        partial_key3(&key_reverse, p, i);
    }
    
    return (key_forward < key_reverse ? key_forward : key_reverse) / 3;
}

static size_t find_next_prime(size_t n) {
    while (1) {
        bool is_prime = true;
        if (n < 2) { n = 2; continue; }
        for (size_t i = 2; i * i <= n; i++) {
            if (n % i == 0) { is_prime = false; break; }
        }
        if (is_prime) return n;
        n++;
    }
}

static bool book_load(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return false;
    
    uint8_t width, height, depth, key_bytes, val_bytes, log_size;
    
    if (fread(&width, 1, 1, f) != 1 || fread(&height, 1, 1, f) != 1 ||
        fread(&depth, 1, 1, f) != 1 || fread(&key_bytes, 1, 1, f) != 1 ||
        fread(&val_bytes, 1, 1, f) != 1 || fread(&log_size, 1, 1, f) != 1) {
        fclose(f); return false;
    }
    
    if (width != WIDTH || height != HEIGHT || val_bytes != 1) { fclose(f); return false; }
    
    size_t size = find_next_prime(1ULL << log_size);
    
    BOOK.size = size;
    BOOK.max_depth = depth;
    BOOK.keys = malloc(size * sizeof(uint32_t));
    BOOK.values = malloc(size * sizeof(uint8_t));
    
    if (!BOOK.keys || !BOOK.values) { fclose(f); return false; }
    
    if (key_bytes == 4) {
        if (fread(BOOK.keys, 4, size, f) != size) goto cleanup;
    } else if (key_bytes == 2) {
        uint16_t *temp = malloc(size * sizeof(uint16_t));
        if (!temp || fread(temp, 2, size, f) != size) { free(temp); goto cleanup; }
        for (size_t i = 0; i < size; i++) BOOK.keys[i] = temp[i];
        free(temp);
    } else {
        uint8_t *temp = malloc(size * sizeof(uint8_t));
        if (!temp || fread(temp, 1, size, f) != size) { free(temp); goto cleanup; }
        for (size_t i = 0; i < size; i++) BOOK.keys[i] = temp[i];
        free(temp);
    }
    
    if (fread(BOOK.values, 1, size, f) != size) goto cleanup;
    
    fclose(f);
    return true;

cleanup:
    free(BOOK.keys); free(BOOK.values);
    BOOK.keys = NULL; BOOK.values = NULL;
    fclose(f);
    return false;
}

static int book_get(const Position *p) {
    if (BOOK.keys == NULL || (int)p->moves > BOOK.max_depth) return 0;
    uint64_t k = key3(p);
    size_t idx = k % BOOK.size;
    if (BOOK.keys[idx] == (k & 0xFF)) return BOOK.values[idx];
    return 0;
}


static void tt_put(uint64_t k, uint8_t val) {
    size_t idx = k & TT_MASK;
    TT.keys[idx] = k;        
    TT.values[idx] = val;
}

static uint8_t tt_get(uint64_t k) {
    size_t idx = k & TT_MASK;
    return (TT.keys[idx] == k) ? TT.values[idx] : 0; 
}

static void ms_init(MoveSorter *ms) { ms->size = 0; }

static void ms_add(MoveSorter *ms, bitboard_t move, int score) {
    int pos = ms->size++;
    while (pos && ms->entries[pos - 1].score > score) {
        ms->entries[pos] = ms->entries[pos - 1];
        pos--;
    }
    ms->entries[pos].move = move;
    ms->entries[pos].score = score;
}

static bitboard_t ms_get_next(MoveSorter *ms) {
    return (ms->size > 0) ? ms->entries[--ms->size].move : 0;
}

static int negamax(const Position *P, int alpha, int beta);

static int solve(const Position *P) {
    if (can_win_next(P)) return (WIDTH * HEIGHT + 1 - (int)P->moves) / 2;
    int min = -(WIDTH * HEIGHT - (int)P->moves) / 2;
    int max = (WIDTH * HEIGHT + 1 - (int)P->moves) / 2;
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

static int negamax(const Position *P, int alpha, int beta) {
    node_count++;
    
    uint64_t k = key(P);
    int val = tt_get(k);
    
    if (val) {
        if (val > MAX_SCORE - MIN_SCORE + 1) {
            int min = val + 2 * MIN_SCORE - MAX_SCORE - 2;
            if (alpha < min) {
                alpha = min;
                if (alpha >= beta) return alpha;
            }
        } else {
            int max = val + MIN_SCORE - 1;
            if (beta > max) {
                beta = max;
                if (alpha >= beta) return beta;
            }
        }
    }

    if ((val = book_get(P))) {
        return val + MIN_SCORE - 1;
    }
    
    bitboard_t possible_moves = possible_non_losing_moves(P);
    if (possible_moves == 0) return -(WIDTH * HEIGHT - (int)P->moves) / 2;
    
    if ((int)P->moves >= WIDTH * HEIGHT - 2) return 0;
    
    int min = -(WIDTH * HEIGHT - 2 - (int)P->moves) / 2;
    if (alpha < min) {
        alpha = min;
        if (alpha >= beta) return alpha;
    }
    
    int max = (WIDTH * HEIGHT - 1 - (int)P->moves) / 2;
    if (beta > max) {
        beta = max;
        if (alpha >= beta) return beta;
    }
    
    MoveSorter moves;
    ms_init(&moves);
    
    for (int i = WIDTH - 1; i >= 0; i--) {
        bitboard_t move = possible_moves & column_mask(column_order[i]);
        if (move) ms_add(&moves, move, move_score(P, move));
    }
    
    bitboard_t next;
    while ((next = ms_get_next(&moves))) {
        Position P2 = *P;
        play(&P2, next);
        int score = -negamax(&P2, -beta, -alpha);
        if (score >= beta) {
            tt_put(k, score + MAX_SCORE - 2 * MIN_SCORE + 2);
            return score;
        }
        if (score > alpha) alpha = score;
    }
    
    tt_put(k, alpha - MIN_SCORE + 1);
    return alpha;
}

static void board_to_position(char **board, char botSym, Position *p) {
    p->current_position = 0;
    p->mask = 0;
    p->moves = 0;
    for (int col = 0; col < WIDTH; col++) {
        for (int row = 0; row < HEIGHT; row++) {
            if (board[row][col] != EMPTY) {
                int bit_row = (HEIGHT - 1) - row;
                int bit_idx = bit_row + col * (HEIGHT + 1);
                p->mask |= (UINT64_C(1) << bit_idx);
                if (board[row][col] == botSym) {
                    p->current_position |= (UINT64_C(1) << bit_idx);
                }
                p->moves++;
            }
        }
    }
}

void init_hard_bot(void) {
    static bool initialized = false;
    if (initialized) return;
    
    BOTTOM_MASK = 0;
    BOARD_MASK = 0;
    for (int i = 0; i < WIDTH; i++) {
        BOTTOM_MASK |= UINT64_C(1) << (i * (HEIGHT + 1));
        BOARD_MASK |= column_mask(i);
    }
    
    for (int i = 0; i < WIDTH; i++) {
        column_order[i] = WIDTH / 2 + (1 - 2 * (i % 2)) * (i + 1) / 2;
    }
    TT.keys = calloc(TT_SIZE, sizeof(uint64_t));
    TT.values = calloc(TT_SIZE, sizeof(uint8_t));
    
    if (!TT.keys || !TT.values) {
        fprintf(stderr, "Failed to allocate transposition table\n");
        exit(1);
    }
    
    book_load("7x6.book");
    initialized = true;
}

int getHardMove(char **board, char botSym) {
    if (!TT.keys) init_hard_bot();
    
    Position pos;
    board_to_position(board, botSym, &pos);
    
    for (int col = 0; col < WIDTH; col++) {
        if (can_play(&pos, col)) {
            bitboard_t move = (pos.mask + bottom_mask_col(col)) & column_mask(col);
            if (winning_position(&pos) & move) return col + 1; 
        }
    }
    
    int best_col = -1;
    int best_score = -99999;
    int root_order[] = {3, 2, 4, 1, 5, 0, 6};

    for (int i = 0; i < WIDTH; i++) {
        int col = root_order[i];
        
        if (can_play(&pos, col)) {
            Position p2 = pos;
            play_col(&p2, col);
            
            node_count = 0;
            int score = -solve(&p2);
            if (score > 0) return col + 1;
            
            if (score > best_score) {
                best_score = score;
                best_col = col;
            }
        }
    }
    
    if (best_col == -1) {
      
        for (int i = 0; i < WIDTH; i++) {
            int col = root_order[i];
            if (board[0][col] == EMPTY) return col + 1;
        }
        return 1;
    }
    
    return best_col + 1; 
}
