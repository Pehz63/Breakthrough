#pragma once
#include "globals.h"
#include <vector>
#include <iosfwd>

// ============================================================
// ML models: pluggable architectures behind learned parts
// ============================================================
// A Model maps a feature vector to a scalar. The SAME model class can back either
//   * a VALUE head  (board features  -> board score), used by LearnedValue, or
//   * a POLICY head (move features   -> move score),  used by the move-rater,
// distinguished by head(). Architectures (linear / mlp / nnue / transformer) are
// registered in g_modelTypes[]; only the type's name + a flag are needed for docs,
// the factory constructs the real object. Model files are text (see loadModel) with
// a `type=` line the factory dispatches on, so adding an architecture is one
// subclass + one factory case + one registry row.

enum ModelHead { HEAD_VALUE = 0, HEAD_POLICY = 1 };

// ---- Probit-approximation math for the distributional (mu, sigma) head ----
// One Bernoulli observation of a playout game from a position whose latent
// White advantage is A ~ N(mu, sigma^2) in logit units, played by agents with
// a known Elo gap d (logit units): P(White wins) = sigmoid(kappa*(mu+d)) with
// kappa = 1/sqrt(1 + (pi/8)*(sigma^2 + v)), the standard logistic-normal
// probit approximation (constant pi/8, NOT pi^2/8, so sigma reads as the true
// latent SD). s = log(sigma) is the trained parametrization, clamped to
// [PROBIT_S_MIN, PROBIT_S_MAX]; callers zero gS when s sits on a clamp.
// Shared by the per-position label fit (rank.exe labelfit) and the DistModel
// training step, so the two can never diverge.
const double ELO_PER_LOGIT = 173.7177928;   // 400/ln(10)
const double PROBIT_S_MIN  = -4.0;          // sigma ~ 3 Elo
const double PROBIT_S_MAX  = 3.0;           // sigma ~ 3500 Elo

struct ProbitGrad {
    double p;      // predicted P(White wins)
    double kappa;  // variance-flattening factor
    double gMu;    // dL/dmu
    double gS;     // dL/ds  (s = log sigma)
};
// Returns the BCE loss of outcome y in {0,1} (0.5 works for draws) and fills
// the gradients. v = known extra variance (e.g. rating standard errors) in
// logit^2 units, 0 to disable.
double probitPoint(double mu, double s, double d, double v, double y, ProbitGrad& out);

struct Model {
    // Provenance: how this model was trained (e.g. the teacher/generator agent that
    // produced its data). Written to the model file as `teacher=...` and surfaced in
    // the manifest so a saved model self-documents its lineage. Empty = unset.
    string teacher;
    virtual ~Model() {}
    virtual const char* typeName()       const = 0;   // matches g_modelTypes / file `type=`
    virtual int  head()                  const = 0;   // ModelHead
    virtual int  featureVersion()        const = 0;
    virtual int  featureCount()          const = 0;
    virtual float outputScale()          const { return 1.0f; }  // maps raw output to eval range
    virtual float forward(const float* x, int n) const = 0;  // raw output (logit-like)
    virtual bool save(const string& path) const = 0;

    // Write just this model's weight block (no header) so a wrapper like
    // ResidualModel can serialize an inner model of any type inline. Default no-op.
    virtual void writeWeights(std::ostream& f) const { (void)f; }

    // One logistic-regression training step toward `target` in [0,1]; returns the
    // pre-update cross-entropy loss. `offset` is a fixed additive term folded into
    // the output logit before the sigmoid (a GLM offset): it shifts the loss/gradient
    // but is NOT itself trained, which is exactly how a frozen skip connection enters
    // training (see ResidualModel). Default no-op returns 0 for non-trainable models.
    virtual float trainStep(const float* x, int n, float target, float lr,
                            float l2 = 0.0f, float offset = 0.0f) { (void)x;(void)n;(void)target;(void)lr;(void)l2;(void)offset; return 0.0f; }

    // Apply an arbitrary upstream gradient gOut on the output (dL/d_output) as
    // one SGD step. Generalizes trainStep (whose logistic loss has
    // gOut = p - target) so a wrapper like DistModel can push its own loss
    // gradients through each head. Default no-op for non-trainable models.
    virtual void gradStep(const float* x, int n, float gOut, float lr,
                          float l2 = 0.0f) { (void)x;(void)n;(void)gOut;(void)lr;(void)l2; }
};

// Raw white-minus-black piece count read out of a value feature vector, used as the
// chip-count skip term (ResidualModel) and for material-stratified analysis. v2
// (sparse piece-square): sum(white squares) - sum(black squares). v1 (dense
// aggregates): feature 0 is (wTotal-bTotal)/16, so multiply back by 16. Units match
// g_chipDiff, so a per-piece skip weight is consistent across the full-scan and
// incremental leaf paths.
float matDiffFromFeatures(const float* x, int featVer);

// ---- Linear model: bias + sum(w[i]*x[i]) ----
struct LinearModel : public Model {
    int   headType;     // ModelHead
    int   featVer;      // feature version it was trained against
    int   n;            // feature count
    float outScale;     // maps raw output to the integer eval range (value head)
    float bias;
    std::vector<float> w;

    LinearModel(int head, int featVersion, int featCount, float scale);
    const char* typeName()  const override { return "linear"; }
    int  head()             const override { return headType; }
    int  featureVersion()   const override { return featVer; }
    int  featureCount()     const override { return n; }
    float outputScale()     const override { return outScale; }
    float forward(const float* x, int m) const override;
    bool save(const string& path) const override;
    void writeWeights(std::ostream& f) const override;

