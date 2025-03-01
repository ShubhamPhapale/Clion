

#ifdef TUNE

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bits.h"
#include "board.h"
#include "eval.h"
#include "search.h"
#include "thread.h"
#include "tune.h"
#include "util.h"

const int MAX_POSITIONS = 100000000;

const int sideScalar[2] = {1, -1};

double ALPHA = 0.0001;
const double BETA1 = 0.9;
const double BETA2 = 0.999;
const double EPSILON = 1e-8;

double K = 2.878242507;

void Tune() {
  Weights weights = {0};

  InitMaterialWeights(&weights);
  InitPsqtWeights(&weights);
  InitPostPsqtWeights(&weights);
  InitMobilityWeights(&weights);
  InitThreatWeights(&weights);
  InitPieceBonusWeights(&weights);
  InitPawnBonusWeights(&weights);
  InitPasserBonusWeights(&weights);
  InitPawnShelterWeights(&weights);
  InitSpaceWeights(&weights);
  InitTempoWeight(&weights);
  InitComplexityWeights(&weights);
  InitKingSafetyWeights(&weights);

  PrintWeights(&weights, 0, 0);

  int n = 0;
  Position* positions = LoadPositions(&n, &weights);
  ALPHA *= sqrt(n);

  printf("Starting Error: %1.8f\n", TotalStaticError(n, positions));

  long start = GetTimeMS();

  for (int epoch = 1; epoch <= 100000; epoch++) {
    double error = UpdateAndTrain(n, positions, &weights);

    long now = GetTimeMS();

    printf("Epoch: [#%4d], Error: %1.8f, Speed: %4.4fs\r", epoch, error / n, (now - start) / (1000.0 * epoch));

    if (epoch % 25 == 0) {
      PrintWeights(&weights, epoch, error);
      printf("\n");
    }
  }

  free(positions);
}

// Finn Eggers method for determining K
void ComputeK(int n, Position* positions) {
  double dK = 0.01;
  double dEdK = 1;

  double rate = 100;
  double dev = 1e-8;

  while (fabs(dEdK) > dev) {
    K += dK;
    double Epdk = TotalStaticError(n, positions);
    K -= 2 * dK;
    double Emdk = TotalStaticError(n, positions);
    K += dK;

    dEdK = (Epdk - Emdk) / (2 * dK);

    printf("K: %.9f, Error: %.9f, Deviation: %.9f\n", K, (Epdk + Emdk) / 2, fabs(dEdK));

    K -= dEdK * rate;
  }
}

double TotalStaticError(int n, Position* positions) {
  double e = 0;

#pragma omp parallel for schedule(static) num_threads(THREADS) reduction(+ : e)
  for (int i = 0; i < n; i++) {

    double sigmoid = Sigmoid(positions[i].staticEval);
    double difference = sigmoid - positions[i].result;
    e += pow(difference, 2);
  }

  return e / n;
}

void UpdateWeight(Weight* w) {
  UpdateParam(&w->mg);
  UpdateParam(&w->eg);
}

void UpdateParam(Param* p) {
  p->epoch++;

  if (!p->g)
    return;

  p->M = BETA1 * p->M + (1.0 - BETA1) * p->g;
  p->V = BETA2 * p->V + (1.0 - BETA2) * p->g * p->g;

  double mHat = p->M / (1 - pow(BETA1, p->epoch));
  double vHat = p->V / (1 - pow(BETA2, p->epoch));
  double delta = ALPHA * mHat / (sqrt(vHat) + EPSILON);

  p->value += delta;

  p->g = 0;
}

void UpdateWeights(Weights* weights) {
  // Fix these values, let the PSQT tune
  // for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++)
  //   UpdateWeight(&weights->pieces[pc]);

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++)
    for (int sq = 0; sq < 32; sq++)
      for (int i = 0; i < 2; i++)
        UpdateWeight(&weights->psqt[pc][i][sq]);

  for (int sq = 0; sq < 12; sq++) {
    UpdateWeight(&weights->knightPostPsqt[sq]);
    UpdateWeight(&weights->bishopPostPsqt[sq]);
  }

  for (int c = 0; c < 9; c++)
    UpdateWeight(&weights->knightMobilities[c]);

  for (int c = 0; c < 14; c++)
    UpdateWeight(&weights->bishopMobilities[c]);

  for (int c = 0; c < 15; c++)
    UpdateWeight(&weights->rookMobilities[c]);

  for (int c = 0; c < 28; c++)
    UpdateWeight(&weights->queenMobilities[c]);

  for (int pc = 0; pc < 6; pc++) {
    UpdateWeight(&weights->knightThreats[pc]);
    UpdateWeight(&weights->bishopThreats[pc]);
    UpdateWeight(&weights->rookThreats[pc]);
  }

  UpdateWeight(&weights->kingThreat);

  UpdateWeight(&weights->pawnThreat);

  UpdateWeight(&weights->pawnPushThreat);

  UpdateWeight(&weights->pawnPushThreatPinned);

  UpdateWeight(&weights->hangingThreat);

  UpdateWeight(&weights->knightCheckQueen);

  UpdateWeight(&weights->bishopCheckQueen);

  UpdateWeight(&weights->rookCheckQueen);

  UpdateWeight(&weights->bishopPair);

  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      UpdateWeight(&weights->imbalance[i][j]);

  UpdateWeight(&weights->minorBehindPawn);

  UpdateWeight(&weights->knightPostReachable);

  UpdateWeight(&weights->bishopPostReachable);

  UpdateWeight(&weights->bishopTrapped);

  UpdateWeight(&weights->rookTrapped);

  UpdateWeight(&weights->badBishopPawns);

  UpdateWeight(&weights->dragonBishop);

  UpdateWeight(&weights->rookOpenFileOffset);

  UpdateWeight(&weights->rookOpenFile);

  UpdateWeight(&weights->rookSemiOpen);

  UpdateWeight(&weights->rookToOpen);

  UpdateWeight(&weights->queenOppositeRook);

  UpdateWeight(&weights->queenRookBattery);

  UpdateWeight(&weights->defendedPawns);

  UpdateWeight(&weights->doubledPawns);

  UpdateWeight(&weights->candidateEdgeDistance);

  for (int f = 0; f < 4; f++)
    UpdateWeight(&weights->isolatedPawns[f]);

  UpdateWeight(&weights->openIsolatedPawns);

  UpdateWeight(&weights->backwardsPawns);

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++)
      UpdateWeight(&weights->connectedPawn[f][r]);

    UpdateWeight(&weights->candidatePasser[r]);
  }

  for (int r = 0; r < 8; r++)
    UpdateWeight(&weights->passedPawn[r]);

  UpdateWeight(&weights->passedPawnEdgeDistance);

  UpdateWeight(&weights->passedPawnKingProximity);

  for (int r = 0; r < 5; r++)
    UpdateWeight(&weights->passedPawnAdvance[r]);

  UpdateWeight(&weights->passedPawnEnemySliderBehind);

  UpdateWeight(&weights->passedPawnSqRule);

  UpdateWeight(&weights->passedPawnUnsupported);

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      UpdateWeight(&weights->pawnShelter[f][r]);

      UpdateWeight(&weights->pawnStorm[f][r]);
    }
  }

  for (int r = 0; r < 8; r++)
    UpdateWeight(&weights->blockedPawnStorm[r]);

  UpdateWeight(&weights->castlingRights);

  UpdateParam(&weights->space.mg);

  UpdateParam(&weights->tempo.mg);

  UpdateParam(&weights->complexPawns.mg);

  UpdateParam(&weights->complexOffset.mg);

  UpdateParam(&weights->complexPawnsOffset.mg);

  if (TUNE_KS) {
    for (int i = 0; i < 5; i++)
      UpdateParam(&weights->ksAttackerWeight[i].mg);

    UpdateParam(&weights->ksWeakSqs.mg);
    UpdateParam(&weights->ksPinned.mg);
    UpdateParam(&weights->ksKnightCheck.mg);
    UpdateParam(&weights->ksBishopCheck.mg);
    UpdateParam(&weights->ksRookCheck.mg);
    UpdateParam(&weights->ksQueenCheck.mg);
    UpdateParam(&weights->ksUnsafeCheck.mg);
    UpdateParam(&weights->ksEnemyQueen.mg);
    UpdateParam(&weights->ksKnightDefense.mg);
  }
}

void MergeWeightGradients(Weight* dest, Weight* src) {
  dest->mg.g += src->mg.g;
  dest->eg.g += src->eg.g;
}

