#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "zobrist.h"

uint64_t random_nums[RANDOM_SIZE];
int zobrist_hash[TABLE_SIZE];

void zobrist_init()
{
    uint64_t i;
    for (i = 0; i < RANDOM_SIZE; ++i)
        random_nums[i] = rand() % HASH_SIZE;
    zobrist_clear();
}

void zobrist_clear()
{
    uint64_t i;
    for (i = 0; i < TABLE_SIZE; ++i)
        zobrist_hash[i] = DEFUALT_VALUE;
}

int is_hashed(Board* board)
{
    if (zobrist_hash[board->hash % TABLE_SIZE] != DEFUALT_VALUE)
    {
        return 1;
    }
    return 0;
}

int get_hashed_value(Board* board)
{
    return zobrist_hash[board->hash % TABLE_SIZE];
}

void set_hashed_value(Board* board, int val)
{
    zobrist_hash[board->hash % TABLE_SIZE] = val;
}

uint64_t hash_position(Board* board)
{
    int i;
    uint64_t hash = 0ULL;
    for (i = 0; i < 12; ++i)
    {
        uint64_t pieces = board->pieces[i];
        uint64_t lsb = pieces & -pieces;
        while (lsb)
        {
            int ind = bitScanForward(lsb);
            hash ^= random_nums[ind + 64 * i];
            pieces &= ~lsb;
            lsb = pieces & -pieces;
        }
    }
    if (board->to_move == BLACK)
        hash ^= random_nums[BLACK_TO_MOVE];
    if (board->castle & 0xFULL)
        hash ^= random_nums[KW_CASTLE];
    if (board->castle & 0xF0ULL)
        hash ^= random_nums[QW_CASTLE];
    if (board->castle & (0xFULL << (8 * 7)))
        hash ^= random_nums[KB_CASTLE];
    if (board->castle & (0xF0ULL << (8 * 7)))
        hash ^= random_nums[QB_CASTLE];
    if (board->en_p)
        hash ^= random_nums[EN_P_BEGIN + 7 - ((bitScanForward(board->en_p))%8)];
    return hash;
}

void update_hash_move(Board* board, Move* move)
{
    int ind = bitScanForward(move->src);
    board->hash ^= random_nums[64 * (move->piece + move->color) + ind];
    ind = bitScanForward(move->dest);
    board->hash ^= random_nums[64 * (move->piece + move->color) + ind];
}

void update_hash_direct(Board* board, int ind)
{
    board->hash ^= random_nums[ind];
}
