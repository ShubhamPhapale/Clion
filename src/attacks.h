

#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"

extern const int BISHOP_RELEVANT_BITS[64];
extern const int ROOK_RELEVANT_BITS[64];

extern BitBoard BETWEEN_SQS[64][64];
extern BitBoard PINNED_MOVES[64][64];

extern BitBoard PAWN_ATTACKS[2][64];
extern BitBoard KNIGHT_ATTACKS[64];
extern BitBoard BISHOP_ATTACKS[64][512];
extern BitBoard ROOK_ATTACKS[64][4096];
extern BitBoard KING_ATTACKS[64];
extern BitBoard ROOK_MASKS[64];
extern BitBoard BISHOP_MASKS[64];

extern uint64_t ROOK_MAGICS[64];
extern uint64_t BISHOP_MAGICS[64];

void InitBetweenSquares();
void InitPinnedMovementSquares();
void initPawnSpans();
void InitPawnAttacks();
void InitKnightAttacks();
void InitBishopMasks();
void InitBishopMagics();
void InitBishopAttacks();
void InitRookMasks();
void InitRookMagics();
void InitRookAttacks();
void InitKingAttacks();
void InitAttacks();

BitBoard GetGeneratedPawnAttacks(int sq, int color);
BitBoard GetGeneratedKnightAttacks(int sq);
BitBoard GetBishopMask(int sq);
BitBoard GetBishopAttacksOTF(int sq, BitBoard blockers);
BitBoard GetRookMask(int sq);
BitBoard GetRookAttacksOTF(int sq, BitBoard blockers);
BitBoard GetGeneratedKingAttacks(int sq);
BitBoard SetPieceLayoutOccupancy(int idx, int bits, BitBoard attacks);

uint64_t FindMagicNumber(int sq, int n, int bishop);

BitBoard GetInBetweenSquares(int from, int to);
BitBoard GetPinnedMovementSquares(int p, int k);

BitBoard GetPawnAttacks(int sq, int color);
BitBoard GetKnightAttacks(int sq);
BitBoard GetBishopAttacks(int sq, BitBoard occupancy);
BitBoard GetRookAttacks(int sq, BitBoard occupancy);
BitBoard GetQueenAttacks(int sq, BitBoard occupancy);
BitBoard GetKingAttacks(int sq);
BitBoard AttacksToSquare(Board* board, int sq, BitBoard occ);

#endif