double UpdateAndTrain(int n, Position* positions, Weights* weights) {
  pthread_t threads[THREADS];
  GradientUpdate jobs[THREADS];
  Weights local[THREADS];

  int chunk = (n / THREADS) + 1;

  for (int t = 0; t < THREADS; t++) {
    memcpy(&local[t], weights, sizeof(Weights));

    jobs[t].error = 0.0;
    jobs[t].n = t < THREADS - 1 ? chunk : (n - ((THREADS - 1) * chunk));
    jobs[t].positions = &positions[t * chunk];
    jobs[t].weights = &local[t];

    pthread_create(&threads[t], NULL, &UpdateGradients, &jobs[t]);
  }

  for (int t = 0; t < THREADS; t++)
    pthread_join(threads[t], NULL);

  double error = 0;
  for (int t = 0; t < THREADS; t++) {
    error += jobs[t].error;
    Weights* w = jobs[t].weights;

    for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
      weights->pieces[pc].mg.g += w->pieces[pc].mg.g;
      weights->pieces[pc].eg.g += w->pieces[pc].eg.g;
    }

    weights->bishopPair.mg.g += w->bishopPair.mg.g;
    weights->bishopPair.eg.g += w->bishopPair.eg.g;

    for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
      for (int sq = 0; sq < 32; sq++) {
        for (int i = 0; i < 2; i++) {
          weights->psqt[pc][i][sq].mg.g += w->psqt[pc][i][sq].mg.g;
          weights->psqt[pc][i][sq].eg.g += w->psqt[pc][i][sq].eg.g;
        }
      }
    }

    for (int sq = 0; sq < 12; sq++) {
      weights->knightPostPsqt[sq].mg.g += w->knightPostPsqt[sq].mg.g;
      weights->knightPostPsqt[sq].eg.g += w->knightPostPsqt[sq].eg.g;

      weights->bishopPostPsqt[sq].mg.g += w->bishopPostPsqt[sq].mg.g;
      weights->bishopPostPsqt[sq].eg.g += w->bishopPostPsqt[sq].eg.g;
    }

    for (int c = 0; c < 9; c++) {
      weights->knightMobilities[c].mg.g += w->knightMobilities[c].mg.g;
      weights->knightMobilities[c].eg.g += w->knightMobilities[c].eg.g;
    }

    for (int c = 0; c < 14; c++) {
      weights->bishopMobilities[c].mg.g += w->bishopMobilities[c].mg.g;
      weights->bishopMobilities[c].eg.g += w->bishopMobilities[c].eg.g;
    }

    for (int c = 0; c < 15; c++) {
      weights->rookMobilities[c].mg.g += w->rookMobilities[c].mg.g;
      weights->rookMobilities[c].eg.g += w->rookMobilities[c].eg.g;
    }

    for (int c = 0; c < 28; c++) {
      weights->queenMobilities[c].mg.g += w->queenMobilities[c].mg.g;
      weights->queenMobilities[c].eg.g += w->queenMobilities[c].eg.g;
    }

    for (int pc = 0; pc < 6; pc++) {
      weights->knightThreats[pc].mg.g += w->knightThreats[pc].mg.g;
      weights->knightThreats[pc].eg.g += w->knightThreats[pc].eg.g;

      weights->bishopThreats[pc].mg.g += w->bishopThreats[pc].mg.g;
      weights->bishopThreats[pc].eg.g += w->bishopThreats[pc].eg.g;

      weights->rookThreats[pc].mg.g += w->rookThreats[pc].mg.g;
      weights->rookThreats[pc].eg.g += w->rookThreats[pc].eg.g;
    }

    weights->kingThreat.mg.g += w->kingThreat.mg.g;
    weights->kingThreat.eg.g += w->kingThreat.eg.g;

    weights->pawnThreat.mg.g += w->pawnThreat.mg.g;
    weights->pawnThreat.eg.g += w->pawnThreat.eg.g;

    weights->pawnPushThreat.mg.g += w->pawnPushThreat.mg.g;
    weights->pawnPushThreat.eg.g += w->pawnPushThreat.eg.g;

    weights->pawnPushThreatPinned.mg.g += w->pawnPushThreatPinned.mg.g;
    weights->pawnPushThreatPinned.eg.g += w->pawnPushThreatPinned.eg.g;

    weights->hangingThreat.mg.g += w->hangingThreat.mg.g;
    weights->hangingThreat.eg.g += w->hangingThreat.eg.g;

    weights->knightCheckQueen.mg.g += w->knightCheckQueen.mg.g;
    weights->knightCheckQueen.eg.g += w->knightCheckQueen.eg.g;

    weights->bishopCheckQueen.mg.g += w->bishopCheckQueen.mg.g;
    weights->bishopCheckQueen.eg.g += w->bishopCheckQueen.eg.g;

    weights->rookCheckQueen.mg.g += w->rookCheckQueen.mg.g;
    weights->rookCheckQueen.eg.g += w->rookCheckQueen.eg.g;

    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 5; j++) {
        weights->imbalance[i][j].mg.g += w->imbalance[i][j].mg.g;
        weights->imbalance[i][j].eg.g += w->imbalance[i][j].eg.g;
      }
    }

    weights->minorBehindPawn.mg.g += w->minorBehindPawn.mg.g;
    weights->minorBehindPawn.eg.g += w->minorBehindPawn.eg.g;

    weights->knightPostReachable.mg.g += w->knightPostReachable.mg.g;
    weights->knightPostReachable.eg.g += w->knightPostReachable.eg.g;

    weights->bishopPostReachable.mg.g += w->bishopPostReachable.mg.g;
    weights->bishopPostReachable.eg.g += w->bishopPostReachable.eg.g;

    weights->bishopTrapped.mg.g += w->bishopTrapped.mg.g;
    weights->bishopTrapped.eg.g += w->bishopTrapped.eg.g;

    weights->rookTrapped.mg.g += w->rookTrapped.mg.g;
    weights->rookTrapped.eg.g += w->rookTrapped.eg.g;

    weights->badBishopPawns.mg.g += w->badBishopPawns.mg.g;
    weights->badBishopPawns.eg.g += w->badBishopPawns.eg.g;

    weights->dragonBishop.mg.g += w->dragonBishop.mg.g;
    weights->dragonBishop.eg.g += w->dragonBishop.eg.g;

    weights->rookOpenFileOffset.mg.g += w->rookOpenFileOffset.mg.g;
    weights->rookOpenFileOffset.eg.g += w->rookOpenFileOffset.eg.g;

    weights->rookOpenFile.mg.g += w->rookOpenFile.mg.g;
    weights->rookOpenFile.eg.g += w->rookOpenFile.eg.g;

    weights->rookSemiOpen.mg.g += w->rookSemiOpen.mg.g;
    weights->rookSemiOpen.eg.g += w->rookSemiOpen.eg.g;

    weights->rookToOpen.mg.g += w->rookToOpen.mg.g;
    weights->rookToOpen.eg.g += w->rookToOpen.eg.g;

    weights->queenOppositeRook.mg.g += w->queenOppositeRook.mg.g;
    weights->queenOppositeRook.eg.g += w->queenOppositeRook.eg.g;

    weights->queenRookBattery.mg.g += w->queenRookBattery.mg.g;
    weights->queenRookBattery.eg.g += w->queenRookBattery.eg.g;

    weights->defendedPawns.mg.g += w->defendedPawns.mg.g;
    weights->defendedPawns.eg.g += w->defendedPawns.eg.g;

    weights->doubledPawns.mg.g += w->doubledPawns.mg.g;
    weights->doubledPawns.eg.g += w->doubledPawns.eg.g;

    for (int f = 0; f < 4; f++) {
      weights->isolatedPawns[f].mg.g += w->isolatedPawns[f].mg.g;
      weights->isolatedPawns[f].eg.g += w->isolatedPawns[f].eg.g;
    }

    weights->openIsolatedPawns.mg.g += w->openIsolatedPawns.mg.g;
    weights->openIsolatedPawns.eg.g += w->openIsolatedPawns.eg.g;

    weights->backwardsPawns.mg.g += w->backwardsPawns.mg.g;
    weights->backwardsPawns.eg.g += w->backwardsPawns.eg.g;

    weights->candidateEdgeDistance.mg.g += w->candidateEdgeDistance.mg.g;
    weights->candidateEdgeDistance.eg.g += w->candidateEdgeDistance.eg.g;

    for (int r = 0; r < 8; r++) {
      for (int f = 0; f < 4; f++) {
        weights->connectedPawn[f][r].mg.g += w->connectedPawn[f][r].mg.g;
        weights->connectedPawn[f][r].eg.g += w->connectedPawn[f][r].eg.g;
      }

      weights->candidatePasser[r].mg.g += w->candidatePasser[r].mg.g;
      weights->candidatePasser[r].eg.g += w->candidatePasser[r].eg.g;
    }

    for (int r = 0; r < 8; r++) {
      weights->passedPawn[r].mg.g += w->passedPawn[r].mg.g;
      weights->passedPawn[r].eg.g += w->passedPawn[r].eg.g;
    }

    weights->passedPawnEdgeDistance.mg.g += w->passedPawnEdgeDistance.mg.g;
    weights->passedPawnEdgeDistance.eg.g += w->passedPawnEdgeDistance.eg.g;

    weights->passedPawnKingProximity.mg.g += w->passedPawnKingProximity.mg.g;
    weights->passedPawnKingProximity.eg.g += w->passedPawnKingProximity.eg.g;

    for (int r = 0; r < 5; r++) {
      weights->passedPawnAdvance[r].mg.g += w->passedPawnAdvance[r].mg.g;
      weights->passedPawnAdvance[r].eg.g += w->passedPawnAdvance[r].eg.g;
    }

    weights->passedPawnEnemySliderBehind.mg.g += w->passedPawnEnemySliderBehind.mg.g;
    weights->passedPawnEnemySliderBehind.eg.g += w->passedPawnEnemySliderBehind.eg.g;

    weights->passedPawnSqRule.mg.g += w->passedPawnSqRule.mg.g;
    weights->passedPawnSqRule.eg.g += w->passedPawnSqRule.eg.g;

    weights->passedPawnUnsupported.mg.g += w->passedPawnUnsupported.mg.g;
    weights->passedPawnUnsupported.eg.g += w->passedPawnUnsupported.eg.g;

    for (int f = 0; f < 4; f++) {
      for (int r = 0; r < 8; r++) {
        weights->pawnShelter[f][r].mg.g += w->pawnShelter[f][r].mg.g;
        weights->pawnShelter[f][r].eg.g += w->pawnShelter[f][r].eg.g;

        weights->pawnStorm[f][r].mg.g += w->pawnStorm[f][r].mg.g;
        weights->pawnStorm[f][r].eg.g += w->pawnStorm[f][r].eg.g;
      }
    }

    for (int r = 0; r < 8; r++) {
      weights->blockedPawnStorm[r].mg.g += w->blockedPawnStorm[r].mg.g;
      weights->blockedPawnStorm[r].eg.g += w->blockedPawnStorm[r].eg.g;
    }

    weights->castlingRights.mg.g += w->castlingRights.mg.g;
    weights->castlingRights.eg.g += w->castlingRights.eg.g;

    weights->space.mg.g += w->space.mg.g;

    weights->tempo.mg.g += w->tempo.mg.g;

    weights->complexPawns.mg.g += w->complexPawns.mg.g;
    weights->complexPawnsOffset.mg.g += w->complexPawnsOffset.mg.g;
    weights->complexOffset.mg.g += w->complexOffset.mg.g;

    if (TUNE_KS) {
      for (int i = 0; i < 5; i++)
        weights->ksAttackerWeight[i].mg.g += w->ksAttackerWeight[i].mg.g;

      weights->ksWeakSqs.mg.g += w->ksWeakSqs.mg.g;
      weights->ksPinned.mg.g += w->ksPinned.mg.g;
      weights->ksKnightCheck.mg.g += w->ksKnightCheck.mg.g;
      weights->ksBishopCheck.mg.g += w->ksBishopCheck.mg.g;
      weights->ksRookCheck.mg.g += w->ksRookCheck.mg.g;
      weights->ksQueenCheck.mg.g += w->ksQueenCheck.mg.g;
      weights->ksUnsafeCheck.mg.g += w->ksUnsafeCheck.mg.g;
      weights->ksEnemyQueen.mg.g += w->ksEnemyQueen.mg.g;
      weights->ksKnightDefense.mg.g += w->ksKnightDefense.mg.g;
    }
  }

  UpdateWeights(weights);

  return error;
}

