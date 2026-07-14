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
#include "ranking.h"
#include <cmath>
#include <cstdlib>

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

// L2 (0 = off, the historical behavior) should pull weight magnitude down
// relative to unregularized SGD on the identical data, without changing the
// direction both are pushed (same target, same inputs).
TEST_CASE("LinearModel - L2 regularization shrinks weight magnitude") {
    LinearModel plain(HEAD_VALUE, 1, 3, 900.0f);
    LinearModel reg(HEAD_VALUE, 1, 3, 900.0f);
    float x[3] = { 1.0f, 1.0f, 1.0f };
    for (int i = 0; i < 3; i++) { plain.w[i] = 0.5f; reg.w[i] = 0.5f; }
    for (int i = 0; i < 200; i++) {
        plain.sgdLogisticStep(x, 3, 1.0f, 0.1f, 0.0f);
        reg.sgdLogisticStep(x, 3, 1.0f, 0.1f, 0.05f);
    }
    REQUIRE(std::fabs(reg.w[0]) < std::fabs(plain.w[0]));
}

// End-to-end: rank.exe's "extract" replays sampled historical matches from the
// real, committed ranking/matches.jsonl (the existing diverse, already-rated
// agent pool) into a labeled dataset, and trainSupervisedValue's --from-data
// fits a value model on it without any fresh self-play. Skips gracefully if the
// store is missing (a fresh checkout with no ranking history yet).
TEST_CASE("rank.exe extract + trainSupervisedValue --from-data round trip") {
    std::ifstream probe("ranking/matches.jsonl");
    if (!probe.is_open()) {
        SUCCEED("ranking/matches.jsonl not present; extract/replay test skipped");
        return;
    }
    probe.close();

    const string outFile = "build/test_replay.jsonl";
    int rc = rankExtract("ranking/matches.jsonl", outFile, "boards/board1.txt", 2, 40, 12345u);
    REQUIRE(rc == 0);

    std::ifstream check(outFile);
    REQUIRE(check.is_open());
    string firstLine;
    REQUIRE(std::getline(check, firstLine));
    REQUIRE(firstLine.find("\"ver\":2") != string::npos);
    check.close();

    int trc = trainSupervisedValue("build/test_replay_model", "boards/board1.txt", 0, 3, 0.05, 1,
                                   0, 0.0, 1u, "Classic", {}, 2, 0.0, 0.0, 0, "", "alphabeta", outFile);
    REQUIRE(trc == 0);

    Model* m = loadModel("build/test_replay_model.txt");
    REQUIRE(m != nullptr);
    REQUIRE(m->featureVersion() == 2);
    REQUIRE(m->featureCount() == MLV2_FEATURES);
    delete m;
}

// ============================================================
// Sparse piece-square features (v2) + the incremental ML accumulator
// ============================================================

// Deterministic v2 value model: distinct nonzero weights so a wrong index in a
// delta update shifts the accumulator by a detectable amount.
static LinearModel* makeV2Model() {
    LinearModel* m = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    m->bias = 0.125f;
    for (int i = 0; i < m->n; i++) m->w[i] = 0.001f * ((i * 37) % 101) - 0.05f;
    return m;
}

// From-scratch reference for g_mlAcc: bias + weights of the occupied
// piece-squares (side-to-move excluded, it is applied at read time).
static double mlAccFull(const LinearModel* m) {
    double acc = m->bias;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            char c = board[x][y];
            if (c == WHITE)      acc += m->w[mlSqW(x, y)];
            else if (c == BLACK) acc += m->w[mlSqB(x, y)];
        }
    return acc;
}

