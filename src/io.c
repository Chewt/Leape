#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "board.h"
#include "io.h"

void print_location(uint64_t board)
{
    int square = 63 - bitScanForward(board);
    char s[20];
    sprintf(s, "%c%d", square%8+'a',8-square/8);
    write(1, s, strlen(s));
}

void print_board(Board* board)
{
    int i;
    for (i = 1; i <= 64; ++i)
    {
        uint64_t square = 0x1ULL;
        square = square << (64 - i);
        if (board->pieces[PAWN] & square)
            printf("P");
        else if (board->pieces[WHITE + BISHOP] & square)
            printf("B");
        else if (board->pieces[WHITE + KNIGHT] & square)
            printf("N");
        else if (board->pieces[WHITE + ROOK] & square)
            printf("R");
        else if (board->pieces[WHITE + QUEEN] & square)
            printf("Q");
        else if (board->pieces[WHITE + KING] & square)
            printf("K");
        else if (board->pieces[BLACK + PAWN] & square)
            printf("p");
        else if (board->pieces[BLACK + BISHOP] & square)
            printf("b");
        else if (board->pieces[BLACK + KNIGHT] & square)
            printf("n");
        else if (board->pieces[BLACK + ROOK] & square)
            printf("r");
        else if (board->pieces[BLACK + QUEEN] & square)
            printf("q");
        else if (board->pieces[BLACK + KING] & square)
            printf("k");
        else
            printf(".");
        if (i % 8 == 0)
            printf("\n");
    }
    printf("\n");
}

void load_fen(Board* board, char* fen)
{
    memset(board, 0, sizeof(Board));
    char* fen_copy = malloc(strlen(fen) + 1);
    strcpy(fen_copy, fen);
    char* token = NULL;
    token = strtok(fen_copy, " ");
    int square_ind = 0;
    int i = 0;
    char curr_char = token[i];
    while (curr_char)
    {
        if (curr_char == 'p')
            board->pieces[BLACK + PAWN] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'b')
            board->pieces[BLACK + BISHOP] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'n')
            board->pieces[BLACK + KNIGHT] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'r')
            board->pieces[BLACK + ROOK] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'q')
            board->pieces[BLACK + QUEEN] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'k')
            board->pieces[BLACK + KING] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'P')
            board->pieces[WHITE + PAWN] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'B')   
            board->pieces[WHITE + BISHOP] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'N')   
            board->pieces[WHITE + KNIGHT] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'R')   
            board->pieces[WHITE + ROOK] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'Q')   
            board->pieces[WHITE + QUEEN] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char == 'K')   
            board->pieces[WHITE + KING] |= 0x1ULL << (63 - square_ind++);
        else if (curr_char <= '9' && curr_char >= '0')
            square_ind += curr_char - '0';
        i++;
        curr_char = token[i];
    }

    token = strtok(NULL, " ");
    if (token[0] == 'w')
        board->to_move = WHITE;
    else if (token[0] == 'b')
        board->to_move = BLACK;

    token = strtok(NULL, " ");
    i = 0;
    curr_char = token[i];
    while (curr_char)
    {
        if (curr_char == 'K')
            board->castle |= 0x02ULL;
        else if (curr_char == 'Q')
            board->castle |= 0x20ULL;
        else if (curr_char == 'k')
            board->castle |= 0x02ULL << 56;
        else if (curr_char == 'q')
            board->castle |= 0x20ULL << 56;
        else if (curr_char == '-')
            board->castle = 0x00ULL;
        curr_char = token[++i];
    }

    token = strtok(NULL, " ");
    if (!strcmp(token, "-"))
        board->en_p = 0x0ULL;
    else
        board->en_p = 0x01ULL << (63 - ((token[0] - 'a') + 
                    ('8' - token[1]) * 8));
    update_combined_pos(board);
    free(fen_copy);
}

