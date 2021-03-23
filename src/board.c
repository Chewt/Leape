#include <string.h>
#include <stdio.h>
#include "board.h"
#include "io.h"

const uint64_t RDIAG = 0x0102040810204080;
const uint64_t LDIAG = 0x8040201008040201;
const uint64_t VERT  = 0x0101010101010101;
const uint64_t HORZ  = 0x00000000000000FF;
const uint64_t NMOV  = 0x0000000A1100110A;
const uint64_t KMOV  = 0x0000000000070507;

/* Table used for determining bit index */
const int index64[64] = {
    0, 47,  1, 56, 48, 27,  2, 60,
   57, 49, 41, 37, 28, 16,  3, 61,
   54, 58, 35, 52, 50, 42, 21, 44,
   38, 32, 29, 23, 17, 11,  4, 62,
   46, 55, 26, 59, 40, 36, 15, 53,
   34, 51, 20, 43, 31, 22, 10, 45,
   25, 39, 14, 33, 19, 30,  9, 24,
   13, 18,  8, 12,  7,  6,  5, 63
};

/*
 * bitScanForward
 * @author Kim Walisch (2012)
 * @param bb bitboard to scan
 * @precondition bb != 0
 * @return index (0..63) of least significant one bit
 */
int bitScanForward(uint64_t bb) 
{
    if (bb == 0)
        return 0;
    const uint64_t debruijn64 = 0x03f79d71b4cb0a89ULL;
    return index64[((bb ^ (bb-1)) * debruijn64) >> 58];
}

/*
 * bitScanReverse
 * @authors Kim Walisch, Mark Dickinson
 * @param bb bitboard to scan
 * @precondition bb != 0
 * @return index (0..63) of most significant one bit
 */
int bitScanReverse(uint64_t bb) 
{
    if (bb == 0)
        return 0;
    const uint64_t debruijn64 = 0x03f79d71b4cb0a89ULL;
    bb |= bb >> 1;
    bb |= bb >> 2;
    bb |= bb >> 4;
    bb |= bb >> 8;
    bb |= bb >> 16;
    bb |= bb >> 32;
    return index64[(bb * debruijn64) >> 58];
}

uint64_t vert_mask(int count)
{
    if (count == 1)
        return 0x0101010101010101;
    else if (count == 2)
        return 0x0303030303030303;
    else if (count == 3)
        return 0x0707070707070707;
    else if (count == 4)
        return 0x0F0F0F0F0F0F0F0F;
    else if (count == 5)
        return 0x1F1F1F1F1F1F1F1F;
    else if (count == 6)
        return 0x3F3F3F3F3F3F3F3F;
    else if (count == 7)
        return 0x7F7F7F7F7F7F7F7F;
    else
        return 0xFFFFFFFFFFFFFFFF;
}

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

uint64_t gen_shift(uint64_t src, int count)
{
    if (bitScanReverse(src) + count < 0 || bitScanForward(src) + count > 63)
        return 0ULL;
    if (count < 0)
        return src >> -count;
    else
        return src << count;
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

uint64_t gen_pawn_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t pawns = pieces;
        if (pawns < 0x000000000100 || pawns > 0x00FF00000000)
            return 0;
    if (color == WHITE)
    {
        pawns <<= 8;
        pawns &= 0xFFFFFFFFFF00;
    }
    else
    {
        pawns >>= 8;
        pawns &= 0x00FFFFFFFFFF;
    }
    return pawns;
}

uint64_t gen_bishop_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    uint64_t enemies = 0;
    int i;
    for (i = 0; i < 6; ++i)
    {
        if (color == WHITE)
        {
            friends |= board->white[i];
            enemies |= board->black[i];
        }
        else
        {
            friends |= board->black[i];
            enemies |= board->white[i];
        }
    }
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            friends &= ~(0x1ULL << i);

            /* Generate top right diagonal moves */
            uint64_t intersection;
            intersection = gen_shift(RDIAG, i) & (friends | enemies);
            int lsbi = bitScanForward(intersection);
            int capt = 0;
            if ((0x1ULL << lsbi) & friends & intersection)
                capt = -7;
            uint64_t tmp;
            tmp = gen_shift(RDIAG, i);
            if (intersection)
                tmp ^= gen_shift(RDIAG, lsbi + capt);
            tmp &= vert_mask(i % 8);
            if (i % 8 > 0)
                moves |= tmp;

            /* Generate top left diagonal moves */
            intersection = gen_shift(LDIAG, i + 9) & (friends | enemies);
            lsbi = bitScanForward(intersection);
            capt = 0;
            if ((0x1ULL << lsbi) & friends & intersection)
                capt = -9;
            tmp = gen_shift(LDIAG, i + 9);
            if (intersection)
                tmp ^= gen_shift(LDIAG, lsbi + 9 + capt);
            tmp &= ~vert_mask((i % 8) + 1);
            if (i / 8 < 7)
                moves |= tmp;

            /* Generate bottom left diagonal moves */
            intersection = gen_shift(RDIAG, -63 + i) & (friends | enemies);
            lsbi = bitScanReverse(intersection);
            capt = 0;
            if ((0x1ULL << lsbi) & friends & intersection)
                capt = 7;
            tmp = gen_shift(RDIAG, -63 + i);
            if (intersection)
                tmp ^= gen_shift(RDIAG, -63 + lsbi + capt);
            tmp &= ~vert_mask((i % 8) + 1);
            moves |= tmp;

            /* Generate bottom right diagonal moves */
            intersection = gen_shift(LDIAG, -63 - 9 + i) & (friends | enemies);
            lsbi = bitScanReverse(intersection);
            capt = 0;
            if ((0x1ULL << lsbi) & friends & intersection)
                capt = 9;
            tmp = gen_shift(LDIAG, -63 - 9 + i);
            if (intersection)
                tmp ^= gen_shift(LDIAG, -63 - 9 + lsbi + capt);
            tmp &= vert_mask((i % 8));
            if (i % 8 > 0)
                moves |= tmp;
        }
    }
    return moves;
}

