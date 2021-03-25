#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "board.h"
#include "io.h"

int main()
{
    srand(time(0));
    int running = 1;
    Board board;
    set_default(&board);
    while (running)
    {
        char* message = NULL;
        size_t size = 0;
        getline(&message, &size, stdin);
        char* saveptr;
        char* token = strtok_r(message, "\n", &saveptr);
        if(!strcmp(token, "uci"))
        {
            printf("id name Leape\nid author Hayden Johnson\nuciok\n");
        }
        else if (!strcmp(token, "isready"))
        {
            printf("readyok\n");
        }
        else
            token = strtok_r(message, " ", &saveptr);
        if (token && !strcmp(token, "position"))
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token && !strcmp(token, "fen"))
            {
                token = strtok_r(NULL, "\n", &saveptr);
                load_fen(&board, token);
            }
        }
        else if (!strcmp(token, "go"))
        {
            token = strtok_r(NULL, " ", &saveptr);
            if (token && !strcmp(token, "depth"))
            {
                token = strtok_r(NULL, " ", &saveptr);
                print_board(&board);
                Move bestmove = find_best_move(&board, atoi(token));
                printf("bestmove ");
                print_location(bestmove.src);
                print_location(bestmove.dest);
                printf("\n");
            }
        }
        else if (!strcmp(token, "quit"))
        {
            running = 0;
        }
        free(message);
    }
    return 0;
}
