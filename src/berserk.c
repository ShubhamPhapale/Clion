

#include <string.h>

#include "attacks.h"
#include "bench.h"
#include "bits.h"
#include "board.h"
#include "eval.h"
#include "random.h"
#include "search.h"
#include "transposition.h"
#include "tune.h"
#include "types.h"
#include "uci.h"
#include "util.h"
#include "zobrist.h"

// Welcome to berserk
int main(int argc, char** argv) {
  SeedRandom(0);

  InitPSQT();
  InitZobristKeys();
  InitPruningAndReductionTables();
  InitAttacks();

  TTInit(32);

  // Compliance for OpenBench
  if (argc > 1 && !strncmp(argv[1], "bench", 5)) {
    Bench();
  } else if (argc > 1 && !strncmp(argv[1], "tune", 4)) {
#ifdef TUNE
    Tune();
#endif
  } else {
    UCILoop();
  }

  return 0;
}
