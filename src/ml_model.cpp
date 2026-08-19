#include "ml_model.h"
#include <cmath>
#include <ostream>
#include <sstream>
#include <iomanip>

#if defined(__AVX2__)
#include <immintrin.h>
// Horizontal sum of an 8-lane float vector. Used by the vectorized leaf tail.
static inline float hsum256(__m256 v) {
    __m128 lo = _mm256_castps256_ps128(v);
    __m128 hi = _mm256_extractf128_ps(v, 1);
    lo = _mm_add_ps(lo, hi);                 // 4 partial sums
    lo = _mm_add_ps(lo, _mm_movehl_ps(lo, lo));
    lo = _mm_add_ss(lo, _mm_shuffle_ps(lo, lo, 1));
    return _mm_cvtss_f32(lo);
}
#endif

static inline float sigmoidf(float z) {
    if (z >= 0) { float e = expf(-z); return 1.0f / (1.0f + e); }
    float e = expf(z); return e / (1.0f + e);
}

static inline double sigmoidD(double z) {
    if (z >= 0) { double e = exp(-z); return 1.0 / (1.0 + e); }
    double e = exp(z); return e / (1.0 + e);
}

// ============================================================
// PROBIT-APPROXIMATION POINT LOSS (distributional head math)
// ============================================================
// See ml_model.h for the model. Gradients, with u = kappa*(mu+d) and
// sigma2 = exp(2s): dL/du = p - y (BCE through sigmoid), dkappa/ds =
// -(pi/8)*sigma2*kappa^3, so du/ds = -u*(pi/8)*sigma2*kappa^2, giving
// gMu = (p-y)*kappa and gS = -(p-y)*u*(pi/8)*sigma2*kappa^2. A surprising
// outcome in a confidently-called position raises s (sigma grows), a
// confirmed call shrinks it.
double probitPoint(double mu, double s, double d, double v, double y, ProbitGrad& out) {
    const double C = 0.39269908169872414;   // pi/8
    double sigma2 = exp(2.0 * s);
    double kappa  = 1.0 / sqrt(1.0 + C * (sigma2 + v));
    double u      = kappa * (mu + d);
    double p      = sigmoidD(u);
    const double eps = 1e-12;
    double loss   = -(y * log(p + eps) + (1.0 - y) * log(1.0 - p + eps));
    out.p     = p;
    out.kappa = kappa;
    out.gMu   = (p - y) * kappa;
    out.gS    = -(p - y) * u * C * sigma2 * kappa * kappa;
    return loss;
}

// ============================================================
// FEATURE-VECTOR MATERIAL READOUT (chip-count skip term)
// ============================================================
float matDiffFromFeatures(const float* x, int featVer) {
    if (featVer == 2) {
        // v2 sparse piece-square: white squares 0..63, black squares 64..127.
        float w = 0.0f, b = 0.0f;
        int half = SIZE * SIZE;
        for (int i = 0; i < half; i++)      w += x[i];
        for (int i = half; i < 2*half; i++) b += x[i];
        return w - b;
    }
    // v1 dense: feature 0 is (wTotal - bTotal) / 16.
    return x[0] * 16.0f;
}

// ============================================================
// LINEAR MODEL
// ============================================================
LinearModel::LinearModel(int head, int featVersion, int featCount, float scale)
    : headType(head), featVer(featVersion), n(featCount), outScale(scale), bias(0.0f) {
    w.assign(featCount, 0.0f);
}

float LinearModel::forward(const float* x, int m) const {
    int lim = (m < n) ? m : n;
    float s = bias;
    for (int i = 0; i < lim; i++) s += w[i] * x[i];
    return s;
}

float LinearModel::sgdLogisticStep(const float* x, int m, float target, float lr, float l2, float offset) {
    int lim = (m < n) ? m : n;
    // Compute the logit inline (not via the virtual forward) so a subclass that
    // overrides forward with an extra term is never double-counted here.
    float z = offset + bias;
    for (int i = 0; i < lim; i++) z += w[i] * x[i];
    float p = sigmoidf(z);
    float eps = 1e-7f;
    float loss = -(target * logf(p + eps) + (1.0f - target) * logf(1.0f - p + eps));
    float g = (p - target);                 // dL/dz for logistic
    for (int i = 0; i < lim; i++) w[i] -= lr * (g * x[i] + l2 * w[i]);
    bias -= lr * g;
    return loss;
}

void LinearModel::gradStep(const float* x, int m, float gOut, float lr, float l2) {
    int lim = (m < n) ? m : n;
    for (int i = 0; i < lim; i++) w[i] -= lr * (gOut * x[i] + l2 * w[i]);
    bias -= lr * gOut;                       // bias not decayed
}

void LinearModel::writeWeights(std::ostream& f) const {
    f << "bias=" << bias << "\n";
    for (int i = 0; i < n; i++) f << "w" << i << "=" << w[i] << "\n";
}

bool LinearModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=linear\n";
    f << "head=" << (headType == HEAD_POLICY ? "policy" : "value") << "\n";
    f << "feature_version=" << featVer << "\n";
    f << "feature_count=" << n << "\n";
    f << "out_scale=" << outScale << "\n";
    writeWeights(f);
    return true;
}

// ============================================================
// MLP MODEL
// ============================================================
MLPModel::MLPModel(int head, int featVersion, int featCount, float scale, const std::vector<int>& hidden)
    : headType(head), featVer(featVersion), n(featCount), outScale(scale) {
    sizes.push_back(featCount);
    for (size_t i = 0; i < hidden.size(); i++) if (hidden[i] > 0) sizes.push_back(hidden[i]);
    sizes.push_back(1);
    int L = (int)sizes.size() - 1;
    W.resize(L); B.resize(L);
    for (int k = 0; k < L; k++) {
        W[k].assign((size_t)sizes[k] * sizes[k+1], 0.0f);
        B[k].assign(sizes[k+1], 0.0f);
    }
    act.resize(L + 1); pre.resize(L + 1);
    for (int k = 0; k <= L; k++) { act[k].assign(sizes[k], 0.0f); pre[k].assign(sizes[k], 0.0f); }
}