void* UpdateGradients(void* arg) {
  GradientUpdate* job = (GradientUpdate*)arg;

  int n = job->n;
  Weights* weights = job->weights;
  Position* positions = job->positions;

  for (int i = 0; i < n; i++) {
    EvalGradientData gd[1];
    double actual = EvaluateCoeffs(&positions[i], weights, gd);

    double sigmoid = Sigmoid(actual);
    double loss = (positions[i].result - sigmoid) * sigmoid * (1 - sigmoid);

    UpdateMaterialGradients(&positions[i], loss, weights, gd);
    UpdatePsqtGradients(&positions[i], loss, weights, gd);
    UpdatePostPsqtGradients(&positions[i], loss, weights, gd);
    UpdateMobilityGradients(&positions[i], loss, weights, gd);
    UpdateThreatGradients(&positions[i], loss, weights, gd);
    UpdatePieceBonusGradients(&positions[i], loss, weights, gd);
    UpdatePawnBonusGradients(&positions[i], loss, weights, gd);
    UpdatePasserBonusGradients(&positions[i], loss, weights, gd);
    UpdatePawnShelterGradients(&positions[i], loss, weights, gd);
    UpdateSpaceGradients(&positions[i], loss, weights, gd);
    UpdateTempoGradient(&positions[i], loss, weights);

    if (TUNE_KS)
      UpdateKingSafetyGradients(&positions[i], loss, weights, gd);

    UpdateComplexityGradients(&positions[i], loss, weights, gd);

    job->error += pow(positions[i].result - sigmoid, 2);
  }

  return NULL;
}

void UpdateMaterialGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->pieces[pc].mg.g += position->coeffs.pieces[pc] * mgBase;
    weights->pieces[pc].eg.g += position->coeffs.pieces[pc] * egBase;
  }
}

void UpdatePsqtGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++) {
    for (int sq = 0; sq < 32; sq++) {
      for (int i = 0; i < 2; i++) {
        weights->psqt[pc][i][sq].mg.g += position->coeffs.psqt[pc][i][sq] * mgBase;
        weights->psqt[pc][i][sq].eg.g += position->coeffs.psqt[pc][i][sq] * egBase;
      }
    }
  }
}

void UpdatePostPsqtGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int sq = 0; sq < 12; sq++) {
    weights->knightPostPsqt[sq].mg.g += position->coeffs.knightPostPsqt[sq] * mgBase;
    weights->knightPostPsqt[sq].eg.g += position->coeffs.knightPostPsqt[sq] * egBase;

    weights->bishopPostPsqt[sq].mg.g += position->coeffs.bishopPostPsqt[sq] * mgBase;
    weights->bishopPostPsqt[sq].eg.g += position->coeffs.bishopPostPsqt[sq] * egBase;
  }
}

void UpdateMobilityGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.g += position->coeffs.knightMobilities[c] * mgBase;
    weights->knightMobilities[c].eg.g += position->coeffs.knightMobilities[c] * egBase;
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.g += position->coeffs.bishopMobilities[c] * mgBase;
    weights->bishopMobilities[c].eg.g += position->coeffs.bishopMobilities[c] * egBase;
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.g += position->coeffs.rookMobilities[c] * mgBase;
    weights->rookMobilities[c].eg.g += position->coeffs.rookMobilities[c] * egBase;
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.g += position->coeffs.queenMobilities[c] * mgBase;
    weights->queenMobilities[c].eg.g += position->coeffs.queenMobilities[c] * egBase;
  }
}

void UpdateThreatGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.g += position->coeffs.knightThreats[pc] * mgBase;
    weights->knightThreats[pc].eg.g += position->coeffs.knightThreats[pc] * egBase;

    weights->bishopThreats[pc].mg.g += position->coeffs.bishopThreats[pc] * mgBase;
    weights->bishopThreats[pc].eg.g += position->coeffs.bishopThreats[pc] * egBase;

    weights->rookThreats[pc].mg.g += position->coeffs.rookThreats[pc] * mgBase;
    weights->rookThreats[pc].eg.g += position->coeffs.rookThreats[pc] * egBase;
  }

  weights->kingThreat.mg.g += position->coeffs.kingThreat * mgBase;
  weights->kingThreat.eg.g += position->coeffs.kingThreat * egBase;

  weights->pawnThreat.mg.g += position->coeffs.pawnThreat * mgBase;
  weights->pawnThreat.eg.g += position->coeffs.pawnThreat * egBase;

  weights->pawnPushThreat.mg.g += position->coeffs.pawnPushThreat * mgBase;
  weights->pawnPushThreat.eg.g += position->coeffs.pawnPushThreat * egBase;

  weights->pawnPushThreatPinned.mg.g += position->coeffs.pawnPushThreatPinned * mgBase;
  weights->pawnPushThreatPinned.eg.g += position->coeffs.pawnPushThreatPinned * egBase;

  weights->hangingThreat.mg.g += position->coeffs.hangingThreat * mgBase;
  weights->hangingThreat.eg.g += position->coeffs.hangingThreat * egBase;

  weights->knightCheckQueen.mg.g += position->coeffs.knightCheckQueen * mgBase;
  weights->knightCheckQueen.eg.g += position->coeffs.knightCheckQueen * egBase;

  weights->bishopCheckQueen.mg.g += position->coeffs.bishopCheckQueen * mgBase;
  weights->bishopCheckQueen.eg.g += position->coeffs.bishopCheckQueen * egBase;

  weights->rookCheckQueen.mg.g += position->coeffs.rookCheckQueen * mgBase;
  weights->rookCheckQueen.eg.g += position->coeffs.rookCheckQueen * egBase;
}

void UpdatePieceBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  weights->bishopPair.mg.g += position->coeffs.bishopPair * mgBase;
  weights->bishopPair.eg.g += position->coeffs.bishopPair * egBase;

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      weights->imbalance[i][j].mg.g += position->coeffs.imbalance[i][j] * mgBase;
      weights->imbalance[i][j].eg.g += position->coeffs.imbalance[i][j] * egBase;
    }
  }

  weights->minorBehindPawn.mg.g += position->coeffs.minorBehindPawn * mgBase;
  weights->minorBehindPawn.eg.g += position->coeffs.minorBehindPawn * egBase;

  weights->knightPostReachable.mg.g += position->coeffs.knightPostReachable * mgBase;
  weights->knightPostReachable.eg.g += position->coeffs.knightPostReachable * egBase;

  weights->bishopPostReachable.mg.g += position->coeffs.bishopPostReachable * mgBase;
  weights->bishopPostReachable.eg.g += position->coeffs.bishopPostReachable * egBase;

  weights->bishopTrapped.mg.g += position->coeffs.bishopTrapped * mgBase;
  weights->bishopTrapped.eg.g += position->coeffs.bishopTrapped * egBase;

  weights->rookTrapped.mg.g += position->coeffs.rookTrapped * mgBase;
  weights->rookTrapped.eg.g += position->coeffs.rookTrapped * egBase;

  weights->badBishopPawns.mg.g += position->coeffs.badBishopPawns * mgBase;
  weights->badBishopPawns.eg.g += position->coeffs.badBishopPawns * egBase;

  weights->dragonBishop.mg.g += position->coeffs.dragonBishop * mgBase;
  weights->dragonBishop.eg.g += position->coeffs.dragonBishop * egBase;

  weights->rookOpenFileOffset.mg.g += position->coeffs.rookOpenFileOffset * mgBase;
  weights->rookOpenFileOffset.eg.g += position->coeffs.rookOpenFileOffset * egBase;

  weights->rookOpenFile.mg.g += position->coeffs.rookOpenFile * mgBase;
  weights->rookOpenFile.eg.g += position->coeffs.rookOpenFile * egBase;

  weights->rookSemiOpen.mg.g += position->coeffs.rookSemiOpen * mgBase;
  weights->rookSemiOpen.eg.g += position->coeffs.rookSemiOpen * egBase;

  weights->rookToOpen.mg.g += position->coeffs.rookToOpen * mgBase;
  weights->rookToOpen.eg.g += position->coeffs.rookToOpen * egBase;

  weights->queenOppositeRook.mg.g += position->coeffs.queenOppositeRook * mgBase;
  weights->queenOppositeRook.eg.g += position->coeffs.queenOppositeRook * egBase;

  weights->queenRookBattery.mg.g += position->coeffs.queenRookBattery * mgBase;
  weights->queenRookBattery.eg.g += position->coeffs.queenRookBattery * egBase;
}

void UpdatePawnBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  weights->defendedPawns.mg.g += position->coeffs.defendedPawns * mgBase;
  weights->defendedPawns.eg.g += position->coeffs.defendedPawns * egBase;

  weights->doubledPawns.mg.g += position->coeffs.doubledPawns * mgBase;
  weights->doubledPawns.eg.g += position->coeffs.doubledPawns * egBase;

  for (int f = 0; f < 4; f++) {
    weights->isolatedPawns[f].mg.g += position->coeffs.isolatedPawns[f] * mgBase;
    weights->isolatedPawns[f].eg.g += position->coeffs.isolatedPawns[f] * egBase;
  }

  weights->openIsolatedPawns.mg.g += position->coeffs.openIsolatedPawns * mgBase;
  weights->openIsolatedPawns.eg.g += position->coeffs.openIsolatedPawns * egBase;

  weights->backwardsPawns.mg.g += position->coeffs.backwardsPawns * mgBase;
  weights->backwardsPawns.eg.g += position->coeffs.backwardsPawns * egBase;

  weights->candidateEdgeDistance.mg.g += position->coeffs.candidateEdgeDistance * mgBase;
  weights->candidateEdgeDistance.eg.g += position->coeffs.candidateEdgeDistance * egBase;

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++) {
      weights->connectedPawn[f][r].mg.g += position->coeffs.connectedPawn[f][r] * mgBase;
      weights->connectedPawn[f][r].eg.g += position->coeffs.connectedPawn[f][r] * egBase;
    }

    weights->candidatePasser[r].mg.g += position->coeffs.candidatePasser[r] * mgBase;
    weights->candidatePasser[r].eg.g += position->coeffs.candidatePasser[r] * egBase;
  }
}

void UpdatePasserBonusGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.g += position->coeffs.passedPawn[r] * mgBase;
    weights->passedPawn[r].eg.g += position->coeffs.passedPawn[r] * egBase;
  }

  weights->passedPawnEdgeDistance.mg.g += position->coeffs.passedPawnEdgeDistance * mgBase;
  weights->passedPawnEdgeDistance.eg.g += position->coeffs.passedPawnEdgeDistance * egBase;

  weights->passedPawnKingProximity.mg.g += position->coeffs.passedPawnKingProximity * mgBase;
  weights->passedPawnKingProximity.eg.g += position->coeffs.passedPawnKingProximity * egBase;

  for (int r = 0; r < 5; r++) {
    weights->passedPawnAdvance[r].mg.g += position->coeffs.passedPawnAdvance[r] * mgBase;
    weights->passedPawnAdvance[r].eg.g += position->coeffs.passedPawnAdvance[r] * egBase;
  }

  weights->passedPawnEnemySliderBehind.mg.g += position->coeffs.passedPawnEnemySliderBehind * mgBase;
  weights->passedPawnEnemySliderBehind.eg.g += position->coeffs.passedPawnEnemySliderBehind * egBase;

  weights->passedPawnSqRule.mg.g += position->coeffs.passedPawnSqRule * mgBase;
  weights->passedPawnSqRule.eg.g += position->coeffs.passedPawnSqRule * egBase;

  weights->passedPawnUnsupported.mg.g += position->coeffs.passedPawnUnsupported * mgBase;
  weights->passedPawnUnsupported.eg.g += position->coeffs.passedPawnUnsupported * egBase;
}

void UpdatePawnShelterGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->eg == 0.0 || gd->complexity >= -fabs(gd->eg));

  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->pawnShelter[f][r].mg.g += position->coeffs.pawnShelter[f][r] * mgBase;
      weights->pawnShelter[f][r].eg.g += position->coeffs.pawnShelter[f][r] * egBase;

      weights->pawnStorm[f][r].mg.g += position->coeffs.pawnStorm[f][r] * mgBase;
      weights->pawnStorm[f][r].eg.g += position->coeffs.pawnStorm[f][r] * egBase;
    }
  }

  for (int f = 0; f < 8; f++) {
    weights->blockedPawnStorm[f].mg.g += position->coeffs.blockedPawnStorm[f] * mgBase;
    weights->blockedPawnStorm[f].eg.g += position->coeffs.blockedPawnStorm[f] * egBase;
  }

  weights->castlingRights.mg.g += position->coeffs.castlingRights * mgBase;
  weights->castlingRights.eg.g += position->coeffs.castlingRights * egBase;
}

void UpdateSpaceGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;

  weights->space.mg.g += position->coeffs.space * mgBase / 1024.0;
}

void UpdateComplexityGradients(Position* position, double loss, Weights* weights, EvalGradientData* gd) {
  int sign = (gd->eg > 0) - (gd->eg < 0);
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (gd->complexity >= -fabs(gd->eg));

  weights->complexPawns.mg.g += position->coeffs.complexPawns * egBase * sign;
  weights->complexPawnsOffset.mg.g += position->coeffs.complexPawnsOffset * egBase * sign;
  weights->complexOffset.mg.g += position->coeffs.complexOffset * egBase * sign;
}

void UpdateKingSafetyGradients(Position* position, double loss, Weights* weights, EvalGradientData* ks) {
  double mgBase = position->phaseMg * position->scale * loss / MAX_SCALE;
  double egBase = position->phaseEg * position->scale * loss / MAX_SCALE;
  egBase *= (ks->eg == 0.0 || ks->complexity >= -fabs(ks->eg));

  for (int i = 1; i < 5; i++) {
    weights->ksAttackerWeight[i].mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) *
                                         position->coeffs.ksAttackerWeights[BLACK][i] *
                                         position->coeffs.ksAttackerCount[BLACK];
    weights->ksAttackerWeight[i].mg.g += (egBase / 32) * (ks->bDanger > 0) *
                                         position->coeffs.ksAttackerWeights[BLACK][i] *
                                         position->coeffs.ksAttackerCount[BLACK];

    weights->ksAttackerWeight[i].mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) *
                                         position->coeffs.ksAttackerWeights[WHITE][i] *
                                         position->coeffs.ksAttackerCount[WHITE];
    weights->ksAttackerWeight[i].mg.g -= (egBase / 32) * (ks->wDanger > 0) *
                                         position->coeffs.ksAttackerWeights[WHITE][i] *
                                         position->coeffs.ksAttackerCount[WHITE];
  }

  weights->ksWeakSqs.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksWeakSqs[BLACK];
  weights->ksWeakSqs.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksWeakSqs[BLACK];
  weights->ksWeakSqs.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksWeakSqs[WHITE];
  weights->ksWeakSqs.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksWeakSqs[WHITE];

  weights->ksPinned.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksPinned[BLACK];
  weights->ksPinned.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksPinned[BLACK];
  weights->ksPinned.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksPinned[WHITE];
  weights->ksPinned.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksPinned[WHITE];

  weights->ksKnightCheck.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksKnightCheck[BLACK];
  weights->ksKnightCheck.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksKnightCheck[BLACK];
  weights->ksKnightCheck.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksKnightCheck[WHITE];
  weights->ksKnightCheck.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksKnightCheck[WHITE];

  weights->ksBishopCheck.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksBishopCheck[BLACK];
  weights->ksBishopCheck.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksBishopCheck[BLACK];
  weights->ksBishopCheck.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksBishopCheck[WHITE];
  weights->ksBishopCheck.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksBishopCheck[WHITE];

  weights->ksRookCheck.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksRookCheck[BLACK];
  weights->ksRookCheck.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksRookCheck[BLACK];
  weights->ksRookCheck.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksRookCheck[WHITE];
  weights->ksRookCheck.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksRookCheck[WHITE];

  weights->ksQueenCheck.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksQueenCheck[BLACK];
  weights->ksQueenCheck.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksQueenCheck[BLACK];
  weights->ksQueenCheck.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksQueenCheck[WHITE];
  weights->ksQueenCheck.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksQueenCheck[WHITE];

  weights->ksUnsafeCheck.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksUnsafeCheck[BLACK];
  weights->ksUnsafeCheck.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksUnsafeCheck[BLACK];
  weights->ksUnsafeCheck.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksUnsafeCheck[WHITE];
  weights->ksUnsafeCheck.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksUnsafeCheck[WHITE];

  weights->ksEnemyQueen.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksEnemyQueen[BLACK];
  weights->ksEnemyQueen.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksEnemyQueen[BLACK];
  weights->ksEnemyQueen.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksEnemyQueen[WHITE];
  weights->ksEnemyQueen.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksEnemyQueen[WHITE];

  weights->ksKnightDefense.mg.g += (mgBase / 512) * fmax(ks->bDanger, 0) * position->coeffs.ksKnightDefense[BLACK];
  weights->ksKnightDefense.mg.g += (egBase / 32) * (ks->bDanger > 0) * position->coeffs.ksKnightDefense[BLACK];
  weights->ksKnightDefense.mg.g -= (mgBase / 512) * fmax(ks->wDanger, 0) * position->coeffs.ksKnightDefense[WHITE];
  weights->ksKnightDefense.mg.g -= (egBase / 32) * (ks->wDanger > 0) * position->coeffs.ksKnightDefense[WHITE];
}

void UpdateTempoGradient(Position* position, double loss, Weights* weights) {
  weights->tempo.mg.g += (position->stm == WHITE ? 1 : -1) * loss;
}

void ApplyCoeff(double* mg, double* eg, int coeff, Weight* w) {
  *mg += coeff * w->mg.value;
  *eg += coeff * w->eg.value;
}

double EvaluateCoeffs(Position* position, Weights* weights, EvalGradientData* gd) {
  double mg = 0, eg = 0;

  EvaluateMaterialValues(&mg, &eg, position, weights);
  EvaluatePsqtValues(&mg, &eg, position, weights);
  EvaluatePostPsqtValues(&mg, &eg, position, weights);
  EvaluateMobilityValues(&mg, &eg, position, weights);
  EvaluateThreatValues(&mg, &eg, position, weights);
  EvaluatePieceBonusValues(&mg, &eg, position, weights);
  EvaluatePawnBonusValues(&mg, &eg, position, weights);
  EvaluatePasserBonusValues(&mg, &eg, position, weights);
  EvaluatePawnShelterValues(&mg, &eg, position, weights);
  EvaluateSpaceValues(&mg, &eg, position, weights);

  if (TUNE_KS)
    EvaluateKingSafetyValues(&mg, &eg, position, weights, gd);
  else {
    mg += scoreMG(position->coeffs.ks);
    eg += scoreEG(position->coeffs.ks);
  }

  EvaluateComplexityValues(&mg, &eg, position, weights, gd);

  double result = (mg * position->phase + eg * (128 - position->phase)) / 128;
  result = (result * position->scale) / MAX_SCALE;
  return result + (position->stm == WHITE ? weights->tempo.mg.value : -weights->tempo.mg.value);
}

void EvaluateMaterialValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++)
    ApplyCoeff(mg, eg, position->coeffs.pieces[pc], &weights->pieces[pc]);
}

void EvaluatePsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = PAWN_TYPE; pc <= KING_TYPE; pc++)
    for (int sq = 0; sq < 32; sq++)
      for (int i = 0; i < 2; i++)
        ApplyCoeff(mg, eg, position->coeffs.psqt[pc][i][sq], &weights->psqt[pc][i][sq]);
}

void EvaluatePostPsqtValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int sq = 0; sq < 12; sq++) {
    ApplyCoeff(mg, eg, position->coeffs.knightPostPsqt[sq], &weights->knightPostPsqt[sq]);

    ApplyCoeff(mg, eg, position->coeffs.bishopPostPsqt[sq], &weights->bishopPostPsqt[sq]);
  }
}

void EvaluateMobilityValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int c = 0; c < 9; c++)
    ApplyCoeff(mg, eg, position->coeffs.knightMobilities[c], &weights->knightMobilities[c]);

  for (int c = 0; c < 14; c++)
    ApplyCoeff(mg, eg, position->coeffs.bishopMobilities[c], &weights->bishopMobilities[c]);

  for (int c = 0; c < 15; c++)
    ApplyCoeff(mg, eg, position->coeffs.rookMobilities[c], &weights->rookMobilities[c]);

  for (int c = 0; c < 28; c++)
    ApplyCoeff(mg, eg, position->coeffs.queenMobilities[c], &weights->queenMobilities[c]);
}

void EvaluateThreatValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    ApplyCoeff(mg, eg, position->coeffs.knightThreats[pc], &weights->knightThreats[pc]);
    ApplyCoeff(mg, eg, position->coeffs.bishopThreats[pc], &weights->bishopThreats[pc]);
    ApplyCoeff(mg, eg, position->coeffs.rookThreats[pc], &weights->rookThreats[pc]);
  }

  ApplyCoeff(mg, eg, position->coeffs.kingThreat, &weights->kingThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnThreat, &weights->pawnThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnPushThreat, &weights->pawnPushThreat);
  ApplyCoeff(mg, eg, position->coeffs.pawnPushThreatPinned, &weights->pawnPushThreatPinned);
  ApplyCoeff(mg, eg, position->coeffs.hangingThreat, &weights->hangingThreat);
  ApplyCoeff(mg, eg, position->coeffs.knightCheckQueen, &weights->knightCheckQueen);
  ApplyCoeff(mg, eg, position->coeffs.bishopCheckQueen, &weights->bishopCheckQueen);
  ApplyCoeff(mg, eg, position->coeffs.rookCheckQueen, &weights->rookCheckQueen);
}

void EvaluatePieceBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  ApplyCoeff(mg, eg, position->coeffs.bishopPair, &weights->bishopPair);

  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 5; j++)
      ApplyCoeff(mg, eg, position->coeffs.imbalance[i][j], &weights->imbalance[i][j]);

  ApplyCoeff(mg, eg, position->coeffs.minorBehindPawn, &weights->minorBehindPawn);
  ApplyCoeff(mg, eg, position->coeffs.knightPostReachable, &weights->knightPostReachable);
  ApplyCoeff(mg, eg, position->coeffs.bishopPostReachable, &weights->bishopPostReachable);
  ApplyCoeff(mg, eg, position->coeffs.bishopTrapped, &weights->bishopTrapped);
  ApplyCoeff(mg, eg, position->coeffs.rookTrapped, &weights->rookTrapped);
  ApplyCoeff(mg, eg, position->coeffs.badBishopPawns, &weights->badBishopPawns);
  ApplyCoeff(mg, eg, position->coeffs.dragonBishop, &weights->dragonBishop);
  ApplyCoeff(mg, eg, position->coeffs.rookOpenFileOffset, &weights->rookOpenFileOffset);
  ApplyCoeff(mg, eg, position->coeffs.rookOpenFile, &weights->rookOpenFile);
  ApplyCoeff(mg, eg, position->coeffs.rookSemiOpen, &weights->rookSemiOpen);
  ApplyCoeff(mg, eg, position->coeffs.rookToOpen, &weights->rookToOpen);
  ApplyCoeff(mg, eg, position->coeffs.queenOppositeRook, &weights->queenOppositeRook);
  ApplyCoeff(mg, eg, position->coeffs.queenRookBattery, &weights->queenRookBattery);
}

void EvaluatePawnBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  ApplyCoeff(mg, eg, position->coeffs.defendedPawns, &weights->defendedPawns);
  ApplyCoeff(mg, eg, position->coeffs.doubledPawns, &weights->doubledPawns);
  ApplyCoeff(mg, eg, position->coeffs.openIsolatedPawns, &weights->openIsolatedPawns);
  ApplyCoeff(mg, eg, position->coeffs.backwardsPawns, &weights->backwardsPawns);
  ApplyCoeff(mg, eg, position->coeffs.candidateEdgeDistance, &weights->candidateEdgeDistance);

  for (int f = 0; f < 4; f++)
    ApplyCoeff(mg, eg, position->coeffs.isolatedPawns[f], &weights->isolatedPawns[f]);

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++)
      ApplyCoeff(mg, eg, position->coeffs.connectedPawn[f][r], &weights->connectedPawn[f][r]);
    ApplyCoeff(mg, eg, position->coeffs.candidatePasser[r], &weights->candidatePasser[r]);
  }
}

void EvaluatePasserBonusValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int r = 0; r < 8; r++)
    ApplyCoeff(mg, eg, position->coeffs.passedPawn[r], &weights->passedPawn[r]);

  ApplyCoeff(mg, eg, position->coeffs.passedPawnEdgeDistance, &weights->passedPawnEdgeDistance);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnKingProximity, &weights->passedPawnKingProximity);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnEnemySliderBehind, &weights->passedPawnEnemySliderBehind);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnSqRule, &weights->passedPawnSqRule);
  ApplyCoeff(mg, eg, position->coeffs.passedPawnUnsupported, &weights->passedPawnUnsupported);

  for (int r = 0; r < 5; r++)
    ApplyCoeff(mg, eg, position->coeffs.passedPawnAdvance[r], &weights->passedPawnAdvance[r]);
}

void EvaluatePawnShelterValues(double* mg, double* eg, Position* position, Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      ApplyCoeff(mg, eg, position->coeffs.pawnShelter[f][r], &weights->pawnShelter[f][r]);
      ApplyCoeff(mg, eg, position->coeffs.pawnStorm[f][r], &weights->pawnStorm[f][r]);
    }
  }

  for (int r = 0; r < 8; r++)
    ApplyCoeff(mg, eg, position->coeffs.blockedPawnStorm[r], &weights->blockedPawnStorm[r]);

  ApplyCoeff(mg, eg, position->coeffs.castlingRights, &weights->castlingRights);
}

void EvaluateSpaceValues(double* mg, double* eg, Position* position, Weights* weights) {
  *mg += position->coeffs.space * weights->space.mg.value / 1024.0;
}

void EvaluateComplexityValues(double* mg, double* eg, Position* position, Weights* weights, EvalGradientData* gd) {
  double complexity = 0.0;
  complexity += position->coeffs.complexPawns * weights->complexPawns.mg.value;
  complexity += position->coeffs.complexPawnsOffset * weights->complexPawnsOffset.mg.value;
  complexity += position->coeffs.complexOffset * weights->complexOffset.mg.value;

  gd->eg = *eg;
  gd->complexity = complexity;

  if (*eg > 0)
    *eg = fmax(0.0, *eg + complexity);
  else if (*eg < 0)
    *eg = fmin(0.0, *eg - complexity);
}

void EvaluateKingSafetyValues(double* mg, double* eg, Position* position, Weights* weights, EvalGradientData* ks) {
  float wDanger = 0.0;
  float bDanger = 0.0;

  for (int i = 1; i < 5; i++) {
    wDanger += position->coeffs.ksAttackerWeights[WHITE][i] * position->coeffs.ksAttackerCount[WHITE] *
               weights->ksAttackerWeight[i].mg.value;
    bDanger += position->coeffs.ksAttackerWeights[BLACK][i] * position->coeffs.ksAttackerCount[BLACK] *
               weights->ksAttackerWeight[i].mg.value;
  }

  wDanger += position->coeffs.ksWeakSqs[WHITE] * weights->ksWeakSqs.mg.value;
  wDanger += position->coeffs.ksPinned[WHITE] * weights->ksPinned.mg.value;
  wDanger += position->coeffs.ksKnightCheck[WHITE] * weights->ksKnightCheck.mg.value;
  wDanger += position->coeffs.ksBishopCheck[WHITE] * weights->ksBishopCheck.mg.value;
  wDanger += position->coeffs.ksRookCheck[WHITE] * weights->ksRookCheck.mg.value;
  wDanger += position->coeffs.ksQueenCheck[WHITE] * weights->ksQueenCheck.mg.value;
  wDanger += position->coeffs.ksUnsafeCheck[WHITE] * weights->ksUnsafeCheck.mg.value;
  wDanger += position->coeffs.ksEnemyQueen[WHITE] * weights->ksEnemyQueen.mg.value;
  wDanger += position->coeffs.ksKnightDefense[WHITE] * weights->ksKnightDefense.mg.value;

  bDanger += position->coeffs.ksWeakSqs[BLACK] * weights->ksWeakSqs.mg.value;
  bDanger += position->coeffs.ksPinned[BLACK] * weights->ksPinned.mg.value;
  bDanger += position->coeffs.ksKnightCheck[BLACK] * weights->ksKnightCheck.mg.value;
  bDanger += position->coeffs.ksBishopCheck[BLACK] * weights->ksBishopCheck.mg.value;
  bDanger += position->coeffs.ksRookCheck[BLACK] * weights->ksRookCheck.mg.value;
  bDanger += position->coeffs.ksQueenCheck[BLACK] * weights->ksQueenCheck.mg.value;
  bDanger += position->coeffs.ksUnsafeCheck[BLACK] * weights->ksUnsafeCheck.mg.value;
  bDanger += position->coeffs.ksEnemyQueen[BLACK] * weights->ksEnemyQueen.mg.value;
  bDanger += position->coeffs.ksKnightDefense[BLACK] * weights->ksKnightDefense.mg.value;

  ks->wDanger = wDanger;
  ks->bDanger = bDanger;

  *mg += -wDanger * fmax(wDanger, 0.0) / 1024;
  *eg += -fmax(wDanger, 0.0) / 32;

  *mg -= -bDanger * fmax(bDanger, 0.0) / 1024;
  *eg -= -fmax(bDanger, 0.0) / 32;
}