uint64_t gen_rook_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    uint64_t enemies = 0;
    int i;
    for (i = 0; i < 6; ++i)
    {
        if (color == WHITE)
        {
            friends |= board->white[i];
            enemies |= board->black[i];
        }
        else
        {
            friends |= board->black[i];
            enemies |= board->white[i];
        }
    }
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            friends &= ~(0x1ULL << i);

            /* Generate north moves */
            uint64_t intersection;
            intersection = gen_shift(VERT, 8 + i) & (friends | enemies);
            int lsbi = bitScanForward(intersection);
            int capt = 0;
            if (((0x1ULL << lsbi) & friends) && intersection)
                capt = -8;
            uint64_t tmp;
            tmp = gen_shift(VERT, 8 + i);
            if (intersection)
                tmp ^= gen_shift(VERT, lsbi + 8 + capt);
            tmp &= gen_shift(~0ULL, i + 8);
            if (i / 8 < 7)
                moves |= tmp;

            /* Generate south moves */
            intersection = gen_shift(VERT, -64 + i) & (friends | enemies);
            lsbi = bitScanReverse(intersection);
            capt = 0;
            if (((0x1ULL << lsbi) & friends) && intersection)
                capt = 8;
            tmp = gen_shift(VERT, -64 + i);
            if (intersection)
                tmp ^= gen_shift(VERT, -64 + lsbi + capt);
            moves |= tmp;

            /* Generate west moves */
            intersection = gen_shift(HORZ, 1 + i) & (friends | enemies);
            lsbi = bitScanForward(intersection);
            capt = 0;
            if (((0x1ULL << lsbi) & friends) && intersection)
                capt = -1;
            tmp = gen_shift(HORZ, 1 + i);
            if (intersection)
                tmp ^= gen_shift(HORZ, 1 + lsbi + capt);
            tmp &= 0xFFULL << (i / 8) * 8;
            moves |= tmp;

            /* Generate east moves */
            intersection = gen_shift(HORZ, -8 + i) & (friends | enemies);
            lsbi = bitScanReverse(intersection);
            capt = 0;
            if (((0x1ULL << lsbi) & friends) && intersection)
                capt = 1;
            tmp = gen_shift(HORZ, -8 + i);
            if (intersection)
                tmp ^= gen_shift(HORZ, -8 + lsbi + capt);
            tmp &= 0xFFULL << (i / 8) * 8;
            moves |= tmp;
        }
    }
    return moves;
}

uint64_t gen_queen_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    if (color == WHITE)
    {
        moves |= gen_rook_moves(board, WHITE, pieces);
        moves |= gen_bishop_moves(board, WHITE, pieces);
    }
    else
    {
        moves |= gen_rook_moves(board, BLACK, pieces);
        moves |= gen_bishop_moves(board, BLACK, pieces);
    }
    return moves;
}

uint64_t gen_knight_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0ULL;
    uint64_t friends = 0;
    uint64_t enemies = 0;
    int i;
    for (i = 0; i < 6; ++i)
    {
        if (color == WHITE)
        {
            friends |= board->white[i];
            enemies |= board->black[i];
        }
        else
        {
            friends |= board->black[i];
            enemies |= board->white[i];
        }
    }
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            uint64_t tmp = 0ULL;
            tmp |= gen_shift(NMOV, i - 16 - 2);
            tmp &= vert_mask((i % 8) + 3);
            if ((i % 8) - 3 > 0)
                tmp &= ~vert_mask((i % 8) - 2);
            if (i / 8 < 5)
                tmp &= ~gen_shift(~(0ULL), (i / 8 + 3) * 8);
            if (i / 8 > 2)
                tmp &= ~gen_shift(~(0ULL), -64 + (i / 8 - 2) * 8);
            moves |= tmp;
            break;
        }
    }
    moves &= ~friends;
    return moves;
}
