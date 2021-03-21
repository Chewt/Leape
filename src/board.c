#include <string.h>
#include "board.h"

void set_default(Board* board)
{
    memset(board, 0, sizeof(Board));
    board->white[PAWN]   = 0x000000000000FF00;
    board->white[BISHOP] = 0x0000000000000024;
    board->white[KNIGHT] = 0x0000000000000042;
    board->white[ROOK]   = 0x0000000000000081;
    board->white[QUEEN]  = 0x0000000000000010;
    board->white[KING]   = 0x0000000000000008;
    board->black[PAWN]   = 0x00FF000000000000;
    board->black[BISHOP] = 0x2400000000000000;
    board->black[KNIGHT] = 0x4200000000000000;
    board->black[ROOK]   = 0x8100000000000000;
    board->black[QUEEN]  = 0x1000000000000000;
    board->black[KING]   = 0x0800000000000000;
    board->castle        = 0x2200000000000022;
    board->en_p          = 0x0000000000000000;
}

void move_piece(Board* board, Move* move)
{
    if (move->color == WHITE)
    {
        board->white[move->piece] ^= move->src;
        board->white[move->piece] ^= move->dest;
    }
    else
    {
        board->black[move->piece] ^= move->src;
        board->black[move->piece] ^= move->dest;
    }
}

/* Exactly the same as move_piece */
void undo_move(Board* board, Move* move)
{
    if (move->color == WHITE)
    {
        board->white[move->piece] ^= move->src;
        board->white[move->piece] ^= move->dest;
    }
    else
    {
        board->black[move->piece] ^= move->src;
        board->black[move->piece] ^= move->dest;
    }

}

uint64_t gen_pawn_moves(Board* board, int color)
{
    uint64_t pawns = 0;
    if (color == WHITE)
    {
        pawns = board->white[PAWN];
        if (pawns < 0x000000000100 || pawns > 0x00FF00000000)
            return 0;
        pawns <<= 8;
        pawns &= 0xFFFFFFFFFF00;
    }
    else
    {
        pawns = board->black[PAWN];
        if (pawns < 0x000000000100 || pawns > 0x00FF00000000)
            return 0;
        pawns >>= 8;
        pawns &= 0x00FFFFFFFFFF;
    }
    return pawns;
}

uint64_t gen_bishop_moves(Board* board, int color)
{
    uint64_t bishops;
    if (color == WHITE)
        bishops = board->white[BISHOP];
    else
        bishops = board->black[BISHOP];
    int i;
    for (i = 0; i < 7; ++i)
    {
        bishops |= bishops >> 7;
        bishops |= bishops >> 9;
        bishops |= bishops << 7;
        bishops |= bishops << 9;
    }
    return bishops;
}
