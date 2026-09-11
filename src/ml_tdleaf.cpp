#include "ml_tdleaf.h"
#include "ml_model.h"
#include "ml_eval.h"
#include "ml_features.h"
#include "ai_eval.h"
#include "agents.h"
#include "explorers.h"
#include "board_io.h"
#include "datastore.h"
#include "transposition.h"
#include "train_budget.h"
#include <vector>
#include <cmath>
#include <sstream>
#include <climits>
#include <cstring>
#include <algorithm>
#include <unordered_set>

// ============================================================
// TD-LEAF(LAMBDA): GRADIENT CORE
// ============================================================
// Pure function, no board or model state, so tests/test_ml.cpp can assert the
// closed forms directly (see ml_tdleaf.h for the derivation).
void tdLeafGradients(const std::vector<double>& p, double z, double lambda,
                     std::vector<double>& gOut) {
    const size_t n = p.size();
    gOut.assign(n, 0.0);
    if (n == 0) return;
    // Backward eligibility recursion: e_t = d_t + lambda * e_{t+1}, e_N = 0,
    // where d_t = p_{t+1} - p_t and p_N := z.
    double e = 0.0;
    for (size_t i = n; i-- > 0; ) {
        double next = (i + 1 < n) ? p[i + 1] : z;
        double d = next - p[i];
        e = d + lambda * e;
        gOut[i] = -e;               // dL/d(logit) for cross-entropy vs the lambda-return
    }
}

double tdLeafScheduledValue(double start, double floor, int decayGames, int gameIndex) {
    if (decayGames <= 0) return start;
    double t = std::min(1.0, (double)gameIndex / (double)decayGames);
    return start * (1.0 - t) + floor * t;
}

// ============================================================
// SMALL UTILITIES
// ============================================================
static double frandTD() { return (double)rand() / ((double)RAND_MAX + 1.0); }

static double sigmoidTD(double z) {
    if (z >= 0) { double e = exp(-z); return 1.0 / (1.0 + e); }
    double e = exp(z); return e / (1.0 + e);
}

// ============================================================
// REPLICATION-STUDY HELPERS (pure; closed forms asserted in tests/test_ml.cpp)
// ============================================================
double tdTerminalTarget(int outcome, int plies, bool depthReward, int maxPlies) {
    if (outcome != 1 && outcome != 2) return 0.5;
    if (!depthReward || maxPlies <= 0) return (outcome == 1) ? 1.0 : 0.0;
    int l = maxPlies - plies + 1;
    if (l < 1) l = 1;
    if (l > maxPlies) l = maxPlies;
    double half = 0.5 * (double)l / (double)maxPlies;
    return (outcome == 1) ? 0.5 + half : 0.5 - half;
}

double tdScoreToProb(int s, float outScale) {
    if (s > WhiteWin - 1024) return 1.0;     // the search's proven-result sentinels
    if (s < BlackWin + 1024) return 0.0;
    double scale = (outScale != 0.0f) ? (double)outScale : 1.0;
    double u = (double)s / scale;            // = tanh(out) up to the rounding step
    if (u > 1.0) u = 1.0;
    if (u < -1.0) u = -1.0;
    double a = sqrt(1.0 + u), b = sqrt(1.0 - u);
    return a / (a + b);                      // sigmoid(atanh(u)), exact at u = +-1
}

double tdOneSidedGrad(double v, double q, int flag) {
    double g = v - q;
    if (flag == TT_LOWER && g > 0.0) return 0.0;   // above a lower bound: consistent
    if (flag == TT_UPPER && g < 0.0) return 0.0;   // below an upper bound: consistent
    return g;
}

double tdOrdinalProb(double e, int n, int i) {
    if (n <= 0 || i < 0 || i >= n) return 0.0;
    double rest = 1.0, p = 0.0;
    for (int k = 0; k <= i; k++) {
        p = (e + (1.0 - e) / (double)(n - k)) * rest;
        rest -= p;
    }
    return p;
}

int tdOrdinalPick(double e, int n) {
    for (int i = 0; i + 1 < n; i++) {
        double stop = e + (1.0 - e) / (double)(n - i);
        if (stop >= 1.0) return i;       // certain: draw nothing, so e = 1 consumes no rand()
        if (frandTD() < stop) return i;
    }
    return (n > 0) ? n - 1 : 0;   // the last rank's stop probability is 1
}

TDLeafRunStats g_tdLastRun;

// Features for whichever version the model reads. Buffers are sized for v2,
// the larger of the two.
static void extractFeatTD(int featVer, int side, float* f) {
    if (featVer == 2) mlExtractValueFeaturesV2(side, f);
    else              mlExtractValueFeatures(side, f);
}

// ============================================================
// ROOT RECOVERY (treestrap and ordinal need the position BEFORE the move)
// ============================================================
// agentChooseMove searches and plays in one call. The TreeStrap walk and the
// ordinal ranking both start from the root, so the move the search played is
// recovered by diffing the board against a snapshot, then taken back.
// playMoveWhite/Black change only the board, the piece counts and g_chipDiff,
// so restoring those three restores the root exactly. trainTDLeaf checks that
// on every ply and stops with an error if it ever fails.
struct PlayedMoveTD { int sx, sy, dx; bool cap; bool ok; };

static PlayedMoveTD findPlayedMoveTD(const char snap[SIZE][SIZE], int side) {
    PlayedMoveTD pm; pm.sx = pm.sy = pm.dx = -1; pm.cap = false; pm.ok = false;
    const char own = (side == White) ? WHITE : BLACK;
    const char opp = (side == White) ? BLACK : WHITE;
    int fx = -1, fy = -1, tx = -1, ty = -1, nFrom = 0, nTo = 0;
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            if (snap[x][y] == own && board[x][y] != own) { fx = x; fy = y; nFrom++; }
            if (board[x][y] == own && snap[x][y] != own) { tx = x; ty = y; nTo++; }
        }
    if (nFrom != 1 || nTo != 1) return pm;
    if (ty != fy + ((side == White) ? 1 : -1)) return pm;
    pm.sx = fx; pm.sy = fy; pm.dx = tx; pm.cap = (snap[tx][ty] == opp); pm.ok = true;
    return pm;
}

