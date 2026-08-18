#include "catch.hpp"
#include "helpers.h"
#include "ml_gumbelzero.h"
#include "ai_gumbel.h"
#include "ml_features.h"
#include "ml_model.h"
#include "ml_eval.h"
#include <cmath>
#include <cstdlib>

// ============================================================
// POLICY GRADIENT CORE (pure, no board state)
// ============================================================

TEST_CASE("gumbelPolicyGradients - vanishes when target already equals the current softmax") {
    double logits[4] = { 0.3, -0.2, 0.5, 0.1 };
    double top = -1e300;
    for (int i = 0; i < 4; i++) if (logits[i] > top) top = logits[i];
    double target[4], sum = 0.0;
    for (int i = 0; i < 4; i++) { target[i] = std::exp(logits[i] - top); sum += target[i]; }
    for (int i = 0; i < 4; i++) target[i] /= sum;   // target IS softmax(logits)

    double gOut[4];
    gumbelPolicyGradients(logits, target, 4, gOut);
    for (int i = 0; i < 4; i++) REQUIRE(gOut[i] == Approx(0.0).margin(1e-9));
}

TEST_CASE("gumbelPolicyGradients - sums to ~0 across the group and matches an independent softmax") {
    double logits[3] = { 1.0, 0.0, -1.0 };
    double target[3] = { 0.7, 0.2, 0.1 };   // deliberately NOT softmax(logits)

    double top = -1e300;
    for (int i = 0; i < 3; i++) if (logits[i] > top) top = logits[i];
    double q[3], sum = 0.0;
    for (int i = 0; i < 3; i++) { q[i] = std::exp(logits[i] - top); sum += q[i]; }
    for (int i = 0; i < 3; i++) q[i] /= sum;

    double gOut[3];
    gumbelPolicyGradients(logits, target, 3, gOut);
    double total = 0.0;
    for (int i = 0; i < 3; i++) {
        REQUIRE(gOut[i] == Approx(q[i] - target[i]));
        total += gOut[i];
    }
    REQUIRE(total == Approx(0.0).margin(1e-9));
    // And it should NOT be all-zero, since target != softmax(logits) here.
    REQUIRE(std::fabs(gOut[0]) > 1e-6);
}

// ============================================================
// GUMBEL-IMPROVED POLICY TARGET (pure, no board state)
// ============================================================

TEST_CASE("gumbelImprovedPolicy - matches an independent softmax(logits + sigma(completedQ)) computation") {
    GumbelRootInfo info;
    info.moveCount = 4;
    double logits[4]      = { 0.3, -0.2, 0.5, 0.1 };
    double completedQ[4]  = { 0.05, 0.2, -0.1, 0.0 };
    int    visitCounts[4] = { 3, 1, 0, 2 };
    for (int i = 0; i < 4; i++) {
        info.logits[i] = logits[i];
        info.completedQ[i] = completedQ[i];
        info.visitCounts[i] = visitCounts[i];
    }

    // Independent reference, using the documented paper-default constants
    // (c_visit=50, c_scale=1.0 -- ai_gumbel.h's own header comment).
    const double cVisit = 50.0, cScale = 1.0;
    int maxN = 3;
    double adj[4], top = -1e300;
    for (int i = 0; i < 4; i++) {
        adj[i] = logits[i] + (cVisit + maxN) * cScale * completedQ[i];
        if (adj[i] > top) top = adj[i];
    }
    double sum = 0.0, expected[4];
    for (int i = 0; i < 4; i++) { expected[i] = std::exp(adj[i] - top); sum += expected[i]; }
    for (int i = 0; i < 4; i++) expected[i] /= sum;

    double got[4];
    gumbelImprovedPolicy(info, got);
    double total = 0.0;
    for (int i = 0; i < 4; i++) {
        REQUIRE(got[i] == Approx(expected[i]));
        total += got[i];
    }
    REQUIRE(total == Approx(1.0));
}

// ============================================================
// REPLAY BUFFER (pure, no board/model state)
// ============================================================

static GumbelZeroRecord makeMarkedRecord(double marker) {
    GumbelZeroRecord r;
    r.valueTarget = marker;
    r.moveCount = 1;
    r.policyTarget[0] = 1.0;
    for (int i = 0; i < MLM_FEATURES; i++) r.moveFeatures[0][i] = 0.0f;
    for (int i = 0; i < MLV2_FEATURES; i++) r.boardFeatures[i] = 0.0f;
    return r;
}

