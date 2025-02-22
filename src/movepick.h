

#ifndef MOVEPICK_H
#define MOVEPICK_H

#include "types.h"

void InitAllMoves(MoveList* moves, Move hashMove, SearchData* data);
void InitTacticalMoves(MoveList* moves, SearchData* data, int cutoff);
void InitPerftMoves(MoveList* moves, Board* board);
Move NextMove(MoveList* moves, Board* board, int skipQuiets);

void PrintMoves(Board* board, ThreadData* thread);

#endif