void LoadPosition(Board* board, Position* position, ThreadData* thread) {
  memset(&C, 0, sizeof(EvalCoeffs));

  int phase = GetPhase(board);

  position->phaseMg = (double)phase / 128;
  position->phaseEg = 1 - (double)phase / 128;
  position->phase = phase;

  position->stm = board->side;

  Score eval = Evaluate(board, thread);
  position->staticEval = sideScalar[board->side] * eval;

  position->scale = Scale(board, C.ss);

  memcpy(&position->coeffs, &C, sizeof(EvalCoeffs));
}

Position* LoadPositions(int* n, Weights* weights) {
  FILE* fp;
  fp = fopen(EPD_FILE_PATH, "r");

  if (fp == NULL)
    return NULL;

  Position* positions = calloc(MAX_POSITIONS, sizeof(Position));

  EvalGradientData ks;
  Board board;
  ThreadData* threads = CreatePool(1);

  char buffer[128];

  int p = 0;
  while (p < MAX_POSITIONS && fgets(buffer, 128, fp)) {
    if (strstr(buffer, "[1.0]"))
      positions[p].result = 1.0;
    else if (strstr(buffer, "[0.5]"))
      positions[p].result = 0.5;
    else if (strstr(buffer, "[0.0]"))
      positions[p].result = 0.0;
    else {
      printf("Cannot Parse %s\n", buffer);
      exit(EXIT_FAILURE);
    }

    ParseFen(buffer, &board);

    if (board.checkers)
      continue;

    if (!(board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    if (bits(board.occupancies[BOTH]) == 3 && (board.pieces[PAWN_WHITE] | board.pieces[PAWN_BLACK]))
      continue;

    LoadPosition(&board, &positions[p], threads);
    if (abs(positions[p].staticEval) > 3000)
      continue;

    double eval = EvaluateCoeffs(&positions[p], weights, &ks);
    if (fabs(positions[p].staticEval - eval) > 3) {
      printf("The coefficient based evaluation does NOT match the eval!\n");
      printf("FEN: %s\n", buffer);
      printf("Static: %d, Coeffs: %f\n", positions[p].staticEval, eval);
      exit(1);
    }

    if (!(++p & 4095))
      printf("Loaded %d positions...\r", p);
  }

  printf("Successfully loaded %d positions.\n", p);

  *n = p;

  positions = realloc(positions, sizeof(Position) * p);
  return positions;
}

void InitMaterialWeights(Weights* weights) {
  for (int pc = PAWN_TYPE; pc < KING_TYPE; pc++) {
    weights->pieces[pc].mg.value = scoreMG(MATERIAL_VALUES[pc]);
    weights->pieces[pc].eg.value = scoreEG(MATERIAL_VALUES[pc]);
  }
}

void InitPsqtWeights(Weights* weights) {
  for (int sq = 0; sq < 32; sq++) {
    for (int i = 0; i < 2; i++) {
      weights->psqt[PAWN_TYPE][i][sq].mg.value = scoreMG(PAWN_PSQT[i][sq]);
      weights->psqt[PAWN_TYPE][i][sq].eg.value = scoreEG(PAWN_PSQT[i][sq]);

      weights->psqt[KNIGHT_TYPE][i][sq].mg.value = scoreMG(KNIGHT_PSQT[i][sq]);
      weights->psqt[KNIGHT_TYPE][i][sq].eg.value = scoreEG(KNIGHT_PSQT[i][sq]);

      weights->psqt[BISHOP_TYPE][i][sq].mg.value = scoreMG(BISHOP_PSQT[i][sq]);
      weights->psqt[BISHOP_TYPE][i][sq].eg.value = scoreEG(BISHOP_PSQT[i][sq]);

      weights->psqt[ROOK_TYPE][i][sq].mg.value = scoreMG(ROOK_PSQT[i][sq]);
      weights->psqt[ROOK_TYPE][i][sq].eg.value = scoreEG(ROOK_PSQT[i][sq]);

      weights->psqt[QUEEN_TYPE][i][sq].mg.value = scoreMG(QUEEN_PSQT[i][sq]);
      weights->psqt[QUEEN_TYPE][i][sq].eg.value = scoreEG(QUEEN_PSQT[i][sq]);

      weights->psqt[KING_TYPE][i][sq].mg.value = scoreMG(KING_PSQT[i][sq]);
      weights->psqt[KING_TYPE][i][sq].eg.value = scoreEG(KING_PSQT[i][sq]);
    }
  }
}

void InitPostPsqtWeights(Weights* weights) {
  for (int sq = 0; sq < 12; sq++) {
    weights->knightPostPsqt[sq].mg.value = scoreMG(KNIGHT_POST_PSQT[sq]);
    weights->knightPostPsqt[sq].eg.value = scoreEG(KNIGHT_POST_PSQT[sq]);

    weights->bishopPostPsqt[sq].mg.value = scoreMG(BISHOP_POST_PSQT[sq]);
    weights->bishopPostPsqt[sq].eg.value = scoreEG(BISHOP_POST_PSQT[sq]);
  }
}

void InitMobilityWeights(Weights* weights) {
  for (int c = 0; c < 9; c++) {
    weights->knightMobilities[c].mg.value = scoreMG(KNIGHT_MOBILITIES[c]);
    weights->knightMobilities[c].eg.value = scoreEG(KNIGHT_MOBILITIES[c]);
  }

  for (int c = 0; c < 14; c++) {
    weights->bishopMobilities[c].mg.value = scoreMG(BISHOP_MOBILITIES[c]);
    weights->bishopMobilities[c].eg.value = scoreEG(BISHOP_MOBILITIES[c]);
  }

  for (int c = 0; c < 15; c++) {
    weights->rookMobilities[c].mg.value = scoreMG(ROOK_MOBILITIES[c]);
    weights->rookMobilities[c].eg.value = scoreEG(ROOK_MOBILITIES[c]);
  }

  for (int c = 0; c < 28; c++) {
    weights->queenMobilities[c].mg.value = scoreMG(QUEEN_MOBILITIES[c]);
    weights->queenMobilities[c].eg.value = scoreEG(QUEEN_MOBILITIES[c]);
  }
}

void InitThreatWeights(Weights* weights) {
  for (int pc = 0; pc < 6; pc++) {
    weights->knightThreats[pc].mg.value = scoreMG(KNIGHT_THREATS[pc]);
    weights->knightThreats[pc].eg.value = scoreEG(KNIGHT_THREATS[pc]);

    weights->bishopThreats[pc].mg.value = scoreMG(BISHOP_THREATS[pc]);
    weights->bishopThreats[pc].eg.value = scoreEG(BISHOP_THREATS[pc]);

    weights->rookThreats[pc].mg.value = scoreMG(ROOK_THREATS[pc]);
    weights->rookThreats[pc].eg.value = scoreEG(ROOK_THREATS[pc]);
  }

  weights->kingThreat.mg.value = scoreMG(KING_THREAT);
  weights->kingThreat.eg.value = scoreEG(KING_THREAT);

  weights->pawnThreat.mg.value = scoreMG(PAWN_THREAT);
  weights->pawnThreat.eg.value = scoreEG(PAWN_THREAT);

  weights->pawnPushThreat.mg.value = scoreMG(PAWN_PUSH_THREAT);
  weights->pawnPushThreat.eg.value = scoreEG(PAWN_PUSH_THREAT);

  weights->pawnPushThreatPinned.mg.value = scoreMG(PAWN_PUSH_THREAT_PINNED);
  weights->pawnPushThreatPinned.eg.value = scoreEG(PAWN_PUSH_THREAT_PINNED);

  weights->hangingThreat.mg.value = scoreMG(HANGING_THREAT);
  weights->hangingThreat.eg.value = scoreEG(HANGING_THREAT);

  weights->knightCheckQueen.mg.value = scoreMG(KNIGHT_CHECK_QUEEN);
  weights->knightCheckQueen.eg.value = scoreEG(KNIGHT_CHECK_QUEEN);

  weights->bishopCheckQueen.mg.value = scoreMG(BISHOP_CHECK_QUEEN);
  weights->bishopCheckQueen.eg.value = scoreEG(BISHOP_CHECK_QUEEN);

  weights->rookCheckQueen.mg.value = scoreMG(ROOK_CHECK_QUEEN);
  weights->rookCheckQueen.eg.value = scoreEG(ROOK_CHECK_QUEEN);
}

void InitPieceBonusWeights(Weights* weights) {
  weights->bishopPair.mg.value = scoreMG(BISHOP_PAIR);
  weights->bishopPair.eg.value = scoreEG(BISHOP_PAIR);

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      weights->imbalance[i][j].mg.value = scoreMG(IMBALANCE[i][j]);
      weights->imbalance[i][j].eg.value = scoreEG(IMBALANCE[i][j]);
    }
  }

  weights->minorBehindPawn.mg.value = scoreMG(MINOR_BEHIND_PAWN);
  weights->minorBehindPawn.eg.value = scoreEG(MINOR_BEHIND_PAWN);

  weights->knightPostReachable.mg.value = scoreMG(KNIGHT_OUTPOST_REACHABLE);
  weights->knightPostReachable.eg.value = scoreEG(KNIGHT_OUTPOST_REACHABLE);

  weights->bishopPostReachable.mg.value = scoreMG(BISHOP_OUTPOST_REACHABLE);
  weights->bishopPostReachable.eg.value = scoreEG(BISHOP_OUTPOST_REACHABLE);

  weights->bishopTrapped.mg.value = scoreMG(BISHOP_TRAPPED);
  weights->bishopTrapped.eg.value = scoreEG(BISHOP_TRAPPED);

  weights->rookTrapped.mg.value = scoreMG(ROOK_TRAPPED);
  weights->rookTrapped.eg.value = scoreEG(ROOK_TRAPPED);

  weights->badBishopPawns.mg.value = scoreMG(BAD_BISHOP_PAWNS);
  weights->badBishopPawns.eg.value = scoreEG(BAD_BISHOP_PAWNS);

  weights->dragonBishop.mg.value = scoreMG(DRAGON_BISHOP);
  weights->dragonBishop.eg.value = scoreEG(DRAGON_BISHOP);

  weights->rookOpenFileOffset.mg.value = scoreMG(ROOK_OPEN_FILE_OFFSET);
  weights->rookOpenFileOffset.eg.value = scoreEG(ROOK_OPEN_FILE_OFFSET);

  weights->rookOpenFile.mg.value = scoreMG(ROOK_OPEN_FILE);
  weights->rookOpenFile.eg.value = scoreEG(ROOK_OPEN_FILE);

  weights->rookSemiOpen.mg.value = scoreMG(ROOK_SEMI_OPEN);
  weights->rookSemiOpen.eg.value = scoreEG(ROOK_SEMI_OPEN);

  weights->rookToOpen.mg.value = scoreMG(ROOK_TO_OPEN);
  weights->rookToOpen.eg.value = scoreEG(ROOK_TO_OPEN);

  weights->queenOppositeRook.mg.value = scoreMG(QUEEN_OPPOSITE_ROOK);
  weights->queenOppositeRook.eg.value = scoreEG(QUEEN_OPPOSITE_ROOK);

  weights->queenRookBattery.mg.value = scoreMG(QUEEN_ROOK_BATTERY);
  weights->queenRookBattery.eg.value = scoreEG(QUEEN_ROOK_BATTERY);
}

