

#include <stdlib.h>
#include <string.h>

#include "eval.h"
#include "types.h"
#include "util.h"

// initialize a pool of threads
ThreadData* CreatePool(int count) {
  ThreadData* threads = malloc(count * sizeof(ThreadData));

  for (int i = 0; i < count; i++) {
    // allow reference to one another
    threads[i].idx = i;
    threads[i].threads = threads;
    threads[i].count = count;
  }

  return threads;
}

// initialize a pool prepping to start a search
void InitPool(Board* board, SearchParams* params, ThreadData* threads, SearchResults* results) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].params = params;
    threads[i].results = results;

    threads[i].data.nodes = 0;
    threads[i].data.seldepth = 0;
    threads[i].data.ply = 0;
    threads[i].data.tbhits = 0;

    // empty unneeded data
    memset(&threads[i].data.skipMove, 0, sizeof(threads[i].data.skipMove));
    memset(&threads[i].data.evals, 0, sizeof(threads[i].data.evals));
    memset(&threads[i].data.moves, 0, sizeof(threads[i].data.moves));

    // need full copies of the board
    memcpy(&threads[i].board, board, sizeof(Board));
  }
}

void ResetThreadPool(ThreadData* threads) {
  for (int i = 0; i < threads->count; i++) {
    threads[i].results = NULL;

    threads[i].data.nodes = 0;
    threads[i].data.seldepth = 0;
    threads[i].data.ply = 0;
    threads[i].data.tbhits = 0;

    // empty ALL data
    memset(&threads[i].data.skipMove, 0, sizeof(threads[i].data.skipMove));
    memset(&threads[i].data.evals, 0, sizeof(threads[i].data.evals));
    memset(&threads[i].data.moves, 0, sizeof(threads[i].data.moves));
    memset(&threads[i].data.killers, 0, sizeof(threads[i].data.killers));
    memset(&threads[i].data.counters, 0, sizeof(threads[i].data.counters));
    memset(&threads[i].data.hh, 0, sizeof(threads[i].data.hh));
    memset(&threads[i].pawnHashTable, 0, PAWN_TABLE_SIZE * sizeof(PawnHashEntry));
    memset(&threads[i].board, 0, sizeof(Board));
  }
}

// sum node counts
uint64_t NodesSearched(ThreadData* threads) {
  uint64_t nodes = 0;
  for (int i = 0; i < threads->count; i++)
    nodes += threads[i].data.nodes;

  return nodes;
}

uint64_t TBHits(ThreadData* threads) {
  uint64_t tbhits = 0;
  for (int i = 0; i < threads->count; i++)
    tbhits += threads[i].data.tbhits;

  return tbhits;
}

int Seldepth(ThreadData* threads) {
  int seldepth = threads[0].data.seldepth;
  for (int i = 1; i < threads->count; i++)
    seldepth = max(seldepth, threads[i].data.seldepth);

  return seldepth;
}