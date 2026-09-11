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

// Linear schedule shared by the lr and explore decays below: interpolates from
// `start` at gameIndex 0 to `floor` at gameIndex >= decayGames, holding at
// `floor` after. decayGames <= 0 means "off": always returns `start`. Mirrors
// ml_train.cpp's --gen-random-floor/--gen-random-decay-plies shape (same
// t = min(1, progress/length) interpolation), indexed by GAMES elapsed in the
// run rather than plies elapsed in one game, since this decays a hyperparameter
// over training progress, not an opening's diversity window.
double tdLeafScheduledValue(double start, double floor, int decayGames, int gameIndex);

// ============================================================
// Replication-study switches (plans/replication-study-plan-1-brass-lectern.md)
// ============================================================
// Each published technique is one switch on this trainer, so two arms differ
// in exactly one setting. The helpers below are the pure parts, exposed so the
// closed forms can be asserted in tests/test_ml.cpp.

// Longest possible Breakthrough game in plies on the 8x8 board. Every move
// advances a piece one row, and reaching the far row wins, so a White piece
// starting on row 0 makes at most 6 non-winning moves and one on row 1 at
// most 5: 8*6 + 8*5 = 88 non-winning moves per side. The side that wins makes
// one more, so a game has at most 88 + 88 + 1 = 177 plies. Captures only
// remove moves. This is P in the additive-depth reward.
#define TD_MAX_GAME_PLIES 177

// Terminal target in [0,1], white-centric. outcome: 1 White won, 2 Black won,
// 0 draw. With depthReward off this is win/loss (1, 0, 0.5). With it on it is
// Cohen-Solal's additive depth reward (JMLR 2026, Section 6.2.2) mapped into a
// probability: l = P - plies + 1 clamped to [1, P], and z = 0.5 + 0.5*l/P for
// a White win, 0.5 - 0.5*l/P for a Black win. plies = 1 gives l = P, which is
// exactly win/loss, the closed form tests/test_ml.cpp asserts.
double tdTerminalTarget(int outcome, int plies, bool depthReward, int maxPlies);

// Inverse of the learned evaluator's score tail. A learned leaf is scored
// round(tanh(out) * outScale), and the trainer's value is sigmoid(out), so a
// white-centric search score s maps back to the target probability
// sigmoid(atanh(s / outScale)) = sqrt(1+u) / (sqrt(1+u) + sqrt(1-u)), u = s/outScale.
// A proven result (the search's near-win sentinels) maps to exactly 1 or 0.
// Used by the RootStrap and TreeStrap backups (Veness et al. 2009).
double tdScoreToProb(int whiteScore, float outScale);

// Veness's one-sided update toward a transposition-table bound, as dL/dlogit
// for cross-entropy. v = the model's win probability at the node, q = the
// bound's probability, flag = TT_EXACT, TT_LOWER or TT_UPPER. An EXACT entry
// always pulls v to q. A LOWER bound (true value >= q) only pushes v up when
// v < q. An UPPER bound (true value <= q) only pushes v down when v > q.
double tdOneSidedGrad(double v, double q, int flag);

// Cohen-Solal's ordinal action distribution (JMLR 2026, Section 7): the
// probability of playing the i-th best of n moves (i = 0 is the best),
//   P(c_i) = (e + (1 - e) / (n - i)) * (1 - sum_{j<i} P(c_j)).
// e = 1 always plays the best move, e = 0 is uniform.
double tdOrdinalProb(double e, int n, int i);
// Draw a rank from that distribution with rand(), exactly as their Algorithm
// 14 does it: walk down the ranking, stopping at rank i with probability
// e + (1 - e) / (n - i).
int    tdOrdinalPick(double e, int n);

struct TDLeafConfig {
    string outPath;                 // model base name; final model at outPath + ".txt"
    string boardFile;               // starting position
    string initModel;               // "" = random init; else start from this model's weights
    int    games;                   // self-play games to run
    int    depth;                   // search depth of the self-play agent
    unsigned long long nodeBudget;  // per-move node cap (0 = off)
    double timeBudgetMs;            // per-move wall-clock cap in ms (0 = off)
    int    iterMinRemain;           // rem=N: decline a deepening iteration unless N%
                                    // of the node budget is unspent (0 = off)
    bool   retainBudget;            // bank a move's unspent budget into the same side's
                                    // next move, so the cap is per GAME not per move
    double lambda;                  // eligibility decay in [0,1]
    double lr;                      // SGD step size (schedule start value if lrDecayGames > 0)
    double lrFloor;                 // lr decays to this by lrDecayGames games (default = lr, i.e. off)
    int    lrDecayGames;            // 0 = off (constant lr); > 0 = linear decay over this many games
    double l2;                      // weight decay
    unsigned seed;
    int    openPlies;               // uniform-random opening plies per side (diversity)
    double explore;                 // per-move chance of a random move during play (schedule start if exploreDecayGames > 0)
    double exploreFloor;            // explore decays to this by exploreDecayGames games (default 0)
    int    exploreDecayGames;       // 0 = off (constant explore); > 0 = linear decay over this many games
    int    batchGames;              // 1 = strictly online; N > 1 = apply updates every N games

