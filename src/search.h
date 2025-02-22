

#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

// search specific score evals
#define UNKNOWN 32257 // this must be higher than CHECKMATE (some conditional logic relies on this)
#define CHECKMATE 32256
#define MATE_BOUND 30000
#define TB_WIN_BOUND 20000

// static evaluation pruning
// capture cutoff is linear 70x
// quiet cutoff is quadratic 20x^2
#define SEE_PRUNE_CAPTURE_CUTOFF 90
#define SEE_PRUNE_CUTOFF 20

// delta pruning in QS
#define DELTA_CUTOFF 150

// base window value
#define WINDOW 8

void InitPruningAndReductionTables();

void* UCISearch(void* arg);
void BestMove(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results);
void* Search(void* arg);
int Negamax(int alpha, int beta, int depth, ThreadData* thread, PV* pv);
int Quiesce(int alpha, int beta, ThreadData* thread, PV* pv);

void PrintInfo(PV* pv, int score, ThreadData* thread, int alpha, int beta, int multiPV, Board* board);
void PrintPV(PV* pv, Board* board);
 
int MoveSearchedByMultiPV(ThreadData* thread, Move move);
int MoveSearchable(SearchParams* params, Move move);

#endif
