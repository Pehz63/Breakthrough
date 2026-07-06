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

// ---- Value features, version 2 (sparse piece-square, white-centric) ----
// One binary input per (color, square) plus a side-to-move input. A move changes
// only 2-3 inputs (source off, destination on, captured square off), which is what
// makes a model over this layout incrementally updatable during search (see the
// g_mlAcc accumulator in ml_eval.h). Index layout:
//   0..63    White piece on square (x + SIZE*y)
//   64..127  Black piece on square (x + SIZE*y)
//   128      side to move (+1 White, -1 Black), applied at read time, never
//            stored in the accumulator
#define MLV2_FEATURES 129
#define MLV2_STM      128
inline int mlSqW(int x, int y) { return x + SIZE*y; }
inline int mlSqB(int x, int y) { return SIZE*SIZE + x + SIZE*y; }
const char* mlValueFeatureNameV2(int i);
// Fill out[0..MLV2_FEATURES-1] from the current board (single scan, no move
// generation). turnColor sets the side-to-move input.
void        mlExtractValueFeaturesV2(int turnColor, float* out);

// ---- Move features (move -> float vector, side-relative) ----
#define MLM_FEATURES 9
int          mlMoveFeatureCount();
int          mlMoveFeatureVersion();
const char*  mlMoveFeatureName(int i);
// Fill out[0..MLM_FEATURES-1] describing move m for `side` on the current board.
void         mlExtractMoveFeatures(const Move& m, int side, float* out);