void MLPModel::initRandom() {
    int L = (int)sizes.size() - 1;
    for (int k = 0; k < L; k++) {
        int in = sizes[k];
        float scale = (in > 0) ? (float)sqrt(1.0 / (double)in) : 1.0f;   // fan-in scaling
        for (size_t t = 0; t < W[k].size(); t++)
            W[k][t] = (float)(((double)rand() / RAND_MAX) * 2.0 - 1.0) * scale;
        for (size_t t = 0; t < B[k].size(); t++) B[k][t] = 0.0f;
    }
}

float MLPModel::computeForward(const float* x, int m) const {
    int L = (int)sizes.size() - 1;
    int in0 = sizes[0];
    int lim = (m < in0) ? m : in0;
    for (int i = 0; i < in0; i++) act[0][i] = (i < lim) ? x[i] : 0.0f;
    for (int k = 0; k < L; k++) {
        int in = sizes[k], out = sizes[k+1];
        const std::vector<float>& Wk = W[k];
        const std::vector<float>& Bk = B[k];
        const float* a = act[k].data();
        bool hidden = (k + 1 < L);
        for (int j = 0; j < out; j++) {
            const float* wrow = &Wk[(size_t)j * in];
            float z = Bk[j];
            for (int i = 0; i < in; i++) z += wrow[i] * a[i];
            pre[k+1][j] = z;
            act[k+1][j] = hidden ? (z > 0.0f ? z : 0.0f) : z;   // ReLU hidden, linear output
        }
    }
    return act[L][0];
}

float MLPModel::forward(const float* x, int m) const {
    return computeForward(x, m);
}

float MLPModel::forwardFromHidden(const float* pre1) const {
    int L = (int)sizes.size() - 1;
#if defined(__AVX2__)
    // AVX2 leaf: the dominant cost of a wide first layer is the O(H) read of the
    // accumulated pre-activations, not the tail MACs. The scalar path's nonzero
    // gather (below) is a data-dependent compaction the compiler cannot vectorize
    // and it dominates. Here instead: (1) ReLU pre1 -> act[1] with a vector max
    // (skip the dead pre[] scratch writes), then (2) run each remaining layer as a
    // DENSE AVX2 FMA matmul (no gather). Doing the ~90% zero MACs is cheaper than
    // the branchy gather because 8 MACs ride one FMA. NOT bit-identical to the
    // scalar path (SIMD reduction order + FMA contraction differ), but within the
    // leaf's existing float-vs-double tolerance -- the equivalence test bounds it.
    {
        const int H1 = sizes[1];
        float* a1 = act[1].data();
        const __m256 zero = _mm256_setzero_ps();
        int j = 0;
        for (; j + 8 <= H1; j += 8)
            _mm256_storeu_ps(a1 + j, _mm256_max_ps(zero, _mm256_loadu_ps(pre1 + j)));
        for (; j < H1; j++) a1[j] = (pre1[j] > 0.0f) ? pre1[j] : 0.0f;
    }
    for (int k = 1; k < L; k++) {
        int in = sizes[k], out = sizes[k+1];
        const float* Wk = W[k].data();
        const float* Bk = B[k].data();
        const float* a = act[k].data();
        float* ao = act[k+1].data();
        bool hidden = (k + 1 < L);
        // Tail cost is (dense) in*out/8 FMAs vs (sparse) in gather + nnz*out MACs.
        // Dense wins only for a narrow output layer (the NNUE-shaped head, out ~ 8);
        // for a wide output (e.g. 128) the ~90% zero MACs outweigh the gather, so
        // fall back to the sparse gather there. Crossover measured between 64 and 128.
        if (out <= 32) {
            for (int j = 0; j < out; j++) {
                const float* wrow = Wk + (size_t)j * in;
                __m256 acc = _mm256_setzero_ps();
                int i = 0;
                for (; i + 8 <= in; i += 8)
                    acc = _mm256_fmadd_ps(_mm256_loadu_ps(wrow + i), _mm256_loadu_ps(a + i), acc);
                float z = Bk[j] + hsum256(acc);
                for (; i < in; i++) z += wrow[i] * a[i];
                ao[j] = hidden ? (z > 0.0f ? z : 0.0f) : z;
            }
        } else {
            nzScratch.clear();
            for (int i = 0; i < in; i++) if (a[i] != 0.0f) nzScratch.push_back(i);
            int nnz = (int)nzScratch.size();
            const int* nz = nzScratch.data();
            for (int j = 0; j < out; j++) {
                const float* wrow = Wk + (size_t)j * in;
                float z = Bk[j];
                for (int t = 0; t < nnz; t++) { int i = nz[t]; z += wrow[i] * a[i]; }
                ao[j] = hidden ? (z > 0.0f ? z : 0.0f) : z;
            }
        }
    }
    return act[L][0];
#else
    // Seed the first hidden layer from the externally-computed pre-activations:
    // act[1] = ReLU(pre1). pre[1] mirrors it for scratch consistency with computeForward.
    for (int j = 0; j < sizes[1]; j++) {
        pre[1][j] = pre1[j];
        act[1][j] = (pre1[j] > 0.0f) ? pre1[j] : 0.0f;
    }
    // Run the remaining layers (k = 1..L-1): ReLU on hidden layers, linear output.
    // ReLU zeros most first-hidden units (~90% for the trained dist heads), and a
    // zero activation contributes exactly nothing to any downstream pre-activation,
    // so gather the layer's nonzero inputs once and sum only over those. This is
    // bit-identical to the dense loop (adding 0*w never changes a float sum) -- a
    // pure speed win when the layer input is sparse, which is exactly the case here
    // (the "skip dead/constant hidden units" idea, applied per leaf).
    for (int k = 1; k < L; k++) {
        int in = sizes[k], out = sizes[k+1];
        const std::vector<float>& Wk = W[k];
        const std::vector<float>& Bk = B[k];
        const float* a = act[k].data();
        bool hidden = (k + 1 < L);
        nzScratch.clear();
        for (int i = 0; i < in; i++) if (a[i] != 0.0f) nzScratch.push_back(i);
        int nnz = (int)nzScratch.size();
        const int* nz = nzScratch.data();
        for (int j = 0; j < out; j++) {
            const float* wrow = &Wk[(size_t)j * in];
            float z = Bk[j];
            for (int t = 0; t < nnz; t++) { int i = nz[t]; z += wrow[i] * a[i]; }
            pre[k+1][j] = z;
            act[k+1][j] = hidden ? (z > 0.0f ? z : 0.0f) : z;
        }
    }
    return act[L][0];
#endif
}

