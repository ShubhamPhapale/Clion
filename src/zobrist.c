

#include "zobrist.h"
#include "bits.h"
#include "random.h"
#include "types.h"

uint64_t ZOBRIST_PIECES[12][64];
uint64_t ZOBRIST_EP_KEYS[64];
uint64_t ZOBRIST_CASTLE_KEYS[16];
uint64_t ZOBRIST_SIDE_KEY;

void InitZobristKeys() {
  for (int i = 0; i < 12; i++)
    for (int j = 0; j < 64; j++)
      ZOBRIST_PIECES[i][j] = RandomUInt64();

  for (int i = 0; i < 64; i++)
    ZOBRIST_EP_KEYS[i] = RandomUInt64();

  for (int i = 0; i < 16; i++)
    ZOBRIST_CASTLE_KEYS[i] = RandomUInt64();

  ZOBRIST_SIDE_KEY = RandomUInt64();
}

// Generate a Zobirst key for the current board state
uint64_t Zobrist(Board* board) {
  uint64_t hash = 0ULL;

  for (int piece = PAWN_WHITE; piece <= KING_BLACK; piece++)
    for (BitBoard pieces = board->pieces[piece]; pieces; popLsb(pieces))
      hash ^= ZOBRIST_PIECES[piece][lsb(pieces)];

  if (board->epSquare)
    hash ^= ZOBRIST_EP_KEYS[board->epSquare];

  hash ^= ZOBRIST_CASTLE_KEYS[board->castling];

  if (board->side)
    hash ^= ZOBRIST_SIDE_KEY;

  return hash;
}