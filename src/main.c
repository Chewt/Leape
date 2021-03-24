#include <string.h>
#include <stdio.h>
#include "board.h"
#include "io.h"

int main()
{
    Board board;
    set_default(&board);
    print_board(&board);
    memset(&bboard, 0, sizeof(Board));
    //board.white[PAWN] = 0x00000000FF000000;
    int i;
    for (i = 0; i < 1000000; ++i)
    {
        gen_all_moves(&board, WHITE);
        gen_all_moves(&board, BLACK);
    }
    return 0;
}
