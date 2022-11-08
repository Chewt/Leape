#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "board.h"
#include "io.h"
#include "zobrist.h"

char* position_hashes = NULL;

/* Constants for piece attacks */
const uint64_t RDIAG = 0x0102040810204080ULL;
const uint64_t LDIAG = 0x8040201008040201ULL;
const uint64_t VERT  = 0x0101010101010101ULL;
const uint64_t HORZ  = 0x00000000000000FFULL;
const uint64_t NMOV  = 0x0000000A1100110AULL;
const uint64_t KMOV  = 0x0000000000070507ULL;
const uint64_t PATTK = 0x0000000000050005ULL;

Move current_line[LINE_LENGTH];

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

/*******************************************************************************
 * Returns a bit mask for a vertical portion of the bit board
 *
 * @param count The number of columns starting from the right side of the board
 *              to include in the bit mask 
 ******************************************************************************/
uint64_t vert_mask(int count)
{
    if (count == 1)
        return 0x0101010101010101ULL;
    else if (count == 2)
        return 0x0303030303030303ULL;
    else if (count == 3)
        return 0x0707070707070707ULL;
    else if (count == 4)
        return 0x0F0F0F0F0F0F0F0FULL;
    else if (count == 5)
        return 0x1F1F1F1F1F1F1F1FULL;
    else if (count == 6)
        return 0x3F3F3F3F3F3F3F3FULL;
    else if (count == 7)
        return 0x7F7F7F7F7F7F7F7FULL;
    else
        return 0xFFFFFFFFFFFFFFFFULL;
}

/*******************************************************************************
 * Sets the board to a default chess position
 *
 * @param board The board to set to a default position
 ******************************************************************************/
void set_default(Board* board)
{
    if (position_hashes)
        free(position_hashes);
    position_hashes = calloc(TABLE_SIZE, sizeof(char));
    memset(board, 0, sizeof(Board));
    board->pieces[WHITE + PAWN]   = 0x000000000000FF00ULL;
    board->pieces[WHITE + BISHOP] = 0x0000000000000024ULL;
    board->pieces[WHITE + KNIGHT] = 0x0000000000000042ULL;
    board->pieces[WHITE + ROOK]   = 0x0000000000000081ULL;
    board->pieces[WHITE + QUEEN]  = 0x0000000000000010ULL;
    board->pieces[WHITE + KING]   = 0x0000000000000008ULL;
    board->pieces[BLACK + PAWN]   = 0x00FF000000000000ULL;
    board->pieces[BLACK + BISHOP] = 0x2400000000000000ULL;
    board->pieces[BLACK + KNIGHT] = 0x4200000000000000ULL;
    board->pieces[BLACK + ROOK]   = 0x8100000000000000ULL;
    board->pieces[BLACK + QUEEN]  = 0x1000000000000000ULL;
    board->pieces[BLACK + KING]   = 0x0800000000000000ULL;
    board->castle                 = 0x2200000000000022ULL;
    board->en_p                   = 0x0000000000000000ULL;
    board->to_move                = WHITE;
    update_combined_pos(board);
    board->hash = hash_position(board);
    add_position(board);
    memset(current_line, 0, sizeof(Move) * LINE_LENGTH);
}

/*******************************************************************************
 * A generalized bitwise shift function that shifts left for positive numbers
 * and shifts right for negative numbers
 *
 * @param src A unsigned 64 bit number to apply the shift to
 * @param count The number of digits to shift the number
 * @return the new shifted number
 ******************************************************************************/
uint64_t gen_shift(uint64_t src, int count)
{
    if (bitScanReverse(src) + count < 0 || bitScanForward(src) + count > 63)
        return EMPTY;
    if (count < 0)
        return src >> -count;
    else
        return src << count;
}

/*******************************************************************************
 * A function to compare two Cand structs by their attribute "weight". 
 * This was implemented for use in the qsort function
 *
 * @param one The "left" side of the expression
 * @param two The "right" side of the expression
 * @return An indicator as to which argument is of greater value than the other
 ******************************************************************************/
int comp_cand(const void* one, const void* two)
{
    if (((Cand*)one)->weight > ((Cand*)two)->weight)
        return -1;
    else if (((Cand*)one)->weight < ((Cand*)two)->weight)
        return 1;
    return 0;
}

/*******************************************************************************
 * Updates the board to the result of moving the requested piece
 *
 * @param board The board to make the move on
 * @param move The move to make on the board
 ******************************************************************************/
