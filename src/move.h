

#ifndef MOVE_H
#define MOVE_H

#include "types.h"

#define NULL_MOVE 0

extern const int CHAR_TO_PIECE[];
extern const char* PIECE_TO_CHAR;
extern const char* PROMOTION_TO_CHAR;
extern const char* SQ_TO_COORD[];

#define BuildMove(start, end, piece, promo, cap, dub, ep, castle)                                                      \
  (start) | ((end) << 6) | ((piece) << 12) | ((promo) << 16) | ((cap) << 20) | ((dub) << 21) | ((ep) << 22) |          \
      ((castle) << 23)
#define MoveStart(move) ((int)(move)&0x3f)
#define MoveEnd(move) (((int)(move)&0xfc0) >> 6)
#define MovePiece(move) (((int)(move)&0xf000) >> 12)
#define MovePromo(move) (((int)(move)&0xf0000) >> 16)
#define MoveCapture(move) (((int)(move)&0x100000) >> 20)
#define MoveDoublePush(move) (((int)(move)&0x200000) >> 21)
#define MoveEP(move) (((int)(move)&0x400000) >> 22)
#define MoveCastle(move) (((int)(move)&0x800000) >> 23)
// just mask the start/end bits into a single int for indexing butterfly tables
#define MoveStartEnd(move) ((int)(move)&0xfff)

#define Tactical(move) (((int)(move)&0x1f0000) >> 16)

Move ParseMove(char* moveStr, Board* board);
char* MoveToStr(Move move, Board* board);
int IsRecapture(SearchData* data, Move move);

#endif