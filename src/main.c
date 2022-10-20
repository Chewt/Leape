/*******************************************************************************
                         _                          
                        | |    ___  __ _ _ __   ___ 
                        | |   / _ \/ _` | '_ \ / _ \
                        | |__|  __/ (_| | |_) |  __/
                        |_____\___|\__,_| .__/ \___|
                                        |_|         

*******************************************************************************/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "board.h"
#include "io.h"
#include "zobrist.h"

int main()
{
    srand(12345);
    zobrist_init();
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
        if (strlen(message) == 1)
            continue;
        char* saveptr;
        char* token = strtok_r(message, "\n", &saveptr);
        if(!strcmp(token, "uci"))
        {
            char* s = "id name Leape develop\nid author Hayden Johnson\nuciok\n";
            if (write(1, s, strlen(s)) == -1)
                perror("from main");
        }
        else if (!strcmp(token, "isready"))
        {
            char* s = "readyok\n";
            if (write(1, s, strlen(s)) == -1)
                perror("from main");
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
            if (token && !strcmp(token, "startpos"))
            {
                set_default(&board);
            }
            token = strtok_r(NULL, " ", &saveptr);
            if (token && !strcmp(token, "moves"))
            {
                token = strtok_r(NULL, " ", &saveptr);
                while (token)
                {
                    Move move;
                    move.src = square_to_bit(token);
                    move.dest = square_to_bit(token + 2);
                    find_piece(&board, &move);
                    move.promote = -1;
                    if (token[4] != '\n')
                    {
                        switch (token[4])
                        {
                            case 'b':
                                move.promote = BISHOP;
                                break;
                            case 'n':
                                move.promote = KNIGHT;
                                break;
                            case 'r':
                                move.promote = ROOK;
                                break;
                            case 'q':
                                move.promote = QUEEN;
                                break;
                        }
                    }
                    move_piece(&board, &move);
                    add_position(&board);
                    token = strtok_r(NULL, " ", &saveptr);
                }
            }
        }
        else if (!strcmp(token, "go"))
        {
            token = strtok_r(NULL, " ", &saveptr);
            while (token)
            {
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

                    struct timeval timecheck;
                    long t;
                    gettimeofday(&timecheck, NULL);
                    t = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec /
                        1000;
                    Move bestmove = find_best_move(&board, atoi(token));
                    gettimeofday(&timecheck, NULL);
                    t = (long)timecheck.tv_sec * 1000 + (long)timecheck.tv_usec / 1000 - t;
                    double time_taken = (double)t / 1000;
                    char s[100];
                    sprintf(s, "Time taken: %.6f seconds\n", time_taken);
                    if(write(2, s, strlen(s)) == -1)
                        perror("from main");
                    if(write(1, "bestmove ", 9) == -1)
                        perror("from main");
                    print_location(1, bestmove.src);
                    print_location(1, bestmove.dest);
                    if (bestmove.promote == BISHOP)
                    {
                        if(write(1, "b", 1) == -1)
                            perror("from main");
                    }
                    else if (bestmove.promote == KNIGHT)
                    {
                        if(write(1, "n", 1) == -1)
                            perror("from main");
                    }
                    else if (bestmove.promote == ROOK)
                    {
                        if(write(1, "r", 1) == -1)
                            perror("from main");
                    }
                    else if (bestmove.promote == QUEEN)
                    {
                        if(write(1, "q", 1) == -1)
                            perror("from main");
                    }
                    if(write(1, "\n", 1) == -1)
                        perror("from main");
                }
                token = strtok_r(NULL, " ", &saveptr);
            }
        }
        else if (!strcmp(token, "quit"))
        {
            running = 0;
        }
        else if (!strcmp(token, "printboard"))
        {
            print_board(&board);
        }
        else if (!strcmp(token, "perft"))
        {
            token = strtok_r(NULL, " ", &saveptr);
            int depth = atoi(token);
            Pres pres = perft(&board, depth);
            printf("%ld nodes at %d depth\n", pres.nodes, depth);
            printf("%ld captures, %ld en pessants\n", pres.caps, pres.eps);
            printf("%ld checks, %ld checkmates\n", pres.checks, pres.checkmates);
            printf("%ld castles, %ld promotions\n", pres.castles, pres.proms);
        }
        free(message);
    }
    fclose(input);
    return 0;
}