static void undoPlayedMoveTD(const PlayedMoveTD& pm, int side) {
    if (side == White) {
        board[pm.sx][pm.sy] = WHITE;
        board[pm.dx][pm.sy + 1] = pm.cap ? BLACK : EMPTY;
        if (pm.cap) { g_blackCount++; g_chipDiff--; }
    } else {
        board[pm.sx][pm.sy] = BLACK;
        board[pm.dx][pm.sy - 1] = pm.cap ? WHITE : EMPTY;
        if (pm.cap) { g_whiteCount++; g_chipDiff++; }
    }
}

static int replayMoveTD(int sx, int sy, int dx, int side) {
    return (side == White) ? playMoveWhite(sx, sy, dx) : playMoveBlack(sx, sy, dx);
}

// ============================================================
// TREESTRAP WALK (Veness et al. 2009, Algorithm 2 "DeltaFromTransTbl")
// ============================================================
// Walks the table the just-finished search filled, starting from the root:
// each successor with an entry from THIS search (generation match) and
// remaining depth >= dMin gets the one-sided update toward its bound, and the
// walk recurses into it. Nothing is written to the table, and the board is
// restored move by move, so the next search sees exactly what it would have
// seen without the walk. Gradients go into a dense per-game accumulator and
// are applied at the same point every other backup applies its updates, so the
// model the search reads never changes mid-game. A linear model's per-game
// update is a sum of per-position terms, so accumulating is the same update as
// applying them one at a time (exactly with l2 = 0, which the study uses).
struct TreeWalkTD {
    const Model* model;
    int featVer, featCount;
    float outScale;
    uint64_t ctx;
    uint8_t gen;
    int dMin;
    bool mirror;
    std::unordered_set<uint64_t> seen;
    std::vector<double> G;       // sum of g * x over the game
    double Gb;                   // sum of g
    long long accepted, updated, mirrorUpdated;
};

static void treeAccumulateTD(TreeWalkTD& w, const float* x, double q, int flag) {
    double v = sigmoidTD((double)w.model->forward(x, w.featCount));
    double g = tdOneSidedGrad(v, q, flag);
    if (g != 0.0) {
        for (int i = 0; i < w.featCount; i++) if (x[i] != 0.0f) w.G[i] += g * (double)x[i];
        w.Gb += g;
        w.updated++;
    }
    if (w.mirror) {
        float xm[MLV2_FEATURES];
        mlv2MirrorFeatures(x, xm);
        double vm = sigmoidTD((double)w.model->forward(xm, w.featCount));
        double gm = tdOneSidedGrad(vm, q, flag);
        if (gm != 0.0) {
            for (int i = 0; i < w.featCount; i++) if (xm[i] != 0.0f) w.G[i] += gm * (double)xm[i];
            w.Gb += gm;
            w.mirrorUpdated++;
        }
    }
}

static void treeVisitTD(TreeWalkTD& w, int side) {
    Move mv[ML_MAX_MOVES];
    const int n = generateMoves(side, mv);
    const int opp = (side == White) ? Black : White;
    for (int i = 0; i < n; i++) {
        const Move& m = mv[i];
        if (m.dy == 0 || m.dy == SIZE - 1) continue;   // a winning move: terminal, never stored
        bool cap = (side == White) ? simulateMoveWhite(m.sx, m.sy, m.dx)
                                   : simulateMoveBlack(m.sx, m.sy, m.dx);
        // Decided positions are never stored (the search returns a sentinel
        // before its store), so there is nothing to probe below them.
        if (g_whiteCount > 0 && g_blackCount > 0 && nearWinCheck(opp) == 0) {
            uint64_t key = (uint64_t)positionKey(opp, false).hash ^ w.ctx;
            TTEntry e;
            if (w.seen.find(key) == w.seen.end() && ttPeek(key, e)
                && e.gen == w.gen && e.depth >= w.dMin) {
                w.seen.insert(key);
                w.accepted++;
                float f[MLV2_FEATURES];
                extractFeatTD(w.featVer, opp, f);
                treeAccumulateTD(w, f, tdScoreToProb(e.score, w.outScale), e.flag);
                treeVisitTD(w, opp);
            }
        }
        if (side == White) unsimulateMoveWhite(m.sx, m.sy, m.dx, cap);
        else               unsimulateMoveBlack(m.sx, m.sy, m.dx, cap);
    }
}

// One gradient step from the dense accumulator, then clear it. Same update as
// LinearModel::gradStep applied once per accumulated term when l2 = 0; with
// l2 > 0 the decay is applied once per application rather than once per term.
static void applyDenseTD(LinearModel* lm, std::vector<double>& G, double& Gb,
                         double lr, double l2) {
    for (int i = 0; i < lm->n; i++) {
        lm->w[i] = (float)((double)lm->w[i] - lr * (G[i] + l2 * (double)lm->w[i]));
        G[i] = 0.0;
    }
    lm->bias = (float)((double)lm->bias - lr * Gb);
    Gb = 0.0;
}

// ============================================================
// ORDINAL MOVE RANKING
// ============================================================
// Ranks the root's moves for Cohen-Solal's ordinal distribution without a
// second search. The search's own choice is ranked first. Every other move is
// ranked by the entry the search stored for the position it leads to, which
// for a move that did not become best is the fail-soft bound that refuted it
// (an upper bound on its value for the mover). A move whose entry is missing
// (overwritten, or from an earlier search) is ranked by the model's static
// eval of that position instead, on the same scale. Positions decided for
// either side rank above or below everything else.
struct RankedMoveTD { Move m; int tier; double key; int idx; };

