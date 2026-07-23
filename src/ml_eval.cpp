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

bool mlValueScoreDist(int turnColor, int slot, double& muElo, double& sdElo) {
    Model* m = mlGetModel(slot);
    DistModel* dm = m ? dynamic_cast<DistModel*>(m) : nullptr;
    if (!dm) return false;
    int nw = nearWinCheck(turnColor);
    if (nw) { muElo = (nw > 0) ? 99999.0 : -99999.0; sdElo = 0.0; return true; }
    float mu, sd;
    if (dm->featureVersion() == 2) {
        float feats[MLV2_FEATURES];
        mlExtractValueFeaturesV2(turnColor, feats);
        dm->forwardDist(feats, MLV2_FEATURES, mu, sd);
    } else {
        float feats[MLV_FEATURES];
        mlExtractValueFeatures(turnColor, feats);
        dm->forwardDist(feats, MLV_FEATURES, mu, sd);
    }
    muElo = mu * ELO_PER_LOGIT;
    sdElo = sd * ELO_PER_LOGIT;
    return true;
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
static float g_mlStmW = 0.0f;       // weight of the side-to-move input (linear path)
static float g_mlOutScale = 1.0f;   // active model's output scale
static float g_mlSkipW = 0.0f;      // frozen chip-count skip weight (ResidualModel); 0 = none

// MLP-path state: the leaf tail model and the buffers g_mlAccVec / g_mlL0ByInput
// point into. Sized once per search by mlIncrementalBegin, never on the hot path.
static const int ML_ACC_MAX = 512;              // fixed leaf pre-activation buffer cap (>= any first-hidden width)
static const MLPModel* g_mlMlp = nullptr;       // active MLP mu head (leaf tail via forwardFromHidden)
static std::vector<double> g_mlAccVecBuf;       // owns g_mlAccVec
static std::vector<float>  g_mlL0ByInputBuf;    // owns g_mlL0ByInput (input-major layer-0 transpose)

bool mlIncrementalBegin(int slot) {
    mlIncrementalEnd();
    Model* m = mlGetModel(slot);
    if (!m || m->head() != HEAD_VALUE || m->featureVersion() != 2) return false;

    // Unwrap a distributional wrapper to its mean head first (search only reads the
    // mu logit; the sigma head plays no role at the leaf), then a residual chip-skip
    // wrapper (the skip rides along as skipW * g_chipDiff added at the leaf, using the
    // already-maintained g_chipDiff). The remaining core is the head the accumulator
    // tracks: a LinearModel keeps the scalar path, an MLPModel the vector path.
    Model* core = m;
    float skip = 0.0f;
    if (DistModel* dm = dynamic_cast<DistModel*>(core)) core = dm->muHead;
    if (ResidualModel* rm = dynamic_cast<ResidualModel*>(core)) { skip = rm->skipW; core = rm->inner; }

    if (LinearModel* lm = dynamic_cast<LinearModel*>(core)) {
        if (lm->n != MLV2_FEATURES) return false;
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
        g_mlAccDim = 0;              // scalar/linear mode
        g_mlIncremental = true;
        return true;
    }

    // NNUE-style vector accumulator over the MLP mu head's first hidden layer.
    // Requires a genuine hidden layer (sizes.size() >= 3, so sizes[1] is hidden).
    MLPModel* mp = dynamic_cast<MLPModel*>(core);
    if (!mp || mp->n != MLV2_FEATURES || (int)mp->sizes.size() < 3) return false;
    int H = mp->sizes[1];
    if (H > ML_ACC_MAX) return false;   // guard the fixed leaf buffer; current heads are 128/256

    // Store layer-0 INPUT-major (transpose of W[0], which is output-major
    // W[0][j*in + idx]) so a touched input's column is contiguous for the make/unmake
    // AXPY. Column MLV2_STM is the side-to-move weights, applied at leaf read.
    int in0 = mp->sizes[0];   // == MLV2_FEATURES
    const std::vector<float>& W0 = mp->W[0];
    const std::vector<float>& B0 = mp->B[0];
    g_mlL0ByInputBuf.assign((size_t)MLV2_FEATURES * H, 0.0f);
    for (int idx = 0; idx < MLV2_FEATURES; idx++)
        for (int j = 0; j < H; j++)
            g_mlL0ByInputBuf[(size_t)idx * H + j] = W0[(size_t)j * in0 + idx];
    g_mlL0ByInput = g_mlL0ByInputBuf.data();

    // Seed: acc[j] = B[0][j] + sum over occupied squares of that square's column
    // (side-to-move column excluded, applied at leaf read).
    g_mlAccVecBuf.assign(H, 0.0);
    for (int j = 0; j < H; j++) g_mlAccVecBuf[j] = B0[j];
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            char c = board[x][y];
            int idx = -1;
            if (c == WHITE)      idx = mlSqW(x, y);
            else if (c == BLACK) idx = mlSqB(x, y);
            if (idx < 0) continue;
            const float* col = &g_mlL0ByInputBuf[(size_t)idx * H];
            for (int j = 0; j < H; j++) g_mlAccVecBuf[j] += col[j];
        }
    g_mlAccVec   = g_mlAccVecBuf.data();
    g_mlAccDim   = H;
    g_mlMlp      = mp;
    g_mlOutScale = mp->outScale;
    g_mlSkipW    = skip;
    g_mlIncremental = true;
    return true;
}

void mlIncrementalEnd() {
    g_mlIncremental = false;
    g_mlWeights = nullptr;
    g_mlStmW = 0.0f;
    g_mlOutScale = 1.0f;
    g_mlSkipW = 0.0f;
    g_mlAccDim = 0;
    g_mlAccVec = nullptr;
    g_mlL0ByInput = nullptr;
    g_mlMlp = nullptr;
}

int mlLeafScore(int turnColor) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    if (g_mlAccDim > 0) {
        // MLP path: form the first-hidden pre-activations (accumulator + the
        // side-to-move column applied here), then ReLU + the remaining layers.
        float pre1[ML_ACC_MAX];
        const float* stmCol = g_mlL0ByInput + (size_t)MLV2_STM * g_mlAccDim;
        double s = (turnColor == White) ? 1.0 : -1.0;
        for (int j = 0; j < g_mlAccDim; j++)
            pre1[j] = (float)(g_mlAccVec[j] + s * stmCol[j]);
        double out = g_mlMlp->forwardFromHidden(pre1) + (double)g_mlSkipW * g_chipDiff;
        return mlSquashToEval(out, g_mlOutScale);
    }
    // Linear path: acc = inner linear logit; add the frozen chip skip (skipW *
    // white-minus-black count, already in g_chipDiff) and the side-to-move term.
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