// Walk every legal move to `depth` through the engine's own make/unmake, counting
// accumulator drift (vs mlAccFull) and incremental-vs-full-scan leaf disagreement
// larger than 1 (the full scan sums floats, the accumulator sums doubles, so the
// rounded evals may differ by at most one integer step). Counters instead of
// REQUIREs to keep Catch assertion counts sane over thousands of nodes.
static int g_mlWalkAccMismatch = 0;
static int g_mlWalkLeafMismatch = 0;
static void mlWalkCheck(const LinearModel* m, int slot, const int* params) {
    if (std::fabs(g_mlAcc - mlAccFull(m)) > 1e-4) g_mlWalkAccMismatch++;
    int lvIdx = evalIdx("LearnedValue");
    for (int t = 0; t < 2; t++) {
        int turn = (t == 0) ? White : Black;
        long long inc  = evalLeaf(turn, lvIdx, params);
        long long full = mlValueScore(turn, slot);
        if (inc - full > 1 || full - inc > 1) g_mlWalkLeafMismatch++;
    }
}
static void mlWalk(const LinearModel* m, int slot, const int* params, int color, int depth) {
    mlWalkCheck(m, slot, params);
    if (depth <= 0) return;
    if (color == White) {
        for (int y = 0; y < SIZE-1; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != WHITE) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (!tryMoveQuickWhite(x, y, z)) continue;
                    bool cap = simulateMoveWhite(x, y, z);
                    mlWalk(m, slot, params, Black, depth-1);
                    unsimulateMoveWhite(x, y, z, cap);
                    if (std::fabs(g_mlAcc - mlAccFull(m)) > 1e-4) g_mlWalkAccMismatch++;
                }
            }
    } else {
        for (int y = 1; y < SIZE; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != BLACK) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (!tryMoveQuickBlack(x, y, z)) continue;
                    bool cap = simulateMoveBlack(x, y, z);
                    mlWalk(m, slot, params, White, depth-1);
                    unsimulateMoveBlack(x, y, z, cap);
                    if (std::fabs(g_mlAcc - mlAccFull(m)) > 1e-4) g_mlWalkAccMismatch++;
                }
            }
    }
}

TEST_CASE("value features v2 - sparse piece-square layout") {
    clearBoard();
    board[0][0] = WHITE; board[3][4] = WHITE; board[7][7] = BLACK; board[2][5] = BLACK;
    float f[MLV2_FEATURES];
    mlExtractValueFeaturesV2(White, f);

    REQUIRE(f[mlSqW(0, 0)] == 1.0f);
    REQUIRE(f[mlSqW(3, 4)] == 1.0f);
    REQUIRE(f[mlSqB(7, 7)] == 1.0f);
    REQUIRE(f[mlSqB(2, 5)] == 1.0f);
    REQUIRE(f[MLV2_STM] == 1.0f);
    int nonzero = 0;
    for (int i = 0; i < MLV2_FEATURES; i++) if (f[i] != 0.0f) nonzero++;
    REQUIRE(nonzero == 5);   // 4 pieces + side to move

    mlExtractValueFeaturesV2(Black, f);
    REQUIRE(f[MLV2_STM] == -1.0f);

    REQUIRE(string(mlValueFeatureNameV2(mlSqW(0, 0))) == "w_a1");
    REQUIRE(string(mlValueFeatureNameV2(mlSqB(7, 7))) == "b_h8");
    REQUIRE(string(mlValueFeatureNameV2(MLV2_STM)) == "side_to_move");
}

TEST_CASE("LinearModel v2 - save/load keeps the feature version") {
    LinearModel* m = makeV2Model();
    REQUIRE(m->save("build\\test_model_v2.tmp"));
    Model* loaded = loadModel("build\\test_model_v2.tmp");
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->featureVersion() == 2);
    REQUIRE(loaded->featureCount() == MLV2_FEATURES);
    delete loaded;
    delete m;
}

TEST_CASE("incremental ML accumulator matches full recompute over make/unmake walk") {
    int lvIdx = evalIdx("LearnedValue");
    int params[MAX_EVAL_PARAMS] = { 0 };   // p[0] = model slot 0

    SECTION("crafted position: captures near both edges") {
        clearBoard();
        board[0][4] = WHITE; board[1][4] = WHITE; board[0][3] = WHITE;
        board[6][4] = WHITE; board[7][4] = WHITE; board[7][3] = WHITE;
        board[0][5] = BLACK; board[1][5] = BLACK; board[2][5] = BLACK;
        board[6][5] = BLACK; board[7][5] = BLACK; board[2][4] = BLACK;

        LinearModel* m = makeV2Model();
        mlSetModel(0, m);   // slot takes ownership
        evalBeginSearch(lvIdx, params);
        REQUIRE(g_mlIncremental);
        REQUIRE(g_mlAcc == Approx(mlAccFull(m)).margin(1e-6));
        g_mlWalkAccMismatch = 0;
        g_mlWalkLeafMismatch = 0;
        mlWalk(m, 0, params, White, 3);
        mlWalk(m, 0, params, Black, 3);
        evalEndSearch();
        REQUIRE(g_mlWalkAccMismatch == 0);
        REQUIRE(g_mlWalkLeafMismatch == 0);
        REQUIRE(g_mlIncremental == false);
        mlClearSlots();
    }

    SECTION("dense standard position") {
        REQUIRE(reloadBoard("boards\\board1.txt") == true);
        LinearModel* m = makeV2Model();
        mlSetModel(0, m);
        evalBeginSearch(lvIdx, params);
        REQUIRE(g_mlIncremental);
        g_mlWalkAccMismatch = 0;
        g_mlWalkLeafMismatch = 0;
        mlWalk(m, 0, params, White, 2);
        evalEndSearch();
        REQUIRE(g_mlWalkAccMismatch == 0);
        REQUIRE(g_mlWalkLeafMismatch == 0);
        mlClearSlots();
    }
}