    // One logistic-regression SGD step toward `target` in [0,1]; returns the
    // pre-update cross-entropy loss so callers can watch it fall. l2 (default 0 =
    // unregularized, the historical behavior) applies simple weight decay
    // w[i] -= lr*l2*w[i] alongside the gradient step (bias is not decayed). offset
    // (default 0) is added to the logit before the sigmoid without being trained
    // (the frozen-skip GLM offset). The logit is computed inline (not via the
    // virtual forward) so a subclass override is never double-counted.
    float sgdLogisticStep(const float* x, int m, float target, float lr, float l2 = 0.0f, float offset = 0.0f);
    float trainStep(const float* x, int m, float target, float lr, float l2, float offset) override {
        return sgdLogisticStep(x, m, target, lr, l2, offset);
    }
    void gradStep(const float* x, int m, float gOut, float lr, float l2) override;
};

// ---- MLP model: 1-2 hidden layers, ReLU hidden, linear output logit ----
// A hand-written fully-connected net. Layer k transforms activation of dim
// sizes[k] to pre-activation of dim sizes[k+1]; hidden layers apply ReLU, the
// final layer is linear (the logit). Backprop is hand-written in trainStep. NOT
// incrementally updatable (see ml_eval: it uses the full-scan leaf path), which is
// a separate NNUE task; here it is the residual inner arm the chip skip rides on.
struct MLPModel : public Model {
    int   headType;
    int   featVer;
    int   n;                          // input feature count (== sizes[0])
    float outScale;
    std::vector<int> sizes;           // [in, h1, (h2,) 1]
    std::vector<std::vector<float> > W;  // W[k]: sizes[k+1] x sizes[k], output-major (idx = j*sizes[k] + i)
    std::vector<std::vector<float> > B;  // B[k]: sizes[k+1]
    // Scratch for the forward pass, reused across calls to avoid per-node allocation
    // (single-threaded engine, so a mutable per-model buffer is safe).
    mutable std::vector<std::vector<float> > act;   // act[0..L]; act[0] = input
    mutable std::vector<std::vector<float> > pre;   // pre[1..L] pre-activations

    // hidden = list of hidden-layer widths (empty => degenerates to a linear map).
    MLPModel(int head, int featVersion, int featCount, float scale, const std::vector<int>& hidden);
    const char* typeName()  const override { return "mlp"; }
    int  head()             const override { return headType; }
    int  featureVersion()   const override { return featVer; }
    int  featureCount()     const override { return n; }
    float outputScale()     const override { return outScale; }
    float forward(const float* x, int m) const override;
    bool save(const string& path) const override;
    void writeWeights(std::ostream& f) const override;
    float trainStep(const float* x, int m, float target, float lr, float l2, float offset) override;
    void gradStep(const float* x, int m, float gOut, float lr, float l2) override;

    // Break weight symmetry before training (zero-init hidden layers can't learn).
    // Uses rand(), so seed with srand() first for reproducibility.
    void initRandom();

private:
    // Fill act[]/pre[] scratch from x and return the output logit (no offset).
    float computeForward(const float* x, int m) const;
    // Backprop an output gradient through act[]/pre[] (must be freshly filled by
    // computeForward for the same x) and apply the SGD update in place.
    void backprop(float gOut, float lr, float l2);
};

// ---- Residual model: a frozen chip-count skip + an inner learned model ----
// output(x) = skipW * matDiffFromFeatures(x, featVer) + inner->forward(x). The skip
// is a fixed logit-space material term (never trained); the inner model learns only
// the residual. This is the theory-24 substrate: with a LinearModel inner it is an
// incrementally-scored residual PST; with an MLPModel inner it is the nonlinear
// capacity arm riding on the fixed material skip. Owns `inner`.
struct ResidualModel : public Model {
    float  skipW;
    int    featVer;
    Model* inner;

    ResidualModel(float skip, int featVersion, Model* innerModel)
        : skipW(skip), featVer(featVersion), inner(innerModel) {}
    ~ResidualModel() override { delete inner; }
    const char* typeName()  const override { return "residual"; }
    int  head()             const override { return inner->head(); }
    int  featureVersion()   const override { return inner->featureVersion(); }
    int  featureCount()     const override { return inner->featureCount(); }
    float outputScale()     const override { return inner->outputScale(); }
    float forward(const float* x, int m) const override {
        return skipW * matDiffFromFeatures(x, featVer) + inner->forward(x, m);
    }
    // Train only the inner residual: the skip enters as a frozen GLM offset.
    float trainStep(const float* x, int m, float target, float lr, float l2, float offset) override {
        return inner->trainStep(x, m, target, lr, l2, offset + skipW * matDiffFromFeatures(x, featVer));
    }
    bool save(const string& path) const override;
};

// ---- Factory / loader ----
// Construct an empty model of the named type (caller fills weights), or nullptr
// for an unimplemented / unknown type. (mlp/residual need extra structure and are
// built by loadModel / the trainer directly, not through this simple factory.)
Model* makeModel(const string& type, int head, int featVersion, int featCount, float scale);
// Load a model from a text file (dispatches on its `type=` line). nullptr on error.
Model* loadModel(const string& path);

// ---- Architecture registry (drives docs + factory coverage) ----
struct ModelTypeDef {
    const char* name;
    const char* desc;
    bool        implemented;
};
extern const ModelTypeDef g_modelTypes[];
extern const int          g_modelTypeCount;
