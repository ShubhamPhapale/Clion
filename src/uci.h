

#ifndef UCI_H
#define UCI_H

extern int CHESS_960;

void RootMoves(SimpleMoveList* moves, Board* board);

void ParseGo(char* in, SearchParams* params, Board* board, ThreadData* threads);
void ParsePosition(char* in, Board* board);
void PrintUCIOptions();

int ReadLine(char* in);
void UCILoop();

int GetOptionIntValue(char* in);

#endif