TEST_CASE("incremental ML path stays off when it does not apply") {
    int lvIdx = evalIdx("LearnedValue");
    int params[MAX_EVAL_PARAMS] = { 0 };
    REQUIRE(reloadBoard("boards\\board1.txt") == true);

    SECTION("v1 model: LearnedValue falls back to the full scan") {
        LinearModel* m = new LinearModel(HEAD_VALUE, 1, MLV_FEATURES, 900.0f);
        for (int i = 0; i < m->n; i++) m->w[i] = 0.1f;
        mlSetModel(0, m);
        evalBeginSearch(lvIdx, params);
        REQUIRE(g_mlIncremental == false);
        // Fallback leaf path = the full evaluator, exactly.
        REQUIRE(evalLeaf(White, lvIdx, params) == mlValueScore(White, 0));
        evalEndSearch();
        mlClearSlots();
    }

    SECTION("heuristic evaluator: v2 model in a slot must not enable the ML path") {
        LinearModel* m = makeV2Model();
        mlSetModel(0, m);
        int classic[MAX_EVAL_PARAMS] = { 1, 4, 2, 3 };
        evalBeginSearch(evalIdx("Classic"), classic);
        REQUIRE(g_mlIncremental == false);
        REQUIRE(evalLeaf(White, evalIdx("Classic"), classic)
                == evaluateBoard(White, evalIdx("Classic"), classic));
        evalEndSearch();
        mlClearSlots();
    }
}

// ============================================================
// MLP model + residual (chip-count skip) value head
// ============================================================

TEST_CASE("MLP forward matches a hand-computed 2-2-1 net") {
    std::vector<int> hidden; hidden.push_back(2);
    MLPModel m(HEAD_VALUE, 1, 2, 900.0f, hidden);   // sizes = [2,2,1]
    // layer 0 (2->2), output-major idx = j*in + i
    m.W[0][0] = 1.0f; m.W[0][1] = 0.0f;   // h0_pre = x0
    m.W[0][2] = 0.0f; m.W[0][3] = 1.0f;   // h1_pre = x1
    m.B[0][0] = 0.0f; m.B[0][1] = 0.0f;
    // layer 1 (2->1)
    m.W[1][0] = 2.0f; m.W[1][1] = 3.0f; m.B[1][0] = 0.5f;
    float x[2] = { 1.0f, -1.0f };          // h_pre = [1,-1] -> ReLU [1,0]
    REQUIRE(m.forward(x, 2) == Approx(0.5f + 2.0f * 1.0f + 3.0f * 0.0f));  // 2.5
}

