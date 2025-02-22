

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

#include "bits.h"
#include "search.h"
#include "transposition.h"
#include "types.h"

// Global TT
TTTable TT = {0};

size_t TTInit(int mb) {
  if (TT.mask)
    TTFree();

  uint64_t keySize = (uint64_t)log2(mb) + (uint64_t)log2(MEGABYTE / sizeof(TTBucket));

#if defined(__linux__) && !defined(__ANDROID__)
  // On Linux systems we align on 2MB boundaries and request Huge Pages
  TT.buckets = aligned_alloc(2 * MEGABYTE, (1ULL << keySize) * sizeof(TTBucket));
  madvise(TT.buckets, (1ULL << keySize) * sizeof(TTBucket), MADV_HUGEPAGE);
#else
  TT.buckets = calloc((1ULL << keySize), sizeof(TTBucket));
#endif

  TT.mask = (1ULL << keySize) - 1ULL;

  TTClear();
  return (TT.mask + 1ULL) * sizeof(TTBucket);
}

void TTFree() { free(TT.buckets); }

inline void TTClear() { memset(TT.buckets, 0, (TT.mask + 1ULL) * sizeof(TTBucket)); }

inline void TTUpdate() { TT.age += 1; }

inline int TTScore(TTEntry* e, int ply) {
  if (e->score == UNKNOWN)
    return UNKNOWN;

  return e->score > MATE_BOUND ? e->score - ply : e->score < -MATE_BOUND ? e->score + ply : e->score;
}

inline void TTPrefetch(uint64_t hash) { __builtin_prefetch(&TT.buckets[TT.mask & hash]); }

inline TTEntry* TTProbe(int* hit, uint64_t hash) {
  TTEntry* bucket = TT.buckets[TT.mask & hash].entries;
  uint32_t shortHash = hash >> 32;

  for (int i = 0; i < BUCKET_SIZE; i++)
    if (bucket[i].hash == shortHash) {
      *hit = 1;
      bucket[i].age = TT.age;
      return &bucket[i];
    }

  return NULL;
}

inline void TTPut(uint64_t hash, int8_t depth, int16_t score, uint8_t flag, Move move, int ply, int16_t eval) {
  TTBucket* bucket = &TT.buckets[TT.mask & hash];
  uint32_t shortHash = hash >> 32;
  TTEntry* toReplace = bucket->entries;

  if (score > MATE_BOUND)
    score += ply;
  else if (score < -MATE_BOUND)
    score -= ply;

  for (TTEntry* entry = bucket->entries; entry < bucket->entries + BUCKET_SIZE; entry++) {
    if (!entry->hash) {
      toReplace = entry;
      break;
    }

    if (entry->hash == shortHash) {
      if (entry->depth > depth * 2 && !(flag & TT_EXACT))
        return;

      toReplace = entry;
      break;
    }

    if (entry->depth - (256 + TT.age - entry->age) * 4 < toReplace->depth - (256 + TT.age - toReplace->age) * 4)
      toReplace = entry;
  }

  *toReplace = (TTEntry){
      .flags = flag, .depth = depth, .eval = eval, .score = score, .hash = shortHash, .move = move, .age = TT.age};
}

inline int TTFull() {
  int c = 1000 / BUCKET_SIZE;
  int t = 0;

  for (int i = 0; i < c; i++) {
    TTBucket b = TT.buckets[i];
    for (int j = 0; j < BUCKET_SIZE; j++) {
      if (b.entries[j].hash && b.entries[j].age == TT.age)
        t++;
    }
  }

  return t * 1000 / (c * BUCKET_SIZE);
}