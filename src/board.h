

#ifndef BOARD_H
#define BOARD_H

#include "types.h"

#define NO_PIECE 12

#define file(sq) ((sq)&7)
#define rank(sq) ((sq) >> 3)
#define sq(r, f) ((r) * 8 + (f))

extern const BitBoard EMPTY;

extern const int PAWN[];
extern const int KNIGHT[];
extern const int BISHOP[];
extern const int ROOK[];
extern const int QUEEN[];
extern const int KING[];
extern const int PIECE_TYPE[];

extern const int MIRROR[];

extern const uint64_t PIECE_COUNT_IDX[];

void ClearBoard(Board* board);
void ParseFen(char* fen, Board* board);
void BoardToFen(char* fen, Board* board);
void PrintBoard(Board* board);

void SetSpecialPieces(Board* board);
void SetOccupancies(Board* board);

int DoesMoveCheck(Move move, Board* board);
int IsRepetition(Board* board, int ply);

int HasNonPawn(Board* board);
int IsOCB(Board* board);
int IsMaterialDraw(Board* board);

void MakeNullMove(Board* board);
void UndoNullMove(Board* board);
void MakeMove(Move move, Board* board);
void UndoMove(Move move, Board* board);

int IsMoveLegal(Move move, Board* board);
int MoveIsLegal(Move move, Board* board);

#endif