void InitPawnBonusWeights(Weights* weights) {
  weights->defendedPawns.mg.value = scoreMG(DEFENDED_PAWN);
  weights->defendedPawns.eg.value = scoreEG(DEFENDED_PAWN);

  weights->doubledPawns.mg.value = scoreMG(DOUBLED_PAWN);
  weights->doubledPawns.eg.value = scoreEG(DOUBLED_PAWN);

  for (int f = 0; f < 4; f++) {
    weights->isolatedPawns[f].mg.value = scoreMG(ISOLATED_PAWN[f]);
    weights->isolatedPawns[f].eg.value = scoreEG(ISOLATED_PAWN[f]);
  }

  weights->openIsolatedPawns.mg.value = scoreMG(OPEN_ISOLATED_PAWN);
  weights->openIsolatedPawns.eg.value = scoreEG(OPEN_ISOLATED_PAWN);

  weights->backwardsPawns.mg.value = scoreMG(BACKWARDS_PAWN);
  weights->backwardsPawns.eg.value = scoreEG(BACKWARDS_PAWN);

  for (int r = 0; r < 8; r++) {
    for (int f = 0; f < 4; f++) {
      weights->connectedPawn[f][r].mg.value = scoreMG(CONNECTED_PAWN[f][r]);
      weights->connectedPawn[f][r].eg.value = scoreEG(CONNECTED_PAWN[f][r]);
    }

    weights->candidatePasser[r].mg.value = scoreMG(CANDIDATE_PASSER[r]);
    weights->candidatePasser[r].eg.value = scoreEG(CANDIDATE_PASSER[r]);
  }

  weights->candidateEdgeDistance.mg.value = scoreMG(CANDIDATE_EDGE_DISTANCE);
  weights->candidateEdgeDistance.eg.value = scoreEG(CANDIDATE_EDGE_DISTANCE);
}

void InitPasserBonusWeights(Weights* weights) {
  for (int r = 0; r < 8; r++) {
    weights->passedPawn[r].mg.value = scoreMG(PASSED_PAWN[r]);
    weights->passedPawn[r].eg.value = scoreEG(PASSED_PAWN[r]);
  }

  weights->passedPawnEdgeDistance.mg.value = scoreMG(PASSED_PAWN_EDGE_DISTANCE);
  weights->passedPawnEdgeDistance.eg.value = scoreEG(PASSED_PAWN_EDGE_DISTANCE);

  weights->passedPawnKingProximity.mg.value = scoreMG(PASSED_PAWN_KING_PROXIMITY);
  weights->passedPawnKingProximity.eg.value = scoreEG(PASSED_PAWN_KING_PROXIMITY);

  for (int r = 0; r < 5; r++) {
    weights->passedPawnAdvance[r].mg.value = scoreMG(PASSED_PAWN_ADVANCE_DEFENDED[r]);
    weights->passedPawnAdvance[r].eg.value = scoreEG(PASSED_PAWN_ADVANCE_DEFENDED[r]);
  }

  weights->passedPawnEnemySliderBehind.mg.value = scoreMG(PASSED_PAWN_ENEMY_SLIDER_BEHIND);
  weights->passedPawnEnemySliderBehind.eg.value = scoreEG(PASSED_PAWN_ENEMY_SLIDER_BEHIND);

  weights->passedPawnSqRule.mg.value = scoreMG(PASSED_PAWN_SQ_RULE);
  weights->passedPawnSqRule.eg.value = scoreEG(PASSED_PAWN_SQ_RULE);

  weights->passedPawnUnsupported.mg.value = scoreMG(PASSED_PAWN_UNSUPPORTED);
  weights->passedPawnUnsupported.eg.value = scoreEG(PASSED_PAWN_UNSUPPORTED);
}

void InitPawnShelterWeights(Weights* weights) {
  for (int f = 0; f < 4; f++) {
    for (int r = 0; r < 8; r++) {
      weights->pawnShelter[f][r].mg.value = scoreMG(PAWN_SHELTER[f][r]);
      weights->pawnShelter[f][r].eg.value = scoreEG(PAWN_SHELTER[f][r]);

      weights->pawnStorm[f][r].mg.value = scoreMG(PAWN_STORM[f][r]);
      weights->pawnStorm[f][r].eg.value = scoreEG(PAWN_STORM[f][r]);
    }
  }

  for (int r = 0; r < 8; r++) {
    weights->blockedPawnStorm[r].mg.value = scoreMG(BLOCKED_PAWN_STORM[r]);
    weights->blockedPawnStorm[r].eg.value = scoreEG(BLOCKED_PAWN_STORM[r]);
  }

  weights->castlingRights.mg.value = scoreMG(CAN_CASTLE);
  weights->castlingRights.eg.value = scoreEG(CAN_CASTLE);
}

void InitSpaceWeights(Weights* weights) { weights->space.mg.value = SPACE; }

void InitComplexityWeights(Weights* weights) {
  weights->complexPawns.mg.value = COMPLEXITY_PAWNS;
  weights->complexPawnsOffset.mg.value = COMPLEXITY_PAWNS_OFFSET;
  weights->complexOffset.mg.value = COMPLEXITY_OFFSET;
}

void InitKingSafetyWeights(Weights* weights) {
  for (int i = 1; i < 5; i++)
    weights->ksAttackerWeight[i].mg.value = KS_ATTACKER_WEIGHTS[i];

  weights->ksWeakSqs.mg.value = KS_WEAK_SQS;
  weights->ksPinned.mg.value = KS_PINNED;
  weights->ksKnightCheck.mg.value = KS_KNIGHT_CHECK;
  weights->ksBishopCheck.mg.value = KS_BISHOP_CHECK;
  weights->ksRookCheck.mg.value = KS_ROOK_CHECK;
  weights->ksQueenCheck.mg.value = KS_QUEEN_CHECK;
  weights->ksUnsafeCheck.mg.value = KS_UNSAFE_CHECK;
  weights->ksEnemyQueen.mg.value = KS_ENEMY_QUEEN;
  weights->ksKnightDefense.mg.value = KS_KNIGHT_DEFENSE;
}

void InitTempoWeight(Weights* weights) { weights->tempo.mg.value = TEMPO; }

double Sigmoid(double s) { return 1.0 / (1.0 + exp(-K * s / 400.0)); }

