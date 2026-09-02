#pragma once
#include "globals.h"
#include "ml_features.h"   // MLV2_FEATURES, MLM_FEATURES, ML_MAX_MOVES
#include <vector>

// ============================================================
// Gumbel-Zero: self-play trainer for the Gumbel MCTS joint model
// ============================================================
// Pass 1 (sanity) only -- see plans/gumbel-zero-plan-1-*.md. Generates games
// with GumbelMCTS (src/ai_gumbel.h) using ONE evolving JointModel that plays
// both sides, and trains both heads against targets the search itself
// produces at every ply (no game-outcome label, no separate exploration
// knob -- Gumbel-top-k already resamples every move):
//
//   value target  = (GumbelRootInfo::searchValue + 1) / 2, in [0,1] -- the
//                   root's own mean backed-up value over the ply's
//                   simulations, i.e. the search's IMPROVED estimate, not
//                   the network's single pre-search read (rootValue) and
//                   not the eventual game outcome. Trained via the same
//                   sigmoid cross-entropy every other value regime in this
//                   project already uses (Model::trainStep).
//   policy target = gumbelImprovedPolicy(info): softmax(logits + sigma(completedQ))
//                   over the ply's legal root moves, using the search's
//                   FINAL stats. Trained via softmax cross-entropy against
//                   the policy head's OWN current group softmax
//                   (gumbelPolicyGradients below).
//
// "Strictly online" (a step per new ply, no waiting for a batch of whole
// games) and "replay buffer" (that step trains against a sampled mix of
// recent plies, not only the one just generated) are two independent axes
// that compose rather than conflict -- see GumbelZeroReplayBuffer.
//
// Initialization is from-scratch only in this pass: no --init flag, since the
// developer confirmed from-scratch during Slice 1's planning and Pass 1 is
// deliberately the smallest reviewable unit. The architecture is selectable
// (GumbelZeroConfig::modelType/mlpHidden/convChannels, mirroring ml_train.cpp's
// selfplay-supervised --model-type/--mlp-hidden). "linear" and "mlp" apply to
// BOTH heads -- value and policy are separate Model instances of the same
// architecture, not a shared one. "conv" applies to the VALUE head only: its
// board features (v2, 129) are a real 8x8/2-plane image a conv tower can read,
// but the policy head's move features (9 handcrafted, non-spatial numbers) are
// not spatial, so ConvModel itself is never usable for the policy head (see
// ml_model.h's ConvModel doc comment) -- a conv run's policy head therefore
// defaults to the same linear scorer it always used. GumbelZeroConfig::policyMlp
// (default false, --policy-mlp) is an INDEPENDENT knob layered on top: it gives
// a conv run's policy head an MLPModel instead (over the same 9 move features,
// the same architecture "mlp" mode already uses for its own policy head -- only
// the value-head/policy-head COUPLING is new, not a new model type), reusing
// GumbelZeroConfig::mlpHidden for its hidden-layer widths. This decouples "what
// architecture does the value head use" from "does the policy head get hidden-
// layer capacity", which the plain modelType selector cannot express since it
// picks one architecture for both heads (or, for conv, implicitly forces the
// policy head linear). A "linear" head is zero-initialized (Slice 1's own
// smoke-test construction, so existing linear checkpoints reproduce exactly).
// "mlp"/"conv" heads, and any policyMlp policy head, call their model's
// initRandom() to break weight symmetry (zero-init hidden layers can never
// learn, ml_model.h), so linear runs and mlp/conv/policyMlp runs draw from
// different points in the rand() stream even at the same seed.

// ---- Policy gradient core (pure, unit-testable) ----

// Softmax cross-entropy gradient for one ply's move group: given the policy
// head's CURRENT logits over `n` candidate moves and a target distribution
// (e.g. gumbelImprovedPolicy's output, src/ai_gumbel.h), fills
// gOut[i] = softmax(logits)[i] - target[i] = dL/d(logits[i]). Sums to ~0
// across the group by construction; ~0 everywhere when target already equals
// softmax(logits). `n` must be <= ML_MAX_MOVES.
void gumbelPolicyGradients(const double* logits, const double* target, int n, double* gOut);