TEST_CASE("GumbelZeroReplayBuffer - push past capacity evicts the oldest record first") {
    GumbelZeroReplayBuffer buf(3);
    for (int i = 0; i < 5; i++) buf.push(makeMarkedRecord((double)i));   // markers 0..4
    REQUIRE(buf.size() == 3);
    REQUIRE(buf.capacity() == 3);

    std::vector<const GumbelZeroRecord*> all;
    buf.sample(10, all);   // n > size: capped to size, not padded
    REQUIRE(all.size() == 3);
    bool seen[5] = { false, false, false, false, false };
    for (size_t i = 0; i < all.size(); i++) seen[(int)all[i]->valueTarget] = true;
    // The 3 most recently pushed (2,3,4) should have survived; the 2 oldest (0,1) evicted.
    REQUIRE(seen[2]); REQUIRE(seen[3]); REQUIRE(seen[4]);
    REQUIRE_FALSE(seen[0]);
    REQUIRE_FALSE(seen[1]);
}

TEST_CASE("GumbelZeroReplayBuffer - sample below capacity returns distinct, valid records") {
    GumbelZeroReplayBuffer buf(10);
    for (int i = 0; i < 6; i++) buf.push(makeMarkedRecord((double)i));
    REQUIRE(buf.size() == 6);

    std::vector<const GumbelZeroRecord*> out;
    buf.sample(4, out);
    REQUIRE(out.size() == 4);
    bool seen[6] = { false, false, false, false, false, false };
    for (size_t i = 0; i < out.size(); i++) {
        int m = (int)out[i]->valueTarget;
        REQUIRE(m >= 0); REQUIRE(m < 6);
        REQUIRE_FALSE(seen[m]);   // no duplicate ply within one minibatch draw
        seen[m] = true;
    }
}

// ============================================================
// END-TO-END (board-coupled)
// ============================================================

TEST_CASE("GumbelRootInfo::searchValue - diverges from rootValue as simulations run (knob validation)") {
    srand(321);
    LinearModel* value = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    for (int i = 0; i < value->n; i++) value->w[i] = 0.02f * (((i * 13) % 29) - 14);
    LinearModel* policy = new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    for (int i = 0; i < policy->n; i++) policy->w[i] = 0.05f * (((i * 7) % 11) - 5);
    JointModel* jm = new JointModel(value, policy);
    mlSetModel(kScratchSlotGumbelZeroSearch, jm);

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    GumbelRootInfo info1;
    gumbelSearch(White, kScratchSlotGumbelZeroSearch, 1, &info1);

    REQUIRE(reloadBoard("boards\\board1.txt") == true);   // gumbelSearch played a move above; reset
    GumbelRootInfo info200;
    gumbelSearch(White, kScratchSlotGumbelZeroSearch, 200, &info200);

    REQUIRE(std::fabs(info1.searchValue - info200.searchValue) > 1e-6);
    REQUIRE(std::fabs(info200.searchValue - info200.rootValue) > 1e-6);
    mlClearSlots();
}

TEST_CASE("trainGumbelZero - Pass 1 sanity: runs, moves weights off zero-init, checkpoint plays through the real search path") {
    GumbelZeroConfig cfg = gumbelZeroDefaults();
    cfg.outPath        = "build\\test_gz_sanity";
    cfg.boardFile      = "boards\\board1.txt";
    cfg.games          = 6;
    cfg.simBudget      = 8;
    cfg.seed           = 777;
    cfg.openPlies      = 1;
    cfg.replayCapacity = 40;
    cfg.replayWarmup   = 4;
    cfg.batchSize      = 4;
    cfg.reportEvery    = 0;

    REQUIRE(trainGumbelZero(cfg) == 0);

    Model* loaded = loadModel("build\\test_gz_sanity.txt");
    REQUIRE(loaded != nullptr);
    REQUIRE(string(loaded->typeName()) == "joint");
    JointModel* jm = dynamic_cast<JointModel*>(loaded);
    REQUIRE(jm != nullptr);
    LinearModel* v = dynamic_cast<LinearModel*>(jm->valueHead);
    LinearModel* p = dynamic_cast<LinearModel*>(jm->policyHead);
    REQUIRE(v != nullptr);
    REQUIRE(p != nullptr);

    bool anyNonzero = (v->bias != 0.0f) || (p->bias != 0.0f);
    for (int i = 0; i < v->n && !anyNonzero; i++) if (v->w[i] != 0.0f) anyNonzero = true;
    for (int i = 0; i < p->n && !anyNonzero; i++) if (p->w[i] != 0.0f) anyNonzero = true;
    REQUIRE(anyNonzero);   // weights actually moved from the zero-initialized starting point

    mlSetModel(kScratchSlotGumbelZeroLoad, loaded);   // takes ownership
    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    char snap[SIZE][SIZE];
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];
    GumbelRootInfo info;
    int victor = gumbelSearch(White, kScratchSlotGumbelZeroLoad, 8, &info);
    bool moved = false;
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) if (snap[x][y] != board[x][y]) moved = true;
    REQUIRE(moved);
    REQUIRE(victor < WhiteWin);   // the standard opening, not a decided win

    mlClearSlots();
}

