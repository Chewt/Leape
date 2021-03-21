#include "board.h"
#include "io.h"

int main()
{
    Board board;
    set_default(&board);
    print_board(&board);
    board.white[BISHOP] = gen_bishop_moves(&board, WHITE);
    print_board(&board);

    return 0;
}
