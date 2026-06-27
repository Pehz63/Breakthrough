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

    float feats[MLV_FEATURES];
    mlExtractValueFeatures(turnColor, feats);
    float out = m->forward(feats, MLV_FEATURES);
    float scaled = std::tanh(out) * m->outputScale();   // bounded in (-out_scale, out_scale)
    int v = (int)lround(scaled);
    if (v >  ML_EVAL_CAP) v =  ML_EVAL_CAP;
    if (v < -ML_EVAL_CAP) v = -ML_EVAL_CAP;
    return v;
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
