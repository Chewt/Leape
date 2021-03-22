#include <string.h>
#include "board.h"
#include "io.h"

int main()
{
    Board board;
    set_default(&board);
    print_board(&board);
    Board bboard;
    memset(&bboard, 0, sizeof(Board));
    board.white[ROOK] = 0x0000000810000000;
    bboard.white[ROOK] = gen_rook_moves(&board, board.white[ROOK]);
    print_board(&bboard);

    return 0;
}