// Gold-standard safety net for the hand-written backprop: one trainStep applies the
// analytic gradient (delta = -lr*grad at l2=0), which must match a central finite
// difference of the loss for every weight and bias.
TEST_CASE("MLP backprop gradient matches finite differences") {
    srand(123);
    std::vector<int> hidden; hidden.push_back(4);
    MLPModel m(HEAD_VALUE, 1, 3, 1.0f, hidden);     // sizes = [3,4,1]
    m.initRandom();
    float x[3] = { 0.5f, -0.3f, 0.8f };
    float target = 1.0f;
    const float lr = 1.0f;

    std::vector<std::vector<float> > Wsnap = m.W, Bsnap = m.B;
    m.trainStep(x, 3, target, lr, 0.0f, 0.0f);       // computes grads at the snapshot, updates in place
    std::vector<std::vector<float> > Wafter = m.W, Bafter = m.B;

    auto lossNow = [&]() {
        double z = m.forward(x, 3);
        double p = 1.0 / (1.0 + std::exp(-z));
        return -(target * std::log(p + 1e-7) + (1.0 - target) * std::log(1.0 - p + 1e-7));
    };
    const float eps = 1e-3f;
    int mism = 0;
    for (size_t k = 0; k < m.W.size(); k++) {
        for (size_t t = 0; t < m.W[k].size(); t++) {
            double analytic = (Wsnap[k][t] - Wafter[k][t]) / lr;
            m.W = Wsnap; m.B = Bsnap;
            m.W[k][t] = Wsnap[k][t] + eps; double lp = lossNow();
            m.W[k][t] = Wsnap[k][t] - eps; double lm = lossNow();
            double numeric = (lp - lm) / (2.0 * eps);
            if (std::fabs(analytic - numeric) > 1e-2 + 1e-2 * std::fabs(numeric)) mism++;
        }
        for (size_t t = 0; t < m.B[k].size(); t++) {
            double analytic = (Bsnap[k][t] - Bafter[k][t]) / lr;
            m.W = Wsnap; m.B = Bsnap;
            m.B[k][t] = Bsnap[k][t] + eps; double lp = lossNow();
            m.B[k][t] = Bsnap[k][t] - eps; double lm = lossNow();
            double numeric = (lp - lm) / (2.0 * eps);
            if (std::fabs(analytic - numeric) > 1e-2 + 1e-2 * std::fabs(numeric)) mism++;
        }
    }
    m.W = Wsnap; m.B = Bsnap;
    REQUIRE(mism == 0);
}

TEST_CASE("MLP backprop reduces loss on a fixed example") {
    srand(7);
    std::vector<int> hidden; hidden.push_back(6);
    MLPModel m(HEAD_VALUE, 1, 3, 1.0f, hidden);
    m.initRandom();
    float x[3] = { 0.4f, -0.2f, 0.6f };
    float first = m.trainStep(x, 3, 1.0f, 0.3f, 0.0f, 0.0f);
    float last = first;
    for (int i = 0; i < 300; i++) last = m.trainStep(x, 3, 1.0f, 0.3f, 0.0f, 0.0f);
    REQUIRE(last < first);
}

TEST_CASE("MLP save/load round trip") {
    srand(99);
    std::vector<int> hidden; hidden.push_back(5);
    MLPModel* m = new MLPModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, hidden);
    m->initRandom();
    REQUIRE(m->save("build\\test_mlp.tmp"));
    Model* loaded = loadModel("build\\test_mlp.tmp");
    REQUIRE(loaded != nullptr);
    REQUIRE(string(loaded->typeName()) == "mlp");
    REQUIRE(loaded->featureVersion() == 2);
    REQUIRE(loaded->featureCount() == MLV2_FEATURES);

    float f[MLV2_FEATURES];
    clearBoard();
    board[1][1] = WHITE; board[4][4] = WHITE; board[6][6] = BLACK; board[3][2] = BLACK;
    mlExtractValueFeaturesV2(White, f);
    REQUIRE(loaded->forward(f, MLV2_FEATURES) == Approx(m->forward(f, MLV2_FEATURES)).margin(1e-3));
    delete loaded;
    delete m;
}

TEST_CASE("ResidualModel forward = skip*matDiff + inner (linear and mlp inner)") {
    clearBoard();
    board[0][0] = WHITE; board[3][4] = WHITE; board[5][5] = WHITE; board[7][7] = BLACK;  // matDiff = 3-1 = 2
    float f[MLV2_FEATURES];
    mlExtractValueFeaturesV2(White, f);
    REQUIRE(matDiffFromFeatures(f, 2) == Approx(2.0f));

    SECTION("linear inner") {
        LinearModel* inner = makeV2Model();
        float base = inner->forward(f, MLV2_FEATURES);
        ResidualModel res(0.4f, 2, inner);
        REQUIRE(res.forward(f, MLV2_FEATURES) == Approx(0.4f * 2.0f + base));
        // ResidualModel owns inner; do not double free (stack object frees on scope exit)
    }
    SECTION("mlp inner") {
        srand(3);
        std::vector<int> hidden; hidden.push_back(4);
        MLPModel* inner = new MLPModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, hidden);
        inner->initRandom();
        float base = inner->forward(f, MLV2_FEATURES);
        ResidualModel res(0.4f, 2, inner);
        REQUIRE(res.forward(f, MLV2_FEATURES) == Approx(0.4f * 2.0f + base));
    }
}

