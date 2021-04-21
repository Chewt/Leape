#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
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
        if (getline(&message, &size, input) == -1)
        {
            perror("Error reading from STDIN");
            exit(-1);
        }
        char* saveptr;
        char* token = strtok_r(message, "\n", &saveptr);
        if(!strcmp(token, "uci"))
        {
            char* s = "id name Leape 1.1\nid author Hayden Johnson\nuciok\n";
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

                /*
                if (is_stalemate(&board, board.to_move))
                    printf("Is stalemate!\n");
                else
                    printf("Is not stalemate!\n");
                if (is_checkmate(&board, board.to_move))
                    printf("Is checkmate!\n");
                else
                    printf("Is not checkmate!\n");
                //print_board(&board);

                Board b;
                memset(&b, 0, sizeof(Board));
                b.pieces[WHITE + PAWN] = gen_pawn_moves(&board, BLACK, board.pieces[BLACK + PAWN]);
                print_board(&b);
                */

                printf("to move: %d\n", board.to_move);
                struct timeval timecheck;
                long t;
                gettimeofday(&timecheck, NULL);
                t = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec /
                    1000;
                Move bestmove = find_best_move(&board, atoi(token));
                gettimeofday(&timecheck, NULL);
                t = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000 - t;
                double time_taken = (double)t / 1000;
                printf("Time taken: %f seconds\n", time_taken);
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
        else if (!strcmp(token, "perft"))
        {
            token = strtok_r(NULL, " ", &saveptr);
            int depth = atoi(token);
            uint64_t nodes = perft(&board, depth);
            printf("%ld nodes at %d depth\n", nodes, depth);
        }
        free(message);
    }
    fclose(input);
    return 0;
}