void move_piece(Board* board, Move* move)
{
    int i;
    for (i = 0; i < 12; ++i)
    {
        if (board->pieces[i] & move->dest)
        {
            board->pieces[i] &= ~move->dest;
            int ind = bitScanForward(move->dest);
            update_hash_direct(board, 64 * i + ind);
        }
    }
    board->pieces[move->color + move->piece] ^= move->src;
    board->pieces[move->color + move->piece] ^= move->dest;
    update_hash_move(board, move);

    if (move->promote != -1)
    {
        board->pieces[move->color + move->piece] ^= move->dest;
        board->pieces[move->color + move->promote] ^= move->dest;
        update_hash_direct(board, 64 * (move->color + move->piece) +
                bitScanForward(move->dest));
        update_hash_direct(board, 64 * (move->color + move->promote) +
                bitScanForward(move->dest));
    }

    if (board->en_p && (move->dest & board->en_p) && (move->piece == PAWN))
    {
        if (move->color == WHITE)
        {
            board->pieces[BLACK + PAWN] &= ~(board->en_p >> 8);
            update_hash_direct(board, 64 * (PAWN + BLACK) +
                    bitScanForward(board->en_p >> 8));
        }
        else
        {
            board->pieces[WHITE + PAWN] &= ~(board->en_p << 8);
            update_hash_direct(board, 64 * (PAWN + WHITE) +
                    bitScanForward(board->en_p << 8));
        }
    }

    if (move->piece == KING && move->color == WHITE)
    {
        if ((move->src & 0x8ULL) && (move->dest & 0x2ULL))
        {
            board->pieces[WHITE + ROOK] ^= 0x1ULL;
            board->pieces[WHITE + ROOK] ^= 0x4ULL;
            update_hash_direct(board, 64 * (ROOK + WHITE));
            update_hash_direct(board, 64 * (ROOK + WHITE) + 2);
        }
        else if ((move->src & 0x8ULL) && (move->dest & 0x20ULL))
        {
            board->pieces[WHITE + ROOK] ^= 0x80ULL;
            board->pieces[WHITE + ROOK] ^= 0x10ULL;
            update_hash_direct(board, 64 * (ROOK + WHITE) + 7);
            update_hash_direct(board, 64 * (ROOK + WHITE) + 4);
        }
    }
    if (move->piece == KING && move->color == BLACK)
    {
        if ((move->src & (0x8ULL << 56)) && (move->dest & (0x2ULL << 56)))
        {
            board->pieces[BLACK + ROOK] ^= 0x1ULL << 56;
            board->pieces[BLACK + ROOK] ^= 0x4ULL << 56;
            update_hash_direct(board, 64 * (ROOK + BLACK) + 56);
            update_hash_direct(board, 64 * (ROOK + BLACK) + 58);
        }
        else if ((move->src & (0x8ULL << 56)) && (move->dest & (0x20ULL << 56)))
        {
            board->pieces[BLACK + ROOK] ^= 0x80ULL << 56;
            board->pieces[BLACK + ROOK] ^= 0x10ULL << 56;
            update_hash_direct(board, 64 * (ROOK + BLACK) + 63);
            update_hash_direct(board, 64 * (ROOK + BLACK) + 60);
        }
    }

    if (board->to_move == WHITE)
        board->to_move = BLACK;
    else
        board->to_move = WHITE;
    update_hash_direct(board, BLACK_TO_MOVE);

    if (board->en_p)
    {
        update_hash_direct(board, EN_P_BEGIN + 7 -
                ((bitScanForward(board->en_p))%8));
        board->en_p = EMPTY;
    }

    if (move->color == WHITE && move->piece == PAWN && 
            (move->dest & RANK_4) && (move->src & RANK_2))
        board->en_p = move->dest >> 8;
    else if (move->color == BLACK && move->piece == PAWN && 
            (move->dest & RANK_5) && (move->src & RANK_7))
        board->en_p = move->dest << 8;

    if (board->en_p)
    {
        update_hash_direct(board, EN_P_BEGIN + 7 -
                ((bitScanForward(board->en_p))%8));
    }

    update_combined_pos(board);
}

/*******************************************************************************
 * Generates all possible legal pawn moves for a given bitboard of pawns
 *
 * @param board The board to base the generated moves on
 * @param color The color of the pawn to generate moves for
 * @param pieces The bitboard of pawns to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_pawn_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = EMPTY;
    uint64_t friends = EMPTY;
    uint64_t enemies = EMPTY;
    if (color == WHITE)
    {
        friends = board->all_white;
        enemies = board->all_black;
    }
    else
    {
        friends = board->all_black;
        enemies = board->all_white;
    }
    int i;
    if (color == WHITE)
        moves |= pieces << 8;
    else
        moves |= pieces >> 8;
    moves &= ~(friends | enemies);

    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            uint64_t tmp = EMPTY;
            if (i / 8 == 1 && !((0x1ULL << (i + 8)) & (friends | enemies)) &&
                    color == WHITE)
                tmp = gen_shift(0x1ULL, i + 16);
            else if (i / 8 == 6 && !((0x1ULL << (i - 8)) & (friends | enemies))
                    && color == BLACK)
                tmp = gen_shift(0x1ULL, i - 16);
            tmp &= ~(friends | enemies);
            moves |= tmp;
            tmp = 0ULL;
            tmp |= gen_shift(PATTK, i - 8 - 1);
            tmp &= vert_mask((i % 8) + 2);
            if ((i % 8) - 1 > 0)
                tmp &= ~vert_mask((i % 8) - 1);
            if (i / 8 < 7 && color == BLACK)
                tmp &= ~gen_shift(~(0ULL), (i / 8) * 8);
            if (i / 8 > 0 && color == WHITE)
                tmp &= ~gen_shift(~(0ULL), -56 + (i / 8) * 8);
            tmp &= enemies | board->en_p;
            moves |= tmp;
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

/*******************************************************************************
 * Generates all possible legal bishop moves for a given bitboard of bishop
 *
 * @param board The board to base the generated moves on
 * @param color The color of the bishops to generate moves for
 * @param pieces The bitboard of bishops to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_bishop_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = EMPTY;
    uint64_t friends = EMPTY;
    uint64_t enemies = EMPTY;
    if (color == WHITE)
    {
        friends = board->all_white;
        enemies = board->all_black;
    }
    else
    {
        friends = board->all_black;
        enemies = board->all_white;
    }
    int i;
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
        if (pieces >> i == 1)
            break;
    }
    return moves;
}

/*******************************************************************************
 * Generates all possible legal rook moves for a given bitboard of rook
 *
 * @param board The board to base the generated moves on
 * @param color The color of the rooks to generate moves for
 * @param pieces The bitboard of rooks to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_rook_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves   = EMPTY;
    uint64_t friends = EMPTY;
    uint64_t enemies = EMPTY;
    if (color == WHITE)
    {
        friends = board->all_white;
        enemies = board->all_black;
    }
    else
    {
        friends = board->all_black;
        enemies = board->all_white;
    }
    int i;
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
            tmp &= gen_shift(~EMPTY, i + 8);
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
            tmp &= RANK_1 << (i / 8) * 8;
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
            tmp &= RANK_1 << (i / 8) * 8;
            moves |= tmp;
        }
        if (pieces >> i == 1)
            break;
    }
    return moves;
}

/*******************************************************************************
 * Generates all possible legal queen moves for a given bitboard of queen
 *
 * @param board The board to base the generated moves on
 * @param color The color of the queens to generate moves for
 * @param pieces The bitboard of queens to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_queen_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = EMPTY;
    moves |= gen_rook_moves(board, color, pieces);
    moves |= gen_bishop_moves(board, color, pieces);
    return moves;
}

/*******************************************************************************
 * Generates all possible legal knight moves for a given bitboard of queen
 *
 * @param board The board to base the generated moves on
 * @param color The color of the knights to generate moves for
 * @param pieces The bitboard of knights to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_knight_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves   = EMPTY;
    uint64_t friends = EMPTY;
    if (color == WHITE)
        friends = board->all_white;
    else
        friends = board->all_black;
    int i;
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            uint64_t tmp = EMPTY;
            tmp |= gen_shift(NMOV, i - 16 - 2);
            tmp &= vert_mask((i % 8) + 3);
            if ((i % 8) - 3 > 0)
                tmp &= ~vert_mask((i % 8) - 2);
            if (i / 8 < 5)
                tmp &= ~gen_shift(~(EMPTY), (i / 8 + 3) * 8);
            if (i / 8 > 2)
                tmp &= ~gen_shift(~(EMPTY), -64 + (i / 8 - 2) * 8);
            moves |= tmp;
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

/*******************************************************************************
 * Generates all possible legal king moves for a given bitboard of queen
 *
 * @param board The board to base the generated moves on
 * @param color The color of the king to generate moves for
 * @param pieces The bitboard of king to generate moves for
 * @return A bitboard containing the generated moves
 ******************************************************************************/
