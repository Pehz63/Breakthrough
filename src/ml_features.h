#pragma once
#include "globals.h"

// ============================================================
// ML board reading: move generation + feature extraction
// ============================================================
// This module turns the live board[][] into ML-friendly forms used by every
// learned part of the system:
//   * Move generation (legal move lists), shared by explorers, policies, and the
//     trainer so they all see the same candidate set.
//   * Value features  (board  -> float[]) for value models / LearnedValue.
//   * Move  features  (move   -> float[]) for policy models / the move-rater.
// Both feature sets are versioned; the version is written into model files so a
// model can refuse a board it was not trained against. Names tables drive the
// auto-exported documentation.

// A single one-step move from (sx,sy) to (dx,dy); capture = the destination held
// an enemy piece when the move was generated.
struct Move {
    int sx, sy, dx, dy;
    bool capture;
};

// Upper bound on legal moves in any position (<=16 pieces * 3 moves).
#define ML_MAX_MOVES 64

// Generate all legal moves for `side` (White / Black) into out[] (capacity
// ML_MAX_MOVES), capture-first to match the search move ordering. Returns count.
int generateMoves(int side, Move* out);

// ---- Value features (board -> float vector, white-centric) ----
#define MLV_FEATURES 30
int          mlValueFeatureCount();
int          mlValueFeatureVersion();
const char*  mlValueFeatureName(int i);
// Fill out[0..MLV_FEATURES-1] from the current board. turnColor sets the
// side-to-move feature; the rest are white-centric (positive favors White).
void         mlExtractValueFeatures(int turnColor, float* out);

// ---- Move features (move -> float vector, side-relative) ----
#define MLM_FEATURES 9
int          mlMoveFeatureCount();
int          mlMoveFeatureVersion();
const char*  mlMoveFeatureName(int i);
// Fill out[0..MLM_FEATURES-1] describing move m for `side` on the current board.
void         mlExtractMoveFeatures(const Move& m, int side, float* out);