float MLPModel::trainStep(const float* x, int m, float target, float lr, float l2, float offset) {
    float out = computeForward(x, m);       // fills act[]/pre[]
    float z = out + offset;
    float p = sigmoidf(z);
    float eps = 1e-7f;
    float loss = -(target * logf(p + eps) + (1.0f - target) * logf(1.0f - p + eps));
    backprop(p - target, lr, l2);
    return loss;
}

void MLPModel::gradStep(const float* x, int m, float gOut, float lr, float l2) {
    computeForward(x, m);                   // fills act[]/pre[]
    backprop(gOut, lr, l2);
}

void MLPModel::backprop(float gOut, float lr, float l2) {
    int L = (int)sizes.size() - 1;
    // g holds dL/d(pre) for the current layer's OUTPUT units; start at the scalar output.
    std::vector<float> g(1, gOut);
    for (int k = L - 1; k >= 0; k--) {
        int in = sizes[k], out2 = sizes[k+1];
        std::vector<float>& Wk = W[k];
        std::vector<float>& Bk = B[k];
        const float* a = act[k].data();
        std::vector<float> gPrev;
        if (k > 0) gPrev.assign(in, 0.0f);
        for (int j = 0; j < out2; j++) {
            float gj = g[j];
            float* wrow = &Wk[(size_t)j * in];
            for (int i = 0; i < in; i++) {
                if (k > 0) gPrev[i] += gj * wrow[i];     // uses pre-update weight
                wrow[i] -= lr * (gj * a[i] + l2 * wrow[i]);
            }
            Bk[j] -= lr * gj;                            // bias not decayed
        }
        if (k > 0) {
            const float* pk = pre[k].data();
            for (int i = 0; i < in; i++) gPrev[i] *= (pk[i] > 0.0f) ? 1.0f : 0.0f;   // ReLU'
            g.swap(gPrev);
        }
    }
}

void MLPModel::writeWeights(std::ostream& f) const {
    f << std::setprecision(9);
    f << "layers=";
    for (size_t i = 0; i < sizes.size(); i++) { if (i) f << ","; f << sizes[i]; }
    f << "\n";
    int L = (int)sizes.size() - 1;
    for (int k = 0; k < L; k++) {
        for (size_t t = 0; t < W[k].size(); t++) f << "l" << k << "w" << t << "=" << W[k][t] << "\n";
        for (size_t t = 0; t < B[k].size(); t++) f << "l" << k << "b" << t << "=" << B[k][t] << "\n";
    }
}

bool MLPModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=mlp\n";
    f << "head=" << (headType == HEAD_POLICY ? "policy" : "value") << "\n";
    f << "feature_version=" << featVer << "\n";
    f << "feature_count=" << n << "\n";
    f << "out_scale=" << outScale << "\n";
    writeWeights(f);
    return true;
}

// ============================================================
// CONV MODEL
// ============================================================
ConvModel::ConvModel(int head, int featVersion, int featCount, float scale,
                     const std::vector<int>& convChannels, const std::vector<int>& fcHiddenSizes)
    : headType(head), featVer(featVersion), n(featCount), outScale(scale),
      channels(convChannels), fcHidden(fcHiddenSizes) {
    int K = (int)channels.size();
    convW.resize(K); convB.resize(K);
    for (int k = 0; k < K; k++) {
        int inCh  = (k == 0) ? 2 : channels[k-1];
        int outCh = channels[k];
        convW[k].assign((size_t)outCh * inCh * 9, 0.0f);
        convB[k].assign(outCh, 0.0f);
    }
    act.resize(K + 1);
    act[0].assign(2 * SIZE * SIZE, 0.0f);
    for (int k = 0; k < K; k++) act[k+1].assign((size_t)channels[k] * SIZE * SIZE, 0.0f);
    pre.resize(K);
    for (int k = 0; k < K; k++) pre[k].assign((size_t)channels[k] * SIZE * SIZE, 0.0f);

    int flatDim = (K == 0) ? (2 * SIZE * SIZE + 1) : (channels.back() * SIZE * SIZE + 1);
    fcSizes.push_back(flatDim);
    for (size_t i = 0; i < fcHidden.size(); i++) if (fcHidden[i] > 0) fcSizes.push_back(fcHidden[i]);
    fcSizes.push_back(1);
    int L = (int)fcSizes.size() - 1;
    fcW.resize(L); fcB.resize(L);
    for (int k = 0; k < L; k++) {
        fcW[k].assign((size_t)fcSizes[k] * fcSizes[k+1], 0.0f);
        fcB[k].assign(fcSizes[k+1], 0.0f);
    }
    fcAct.resize(L + 1); fcPre.resize(L + 1);
    for (int k = 0; k <= L; k++) { fcAct[k].assign(fcSizes[k], 0.0f); fcPre[k].assign(fcSizes[k], 0.0f); }
    flat.assign(flatDim, 0.0f);
}

