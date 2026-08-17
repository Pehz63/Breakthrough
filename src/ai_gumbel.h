#pragma once
#include "globals.h"
#include "ml_features.h"   // Move, ML_MAX_MOVES

// ============================================================
// Gumbel MCTS (Danihelka, Pohlen, Rowland, Hessel, Ozair, Silver, van
// Hasselt, "Policy improvement by planning with Gumbel", ICLR 2022)
// ============================================================
// A move-tree explorer, registered in g_explorers[] as "GumbelMCTS". Unlike
// AlphaBeta/Greedy it needs a POLICY as well as a value, so the evaluator
// slot (threaded through params[0], the existing LearnedValue convention --
// see agentChooseMove in src/agents.cpp) must hold a JointModel
// (src/ml_model.h): the search calls mlValueScore(side, slot) for leaf
// values (works unchanged, since JointModel::forward() delegates to its
// value head) and JointModel::policyForward() directly for per-move prior
// logits (bypassing mlRateMoves, which is single-head-only).
//
// Algorithm shape:
//   - Root: Gumbel-top-k sampling over the legal moves' prior logits selects
//     up to kGumbelM candidate root actions (replaces Dirichlet-noise
//     exploration).
//   - Root: Sequential Halving spends the simulation budget across those
//     candidates in halving rounds. Every survivor gets an equal share of
//     simulations in a round; at the end of a round survivors are ranked by
//     logit + sigma(completedQ) and the bottom half is cut, narrowing to
//     exactly one action.
//   - Every NON-ROOT node, on every simulation's downward walk: a
//     deterministic action-selection rule (gumbelSelectAction) whose
//     empirical visit fractions converge to softmax(logits + sigma(completedQ)).
//   - completedQ for an unvisited child substitutes the PARENT's own
//     current mean backed-up value (a documented Pass-1 simplification of
//     the paper's fuller v_mix, which would blend the network value with
//     visited children weighted by visit fraction).
//
// budget (ExplorerDef::fn's parameter) is the TOTAL simulation count for the
// move, reused the same way AlphaBeta reuses it as search depth.
//
// No perf optimization in this slice (heap-allocated tree nodes, one per
// simulation, no transposition sharing): correctness first, matching this
// project's Pass-1 discipline (Docs/model-training-playbook.md).

// ---- Pure, unit-testable core math (no board state) ----

// sigma(q) = (c_visit + maxVisitCount) * c_scale * q -- the transform that
// turns a completed Q-value (mover-relative, in [-1,1]) into a logit-scale
// bonus comparable to a prior logit. Paper defaults: c_visit=50, c_scale=1.0
// (see kGumbelCVisit/kGumbelCScale in ai_gumbel.cpp).
double gumbelSigma(double q, int maxVisitCount, double cVisit, double cScale);

// Deterministic action-selection rule. Among `n` actions with prior logits
// `logits[i]`, mover-relative completed Q-values `completedQ[i]` (in
// [-1,1]), and current visit counts `visitCounts[i]`, returns the index
// maximizing
//   softmax(logits[i] + sigma(completedQ[i]))[i]  -  visitCounts[i] / (1 + sum(visitCounts))
// A greedy rule whose empirical visit distribution provably converges to
// that target softmax (Danihelka et al. 2022, Algorithm 1's non-root action
// selection). Used at every non-root node during a simulation's descent.
// `n` must be <= ML_MAX_MOVES. Returns -1 if n <= 0.
int gumbelSelectAction(const double* logits, const double* completedQ,
                       const int* visitCounts, int n, double cVisit, double cScale);

// Gumbel-top-k: draws one Gumbel(0,1) variate per action (g_a = -log(-log(u)),
// u ~ Uniform(0,1) via rand()), ranks by g_a + logits[a], and writes the
// indices of the top min(k, n) actions (by that ranked key, descending) into
// `out` (capacity >= min(k, n)). Returns the count written (0 if n <= 0 or
// k <= 0). Draws from rand() on every call -- an agent wearing this explorer
// is never deterministic (see rankAgentIsDeterministic, src/ranking.cpp).
int gumbelTopK(const double* logits, int n, int k, int* out);

// Sequential Halving's round schedule for `m` starting candidates and a
// total simulation budget `budget`. Returns the number of rounds
// (ceil(log2(m)), minimum 1) and fills simsPerRound[0..rounds-1] with the
// number of ADDITIONAL simulations each surviving candidate receives in
// that round (every survivor in a round gets the same count; the round-end
// halving cut, not this schedule, is what narrows the candidate set).
// `simsPerRound` capacity must be >= the returned round count.
int gumbelHalvingRounds(int m, int budget, int* simsPerRound);

// ---- Search entry point ----

// Root-level diagnostics: the ingredients of the Gumbel-improved policy
// target (softmax(logits + sigma(completedQ)) over the root's legal moves,
// using the search's FINAL visit counts/completedQ), useful to a future
// self-play training regime. Unused by the plain registered explorer
// wrapper below.
struct GumbelRootInfo {
    int    moveCount;
    double logits[ML_MAX_MOVES];
    double completedQ[ML_MAX_MOVES];      // mover-relative, in [-1,1]
    int    visitCounts[ML_MAX_MOVES];
    double rootValue;                     // the joint model's own root value estimate, white-centric [-1,1]
    double searchValue;                   // the root's mean backed-up value over every simulation run
                                           // (white-centric [-1,1]) -- the search's OWN improved estimate,
                                           // as opposed to rootValue's single pre-search network read. Equal
                                           // to rootValue when no simulations ran (the immediate-win shortcut,
                                           // or a forced single legal move).
};

// Runs the search for `side` using the joint model in `slot`, plays the
// chosen move on the live board, and returns the victor code (matches
// ExplorerDef::fn's contract). `simBudget` is the total simulation count
// (>= 1; values < 1 are treated as 1). `info`, if non-null, receives the
// root search's final statistics.
int gumbelSearch(int side, int slot, int simBudget, GumbelRootInfo* info = nullptr);

// The Gumbel-improved policy target: softmax(logits[i] + sigma(completedQ[i],
// maxVisitCount, cVisit, cScale)) over info.moveCount legal root moves, using
// the search's FINAL logits/completedQ/visitCounts (reuses this file's own
// internal cVisit/cScale constants, so a caller never has to duplicate or
// re-derive the search's own math). Writes into `out` (capacity >=
// info.moveCount), which sums to 1. A future self-play training regime's
// policy-head target; unused by the plain registered explorer.
void gumbelImprovedPolicy(const GumbelRootInfo& info, double* out);

// Thin wrapper matching ExplorerDef::fn's (side, evaluator, params, budget)
// signature (params[0] = model slot, the LearnedValue convention), registered
// as "GumbelMCTS" in src/explorers.cpp.
int gumbelExplore(int side, int evaluator, const int* params, int budget);
