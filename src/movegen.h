

#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

extern const int MVV_LVA[12][12];

extern const BitBoard PROMOTION_RANKS[];
extern const BitBoard HOME_RANKS[];
extern const BitBoard THIRD_RANKS[];
extern const int PAWN_DIRECTIONS[];

extern const int HASH_MOVE_SCORE;
extern const int GOOD_CAPTURE_SCORE;
extern const int BAD_CAPTURE_SCORE;
extern const int KILLER1_SCORE;
extern const int KILLER2_SCORE;
extern const int COUNTER_SCORE;

void AppendMove(Move* arr, uint8_t* n, Move move);
void GenerateAllMoves(MoveList* moveList, Board* board);
void GenerateQuietMoves(MoveList* moveList, Board* board);
void GenerateTacticalMoves(MoveList* moveList, Board* board);

#endif