void ConvModel::initRandom() {
    for (size_t k = 0; k < convW.size(); k++) {
        int inCh = (k == 0) ? 2 : channels[k-1];
        float scaleW = (float)sqrt(1.0 / (double)(inCh * 9));   // fan-in = inCh*3*3
        for (size_t t = 0; t < convW[k].size(); t++)
            convW[k][t] = (float)(((double)rand() / RAND_MAX) * 2.0 - 1.0) * scaleW;
        for (size_t t = 0; t < convB[k].size(); t++) convB[k][t] = 0.0f;
    }
    int L = (int)fcSizes.size() - 1;
    for (int k = 0; k < L; k++) {
        int in = fcSizes[k];
        float scaleW = (in > 0) ? (float)sqrt(1.0 / (double)in) : 1.0f;
        for (size_t t = 0; t < fcW[k].size(); t++)
            fcW[k][t] = (float)(((double)rand() / RAND_MAX) * 2.0 - 1.0) * scaleW;
        for (size_t t = 0; t < fcB[k].size(); t++) fcB[k][t] = 0.0f;
    }
}

float ConvModel::computeForward(const float* x, int m) const {
    int in0 = 2 * SIZE * SIZE;
    int lim = (m < in0) ? m : in0;
    for (int i = 0; i < in0; i++) act[0][i] = (i < lim) ? x[i] : 0.0f;

    int K = (int)channels.size();
    for (int k = 0; k < K; k++) {
        int inCh  = (k == 0) ? 2 : channels[k-1];
        int outCh = channels[k];
        const float* in = act[k].data();
        const std::vector<float>& Wk = convW[k];
        const std::vector<float>& Bk = convB[k];
        float* preK = pre[k].data();
        for (int oc = 0; oc < outCh; oc++) {
            for (int y = 0; y < SIZE; y++) {
                for (int xx = 0; xx < SIZE; xx++) {
                    float z = Bk[oc];
                    for (int ic = 0; ic < inCh; ic++) {
                        const float* inPlane = in + (size_t)ic * SIZE * SIZE;
                        const float* wbase = &Wk[((size_t)oc * inCh + ic) * 9];
                        for (int ky = -1; ky <= 1; ky++) {
                            int iy = y + ky;
                            if (iy < 0 || iy >= SIZE) continue;
                            for (int kx = -1; kx <= 1; kx++) {
                                int ix = xx + kx;
                                if (ix < 0 || ix >= SIZE) continue;
                                z += wbase[(ky+1)*3 + (kx+1)] * inPlane[iy*SIZE + ix];
                            }
                        }
                    }
                    preK[oc*SIZE*SIZE + y*SIZE + xx] = z;
                }
            }
        }
        if (residualLayer(k)) {
            // Same shape guaranteed (inCh == outCh when residual), so this is a
            // plain elementwise add of the block's own input.
            for (int i = 0; i < outCh*SIZE*SIZE; i++) preK[i] += in[i];
        }
        float* outAct = act[k+1].data();
        for (int i = 0; i < outCh*SIZE*SIZE; i++) outAct[i] = (preK[i] > 0.0f) ? preK[i] : 0.0f;
    }

    const float* trunkOut = (K > 0) ? act[K].data() : act[0].data();
    int trunkLen = (K > 0) ? channels.back() * SIZE * SIZE : 2 * SIZE * SIZE;
    for (int i = 0; i < trunkLen; i++) flat[i] = trunkOut[i];
    flat[trunkLen] = (m > in0) ? x[in0] : 0.0f;   // side-to-move (feature 128), not convolved

    int L = (int)fcSizes.size() - 1;
    for (int i = 0; i < fcSizes[0]; i++) fcAct[0][i] = flat[i];
    for (int k = 0; k < L; k++) {
        int in = fcSizes[k], out = fcSizes[k+1];
        const std::vector<float>& Wk = fcW[k];
        const std::vector<float>& Bk = fcB[k];
        const float* a = fcAct[k].data();
        bool hidden = (k + 1 < L);
        for (int j = 0; j < out; j++) {
            const float* wrow = &Wk[(size_t)j * in];
            float z = Bk[j];
            for (int i = 0; i < in; i++) z += wrow[i] * a[i];
            fcPre[k+1][j] = z;
            fcAct[k+1][j] = hidden ? (z > 0.0f ? z : 0.0f) : z;
        }
    }
    return fcAct[L][0];
}

float ConvModel::forward(const float* x, int m) const {
    return computeForward(x, m);
}

float ConvModel::trainStep(const float* x, int m, float target, float lr, float l2, float offset) {
    float outv = computeForward(x, m);
    float z = outv + offset;
    float p = sigmoidf(z);
    float eps = 1e-7f;
    float loss = -(target * logf(p + eps) + (1.0f - target) * logf(1.0f - p + eps));
    backprop(p - target, lr, l2);
    return loss;
}

void ConvModel::gradStep(const float* x, int m, float gOut, float lr, float l2) {
    computeForward(x, m);
    backprop(gOut, lr, l2);
}

