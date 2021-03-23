#include <string.h>
#include <stdio.h>
#include "board.h"
#include "io.h"

int main()
{
    Board board;
    set_default(&board);
    print_board(&board);
    Board bboard;
    memset(&bboard, 0, sizeof(Board));
    //board.white[PAWN] = 0x00000000FF000000;
    int i;
    for (i = 0; i < 64; ++i)
    {
        board.white[QUEEN] = 0x0000000000000001ULL << i;
        //board.white[PAWN]  = 0x000081818181FF00;
        bboard.white[QUEEN] = gen_queen_moves(&board, WHITE,
                                                        board.white[QUEEN]);
        printf("i = %d\n", i);
        print_board(&bboard);

    }
    return 0;
}
