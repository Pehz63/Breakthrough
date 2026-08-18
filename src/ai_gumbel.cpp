#include "ai_gumbel.h"
#include "moves.h"
#include "ai_eval.h"
#include "ml_model.h"
#include "ml_eval.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// ============================================================
// TUNING CONSTANTS
// ============================================================
// c_visit, c_scale, and the root candidate count m are per-agent knobs (see
// g_gumbelCVisit/g_gumbelCScale/g_gumbelRootM in globals.h/.cpp and the
// gaz(sims=N,cvisit=C,cscale=S,m=M)@1 grammar in src/ranking.cpp), set by
// agentChooseMove before the explorer call. Direct callers of gumbelSearch
// that bypass agentChooseMove (the self-play trainer in ml_gumbelzero.cpp,
// unit tests) get the globals' paper-default values (50.0, 1.0, 16) since
// nothing else in those call paths touches them.
static const int kGumbelMaxSimPlies = 200;   // safety depth guard per simulation, like QS_MAX_PLY;
                                              // not a per-agent knob (a headroom guard, not a search-shape lever)

// ============================================================
// PURE CORE MATH
// ============================================================
double gumbelSigma(double q, int maxVisitCount, double cVisit, double cScale) {
    return (cVisit + (double)maxVisitCount) * cScale * q;
}

int gumbelSelectAction(const double* logits, const double* completedQ,
                       const int* visitCounts, int n, double cVisit, double cScale) {
    if (n <= 0) return -1;
    int maxN = 0;
    long long sumN = 0;
    for (int i = 0; i < n; i++) {
        if (visitCounts[i] > maxN) maxN = visitCounts[i];
        sumN += visitCounts[i];
    }
    double adj[ML_MAX_MOVES];
    double top = -1e300;
    for (int i = 0; i < n; i++) {
        adj[i] = logits[i] + gumbelSigma(completedQ[i], maxN, cVisit, cScale);
        if (adj[i] > top) top = adj[i];
    }
    double sum = 0.0;
    double p[ML_MAX_MOVES];
    for (int i = 0; i < n; i++) { p[i] = exp(adj[i] - top); sum += p[i]; }
    int best = 0;
    double bestScore = -1e300;
    for (int i = 0; i < n; i++) {
        double prob = p[i] / sum;
        double score = prob - (double)visitCounts[i] / (1.0 + (double)sumN);
        if (score > bestScore) { bestScore = score; best = i; }
    }
    return best;
}

int gumbelTopK(const double* logits, int n, int k, int* out) {
    if (n <= 0 || k <= 0) return 0;
    if (k > n) k = n;
    double key[ML_MAX_MOVES];
    for (int i = 0; i < n; i++) {
        double u = ((double)rand() + 1.0) / ((double)RAND_MAX + 2.0);   // avoid exactly 0 or 1
        double g = -log(-log(u));
        key[i] = g + logits[i];
    }
    int idx[ML_MAX_MOVES];
    for (int i = 0; i < n; i++) idx[i] = i;
    std::stable_sort(idx, idx + n, [&](int a, int b) { return key[a] > key[b]; });
    for (int i = 0; i < k; i++) out[i] = idx[i];
    return k;
}

int gumbelHalvingRounds(int m, int budget, int* simsPerRound) {
    if (budget < 1) budget = 1;
    if (m <= 1) {
        if (simsPerRound) simsPerRound[0] = budget;
        return 1;
    }
    int rounds = 0;
    { int t = m; while (t > 1) { t = (t + 1) / 2; rounds++; } }   // ceil(log2(m))
    if (rounds < 1) rounds = 1;
    int survivors = m;
    for (int r = 0; r < rounds; r++) {
        int per = budget / (rounds * survivors);
        if (per < 1) per = 1;
        if (simsPerRound) simsPerRound[r] = per;
        survivors = (survivors + 1) / 2;
    }
    return rounds;
}

