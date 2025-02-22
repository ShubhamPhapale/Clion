

#ifndef HISTORY_H
#define HISTORY_H

#include "types.h"

void AddKillerMove(SearchData* data, Move move);
void AddCounterMove(SearchData* data, Move move, Move parent);
void AddHistoryHeuristic(int* entry, int inc);
void UpdateHistories(SearchData* data, Move bestMove, int depth, int stm, Move quiets[], int nQ);
int GetHistory(SearchData* data, Move move, int stm);
int GetCounterHistory(SearchData* data, Move move);

#endif