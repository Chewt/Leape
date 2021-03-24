#include <stdio.h>
#include "board.h"
#include "io.h"

void print_board(Board* board)
{
    int i;
    for (i = 1; i <= 64; ++i)
    {
        uint64_t square = 0x1ULL;
        square = square << (64 - i);
        if (board->white[PAWN] & square)
            printf("P");
        else if (board->white[BISHOP] & square)
            printf("B");
        else if (board->white[KNIGHT] & square)
            printf("N");
        else if (board->white[ROOK] & square)
            printf("R");
        else if (board->white[QUEEN] & square)
            printf("Q");
        else if (board->white[KING] & square)
            printf("K");
        else if (board->black[PAWN] & square)
            printf("p");
        else if (board->black[BISHOP] & square)
            printf("b");
        else if (board->black[KNIGHT] & square)
            printf("n");
        else if (board->black[ROOK] & square)
            printf("r");
        else if (board->black[QUEEN] & square)
            printf("q");
        else if (board->black[KING] & square)
            printf("k");
        else
            printf(".");
        if (i % 8 == 0)
            printf("\n");
    }
    printf("\n");
}