uint64_t gen_king_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves   = EMPTY;
    uint64_t friends = EMPTY;
    if (color == WHITE)
        friends = board->all_white;
    else
        friends = board->all_black;
    int i;
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            uint64_t tmp = EMPTY;
            tmp |= gen_shift(KMOV, i - 8 - 1);
            tmp &= vert_mask((i % 8) + 2);
            if ((i % 8) - 1 > 0)
                tmp &= ~vert_mask((i % 8) - 1);
            if (i / 8 < 5)
                tmp &= ~gen_shift(~(EMPTY), (i / 8 + 2) * 8);
            if (i / 8 > 1)
                tmp &= ~gen_shift(~(EMPTY), -64 + (i / 8 - 1) * 8);
            moves |= tmp;
            if (i == 3 && color == WHITE)
            {
                if ((board->castle & 0x02ULL) &&
                        !((board->all_white|board->all_black) & 0x06ULL)) 
                    moves |= 0x02ULL;
                if ((board->castle & 0x20ULL) &&
                        !((board->all_white|board->all_black) & 0x70ULL))
                    moves |= 0x20ULL;
            }
            else if (i == 59 && color == BLACK)
            {
                if ((board->castle & (0x02ULL << 56)) && 
                       !((board->all_white|board->all_black) & (0x06ULL << 56)))
                    moves |= 0x02ULL << 56;
                if ((board->castle & (0x20ULL << 56)) && 
                       !((board->all_white|board->all_black) & (0x70ULL << 56)))
                    moves |= 0x20ULL << 56;
            }
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

/*******************************************************************************
 * Updates various attributes of the given board struct regarding board state
 * that may have changed such as castling rights and combined locations of all
 * pieces
 *
 * @param board The board to update
 ******************************************************************************/
void update_combined_pos(Board* board)
{
    int i;
    board->all_white = EMPTY;
    board->all_black = EMPTY;
    for (i = 0; i < 6; ++i)
    {
        board->all_white |= board->pieces[WHITE + i];
        board->all_black |= board->pieces[BLACK + i];
    }
    if (!(board->pieces[WHITE + KING] & 0x08ULL) && (board->castle & RANK_1))
    {
        if (board->castle & 0xFULL)
            update_hash_direct(board, KW_CASTLE);
        if (board->castle & 0xF0ULL)
            update_hash_direct(board, QW_CASTLE);
        board->castle &= ~RANK_1;
    }
    if (!(board->pieces[BLACK + KING] & (0x08ULL << 56)) && (board->castle &
                RANK_7))
    {
        if (board->castle & (0xFULL << (8 * 7)))
            update_hash_direct(board, KB_CASTLE);
        if (board->castle & (0xF0ULL << (8 * 7)))
            update_hash_direct(board, QB_CASTLE);
        board->castle &= ~RANK_7;
    }
    if (!(board->pieces[WHITE + ROOK] & 0x01ULL) && (board->castle & 0x0FULL))
    {
        board->castle &= ~(0x0FULL);
        update_hash_direct(board, KW_CASTLE);
    }
    if (!(board->pieces[WHITE + ROOK] & 0x80ULL) && (board->castle & 0xF0ULL))
    {
        board->castle &= ~(0xF0ULL);
        update_hash_direct(board, QW_CASTLE);
    }
    if (!(board->pieces[BLACK + ROOK] & (0x01ULL << 56)) && (board->castle &
                (0x0FULL << 56)))
    {
        board->castle &= ~(0x0FULL << 56);
        update_hash_direct(board, KB_CASTLE);
    }
    if (!(board->pieces[BLACK + ROOK] & (0x80ULL << 56)) && (board->castle &
                (0xF0ULL << 56)))
    {
        board->castle &= ~(0xF0ULL << 56);
        update_hash_direct(board, QB_CASTLE);
    }
}

/*******************************************************************************
 * Generates a bitboard with all enemy moves on it, with no information 
 * regarding what type of piece is making that move.
 *
 * @param board The board to generate the moves from
 * @param color The color of the side you wish to generate the moves from
 * @return The bitboard containing the attacks
 ******************************************************************************/
