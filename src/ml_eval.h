#pragma once
#include "globals.h"
#include "ml_features.h"
#include "ml_model.h"

// ============================================================
// ML inference glue: model slots + scoring
// ============================================================
// Models live in a small fixed set of "slots" so that, in one process, White and
// Black (or many tournament agents) can each use a different model at once. The
// LearnedValue evaluator and the LearnedPolicy chooser both reference a slot.

// Slots 0/1/2 have fixed file conventions (lin_value/lin_policy/pst_value, see
// ranking.cpp's slotFile()); slots 3.. are generic sweep/experiment slots
// (models/sweep/slot<N>.txt) so a large hyperparameter sweep can hold many
// independently-trained candidates rated together in one process, instead of
// serially swapping one shared file. The trainer's internal quick-score-vs-random
// check (see ml_train.cpp) always uses the LAST slot as scratch.
#define ML_SLOTS 128

// Best-effort: load the default trained models into their conventional slots
// (models/lin_value.txt -> slot 0, models/lin_policy.txt -> slot 1) if present, so
// the engine binaries can use LearnedValue / LearnedPolicy without extra wiring.
// Missing files are simply skipped.
void   mlAutoLoadDefaultSlots();

// Slot management. mlSetModel takes ownership of m (frees any previous occupant).
bool   mlLoadSlot(int slot, const string& path);   // load a file into a slot
void   mlSetModel(int slot, Model* m);             // inject an in-memory model
Model* mlGetModel(int slot);
void   mlClearSlots();

// White-centric value score of the current board using the model in `slot`
// (positive favors White). Applies the shared near-win shortcut first, then maps
// the model output through tanh*out_scale and clamps it strictly inside the
// (BlackWin, WhiteWin) sentinels. Falls back to Classic defaults if the slot is
// empty, so callers always get a usable number.
int mlValueScore(int turnColor, int slot);

// ---- Incremental ML value path (sparse piece-square models, feature v2) ----
// mlIncrementalBegin: if the model in `slot` is a value head over the v2 sparse
// piece-square features, seed g_mlAcc (bias + occupied piece-square weights, one
// board scan), latch g_mlWeights, and return true. Returns false (and leaves the
// globals cleared) for any other model, so callers fall back to the full-scan
// path. mlIncrementalEnd clears the state. mlLeafScore reads the maintained
// accumulator at a search leaf: near-win shortcut, then
// tanh(acc + stmW*turn) * out_scale, clamped exactly like mlValueScore.
bool mlIncrementalBegin(int slot);
void mlIncrementalEnd();
int  mlLeafScore(int turnColor);

// Score each move in moves[0..n) for `side` with the policy model in `slot`,
// writing raw scores to scoresOut[] (may be null). Returns the index of the
// best-scoring move, or -1 if the slot has no usable policy model.
int mlRateMoves(int side, int slot, const Move* moves, int n, float* scoresOut);
