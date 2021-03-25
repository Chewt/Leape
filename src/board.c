#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "board.h"
#include "io.h"

const uint64_t RDIAG = 0x0102040810204080;
const uint64_t LDIAG = 0x8040201008040201;
const uint64_t VERT  = 0x0101010101010101;
const uint64_t HORZ  = 0x00000000000000FF;
const uint64_t NMOV  = 0x0000000A1100110A;
const uint64_t KMOV  = 0x0000000000070507;
const uint64_t PATTK = 0x0000000000050005;

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
    board->pieces[WHITE + PAWN]   = 0x000000000000FF00;
    board->pieces[WHITE + BISHOP] = 0x0000000000000024;
    board->pieces[WHITE + KNIGHT] = 0x0000000000000042;
    board->pieces[WHITE + ROOK]   = 0x0000000000000081;
    board->pieces[WHITE + QUEEN]  = 0x0000000000000010;
    board->pieces[WHITE + KING]   = 0x0000000000000008;
    board->pieces[BLACK + PAWN]   = 0x00FF000000000000;
    board->pieces[BLACK + BISHOP] = 0x2400000000000000;
    board->pieces[BLACK + KNIGHT] = 0x4200000000000000;
    board->pieces[BLACK + ROOK]   = 0x8100000000000000;
    board->pieces[BLACK + QUEEN]  = 0x1000000000000000;
    board->pieces[BLACK + KING]   = 0x0800000000000000;
    board->castle                 = 0x2200000000000022;
    board->en_p                   = 0x0000000000000000;
    board->to_move                = WHITE;
    update_combined_pos(board);
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

int comp_cand(const void* one, const void* two)
{
    if (((Cand*)one)->weight > ((Cand*)two)->weight)
        return -1;
    else if (((Cand*)one)->weight < ((Cand*)two)->weight)
        return 1;
    return 0;
}

void move_piece(Board* board, Move* move)
{
    int i;
    for (i = 0; i < 12; ++i)
        board->pieces[i] &= ~move->dest;
    board->pieces[move->color + move->piece] ^= move->src;
    board->pieces[move->color + move->piece] |= move->dest;
    if (move->dest & board->en_p && move->piece == PAWN)
    {
        if (move->color == WHITE)
            board->pieces[BLACK + PAWN] &= ~(board->en_p >> 8);
        else
            board->pieces[WHITE + PAWN] &= ~(board->en_p << 8);
    }
    if (board->to_move == WHITE)
        board->to_move = BLACK;
    else
        board->to_move = WHITE;
    if (move->piece == PAWN && (move->dest & 0xFFULL << 24))
        board->en_p = move->dest << 8;
    else if (move->piece == PAWN && (move->dest & 0xFFULL << 32))
        board->en_p = move->dest >> 8;
    else
        board->en_p = 0x0ULL;
    update_combined_pos(board);
}

uint64_t gen_pawn_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0ULL;
    uint64_t friends = 0;
    uint64_t enemies = 0;
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
            uint64_t tmp = 0ULL;
            if (i / 8 == 1 && color == WHITE)
                tmp = 0x1ULL << (i + 16);
            else if (i / 8 == 6 && color == BLACK)
                tmp = 0x1ULL >> (i - 16);
            tmp &= ~(friends | enemies);
            moves |= tmp;
            tmp = 0ULL;
            tmp |= gen_shift(PATTK, i - 8 - 1);
            tmp &= vert_mask((i % 8) + 2);
            if ((i % 8) - 1 > 0)
                tmp &= ~vert_mask((i % 8) - 1);
            if (i / 8 < 7 && color == BLACK)
                tmp &= ~gen_shift(~(0ULL), (i / 8 - 1) * 8);
            if (i / 8 > 1 && color == WHITE)
                tmp &= ~gen_shift(~(0ULL), -64 + (i / 8 + 1) * 8);
            tmp &= enemies | board->en_p;;
            moves |= tmp;
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

uint64_t gen_bishop_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    uint64_t enemies = 0;
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

