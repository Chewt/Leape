#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#define MOVES_PER_POSITION 218

#define RANK_1 0x00000000000000FFULL
#define RANK_2 0x000000000000FF00ULL
#define RANK_3 0x0000000000FF0000ULL
#define RANK_4 0x00000000FF000000ULL
#define RANK_5 0x000000FF00000000ULL
#define RANK_6 0x0000FF0000000000ULL
#define RANK_7 0x00FF000000000000ULL
#define RANK_8 0xFF00000000000000ULL

#define EMPTY  0x0000000000000000ULL

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
    uint64_t black_attacks;
    uint64_t white_attacks;
    uint64_t en_p;
    uint64_t castle;
    int to_move;
    uint64_t hash;
} Board;

typedef struct
{
    uint64_t src;
    uint64_t dest;
    int piece;
    int color;
    int promote;
} Move;

typedef struct
{
    Move move;
    int weight;
} Cand;

typedef struct
{
    uint64_t nodes;
    uint64_t caps;
    uint64_t eps;
    uint64_t checks;
    uint64_t checkmates;
    uint64_t castles;
    uint64_t proms;
} Pres;

extern const uint64_t RDIAG;
extern const uint64_t RDIAG;
extern const uint64_t LDIAG;
extern const uint64_t VERT;
extern const uint64_t HORZ;
extern const uint64_t NMOV;
extern const uint64_t KMOV;
extern const uint64_t PATTK;

extern const Move default_move;

#define LINE_LENGTH 20
extern Move current_line[LINE_LENGTH];
 
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
void apply_heuristics(Board* board, Cand* cand);
int get_piece_value(Board* board, int color, uint64_t piece);
int is_checkmate(Board* board, int color);
int will_be_checkmate(Board* board, int color, Move* move);
int will_be_check(Board* board, int color, Move* move);
int is_stalemate(Board* board, int color);

int bitScanForward(uint64_t bb);

Pres perft(Board* board, int depth);
void get_nodes(Board* board, Cand* cand, int depth, Pres* pres);
#endif
