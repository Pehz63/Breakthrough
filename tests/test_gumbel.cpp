#include "catch.hpp"
#include "helpers.h"
#include "ai_gumbel.h"
#include "ml_features.h"
#include "ml_model.h"
#include "ml_eval.h"
#include "ai_eval.h"
#include "explorers.h"
#include "agents.h"
#include <cmath>
#include <cstdlib>

// Small local lookup helpers, mirroring test_ml.cpp's own copies. Returns -1
// (rather than silently falling back to index 0) so a missing registration is
// a loud test failure, not a false pass against Greedy.
static int findExplorer(const char* n) {
    for (int i = 0; i < g_explorerCount; i++) if (string(g_explorers[i].name) == n) return i;
    return -1;
}
static int evalIdx(const char* n) {
    for (int i = 0; i < g_evalCount; i++) if (string(g_evaluators[i].name) == n) return i;
    return 0;
}

// ============================================================
// PURE CORE MATH (no board state)
// ============================================================

TEST_CASE("gumbelSigma - matches its formula at known inputs") {
    REQUIRE(gumbelSigma(0.5, 10, 50.0, 1.0) == Approx((50.0 + 10) * 1.0 * 0.5));
    REQUIRE(gumbelSigma(-0.2, 0, 50.0, 2.0) == Approx((50.0 + 0) * 2.0 * -0.2));
    REQUIRE(gumbelSigma(0.0, 100, 50.0, 1.0) == Approx(0.0));
}

TEST_CASE("gumbelSelectAction - matches an independent reference computation") {
    double logits[4] = { 0.3, -0.2, 0.5, 0.1 };
    double q[4]      = { 0.05, 0.2, -0.1, 0.0 };
    int visits[4]    = { 3, 1, 0, 2 };
    const double cVisit = 50.0, cScale = 1.0;

    // Independent reference: recompute softmax(logits + sigma(q)) and the
    // visit-fraction-matching score by hand, not via any shared helper.
    int maxN = 3; long long sumN = 6;
    double adj[4], top = -1e300;
    for (int i = 0; i < 4; i++) {
        adj[i] = logits[i] + (cVisit + maxN) * cScale * q[i];
        if (adj[i] > top) top = adj[i];
    }
    double sum = 0.0, p[4];
    for (int i = 0; i < 4; i++) { p[i] = std::exp(adj[i] - top); sum += p[i]; }
    int expected = 0; double best = -1e300;
    for (int i = 0; i < 4; i++) {
        double score = p[i] / sum - (double)visits[i] / (1.0 + (double)sumN);
        if (score > best) { best = score; expected = i; }
    }

    int got = gumbelSelectAction(logits, q, visits, 4, cVisit, cScale);
    REQUIRE(got == expected);
}

TEST_CASE("gumbelSelectAction - with no visits yet, picks the highest logit+sigma(Q) action") {
    double logits[3] = { 0.0, 1.0, -1.0 };
    double q[3]      = { 0.0, 0.0, 0.0 };
    int visits[3]    = { 0, 0, 0 };
    REQUIRE(gumbelSelectAction(logits, q, visits, 3, 50.0, 1.0) == 1);
}

TEST_CASE("gumbelSelectAction - concentrates visits on the highest-completedQ action over many draws") {
    double logits[3] = { 0.0, 0.0, 0.0 };   // equal priors, isolates the completedQ effect
    double q[3]      = { 0.0, 0.3, -0.2 };  // action 1 is clearly best
    int visits[3]    = { 0, 0, 0 };
    for (int t = 0; t < 500; t++) {
        int sel = gumbelSelectAction(logits, q, visits, 3, 50.0, 1.0);
        visits[sel]++;
    }
    REQUIRE(visits[1] > visits[0]);
    REQUIRE(visits[1] > visits[2]);
    REQUIRE(visits[1] > 250);   // action 1 should dominate as maxVisitCount grows
}

TEST_CASE("gumbelTopK - returns min(k,n) distinct valid indices") {
    srand(42);
    double logits[5] = { 0.1, 0.5, -0.2, 0.3, 0.0 };
    int out[5];
    int got = gumbelTopK(logits, 5, 3, out);
    REQUIRE(got == 3);
    bool seen[5] = { false, false, false, false, false };
    for (int i = 0; i < got; i++) {
        REQUIRE(out[i] >= 0);
        REQUIRE(out[i] < 5);
        REQUIRE_FALSE(seen[out[i]]);
        seen[out[i]] = true;
    }
}

TEST_CASE("gumbelTopK - clamps k to n and handles a single action") {
    double logits[2] = { 0.0, 1.0 };
    int out[2];
    REQUIRE(gumbelTopK(logits, 2, 5, out) == 2);   // k > n clamps to n
    REQUIRE(gumbelTopK(logits, 1, 3, out) == 1);
    REQUIRE(out[0] == 0);
    REQUIRE(gumbelTopK(logits, 0, 3, out) == 0);
}

TEST_CASE("gumbelTopK - a much larger logit is picked far more often (validates the sampling actually uses the logits)") {
    srand(7);
    double logits[2] = { 50.0, -50.0 };
    int out[1];
    int winsA = 0;
    for (int t = 0; t < 200; t++) {
        gumbelTopK(logits, 2, 1, out);
        if (out[0] == 0) winsA++;
    }
    REQUIRE(winsA > 190);
}

