#ifndef IO_H
#define IO_H

void print_board(Board* board);
void print_move(Move* move);
void print_location(uint64_t board);
void load_fen(Board* board, char* fen);

#endif