TEST_CASE("trainGumbelZero - mlp model-type builds MLP heads for both value and policy, and the checkpoint plays through the real search path") {
    GumbelZeroConfig cfg = gumbelZeroDefaults();
    cfg.outPath        = "build\\test_gz_mlp";
    cfg.boardFile      = "boards\\board1.txt";
    cfg.games          = 6;
    cfg.simBudget      = 8;
    cfg.seed           = 777;
    cfg.openPlies      = 1;
    cfg.replayCapacity = 40;
    cfg.replayWarmup   = 4;
    cfg.batchSize      = 4;
    cfg.reportEvery    = 0;
    cfg.modelType      = "mlp";
    cfg.mlpHidden.push_back(4);   // tiny hidden layer, just enough to exercise the architecture

    REQUIRE(trainGumbelZero(cfg) == 0);

    Model* loaded = loadModel("build\\test_gz_mlp.txt");
    REQUIRE(loaded != nullptr);
    REQUIRE(string(loaded->typeName()) == "joint");
    JointModel* jm = dynamic_cast<JointModel*>(loaded);
    REQUIRE(jm != nullptr);
    MLPModel* v = dynamic_cast<MLPModel*>(jm->valueHead);
    MLPModel* p = dynamic_cast<MLPModel*>(jm->policyHead);
    REQUIRE(v != nullptr);   // architecture actually applied, not silently left linear
    REQUIRE(p != nullptr);
    REQUIRE(v->sizes.size() == 3);
    REQUIRE(v->sizes[0] == MLV2_FEATURES);
    REQUIRE(v->sizes[1] == 4);
    REQUIRE(v->sizes[2] == 1);
    REQUIRE(p->sizes.size() == 3);
    REQUIRE(p->sizes[0] == MLM_FEATURES);
    REQUIRE(p->sizes[1] == 4);
    REQUIRE(p->sizes[2] == 1);

    bool anyNonzero = false;
    for (size_t k = 0; k < v->W.size() && !anyNonzero; k++)
        for (size_t t = 0; t < v->W[k].size() && !anyNonzero; t++)
            if (v->W[k][t] != 0.0f) anyNonzero = true;
    REQUIRE(anyNonzero);   // symmetry-broken by initRandom, not left at zero-init (which could never learn)

    mlSetModel(kScratchSlotGumbelZeroMlpLoad, loaded);   // takes ownership
    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    GumbelRootInfo info;
    int victor = gumbelSearch(White, kScratchSlotGumbelZeroMlpLoad, 8, &info);
    REQUIRE(victor < WhiteWin);   // the standard opening, not a decided win

    mlClearSlots();
}

TEST_CASE("trainGumbelZero - two seeds produce different trained weights (knob validation)") {
    GumbelZeroConfig cfg = gumbelZeroDefaults();
    cfg.boardFile      = "boards\\board1.txt";
    cfg.games          = 6;
    cfg.simBudget      = 8;
    cfg.openPlies      = 1;
    cfg.replayCapacity = 40;
    cfg.replayWarmup   = 4;
    cfg.batchSize      = 4;
    cfg.reportEvery    = 0;

    cfg.outPath = "build\\test_gz_seedA";
    cfg.seed    = 777;
    REQUIRE(trainGumbelZero(cfg) == 0);

    cfg.outPath = "build\\test_gz_seedB";
    cfg.seed    = 778;
    REQUIRE(trainGumbelZero(cfg) == 0);

    Model* a = loadModel("build\\test_gz_seedA.txt");
    Model* b = loadModel("build\\test_gz_seedB.txt");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    JointModel* ja = dynamic_cast<JointModel*>(a);
    JointModel* jb = dynamic_cast<JointModel*>(b);
    REQUIRE(ja != nullptr);
    REQUIRE(jb != nullptr);
    LinearModel* va = dynamic_cast<LinearModel*>(ja->valueHead);
    LinearModel* vb = dynamic_cast<LinearModel*>(jb->valueHead);
    REQUIRE(va != nullptr);
    REQUIRE(vb != nullptr);

    bool differ = (va->bias != vb->bias);
    for (int i = 0; i < va->n && !differ; i++) if (va->w[i] != vb->w[i]) differ = true;
    REQUIRE(differ);

    delete a;
    delete b;
}