uint64_t gen_all_dests(Board* board, int color)
{
    uint64_t moves = EMPTY;
    moves |= gen_pawn_moves(board, color, board->pieces[color + PAWN]);
    moves |= gen_bishop_moves(board, color, board->pieces[color + BISHOP]);
    moves |= gen_knight_moves(board, color, board->pieces[color + KNIGHT]);
    moves |= gen_rook_moves(board, color, board->pieces[color + ROOK]);
    moves |= gen_queen_moves(board, color, board->pieces[color + QUEEN]);
    moves |= gen_king_moves(board, color, board->pieces[color + KING]);
    return moves;
}

/*******************************************************************************
 * Populates an array with all possible moves on the current board. It will
 * only generate moves for the side that is indicated by the attribute to_move
 * 
 * @param board The board to generate moves from
 * @param movearr The array of moves to populate with the created moves. Its 
 *                length will always be of the constant MOVES_PER_POSITION
 ******************************************************************************/
int gen_all_moves(Board* board, Cand* movearr)
{
    memset(movearr, 0, sizeof(Cand) * MOVES_PER_POSITION);
    int nmoves = 0;
    uint64_t pieces = EMPTY;
    if (board->to_move == WHITE)
        pieces = board->all_white;
    else
        pieces = board->all_black;
    uint64_t lsb = pieces & -pieces;
    while (lsb)
    {
        nmoves += extract_moves(board, board->to_move, lsb, movearr + nmoves);
        pieces &= ~lsb;
        lsb = pieces & -pieces;
    }
    return nmoves;
}

/*******************************************************************************
 * Populates an array with all possible moves on the current board that attack
 * an enemy piece. It will only generate moves for the side that is indicated by
 * the Board attribute to_move
 *
 * @param board The board to generate moves from
 * @param movearr The array of moves to populate with the created moves. Its
 *                length will always be of the constant MOVES_PER_POSITION
 ******************************************************************************/
int gen_all_attack_moves(Board* board, Cand* movearr)
{
    memset(movearr, 0, sizeof(Cand) * MOVES_PER_POSITION);
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(board, cans);
    uint64_t pieces;
    if (board->to_move == WHITE)
        pieces = board->all_black;
    else
        pieces = board->all_white;
    int num_attacks, i;
    num_attacks = 0;
    for (i = 0; i < num_moves; ++i) {
        if (cans[i].move.dest & pieces)
        {
            memcpy(movearr + num_attacks, cans + i, sizeof(Cand));
            num_attacks++;
        }
    }
    return num_attacks;
}

/*******************************************************************************
 * Populates an array with all possible moves on the current board that attack
 * an specific square. It will only generate moves for the side that is indicated
 * by the Board attribute to_move
 *
 * @param board   The board to generate moves from
 * @param movearr The array of moves to populate with the created moves. Its
 *                length will always be of the constant MOVES_PER_POSITION
 * @param square  The target square to find moves for
 ******************************************************************************/
int gen_all_attacks_on_square(Board* board, Cand* movearr, uint64_t square)
{
    memset(movearr, 0, sizeof(Cand) * MOVES_PER_POSITION);
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(board, cans);
    int num_attacks, i;
    num_attacks = 0;
    for (i = 0; i < num_moves; ++i) {
        if (cans[i].move.dest & square)
        {
            memcpy(movearr + num_attacks, cans + i, sizeof(Cand));
            num_attacks++;
        }
    }
    return num_attacks;
}

void remove_position(Board* board)
{
    position_hashes[board->hash % TABLE_SIZE]--;
}
int alphaBetaMin_attack(Board* board, Cand* cand, int alpha, int beta, int depth);
int alphaBetaMax_attack(Board* board, Cand* cand, int alpha, int beta, int depth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand->move);
    if (is_stalemate(&temp_board, temp_board.to_move))
        return 5;
    if(is_checkmate(&temp_board, temp_board.to_move))
        return (-300 + depth);
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_attacks_on_square(&temp_board, cans, cand->move.dest);
    if (!num_moves)
    {
        int bv = get_board_value(&temp_board);
        return get_board_value(&temp_board);
    }
    int i;
    int score;
    for (i = 0; i < num_moves; ++i)
    {
        score = alphaBetaMin_attack(&temp_board, &cans[i], alpha, beta, depth + 1);
        if (score >= beta)
            return beta;
        if (score > alpha)
            alpha = score;
    }
    return alpha;
}

int alphaBetaMin_attack(Board* board, Cand* cand, int alpha, int beta, int depth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand->move);
    if (is_stalemate(&temp_board, temp_board.to_move))
        return -5;
    if(is_checkmate(&temp_board, temp_board.to_move))
        return (300 - depth);
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_attacks_on_square(&temp_board, cans, cand->move.dest);
    if (!num_moves)
    {
        int bv = get_board_value(&temp_board);
        return bv;
    }
    int i;
    int score;
    for (i = 0; i < num_moves; ++i)
    {
        score = alphaBetaMax_attack(&temp_board, &cans[i], alpha, beta, depth + 1);
        if (score <= alpha)
            return alpha;
        if (score < beta)
            beta = score;
    }
    return beta;
}

int alphaBetaMin(Board* board, Cand* cand, int alpha, int beta, int depth, int startDepth);
int alphaBetaMax(Board* board, Cand* cand, int alpha, int beta, int depth, int startDepth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand->move);
    add_position(&temp_board);
    if (is_threefold(&temp_board))
    {
        remove_position(&temp_board);
        return 5;
    }
    if (is_hashed(&temp_board, depth))
    {
        remove_position(&temp_board);
        return get_hashed_value(&temp_board);
    }
    if (is_stalemate(&temp_board, temp_board.to_move))
    {
        remove_position(&temp_board);
        return 5;
    }
    if(is_checkmate(&temp_board, temp_board.to_move))
    {
        remove_position(&temp_board);
        return (-300 - depth);
    }
    if (!depth)
    {
        int bv = get_board_value(&temp_board);
        set_hashed_value(&temp_board, bv, startDepth);
        remove_position(&temp_board);
        return get_board_value(&temp_board);
    }
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(&temp_board, cans);
    int i;
    int score;
    for (i = 0; i < num_moves; ++i)
    {
        score = alphaBetaMin(&temp_board, &cans[i], alpha, beta, depth - 1, startDepth);
        if (score >= beta)
        {
            remove_position(&temp_board);
            return beta;
        }
        if (score > alpha)
            alpha = score;
    }
    remove_position(&temp_board);
    return alpha;
}

