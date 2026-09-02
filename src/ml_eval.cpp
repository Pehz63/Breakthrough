#include "ml_eval.h"
#include "ai_eval.h"
#include <cmath>
#include <unordered_map>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

// ============================================================
// MODEL SLOTS
// ============================================================
// Backed by a sparse map rather than a fixed Model*[ML_SLOTS] array: the slot
// number is still the runtime handle every call site threads through (see
// ml_eval.h), but nothing about storage depends on ML_SLOTS being an upper
// bound any more, so ML_SLOTS is validation-only now (a sanity ceiling on a
// parsed roster/test slot number), not an allocation size.
static std::unordered_map<int, Model*> g_mlModels;

void mlSetModel(int slot, Model* m) {
    if (slot < 0 || slot >= ML_SLOTS) { delete m; return; }
    auto it = g_mlModels.find(slot);
    if (it != g_mlModels.end() && it->second != m) delete it->second;
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
    auto it = g_mlModels.find(slot);
    return it == g_mlModels.end() ? nullptr : it->second;
}
void mlClearSlots() {
    for (auto& kv : g_mlModels) delete kv.second;
    g_mlModels.clear();
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

// ---- Fast, BIT-EXACT leaf tail -------------------------------------------
//
// Every learned value score ends in the same four steps: tanh squash, scale,
// round, clamp. On a wall-clock leaderboard that tail is per-leaf currency, and
// it is the one thing the integer heuristic evaluators never pay. Measured on
// this machine (2026-09-01, 20M calls against a 0.73 ns/call load-and-add
// baseline, two runs agreeing to 0.15 ns): std::tanh 10.7 ns, the whole tail
// 17.3 ns, so lround plus the clamp is the other 6.6 ns -- lround is a libm
// call on MSVC, not an instruction.
//
// **Exactness is a hard requirement, not a nicety.** Every learned agent's
// canonical id carries `learned(...)@1`, and an eval that returns a different
// integer for any input is a behavior change that would have to bump that
// module version and re-identify the whole learned roster. So both halves below
// return the SAME int as `lround(std::tanh(out) * scale)` for every input whose
// product fits a 32-bit long, and tests/test_train_budget.cpp's siblings in
// tests/test_ml.cpp assert that over a dense sweep plus the edge cases.
//
// The fit-a-long qualifier is not a loophole for the leaf: `out_scale` is 900
// for every value head this project trains (1.0 for a policy head), so the
// product never leaves +-900. Past |tanh(out) * scale| = LONG_MAX the SHIPPED
// reference is itself undefined -- lround returns a long and MSVC's long is 32
// bits -- so there is no defined behavior to be exact against. The fast path
// stays well defined there by rounding and clamping in long long.
//
// Two independent pieces:
//
//  1. `roundHalfAway` replaces lround with no approximation at all. Truncating
//     to an integer and subtracting is exact for any |a| < 2^52, so the
//     fractional part is exact and the `>= 0.5` test reproduces lround's
//     round-half-away-from-zero rule directly. (The obvious `(long)(s + 0.5)`
//     shortcut does NOT: at s = 0.49999999999999994 the addition itself rounds
//     up to 1.0 and the result is 1 where lround gives 0.)
//
//  2. `fastTanhAbs` is a linear interpolation over a 1024-interval table of
//     tanh on [0, 8], with a proven error bound, followed by a check that the
//     bound cannot straddle a rounding boundary. When it can (about 2% of
//     inputs at scale 900), the code falls through to std::tanh. The bound:
//     linear interpolation of a twice-differentiable function has error at most
//     h^2/8 * max|f''|, with h = 1/128 and max|tanh''| = 4/(3*sqrt(3)) =
//     0.7698, giving 5.9e-6; float storage adds ~6e-8; past x = 8 the value is
//     pinned at tanh(8) with error at most 1 - tanh(8) = 2.3e-7. TANH_EPS =
//     1e-5 covers all three with room to spare.
static const double TANH_XMAX = 8.0;
static const int    TANH_STEPS = 1024;                       // intervals over [0, XMAX]
static const double TANH_INV_H = (double)TANH_STEPS / TANH_XMAX;
static const double TANH_EPS  = 1e-5;                        // bound on |approx - tanh|

namespace {
struct TanhTable {
    float v[TANH_STEPS + 1];
    double atMax;
    TanhTable() {
        for (int i = 0; i <= TANH_STEPS; i++)
            v[i] = (float)std::tanh((double)i / TANH_INV_H);
        atMax = std::tanh(TANH_XMAX);
    }
};
// Namespace scope, not a function-local static: a local static would pay a
// thread-safe-initialisation guard check on every leaf.
const TanhTable g_tanhTable;
}

// |tanh| for a >= 0, within TANH_EPS. Never used on its own -- always paired
// with the boundary check in mlSquashToEval.
static inline double fastTanhAbs(double a) {
    double u = a * TANH_INV_H;
    if (u >= (double)TANH_STEPS) return g_tanhTable.atMax;
    int i = (int)u;
    double frac = u - (double)i;
    double lo = (double)g_tanhTable.v[i];
    return lo + frac * ((double)g_tanhTable.v[i + 1] - lo);
}

// Bit-exact replacement for lround over the range a leaf eval can produce.
// Returns long long rather than lround's long: MSVC's long is 32 bits, and the
// clamp below is applied in the wider type so an out-of-range product saturates
// instead of wrapping.
static inline long long roundHalfAway(double s) {
    bool neg = (s < 0.0);
    double a = neg ? -s : s;
    long long n = (long long)a;            // truncation toward zero
    double frac = a - (double)n;           // exact: n is the integer part of a
    if (frac >= 0.5) n++;
    return neg ? -n : n;
}

// Clamp into the eval band. Taken in long long so a product larger than a 32-bit
// long saturates rather than wrapping.
static inline int clampToEvalCap(long long e) {
    if (e >  (long long)ML_EVAL_CAP) return  ML_EVAL_CAP;
    if (e < -(long long)ML_EVAL_CAP) return -ML_EVAL_CAP;
    return (int)e;
}

// Shared tail of every learned value score: tanh squash, scale, round, clamp.
// Used by both the full-scan path (mlValueScore) and the incremental leaf read
// (mlLeafScore) so the two can never diverge in how a raw output becomes an eval.
static int mlSquashToEval(double out, float scale) {
    const double absScale = (scale < 0.0f) ? -(double)scale : (double)scale;
    const double absOut   = (out < 0.0) ? -out : out;

    // Magnitude first, sign applied at the end: tanh is odd, so |tanh(out)*scale|
    // = |tanh|out|| * |scale| exactly, and doing the boundary check on a
    // non-negative number keeps the truncation and the 0.5 test simple.
    double mag = fastTanhAbs(absOut) * absScale;

    // Guard the truncation below and skip a scale so large the clamp decides
    // the answer anyway.
    if (mag < 4503599627370496.0) {            // 2^52
        double slack = TANH_EPS * absScale;
        double frac = mag - (double)(long long)mag;   // exact
        // The whole error band sits inside one rounding bucket, so the
        // approximation's rounding IS the true rounding.
        if (frac - 0.5 > slack || 0.5 - frac > slack) {
            bool neg = (out < 0.0) != (scale < 0.0f);
            return clampToEvalCap(roundHalfAway(neg ? -mag : mag));
        }
    }
    // Too close to a rounding boundary (or out of the fast path's range):
    // settle it with the transcendental.
    return clampToEvalCap(roundHalfAway(std::tanh(out) * (double)scale));
}

// Reference implementation, kept so tests can assert the fast path above is
// bit-identical to the tail this project shipped before it.
int mlSquashToEvalReference(double out, float scale) {
    double scaled = std::tanh(out) * scale;   // bounded in (-out_scale, out_scale)
    int v = (int)lround(scaled);
    if (v >  ML_EVAL_CAP) v =  ML_EVAL_CAP;
    if (v < -ML_EVAL_CAP) v = -ML_EVAL_CAP;
    return v;
}

int mlSquashToEvalFast(double out, float scale) { return mlSquashToEval(out, scale); }

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

int mlValueScoreRisk(int turnColor, int slot, int riskTenths) {
    if (riskTenths == 0) return mlValueScore(turnColor, slot);
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;

    Model* m = mlGetModel(slot);
    DistModel* dm = m ? dynamic_cast<DistModel*>(m) : nullptr;
    if (!dm) return mlValueScore(turnColor, slot);   // no sigma head to blend in

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
    double out = (double)mu + ((double)riskTenths / 10.0) * (double)sd;
    return mlSquashToEval(out, dm->outputScale());
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
        int H = g_mlAccDim, j = 0;
#if defined(__AVX2__)
        // Vectorize the O(H) accumulator read: pre1 = (float)(accVec + s*stmCol),
        // reading the double accumulator 8 units at a time (4 doubles per register).
        const __m256d vs = _mm256_set1_pd(s);
        for (; j + 8 <= H; j += 8) {
            __m256d a0 = _mm256_loadu_pd(g_mlAccVec + j);
            __m256d a1 = _mm256_loadu_pd(g_mlAccVec + j + 4);
            a0 = _mm256_fmadd_pd(vs, _mm256_cvtps_pd(_mm_loadu_ps(stmCol + j)),     a0);
            a1 = _mm256_fmadd_pd(vs, _mm256_cvtps_pd(_mm_loadu_ps(stmCol + j + 4)), a1);
            _mm_storeu_ps(pre1 + j,     _mm256_cvtpd_ps(a0));
            _mm_storeu_ps(pre1 + j + 4, _mm256_cvtpd_ps(a1));
        }
#endif
        for (; j < H; j++)
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
