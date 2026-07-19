#include "ml_model.h"
#include <cmath>
#include <ostream>
#include <sstream>
#include <iomanip>

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
                              float lr, float l2) {
    double mu   = muHead->forward(x, m);
    double sRaw = sHead->forward(x, m);
    double s = sRaw;
    if (s < PROBIT_S_MIN) s = PROBIT_S_MIN;
    if (s > PROBIT_S_MAX) s = PROBIT_S_MAX;
    ProbitGrad g;
    double loss = probitPoint(mu, s, dLogit, extraVar, y, g);
    muHead->gradStep(x, m, (float)g.gMu, lr, l2);
    // Projected gradient at the s clamp: block only pushes that would move s
    // further outside the range (descent direction is -gS).
    bool outward = (sRaw >= PROBIT_S_MAX && g.gS < 0.0) ||
                   (sRaw <= PROBIT_S_MIN && g.gS > 0.0);
    if (!outward) sHead->gradStep(x, m, (float)g.gS, lr, l2);
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
    return nullptr;   // unimplemented architecture
}

// ============================================================
// ARCHITECTURE REGISTRY
// ============================================================
const ModelTypeDef g_modelTypes[] = {
    { "linear",      "Linear: bias + weighted sum of features. Fast; value or policy head.", true  },
    { "mlp",         "Multilayer perceptron (1-2 hidden layers), hand-written forward + backprop; ReLU hidden, linear output.", true },
    { "residual",    "Frozen chip-count skip + an inner model (linear or mlp): output = skipW*matDiff + inner. Learns the residual.", true },
    { "dist",        "Two-headed distributional value model: mu head (White advantage in logits, the evaluator output) + log-sigma head (volatility), probit-BCE trained on rated-gap playout outcomes.", true },
    { "nnue",        "Efficiently updatable NN; designed to plug into the g_evalPos accumulator.", false },
    { "transformer", "Squares-as-tokens self-attention; teacher / offline label generator only.", false },
};
const int g_modelTypeCount = (int)(sizeof(g_modelTypes) / sizeof(g_modelTypes[0]));