uint64_t gen_rook_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    uint64_t friends = 0;
    uint64_t enemies = 0;
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
        if (pieces >> i == 1)
            break;
    }
    return moves;
}

uint64_t gen_queen_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0;
    moves |= gen_rook_moves(board, color, pieces);
    moves |= gen_bishop_moves(board, color, pieces);
    return moves;
}

uint64_t gen_knight_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0ULL;
    uint64_t friends = 0;
    if (color == WHITE)
        friends = board->all_white;
    else
        friends = board->all_black;
    int i;
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
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

uint64_t gen_king_moves(Board* board, int color, uint64_t pieces)
{
    uint64_t moves = 0ULL;
    uint64_t friends = 0ULL;
    if (color == WHITE)
        friends = board->all_white;
    else
        friends = board->all_black;
    int i;
    for (i = 0; i < 64; ++i)
    {
        if ((0x1ULL << i) & pieces)
        {
            uint64_t tmp = 0x0ULL;
            tmp |= gen_shift(KMOV, i - 8 - 1);
            tmp &= vert_mask((i % 8) + 2);
            if ((i % 8) - 1 > 0)
                tmp &= ~vert_mask((i % 8) - 1);
            if (i / 8 < 5)
                tmp &= ~gen_shift(~(0ULL), (i / 8 + 2) * 8);
            if (i / 8 > 1)
                tmp &= ~gen_shift(~(0ULL), -64 + (i / 8 - 1) * 8);
            moves |= tmp;
            if (i == 3 && color == WHITE)
            {
                if ((board->castle & 0x02ULL) && !(friends &
                                0x06ULL))
                    moves |= 0x02ULL;
                if ((board->castle & 0x20ULL) && !(friends &
                                0x68ULL))
                    moves |= 0x20ULL;
            }
            else if (i == 59 && color == BLACK)
            {
                if ((board->castle & (0x02ULL << 7 * 8)) && 
                        ((friends & (0x06ULL << 7 * 8)) == 0))
                    moves |= 0x02ULL << 7 * 8;
                if ((board->castle & (0x20ULL << 7 * 8)) && 
                        ((friends & (0x68ULL << 7 * 8)) == 0))
                    moves |= 0x20ULL << 7 * 8;
            }
        }
        if (pieces >> i == 1)
            break;
    }
    moves &= ~friends;
    return moves;
}

void update_combined_pos(Board* board)
{
    int i;
    board->all_white = 0x0ULL;
    board->all_black = 0x0ULL;
    for (i = 0; i < 6; ++i)
    {
        board->all_white |= board->pieces[WHITE + i];
        board->all_black |= board->pieces[BLACK + i];
    }
    if (!(board->pieces[WHITE + KING] & 0x08ULL))
        board->castle &= ~0xFFULL;
    if (!(board->pieces[BLACK + KING] & (0x08ULL << 56)))
        board->castle &= ~(0xFFULL << 56);
    if (!(board->pieces[WHITE + ROOK] & 0x01ULL))
        board->castle &= ~(0x0FULL);
    if (!(board->pieces[WHITE + ROOK] & 0x80ULL))
        board->castle &= ~(0xF0ULL);
    if (!(board->pieces[BLACK + ROOK] & (0x01ULL << 56)))
        board->castle &= ~(0x0FULL << 56);
    if (!(board->pieces[BLACK + ROOK] & (0x80ULL << 56)))
        board->castle &= ~(0xF0ULL << 56);
}

uint64_t gen_all_attacks(Board* board, int color)
{
    uint64_t moves = 0x0ULL;
        moves |= gen_pawn_moves(board, color, board->pieces[color + PAWN]);
        moves |= gen_bishop_moves(board, color, board->pieces[color + BISHOP]);
        moves |= gen_knight_moves(board, color, board->pieces[color + KNIGHT]);
        moves |= gen_rook_moves(board, color, board->pieces[color + ROOK]);
        moves |= gen_queen_moves(board, color, board->pieces[color + QUEEN]);
        moves |= gen_king_moves(board, color, board->pieces[color + KING]);
    return moves;
}

