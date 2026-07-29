#pragma once
#include "globals.h"
#include <vector>

// ============================================================
// TD-Leaf(lambda): online, bootstrapped value training
// ============================================================
// Every other value regime in this project is supervised and offline: a position
// gets a fixed label (game outcome, teacher eval, or the position-oracle's fitted
// Elo gap) and the model is fit to those labels afterwards. TD-Leaf's target for a
// position is instead the model's OWN evaluation of a LATER position, backed up
// through the search, and the weights move while the games are being played.
//
// The "Leaf" part is what adapts temporal-difference learning to a search engine.
// A position's usable value is not its static eval, it is the minimax value the
// search returns -- and that value IS the static eval of the leaf ending the
// principal variation. So the gradient for position s_t is taken at leaf(s_t),
// not at s_t itself (Baxter/Tridgell/Weaver, KnightCap).
//
// PV leaves are recovered by probing the transposition table along the played
// line, NOT by re-searching at decreasing depth. Re-searching costs up to `depth`
// times the node budget per move (~6x at the d6/nb200k head); TT probes are
// essentially free. The cost is that an always-replace table can lose entries, so
// the walk can stop short -- trainTDLeaf reports the mean PV depth actually
// reached so the instrument can be checked rather than assumed. Nothing in
// ai_minimax.cpp is modified: a g_collectPV branch in the hot recursion would
// shift us/node for every rated agent and invalidate the ranking instrument.

// Per-position gradient of the cross-entropy loss against the lambda-return, for
// one game's principal-variation leaf win-probabilities.
//
//   p[t] = sigmoid(model logit at leaf(s_t)), white-centric, t = 0 .. N-1
//   z    = final white-centric outcome in [0,1]  (1 White won, 0 Black won, 0.5 draw)
//
// Fills gOut[0..N-1] with dL/d(output logit) at each leaf, ready to hand to
// Model::gradStep. Derivation: TD errors d_j = p_{j+1} - p_j with p_N := z,
// eligibility sum e_t = d_t + lambda*e_{t+1} (e_N = 0), and gOut_t = -e_t.
//
// Two closed forms fall out, and both are asserted in tests/test_ml.cpp:
//   lambda = 1 -> e_t telescopes to (z - p_t), so gOut_t = p_t - z. TD-Leaf at
//                 lambda=1 is EXACTLY outcome-supervised training on PV leaves.
//   lambda = 0 -> gOut_t = p_t - p_{t+1}, pure one-step TD.
void tdLeafGradients(const std::vector<double>& p, double z, double lambda,
                     std::vector<double>& gOut);

struct TDLeafConfig {
    string outPath;                 // model base name; final model at outPath + ".txt"
    string boardFile;               // starting position
    string initModel;               // "" = random init; else start from this model's weights
    int    games;                   // self-play games to run
    int    depth;                   // search depth of the self-play agent
    unsigned long long nodeBudget;  // per-move node cap (0 = off)
    double lambda;                  // eligibility decay in [0,1]
    double lr;                      // SGD step size
    double l2;                      // weight decay
    unsigned seed;
    int    openPlies;               // uniform-random opening plies per side (diversity)
    double explore;                 // per-move chance of a random move during play
    int    batchGames;              // 1 = strictly online; N > 1 = apply updates every N games
    string modelType;               // "linear" | "mlp"
    std::vector<int> mlpHidden;     // hidden widths when modelType == "mlp"
    int    ckptEvery;               // checkpoint every N games (0 = off)
    int    reportEvery;             // progress line every N games (0 = off)
};

// Fill a config with the defaults the CLI uses (so tests and callers agree).
TDLeafConfig tdLeafDefaults();

// Run the regime. Returns 0 on success. Writes outPath + ".txt" (and
// outPath + "_ckptN.txt" when ckptEvery > 0), with a `teacher=` provenance line
// recording the full recipe so two differently-configured runs never collide.
int trainTDLeaf(const TDLeafConfig& cfg);
