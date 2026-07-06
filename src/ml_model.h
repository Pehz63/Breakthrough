#pragma once
#include "globals.h"
#include <vector>

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
};

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

    // One logistic-regression SGD step toward `target` in [0,1]; returns the
    // pre-update cross-entropy loss so callers can watch it fall. l2 (default 0 =
    // unregularized, the historical behavior) applies simple weight decay
    // w[i] -= lr*l2*w[i] alongside the gradient step (bias is not decayed).
    float sgdLogisticStep(const float* x, int m, float target, float lr, float l2 = 0.0f);
};

// ---- Factory / loader ----
// Construct an empty model of the named type (caller fills weights), or nullptr
// for an unimplemented / unknown type.
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
