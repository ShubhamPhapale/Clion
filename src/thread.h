

#ifndef THREAD_H
#define THREAD_H

#include "types.h"

ThreadData* CreatePool(int count);
void InitPool(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results);
void ResetThreadPool(ThreadData* threads);
uint64_t NodesSearched(ThreadData* threads);
uint64_t TBHits(ThreadData* threads);
int Seldepth(ThreadData* threads);

#endif