static void rankRootMovesTD(int side, const PlayedMoveTD& searchMove, int slot,
                            uint64_t ctx, uint8_t gen, std::vector<RankedMoveTD>& out,
                            long long& ranked, long long& unranked) {
    out.clear();
    Move mv[ML_MAX_MOVES];
    const int n = generateMoves(side, mv);
    const int opp = (side == White) ? Black : White;
    const double sgn = (side == White) ? 1.0 : -1.0;
    for (int i = 0; i < n; i++) {
        RankedMoveTD r; r.m = mv[i]; r.idx = i; r.tier = 1; r.key = 0.0;
        if (mv[i].sx == searchMove.sx && mv[i].sy == searchMove.sy && mv[i].dx == searchMove.dx) {
            r.tier = 3;
            out.push_back(r);
            continue;
        }
        bool cap = (side == White) ? simulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx)
                                   : simulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx);
        int moverWins = (side == White) ? WhiteWin : BlackWin;
        int decided = 0;
        if (mv[i].dy == 0 || mv[i].dy == SIZE - 1) decided = moverWins;
        else if (g_whiteCount == 0) decided = BlackWin;
        else if (g_blackCount == 0) decided = WhiteWin;
        else decided = nearWinCheck(opp);
        if (decided != 0) {
            r.tier = (decided == moverWins) ? 2 : 0;
        } else {
            uint64_t key = (uint64_t)positionKey(opp, false).hash ^ ctx;
            TTEntry e;
            if (ttPeek(key, e) && e.gen == gen) { r.key = sgn * (double)e.score; ranked++; }
            else { r.key = sgn * (double)mlValueScore(opp, slot); unranked++; }
        }
        if (side == White) unsimulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx, cap);
        else               unsimulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx, cap);
        out.push_back(r);
    }
    std::sort(out.begin(), out.end(), [](const RankedMoveTD& a, const RankedMoveTD& b) {
        if (a.tier != b.tier) return a.tier > b.tier;
        if (a.key != b.key) return a.key > b.key;
        return a.idx < b.idx;
    });
}

// The sub-model that actually owns trainable weights. A DistModel's search-facing
// value is its mu head, and a ResidualModel's skip is frozen by design -- neither
// wrapper overrides gradStep, so applying the step to the wrapper would silently
// train nothing. Unwrap for the UPDATE; keep using the wrapper's forward() for the
// VALUE, so a residual skip stays included in p exactly as it is in search.
static Model* trainableHead(Model* m) {
    if (DistModel* dm = dynamic_cast<DistModel*>(m)) return trainableHead(dm->muHead);
    if (ResidualModel* rm = dynamic_cast<ResidualModel*>(m)) return trainableHead(rm->inner);
    return m;
}

static int gameOutcomeTD(int victor) {
    if (victor >= WhiteWin) return 1;
    if (victor <= BlackWin) return 2;
    return 0;
}

static int explorerIdxTD(const char* n) {
    for (int i = 0; i < g_explorerCount; i++) if (string(g_explorers[i].name) == n) return i;
    return 0;
}

// ============================================================
// PRINCIPAL-VARIATION WALK
// ============================================================
// One make/unmake step recorded so the walk can be unwound exactly.
struct PVStep { int sx, sy, dx, side; bool cap; };

// Walk up to maxPlies plies along the principal variation from the CURRENT board
// (with `side` to move), reading each step's best move from the transposition
// table the just-finished search populated. Leaves the board AT the leaf and fills
// `steps` so unwindPV can restore it. Returns the side to move at the leaf.
//
// Stops early on a decided position, a missing TT entry (always-replace tables do
// lose entries), or a stored move that does not validate against the live board (a
// hash collision). Callers report the achieved depth so a degraded walk is visible
// rather than silent.
static int walkPV(int side, int maxPlies, std::vector<PVStep>& steps) {
    steps.clear();
    int s = side;
    for (int k = 0; k < maxPlies; k++) {
        if (nearWinCheck(s) != 0) break;            // decided: this IS the leaf
        PosKey pk = positionKey(s, false);
        int sc = 0, fromSq = -1, toSq = -1;
        // The search salts every key it stores with its own context, so probing
        // the bare position hash matches nothing and the walk stops at depth 1.
        ttProbe((unsigned long long)pk.hash ^ ttSearchContext(),
                0, INT_MIN, INT_MAX, sc, fromSq, toSq);
        if (fromSq < 0 || toSq < 0) break;          // no stored move here
        int sx = fromSq % SIZE, sy = fromSq / SIZE;
        int dx = toSq % SIZE,   dy = toSq / SIZE;
        // Validate against the live board before trusting a hashed move.
        char want = (s == White) ? WHITE : BLACK;
        if (sx < 0 || sx >= SIZE || sy < 0 || sy >= SIZE || dx < 0 || dx >= SIZE) break;
        if (board[sx][sy] != want) break;
        if (dy != sy + ((s == White) ? 1 : -1)) break;
        bool ok = (s == White) ? tryMoveQuickWhite(sx, sy, dx) : tryMoveQuickBlack(sx, sy, dx);
        if (!ok) break;
        bool cap = (s == White) ? simulateMoveWhite(sx, sy, dx) : simulateMoveBlack(sx, sy, dx);
        PVStep st; st.sx = sx; st.sy = sy; st.dx = dx; st.side = s; st.cap = cap;
        steps.push_back(st);
        s = (s == White) ? Black : White;
    }
    return s;
}

// Undo a walk in reverse order. simulate/unsimulate maintain every incremental
// counter, so the board is bit-identical to its pre-walk state afterwards.
static void unwindPV(const std::vector<PVStep>& steps) {
    for (size_t i = steps.size(); i-- > 0; ) {
        const PVStep& st = steps[i];
        if (st.side == White) unsimulateMoveWhite(st.sx, st.sy, st.dx, st.cap);
        else                  unsimulateMoveBlack(st.sx, st.sy, st.dx, st.cap);
    }
}