int alphaBetaMin(Board* board, Cand* cand, int alpha, int beta, int depth, int startDepth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand->move);
    add_position(&temp_board);
    if (is_threefold(&temp_board))
    {
        remove_position(&temp_board);
        return -5;
    }
    if (is_hashed(&temp_board, depth))
    {
        remove_position(&temp_board);
        return get_hashed_value(&temp_board);
    }
    if (is_stalemate(&temp_board, temp_board.to_move))
    {
        remove_position(&temp_board);
        return 0;
    }
    if(is_checkmate(&temp_board, temp_board.to_move))
    {
        remove_position(&temp_board);
        return (300 + depth);
    }
    if (!depth)
    {
        int bv = get_board_value(&temp_board);
        set_hashed_value(&temp_board, bv, startDepth);
        remove_position(&temp_board);
        return bv;
    }
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(&temp_board, cans);
    int i;
    int score;
    for (i = 0; i < num_moves; ++i)
    {
        score = alphaBetaMax(&temp_board, &cans[i], alpha, beta, depth - 1, startDepth);
        if (score <= alpha)
        {
            remove_position(&temp_board);
            return alpha;
        }
        if (score < beta)
            beta = score;
    }
    remove_position(&temp_board);
    return beta;
}

/*******************************************************************************
 * Minimax algo with alpha-beta pruning to find the best chess move.
 *
 * @param board The board to find the best move for
 * @param cand The candidate move used to start the search. The candidate move 
 *             is made on a temporary board, then the best move among the 
 *             resulting board is found
 * @param alpha Used for alpha-beta pruning. It is the the highest value seen so
 *             far in the search.
 * @param beta Used for alpha-beta pruning. It is the lowest value seen so far
 *             in the search.
 * @param depth The depth to which the function should search
 ******************************************************************************/
int eval_prune(Board* board, Cand* cand, int alpha, int beta, int depth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand->move);
    if (is_threefold(&temp_board))
        return 0;
    if (is_hashed(&temp_board, depth))
        return get_hashed_value(&temp_board);
    if (is_stalemate(&temp_board, temp_board.to_move))
        return 0;
    if (depth == 0)
    {
        int bv = get_board_value(&temp_board);
        //set_hashed_value(&temp_board, bv);
        //current_line[0] = cand->move;
        return bv;
    }
    else
    {
        Cand cans[MOVES_PER_POSITION];
        int num_moves = gen_all_moves(&temp_board, cans);
        int i;
        int board_value;
        int temp = 0;
        int broken = 0;
        Move* bestmove = &cans[0].move;
        if (temp_board.to_move == BLACK)
            board_value = 300;
        else
            board_value = -300;
        for (i = 0; i < num_moves; ++i)
        {
            if (cans[i].move.src == EMPTY)
                break;
            if (temp_board.to_move == BLACK)
            {
                temp = eval_prune(&temp_board,&cans[i], alpha, beta, depth - 1);
                if (temp < board_value)
                {
                    board_value = temp;
                    bestmove = &cans[i].move;
                }
                beta = (temp < beta) ? temp : beta;
                if (beta <= alpha)
                {
                    broken = 1;
                    //memset(current_line, 0, sizeof(Move) * depth);
                    break;
                }
            }
            else
            {
                temp = eval_prune(&temp_board,&cans[i], alpha, beta, depth - 1);
                if (temp > board_value)
                {
                    board_value = temp;
                    bestmove = &cans[i].move;
                }
                alpha = (temp > alpha) ? temp : alpha;
                if (beta <= alpha)
                {
                    broken = 1;
                    //memset(current_line, 0, sizeof(Move) * depth);
                    break;
                }
            }
        }
        //if (!broken)
            //set_hashed_value(&temp_board, board_value);
        current_line[depth - 1] = *bestmove;
        return board_value;
    }
}

/*******************************************************************************
 * Returns the material value of the board in terms of the pieces on the board.
 * black positive values are good for white, while negative values are good for 
 * black.
 *
 * @param board The board that gets evaluated
 ******************************************************************************/
int get_board_value(Board* board)
{
    int white_score = 0;
    int black_score = 0;
    uint64_t all_pieces = board->all_white | board->all_black;
    uint64_t lsb = all_pieces & -all_pieces;
    while (lsb)
    {
        if (lsb & board->pieces[WHITE + PAWN])
            white_score += 1;
        else if (lsb & board->pieces[WHITE + BISHOP])
            white_score += 3;
        else if (lsb & board->pieces[WHITE + KNIGHT])
            white_score += 3;
        else if (lsb & board->pieces[WHITE + ROOK])
            white_score += 5;
        else if (lsb & board->pieces[WHITE + QUEEN])
            white_score += 9;
        else if (lsb & board->pieces[BLACK + PAWN])
            black_score += 1;
        else if (lsb & board->pieces[BLACK + BISHOP])
            black_score += 3;
        else if (lsb & board->pieces[BLACK + KNIGHT])
            black_score += 3;
        else if (lsb & board->pieces[BLACK + ROOK])
            black_score += 5;
        else if (lsb & board->pieces[BLACK + QUEEN])
            black_score += 9;
        all_pieces &= ~lsb;
        lsb = all_pieces & -all_pieces;
    }
    if (board->to_move == WHITE)
        return white_score - black_score;
    else
        return black_score - white_score;
}

