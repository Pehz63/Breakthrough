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

#define ML_SLOTS 8

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

// Score each move in moves[0..n) for `side` with the policy model in `slot`,
// writing raw scores to scoresOut[] (may be null). Returns the index of the
// best-scoring move, or -1 if the slot has no usable policy model.
int mlRateMoves(int side, int slot, const Move* moves, int n, float* scoresOut);
