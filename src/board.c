#include "board.h"

void set_default(Board* board)
{
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