/*******************************************************************************
 * Extracts all the moves from a bitboard and creates a Move struct for each of
 * them and puts them in the given array.
 *
 * @param board The board to extract the moves from. It is used to verify that
 *              the move being extracted is legal.
 * @param color The color of the pieces that are making the moves
 * @param src The bitboard that contains the piece that will be making the moves
 * @param movearr The array of candidate moves to be filled with the extracted
 *                moves. It is guaranteed to be of length MOVES_PER_POSITION. 
 ******************************************************************************/
int extract_moves(Board* board, int color, uint64_t src, Cand* movearr)
{
    //printf("0x%016lX\n", src);
    int count = 0;
    int piece = -1;
    uint64_t moves = EMPTY;
    if (src & board->pieces[color + PAWN])
    {
        moves = gen_pawn_moves(board, color, src);
        piece = PAWN;
    }
    else if (src & board->pieces[color + BISHOP])
    {
        moves = gen_bishop_moves(board, color, src);
        piece = BISHOP;
    }
    else if (src & board->pieces[color + KNIGHT])
    {
        moves = gen_knight_moves(board, color, src);
        piece = KNIGHT;
    }
    else if (src & board->pieces[color + ROOK])
    {
        moves = gen_rook_moves(board, color, src);
        piece = ROOK;
    }
    else if (src & board->pieces[color + QUEEN])
    {
        moves = gen_queen_moves(board, color, src);
        piece = QUEEN;
    }
    else if (src & board->pieces[color + KING])
    {
        moves = gen_king_moves(board, color, src);
        piece = KING;
    }
    uint64_t lsb = moves & -moves;
    while (lsb)
    {
        Move temp_move;
        temp_move.src = src;
        temp_move.dest = lsb;
        temp_move.piece = piece;
        temp_move.color = color;
        temp_move.promote = -1;
        if (is_legal(board, temp_move))
        {
            if (piece == PAWN && (lsb & (RANK_1 | RANK_8)))
            {
                int i;
                for (i = BISHOP; i <= QUEEN; ++i)
                {
                    movearr[count].move.src = src;
                    movearr[count].move.dest = lsb;
                    movearr[count].move.piece = piece;
                    movearr[count].move.color = color;
                    movearr[count].move.promote = i;
                    movearr[count].weight = 1;
                    apply_heuristics(board, movearr + count);
                    count++;

                }
            }
            else
            {
                movearr[count].move.src = src;
                movearr[count].move.dest = lsb;
                movearr[count].move.piece = piece;
                movearr[count].move.color = color;
                movearr[count].move.promote = -1;
                movearr[count].weight = 1;
                apply_heuristics(board, movearr + count);
                count++;
            }
        }
        moves &= ~lsb;
        lsb = moves & -moves;
    }
    return count;
}

/*******************************************************************************
 * Determines if making the proposed move will result in the friendly king being
 * put in check
 *
 * @param board The board to apply the move to
 * @param move The proposed move that will be checked
 ******************************************************************************/
int is_legal(Board* board, Move move)
{
    Board temp;
    memcpy(&temp, board, sizeof(Board));
    move_piece(&temp, &move);
    uint64_t all_attacks;
    if(board->to_move == WHITE)
    {
        all_attacks = gen_all_dests(&temp, BLACK);
        if (move.piece == KING && (0xEULL & all_attacks) && (move.dest &
                    (board->castle & 0xFULL)))
            return 0;
        if (move.piece == KING && (0x38ULL & all_attacks) && (move.dest &
                    (board->castle & 0xF0ULL)))
            return 0;
        return !(temp.pieces[WHITE + KING] & all_attacks);
    }
    else
    {
        all_attacks = gen_all_dests(&temp, WHITE);
        if (move.piece == KING && ((0xEULL << 56) & all_attacks) &&
                (move.dest & (board->castle & (0xFULL << 56))))
            return 0;
        if (move.piece == KING && ((0x38ULL << 56) & all_attacks) &&
                (move.dest & (board->castle & (0xF0ULL << 56))))
            return 0;
        return !(temp.pieces[BLACK + KING] & all_attacks);
    }
}

/*******************************************************************************
 * Returns the best move to make in a given position when searched at a given
 * depth
 *
 * @param board The board to search for the best move
 * @param depth The depth at which to search the board
 * @param time  The time left on the clock in msec
 ******************************************************************************/
Move find_best_move(Board* board, int depth, int time)
{
    Cand cands[MOVES_PER_POSITION];
    Cand bestmove;
    bestmove.weight = -9001;
    int num_attack_moves = gen_all_attack_moves(board, cands);
    int i;
    for (i = 0; i < num_attack_moves; ++i)
    {
        if (cands[i].move.src == EMPTY)
            break;
        int temp_weight = 0;
        temp_weight = -alphaBetaMax_attack(board, &cands[i], -300, 300, 0);
        cands[i].weight = temp_weight;
        print_move(2, &cands[i].move);
        char s[100];
        sprintf(s, " attack weight: %d ", cands[i].weight);
        if(write(2, s, strlen(s)) == -1)
            perror("from find_best_move");
        int m;
        if(write(2, "\n", 1) == -1)
            perror("from find_best_move");
        if (cands[i].weight > bestmove.weight)
            bestmove = cands[i];
    }
    int bv = get_board_value(board);
    if (bv < 0)
        bv *= -1;
    if (bestmove.weight > bv)
        return bestmove.move;

    if (time < 60000 && time >= 0)
        depth--;
    int num_moves = gen_all_moves(board, cands);
    qsort(cands, MOVES_PER_POSITION, sizeof(Cand), comp_cand);
    bestmove.weight = -9001;
    int j;
    for (j = 1; j <= depth; ++j)
    {
        //zobrist_clear();
        for (i = 0; i < num_moves; ++i)
        {
            if (cands[i].move.src == EMPTY)
                break;
            //int temp_weight = eval_prune(board, &cands[i], -300, 300, j - 1);
            //int temp_weight = alphaBetaMax(board, &cands[i], -300, 300, j - 1);
            int temp_weight = 0;
            //if (board->to_move == WHITE)
            //temp_weight = alphaBetaMin(board, &cands[i], -300, 300, j - 1);
            //else 
            temp_weight = -alphaBetaMax(board, &cands[i], -300, 300, j - 1,
                    j - 1);
            cands[i].weight = temp_weight;
            if (j == depth)
            {
                //current_line[j] = cands[i].move;
                print_move(2, &cands[i].move);
                char s[100];
                sprintf(s, " weight: %d ", cands[i].weight);
                if(write(2, s, strlen(s)) == -1)
                    perror("from find_best_move");
                int m;
                for (m = LINE_LENGTH - 1; m >= 0; --m)
                {
                    if (current_line[m].src)
                    {
                        print_move(2, &current_line[m]);
                        if(write(2, " ", 1) == -1)
                            perror("from find_best_move");
                    }
                }
                if(write(2, "\n", 1) == -1)
                    perror("from find_best_move");
                memset(current_line, 0, sizeof(Move) * LINE_LENGTH);
                if (cands[i].weight > bestmove.weight)
                    bestmove = cands[i];
            }
        }
        qsort(cands, num_moves, sizeof(Cand), comp_cand);
    }
    for (i = 0; i < num_moves; ++i)
    {
        if (cands[i].weight != bestmove.weight)
            break;
    }
    if (i - 1 != 0)
        bestmove = cands[rand() % (i - 1)];
    else
        bestmove = cands[0];
    return bestmove.move;
}

