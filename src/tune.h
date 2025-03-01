

#ifdef TUNE

#ifndef TUNE_H
#define TUNE_H

#include <stdio.h>

#include "types.h"

#define EPD_FILE_PATH "C:\\Programming\\berserk-testing\\texel\\lichess-big3-resolved.book"
#define THREADS 24
#define TUNE_KS 0

typedef struct {
  int epoch;
  float value;
  float g;
  float M;
  float V;
} Param;

typedef struct {
  Param mg;
  Param eg;
} Weight;

typedef struct {
  Weight pieces[5];
  Weight psqt[6][2][32];
  Weight bishopPair;

  Weight knightPostPsqt[12];
  Weight bishopPostPsqt[12];

  Weight knightMobilities[9];
  Weight bishopMobilities[14];
  Weight rookMobilities[15];
  Weight queenMobilities[28];

  Weight minorBehindPawn;
  Weight knightPostReachable;
  Weight bishopPostReachable;
  Weight bishopTrapped;
  Weight rookTrapped;
  Weight badBishopPawns;
  Weight dragonBishop;
  Weight rookOpenFileOffset;
  Weight rookOpenFile;
  Weight rookSemiOpen;
  Weight rookToOpen;
  Weight queenOppositeRook;
  Weight queenRookBattery;

  Weight defendedPawns;
  Weight doubledPawns;
  Weight isolatedPawns[4];
  Weight openIsolatedPawns;
  Weight backwardsPawns;
  Weight connectedPawn[4][8];
  Weight candidatePasser[8];
  Weight candidateEdgeDistance;

  Weight passedPawn[8];
  Weight passedPawnEdgeDistance;
  Weight passedPawnKingProximity;
  Weight passedPawnAdvance[5];
  Weight passedPawnEnemySliderBehind;
  Weight passedPawnSqRule;
  Weight passedPawnUnsupported;

  Weight knightThreats[6];
  Weight bishopThreats[6];
  Weight rookThreats[6];
  Weight kingThreat;
  Weight pawnThreat;
  Weight pawnPushThreat;
  Weight pawnPushThreatPinned;
  Weight hangingThreat;
  Weight knightCheckQueen;
  Weight bishopCheckQueen;
  Weight rookCheckQueen;

  Weight space;

  Weight imbalance[5][5];

  Weight pawnShelter[4][8];
  Weight pawnStorm[4][8];
  Weight blockedPawnStorm[8];
  Weight castlingRights;

  Weight complexPawns;
  Weight complexPawnsOffset;
  Weight complexOffset;

  Weight ksAttackerWeight[5];
  Weight ksWeakSqs;
  Weight ksPinned;
  Weight ksKnightCheck;
  Weight ksBishopCheck;
  Weight ksRookCheck;
  Weight ksQueenCheck;
  Weight ksUnsafeCheck;
  Weight ksEnemyQueen;
  Weight ksKnightDefense;

  Weight tempo;
} Weights;

typedef struct {
  uint8_t phase;
  int8_t stm;
  float result;
  int scale;
  float phaseMg;
  float phaseEg;
  Score staticEval;
  EvalCoeffs coeffs;
} Position;

typedef struct {
  double wDanger;
  double bDanger;
  double complexity;
  double eg;
} EvalGradientData;

typedef struct {
  float error;
  int n;
  Position* positions;
  Weights* weights;
} GradientUpdate;

void Tune();

void ValidateEval(int n, Position* positions, Weights* weights);
void ComputeK(int n, Position* positions);
double TotalStaticError(int n, Position* positions);

void UpdateParam(Param* p);
void UpdateWeight(Weight* w);
void UpdateWeights(Weights* weights);
void MergeWeightGradients(Weight* dest, Weight* src);
double UpdateAndTrain(int n, Position* positions, Weights* weights);

void* UpdateGradients(void* arg);
void UpdateMaterialGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePsqtGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdateMobilityGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdateThreatGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdatePawnShelterGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdateSpaceGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdateComplexityGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd);
void UpdateKingSafetyGradients(Position* position, double loss, Weights* weights, EvalGradientData* ks);
void UpdateTempoGradient(Position* position, double loss, Weights* weights);

void ApplyCoeff(double* mg, double* eg, int coeff, Weight* w);
double EvaluateCoeffs(Position* position, Weights* weights, EvalGradientData* gd);
void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluatePawnShelterValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateSpaceValues(double* mg, double* eg, Position* position, Weights* weights);
void EvaluateComplexityValues(double* mg, double* eg, Position* position, Weights* weights, EvalGradientData* gd);
void EvaluateKingSafetyValues(double* mg, double* eg, Position* position, Weights* weights, EvalGradientData* ks);

void InitMaterialWeights(Weights* weights);
void InitPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitPostPsqtWeights(Weights* weights);
void InitMobilityWeights(Weights* weights);
void InitThreatWeights(Weights* weights);
void InitPieceBonusWeights(Weights* weights);
void InitPawnBonusWeights(Weights* weights);
void InitPasserBonusWeights(Weights* weights);
void InitPawnShelterWeights(Weights* weights);
void InitSpaceWeights(Weights* weights);
void InitComplexityWeights(Weights* weights);
void InitKingSafetyWeights(Weights* weights);
void InitTempoWeight(Weights* weights);

void LoadPosition(Board* board, Position* position, ThreadData* thread);
Position* LoadPositions(int* n, Weights* weights);

double Sigmoid(double s);

void PrintWeights(Weights* weights, int epoch, double error);
void PrintWeightArray(FILE* fp, Weight* weights, int n, int wrap);
void PrintWeight(FILE* fp, Weight* weight);

#endif
#endif