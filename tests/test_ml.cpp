#include "catch.hpp"
#include "helpers.h"
#include "ml_features.h"
#include "ml_model.h"
#include "ml_eval.h"
#include "explorers.h"
#include "choosers.h"
#include "agents.h"
#include "ml_train.h"
#include "ai_eval.h"

// Find a registry entry by name (small helper for the tests).
static int explorerIdx(const char* n) { for (int i=0;i<g_explorerCount;i++) if (string(g_explorers[i].name)==n) return i; return 0; }
static int chooserIdx(const char* n)  { for (int i=0;i<g_chooserCount;i++)  if (string(g_choosers[i].name)==n)  return i; return 0; }
static int evalIdx(const char* n)     { for (int i=0;i<g_evalCount;i++)     if (string(g_evaluators[i].name)==n) return i; return 0; }

TEST_CASE("features - deterministic and correct size") {
    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    float a[MLV_FEATURES], b[MLV_FEATURES];
    mlExtractValueFeatures(White, a);
    mlExtractValueFeatures(White, b);
    REQUIRE(mlValueFeatureCount() == MLV_FEATURES);
    bool same = true;
    for (int i = 0; i < MLV_FEATURES; i++) if (a[i] != b[i]) same = false;
    REQUIRE(same);
}

TEST_CASE("LinearModel - forward equals manual dot product") {
    LinearModel m(HEAD_VALUE, 1, 3, 900.0f);
    m.w[0] = 1.0f; m.w[1] = 2.0f; m.w[2] = 3.0f; m.bias = 0.5f;
    float x[3] = { 0.1f, 0.2f, 0.3f };
    REQUIRE(m.forward(x, 3) == Approx(0.5f + 0.1f + 0.4f + 0.9f));
}

TEST_CASE("LinearModel - save/load round trip") {
    LinearModel m(HEAD_VALUE, 1, 4, 900.0f);
    m.bias = -1.25f;
    for (int i = 0; i < 4; i++) m.w[i] = 0.5f * (i + 1);
    REQUIRE(m.save("build\\test_model.tmp"));

    Model* loaded = loadModel("build\\test_model.tmp");
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->featureCount() == 4);
    REQUIRE(loaded->head() == HEAD_VALUE);
    float x[4] = { 0.2f, -0.1f, 0.4f, 0.05f };
    REQUIRE(loaded->forward(x, 4) == Approx(m.forward(x, 4)));
    delete loaded;
}

TEST_CASE("LinearModel - logistic SGD reduces loss") {
    LinearModel m(HEAD_VALUE, 1, 3, 900.0f);
    float x[3] = { 0.5f, -0.2f, 0.3f };
    float first = m.sgdLogisticStep(x, 3, 1.0f, 0.5f);
    float last = first;
    for (int i = 0; i < 200; i++) last = m.sgdLogisticStep(x, 3, 1.0f, 0.5f);
    REQUIRE(last < first);
}

TEST_CASE("mlValueScore - stays inside the win sentinels") {
    clearBoard();
    board[2][2] = WHITE; board[5][5] = BLACK;   // not a decided position
    LinearModel* m = new LinearModel(HEAD_VALUE, mlValueFeatureVersion(), MLV_FEATURES, 900.0f);
    for (int i = 0; i < m->n; i++) m->w[i] = 0.3f;   // arbitrary nonzero weights
    mlSetModel(0, m);
    int s = mlValueScore(White, 0);
    REQUIRE(s < WhiteWin - 1024);
    REQUIRE(s > BlackWin + 1024);
    mlClearSlots();
}

TEST_CASE("Greedy explorer - takes an immediate winning move") {
    clearBoard();
    board[3][SIZE-2] = WHITE;          // one step from the goal row
    int params[MAX_EVAL_PARAMS] = { 1, 4, 0, 0 };
    int victor = g_explorers[explorerIdx("Greedy")].fn(White, evalIdx("Classic"), params, 1);
    REQUIRE(victor == WhiteWin);
}

TEST_CASE("LearnedPolicy chooser - plays a legal move") {
    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    LinearModel* p = new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    for (int i = 0; i < p->n; i++) p->w[i] = 0.2f;
    mlSetModel(1, p);

    char snap[SIZE][SIZE];
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];
    int victor = g_choosers[chooserIdx("LearnedPolicy")].fn(White, 1, 0);
    bool moved = false;
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) if (snap[x][y] != board[x][y]) moved = true;
    REQUIRE(moved);
    REQUIRE(victor < WhiteWin);   // a normal opening move, not a win
    mlClearSlots();
}

TEST_CASE("Elo - expected score and monotonic update") {
    REQUIRE(eloExpected(1500, 1500) == Approx(0.5));
    REQUIRE(eloExpected(1900, 1500) > 0.5);
    double ra = 1500, rb = 1500;
    for (int i = 0; i < 20; i++) eloUpdate(ra, rb, 1.0, 24.0);  // A keeps winning
    REQUIRE(ra > rb);
    REQUIRE(ra > 1500.0);
}
