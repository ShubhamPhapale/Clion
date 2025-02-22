

#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

extern uint64_t ZOBRIST_PIECES[12][64];
extern uint64_t ZOBRIST_EP_KEYS[64];
extern uint64_t ZOBRIST_CASTLE_KEYS[16];
extern uint64_t ZOBRIST_SIDE_KEY;

void InitZobristKeys();
uint64_t Zobrist(Board* board);

#endif