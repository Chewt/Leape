#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

enum piece_type
{
    PAWN = 0,
    BISHOP,
    KNIGHT,
    ROOK,
    QUEEN,
    KING
};

enum color
{
    WHITE,
    BLACK
};

typedef struct
{
    uint64_t white[6];
    uint64_t black[6];
    uint64_t en_p;
    uint64_t castle;
} Board;

typedef struct
{
    uint64_t src;
    uint64_t dest;
    int piece;
    int color;
} Move;
 
void set_default(Board* board);
void move_piece(Board* board, Move* move);
void undo_move(Board* board, Move* move);

uint64_t gen_pawn_moves(Board* board, int color);
uint64_t gen_bishop_moves(Board* board, uint64_t pieces);
uint64_t gen_rook_moves(Board* board, uint64_t pieces);
uint64_t gen_queen_moves(Board* board, int color);
#endif