void PrintWeights(Weights* weights, int epoch, double error) {
  FILE* fp;
  fp = fopen("weights.out", "a");

  if (fp == NULL)
    return;

  fprintf(fp, "Epoch: %d, Error: %f\n", epoch, error);

  fprintf(fp, "\nconst Score MATERIAL_VALUES[7] = {");
  PrintWeightArray(fp, weights->pieces, 5, 0);
  fprintf(fp, " S(   0,   0), S(   0,   0) };\n");

  fprintf(fp, "\nconst Score BISHOP_PAIR = ");
  PrintWeight(fp, &weights->bishopPair);

  fprintf(fp, "\nconst Score PAWN_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[PAWN_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[PAWN_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score KNIGHT_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[KNIGHT_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[KNIGHT_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score BISHOP_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[BISHOP_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[BISHOP_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score ROOK_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[ROOK_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[ROOK_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score QUEEN_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[QUEEN_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[QUEEN_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score KING_PSQT[2][32] = {{\n");
  PrintWeightArray(fp, weights->psqt[KING_TYPE][0], 32, 4);
  fprintf(fp, "},{\n");
  PrintWeightArray(fp, weights->psqt[KING_TYPE][1], 32, 4);
  fprintf(fp, "}};\n");

  fprintf(fp, "\nconst Score KNIGHT_POST_PSQT[12] = {\n");
  PrintWeightArray(fp, weights->knightPostPsqt, 12, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_POST_PSQT[12] = {\n");
  PrintWeightArray(fp, weights->bishopPostPsqt, 12, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KNIGHT_MOBILITIES[9] = {\n");
  PrintWeightArray(fp, weights->knightMobilities, 9, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_MOBILITIES[14] = {\n");
  PrintWeightArray(fp, weights->bishopMobilities, 14, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_MOBILITIES[15] = {\n");
  PrintWeightArray(fp, weights->rookMobilities, 15, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score QUEEN_MOBILITIES[28] = {\n");
  PrintWeightArray(fp, weights->queenMobilities, 28, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score MINOR_BEHIND_PAWN = ");
  PrintWeight(fp, &weights->minorBehindPawn);

  fprintf(fp, "\nconst Score KNIGHT_OUTPOST_REACHABLE = ");
  PrintWeight(fp, &weights->knightPostReachable);

  fprintf(fp, "\nconst Score BISHOP_OUTPOST_REACHABLE = ");
  PrintWeight(fp, &weights->bishopPostReachable);

  fprintf(fp, "\nconst Score BISHOP_TRAPPED = ");
  PrintWeight(fp, &weights->bishopTrapped);

  fprintf(fp, "\nconst Score ROOK_TRAPPED = ");
  PrintWeight(fp, &weights->rookTrapped);

  fprintf(fp, "\nconst Score BAD_BISHOP_PAWNS = ");
  PrintWeight(fp, &weights->badBishopPawns);

  fprintf(fp, "\nconst Score DRAGON_BISHOP = ");
  PrintWeight(fp, &weights->dragonBishop);

  fprintf(fp, "\nconst Score ROOK_OPEN_FILE_OFFSET = ");
  PrintWeight(fp, &weights->rookOpenFileOffset);

  fprintf(fp, "\nconst Score ROOK_OPEN_FILE = ");
  PrintWeight(fp, &weights->rookOpenFile);

  fprintf(fp, "\nconst Score ROOK_SEMI_OPEN = ");
  PrintWeight(fp, &weights->rookSemiOpen);

  fprintf(fp, "\nconst Score ROOK_TO_OPEN = ");
  PrintWeight(fp, &weights->rookToOpen);

  fprintf(fp, "\nconst Score QUEEN_OPPOSITE_ROOK = ");
  PrintWeight(fp, &weights->queenOppositeRook);

  fprintf(fp, "\nconst Score QUEEN_ROOK_BATTERY = ");
  PrintWeight(fp, &weights->queenRookBattery);

  fprintf(fp, "\nconst Score DEFENDED_PAWN = ");
  PrintWeight(fp, &weights->defendedPawns);

  fprintf(fp, "\nconst Score DOUBLED_PAWN = ");
  PrintWeight(fp, &weights->doubledPawns);

  fprintf(fp, "\nconst Score ISOLATED_PAWN[4] = {\n");
  PrintWeightArray(fp, weights->isolatedPawns, 4, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score OPEN_ISOLATED_PAWN = ");
  PrintWeight(fp, &weights->openIsolatedPawns);

  fprintf(fp, "\nconst Score BACKWARDS_PAWN = ");
  PrintWeight(fp, &weights->backwardsPawns);

  fprintf(fp, "\nconst Score CONNECTED_PAWN[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->connectedPawn[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->connectedPawn[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score CANDIDATE_PASSER[8] = {\n");
  PrintWeightArray(fp, weights->candidatePasser, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score CANDIDATE_EDGE_DISTANCE = ");
  PrintWeight(fp, &weights->candidateEdgeDistance);

  fprintf(fp, "\nconst Score PASSED_PAWN[8] = {\n");
  PrintWeightArray(fp, weights->passedPawn, 8, 4);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN_EDGE_DISTANCE = ");
  PrintWeight(fp, &weights->passedPawnEdgeDistance);

  fprintf(fp, "\nconst Score PASSED_PAWN_KING_PROXIMITY = ");
  PrintWeight(fp, &weights->passedPawnKingProximity);

  fprintf(fp, "\nconst Score PASSED_PAWN_ADVANCE_DEFENDED[5] = {\n");
  PrintWeightArray(fp, weights->passedPawnAdvance, 5, 5);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PASSED_PAWN_ENEMY_SLIDER_BEHIND = ");
  PrintWeight(fp, &weights->passedPawnEnemySliderBehind);

  fprintf(fp, "\nconst Score PASSED_PAWN_SQ_RULE = ");
  PrintWeight(fp, &weights->passedPawnSqRule);

  fprintf(fp, "\nconst Score PASSED_PAWN_UNSUPPORTED = ");
  PrintWeight(fp, &weights->passedPawnUnsupported);

  fprintf(fp, "\nconst Score KNIGHT_THREATS[6] = {");
  PrintWeightArray(fp, weights->knightThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BISHOP_THREATS[6] = {");
  PrintWeightArray(fp, weights->bishopThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score ROOK_THREATS[6] = {");
  PrintWeightArray(fp, weights->rookThreats, 6, 0);
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score KING_THREAT = ");
  PrintWeight(fp, &weights->kingThreat);

  fprintf(fp, "\nconst Score PAWN_THREAT = ");
  PrintWeight(fp, &weights->pawnThreat);

  fprintf(fp, "\nconst Score PAWN_PUSH_THREAT = ");
  PrintWeight(fp, &weights->pawnPushThreat);

  fprintf(fp, "\nconst Score PAWN_PUSH_THREAT_PINNED = ");
  PrintWeight(fp, &weights->pawnPushThreatPinned);

  fprintf(fp, "\nconst Score HANGING_THREAT = ");
  PrintWeight(fp, &weights->hangingThreat);

  fprintf(fp, "\nconst Score KNIGHT_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->knightCheckQueen);

  fprintf(fp, "\nconst Score BISHOP_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->bishopCheckQueen);

  fprintf(fp, "\nconst Score ROOK_CHECK_QUEEN = ");
  PrintWeight(fp, &weights->rookCheckQueen);

  fprintf(fp, "\nconst Score SPACE = %d;\n", (int)round(weights->space.mg.value));

  fprintf(fp, "\nconst Score IMBALANCE[5][5] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->imbalance[0], 1, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[1], 2, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[2], 3, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[3], 4, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->imbalance[4], 5, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_SHELTER[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->pawnShelter[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnShelter[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score PAWN_STORM[4][8] = {\n");
  fprintf(fp, " {");
  PrintWeightArray(fp, weights->pawnStorm[0], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[1], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[2], 8, 0);
  fprintf(fp, "},\n {");
  PrintWeightArray(fp, weights->pawnStorm[3], 8, 0);
  fprintf(fp, "},\n");
  fprintf(fp, "};\n");

  fprintf(fp, "\nconst Score BLOCKED_PAWN_STORM[8] = {\n");
  PrintWeightArray(fp, weights->blockedPawnStorm, 8, 0);
  fprintf(fp, "\n};\n");

  fprintf(fp, "\nconst Score CAN_CASTLE = ");
  PrintWeight(fp, &weights->castlingRights);

  fprintf(fp, "\nconst Score COMPLEXITY_PAWNS = %d;\n", (int)round(weights->complexPawns.mg.value));

  fprintf(fp, "\nconst Score COMPLEXITY_PAWNS_OFFSET = %d;\n", (int)round(weights->complexPawnsOffset.mg.value));

  fprintf(fp, "\nconst Score COMPLEXITY_OFFSET = %d;\n", (int)round(weights->complexOffset.mg.value));

  fprintf(fp, "\nconst Score KS_ATTACKER_WEIGHTS[5] = {\n");
  fprintf(fp, " 0, %d, %d, %d, %d", (int)round(weights->ksAttackerWeight[1].mg.value),
          (int)round(weights->ksAttackerWeight[2].mg.value), (int)round(weights->ksAttackerWeight[3].mg.value),
          (int)round(weights->ksAttackerWeight[4].mg.value));
  fprintf(fp, "\n};\n");

  fprintf(fp, "\nconst Score KS_WEAK_SQS = %d;\n", (int)round(weights->ksWeakSqs.mg.value));

  fprintf(fp, "\nconst Score KS_PINNED = %d;\n", (int)round(weights->ksPinned.mg.value));

  fprintf(fp, "\nconst Score KS_KNIGHT_CHECK = %d;\n", (int)round(weights->ksKnightCheck.mg.value));

  fprintf(fp, "\nconst Score KS_BISHOP_CHECK = %d;\n", (int)round(weights->ksBishopCheck.mg.value));

  fprintf(fp, "\nconst Score KS_ROOK_CHECK = %d;\n", (int)round(weights->ksRookCheck.mg.value));

  fprintf(fp, "\nconst Score KS_QUEEN_CHECK = %d;\n", (int)round(weights->ksQueenCheck.mg.value));

  fprintf(fp, "\nconst Score KS_UNSAFE_CHECK = %d;\n", (int)round(weights->ksUnsafeCheck.mg.value));

  fprintf(fp, "\nconst Score KS_ENEMY_QUEEN = %d;\n", (int)round(weights->ksEnemyQueen.mg.value));

  fprintf(fp, "\nconst Score KS_KNIGHT_DEFENSE = %d;\n", (int)round(weights->ksKnightDefense.mg.value));

  fprintf(fp, "\nconst Score TEMPO = %d;\n", (int)round(weights->tempo.mg.value));

  fprintf(fp, "\n");
  fclose(fp);
}

void PrintWeightArray(FILE* fp, Weight* weights, int n, int wrap) {
  for (int i = 0; i < n; i++) {
    if (wrap)
      fprintf(fp, " S(%4d,%4d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));
    else
      fprintf(fp, " S(%d, %d),", (int)round(weights[i].mg.value), (int)round(weights[i].eg.value));

    if (wrap && i % wrap == wrap - 1)
      fprintf(fp, "\n");
  }
}

void PrintWeight(FILE* fp, Weight* weight) {
  fprintf(fp, "S(%d, %d);\n", (int)round(weight->mg.value), (int)round(weight->eg.value));
}

#endif