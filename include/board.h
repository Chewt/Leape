#ifndef BOARD_H
#define BOARD_H

enum piece_type
{
    PAWN = 0,
    BISHOP,
    KNIGHT,
    ROOK,
    QUEEEN,
    KING
};

typedef struct
{
    uint64_t white[6];
    uint64_t black[6];
    uint64_t en_p;
    uint64_t castle;
} Board;

#endif