void gumbelImprovedPolicy(const GumbelRootInfo& info, double* out) {
    int n = info.moveCount;
    if (n <= 0) return;
    int maxN = 0;
    for (int i = 0; i < n; i++) if (info.visitCounts[i] > maxN) maxN = info.visitCounts[i];
    double adj[ML_MAX_MOVES];
    double top = -1e300;
    for (int i = 0; i < n; i++) {
        adj[i] = info.logits[i] + gumbelSigma(info.completedQ[i], maxN, g_gumbelCVisit, g_gumbelCScale);
        if (adj[i] > top) top = adj[i];
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) { out[i] = exp(adj[i] - top); sum += out[i]; }
    if (sum <= 0.0) sum = 1.0;   // degenerate guard, never hit in practice (exp of the max term is 1)
    for (int i = 0; i < n; i++) out[i] /= sum;
}

// ============================================================
// BOARD-COUPLED HELPERS
// ============================================================
static inline double moverRelative(double whiteCentricValue, int side) {
    return (side == White) ? whiteCentricValue : -whiteCentricValue;
}

static JointModel* gumbelModelForSlot(int slot) {
    Model* m = mlGetModel(slot);
    if (!m || std::strcmp(m->typeName(), "joint") != 0) return nullptr;
    return static_cast<JointModel*>(m);
}

// White-centric value in [-1, 1] for the position with `turnColor` to move.
// Terminal (canWinWhite/Black) checked first with an exact +-1; otherwise the
// joint model's value head, tanh-bounded and read back from mlValueScore's
// squashed-and-clamped int (dividing out the model's outputScale to recover
// the underlying [-1,1] range).
static double gumbelLeafValue(int turnColor, int slot) {
    if (turnColor == White) {
        if (canWinWhite()) return 1.0;
        if (canWinBlack()) return -1.0;
    } else {
        if (canWinBlack()) return -1.0;
        if (canWinWhite()) return 1.0;
    }
    Model* m = mlGetModel(slot);
    float scale = (m && m->outputScale() > 0.0f) ? m->outputScale() : 900.0f;
    int raw = mlValueScore(turnColor, slot);
    if (raw >= WhiteWin - 1024) return 1.0;
    if (raw <= BlackWin + 1024) return -1.0;
    double v = (double)raw / (double)scale;
    if (v > 1.0) v = 1.0;
    if (v < -1.0) v = -1.0;
    return v;
}

static void gumbelScorePriors(const Move* moves, int n, int side, JointModel* jm, double* logitsOut) {
    for (int i = 0; i < n; i++) {
        if (!jm) { logitsOut[i] = 0.0; continue; }
        float feats[MLM_FEATURES];
        mlExtractMoveFeatures(moves[i], side, feats);
        logitsOut[i] = (double)jm->policyForward(feats, MLM_FEATURES);
    }
}

// ============================================================
// SEARCH TREE
// ============================================================
namespace {
struct GNode {
    bool   expanded = false;
    bool   terminal = false;
    double terminalValue = 0.0;      // white-centric; valid only if terminal
    int    visitCount = 0;
    double valueSum = 0.0;
    int    moveCount = 0;
    Move   moves[ML_MAX_MOVES];
    double logits[ML_MAX_MOVES];
    GNode* children[ML_MAX_MOVES] = {};

    double meanValue() const { return visitCount > 0 ? valueSum / visitCount : 0.0; }
    ~GNode() { for (int i = 0; i < moveCount; i++) delete children[i]; }
};
}

