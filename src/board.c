#include <string.h>
#include <stdio.h>
#include "board.h"
#include "io.h"

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

uint64_t gen_bishop_moves(Board* board, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    int i;
    for (i = 0; i < 6; ++i)
    {
        friends |= board->white[i];
        friends |= board->black[i];
    }
    for (i = 0; i < 8; ++i)
    {
        uint64_t row = 0xFFULL << 8 * i;
        moves |= (pieces >> 7 * i) & row;
        moves |= (pieces >> 9 * i) & row;
        moves |= (pieces << 7 * i) & row;
        moves |= (pieces << 9 * i) & row;
    }
    friends = ~friends;
    moves &= friends;
    return moves;
}

uint64_t gen_rook_moves(Board* board, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    int i;
    for (i = 0; i < 6; ++i)
    {
        friends |= board->white[i];
        friends |= board->black[i];
    }
    for (i = 0; i < 8; ++i)
    {
        uint64_t row = 0xFFULL << 8 * i;
        if (pieces & row)
            moves |= row;
        moves |= pieces >> 8 * i;
        moves |= pieces << 8 * i;
    }
    friends = ~friends;
    moves &= friends;
    return moves;
}

uint64_t gen_queen_moves(Board* board, int color)
{
    uint64_t moves = 0;
    if (color == WHITE)
    {
        moves |= gen_rook_moves(board, board->white[QUEEN]);
        moves |= gen_bishop_moves(board, board->white[BISHOP]);
    }
    else
    {
        moves |= gen_rook_moves(board, board->black[QUEEN]);
        moves |= gen_bishop_moves(board, board->black[BISHOP]);
    }
    return moves;
}