void ConvModel::backprop(float gOut, float lr, float l2) {
    // ---- FC head, top to bottom, also computing dL/d(flat) at k==0 (unlike
    // MLPModel::backprop, which never needs its input gradient) ----
    int L = (int)fcSizes.size() - 1;
    std::vector<float> g(1, gOut);
    std::vector<float> dFlat;
    for (int k = L - 1; k >= 0; k--) {
        int in = fcSizes[k], out2 = fcSizes[k+1];
        std::vector<float>& Wk = fcW[k];
        std::vector<float>& Bk = fcB[k];
        const float* a = fcAct[k].data();
        std::vector<float> gPrev(in, 0.0f);
        for (int j = 0; j < out2; j++) {
            float gj = g[j];
            float* wrow = &Wk[(size_t)j * in];
            for (int i = 0; i < in; i++) {
                gPrev[i] += gj * wrow[i];               // pre-update weight
                wrow[i] -= lr * (gj * a[i] + l2 * wrow[i]);
            }
            Bk[j] -= lr * gj;
        }
        if (k > 0) {
            const float* pk = fcPre[k].data();
            for (int i = 0; i < in; i++) gPrev[i] *= (pk[i] > 0.0f) ? 1.0f : 0.0f;   // ReLU'
            g.swap(gPrev);
        } else {
            dFlat.swap(gPrev);   // input has no activation function: pass through raw
        }
    }

    int K = (int)channels.size();
    if (K == 0) return;   // no conv layers to backprop into
    int trunkLen = channels.back() * SIZE * SIZE;
    std::vector<float> dOut(dFlat.begin(), dFlat.begin() + trunkLen);   // dFlat's last entry (stm) has no upstream

    // ---- Conv trunk, top to bottom ----
    for (int k = K - 1; k >= 0; k--) {
        int inCh  = (k == 0) ? 2 : channels[k-1];
        int outCh = channels[k];
        bool res  = residualLayer(k);
        const float* preK  = pre[k].data();
        const float* inAct = act[k].data();
        std::vector<float>& Wk = convW[k];
        std::vector<float>& Bk = convB[k];

        std::vector<float> dPre((size_t)outCh * SIZE * SIZE);
        for (size_t i = 0; i < dPre.size(); i++) dPre[i] = dOut[i] * (preK[i] > 0.0f ? 1.0f : 0.0f);

        std::vector<float> dW(Wk.size(), 0.0f);
        std::vector<float> dB(outCh, 0.0f);
        std::vector<float> dIn((size_t)inCh * SIZE * SIZE, 0.0f);

        for (int oc = 0; oc < outCh; oc++) {
            const float* dPreOc = &dPre[(size_t)oc * SIZE * SIZE];
            float bsum = 0.0f;
            for (int y = 0; y < SIZE; y++) {
                for (int xx = 0; xx < SIZE; xx++) {
                    float dp = dPreOc[y*SIZE + xx];
                    bsum += dp;
                    if (dp == 0.0f) continue;
                    for (int ic = 0; ic < inCh; ic++) {
                        const float* inPlane = inAct + (size_t)ic * SIZE * SIZE;
                        float* dWbase = &dW[((size_t)oc * inCh + ic) * 9];
                        const float* wbase = &Wk[((size_t)oc * inCh + ic) * 9];
                        float* dInPlane = &dIn[(size_t)ic * SIZE * SIZE];
                        for (int ky = -1; ky <= 1; ky++) {
                            int iy = y + ky;
                            if (iy < 0 || iy >= SIZE) continue;
                            for (int kx = -1; kx <= 1; kx++) {
                                int ix = xx + kx;
                                if (ix < 0 || ix >= SIZE) continue;
                                int widx = (ky+1)*3 + (kx+1);
                                dWbase[widx] += dp * inPlane[iy*SIZE + ix];
                                dInPlane[iy*SIZE + ix] += wbase[widx] * dp;
                            }
                        }
                    }
                }
            }
            dB[oc] = bsum;
        }

        if (res) {
            // pre = conv(x)+bias + x (skip), so d(pre)/dx also gets a direct
            // identity term (valid since res implies inCh == outCh, same shape).
            for (size_t i = 0; i < dIn.size(); i++) dIn[i] += dPre[i];
        }

        for (size_t i = 0; i < Wk.size(); i++) Wk[i] -= lr * (dW[i] + l2 * Wk[i]);
        for (int oc = 0; oc < outCh; oc++) Bk[oc] -= lr * dB[oc];

        dOut = dIn;   // becomes the upstream gradient for layer k-1
    }
}

void ConvModel::writeWeights(std::ostream& f) const {
    f << std::setprecision(9);
    f << "channels=";
    for (size_t i = 0; i < channels.size(); i++) { if (i) f << ","; f << channels[i]; }
    f << "\n";
    f << "fc_layers=";
    for (size_t i = 0; i < fcSizes.size(); i++) { if (i) f << ","; f << fcSizes[i]; }
    f << "\n";
    for (size_t k = 0; k < convW.size(); k++) {
        for (size_t t = 0; t < convW[k].size(); t++) f << "c" << k << "w" << t << "=" << convW[k][t] << "\n";
        for (size_t t = 0; t < convB[k].size(); t++) f << "c" << k << "b" << t << "=" << convB[k][t] << "\n";
    }
    int L = (int)fcSizes.size() - 1;
    for (int k = 0; k < L; k++) {
        for (size_t t = 0; t < fcW[k].size(); t++) f << "f" << k << "w" << t << "=" << fcW[k][t] << "\n";
        for (size_t t = 0; t < fcB[k].size(); t++) f << "f" << k << "b" << t << "=" << fcB[k][t] << "\n";
    }
}

bool ConvModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=conv\n";
    f << "head=" << (headType == HEAD_POLICY ? "policy" : "value") << "\n";
    f << "feature_version=" << featVer << "\n";
    f << "feature_count=" << n << "\n";
    f << "out_scale=" << outScale << "\n";
    writeWeights(f);
    return true;
}

