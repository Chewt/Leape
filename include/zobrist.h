#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"

#define RANDOM_SIZE 781
#define HASH_SIZE 0xFFFFFFFFFFFFFFFFULL
#define TABLE_SIZE (0xFFFFFFF) 
#define DEFUALT_VALUE 3000

#define BLACK_TO_MOVE (64 * 12)
#define KW_CASTLE (64 * 12 + 1)
#define QW_CASTLE (64 * 12 + 2)
#define KB_CASTLE (64 * 12 + 3)
#define QB_CASTLE (64 * 12 + 4)
#define EN_P_BEGIN (64 * 12 + 5)

typedef struct
{
    uint64_t hash;
    uint8_t depth;
    int score;
} TEntry;

void zobrist_init();
void zobrist_clear();
int is_hashed(Board* board, int depth);
int get_hashed_value(Board* board);
uint64_t hash_position(Board* board);
void set_hashed_value(Board* board, int val, int depth);
void update_hash_move(Board* board, Move* move);
void update_hash_direct(Board* board, int ind);

#endif 
