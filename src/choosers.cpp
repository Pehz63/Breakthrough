#include "choosers.h"
#include "ai_random.h"
#include "ml_features.h"   // generateMoves, Move
#include "ml_eval.h"       // mlRateMoves
#include "moves.h"

// ============================================================
// HEURISTIC CHOOSERS (wrap the existing random family)
// ============================================================
static int chooseUniform(int side, int /*slot*/, int /*param*/) {
    return (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
}
static int chooseTiered(int side, int /*slot*/, int /*param*/) {
    return (side == White) ? tieredRandomMoveWhite() : tieredRandomMoveBlack();
}
static int chooseSmart(int side, int /*slot*/, int param) {
    if (param < 1) param = 1;
    return (side == White) ? smartRandomMoveWhite(param) : smartRandomMoveBlack(param);
}

// ============================================================
// LEARNED POLICY (move-rater, no search)
// ============================================================
// Score every legal move with the policy model in `slot` and play the best one.
// Falls back to TieredRandom if the slot has no usable policy model.
static int chooseLearnedPolicy(int side, int slot, int /*param*/) {
    Move mv[ML_MAX_MOVES];
    int n = generateMoves(side, mv);
    if (n == 0) return (side == White) ? BlackWin : WhiteWin;

    int idx = mlRateMoves(side, slot, mv, n, nullptr);
    if (idx < 0) return chooseTiered(side, slot, 0);   // no model -> heuristic fallback

    return (side == White)
        ? playMoveWhite(mv[idx].sx, mv[idx].sy, mv[idx].dx)
        : playMoveBlack(mv[idx].sx, mv[idx].sy, mv[idx].dx);
}

// ============================================================
// REGISTRY
// ============================================================
const ChooserDef g_choosers[] = {
    { "UniformRandom", "Every legal move equally likely.",                         chooseUniform       },
    { "TieredRandom",  "Prefer wins, then captures, then normal moves.",           chooseTiered        },
    { "SmartRandom",   "TieredRandom restricted to the furthest-N pieces (param).", chooseSmart         },
    { "LearnedPolicy", "Argmax of a learned move-rater (policy model); no search.", chooseLearnedPolicy },
};
const int g_chooserCount = (int)(sizeof(g_choosers) / sizeof(g_choosers[0]));
