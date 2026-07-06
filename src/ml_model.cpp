#include "ml_model.h"
#include <cmath>

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

static inline float sigmoidf(float z) {
    if (z >= 0) { float e = expf(-z); return 1.0f / (1.0f + e); }
    float e = expf(z); return e / (1.0f + e);
}

float LinearModel::sgdLogisticStep(const float* x, int m, float target, float lr, float l2) {
    int lim = (m < n) ? m : n;
    float z = forward(x, lim);
    float p = sigmoidf(z);
    float eps = 1e-7f;
    float loss = -(target * logf(p + eps) + (1.0f - target) * logf(1.0f - p + eps));
    float g = (p - target);                 // dL/dz for logistic
    for (int i = 0; i < lim; i++) w[i] -= lr * (g * x[i] + l2 * w[i]);
    bias -= lr * g;
    return loss;
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
    f << "bias=" << bias << "\n";
    for (int i = 0; i < n; i++) f << "w" << i << "=" << w[i] << "\n";
    return true;
}

// ============================================================
// FACTORY / LOADER
// ============================================================
Model* makeModel(const string& type, int head, int featVersion, int featCount, float scale) {
    if (type == "linear") return new LinearModel(head, featVersion, featCount, scale);
    // mlp / nnue / transformer: registered for docs, not yet constructible.
    return nullptr;
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
        LinearModel* m = new LinearModel(head, featVer, n, scale);
        if (kv.count("teacher")) m->teacher = kv["teacher"];
        if (kv.count("bias")) m->bias = std::stof(kv["bias"]);
        for (int i = 0; i < n; i++) {
            string k = "w" + std::to_string(i);
            if (kv.count(k)) m->w[i] = std::stof(kv[k]);
        }
        return m;
    }
    return nullptr;   // unimplemented architecture
}

// ============================================================
// ARCHITECTURE REGISTRY
// ============================================================
const ModelTypeDef g_modelTypes[] = {
    { "linear",      "Linear: bias + weighted sum of features. Fast; value or policy head.", true  },
    { "mlp",         "Multilayer perceptron (1-2 hidden layers), hand-written forward pass.", false },
    { "nnue",        "Efficiently updatable NN; designed to plug into the g_evalPos accumulator.", false },
    { "transformer", "Squares-as-tokens self-attention; teacher / offline label generator only.", false },
};
const int g_modelTypeCount = (int)(sizeof(g_modelTypes) / sizeof(g_modelTypes[0]));
