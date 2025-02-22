

#ifndef ENDGAME_H
#define ENDGAME_H

#include "types.h"

int Push(Board* board, int ss);
int StaticMaterialScore(int side, Board* board);
int EvaluateMaterialOnlyEndgame(Board* board);
int EvaluateKXK(Board* board);

uint8_t GetKPKBit(uint32_t bit);
uint32_t KPKIndex(int ssKing, int wsKing, int p, int stm);
uint8_t KPKDraw(int ss, int ssKing, int wsKing, int p, int stm);

#endif