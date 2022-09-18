#ifndef IO_H
#define IO_H

void print_board(Board* board);
void print_move(int fd, Move* move);
void print_location(int fd, uint64_t board);
void load_fen(Board* board, char* fen);
uint64_t square_to_bit(char* square);
void find_piece(Board* board, Move* move);

#endif