TEST_CASE("gumbelHalvingRounds - narrows m candidates to 1 in ceil(log2(m)) rounds") {
    int sims[16];
    REQUIRE(gumbelHalvingRounds(1, 100, sims) == 1);
    REQUIRE(gumbelHalvingRounds(2, 100, sims) == 1);
    REQUIRE(gumbelHalvingRounds(3, 100, sims) == 2);
    REQUIRE(gumbelHalvingRounds(4, 100, sims) == 2);
    REQUIRE(gumbelHalvingRounds(16, 400, sims) == 4);
}

TEST_CASE("gumbelHalvingRounds - per-round sim counts are always at least 1, even under a tiny budget") {
    int sims[16];
    int rounds = gumbelHalvingRounds(16, 8, sims);   // fewer sims than candidates
    for (int r = 0; r < rounds; r++) REQUIRE(sims[r] >= 1);
}

// ============================================================
// END-TO-END (board-coupled)
// ============================================================

TEST_CASE("GumbelMCTS explorer - takes an immediate winning move") {
    clearBoard();
    board[3][SIZE - 2] = WHITE;   // one step from the goal row
    int params[MAX_EVAL_PARAMS] = { 0, 0, 0, 0 };   // the immediate-win shortcut needs no model
    int gaz = findExplorer("GumbelMCTS");
    REQUIRE(gaz >= 0);
    int victor = g_explorers[gaz].fn(White, evalIdx("LearnedValue"), params, 8);
    REQUIRE(victor == WhiteWin);
}

TEST_CASE("gumbelSearch - GumbelRootInfo reports sane root statistics") {
    srand(55);
    LinearModel* value = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    LinearModel* policy = new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    for (int i = 0; i < policy->n; i++) policy->w[i] = 0.1f * (((i * 3) % 5) - 2);
    JointModel* jm = new JointModel(value, policy);
    mlSetModel(702, jm);

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    Move legal[ML_MAX_MOVES];
    int nLegal = generateMoves(White, legal);

    GumbelRootInfo info;
    gumbelSearch(White, 702, 32, &info);
    REQUIRE(info.moveCount == nLegal);
    int totalVisits = 0;
    for (int i = 0; i < info.moveCount; i++) {
        REQUIRE(info.completedQ[i] >= -1.0 - 1e-9);
        REQUIRE(info.completedQ[i] <=  1.0 + 1e-9);
        REQUIRE(info.visitCounts[i] >= 0);
        totalVisits += info.visitCounts[i];
    }
    REQUIRE(totalVisits > 0);
    mlClearSlots();
}

TEST_CASE("GumbelMCTS - plays a full legal game to completion with a random-weight joint model") {
    srand(123);
    LinearModel* value = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    for (int i = 0; i < value->n; i++) value->w[i] = 0.01f * (((i * 17) % 41) - 20);
    LinearModel* policy = new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    for (int i = 0; i < policy->n; i++) policy->w[i] = 0.05f * (((i * 11) % 13) - 6);
    JointModel* jm = new JointModel(value, policy);
    mlSetModel(703, jm);

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    int gaz = findExplorer("GumbelMCTS");
    REQUIRE(gaz >= 0);
    AgentSpec a = agentMakeSearch("gumbel-test", gaz, evalIdx("LearnedValue"), /*depth == sim budget*/ 24, 703);

    int result = -1;
    for (int h = 0; h < 300; h++) {
        int side = (h % 2 == 0) ? White : Black;
        int victor = agentChooseMove(a, side);
        if (victor == WhiteWin) { result = 0; break; }
        if (victor == BlackWin) { result = 1; break; }
    }
    REQUIRE(result != -1);   // completed within the ply cap: no crash, no hang, a real decided game
    mlClearSlots();
}

TEST_CASE("GumbelMCTS - a saved JointModel loads and plays through the real search path") {
    LinearModel* value = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    for (int i = 0; i < value->n; i++) value->w[i] = 0.02f * (((i * 23) % 19) - 9);
    LinearModel* policy = new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    for (int i = 0; i < policy->n; i++) policy->w[i] = 0.03f * (((i * 5) % 7) - 3);
    JointModel jm(value, policy);
    REQUIRE(jm.save("build\\test_gumbel_joint.tmp"));

    Model* loaded = loadModel("build\\test_gumbel_joint.tmp");
    REQUIRE(loaded != nullptr);
    REQUIRE(string(loaded->typeName()) == "joint");
    mlSetModel(704, loaded);   // takes ownership

    REQUIRE(reloadBoard("boards\\board1.txt") == true);
    int s = mlValueScore(White, 704);
    REQUIRE(s > BlackWin + 1024);
    REQUIRE(s < WhiteWin - 1024);

    int gaz = findExplorer("GumbelMCTS");
    REQUIRE(gaz >= 0);
    int params[MAX_EVAL_PARAMS] = { 704, 0, 0, 0 };
    char snap[SIZE][SIZE];
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];
    int victor = g_explorers[gaz].fn(White, evalIdx("LearnedValue"), params, 16);
    bool moved = false;
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) if (snap[x][y] != board[x][y]) moved = true;
    REQUIRE(moved);
    REQUIRE(victor < WhiteWin);   // the standard opening, not a decided win

    mlClearSlots();
}