// A hard chip skip over a LINEAR inner is representationally a plain linear model
// with the skip folded into the piece-square weights (+skip on white squares,
// -skip on black). The two must produce identical evals -- validates the skip
// inference against the existing linear machinery.
TEST_CASE("Residual+linear equals the fold into a plain linear model") {
    const float skipW = 0.35f;
    LinearModel* inner = makeV2Model();
    // Folded plain linear: w_white += skip, w_black -= skip, STM + bias unchanged.
    LinearModel* folded = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    folded->bias = inner->bias;
    for (int i = 0; i < MLV2_FEATURES; i++) folded->w[i] = inner->w[i];
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            folded->w[mlSqW(x, y)] += skipW;
            folded->w[mlSqB(x, y)] -= skipW;
        }
    ResidualModel* res = new ResidualModel(skipW, 2, inner);

    mlSetModel(0, res);      // slot owns res (and its inner)
    mlSetModel(1, folded);   // slot owns folded
    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    for (int t = 0; t < 2; t++) {
        int turn = (t == 0) ? White : Black;
        REQUIRE(std::abs(mlValueScore(turn, 0) - mlValueScore(turn, 1)) <= 1);
    }
    clearBoard();
    board[2][2] = WHITE; board[4][3] = WHITE; board[5][6] = BLACK;
    for (int t = 0; t < 2; t++) {
        int turn = (t == 0) ? White : Black;
        REQUIRE(std::abs(mlValueScore(turn, 0) - mlValueScore(turn, 1)) <= 1);
    }
    mlClearSlots();
}

TEST_CASE("ResidualModel save/load round trip (linear and mlp inner)") {
    SECTION("linear inner") {
        LinearModel* inner = makeV2Model();
        ResidualModel* res = new ResidualModel(0.42f, 2, inner);
        REQUIRE(res->save("build\\test_res_lin.tmp"));
        Model* loaded = loadModel("build\\test_res_lin.tmp");
        REQUIRE(loaded != nullptr);
        REQUIRE(string(loaded->typeName()) == "residual");
        REQUIRE(loaded->featureVersion() == 2);
        ResidualModel* rl = dynamic_cast<ResidualModel*>(loaded);
        REQUIRE(rl != nullptr);
        REQUIRE(rl->skipW == Approx(0.42f));
        REQUIRE(string(rl->inner->typeName()) == "linear");
        float f[MLV2_FEATURES];
        clearBoard(); board[1][2] = WHITE; board[6][5] = BLACK;
        mlExtractValueFeaturesV2(Black, f);
        REQUIRE(loaded->forward(f, MLV2_FEATURES) == Approx(res->forward(f, MLV2_FEATURES)).margin(1e-3));
        delete loaded; delete res;
    }
    SECTION("mlp inner") {
        srand(5);
        std::vector<int> hidden; hidden.push_back(6);
        MLPModel* inner = new MLPModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, hidden);
        inner->initRandom();
        ResidualModel* res = new ResidualModel(0.30f, 2, inner);
        REQUIRE(res->save("build\\test_res_mlp.tmp"));
        Model* loaded = loadModel("build\\test_res_mlp.tmp");
        REQUIRE(loaded != nullptr);
        REQUIRE(string(loaded->typeName()) == "residual");
        ResidualModel* rl = dynamic_cast<ResidualModel*>(loaded);
        REQUIRE(rl != nullptr);
        REQUIRE(rl->skipW == Approx(0.30f));
        REQUIRE(string(rl->inner->typeName()) == "mlp");
        float f[MLV2_FEATURES];
        clearBoard(); board[2][3] = WHITE; board[3][3] = WHITE; board[5][4] = BLACK;
        mlExtractValueFeaturesV2(White, f);
        REQUIRE(loaded->forward(f, MLV2_FEATURES) == Approx(res->forward(f, MLV2_FEATURES)).margin(1e-3));
        delete loaded; delete res;
    }
}