// ---- Replay buffer ----

// Everything needed to retrain one self-play ply's targets against
// WHATEVER the model's weights are at sample time (not the weights when the
// ply was generated) -- the policy side needs a fresh group softmax under
// current weights, so a record stores raw features/targets, never a
// precomputed gradient.
struct GumbelZeroRecord {
    float  boardFeatures[MLV2_FEATURES];
    double valueTarget;                     // in [0,1]
    int    moveCount;
    float  moveFeatures[ML_MAX_MOVES][MLM_FEATURES];
    double policyTarget[ML_MAX_MOVES];      // sums to 1 over [0, moveCount)
};

// Fixed-capacity ring buffer of self-play ply records: push evicts the
// oldest record once full; sample draws up to n DISTINCT records (partial
// Fisher-Yates over the live records, so one minibatch never repeats a ply).
// Pure logic, no board/model dependency, so it is independently unit-tested.
class GumbelZeroReplayBuffer {
public:
    explicit GumbelZeroReplayBuffer(int capacity);
    void push(const GumbelZeroRecord& r);
    int  size() const { return (int)buf_.size(); }
    int  capacity() const { return cap_; }
    // Fills out with pointers to min(n, size()) DISTINCT live records, drawn
    // via rand(). Pointers are valid only until the next push().
    void sample(int n, std::vector<const GumbelZeroRecord*>& out) const;

private:
    int cap_;
    std::vector<GumbelZeroRecord> buf_;
    int next_ = 0;   // ring write cursor once full
};

// ---- Config ----

struct GumbelZeroConfig {
    string outPath;
    string boardFile;
    int    games;             // self-play games this run
    int    simBudget;         // gaz sims/move (both sides -- one model self-plays)
    unsigned seed;
    int    openPlies;         // uniform-random opening plies per side (diversity)
    string modelType;         // "linear" (default), "mlp" (both heads), or "conv" (value head only)
    std::vector<int> mlpHidden;  // MLP hidden-layer widths (modelType=="mlp"); also the conv FC head's
                                  // hidden layers (modelType=="conv"); default {32} if empty for mlp,
                                  // no forced default for conv (empty = direct linear read-out)
    std::vector<int> convChannels;  // conv layer output-channel counts (modelType=="conv"); default
                                     // {16,16} if empty (a 2->16 projection + one 16-wide residual block)
    bool   policyMlp = false;  // give the policy head an MLPModel (mlpHidden widths) even when
                                // modelType=="conv" (whose policy head is otherwise linear); no effect
                                // when modelType=="mlp" (already MLP) or "linear" (see doc comment above)
    double lr;
    double l2;
    int    replayCapacity;
    int    replayWarmup;      // don't start training until the buffer holds this many records
    int    batchSize;         // records sampled per training step
    int    ckptEvery;         // checkpoint every N games (0 = off)
    std::vector<int> ckptAt;  // game-count ladder, same mechanism as TD-Leaf's --ckpt-at
    int    reportEvery;

    // Wall-clock rung ladder and resume, identical in meaning to TD-Leaf's
    // (src/ml_tdleaf.h, src/train_budget.h): cumulative second marks written as
    // outPath + "_t<sec>.txt", a hard cumulative stop, and a checkpoint to
    // continue the same ladder from. Empty/0/"" = off.
    std::vector<double> wallCkptAt;
    double wallStopSec;
    string resumeFrom;
};

// Fill a config with the defaults the CLI uses (so tests and callers agree).
GumbelZeroConfig gumbelZeroDefaults();

// Run the regime. Returns 0 on success. Writes outPath + ".txt" (and
// outPath + "_gN.txt" for each ckptAt rung, outPath + "_t<sec>.txt" for each
// wall-clock rung), with a `teacher=` provenance line recording the full recipe
// (`gumbelzero(...)`, recognized by src/ranking.cpp's regime classifier as
// "gumbel_self").
//
// Provenance is composed immediately before each save from the spend that save
// actually represents, never once up front from the requested configuration --
// see trainTDLeaf's header comment for the defect that rule exists to prevent.
int trainGumbelZero(const GumbelZeroConfig& cfg);