// ============================================================
// CONFIG
// ============================================================
TDLeafConfig tdLeafDefaults() {
    TDLeafConfig c;
    c.outPath     = "models/tdleaf";
    c.boardFile   = "boards/board1.txt";
    c.initModel   = "";
    c.games       = 500;
    // Default to the head the cohort is certified at (CHAMPION.md rule 5). A
    // TD-Leaf target IS the search's backed-up value, so training against a
    // shallower search than the one that will be rated is a distribution
    // mismatch. Training cost is ~100x below rating cost, so a cheaper default
    // would buy nothing.
    c.depth       = 6;
    c.nodeBudget  = 200000;
    c.timeBudgetMs   = 0.0;
    c.iterMinRemain  = 0;
    c.retainBudget   = false;
    c.lambda      = 0.7;
    c.lr          = 0.01;
    c.lrFloor     = 0.01;   // == lr: off unless lrDecayGames > 0
    c.lrDecayGames = 0;
    c.l2          = 0.0;
    c.seed        = 1001;
    c.openPlies   = 4;
    c.explore     = 0.0;
    c.exploreFloor = 0.0;
    c.exploreDecayGames = 0;
    c.batchGames  = 1;
    c.backup       = "td-leaf";
    c.treeMinDepth = 1;         // Veness et al. 2009 used d_min = 1
    c.terminal     = "winloss";
    c.augment      = "";
    c.exploreDist  = "eps";
    c.ordinalStart = 0.0;       // Cohen-Solal anneal e = t/T: 0 -> 1 over the run
    c.ordinalEnd   = 1.0;
    c.ordinalGames = 0;
    c.modelType   = "linear";
    c.featureVersion = 2;
    c.ckptEvery   = 0;
    c.reportEvery = 50;
    c.wallStopSec = 0.0;
    c.resumeFrom  = "";
    return c;
}

// Highest ladder rung, so the run knows when it may stop early.
static int maxLadderRung(const std::vector<int>& v) {
    int m = 0;
    for (size_t i = 0; i < v.size(); i++) if (v[i] > m) m = v[i];
    return m;
}

// ============================================================
// REGIME
// ============================================================
double g_tdLastMeanPV = 0.0;