// ============================================================
// RESIDUAL MODEL (frozen chip-count skip + inner model)
// ============================================================
bool ResidualModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << std::setprecision(9);
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=residual\n";
    f << "inner_type=" << inner->typeName() << "\n";
    f << "skip_weight=" << skipW << "\n";
    f << "head=" << (inner->head() == HEAD_POLICY ? "policy" : "value") << "\n";
    f << "feature_version=" << inner->featureVersion() << "\n";
    f << "feature_count=" << inner->featureCount() << "\n";
    f << "out_scale=" << inner->outputScale() << "\n";
    inner->writeWeights(f);     // inner's weight block inline (mlp writes its layers= line here too)
    return true;
}

// ============================================================
// DIST MODEL (two heads: mu + log-sigma)
// ============================================================
void DistModel::forwardDist(const float* x, int m, float& muLogit, float& sigmaLogit) const {
    muLogit = muHead->forward(x, m);
    double s = sHead->forward(x, m);
    if (s < PROBIT_S_MIN) s = PROBIT_S_MIN;
    if (s > PROBIT_S_MAX) s = PROBIT_S_MAX;
    sigmaLogit = (float)exp(s);
}

float DistModel::trainStepRow(const float* x, int m, float y, float dLogit, float extraVar,
                              float lrMu, float lrS, float l2) {
    double mu   = muHead->forward(x, m);
    double sRaw = sHead->forward(x, m);
    double s = sRaw;
    if (s < PROBIT_S_MIN) s = PROBIT_S_MIN;
    if (s > PROBIT_S_MAX) s = PROBIT_S_MAX;
    ProbitGrad g;
    double loss = probitPoint(mu, s, dLogit, extraVar, y, g);
    muHead->gradStep(x, m, (float)g.gMu, lrMu, l2);
    // Projected gradient at the s clamp: block only pushes that would move s
    // further outside the range (descent direction is -gS).
    bool outward = (sRaw >= PROBIT_S_MAX && g.gS < 0.0) ||
                   (sRaw <= PROBIT_S_MIN && g.gS > 0.0);
    if (!outward) sHead->gradStep(x, m, (float)g.gS, lrS, l2);
    return (float)loss;
}

float DistModel::trainStepGauss(const float* x, int m, float muLab, float sdLab,
                                float wMu, float wSd, float lr, float l2) {
    double mu   = muHead->forward(x, m);
    double sRaw = sHead->forward(x, m);
    double sLab = log((sdLab > 1e-6f) ? (double)sdLab : 1e-6);
    double eMu = mu - muLab;
    double eS  = sRaw - sLab;
    muHead->gradStep(x, m, (float)(wMu * eMu), lr, l2);
    sHead->gradStep(x, m, (float)(wSd * eS), lr, l2);
    return (float)(0.5 * (wMu * eMu * eMu + wSd * eS * eS));
}

// Serialize a head's weight block with every key prefixed, so two heads of the
// same architecture never collide in the flat key=value file.
static void writePrefixedWeights(std::ostream& f, const char* prefix, const Model* m) {
    std::ostringstream ss;
    ss << std::setprecision(9);
    m->writeWeights(ss);
    std::istringstream in(ss.str());
    string line;
    while (std::getline(in, line))
        if (!line.empty()) f << prefix << line << "\n";
}

void DistModel::writeWeights(std::ostream& f) const {
    writePrefixedWeights(f, "mu_", muHead);
    writePrefixedWeights(f, "s_", sHead);
}

bool DistModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << std::setprecision(9);
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=dist\n";
    f << "mu_type=" << muHead->typeName() << "\n";
    f << "s_type=" << sHead->typeName() << "\n";
    f << "head=value\n";
    f << "feature_version=" << muHead->featureVersion() << "\n";
    f << "feature_count=" << muHead->featureCount() << "\n";
    f << "out_scale=" << muHead->outputScale() << "\n";
    writeWeights(f);
    return true;
}

// ============================================================
// JOINT MODEL (value head + policy head, different feature layouts)
// ============================================================
void JointModel::writeWeights(std::ostream& f) const {
    writePrefixedWeights(f, "v_", valueHead);
    writePrefixedWeights(f, "p_", policyHead);
}

bool JointModel::save(const string& path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << std::setprecision(9);
    f << "# Breakthrough ML model\n";
    if (!teacher.empty()) f << "teacher=" << teacher << "\n";
    f << "type=joint\n";
    f << "head=value\n";
    // Uniform v_/p_ prefix for BOTH the meta keys here and the weight-block
    // keys writeWeights() emits (mirrors DistModel's mu_/s_ convention, where
    // one prefix names everything belonging to a head).
    f << "v_type=" << valueHead->typeName() << "\n";
    f << "v_feature_version=" << valueHead->featureVersion() << "\n";
    f << "v_feature_count=" << valueHead->featureCount() << "\n";
    f << "v_out_scale=" << valueHead->outputScale() << "\n";
    f << "p_type=" << policyHead->typeName() << "\n";
    f << "p_feature_version=" << policyHead->featureVersion() << "\n";
    f << "p_feature_count=" << policyHead->featureCount() << "\n";
    f << "p_out_scale=" << policyHead->outputScale() << "\n";
    writeWeights(f);
    return true;
}

// ============================================================
// FACTORY / LOADER
// ============================================================
Model* makeModel(const string& type, int head, int featVersion, int featCount, float scale) {
    if (type == "linear") return new LinearModel(head, featVersion, featCount, scale);
    // mlp / residual need extra structure (hidden sizes / a skip + inner) and are
    // built by loadModel / the trainer directly. nnue / transformer: docs only.
    return nullptr;
}