int gen_all_moves(Board* board, Cand* movearr)
{
    memset(movearr, 0, sizeof(Cand) * MOVES_PER_POSITION);
    int ind = 0;
    uint64_t pieces = 0x0ULL;
    if (board->to_move == WHITE)
        pieces = board->all_white;
    else
        pieces = board->all_black;
    uint64_t lsb = pieces & -pieces;
    while (lsb)
    {
        ind += extract_moves(board, board->to_move, lsb, movearr + ind);
        pieces &= ~lsb;
        lsb = pieces & -pieces;
    }
    return ind;
}

int eval_prune(Board* board, Cand cand, int alpha, int beta, int depth)
{
    Board temp_board;
    memcpy(&temp_board, board, sizeof(Board));
    move_piece(&temp_board, &cand.move);
    if (depth == 0)
        return get_board_value(&temp_board);
    else
    {
        Cand cans[MOVES_PER_POSITION];
        gen_all_moves(&temp_board, cans);
        int i;
        int board_value;
        int temp = 0;
        if (temp_board.to_move == BLACK)
            board_value = 300;
        else
            board_value = -300;
        for (i = 0; i < MOVES_PER_POSITION; ++i)
        {
            if (cans[i].move.src == 0x0ULL)
                continue;
            if (temp_board.to_move == BLACK)
            {
                temp = eval_prune(&temp_board, cans[i], alpha, beta, depth - 1);
                if (temp < board_value)
                    board_value = temp;
                beta = (temp < beta) ? temp : beta;
                if (beta <= alpha)
                    break;
            }
            else
            {
                temp = eval_prune(&temp_board, cans[i], alpha, beta, depth - 1);
                if (temp > board_value)
                    board_value = temp;
                alpha = (temp > alpha) ? temp : alpha;
                if (beta <= alpha)
                   break;
            }
        }
        return board_value;
    }
}

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
    return white_score - black_score;
}

int extract_moves(Board* board, int color, uint64_t src, Cand* movearr)
{
    //printf("0x%016lX\n", src);
    int count = 0;
    int piece = -1;
    uint64_t moves = 0x0ULL;
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
        if (is_legal(board, temp_move))
        {
            movearr[count].move.src = src;
            movearr[count].move.dest = lsb;
            movearr[count].move.piece = piece;
            movearr[count].move.color = color;
            movearr[count].weight = 1;
            count++;
        }
        moves &= ~lsb;
        lsb = moves & -moves;
    }
    return count;
}

int is_legal(Board* board, Move move)
{
    Board temp;
    memcpy(&temp, board, sizeof(Board));
    move_piece(&temp, &move);
    if(temp.to_move == WHITE)
        return !(temp.pieces[BLACK + KING] & gen_all_attacks(&temp, WHITE));
    else
        return !(temp.pieces[WHITE + KING] & gen_all_attacks(&temp, BLACK));
}

Move find_best_move(Board* board, int depth)
{
    Cand cands[MOVES_PER_POSITION];
    gen_all_moves(board, cands);
    Cand bestmove = cands[0];
    int i;
    for (i = 0; i < MOVES_PER_POSITION; ++i)
    {
        if (cands[i].move.src == 0x0ULL)
            continue;
        cands[i].weight = eval_prune(board, cands[i], -300, 300, depth);
        print_location(cands[i].move.src);
        printf(" to ");
        print_location(cands[i].move.dest);
        printf(" weight: %d\n", cands[i].weight);
        if (cands[i].weight > bestmove.weight)
            bestmove = cands[i];
    }
    qsort(cands, MOVES_PER_POSITION, sizeof(Cand), comp_cand);
    if (bestmove.weight != 0)
    {
        for (i = 0; i < MOVES_PER_POSITION; ++i)
            if (cands[i].weight != bestmove.weight)
                break;
    }
    if (i != 0)
        bestmove = cands[rand() % i - 1];
    else
        bestmove = cands[0];
    return bestmove.move;
}