int trainTDLeaf(const TDLeafConfig& cfg) {
    srand(cfg.seed);
    PRNT = 0;

    // ---- Training-compute meter and wall-clock ladder (src/train_budget.h) ----
    TrainBudget budget = tbDefaults();
    budget.wallCkptAt  = cfg.wallCkptAt;
    budget.wallStopSec = cfg.wallStopSec;

    // ---- Model: resume a run, load an initialisation, or build a fresh one ----
    Model* model = nullptr;
    string provInit;
    const string loadFrom = !cfg.resumeFrom.empty() ? cfg.resumeFrom : cfg.initModel;
    if (!loadFrom.empty()) {
        model = loadModel(loadFrom);
        if (!model) { cout << "ERROR: cannot load model " << loadFrom << "\n"; return 1; }
        if (model->head() != HEAD_VALUE) {
            cout << "ERROR: " << loadFrom << " is not a value model.\n";
            delete model; return 1;
        }
        if (!cfg.resumeFrom.empty()) {
            // Resume: carry the checkpoint's own recorded spend forward, so the
            // ladder stays cumulative and a rung already written is not rewritten.
            if (!tbParsePrior(model->teacher, budget, "games")) {
                cout << "ERROR: " << cfg.resumeFrom << " carries no spend stamp"
                     << " (games=/secs=/nodes=), so its ladder cannot be continued.\n";
                delete model; return 1;
            }
            // The resumed file's provenance is the source of the recipe prefix
            // only for reporting; the recipe itself comes from THIS command line,
            // which is the caller's responsibility to keep identical.
            provInit = "resume:" + cfg.resumeFrom;
            cout << "Resuming from " << cfg.resumeFrom << ": "
                 << budget.priorUnits << " games, " << budget.priorSec << " s, "
                 << budget.priorNodes << " nodes already spent\n";
        } else {
            provInit = "init:" + cfg.initModel;
        }
        if (cfg.featureVersion != 2)
            cout << "NOTE: --feature-version ignored (" << loadFrom
                 << "'s own feature version governs when --init/--resume is set)\n";
    } else {
        const int featVer = (cfg.featureVersion == 1) ? 1 : 2;
        const int featCount = (featVer == 1) ? MLV_FEATURES : MLV2_FEATURES;
        if (cfg.modelType == "mlp") {
            std::vector<int> hidden = cfg.mlpHidden;
            if (hidden.empty()) hidden.push_back(32);
            MLPModel* mm = new MLPModel(HEAD_VALUE, featVer, featCount, 900.0f, hidden);
            mm->initRandom();
            model = mm;
        } else {
            LinearModel* lm = new LinearModel(HEAD_VALUE, featVer, featCount, 900.0f);
            for (int i = 0; i < lm->n; i++) lm->w[i] = (float)((frandTD() * 2.0 - 1.0) * 0.05);
            model = lm;
        }
        provInit = "init:scratch";
    }

    Model* head = trainableHead(model);
    const int featVer   = model->featureVersion();
    const int featCount = model->featureCount();
    if (featVer != 1 && featVer != 2) {
        cout << "ERROR: unsupported feature version " << featVer << "\n"; delete model; return 1;
    }

    // ---- Replication-study switches ----
    enum { BK_LEAF, BK_DIRECTED, BK_ROOT, BK_TREE };
    int backupKind;
    if      (cfg.backup == "td-leaf")     backupKind = BK_LEAF;
    else if (cfg.backup == "td-directed") backupKind = BK_DIRECTED;
    else if (cfg.backup == "rootstrap")   backupKind = BK_ROOT;
    else if (cfg.backup == "treestrap")   backupKind = BK_TREE;
    else {
        cout << "ERROR: --backup must be td-leaf, td-directed, rootstrap or treestrap (got '"
             << cfg.backup << "')\n";
        delete model; return 1;
    }
    if (cfg.terminal != "winloss" && cfg.terminal != "depth") {
        cout << "ERROR: --terminal must be winloss or depth (got '" << cfg.terminal << "')\n";
        delete model; return 1;
    }
    const bool depthReward = (cfg.terminal == "depth");
    if (!cfg.augment.empty() && cfg.augment != "mirror") {
        cout << "ERROR: --augment must be mirror (got '" << cfg.augment << "')\n";
        delete model; return 1;
    }
    const bool mirror = (cfg.augment == "mirror");
    if (mirror && featVer != 2) {
        cout << "ERROR: --augment mirror needs the v2 piece-square features\n";
        delete model; return 1;
    }
    if (cfg.exploreDist != "eps" && cfg.exploreDist != "ordinal") {
        cout << "ERROR: --explore-dist must be eps or ordinal (got '" << cfg.exploreDist << "')\n";
        delete model; return 1;
    }
    const bool ordinal = (cfg.exploreDist == "ordinal");
    if (ordinal && cfg.explore > 0.0) {
        cout << "ERROR: --explore-dist ordinal replaces --explore, set one or the other\n";
        delete model; return 1;
    }
    LinearModel* treeLinear = nullptr;
    if (backupKind == BK_TREE) {
        // The walk accumulates a dense per-game gradient, which is the model's
        // update only when the model is linear in its trained weights.
        treeLinear = dynamic_cast<LinearModel*>(head);
        if (!treeLinear) {
            cout << "ERROR: --backup treestrap supports linear models only\n";
            delete model; return 1;
        }
        if (cfg.treeMinDepth < 1) {
            cout << "ERROR: --tree-min-depth must be >= 1\n";
            delete model; return 1;
        }
    }

    // The slot owns the model from here (mlClearSlots frees it), and search reads
    // the SAME object being trained, so each game is played by the current weights.
    const int slot = ML_SLOTS - 2;
    mlSetModel(slot, model);

    // ---- Self-play agent ----
    AgentSpec agent = agentMakeSearch("tdleaf", explorerIdxTD("AlphaBeta"),
                                      learnedValueIndex(), cfg.depth, slot);
    agent.nodeBudget    = cfg.nodeBudget;
    agent.timeBudgetMs  = cfg.timeBudgetMs;
    agent.useAlphaBeta  = true;
    agent.useTT         = true;      // REQUIRED: the PV walk reads this table
    agent.useMoveOrder  = true;
    agent.randomMoveProb = 0.0;      // exploration is handled explicitly below
    // The serving heads carry `rem=` and `retain`, and both change WHERE the
    // budget goes rather than how much of it there is: the gate declines an
    // iteration that cannot finish, and the purse moves the saving onto a later
    // move. A TD-Leaf target IS the value this search backs up, so a generator
    // without them trains against a different search than the one being rated.
    // Same argument as the depth/nodeBudget defaults above, one level finer.
    agent.iterMinRemain = cfg.iterMinRemain;
    agent.retainBudget  = cfg.retainBudget;

    // Provenance is split into the RECIPE (fixed by the command line) and the
    // SPEND (only knowable once a save point is reached). The recipe is built
    // once here; the spend is appended at each save by stampProvenance below.
    string provRecipe;
    {
        std::ostringstream prov;
        prov << "tdleaf(lambda=" << cfg.lambda << ",lr=" << cfg.lr;
        if (cfg.lrDecayGames > 0)
            prov << "->" << cfg.lrFloor << "/" << cfg.lrDecayGames << "g";
        prov << ",l2=" << cfg.l2 << ",d" << cfg.depth;
        if (cfg.nodeBudget) prov << ",nb" << cfg.nodeBudget;
        if (cfg.timeBudgetMs > 0.0) prov << ",tb" << cfg.timeBudgetMs << "ms";
        // Emitted only when set, so an existing recipe string is unchanged and a
        // checkpoint trained without the gate keeps the provenance it always had.
        if (cfg.iterMinRemain > 0) prov << ",rem=" << cfg.iterMinRemain;
        if (cfg.retainBudget)      prov << ",retain";
        prov << ",batch=" << cfg.batchGames
             << ",open=" << cfg.openPlies << ",explore=" << cfg.explore;
        if (cfg.exploreDecayGames > 0)
            prov << "->" << cfg.exploreFloor << "/" << cfg.exploreDecayGames << "g";
        // Replication-study switches, each only when not the baseline value.
        if (backupKind != BK_LEAF) prov << ",backup=" << cfg.backup;
        if (backupKind == BK_TREE) prov << ",dmin=" << cfg.treeMinDepth;
        if (depthReward)           prov << ",terminal=depth";
        if (mirror)                prov << ",augment=mirror";
        if (ordinal) {
            prov << ",ordinal=" << cfg.ordinalStart;
            if (cfg.ordinalGames > 0)
                prov << "->" << cfg.ordinalEnd << "/" << cfg.ordinalGames << "g";
        }
        prov << ",seed=" << cfg.seed;
        provRecipe = prov.str();
    }
    // Attach the spend this checkpoint actually represents, then save. Every
    // save in this function goes through here, which is what keeps a rung's
    // header honest -- see the header comment on trainTDLeaf.
    struct Saver {
        Model* model; const string& recipe; const string& init; const TrainBudget& b;
        bool save(const string& path) const {
            model->teacher = recipe + tbStamp(b, "games") + ") " + init;
            return model->save(path);
        }
    } saver = { model, provRecipe, provInit, budget };

    cout << "TD-Leaf: " << provRecipe << ",...) " << provInit << "\n";
    cout << "Model: type=" << model->typeName() << " featVer=" << featVer
         << " trainable=" << head->typeName() << "\n";
    if (cfg.wallStopSec > 0.0 || !cfg.wallCkptAt.empty()) {
        cout << "Wall ladder:";
        for (size_t k = 0; k < cfg.wallCkptAt.size(); k++) cout << " " << cfg.wallCkptAt[k] << "s";
        if (cfg.wallStopSec > 0.0) cout << "   stop at " << cfg.wallStopSec << "s";
        cout << "  (cumulative)\n";
    }

    // ---- Instrument diagnostics (see the standing "validate the instrument" rule) ----
    long long pvDepthSum = 0, pvCount = 0, pvTruncated = 0;
    long long trainedPositions = 0, skippedDecided = 0;
    int wWins = 0, bWins = 0, draws = 0;

    // Batched mode accumulates (features, gOut) and applies at the batch boundary.
    std::vector<std::vector<float> > batchX;
    std::vector<double> batchG;

    std::vector<float> feat(featCount);
    std::vector<double> p, gOut;
    std::vector<char> trainable;
    std::vector<std::vector<float> > leafFeat;
    std::vector<PVStep> steps;

    // Replication-study state. tgt holds RootStrap's per-position target (the
    // lambda-return backups compute theirs from p and z at game end). pMir and
    // mirFeat hold each trained position's mirror, evaluated with the same
    // pre-game weights as p so both halves of the update see one model.
    std::vector<double> tgt, pMir;
    std::vector<std::vector<float> > mirFeat;
    std::vector<float> rootFeat(MLV2_FEATURES), mfeat(MLV2_FEATURES);
    const bool needRootFeat  = (backupKind != BK_LEAF);
    const bool needRootBoard = (backupKind == BK_TREE) || ordinal;
    char snap[SIZE][SIZE];
    std::vector<RankedMoveTD> rankedMoves;
    TreeWalkTD walk;
    walk.model = model; walk.featVer = featVer; walk.featCount = featCount;
    walk.outScale = model->outputScale(); walk.ctx = 0; walk.gen = 0;
    walk.dMin = cfg.treeMinDepth; walk.mirror = mirror;
    walk.G.assign(featCount, 0.0); walk.Gb = 0.0;
    walk.accepted = walk.updated = walk.mirrorUpdated = 0;
    long long searches = 0, gamePliesTotal = 0, updates = 0, mirrorUpdates = 0;
    long long ordMoves = 0, ordNonBest = 0, ordRanked = 0, ordUnranked = 0;
    double effOrdinal = cfg.ordinalStart;

    // Run at least as far as the highest ladder rung, so `--ckpt-at` alone is
    // enough to specify a run and no rung is silently never written. A
    // non-positive game count means the wall clock alone governs the length.
    const int totalGames = (cfg.games <= 0 && cfg.ckptAt.empty())
                         ? 0 : std::max(cfg.games, maxLadderRung(cfg.ckptAt));

    // Declared outside the loop so the final partial-batch flush (after the loop
    // ends) can apply the LAST game's scheduled lr rather than going out of scope.
    double effLr = cfg.lr, effExplore = cfg.explore;

    tbBegin(budget);
    bool stoppedOnWall = false;
    // The game index used by the lr/explore schedules and the game-count ladder
    // is CUMULATIVE across a resume, so a resumed run continues the same
    // schedule instead of restarting it at game 0.
    const int gameBase = (int)budget.priorUnits;

    for (int g = gameBase; totalGames <= 0 || g < totalGames; g++) {
        // Wall-clock stop is checked BETWEEN games: a game is the smallest unit
        // whose training signal is complete (the outcome z is needed before any
        // gradient can be formed), so cutting one mid-way would throw it away.
        if (tbShouldStop(budget)) { stoppedOnWall = true; break; }
        // Independence: a stale table would make a game's result depend on which
        // games preceded it (the cross-game TT pollution defect fixed elsewhere in
        // this project). Every game starts from a clean table.
        ttClear();
        // Same reason as ttClear, one purse further out: a retained budget is
        // cross-game state, so without this a game's play would depend on which
        // games preceded it.
        retainResetCarry();
        reloadBoard(cfg.boardFile);

        effLr = tdLeafScheduledValue(cfg.lr, cfg.lrFloor, cfg.lrDecayGames, g);
        effExplore = tdLeafScheduledValue(cfg.explore, cfg.exploreFloor, cfg.exploreDecayGames, g);
        effOrdinal = tdLeafScheduledValue(cfg.ordinalStart, cfg.ordinalEnd, cfg.ordinalGames, g);

        p.clear(); trainable.clear(); leafFeat.clear();
        tgt.clear(); pMir.clear(); mirFeat.clear();
        int victor = None;
        int gamePlies = 0;             // every move of the game, opening included

        // Record one trained position: its value, and its mirror's value under
        // the same weights when mirroring is on.
        auto recordTrained = [&](const float* x, double pv, double target) {
            p.push_back(pv);
            trainable.push_back(1);
            leafFeat.push_back(std::vector<float>(x, x + featCount));
            tgt.push_back(target);
            if (mirror) {
                mlv2MirrorFeatures(x, mfeat.data());
                pMir.push_back(sigmoidTD((double)model->forward(mfeat.data(), featCount)));
                mirFeat.push_back(mfeat);
            } else {
                pMir.push_back(0.0);
                mirFeat.push_back(std::vector<float>());
            }
        };

        // Random opening for position diversity. Deterministic openers would make
        // self-play replay one game forever.
        for (int h = 0; h < cfg.openPlies * 2; h++) {
            int side = (h % 2 == 0) ? White : Black;
            victor = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            gamePlies++;
            if (gameOutcomeTD(victor)) break;
        }

        if (!gameOutcomeTD(victor)) {
            for (int h = 0; h < 400; h++) {
                int side = (h % 2 == 0) ? White : Black;

                // Plies that carry no usable TD signal: a random move is not the
                // search's choice (so the line under it is not this position's
                // principal variation), and an already-decided root has no
                // informative leaf. Play them out -- the game must still reach a
                // real conclusion -- but capture nothing.
                bool exploreMove = !ordinal && (effExplore > 0.0 && frandTD() < effExplore);
                if (exploreMove || nearWinCheck(side) != 0) {
                    victor = exploreMove
                        ? ((side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack())
                        : agentChooseMove(agent, side);
                    gamePlies++;
                    if (gameOutcomeTD(victor)) break;
                    continue;
                }

                // The root, captured before the search moves anything.
                if (needRootFeat) extractFeatTD(featVer, side, rootFeat.data());
                const int snapW = g_whiteCount, snapB = g_blackCount, snapD = g_chipDiff;
                if (needRootBoard) memcpy(snap, board, sizeof(snap));

                victor = agentChooseMove(agent, side);
                gamePlies++;
                searches++;
                // White-centric score of the line the search chose. Read here,
                // before anything else can search and overwrite it.
                const int rootScore = (side == White) ? g_downEvalWhite : g_downEvalBlack;
                if (gameOutcomeTD(victor)) break;   // this move ended it: no leaf to capture

                if (needRootBoard) {
                    PlayedMoveTD pm = findPlayedMoveTD(snap, side);
                    if (!pm.ok) {
                        cout << "ERROR: could not recover the move the search played\n";
                        mlClearSlots(); return 1;
                    }
                    undoPlayedMoveTD(pm, side);
                    if (memcmp(snap, board, sizeof(snap)) != 0 || g_whiteCount != snapW
                        || g_blackCount != snapB || g_chipDiff != snapD) {
                        cout << "ERROR: taking back the searched move did not restore the root\n";
                        mlClearSlots(); return 1;
                    }
                    if (backupKind == BK_TREE) {
                        walk.ctx = ttSearchContext();
                        walk.gen = ttGeneration();
                        walk.seen.clear();
                        walk.seen.insert((uint64_t)positionKey(side, false).hash ^ walk.ctx);
                        treeVisitTD(walk, side);
                    }
                    int px = pm.sx, py = pm.sy, pdx = pm.dx;
                    if (ordinal) {
                        rankRootMovesTD(side, pm, slot, ttSearchContext(), ttGeneration(),
                                        rankedMoves, ordRanked, ordUnranked);
                        int pick = tdOrdinalPick(effOrdinal, (int)rankedMoves.size());
                        const Move& cm = rankedMoves[pick].m;
                        px = cm.sx; py = cm.sy; pdx = cm.dx;
                        ordMoves++;
                        if (rankedMoves[pick].tier != 3) ordNonBest++;
                    }
                    victor = replayMoveTD(px, py, pdx, side);
                    if (gameOutcomeTD(victor)) break;
                }
                int after = (side == White) ? Black : White;

                if (backupKind == BK_LEAF) {
                    // The root move just played is PV ply 1; walk the remaining
                    // depth-1 plies through the table the search just filled.
                    int leafSide = walkPV(after, cfg.depth - 1, steps);
                    int reached = 1 + (int)steps.size();
                    pvDepthSum += reached; pvCount++;
                    if (reached < cfg.depth) pvTruncated++;

                    int decided = nearWinCheck(leafSide);
                    if (decided != 0) {
                        // Saturated leaf: its value is known, so record it for TD
                        // continuity but do not push a gradient into a certainty.
                        p.push_back(decided >= WhiteWin ? 1.0 : 0.0);
                        trainable.push_back(0);
                        leafFeat.push_back(std::vector<float>());
                        tgt.push_back(0.0);
                        pMir.push_back(0.0);
                        mirFeat.push_back(std::vector<float>());
                        skippedDecided++;
                    } else {
                        extractFeatTD(featVer, leafSide, feat.data());
                        recordTrained(feat.data(),
                                      sigmoidTD((double)model->forward(feat.data(), featCount)), 0.0);
                    }
                    unwindPV(steps);
                } else {
                    double pr = sigmoidTD((double)model->forward(rootFeat.data(), featCount));
                    double q  = tdScoreToProb(rootScore, walk.outScale);
                    if (backupKind == BK_TREE)
                        treeAccumulateTD(walk, rootFeat.data(), q, TT_EXACT);   // the root's own score is exact
                    else
                        recordTrained(rootFeat.data(), pr, (backupKind == BK_ROOT) ? q : 0.0);
                }
            }
        }

        int oc = gameOutcomeTD(victor);
        if (oc == 1) wWins++; else if (oc == 2) bWins++; else draws++;
        gamePliesTotal += gamePlies;
        double z = tdTerminalTarget(oc, gamePlies, depthReward, TD_MAX_GAME_PLIES);

        if (backupKind == BK_ROOT) {
            // RootStrap: each root toward its own search score, no lambda-return.
            gOut.assign(p.size(), 0.0);
            for (size_t t = 0; t < p.size(); t++) gOut[t] = p[t] - tgt[t];
        } else {
            tdLeafGradients(p, z, cfg.lambda, gOut);   // empty for treestrap
        }

        auto applyOne = [&](const std::vector<float>& x, double gv) {
            if (cfg.batchGames <= 1) {
                head->gradStep(x.data(), featCount, (float)gv, (float)effLr, (float)cfg.l2);
            } else {
                batchX.push_back(x);
                batchG.push_back(gv);
            }
        };
        for (size_t t = 0; t < gOut.size(); t++) {
            if (!trainable[t]) continue;
            applyOne(leafFeat[t], gOut[t]);
            trainedPositions++;
            updates++;
            if (mirror) {
                // The mirror moves toward the SAME target, p - gOut, from its
                // own value under the same weights.
                applyOne(mirFeat[t], pMir[t] - (p[t] - gOut[t]));
                updates++; mirrorUpdates++;
            }
        }
        if (treeLinear && cfg.batchGames <= 1)
            applyDenseTD(treeLinear, walk.G, walk.Gb, effLr, cfg.l2);

        if (cfg.batchGames > 1 && ((g + 1) % cfg.batchGames == 0)) {
            for (size_t i = 0; i < batchX.size(); i++)
                head->gradStep(batchX[i].data(), featCount, (float)batchG[i],
                               (float)effLr, (float)cfg.l2);
            batchX.clear(); batchG.clear();
            if (treeLinear) applyDenseTD(treeLinear, walk.G, walk.Gb, effLr, cfg.l2);
        }

        budget.units++;   // one completed game

        if (cfg.reportEvery > 0 && ((g + 1) % cfg.reportEvery == 0)) {
            cout << "  game " << (g + 1);
            if (totalGames > 0) cout << "/" << totalGames;
            cout << "  W-B-D " << wWins << "-" << bWins << "-" << draws
                 << "  trained " << trainedPositions
                 << "  meanPV " << (pvCount ? (double)pvDepthSum / pvCount : 0.0)
                 << "  elapsed " << tbElapsed(budget) << "s"
                 << "  nodes " << tbNodes(budget)
                 << "\n";
            cout.flush();
        }
        if (cfg.ckptEvery > 0 && ((g + 1) % cfg.ckptEvery == 0))
            saver.save(cfg.outPath + "_ckpt" + std::to_string(g + 1) + ".txt");
        for (size_t k = 0; k < cfg.ckptAt.size(); k++)
            if (cfg.ckptAt[k] == g + 1) {
                string lp = cfg.outPath + "_g" + std::to_string(g + 1) + ".txt";
                saver.save(lp);
                cout << "  [ladder] " << (g + 1) << " games -> " << lp << "\n";
                cout.flush();
            }
        // Wall-clock rungs. A loop, not an if: one long game can carry the run
        // past several marks, and every rung the ladder asked for must exist.
        double mark = 0.0;
        while (tbTakeDueMark(budget, mark)) {
            string wp = tbMarkPath(cfg.outPath, mark);
            saver.save(wp);
            cout << "  [wall] " << mark << "s mark (" << tbElapsed(budget)
                 << "s actual, " << (g + 1) << " games) -> " << wp << "\n";
            cout.flush();
        }
    }

    // Flush a partial batch so no game's signal is silently dropped.
    if (cfg.batchGames > 1 && !batchX.empty()) {
        for (size_t i = 0; i < batchX.size(); i++)
            head->gradStep(batchX[i].data(), featCount, (float)batchG[i],
                           (float)effLr, (float)cfg.l2);
        batchX.clear(); batchG.clear();
    }
    if (treeLinear && (walk.Gb != 0.0
                       || std::any_of(walk.G.begin(), walk.G.end(), [](double v) { return v != 0.0; })))
        applyDenseTD(treeLinear, walk.G, walk.Gb, effLr, cfg.l2);

    // A wall-clock stop can land between two marks, so write any rung the run
    // reached but had not yet checkpointed before the final save.
    {
        double mark = 0.0;
        while (tbTakeDueMark(budget, mark)) {
            string wp = tbMarkPath(cfg.outPath, mark);
            saver.save(wp);
            cout << "  [wall] " << mark << "s mark (" << tbElapsed(budget)
                 << "s actual) -> " << wp << "\n";
        }
    }

    const string outFile = cfg.outPath + ".txt";
    bool saved = saver.save(outFile);

    // W/B/D count THIS process's games; the cumulative total also covers any
    // games a --resume carried in, whose outcomes this process never saw.
    cout << "\nTD-Leaf done: " << budget.units << " games this run ("
         << wWins << " W / " << bWins << " B / " << draws << " draw)"
         << (stoppedOnWall ? "  [stopped on wall clock]" : "") << "\n";
    cout << "  cumulative: " << tbUnits(budget) << " games\n";
    cout << "  training compute: " << tbElapsed(budget) << " s wall, "
         << tbCpu(budget) << " s cpu, "
         << tbNodes(budget) << " search nodes";
    if (tbElapsed(budget) > 0.0)
        cout << "  (" << (double)tbNodes(budget) / tbElapsed(budget) << " nodes/s)";
    cout << "\n";
    // TreeStrap trains through the dense accumulator (its line is below), and
    // only td-leaf walks a PV, so each line prints only where it measures something.
    if (backupKind != BK_TREE)
        cout << "  trained positions: " << trainedPositions
             << "   skipped (decided leaf): " << skippedDecided << "\n";
    g_tdLastMeanPV = pvCount ? (double)pvDepthSum / pvCount : 0.0;
    if (backupKind == BK_LEAF) {
        cout << "  mean PV depth reached: "
             << g_tdLastMeanPV << " of " << cfg.depth
             << "   truncated: " << pvTruncated << "/" << pvCount;
        if (pvCount) cout << " (" << (100.0 * pvTruncated / pvCount) << "%)";
        cout << "\n";
    }
    const long long gamesRun = (long long)budget.units;
    cout << "  backup " << cfg.backup << "   searched moves " << searches
         << "   mean game length " << (gamesRun ? (double)gamePliesTotal / gamesRun : 0.0)
         << " plies\n";
    if (backupKind == BK_TREE) {
        updates = walk.updated + walk.mirrorUpdated;
        mirrorUpdates = walk.mirrorUpdated;
        cout << "  treestrap (d_min " << cfg.treeMinDepth << "): table entries accepted "
             << walk.accepted << " ("
             << (searches ? (double)walk.accepted / searches : 0.0) << " per search),"
             << " updates that moved " << walk.updated << "\n";
    }
    if (mirror)
        cout << "  mirror: " << mirrorUpdates << " of " << updates << " updates were mirrored\n";
    if (ordinal)
        cout << "  ordinal: " << ordMoves << " drawn, " << ordNonBest
             << " not the search's choice; alternatives ranked by table entry "
             << ordRanked << ", by static eval " << ordUnranked << "\n";

    g_tdLastRun.games = gamesRun;
    g_tdLastRun.gamePlies = gamePliesTotal;
    g_tdLastRun.searches = searches;
    g_tdLastRun.nodes = tbNodes(budget) - budget.priorNodes;
    g_tdLastRun.updates = updates;
    g_tdLastRun.mirrorUpdates = mirrorUpdates;
    g_tdLastRun.treeNodes = walk.accepted;
    g_tdLastRun.treeUpdated = walk.updated;
    g_tdLastRun.ordinalMoves = ordMoves;
    g_tdLastRun.ordinalNonBest = ordNonBest;
    g_tdLastRun.ordinalRanked = ordRanked;
    g_tdLastRun.ordinalUnranked = ordUnranked;
    g_tdLastRun.wWins = wWins; g_tdLastRun.bWins = bWins; g_tdLastRun.draws = draws;
    cout << "  model -> " << outFile << (saved ? "" : "  (SAVE FAILED)") << "\n";
    cout << "  provenance: " << model->teacher << "\n";

    mlClearSlots();                 // frees the model
    return saved ? 0 : 1;
}