// Parse a comma list "129,32,1" into ints.
static std::vector<int> parseIntList(const string& s) {
    std::vector<int> v; size_t i = 0;
    while (i <= s.size()) {
        size_t c = s.find(',', i);
        string tok = s.substr(i, (c == string::npos ? s.size() : c) - i);
        if (!tok.empty()) { try { v.push_back(std::stoi(tok)); } catch (...) {} }
        if (c == string::npos) break;
        i = c + 1;
    }
    return v;
}

// Build a linear model's weights from a parsed key/value map. Reused by the direct
// `type=linear` case and by the residual loader for a linear inner.
static LinearModel* buildLinearFromKV(const map<string, string>& kv, int head, int featVer, int n, float scale) {
    LinearModel* m = new LinearModel(head, featVer, n, scale);
    map<string, string>::const_iterator it = kv.find("bias");
    if (it != kv.end()) m->bias = std::stof(it->second);
    for (int i = 0; i < n; i++) {
        it = kv.find("w" + std::to_string(i));
        if (it != kv.end()) m->w[i] = std::stof(it->second);
    }
    return m;
}

// Build an MLP model's weights from a parsed key/value map (needs the `layers=`
// line for the architecture). Reused by the direct case and the residual loader.
static MLPModel* buildMLPFromKV(const map<string, string>& kv, int head, int featVer, int n, float scale) {
    map<string, string>::const_iterator it = kv.find("layers");
    if (it == kv.end()) return nullptr;
    std::vector<int> sz = parseIntList(it->second);
    if ((int)sz.size() < 2 || sz.front() != n) return nullptr;
    std::vector<int> hidden(sz.begin() + 1, sz.end() - 1);   // strip input + output
    MLPModel* m = new MLPModel(head, featVer, n, scale, hidden);
    int L = (int)m->sizes.size() - 1;
    for (int k = 0; k < L; k++) {
        for (size_t t = 0; t < m->W[k].size(); t++) {
            it = kv.find("l" + std::to_string(k) + "w" + std::to_string(t));
            if (it != kv.end()) m->W[k][t] = std::stof(it->second);
        }
        for (size_t t = 0; t < m->B[k].size(); t++) {
            it = kv.find("l" + std::to_string(k) + "b" + std::to_string(t));
            if (it != kv.end()) m->B[k][t] = std::stof(it->second);
        }
    }
    return m;
}

// Build a conv model's weights from a parsed key/value map (needs `channels=` and
// `fc_layers=`). Reused by the direct `type=conv` case and the joint loader's
// value-head branch. Guards featVer/n against the spatial layout ConvModel
// hardcodes (2 planes of SIZE*SIZE + 1 stm scalar) rather than trusting the file --
// a hand-edited or corrupted `feature_count` here would otherwise read/write past
// the fixed-size board-plane buffers computeForward/backprop assume.
static ConvModel* buildConvFromKV(const map<string, string>& kv, int head, int featVer, int n, float scale) {
    if (featVer != 2 || n != 2*SIZE*SIZE + 1) return nullptr;
    map<string, string>::const_iterator it = kv.find("channels");
    if (it == kv.end()) return nullptr;
    std::vector<int> channels = parseIntList(it->second);
    it = kv.find("fc_layers");
    if (it == kv.end()) return nullptr;
    std::vector<int> fcSz = parseIntList(it->second);
    if ((int)fcSz.size() < 2) return nullptr;
    std::vector<int> fcHidden(fcSz.begin() + 1, fcSz.end() - 1);
    ConvModel* m = new ConvModel(head, featVer, n, scale, channels, fcHidden);
    if (m->fcSizes.size() != fcSz.size() || m->fcSizes[0] != fcSz[0]) { delete m; return nullptr; }
    for (size_t k = 0; k < m->convW.size(); k++) {
        for (size_t t = 0; t < m->convW[k].size(); t++) {
            it = kv.find("c" + std::to_string(k) + "w" + std::to_string(t));
            if (it != kv.end()) m->convW[k][t] = std::stof(it->second);
        }
        for (size_t t = 0; t < m->convB[k].size(); t++) {
            it = kv.find("c" + std::to_string(k) + "b" + std::to_string(t));
            if (it != kv.end()) m->convB[k][t] = std::stof(it->second);
        }
    }
    int L = (int)m->fcSizes.size() - 1;
    for (int k = 0; k < L; k++) {
        for (size_t t = 0; t < m->fcW[k].size(); t++) {
            it = kv.find("f" + std::to_string(k) + "w" + std::to_string(t));
            if (it != kv.end()) m->fcW[k][t] = std::stof(it->second);
        }
        for (size_t t = 0; t < m->fcB[k].size(); t++) {
            it = kv.find("f" + std::to_string(k) + "b" + std::to_string(t));
            if (it != kv.end()) m->fcB[k][t] = std::stof(it->second);
        }
    }
    return m;
}

