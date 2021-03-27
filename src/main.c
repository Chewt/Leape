#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "board.h"
#include "io.h"

int main()
{
    srand(time(0));
    int running = 1;
    FILE* input = fdopen(0, "r");
    Board board;
    set_default(&board);
    while (running)
    {
        char* message = NULL;
        size_t size = 0;
        getline(&message, &size, input);
        char* saveptr;
        char* token = strtok_r(message, "\n", &saveptr);
        if(!strcmp(token, "uci"))
        {
            char* s = "id name Leape\nid author Hayden Johnson\nuciok\n";
            write(1, s, strlen(s));
        }
        else if (!strcmp(token, "isready"))
        {
            char* s = "readyok\n";
            write(1, s, strlen(s));
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
                printf("to move: %d\n", board.to_move);
                clock_t t = clock();
                Move bestmove = find_best_move(&board, atoi(token));
                t = clock() - t;
                double time_taken = ((double)t)/CLOCKS_PER_SEC;
                printf("Time taken: %f\n", time_taken);
                write(1, "bestmove ", 9);
                print_location(bestmove.src);
                print_location(bestmove.dest);
                write(1, "\n", 1);
            }
        }
        else if (!strcmp(token, "quit"))
        {
            running = 0;
        }
        free(message);
    }
    fclose(input);
    return 0;
}