// Runs one MCTS simulation starting at `node` (the live board is already at
// the position `node` represents, with `side` to move), updates node's own
// (and every descendant's) visit/value stats, and returns the backed-up
// white-centric value. Reverses every simulate/unsimulate it applies before
// returning, so the live board is unchanged on exit.
static double gumbelSimulate(GNode* node, int side, int slot, int plyBudget) {
    if (!node->terminal) {
        bool term = false; double termVal = 0.0;
        if (side == White) {
            if (canWinWhite()) { term = true; termVal = 1.0; }
            else if (canWinBlack()) { term = true; termVal = -1.0; }
        } else {
            if (canWinBlack()) { term = true; termVal = -1.0; }
            else if (canWinWhite()) { term = true; termVal = 1.0; }
        }
        if (term) { node->terminal = true; node->terminalValue = termVal; node->expanded = true; }
    }
    if (node->terminal) {
        node->visitCount++; node->valueSum += node->terminalValue;
        return node->terminalValue;
    }

    if (!node->expanded) {
        Move mv[ML_MAX_MOVES];
        int n = generateMoves(side, mv);
        if (n == 0) {
            // The side to move has no legal moves at all: a loss for them,
            // distinct from the canWin* checks above (cached here so a
            // revisit doesn't re-run generateMoves).
            double v = (side == White) ? -1.0 : 1.0;
            node->terminal = true; node->terminalValue = v; node->expanded = true;
            node->visitCount++; node->valueSum += v;
            return v;
        }
        node->moveCount = n;
        for (int i = 0; i < n; i++) node->moves[i] = mv[i];
        gumbelScorePriors(mv, n, side, gumbelModelForSlot(slot), node->logits);
        node->expanded = true;
        double v = gumbelLeafValue(side, slot);
        node->visitCount++; node->valueSum += v;
        return v;
    }

    if (plyBudget <= 0) {
        // Safety stand-pat (mirrors QS_MAX_PLY): does not expand further,
        // essentially never reached at realistic simulation budgets.
        double v = gumbelLeafValue(side, slot);
        node->visitCount++; node->valueSum += v;
        return v;
    }

    // Already expanded, non-terminal: pick a child via the deterministic
    // visit-fraction-matching rule (mover-relative completedQ, substituting
    // this node's own current mean value for an as-yet-unvisited child).
    double completedQ[ML_MAX_MOVES];
    int    visitCounts[ML_MAX_MOVES];
    for (int i = 0; i < node->moveCount; i++) {
        GNode* c = node->children[i];
        visitCounts[i] = c ? c->visitCount : 0;
        completedQ[i] = (c && c->visitCount > 0)
                      ? moverRelative(c->meanValue(), side)
                      : moverRelative(node->meanValue(), side);
    }
    int sel = gumbelSelectAction(node->logits, completedQ, visitCounts, node->moveCount,
                                 g_gumbelCVisit, g_gumbelCScale);
    if (sel < 0) sel = 0;
    if (!node->children[sel]) node->children[sel] = new GNode();
    const Move& mv = node->moves[sel];
    bool isCap = (side == White) ? simulateMoveWhite(mv.sx, mv.sy, mv.dx)
                                  : simulateMoveBlack(mv.sx, mv.sy, mv.dx);
    int otherSide = (side == White) ? Black : White;
    double v = gumbelSimulate(node->children[sel], otherSide, slot, plyBudget - 1);
    if (side == White) unsimulateMoveWhite(mv.sx, mv.sy, mv.dx, isCap);
    else                unsimulateMoveBlack(mv.sx, mv.sy, mv.dx, isCap);
    node->visitCount++; node->valueSum += v;
    return v;
}

