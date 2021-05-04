#include <string.h>
#include <stdlib.h>
#include "board.h"
#include "zobrist.h"

uint64_t random_nums[RANDOM_SIZE];
int zobrist_hash[TABLE_SIZE];

void zobrist_init()
{
    int i;
    for (i = 0; i < RANDOM_SIZE; ++i)
        random_nums[i] = rand() % HASH_SIZE;
    zobrist_clear();
}

void zobrist_clear()
{
    memset(zobrist_hash, -1, TABLE_SIZE * sizeof(int));
}

int is_hashed(Board* board)
{
    if (zobrist_hash[hash_position(board) % TABLE_SIZE] != -1)
        return 1;
    return 0;
}

int get_hashed_value(Board* board)
{
    return zobrist_hash[hash_position(board) % TABLE_SIZE];
}

void set_hashed_value(Board* board, int val)
{
    zobrist_hash[hash_position(board) % TABLE_SIZE] = val;
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
        hash ^= random_nums[64 * 12];
    if (board->castle & 0xFULL)
        hash ^= random_nums[64 * 12 + 1];
    if (board->castle & 0xF0ULL)
        hash ^= random_nums[64 * 12 + 2];
    if (board->castle & (0xFULL << (8 * 7)))
        hash ^= random_nums[64 * 12 + 3];
    if (board->castle & (0xF0ULL << (8 * 7)))
        hash ^= random_nums[64 * 12 + 4];
    if (board->en_p)
        hash ^= random_nums[64 * 12 + 5 + 7 - (bitScanForward(board->en_p))%8];
    return hash;
}