/*******************************************************************************
 * Gives a move a weight based on the following heuristics:
 * Is the square the piece is moving to worth more than itself?
 * Is the square the piece is moving to occupied by an enemy piece?
 * Does the move result in the enemy king being put in check?
 * Is the target square not attacked by any enemy piece?
 * Will the move result in checkmate?
 *
 * @param board The board to check the move against
 * @param cand The candidate move to apply these heuristics to.
 ******************************************************************************/
void apply_heuristics(Board* board, Cand* cand)
{
    if (cand->move.color == WHITE)
    {
        if (get_piece_value(board, BLACK, cand->move.dest) >
                get_piece_value(board, WHITE, cand->move.src))
            cand->weight += 1;
        if (cand->move.dest & board->all_black)
            cand->weight += 1;
        if (will_be_check(board, WHITE, &cand->move))
            cand->weight += 1;
        if (cand->move.dest & ~(gen_all_dests(board, BLACK)))
            cand->weight += 3;
        //        if (will_be_checkmate(board, BLACK, &cand->move))
        //           cand->weight += 600;
    }
    else
    {
        if (get_piece_value(board, WHITE, cand->move.dest) >
                get_piece_value(board, BLACK, cand->move.src))
            cand->weight += 1;
        if (cand->move.dest & board->all_white)
            cand->weight += 1;
        if (will_be_check(board, BLACK, &cand->move))
            cand->weight += 1;
        if (cand->move.dest & ~(gen_all_dests(board, WHITE)))
            cand->weight += 3;
        //if (will_be_checkmate(board, WHITE, &cand->move))
        //cand->weight += 600;
    }
}

/*******************************************************************************
 * Gets the material value of the given piece or piece
 *
 * @param board The board from which the piece came from
 * @param color The color of the piece to check
 * @param pieces The piece whos value is to be returned
 ******************************************************************************/
int get_piece_value(Board* board, int color, uint64_t piece)
{
    if (piece & board->pieces[color + PAWN])
        return 1;
    if (piece & board->pieces[color + BISHOP])
        return 3;
    if (piece & board->pieces[color + KNIGHT])
        return 3;
    if (piece & board->pieces[color + ROOK])
        return 5;
    if (piece & board->pieces[color + QUEEN])
        return 9;
    return 0;
}

/*******************************************************************************
 * Checks if the current boardstate is checkmate for the given color
 *
 * @param board The board to check
 * @param color The color of the king to check for checkmate
 ******************************************************************************/
int is_checkmate(Board* board, int color)
{
    int ecolor = (color == BLACK) ? WHITE : BLACK;
    if (!(board->pieces[KING + color] & gen_all_dests(board, ecolor)))
        return 0;

    int i;
    for (i = 0; i < 6; ++i)
    {
        uint64_t src = board->pieces[i + color];
        uint64_t lsb_src = src & -src;
        while (lsb_src)
        {
            uint64_t moves = EMPTY;
            int piece = -1;
            if (i == PAWN)
            {
                moves = gen_pawn_moves(board, color, lsb_src);
                piece = PAWN;
            }
            else if (i == BISHOP)
            {
                moves = gen_bishop_moves(board, color, lsb_src);
                piece = BISHOP;
            }
            else if (i == KNIGHT)
            {
                moves = gen_knight_moves(board, color, lsb_src);
                piece = KNIGHT;
            }
            else if (i == ROOK)
            {
                moves = gen_rook_moves(board, color, lsb_src);
                piece = ROOK;
            }
            else if (i == QUEEN)
            {
                moves = gen_queen_moves(board, color, lsb_src);
                piece = QUEEN;
            }
            else if (i == KING)
            {
                moves = gen_king_moves(board, color, lsb_src);
                piece = KING;
            }
            uint64_t lsb = moves & -moves;
            while (lsb)
            {
                Move temp_move;
                temp_move.src = lsb_src;
                temp_move.dest = lsb;
                temp_move.piece = piece;
                temp_move.color = color;
                temp_move.promote = -1;
                if (is_legal(board, temp_move))
                    return 0;
                moves &= ~lsb;
                lsb = moves & -moves;
            }
            src &= ~lsb_src;
            lsb_src = src & -src;
        }
    }
    return 1;
}

/*******************************************************************************
 * Checks if making a move will result in checkmate
 *
 * @param board the board to test for checkmate
 * @param color the color of the side to check checkmate for
 * @param move the move to test if it results in checkmate
 ******************************************************************************/
