#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define MOVES_PER_POSITION 218

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
    WHITE = 0,
    BLACK = 6
};

typedef struct
{
    uint64_t pieces[12];
    uint64_t all_white;
    uint64_t all_black;
    uint64_t en_p;
    uint64_t castle;
    int to_move;
} Board;

typedef struct
{
    uint64_t src;
    uint64_t dest;
    int piece;
    int color;
} Move;

typedef struct
{
    Move move;
    int weight;
} Cand;

extern const uint64_t RDIAG;
extern const uint64_t RDIAG;
extern const uint64_t LDIAG;
extern const uint64_t VERT;
extern const uint64_t HORZ;
extern const uint64_t NMOV;
extern const uint64_t KMOV;
extern const uint64_t PATTK;

extern const Move default_move;
 
void set_default(Board* board);
void move_piece(Board* board, Move* move);
void undo_move(Board* board, Move* move);
void update_combined_pos(Board* board);

uint64_t gen_pawn_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_bishop_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_rook_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_queen_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_knight_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_king_moves(Board* board, int color, uint64_t pieces);
uint64_t gen_all_attacks(Board* board, int color);

Move find_best_move(Board* board, int depth);
int get_board_value(Board* board);
int extract_moves(Board* board, int color, uint64_t src, Cand* movearr);
int is_legal(Board* board, Move move);

int bitScanForward(uint64_t bb);
#endif
