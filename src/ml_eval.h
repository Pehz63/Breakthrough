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

// Distribution accessor: mean and SD of the current board's White advantage,
// in Elo, from the DistModel in `slot`. Returns false (outputs untouched) when
// the slot holds no dist model. A decided position (nearWinCheck) returns true
// with muElo = +-99999 and sdElo = 0. Analysis/GUI surface; search never
// reads the SD.
bool mlValueScoreDist(int turnColor, int slot, double& muElo, double& sdElo);

// ---- Incremental ML value path (sparse piece-square models, feature v2) ----
// mlIncrementalBegin: if the model in `slot` is a value head over the v2 sparse
// piece-square features, seed the accumulator and return true; returns false (and
// leaves the globals cleared) for any other model, so callers fall back to the
// full-scan path. Two accumulator modes, disambiguated by g_mlAccDim:
//   - Linear mu head (g_mlAccDim == 0): the scalar g_mlAcc = bias + occupied
//     piece-square weights, read as tanh(acc + skip*chipDiff + stmW*turn)*scale.
//   - MLP mu head (g_mlAccDim == H > 0): the NNUE-style vector g_mlAccVec holds the
//     H first-hidden pre-activations (B[0] + occupied columns); mlLeafScore adds the
//     side-to-move column, applies ReLU, and runs the remaining layers via
//     MLPModel::forwardFromHidden.
// A DistModel is unwrapped to its mu head and a ResidualModel to its inner first,
// so a dist/residual wrapper over either a linear or MLP head is handled.
// mlIncrementalEnd clears all of it.
bool mlIncrementalBegin(int slot);
void mlIncrementalEnd();
int  mlLeafScore(int turnColor);

// Add / subtract input `idx`'s layer-0 weight column into the MLP vector
// accumulator (g_mlAccVec, length g_mlAccDim). Called by the make/unmake hooks in
// moves.cpp for the MLP path; inline so the hot loop stays a contiguous AXPY.
inline void mlAccAddColumn(int idx) {
    const float* col = g_mlL0ByInput + (size_t)idx * g_mlAccDim;
    for (int j = 0; j < g_mlAccDim; j++) g_mlAccVec[j] += col[j];
}
inline void mlAccSubColumn(int idx) {
    const float* col = g_mlL0ByInput + (size_t)idx * g_mlAccDim;
    for (int j = 0; j < g_mlAccDim; j++) g_mlAccVec[j] -= col[j];
}

// Score each move in moves[0..n) for `side` with the policy model in `slot`,
// writing raw scores to scoresOut[] (may be null). Returns the index of the
// best-scoring move, or -1 if the slot has no usable policy model.
int mlRateMoves(int side, int slot, const Move* moves, int n, float* scoresOut);