// ============================================================
// ROOT ORCHESTRATION
// ============================================================
int gumbelSearch(int side, int slot, int simBudget, GumbelRootInfo* info) {
    Move rootMoves[ML_MAX_MOVES];
    int n = generateMoves(side, rootMoves);
    if (n == 0) return (side == White) ? BlackWin : WhiteWin;

    // Immediate-win shortcut (matches greedyExplore's own generated-move scan).
    for (int i = 0; i < n; i++) {
        bool wins = (side == White) ? (rootMoves[i].dy == SIZE - 1) : (rootMoves[i].dy == 0);
        if (!wins) continue;
        if (info) {
            info->moveCount = 0;
            info->rootValue = info->searchValue = (side == White) ? 1.0 : -1.0;
        }
        return (side == White)
            ? playMoveWhite(rootMoves[i].sx, rootMoves[i].sy, rootMoves[i].dx)
            : playMoveBlack(rootMoves[i].sx, rootMoves[i].sy, rootMoves[i].dx);
    }

    if (simBudget < 1) simBudget = 1;

    GNode root;
    root.moveCount = n;
    for (int i = 0; i < n; i++) root.moves[i] = rootMoves[i];
    gumbelScorePriors(rootMoves, n, side, gumbelModelForSlot(slot), root.logits);
    root.expanded = true;
    double rootLeafValue = gumbelLeafValue(side, slot);
    root.visitCount = 1; root.valueSum = rootLeafValue;

    if (n == 1) {
        if (info) {
            info->moveCount = 1; info->logits[0] = root.logits[0];
            info->completedQ[0] = moverRelative(rootLeafValue, side);
            info->visitCounts[0] = 0; info->rootValue = info->searchValue = rootLeafValue;
        }
        return (side == White)
            ? playMoveWhite(root.moves[0].sx, root.moves[0].sy, root.moves[0].dx)
            : playMoveBlack(root.moves[0].sx, root.moves[0].sy, root.moves[0].dx);
    }

    int m = (g_gumbelRootM < n) ? g_gumbelRootM : n;
    int cand[ML_MAX_MOVES];
    int survivorCount = gumbelTopK(root.logits, n, m, cand);

    int simsPerRound[32];
    int rounds = gumbelHalvingRounds(survivorCount, simBudget, simsPerRound);
    int otherSide = (side == White) ? Black : White;

    for (int r = 0; r < rounds && survivorCount > 1; r++) {
        int perCand = simsPerRound[r];
        for (int c = 0; c < survivorCount; c++) {
            int mi = cand[c];
            if (!root.children[mi]) root.children[mi] = new GNode();
            const Move& mv = root.moves[mi];
            for (int s = 0; s < perCand; s++) {
                bool isCap = (side == White) ? simulateMoveWhite(mv.sx, mv.sy, mv.dx)
                                              : simulateMoveBlack(mv.sx, mv.sy, mv.dx);
                double v = gumbelSimulate(root.children[mi], otherSide, slot, kGumbelMaxSimPlies);
                if (side == White) unsimulateMoveWhite(mv.sx, mv.sy, mv.dx, isCap);
                else                unsimulateMoveBlack(mv.sx, mv.sy, mv.dx, isCap);
                root.visitCount++; root.valueSum += v;
            }
        }
        // Round-end cut: rank survivors by logit + sigma(completedQ), keep the top half.
        int maxN = 0;
        for (int c = 0; c < survivorCount; c++) {
            int vc = root.children[cand[c]] ? root.children[cand[c]]->visitCount : 0;
            if (vc > maxN) maxN = vc;
        }
        double rank[ML_MAX_MOVES];
        for (int c = 0; c < survivorCount; c++) {
            int mi = cand[c];
            GNode* ch = root.children[mi];
            double q = (ch && ch->visitCount > 0) ? moverRelative(ch->meanValue(), side)
                                                   : moverRelative(root.meanValue(), side);
            rank[c] = root.logits[mi] + gumbelSigma(q, maxN, g_gumbelCVisit, g_gumbelCScale);
        }
        int order[ML_MAX_MOVES];
        for (int c = 0; c < survivorCount; c++) order[c] = c;
        std::stable_sort(order, order + survivorCount, [&](int a, int b) { return rank[a] > rank[b]; });
        int keep = (survivorCount + 1) / 2;
        int newCand[ML_MAX_MOVES];
        for (int c = 0; c < keep; c++) newCand[c] = cand[order[c]];
        for (int c = 0; c < keep; c++) cand[c] = newCand[c];
        survivorCount = keep;
    }

    int chosen = cand[0];   // Sequential Halving narrows to exactly one by construction.

    if (info) {
        info->moveCount = n;
        info->rootValue = rootLeafValue;
        info->searchValue = root.meanValue();
        for (int i = 0; i < n; i++) {
            info->logits[i] = root.logits[i];
            GNode* ch = root.children[i];
            int vc = ch ? ch->visitCount : 0;
            info->visitCounts[i] = vc;
            info->completedQ[i] = (ch && vc > 0) ? moverRelative(ch->meanValue(), side)
                                                  : moverRelative(root.meanValue(), side);
        }
    }

    const Move& winMove = root.moves[chosen];
    return (side == White)
        ? playMoveWhite(winMove.sx, winMove.sy, winMove.dx)
        : playMoveBlack(winMove.sx, winMove.sy, winMove.dx);
}

int gumbelExplore(int side, int /*evaluator*/, const int* params, int budget) {
    return gumbelSearch(side, params[0], budget, nullptr);
}
