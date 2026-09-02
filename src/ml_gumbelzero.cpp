#include "ml_gumbelzero.h"
#include "train_budget.h"
#include "ai_gumbel.h"
#include "ml_model.h"
#include "ml_eval.h"
#include <cmath>
#include <sstream>
#include <algorithm>

// ============================================================
// POLICY GRADIENT CORE
// ============================================================
// Pure function, no board/model state, so tests can assert the closed form
// directly (mirrors ml_tdleaf.h's tdLeafGradients pattern).
void gumbelPolicyGradients(const double* logits, const double* target, int n, double* gOut) {
    if (n <= 0) return;
    double top = -1e300;
    for (int i = 0; i < n; i++) if (logits[i] > top) top = logits[i];
    double q[ML_MAX_MOVES];
    double sum = 0.0;
    for (int i = 0; i < n; i++) { q[i] = exp(logits[i] - top); sum += q[i]; }
    if (sum <= 0.0) sum = 1.0;   // degenerate guard, never hit (exp of the max term is 1)
    for (int i = 0; i < n; i++) gOut[i] = (q[i] / sum) - target[i];
}

// ============================================================
// REPLAY BUFFER
// ============================================================
GumbelZeroReplayBuffer::GumbelZeroReplayBuffer(int capacity) : cap_(capacity > 0 ? capacity : 1) {
    buf_.reserve(cap_);
}

void GumbelZeroReplayBuffer::push(const GumbelZeroRecord& r) {
    if ((int)buf_.size() < cap_) {
        buf_.push_back(r);
    } else {
        buf_[next_] = r;
        next_ = (next_ + 1) % cap_;
    }
}

void GumbelZeroReplayBuffer::sample(int n, std::vector<const GumbelZeroRecord*>& out) const {
    out.clear();
    int sz = (int)buf_.size();
    if (sz <= 0 || n <= 0) return;
    if (n >= sz) {
        for (int i = 0; i < sz; i++) out.push_back(&buf_[i]);
        return;
    }
    // Partial Fisher-Yates: shuffle just the first n slots of an index array,
    // so a minibatch never samples the same ply twice.
    std::vector<int> idx(sz);
    for (int i = 0; i < sz; i++) idx[i] = i;
    for (int i = 0; i < n; i++) {
        int j = i + rand() % (sz - i);
        std::swap(idx[i], idx[j]);
        out.push_back(&buf_[idx[i]]);
    }
}

// ============================================================
// SMALL UTILITIES
// ============================================================
static int gameOutcomeGZ(int victor) {
    if (victor >= WhiteWin) return 1;
    if (victor <= BlackWin) return 2;
    return 0;
}

// Highest ladder rung, so the run knows when it may stop early (mirrors
// ml_tdleaf.cpp's maxLadderRung).
static int maxLadderRungGZ(const std::vector<int>& v) {
    int m = 0;
    for (size_t i = 0; i < v.size(); i++) if (v[i] > m) m = v[i];
    return m;
}

// ============================================================
// CONFIG
// ============================================================
GumbelZeroConfig gumbelZeroDefaults() {
    GumbelZeroConfig c;
    c.outPath        = "models/gumbelzero";
    c.boardFile      = "boards/board1.txt";
    c.games          = 50;
    c.simBudget      = 50;    // matches Slice 1's own smoke-tested gaz(sims=50) value
    c.seed           = 1001;
    c.openPlies      = 4;     // matches TD-Leaf's own default diversity window
    c.modelType      = "linear";
    // c.mlpHidden and c.convChannels default-empty (populated with the
    // regime's own defaults inside trainGumbelZero when the relevant
    // modelType is selected and the list was left empty).
    c.lr             = 0.01;
    c.l2             = 0.0;
    c.replayCapacity = 2000;
    c.replayWarmup   = 32;
    c.batchSize      = 32;
    c.ckptEvery      = 0;
    c.reportEvery    = 10;
    c.wallStopSec    = 0.0;
    c.resumeFrom     = "";
    return c;
}

