#include "ml_eval.h"
#include "ai_eval.h"
#include <cmath>

// ============================================================
// MODEL SLOTS
// ============================================================
static Model* g_mlModels[ML_SLOTS] = { nullptr };

void mlSetModel(int slot, Model* m) {
    if (slot < 0 || slot >= ML_SLOTS) { delete m; return; }
    if (g_mlModels[slot] && g_mlModels[slot] != m) delete g_mlModels[slot];
    g_mlModels[slot] = m;
}
bool mlLoadSlot(int slot, const string& path) {
    if (slot < 0 || slot >= ML_SLOTS) return false;
    Model* m = loadModel(path);
    if (!m) return false;
    mlSetModel(slot, m);
    return true;
}
Model* mlGetModel(int slot) {
    if (slot < 0 || slot >= ML_SLOTS) return nullptr;
    return g_mlModels[slot];
}
void mlClearSlots() {
    for (int i = 0; i < ML_SLOTS; i++) { delete g_mlModels[i]; g_mlModels[i] = nullptr; }
}

void mlAutoLoadDefaultSlots() {
    mlLoadSlot(0, "models/lin_value.txt");    // value model -> slot 0 (LearnedValue default)
    mlLoadSlot(1, "models/lin_policy.txt");   // policy model -> slot 1 (LearnedPolicy default)
}

// ============================================================
// SCORING
// ============================================================
// Keep learned scores strictly inside the forced-win band so they never collide
// with the +/-WIN sentinels handled by nearWinCheck / canWin*.
static const int ML_EVAL_CAP = INT_MAX - 4096;   // < WhiteWin-1024, > BlackWin+1024 when negated

// Shared tail of every learned value score: tanh squash, scale, round, clamp.
// Used by both the full-scan path (mlValueScore) and the incremental leaf read
// (mlLeafScore) so the two can never diverge in how a raw output becomes an eval.
static int mlSquashToEval(double out, float scale) {
    double scaled = std::tanh(out) * scale;   // bounded in (-out_scale, out_scale)
    int v = (int)lround(scaled);
    if (v >  ML_EVAL_CAP) v =  ML_EVAL_CAP;
    if (v < -ML_EVAL_CAP) v = -ML_EVAL_CAP;
    return v;
}

int mlValueScore(int turnColor, int slot) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;

    Model* m = mlGetModel(slot);
    if (!m || m->head() != HEAD_VALUE) {
        // Fallback so a misconfigured slot still yields a meaningful number.
        int defs[MAX_EVAL_PARAMS] = { 0 };
        for (int i = 0; i < g_evaluators[0].paramCount; i++) defs[i] = g_evaluators[0].params[i].def;
        return evaluateBoard(turnColor, 0, defs);
    }

    // Dispatch the extractor on the model's feature version: v1 = dense
    // aggregates, v2 = sparse piece-square. This full-scan path serves the GUI
    // readout, the Greedy explorer, and the reference side of the incremental
    // equivalence tests.
    if (m->featureVersion() == 2) {
        float feats[MLV2_FEATURES];
        mlExtractValueFeaturesV2(turnColor, feats);
        return mlSquashToEval(m->forward(feats, MLV2_FEATURES), m->outputScale());
    }
    float feats[MLV_FEATURES];
    mlExtractValueFeatures(turnColor, feats);
    return mlSquashToEval(m->forward(feats, MLV_FEATURES), m->outputScale());
}

// ============================================================
// INCREMENTAL VALUE PATH (feature v2 accumulator)
// ============================================================
// The scalar analog of NNUE's accumulator: for a linear model over the sparse
// piece-square inputs, the whole forward pass short of the squash is
//   bias + sum(weights of occupied piece-squares) + stmW * sideToMove.
// The board-dependent part is kept in g_mlAcc, updated by 2-3 weight
// adds/subtracts per make/unmake (see moves.cpp). The side-to-move term is
// applied at read time so unmake never has to know whose turn it was.
static float g_mlStmW = 0.0f;       // weight of the side-to-move input
static float g_mlOutScale = 1.0f;   // active model's output scale
static float g_mlSkipW = 0.0f;      // frozen chip-count skip weight (ResidualModel); 0 = none

bool mlIncrementalBegin(int slot) {
    mlIncrementalEnd();
    Model* m = mlGetModel(slot);
    if (!m || m->head() != HEAD_VALUE || m->featureVersion() != 2) return false;

    // Unwrap a residual chip-skip wrapper: its inner model is what the accumulator
    // tracks, and the skip rides along as skipW * g_chipDiff added at the leaf
    // (g_chipDiff is already maintained by make/unmake). An MLP inner has no linear
    // accumulator, so we fall through to false and the full-scan leaf path.
    Model* core = m;
    float skip = 0.0f;
    if (ResidualModel* rm = dynamic_cast<ResidualModel*>(m)) { skip = rm->skipW; core = rm->inner; }
    LinearModel* lm = dynamic_cast<LinearModel*>(core);
    if (!lm || lm->n != MLV2_FEATURES) return false;

    g_mlWeights  = lm->w.data();
    g_mlStmW     = lm->w[MLV2_STM];
    g_mlOutScale = lm->outScale;
    g_mlSkipW    = skip;

    double acc = lm->bias;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            char c = board[x][y];
            if (c == WHITE)      acc += g_mlWeights[mlSqW(x, y)];
            else if (c == BLACK) acc += g_mlWeights[mlSqB(x, y)];
        }
    g_mlAcc = acc;
    g_mlIncremental = true;
    return true;
}

void mlIncrementalEnd() {
    g_mlIncremental = false;
    g_mlWeights = nullptr;
    g_mlStmW = 0.0f;
    g_mlOutScale = 1.0f;
    g_mlSkipW = 0.0f;
}

int mlLeafScore(int turnColor) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    // acc = inner linear logit; add the frozen chip skip (skipW * white-minus-black
    // count, already maintained in g_chipDiff) and the side-to-move term.
    double out = g_mlAcc + (double)g_mlSkipW * g_chipDiff + g_mlStmW * ((turnColor == White) ? 1.0 : -1.0);
    return mlSquashToEval(out, g_mlOutScale);
}

int mlRateMoves(int side, int slot, const Move* moves, int n, float* scoresOut) {
    Model* m = mlGetModel(slot);
    if (!m || m->head() != HEAD_POLICY || n <= 0) return -1;

    int best = -1;
    float bestScore = 0.0f;
    for (int i = 0; i < n; i++) {
        float feats[MLM_FEATURES];
        mlExtractMoveFeatures(moves[i], side, feats);
        float s = m->forward(feats, MLM_FEATURES);
        if (scoresOut) scoresOut[i] = s;
        if (best < 0 || s > bestScore) { best = i; bestScore = s; }
    }
    return best;
}
