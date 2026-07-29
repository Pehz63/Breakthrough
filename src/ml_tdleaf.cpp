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
#include <vector>
#include <cmath>
#include <sstream>
#include <climits>
#include <algorithm>

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

// ============================================================
// SMALL UTILITIES
// ============================================================
static double frandTD() { return (double)rand() / ((double)RAND_MAX + 1.0); }

static double sigmoidTD(double z) {
    if (z >= 0) { double e = exp(-z); return 1.0 / (1.0 + e); }
    double e = exp(z); return e / (1.0 + e);
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
        ttProbe((unsigned long long)pk.hash, 0, INT_MIN, INT_MAX, sc, fromSq, toSq);
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
    c.lambda      = 0.7;
    c.lr          = 0.01;
    c.l2          = 0.0;
    c.seed        = 1001;
    c.openPlies   = 4;
    c.explore     = 0.0;
    c.batchGames  = 1;
    c.modelType   = "linear";
    c.ckptEvery   = 0;
    c.reportEvery = 50;
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
int trainTDLeaf(const TDLeafConfig& cfg) {
    srand(cfg.seed);
    PRNT = 0;

    // ---- Model: load an initialisation or build a fresh one ----
    Model* model = nullptr;
    string provInit;
    if (!cfg.initModel.empty()) {
        model = loadModel(cfg.initModel);
        if (!model) { cout << "ERROR: cannot load init model " << cfg.initModel << "\n"; return 1; }
        if (model->head() != HEAD_VALUE) {
            cout << "ERROR: " << cfg.initModel << " is not a value model.\n";
            delete model; return 1;
        }
        provInit = "init:" + cfg.initModel;
    } else {
        const int featVer = 2, featCount = MLV2_FEATURES;
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

    // The slot owns the model from here (mlClearSlots frees it), and search reads
    // the SAME object being trained, so each game is played by the current weights.
    const int slot = ML_SLOTS - 2;
    mlSetModel(slot, model);

    // ---- Self-play agent ----
    AgentSpec agent = agentMakeSearch("tdleaf", explorerIdxTD("AlphaBeta"),
                                      learnedValueIndex(), cfg.depth, slot);
    agent.nodeBudget    = cfg.nodeBudget;
    agent.useAlphaBeta  = true;
    agent.useTT         = true;      // REQUIRED: the PV walk reads this table
    agent.useMoveOrder  = true;
    agent.randomMoveProb = 0.0;      // exploration is handled explicitly below

    {
        std::ostringstream prov;
        prov << "tdleaf(lambda=" << cfg.lambda << ",lr=" << cfg.lr << ",l2=" << cfg.l2
             << ",d" << cfg.depth;
        if (cfg.nodeBudget) prov << ",nb" << cfg.nodeBudget;
        prov << ",games=" << cfg.games << ",batch=" << cfg.batchGames
             << ",open=" << cfg.openPlies << ",explore=" << cfg.explore
             << ",seed=" << cfg.seed << ") " << provInit;
        model->teacher = prov.str();
    }
    cout << "TD-Leaf: " << model->teacher << "\n";
    cout << "Model: type=" << model->typeName() << " featVer=" << featVer
         << " trainable=" << head->typeName() << "\n";

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

    // Run at least as far as the highest ladder rung, so `--ckpt-at` alone is
    // enough to specify a run and no rung is silently never written.
    const int totalGames = std::max(cfg.games, maxLadderRung(cfg.ckptAt));

    for (int g = 0; g < totalGames; g++) {
        // Independence: a stale table would make a game's result depend on which
        // games preceded it (the cross-game TT pollution defect fixed elsewhere in
        // this project). Every game starts from a clean table.
        ttClear();
        reloadBoard(cfg.boardFile);

        p.clear(); trainable.clear(); leafFeat.clear();
        int victor = None;

        // Random opening for position diversity. Deterministic openers would make
        // self-play replay one game forever.
        for (int h = 0; h < cfg.openPlies * 2; h++) {
            int side = (h % 2 == 0) ? White : Black;
            victor = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
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
                bool exploreMove = (cfg.explore > 0.0 && frandTD() < cfg.explore);
                if (exploreMove || nearWinCheck(side) != 0) {
                    victor = exploreMove
                        ? ((side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack())
                        : agentChooseMove(agent, side);
                    if (gameOutcomeTD(victor)) break;
                    continue;
                }

                victor = agentChooseMove(agent, side);
                if (gameOutcomeTD(victor)) break;   // this move ended it: no leaf to capture
                int after = (side == White) ? Black : White;

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
                    skippedDecided++;
                } else {
                    if (featVer == 2) mlExtractValueFeaturesV2(leafSide, feat.data());
                    else              mlExtractValueFeatures(leafSide, feat.data());
                    p.push_back(sigmoidTD((double)model->forward(feat.data(), featCount)));
                    trainable.push_back(1);
                    leafFeat.push_back(feat);
                }
                unwindPV(steps);
            }
        }

        int oc = gameOutcomeTD(victor);
        if (oc == 1) wWins++; else if (oc == 2) bWins++; else draws++;
        double z = (oc == 1) ? 1.0 : (oc == 2) ? 0.0 : 0.5;

        tdLeafGradients(p, z, cfg.lambda, gOut);

        for (size_t t = 0; t < gOut.size(); t++) {
            if (!trainable[t]) continue;
            if (cfg.batchGames <= 1) {
                head->gradStep(leafFeat[t].data(), featCount, (float)gOut[t],
                               (float)cfg.lr, (float)cfg.l2);
            } else {
                batchX.push_back(leafFeat[t]);
                batchG.push_back(gOut[t]);
            }
            trainedPositions++;
        }

        if (cfg.batchGames > 1 && ((g + 1) % cfg.batchGames == 0)) {
            for (size_t i = 0; i < batchX.size(); i++)
                head->gradStep(batchX[i].data(), featCount, (float)batchG[i],
                               (float)cfg.lr, (float)cfg.l2);
            batchX.clear(); batchG.clear();
        }

        if (cfg.reportEvery > 0 && ((g + 1) % cfg.reportEvery == 0)) {
            cout << "  game " << (g + 1) << "/" << totalGames
                 << "  W-B-D " << wWins << "-" << bWins << "-" << draws
                 << "  trained " << trainedPositions
                 << "  meanPV " << (pvCount ? (double)pvDepthSum / pvCount : 0.0)
                 << "\n";
            cout.flush();
        }
        if (cfg.ckptEvery > 0 && ((g + 1) % cfg.ckptEvery == 0))
            model->save(cfg.outPath + "_ckpt" + std::to_string(g + 1) + ".txt");
        for (size_t k = 0; k < cfg.ckptAt.size(); k++)
            if (cfg.ckptAt[k] == g + 1) {
                string lp = cfg.outPath + "_g" + std::to_string(g + 1) + ".txt";
                model->save(lp);
                cout << "  [ladder] " << (g + 1) << " games -> " << lp << "\n";
                cout.flush();
            }
    }

    // Flush a partial batch so no game's signal is silently dropped.
    if (cfg.batchGames > 1 && !batchX.empty()) {
        for (size_t i = 0; i < batchX.size(); i++)
            head->gradStep(batchX[i].data(), featCount, (float)batchG[i],
                           (float)cfg.lr, (float)cfg.l2);
        batchX.clear(); batchG.clear();
    }

    const string outFile = cfg.outPath + ".txt";
    bool saved = model->save(outFile);

    cout << "\nTD-Leaf done: " << cfg.games << " games (" << wWins << " W / "
         << bWins << " B / " << draws << " draw)\n";
    cout << "  trained positions: " << trainedPositions
         << "   skipped (decided leaf): " << skippedDecided << "\n";
    cout << "  mean PV depth reached: "
         << (pvCount ? (double)pvDepthSum / pvCount : 0.0) << " of " << cfg.depth
         << "   truncated: " << pvTruncated << "/" << pvCount;
    if (pvCount) cout << " (" << (100.0 * pvTruncated / pvCount) << "%)";
    cout << "\n";
    cout << "  model -> " << outFile << (saved ? "" : "  (SAVE FAILED)") << "\n";

    mlClearSlots();                 // frees the model
    return saved ? 0 : 1;
}