Model* loadModel(const string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return nullptr;
    map<string, string> kv;
    string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }
    if (!kv.count("type")) return nullptr;
    string type = kv["type"];
    int head = (kv.count("head") && kv["head"] == "policy") ? HEAD_POLICY : HEAD_VALUE;
    int featVer = kv.count("feature_version") ? std::stoi(kv["feature_version"]) : 1;
    int n       = kv.count("feature_count")   ? std::stoi(kv["feature_count"])   : 0;
    float scale = kv.count("out_scale")        ? std::stof(kv["out_scale"])        : 900.0f;

    if (type == "linear") {
        LinearModel* m = buildLinearFromKV(kv, head, featVer, n, scale);
        if (kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    if (type == "mlp") {
        MLPModel* m = buildMLPFromKV(kv, head, featVer, n, scale);
        if (m && kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    if (type == "conv") {
        ConvModel* m = buildConvFromKV(kv, head, featVer, n, scale);
        if (m && kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    if (type == "residual") {
        float skipW = kv.count("skip_weight") ? std::stof(kv["skip_weight"]) : 0.0f;
        string innerType = kv.count("inner_type") ? kv["inner_type"] : "linear";
        Model* inner = (innerType == "mlp") ? (Model*)buildMLPFromKV(kv, head, featVer, n, scale)
                                            : (Model*)buildLinearFromKV(kv, head, featVer, n, scale);
        if (!inner) return nullptr;
        ResidualModel* m = new ResidualModel(skipW, featVer, inner);
        if (kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    if (type == "dist") {
        string muType = kv.count("mu_type") ? kv["mu_type"] : "linear";
        string sType  = kv.count("s_type")  ? kv["s_type"]  : "linear";
        // Split the flat key space into the two heads' prefixed sub-maps
        // (mu_type/s_type land as a harmless "type" key in each sub-map).
        map<string, string> muKv, sKv;
        for (map<string, string>::const_iterator it = kv.begin(); it != kv.end(); ++it) {
            const string& k = it->first;
            if (k.compare(0, 3, "mu_") == 0)     muKv[k.substr(3)] = it->second;
            else if (k.compare(0, 2, "s_") == 0) sKv[k.substr(2)]  = it->second;
        }
        Model* mu = (muType == "mlp") ? (Model*)buildMLPFromKV(muKv, head, featVer, n, scale)
                                      : (Model*)buildLinearFromKV(muKv, head, featVer, n, scale);
        Model* sh = (sType == "mlp") ? (Model*)buildMLPFromKV(sKv, head, featVer, n, scale)
                                     : (Model*)buildLinearFromKV(sKv, head, featVer, n, scale);
        if (!mu || !sh) { delete mu; delete sh; return nullptr; }
        DistModel* m = new DistModel(mu, sh);
        if (kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    if (type == "joint") {
        string vType = kv.count("v_type") ? kv["v_type"] : "linear";
        string pType = kv.count("p_type") ? kv["p_type"] : "linear";
        int   vFeatVer = kv.count("v_feature_version") ? std::stoi(kv["v_feature_version"]) : 2;
        int   vFeatN   = kv.count("v_feature_count")   ? std::stoi(kv["v_feature_count"])   : 0;
        float vScale   = kv.count("v_out_scale")        ? std::stof(kv["v_out_scale"])        : 900.0f;
        int   pFeatVer = kv.count("p_feature_version") ? std::stoi(kv["p_feature_version"]) : 1;
        int   pFeatN   = kv.count("p_feature_count")   ? std::stoi(kv["p_feature_count"])   : 0;
        float pScale   = kv.count("p_out_scale")        ? std::stof(kv["p_out_scale"])        : 1.0f;
        // Split the flat key space into the two heads' prefixed sub-maps, mirroring
        // dist's mu_/s_ split -- but unlike dist, each head keeps its OWN feature
        // layout (value = board features, policy = move features), so there is no
        // shared featVer/n/scale to reuse from the top-level generic keys.
        map<string, string> vKv, pKv;
        for (map<string, string>::const_iterator it = kv.begin(); it != kv.end(); ++it) {
            const string& k = it->first;
            if (k.compare(0, 2, "v_") == 0)      vKv[k.substr(2)] = it->second;
            else if (k.compare(0, 2, "p_") == 0) pKv[k.substr(2)] = it->second;
        }
        Model* v = (vType == "mlp")  ? (Model*)buildMLPFromKV(vKv, HEAD_VALUE, vFeatVer, vFeatN, vScale)
                 : (vType == "conv") ? (Model*)buildConvFromKV(vKv, HEAD_VALUE, vFeatVer, vFeatN, vScale)
                                      : (Model*)buildLinearFromKV(vKv, HEAD_VALUE, vFeatVer, vFeatN, vScale);
        Model* p = (pType == "mlp") ? (Model*)buildMLPFromKV(pKv, HEAD_POLICY, pFeatVer, pFeatN, pScale)
                                     : (Model*)buildLinearFromKV(pKv, HEAD_POLICY, pFeatVer, pFeatN, pScale);
        if (!v || !p) { delete v; delete p; return nullptr; }
        JointModel* m = new JointModel(v, p);
        if (kv.count("teacher")) m->teacher = kv["teacher"];
        return m;
    }
    return nullptr;   // unimplemented architecture
}

// ============================================================
// ARCHITECTURE REGISTRY
// ============================================================
const ModelTypeDef g_modelTypes[] = {
    { "linear",      "Linear: bias + weighted sum of features. Fast; value or policy head.", true  },
    { "mlp",         "Multilayer perceptron (1-2 hidden layers), hand-written forward + backprop; ReLU hidden, linear output.", true },
    { "conv",        "Small residual conv tower (3x3 same-pad layers) over the v2 board's white/black occupancy planes + a dense head; side-to-move rides in at the FC stage. Value head only (the policy head's move features aren't spatial). AlphaZero-style capacity arm.", true },
    { "residual",    "Frozen chip-count skip + an inner model (linear or mlp): output = skipW*matDiff + inner. Learns the residual.", true },
    { "dist",        "Two-headed distributional value model: mu head (White advantage in logits, the evaluator output) + log-sigma head (volatility), probit-BCE trained on rated-gap playout outcomes.", true },
    { "joint",       "Two-headed value+policy model: a board value head (feature v1/v2) + a per-move policy head (move features), different feature layouts. Gumbel MCTS search substrate.", true },
    { "nnue",        "Efficiently updatable NN; designed to plug into the g_evalPos accumulator.", false },
    { "transformer", "Squares-as-tokens self-attention; teacher / offline label generator only.", false },
};
const int g_modelTypeCount = (int)(sizeof(g_modelTypes) / sizeof(g_modelTypes[0]));