// ============================================================
// REGIME
// ============================================================
int trainGumbelZero(const GumbelZeroConfig& cfg) {
    srand(cfg.seed);
    PRNT = 0;

    // ---- Model: from scratch only in this pass. Architecture is selectable
    // (cfg.modelType/mlpHidden/convChannels, mirroring ml_train.cpp's
    // selfplay-supervised --model-type/--mlp-hidden). "linear"/"mlp" apply to
    // BOTH heads as separate Model instances of the same architecture. "conv"
    // applies to the VALUE head only -- its v2 board features are a real
    // 8x8/2-plane image, but the policy head's 9 handcrafted move features
    // are not spatial, so a conv run's policy head stays the linear scorer
    // (see ml_model.h's ConvModel doc comment). "linear" heads stay
    // zero-initialized (Slice 1's own smoke-tested construction,
    // tests/test_gumbelzero.cpp); "mlp"/"conv" heads need their model's
    // initRandom() to break weight symmetry, since a zero-initialized hidden
    // layer can never learn (ml_model.h).
    const bool useMlp       = (cfg.modelType == "mlp");
    const bool useConv      = (cfg.modelType == "conv");
    const bool usePolicyMlp = useMlp || cfg.policyMlp;
    std::vector<int> hidden = cfg.mlpHidden;
    if (usePolicyMlp && hidden.empty()) hidden.push_back(32);   // default one 32-wide hidden layer

    std::vector<int> convChannels = cfg.convChannels;
    if (useConv && convChannels.empty()) { convChannels.push_back(16); convChannels.push_back(16); }
    const std::vector<int>& convFcHidden = cfg.mlpHidden;   // reused: conv FC head's own hidden layers (empty = direct linear read-out)

    Model* valueHead;
    if (useConv)      valueHead = new ConvModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, convChannels, convFcHidden);
    else if (useMlp)  valueHead = new MLPModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f, hidden);
    else              valueHead = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);

    Model* policyHead = usePolicyMlp ? (Model*)new MLPModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f, hidden)
                                      : (Model*)new LinearModel(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    if (useMlp)  static_cast<MLPModel*>(valueHead)->initRandom();
    else if (useConv) static_cast<ConvModel*>(valueHead)->initRandom();
    if (usePolicyMlp) static_cast<MLPModel*>(policyHead)->initRandom();
    JointModel*  model = new JointModel(valueHead, policyHead);

    // ---- Training-compute meter and wall-clock ladder (src/train_budget.h) ----
    TrainBudget budget = tbDefaults();
    budget.wallCkptAt  = cfg.wallCkptAt;
    budget.wallStopSec = cfg.wallStopSec;

    // ---- Resume: continue a previous run's ladder with its weights ----
    if (!cfg.resumeFrom.empty()) {
        Model* prev = loadModel(cfg.resumeFrom);
        if (!prev) { cout << "ERROR: cannot load resume model " << cfg.resumeFrom << "\n"; delete model; return 1; }
        JointModel* pj = dynamic_cast<JointModel*>(prev);
        if (!pj) {
            cout << "ERROR: " << cfg.resumeFrom << " is not a joint model.\n";
            delete prev; delete model; return 1;
        }
        if (!tbParsePrior(prev->teacher, budget, "games")) {
            cout << "ERROR: " << cfg.resumeFrom << " carries no spend stamp"
                 << " (games=/secs=/nodes=), so its ladder cannot be continued.\n";
            delete prev; delete model; return 1;
        }
        delete model;                 // the freshly-built architecture is discarded
        model = pj;
        valueHead  = pj->valueHead;
        policyHead = pj->policyHead;
        cout << "Resuming from " << cfg.resumeFrom << ": "
             << budget.priorUnits << " games, " << budget.priorSec << " s, "
             << budget.priorNodes << " nodes already spent\n";
    }

    // Provenance splits into the RECIPE (fixed by the command line) and the
    // SPEND (only knowable at a save point). The recipe is built once here, the
    // spend appended at each save by the saver below -- see trainTDLeaf.
    string provRecipe, provTail;
    {
        std::ostringstream prov;
        prov << "gumbelzero(sims=" << cfg.simBudget << ",lr=" << cfg.lr << ",l2=" << cfg.l2
             << ",replay=" << cfg.replayCapacity << ",warmup=" << cfg.replayWarmup
             << ",batch=" << cfg.batchSize
             << ",open=" << cfg.openPlies << ",seed=" << cfg.seed;
        provRecipe = prov.str();

        std::ostringstream tail;
        tail << ") " << (cfg.resumeFrom.empty() ? string("init:scratch")
                                                : ("resume:" + cfg.resumeFrom));
        if (useMlp) {
            tail << " mlp(";
            for (size_t i = 0; i < hidden.size(); i++) { if (i) tail << ","; tail << hidden[i]; }
            tail << ")";
        } else if (useConv) {
            tail << " conv(ch=";
            for (size_t i = 0; i < convChannels.size(); i++) { if (i) tail << ","; tail << convChannels[i]; }
            tail << ";fc=";
            for (size_t i = 0; i < convFcHidden.size(); i++) { if (i) tail << ","; tail << convFcHidden[i]; }
            tail << ";policy=" << (cfg.policyMlp ? "mlp" : "linear");
            tail << ")";
        }
        provTail = tail.str();
    }
    // Attach the spend this checkpoint actually represents, then save. Every
    // save in this function goes through here (see trainTDLeaf's header).
    struct Saver {
        Model* model; const string& recipe; const string& tail; const TrainBudget& b;
        bool save(const string& path) const {
            model->teacher = recipe + tbStamp(b, "games") + tail;
            return model->save(path);
        }
    } saver = { model, provRecipe, provTail, budget };

    cout << "Gumbel-Zero: " << provRecipe << ",..." << provTail << "\n";
    if (cfg.wallStopSec > 0.0 || !cfg.wallCkptAt.empty()) {
        cout << "Wall ladder:";
        for (size_t k = 0; k < cfg.wallCkptAt.size(); k++) cout << " " << cfg.wallCkptAt[k] << "s";
        if (cfg.wallStopSec > 0.0) cout << "   stop at " << cfg.wallStopSec << "s";
        cout << "  (cumulative)\n";
    }

    // Scratch slot for the model actually being trained AND searched (search
    // reads the SAME object being trained, so each game is played by the
    // current weights); picked from the permanently test/scratch-reserved
    // range (ml_eval.h's ML_RESERVED_SLOTS) so it can never collide with a
    // live roster agent's published slot.
    const int slot = ML_SLOTS - 3;
    mlSetModel(slot, model);

    GumbelZeroReplayBuffer buffer(cfg.replayCapacity);

    long long trainedSteps = 0;
    int wWins = 0, bWins = 0, draws = 0;

    // A non-positive game count with no game-count ladder means the wall clock
    // alone governs the run length (mirrors ml_tdleaf.cpp).
    const int totalGames = (cfg.games <= 0 && cfg.ckptAt.empty())
                         ? 0 : std::max(cfg.games, maxLadderRungGZ(cfg.ckptAt));

    std::vector<const GumbelZeroRecord*> batch;
    double logits[ML_MAX_MOVES], gOut[ML_MAX_MOVES];

    tbBegin(budget);
    bool stoppedOnWall = false;
    const int gameBase = (int)budget.priorUnits;

    for (int g = gameBase; totalGames <= 0 || g < totalGames; g++) {
        // Checked between games, like TD-Leaf's: a game is the smallest unit
        // that produces a complete set of replay records.
        if (tbShouldStop(budget)) { stoppedOnWall = true; break; }
        reloadBoard(cfg.boardFile);
        int victor = None;

        // Random opening for position diversity (matches TD-Leaf's own
        // convention). No separate per-move exploration knob is needed the
        // way TD-Leaf needs --explore: Gumbel-top-k already draws fresh
        // randomness at the root of every move.
        for (int h = 0; h < cfg.openPlies * 2; h++) {
            int side = (h % 2 == 0) ? White : Black;
            victor = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            if (gameOutcomeGZ(victor)) break;
        }

        if (!gameOutcomeGZ(victor)) {
            for (int h = 0; h < 400; h++) {
                int side = (h % 2 == 0) ? White : Black;

                Move rootMoves[ML_MAX_MOVES];
                int n = generateMoves(side, rootMoves);
                if (n == 0) {
                    // No legal moves for `side`: an immediate loss, exactly
                    // what gumbelSearch itself would report -- decided here
                    // so info is never touched while genuinely uninitialized.
                    victor = (side == White) ? BlackWin : WhiteWin;
                    break;
                }

                GumbelZeroRecord rec;
                rec.moveCount = n;
                mlExtractValueFeaturesV2(side, rec.boardFeatures);
                for (int i = 0; i < n; i++)
                    mlExtractMoveFeatures(rootMoves[i], side, rec.moveFeatures[i]);

                GumbelRootInfo info;
                victor = gumbelSearch(side, slot, cfg.simBudget, &info);

                // info's move order matches rootMoves': both come from the
                // identical, deterministic generateMoves(side,...) call on
                // this same board state, with no move played in between.
                // info.moveCount == 0 only via gumbelSearch's own
                // immediate-win shortcut (it found and played a winning move
                // without populating info) -- nothing to train on that ply.
                if (info.moveCount == n) {
                    rec.valueTarget = (info.searchValue + 1.0) / 2.0;
                    gumbelImprovedPolicy(info, rec.policyTarget);
                    buffer.push(rec);
                }

                if (buffer.size() >= cfg.replayWarmup) {
                    buffer.sample(cfg.batchSize, batch);
                    for (size_t bi = 0; bi < batch.size(); bi++) {
                        const GumbelZeroRecord* r = batch[bi];
                        valueHead->trainStep(r->boardFeatures, MLV2_FEATURES, (float)r->valueTarget,
                                             (float)cfg.lr, (float)cfg.l2, 0.0f);

                        for (int m = 0; m < r->moveCount; m++)
                            logits[m] = policyHead->forward(r->moveFeatures[m], MLM_FEATURES);
                        gumbelPolicyGradients(logits, r->policyTarget, r->moveCount, gOut);
                        for (int m = 0; m < r->moveCount; m++)
                            policyHead->gradStep(r->moveFeatures[m], MLM_FEATURES, (float)gOut[m],
                                                 (float)cfg.lr, (float)cfg.l2);
                        trainedSteps++;
                    }
                }

                if (gameOutcomeGZ(victor)) break;
            }
        }

        int oc = gameOutcomeGZ(victor);
        if (oc == 1) wWins++; else if (oc == 2) bWins++; else draws++;
        budget.units++;   // one completed game

        if (cfg.reportEvery > 0 && ((g + 1) % cfg.reportEvery == 0)) {
            cout << "  game " << (g + 1);
            if (totalGames > 0) cout << "/" << totalGames;
            cout << "  W-B-D " << wWins << "-" << bWins << "-" << draws
                 << "  buffer " << buffer.size() << "/" << buffer.capacity()
                 << "  trained " << trainedSteps
                 << "  elapsed " << tbElapsed(budget) << "s"
                 << "  nodes " << tbNodes(budget) << "\n";
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
        double mark = 0.0;
        while (tbTakeDueMark(budget, mark)) {
            string wp = tbMarkPath(cfg.outPath, mark);
            saver.save(wp);
            cout << "  [wall] " << mark << "s mark (" << tbElapsed(budget)
                 << "s actual, " << (g + 1) << " games) -> " << wp << "\n";
            cout.flush();
        }
    }

    // A wall stop can land between two marks: write any rung the run reached
    // but had not yet checkpointed.
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

    // W/B/D count THIS process's games; the cumulative total also covers games
    // a --resume carried in.
    cout << "\nGumbel-Zero done: " << budget.units << " games this run ("
         << wWins << " W / " << bWins << " B / " << draws << " draw)"
         << (stoppedOnWall ? "  [stopped on wall clock]" : "") << "\n";
    cout << "  cumulative: " << tbUnits(budget) << " games\n";
    cout << "  training compute: " << tbElapsed(budget) << " s wall, "
         << tbNodes(budget) << " search nodes\n";
    cout << "  trained steps: " << trainedSteps
         << "   replay buffer: " << buffer.size() << "/" << buffer.capacity() << "\n";
    cout << "  model -> " << outFile << (saved ? "" : "  (SAVE FAILED)") << "\n";
    cout << "  provenance: " << model->teacher << "\n";

    mlClearSlots();                 // frees the model
    return saved ? 0 : 1;
}
