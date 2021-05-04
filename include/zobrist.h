#ifndef ZOBRIST_H
#define ZOBRIST_H

#define RANDOM_SIZE 781
#define HASH_SIZE 0xFFFFFFFFFFFFFFFFULL
#define TABLE_SIZE (0xFFFFFF + 17)

void zobrist_init();
void zobrist_clear();
int is_hashed(Board* board);
int get_hashed_value(Board* board);
uint64_t hash_position(Board* board);
void set_hashed_value(Board* board, int val);

#endif 