TEST_CASE("Residual+linear v2: incremental leaf matches full recompute over a walk") {
    int lvIdx = evalIdx("LearnedValue");
    int params[MAX_EVAL_PARAMS] = { 0 };
    LinearModel* inner = makeV2Model();
    ResidualModel* res = new ResidualModel(0.3f, 2, inner);
    mlSetModel(0, res);   // slot owns res + inner; `inner` pointer stays valid for mlAccFull

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    evalBeginSearch(lvIdx, params);
    REQUIRE(g_mlIncremental);                          // linear inner -> incremental on
    REQUIRE(g_mlAcc == Approx(mlAccFull(inner)).margin(1e-6));   // accumulator tracks the INNER logit
    g_mlWalkAccMismatch = 0; g_mlWalkLeafMismatch = 0;
    mlWalk(inner, 0, params, White, 2);               // leaf compares evalLeaf (skip incl.) vs mlValueScore (skip incl.)
    evalEndSearch();
    REQUIRE(g_mlWalkAccMismatch == 0);
    REQUIRE(g_mlWalkLeafMismatch == 0);
    mlClearSlots();
}

TEST_CASE("Residual+MLP v2: incremental stays off, full-scan leaf is used") {
    int lvIdx = evalIdx("LearnedValue");
    int params[MAX_EVAL_PARAMS] = { 0 };
    srand(11);
    std::vector<int> hidden; hidden.push_back(8);
    MLPModel* inner = new MLPModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, hidden);
    inner->initRandom();
    ResidualModel* res = new ResidualModel(0.3f, 2, inner);
    mlSetModel(0, res);

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    evalBeginSearch(lvIdx, params);
    REQUIRE(g_mlIncremental == false);                 // MLP inner has no linear accumulator
    REQUIRE(evalLeaf(White, lvIdx, params) == mlValueScore(White, 0));   // fallback == full scan
    evalEndSearch();
    int s = mlValueScore(White, 0);
    REQUIRE(s < WhiteWin - 1024);
    REQUIRE(s > BlackWin + 1024);
    mlClearSlots();
}

TEST_CASE("trainStep with a frozen offset still reduces loss") {
    SECTION("linear") {
        LinearModel m(HEAD_VALUE, 1, 3, 900.0f);
        float x[3] = { 0.5f, -0.2f, 0.3f };
        float first = m.sgdLogisticStep(x, 3, 1.0f, 0.5f, 0.0f, 2.0f);   // offset = 2
        float last = first;
        for (int i = 0; i < 200; i++) last = m.sgdLogisticStep(x, 3, 1.0f, 0.5f, 0.0f, 2.0f);
        REQUIRE(last < first);
    }
    SECTION("mlp") {
        srand(21);
        std::vector<int> hidden; hidden.push_back(5);
        MLPModel m(HEAD_VALUE, 1, 3, 1.0f, hidden);
        m.initRandom();
        float x[3] = { 0.5f, -0.2f, 0.3f };
        float first = m.trainStep(x, 3, 1.0f, 0.3f, 0.0f, -1.5f);
        float last = first;
        for (int i = 0; i < 300; i++) last = m.trainStep(x, 3, 1.0f, 0.3f, 0.0f, -1.5f);
        REQUIRE(last < first);
    }
}

// End-to-end: a validation split + early stopping runs to completion and produces a
// loadable model (exercises the train/val partition, per-epoch val loss, and the
// early-stop best-checkpoint reload path).
TEST_CASE("trainSupervisedValue --val-split + --early-stop completes and saves") {
    int rc = trainSupervisedValue("build/test_valsplit_model", "boards/board1.txt",
                                  4,          // games (tiny self-play)
                                  3,          // epochs
                                  0.05,       // lr
                                  3,          // ckptEvery
                                  2,          // genDepth
                                  0.2,        // genRandom
                                  777u,       // seed
                                  "Classic", {}, 2, 0.0, 0.0, 0, "", "alphabeta", "",
                                  "linear", {}, 0.0,
                                  0.25,       // valSplit
                                  true);      // earlyStop
    REQUIRE(rc == 0);
    Model* m = loadModel("build/test_valsplit_model.txt");
    REQUIRE(m != nullptr);
    REQUIRE(m->featureVersion() == 2);
    delete m;
    mlClearSlots();
}