int will_be_checkmate(Board* board, int color, Move* move)
{
    Board temp;
    memcpy(&temp, board, sizeof(Board));
    move_piece(&temp, move);
    if (is_checkmate(&temp, color))
        return 1;
    return 0;
}

/*******************************************************************************
 * Checks if making a move will result in the enemy king being in check
 *
 * @param board The board to test the move on 
 * @param color The color of the piece that makes the move
 * @param move The move to test if it results in check
 ******************************************************************************/
int will_be_check(Board* board, int color, Move* move)
{
    Board temp;
    memcpy(&temp, board, sizeof(Board));
    move_piece(&temp, move);
    if (color == WHITE)
    {
        if (board->pieces[BLACK + KING] & gen_all_dests(&temp, WHITE))
            return 1;
    }
    else
    {
        if (board->pieces[WHITE + KING] & gen_all_dests(&temp, BLACK))
            return 1;
    }
    return 0;
}

/*******************************************************************************
 * Checks if the current boardstate is stalemate for the given color
 *
 * @param board The board to check
 * @param color The color of the side to check for stalemate
 ******************************************************************************/
int is_stalemate(Board* board, int color)
{
    Board temp;
    memcpy(&temp, board, sizeof(Board));
    int i;
    if (color == WHITE)
    {
        uint64_t katt = gen_king_moves(board, WHITE, board->pieces[WHITE +
                KING]);
        for (i = 0; i < 6; ++i)
            temp.pieces[i + BLACK] &= ~katt;
        uint64_t all_attacks = gen_all_dests(&temp, BLACK);
        if (!(board->pieces[WHITE + KING] & gen_all_dests(board, BLACK)))
            if (!(gen_all_dests(board, WHITE) & ~all_attacks))
                return 1;
    }
    else if (color == BLACK)
    {
        uint64_t katt = gen_king_moves(board, BLACK, board->pieces[BLACK +
                KING]);
        for (i = 0; i < 6; ++i)
            temp.pieces[i + WHITE] &= ~katt;
        uint64_t all_attacks = gen_all_dests(&temp, WHITE);
        if (!(board->pieces[BLACK + KING] & gen_all_dests(board, WHITE)))
            if (!(gen_all_dests(board, BLACK) & ~all_attacks))
                return 1;
    }
    return 0;
}

/*******************************************************************************
 * Perft function to find the node count of a certain depth
 *
 * @param board The board to calculate for
 * @param depth The depth at which to search
 * @return the number of nodes found
 ******************************************************************************/
Pres perft(Board* board, int depth)
{
    uint64_t nodes = 0;
    Pres pres;
    memset(&pres, 0, sizeof(Pres));
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(board, cans);
    int i;
    for (i = 0; i < num_moves; ++i)
    {
        if (cans[i].move.src == EMPTY)
            break;
        Board temp_board;
        memcpy(&temp_board, board, sizeof(Board));
        move_piece(&temp_board, &cans[i].move);
        get_nodes(board, &(cans[i]), depth - 1, &pres);
    }
    return pres;
}

/*******************************************************************************
 * Recursive function that implements Perft function
 ******************************************************************************/
void get_nodes(Board* board, Cand* cand, int depth, Pres* pres)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    if (depth == 0)
    {
        if (board->to_move == BLACK && (board->all_white & cand->move.dest))
            pres->caps++;
        else if (board->to_move == WHITE && (board->all_black & cand->move.dest))
            pres->caps++;
        if ((board->en_p & cand->move.dest) && cand->move.piece == PAWN)
        {
            pres->caps++;
            pres->eps++;
        }
        if (cand->move.piece == KING && cand->move.color == WHITE)
        {
            if ((cand->move.src & 0x8ULL) && (cand->move.dest & 0x22ULL))
            {
                pres->castles++;
                //print_board(&temp_board);
            }
        }
        if (cand->move.piece == KING && cand->move.color == BLACK)
        {
            if ((cand->move.src & (0x8ULL << 56)) && (cand->move.dest &
                        (0x22ULL << 56)))
            {
                pres->castles++;
                //print_board(&temp_board);
            }
        }
        if (cand->move.promote != -1)
            pres->proms++;
        pres->nodes++;
    }
    move_piece(&temp_board, &cand->move);
    if (depth == 0)
    {
        if (temp_board.to_move == BLACK && (temp_board.pieces[BLACK + KING] &
                    gen_all_dests(&temp_board, WHITE)))
            pres->checks++;
        if (temp_board.to_move == WHITE && (temp_board.pieces[WHITE + KING] &
                    gen_all_dests(&temp_board, BLACK)))
            pres->checks++;
        if (is_checkmate(&temp_board, temp_board.to_move))
        {
            pres->checkmates++;
            //print_board(&temp_board);
        }
        if (cand->move.piece == KING && cand->move.color == WHITE)
        {
            if ((cand->move.src & 0x8ULL) && (cand->move.dest & 0x22ULL))
            {
                //print_board(&temp_board);
            }
        }
        if (cand->move.piece == KING && cand->move.color == BLACK)
        {
            if ((cand->move.src & (0x8ULL << 56)) && (cand->move.dest &
                        (0x22ULL << 56)))
            {
                //print_board(&temp_board);
            }
        }

        return;
    }
    Cand cans[MOVES_PER_POSITION];
    int num_moves = gen_all_moves(&temp_board, cans);
    int i;
    for (i = 0; i < num_moves; ++i)
    {
        if (cans[i].move.src == EMPTY)
            break;
        get_nodes(&temp_board, &(cans[i]), depth - 1, pres);
    }
}

void add_position(Board* board)
{
    position_hashes[board->hash % TABLE_SIZE]++;
}

int is_threefold(Board* board)
{
    if (position_hashes[board->hash % TABLE_SIZE] >= 2)
    {
        //printf("Found a match!\n");
        return 1;
    }
    return 0;
}