    // ---- Replication-study switches. The defaults are the baseline pipeline,
    // and each is written into provenance only when it is not the default, so
    // a checkpoint trained before they existed keeps the recipe it always had.
    //
    // backup: which positions are trained, toward which target.
    //   "td-leaf"     (default) PV leaf of each root, toward the lambda-return
    //                 over the game's PV leaves (Baxter et al. 1999).
    //   "td-directed" the root itself, toward the lambda-return over the
    //                 game's roots. The search still picks every move.
    //   "rootstrap"   the root, toward the root's own search score. No lambda,
    //                 no game outcome (Veness et al. 2009, RootStrap(ab)).
    //   "treestrap"   the root toward its search score, plus every node the
    //                 search stored with remaining depth >= treeMinDepth,
    //                 toward that node's bound with the one-sided update
    //                 (Veness et al. 2009, TreeStrap(ab)). Linear models only.
    string backup;
    int    treeMinDepth;            // treestrap: Veness's d_min (default 1)
    // terminal: "winloss" (default) or "depth" (additive depth reward, see
    // tdTerminalTarget). Only the lambda-return backups read the outcome.
    string terminal;
    // augment: "" (default) or "mirror", which also trains every trained
    // position's left-right mirror toward the same target. v2 features only.
    string augment;
    // exploreDist: "eps" (default) plays a uniformly random move with
    // probability `explore` and does not train on it. "ordinal" draws every
    // searched move from Cohen-Solal's ordinal distribution over the root's
    // moves, ranked by the search, and trains on it. e follows the linear
    // schedule ordinalStart -> ordinalEnd over ordinalGames games (their
    // annealing is e = t/T, so 0 -> 1 over the whole run).
    string exploreDist;
    double ordinalStart;
    double ordinalEnd;
    int    ordinalGames;
    string modelType;               // "linear" | "mlp"
    std::vector<int> mlpHidden;     // hidden widths when modelType == "mlp"
    int    featureVersion;          // 1 (dense, 30) | 2 (sparse piece-square, 129). Scratch-init only --
                                     // ignored (with a warning) when initModel is set, since the loaded
                                     // model's own feature version governs then.
    int    ckptEvery;               // checkpoint every N games (0 = off)
    // Game-count ladder: checkpoint after exactly these game counts, written to
    // outPath + "_gN.txt". This is how the study learns the game count instead of
    // assuming one -- each rung is rated as its own agent, so the learning curve
    // is an output rather than an input. Cheaper and better controlled than
    // separate runs per size: one run's rungs share a training trajectory, so
    // they differ ONLY in how long it ran. (For the same reason rungs of one run
    // are NOT independent replicates -- only distinct seeds are.)
    std::vector<int> ckptAt;
    int    reportEvery;             // progress line every N games (0 = off)

    // Wall-clock rung ladder (src/train_budget.h). Training compute is
    // normalized on wall clock across regimes, so the rungs a comparison needs
    // are cumulative SECONDS, not game counts: a fast recipe and a slow one at
    // "1000 games" have not spent the same compute, and that is the whole thing
    // the ladder exists to equalize. wallCkptAt writes outPath + "_t<sec>.txt"
    // at each mark, wallStopSec ends the run. Both empty/0 = off, and the
    // game-count ladder above is unaffected, so a run can carry either or both.
    std::vector<double> wallCkptAt;
    double wallStopSec;
    // Continue a previous run's ladder instead of starting a new one: loads the
    // model AND its recorded spend, so a run stopped at 8h and resumed reaches
    // 16h cumulative rather than 8h twice. Takes precedence over initModel.
    string resumeFrom;
};

// Fill a config with the defaults the CLI uses (so tests and callers agree).
TDLeafConfig tdLeafDefaults();

// Run the regime. Returns 0 on success. Writes outPath + ".txt" (and
// outPath + "_ckptN.txt" when ckptEvery > 0, outPath + "_gN.txt" per game-count
// rung, outPath + "_t<sec>.txt" per wall-clock rung), each with a `teacher=`
// provenance line recording the full recipe so two differently-configured runs
// never collide.
//
// Provenance is composed and attached IMMEDIATELY BEFORE EACH SAVE, from the
// spend that save actually represents, never once up front from the requested
// configuration. Writing it up front is how every checkpoint of a laddered run
// came to claim the last rung's game count: slot169's header says games=4000
// and it trained on 1,500 (plans/budget-parity-plan-1-steady-meridian.md, P5).
int trainTDLeaf(const TDLeafConfig& cfg);

// Mean PV depth the last trainTDLeaf run actually reached, the same number its
// summary prints. Exposed so a test can assert the PV walk is finding entries at
// all: this regime's whole premise is that the gradient is taken at the leaf of
// the principal variation, so a walk that stops at its first step silently turns
// TD-Leaf into TD on the root's successor, with no error and no crash. That is
// what a bare (unsalted) transposition probe did between 2026-08-27 and
// 2026-09-09. Set to 0.0 when a run captured no leaf.
extern double g_tdLastMeanPV;

// What the last trainTDLeaf run actually did, for the instrument checks in
// tests/test_ml.cpp and the per-arm diagnostics the replication study reports.
struct TDLeafRunStats {
    long long games;              // games this process played
    long long gamePlies;          // plies over those games, openings included
    long long searches;           // searched (non-random, undecided) root moves
    unsigned long long nodes;     // search nodes this process spent
    long long updates;            // gradient applications, mirrored ones included
    long long mirrorUpdates;      // of which mirrored
    long long treeNodes;          // treestrap: table entries accepted by the walk
    long long treeUpdated;        // treestrap: of which moved (a bound was violated)
    long long ordinalMoves;       // ordinal: moves drawn from the distribution
    long long ordinalNonBest;     // ordinal: of which not the search's choice
    long long ordinalRanked;      // ordinal: root moves ranked from a table entry
    long long ordinalUnranked;    // ordinal: root moves ranked by static eval instead
    int       wWins, bWins, draws;
};
extern TDLeafRunStats g_tdLastRun;
