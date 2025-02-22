

#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

#include "types.h"

#define NO_ENTRY 0ULL
#define MEGABYTE 0x100000ULL
#define BUCKET_SIZE 4

typedef struct {
  uint32_t hash, move;
  int16_t eval, score;
  int8_t depth;
  uint8_t flags, age;
} TTEntry;

typedef struct {
  TTEntry entries[BUCKET_SIZE];
} TTBucket;

typedef struct {
  TTBucket* buckets;
  uint64_t mask;
  uint64_t size;
  uint8_t age;
} TTTable;

enum { TT_UNKNOWN = 0, TT_LOWER = 1, TT_UPPER = 2, TT_EXACT = 4 };

extern TTTable TT;

size_t TTInit(int mb);
void TTFree();
void TTClear();
void TTUpdate();
void TTPrefetch(uint64_t hash);
TTEntry* TTProbe(int* hit, uint64_t hash);
int TTScore(TTEntry* e, int ply);
void TTPut(uint64_t hash, int8_t depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval);
int TTFull();

#endif