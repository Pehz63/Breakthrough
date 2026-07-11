// Windows headers MUST precede the project headers: globals.h does `#define SIZE 8`,
// which would otherwise mangle wingdi.h's `SIZE` struct. NOMINMAX keeps std::min/max.
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#endif
#include "ml_train.h"
#include "explorers.h"
#include "choosers.h"
#include "ai_eval.h"
#include "ml_eval.h"
#include "ml_model.h"
#include "ml_features.h"
#include "datastore.h"
#include "board_io.h"
#include "board_analysis.h"
#include "moves.h"
#include "ai_random.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <sstream>
#include <chrono>
#include <map>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// Emit a process-level resource line (peak working set + total CPU seconds). One shared
// process serves all agents, so this is a run-level figure, not cleanly per-agent.
static void emitResourceLine(std::ofstream& out) {
#ifdef _WIN32
    double peakMB = 0.0, cpuS = 0.0;
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        peakMB = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
    FILETIME ct, et, kt, ut;
    if (GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
        u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
        cpuS = (double)(k.QuadPart + u.QuadPart) / 1e7;   // 100ns ticks -> seconds
    }
    out << "{\"resource\":1,\"peak_ws_mb\":" << peakMB << ",\"cpu_s\":" << cpuS << "}\n";
#else
    (void)out;
#endif
}

// ============================================================
// REGISTRY LOOKUPS + SMALL UTILITIES
// ============================================================
// Create a directory if it does not already exist (no error if it does).
static void ensureDir(const string& p) {
#ifdef _WIN32
    _mkdir(p.c_str());
#else
    mkdir(p.c_str(), 0755);
#endif
}

static int explorerIndexByName(const char* n) {
    for (int i = 0; i < g_explorerCount; i++) if (string(g_explorers[i].name) == n) return i;
    return 0;
}
static int chooserIndexByName(const char* n) {
    for (int i = 0; i < g_chooserCount; i++) if (string(g_choosers[i].name) == n) return i;
    return 0;
}
static int evaluatorIndexByName(const char* n) {
    for (int i = 0; i < g_evalCount; i++) if (string(g_evaluators[i].name) == n) return i;
    return 0;
}
static double frand() { return (double)rand() / (double)RAND_MAX; }
static float  randWeight() { return (float)((frand() * 2.0 - 1.0) * 0.05); }

// Forward declaration: defined near the JSONL field extractors below (~line 560),
// used by trainSupervisedValue's --from-data path (see ml_train.h).
static bool loadReplayDataset(const string& path, int featVer, int featCount,
                              std::vector<std::vector<float> >& X, std::vector<float>& Y,
                              string& err);

template <class T> static void shuffleVec(std::vector<T>& v) {
    for (size_t i = v.size(); i > 1; i--) {
        size_t j = (size_t)(frand() * i);
        if (j >= i) j = i - 1;
        std::swap(v[i-1], v[j]);
    }
}

// Victor code -> 1 (White won), 2 (Black won), 0 (ongoing / draw). Mirrors the
// engine's own convention (see main.cpp's game loop).
static int gameOutcome(int victor) {
    if (victor >= WhiteWin) return 1;
    if (victor <= BlackWin) return 2;
    return 0;
}

// ============================================================
// ELO
// ============================================================
double eloExpected(double ra, double rb) {
    return 1.0 / (1.0 + pow(10.0, (rb - ra) / 400.0));
}
void eloUpdate(double& ra, double& rb, double scoreA, double k) {
    double e = eloExpected(ra, rb);
    ra += k * (scoreA - e);
    rb += k * ((1.0 - scoreA) - (1.0 - e));
}
static double winrateToElo(double wr) {
    if (wr < 0.01) wr = 0.01;
    if (wr > 0.99) wr = 0.99;
    return 1500.0 + 400.0 * log10(wr / (1.0 - wr));
}

// ============================================================
// GAME RUNNER (with optional position capture)
// ============================================================
// One captured position: value features + side to move + its canonical key, so
// the trainer can both learn from it and emit it to the datastore for analysis.
struct PosCap {
    std::vector<float>  f;
    int                 side;
    unsigned long long  hash;
    string              enc;
};

// genRandomDecayPlies > 0 linearly decays each side's randomMoveProb from its
// starting value (at ply 0) to genRandomFloor by that many half-moves, then
// holds at the floor -- explore early, play the late (most outcome-informative)
// moves for real. 0 (the default) is the historical constant-probability
// behavior; existing callers are unaffected.
static int playGame(const AgentSpec& w, const AgentSpec& b, const string& boardFile,
                    int maxHalf, std::vector<PosCap>* cap, int featVer = 1,
                    double genRandomFloor = 0.0, int genRandomDecayPlies = 0) {
    reloadBoard(boardFile);
    int victor = None;
    bool decay = genRandomDecayPlies > 0;
    AgentSpec ww = w, bb = b;
    for (int h = 0; h < maxHalf; h++) {
        int side = (h % 2 == 0) ? White : Black;
        if (decay) {
            double t = std::min(1.0, h / (double)genRandomDecayPlies);
            double p = w.randomMoveProb * (1.0 - t) + genRandomFloor * t;
            ww.randomMoveProb = p; bb.randomMoveProb = p;
        }
        if (cap) {
            PosCap pc;
            if (featVer == 2) {
                pc.f.resize(MLV2_FEATURES);
                mlExtractValueFeaturesV2(side, pc.f.data());
            } else {
                pc.f.resize(MLV_FEATURES);
                mlExtractValueFeatures(side, pc.f.data());
            }
            pc.side = side;
            PosKey k = positionKey(side, true);
            pc.hash = k.hash;
            pc.enc  = k.enc;
            cap->push_back(pc);
        }
        victor = agentChooseMove(side == White ? ww : bb, side);
        if (gameOutcome(victor)) break;
    }
    return victor;
}

// Score for the learned agent across 2*N games vs a TieredRandom opponent (learned
// plays White for N, Black for N). Loads modelPath into `slot`.
static double quickScoreVsRandom(const string& modelPath, bool policy, const string& boardFile,
                                 int slot, int n) {
    if (!mlLoadSlot(slot, modelPath)) return 0.0;
    AgentSpec learned = policy
        ? agentMakePolicy("learned", chooserIndexByName("LearnedPolicy"), 0, slot)
        : agentMakeSearch("learned", explorerIndexByName("Greedy"), learnedValueIndex(), 1, slot);
    AgentSpec rnd = agentMakePolicy("rnd", chooserIndexByName("TieredRandom"), 0, 0);

    double score = 0.0;
    for (int g = 0; g < n; g++) {                       // learned as White
        int oc = gameOutcome(playGame(learned, rnd, boardFile, 400, nullptr));
        score += (oc == 1) ? 1.0 : (oc == 2) ? 0.0 : 0.5;
    }
    for (int g = 0; g < n; g++) {                        // learned as Black
        int oc = gameOutcome(playGame(rnd, learned, boardFile, 400, nullptr));
        score += (oc == 2) ? 1.0 : (oc == 1) ? 0.0 : 0.5;
    }
    return score / (2.0 * n);
}

// Build a teacher/generator AlphaBeta agent for the given evaluator (by name) and
// depth, seeding its weights from the evaluator's registry defaults and overriding
// with `params` where provided. Used by both learning regimes so the teacher is
// selectable (Classic or Experimental, with any weights), not hard-wired to Classic.
static AgentSpec makeTeacherAgent(const char* name, const string& evalName, int depth,
                                  const std::vector<int>& params) {
    int ev = evaluatorIndexByName(evalName.empty() ? "Classic" : evalName.c_str());
    if (ev < 0) ev = evaluatorIndexByName("Classic");
    AgentSpec a = agentMakeSearch(name, explorerIndexByName("AlphaBeta"), ev, depth, 0);
    for (size_t i = 0; i < params.size() && i < (size_t)MAX_EVAL_PARAMS; i++)
        a.evalParams[i] = params[i];
    return a;
}
// One-line provenance string: the teacher's evaluator, depth, and actual weights.
static string teacherSpec(const AgentSpec& a) {
    int ev = (a.evaluator >= 0 && a.evaluator < g_evalCount) ? a.evaluator : 0;
    int pc = g_evaluators[ev].paramCount;
    string s = "AlphaBeta(" + string(g_evaluators[ev].name) + ", d" + std::to_string(a.depth) + ") params=[";
    for (int i = 0; i < pc; i++) { s += std::to_string(a.evalParams[i]); if (i+1 < pc) s += ","; }
    return s + "]";
}

// ============================================================
// REGIME: SUPERVISED VALUE
// ============================================================
int trainSupervisedValue(const string& outDir, const string& boardFile, int games,
                         int epochs, double lr, int ckptEvery, int genDepth,
                         double genRandom, unsigned seed,
                         const string& genEval, const std::vector<int>& genParams,
                         int featVer, double l2, double genRandomFloor,
                         int genRandomDecayPlies, const string& genModelPath,
                         const string& genModelExplorer, const string& fromDataFile) {
    srand(seed);
    PRNT = 0;
    if (featVer != 1 && featVer != 2) featVer = 1;
    const int featCount = (featVer == 2) ? MLV2_FEATURES : MLV_FEATURES;

    std::vector<std::vector<float> > X;
    std::vector<float> Y;
    std::vector<unsigned long long> H;     // position hash (parallel to X); empty when fromDataFile is used
    std::vector<string> E;                 // position encoding; empty when fromDataFile is used
    std::vector<int> S;                    // side to move; empty when fromDataFile is used
    string teach;

    if (!fromDataFile.empty()) {
        // Skip self-play generation entirely: fit on positions replayed from the
        // existing rank.exe agent pool's real match history instead of a fresh,
        // hand-picked teacher (see rank.exe's "extract" subcommand).
        string err;
        if (!loadReplayDataset(fromDataFile, featVer, featCount, X, Y, err)) {
            cout << "ERROR: " << err << "\n";
            return 1;
        }
        teach = "replay:" + fromDataFile;
        cout << "Data source: " << teach << "  (" << X.size() << " positions, feature v" << featVer << ")\n";
    } else {
        AgentSpec gen;
        if (!genModelPath.empty()) {
            // Self-play bootstrap: the teacher IS a previously-trained value model,
            // wrapped in a search, instead of the fixed Classic/Experimental heuristic.
            int scratchSlot = ML_SLOTS - 2;
            if (!mlLoadSlot(scratchSlot, genModelPath)) {
                cout << "ERROR: cannot load generator model " << genModelPath << "\n";
                return 1;
            }
            int expIdx = explorerIndexByName(genModelExplorer == "greedy" ? "Greedy" : "AlphaBeta");
            gen = agentMakeSearch("gen-bootstrap", expIdx, learnedValueIndex(), genDepth, scratchSlot);
            teach = "self-play-bootstrap(" + genModelExplorer + (genModelExplorer == "greedy" ? "" : ",d" + std::to_string(genDepth))
                  + ",model=" + genModelPath + ")";
        } else {
            gen = makeTeacherAgent("gen", genEval, genDepth, genParams);
            teach = teacherSpec(gen);
        }
        gen.randomMoveProb = genRandom;       // dilution -> position diversity (start value; see decay below)
        // Record the generation dilution in the provenance string, so two models
        // trained with different diversity recipes never carry identical teacher=
        // lines (the decay params were previously omitted).
        if (genRandom > 0.0) {
            std::ostringstream dl;
            dl << " dil(" << genRandom;
            if (genRandomDecayPlies > 0) dl << "->" << genRandomFloor << "/" << genRandomDecayPlies << "p";
            dl << ")";
            teach += dl.str();
        }
        cout << "Teacher/generator: " << teach << "  randomMoveProb=" << genRandom;
        if (genRandomDecayPlies > 0) cout << " decaying to " << genRandomFloor << " over " << genRandomDecayPlies << " plies";
        cout << "\n";

        int wWins = 0, bWins = 0, draws = 0;
        for (int g = 0; g < games; g++) {
            std::vector<PosCap> gf;
            int v = playGame(gen, gen, boardFile, 400, &gf, featVer, genRandomFloor, genRandomDecayPlies);
            int oc = gameOutcome(v);
            if (oc == 1) wWins++; else if (oc == 2) bWins++; else draws++;
            float y = (oc == 1) ? 1.0f : (oc == 2) ? 0.0f : 0.5f;
            for (size_t k = 0; k < gf.size(); k++) {
                X.push_back(gf[k].f); Y.push_back(y);
                H.push_back(gf[k].hash); E.push_back(gf[k].enc); S.push_back(gf[k].side);
            }
        }
        cout << "Generated " << games << " games (" << wWins << " W / " << bWins << " B / "
             << draws << " draw), " << X.size() << " positions.\n";
    }
    if (X.empty()) { cout << "No data.\n"; return 1; }

    LinearModel model(HEAD_VALUE, featVer, featCount, 900.0f);
    model.teacher = teach;                 // provenance, written into the model file
    for (int i = 0; i < model.n; i++) model.w[i] = randWeight();

    std::vector<int> idx(X.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;

    std::vector<ModelRecord> records;
    string runId = "linval_" + std::to_string((long)time(nullptr));

    for (int e = 1; e <= epochs; e++) {
        shuffleVec(idx);
        double sum = 0.0;
        for (size_t i = 0; i < idx.size(); i++)
            sum += model.sgdLogisticStep(X[idx[i]].data(), featCount, Y[idx[i]], lr, (float)l2);
        double avg = sum / X.size();

        bool ckpt = (e % ckptEvery == 0) || (e == epochs);
        cout << "epoch " << e << "/" << epochs << "  loss=" << avg << (ckpt ? "  [ckpt]" : "") << "\n";
        if (!ckpt) continue;

        string path = outDir + "_ckpt" + std::to_string(e) + ".txt";
        model.save(path);
        double wr = quickScoreVsRandom(path, false, boardFile, ML_SLOTS - 1, 8);
        double elo = winrateToElo(wr);

        ModelRecord r;
        r.id = runId + "_e" + std::to_string(e);
        r.type = "linear"; r.head = "value"; r.regime = "selfplay-supervised";
        r.conditions = "genDepth=" + std::to_string(genDepth) + ",genRandom=" + std::to_string(genRandom)
                     + ",featVer=" + std::to_string(featVer) + ",l2=" + std::to_string(l2)
                     + ",teacher=" + teach;
        r.path = path; r.epoch = e; r.games = games; r.loss = avg; r.winrate = wr; r.elo = elo;
        records.push_back(r);

        string ml = "{\"id\":\"" + r.id + "\",\"regime\":\"" + r.regime + "\",\"epoch\":" +
                    std::to_string(e) + ",\"loss\":" + std::to_string(avg) + ",\"winrate_vs_random\":" +
                    std::to_string(wr) + ",\"elo\":" + std::to_string(elo) +
                    ",\"teacher\":\"" + dsJsonEscape(teach) + "\",\"path\":\"" +
                    dsJsonEscape(path) + "\"}";
        dsAppendLine("data/models.jsonl", ml);
        dsAppendLine("data/metrics.jsonl",
                     "{\"run\":\"" + runId + "\",\"epoch\":" + std::to_string(e) +
                     ",\"loss\":" + std::to_string(avg) + ",\"winrate\":" + std::to_string(wr) + "}");
        cout << "          saved " << path << "  winrate_vs_random=" << wr << "  elo~" << (int)elo << "\n";
    }

    model.save(outDir + ".txt");
    cout << "Final model -> " << outDir << ".txt\n";

    // Emit a sample of positions + outcome labels + this model's evaluations to the
    // datastore so the Python/DuckDB analysis layer has joinable data (positions <-
    // labels, evaluations). Capped so a big run does not write a huge file. Skipped
    // for replayed data: H/E/S carry no fresh provenance in that path (the source
    // positions' provenance was already recorded when rank.exe extracted them).
    if (!H.empty()) {
        std::vector<int> sample(X.size());
        for (size_t i = 0; i < sample.size(); i++) sample[i] = (int)i;
        shuffleVec(sample);
        int cap = (int)std::min((size_t)800, sample.size());
        for (int s = 0; s < cap; s++) {
            int i = sample[s];
            int ev = (int)lround(tanh(model.forward(X[i].data(), featCount)) * model.outScale);
            string hh = std::to_string(H[i]);
            dsAppendLine("data/positions.jsonl",
                         "{\"hash\":" + hh + ",\"enc\":\"" + E[i] + "\",\"side\":\"" +
                         (S[i] == White ? string("W") : string("B")) + "\"}");
            dsAppendLine("data/labels.jsonl",
                         "{\"hash\":" + hh + ",\"kind\":\"outcome\",\"value\":" + std::to_string(Y[i]) + "}");
            dsAppendLine("data/evaluations.jsonl",
                         "{\"hash\":" + hh + ",\"model\":\"" + runId + "\",\"eval\":" + std::to_string(ev) + "}");
        }
        cout << "Emitted " << cap << " positions/labels/evaluations to data/*.jsonl\n";
    }

    writeManifest(records);
    return 0;
}

// ============================================================
// REGIME: IMITATION POLICY (behavioral cloning)
// ============================================================
static int identifyMove(const char snap[SIZE][SIZE], int side, const Move* mv, int n) {
    char mine = (side == White) ? WHITE : BLACK;
    int sx = -1, sy = -1, dx = -1, dy = -1;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (snap[x][y] == mine && board[x][y] == EMPTY) { sx = x; sy = y; }
            if (board[x][y] == mine && snap[x][y] != mine)  { dx = x; dy = y; }
        }
    for (int i = 0; i < n; i++)
        if (mv[i].sx == sx && mv[i].sy == sy && mv[i].dx == dx && mv[i].dy == dy) return i;
    return -1;
}

int trainImitationPolicy(const string& outFile, const string& boardFile, int games,
                         int epochs, double lr, int teacherDepth, unsigned seed,
                         const string& teacherEval, const std::vector<int>& teacherParams) {
    srand(seed);
    PRNT = 0;

    AgentSpec teacher = makeTeacherAgent("teacher", teacherEval, teacherDepth, teacherParams);
    teacher.randomMoveProb = 0.10;       // mild noise for varied positions
    string teach = teacherSpec(teacher);
    cout << "Teacher: " << teach << "\n";

    std::vector<std::vector<float> > X;
    std::vector<float> Y;
    for (int g = 0; g < games; g++) {
        reloadBoard(boardFile);
        for (int h = 0; h < 400; h++) {
            int side = (h % 2 == 0) ? White : Black;
            Move mv[ML_MAX_MOVES];
            int n = generateMoves(side, mv);
            if (n == 0) break;

            char snap[SIZE][SIZE];
            for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];

            int victor = agentChooseMove(teacher, side);
            int chosen = identifyMove(snap, side, mv, n);
            if (chosen >= 0) {
                for (int i = 0; i < n; i++) {
                    std::vector<float> f(MLM_FEATURES);
                    // features are read from the live board, so rebuild it for each
                    // candidate is unnecessary: move features only read squares around
                    // the move on the *pre-move* board, which we overwrote. Recompute
                    // from the snapshot instead.
                    char cur[SIZE][SIZE];
                    for (int y = 0; y < SIZE; y++) for (int xx = 0; xx < SIZE; xx++) cur[xx][y] = board[xx][y];
                    for (int y = 0; y < SIZE; y++) for (int xx = 0; xx < SIZE; xx++) board[xx][y] = snap[xx][y];
                    mlExtractMoveFeatures(mv[i], side, f.data());
                    for (int y = 0; y < SIZE; y++) for (int xx = 0; xx < SIZE; xx++) board[xx][y] = cur[xx][y];
                    X.push_back(f);
                    Y.push_back(i == chosen ? 1.0f : 0.0f);
                }
            }
            if (gameOutcome(victor)) break;
        }
    }
    cout << "Collected " << X.size() << " (move,label) pairs from " << games << " teacher games.\n";
    if (X.empty()) { cout << "No data.\n"; return 1; }

    LinearModel model(HEAD_POLICY, mlMoveFeatureVersion(), MLM_FEATURES, 1.0f);
    model.teacher = teach;                 // provenance, written into the model file
    for (int i = 0; i < model.n; i++) model.w[i] = randWeight();

    std::vector<int> idx(X.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;
    for (int e = 1; e <= epochs; e++) {
        shuffleVec(idx);
        double sum = 0.0;
        for (size_t i = 0; i < idx.size(); i++)
            sum += model.sgdLogisticStep(X[idx[i]].data(), MLM_FEATURES, Y[idx[i]], lr);
        cout << "epoch " << e << "/" << epochs << "  loss=" << (sum / X.size()) << "\n";
    }
    model.save(outFile);

    double wr = quickScoreVsRandom(outFile, true, boardFile, ML_SLOTS - 1, 10);
    cout << "Policy model -> " << outFile << "  winrate_vs_random=" << wr << "\n";
    dsAppendLine("data/models.jsonl",
                 "{\"id\":\"linpol_" + std::to_string((long)time(nullptr)) +
                 "\",\"regime\":\"imitate\",\"head\":\"policy\",\"winrate_vs_random\":" +
                 std::to_string(wr) + ",\"teacher\":\"" + dsJsonEscape(teach) +
                 "\",\"path\":\"" + dsJsonEscape(outFile) + "\"}");
    return 0;
}

// ============================================================
// TOURNAMENT: roster + sharded play + Elo fit
// ============================================================
// Reasonable "preset" weightings for the simple evaluators, so the roster has many
// distinct bots from a handful of presets x depths. Params are the standard layout
// (turn, chip, wall, column, [forward]); a Learned preset carries the model slot.
struct EvalPreset {
    const char* tag;
    int evaluator;
    int params[MAX_EVAL_PARAMS];
};

static AgentSpec makePresetAgent(const string& name, int explorer, const EvalPreset& p, int depth) {
    AgentSpec a = agentMakeSearch(name.c_str(), explorer, p.evaluator, depth, 0);
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) a.evalParams[i] = p.params[i];
    return a;
}

// Compact human label for a node budget (1000 -> "1k", 1500000 -> "1M5", etc.).
static string budgetLabel(unsigned long long b) {
    if (b >= 1000000ULL && b % 1000000ULL == 0) return std::to_string(b / 1000000ULL) + "M";
    if (b >= 1000ULL    && b % 1000ULL == 0)    return std::to_string(b / 1000ULL) + "k";
    return std::to_string(b);
}

std::vector<AgentSpec> buildTournamentRoster(const std::vector<int>& depths,
                                             bool hasValue, bool hasPolicy,
                                             const std::vector<unsigned long long>& budgets,
                                             bool ablate, bool forwardStudy) {
    int abIdx = explorerIndexByName("AlphaBeta");
    int grIdx = explorerIndexByName("Greedy");
    int classic = evaluatorIndexByName("Classic");
    int exper   = evaluatorIndexByName("Experimental");
    int learned = learnedValueIndex();

    std::vector<EvalPreset> presets;
    presets.push_back({ "Classic-chip", classic, { 1, 4, 0, 0 } });
    presets.push_back({ "Classic-wall", classic, { 1, 4, 2, 0 } });
    presets.push_back({ "Classic-col",  classic, { 1, 4, 0, 2 } });
    presets.push_back({ "Classic-bal",  classic, { 1, 4, 2, 2 } });
    // Wider chip/structure samples (chip kept well above structure, see plan Part 1d).
    presets.push_back({ "Classic-chip2", classic, { 2, 3, 1, 1 } });
    presets.push_back({ "Classic-chip5", classic, { 4, 5, 2, 2 } });
    // Experimental family (Classic + forward), for evaluator parity in the ladder.
    presets.push_back({ "Exp-fwd",  exper, { 1, 4, 1, 1, 2 } });   // structure + forward
    presets.push_back({ "Exp-bal",  exper, { 1, 4, 2, 2, 2 } });   // balanced + forward
    presets.push_back({ "Exp-push", exper, { 1, 4, 0, 0, 2 } });   // material + forward only
    if (hasValue && learned >= 0)
        presets.push_back({ "Learned", learned, { 0 } });   // p[0] = model slot 0

    std::vector<AgentSpec> r;
    // No-lookahead rung.
    r.push_back(agentMakePolicy("UniformRandom", chooserIndexByName("UniformRandom"), 0, 0));
    r.push_back(agentMakePolicy("TieredRandom",  chooserIndexByName("TieredRandom"), 0, 0));
    int smartIdx = chooserIndexByName("SmartRandom");
    for (int n : { 2, 4, 6 })
        r.push_back(agentMakePolicy(("SmartRandom-N" + std::to_string(n)).c_str(), smartIdx, n, 0));
    if (hasPolicy)
        r.push_back(agentMakePolicy("LearnedPolicy", chooserIndexByName("LearnedPolicy"), 0, 1));

    // Greedy (1-ply) rung over each preset.
    for (const EvalPreset& p : presets)
        r.push_back(makePresetAgent(string("Greedy-") + p.tag, grIdx, p, 1));

    // AlphaBeta ladder over each preset x depth.
    for (const EvalPreset& p : presets)
        for (int d : depths)
            r.push_back(makePresetAgent("AB" + std::to_string(d) + "-" + p.tag, abIdx, p, d));

    // Opt-in BUDGET LADDER: a representative preset at effectively unbounded depth,
    // one agent per node budget, so a budget sweep varies strength directly (the
    // decoupled "budget is the knob" experiment). Built for BOTH evaluators (Classic
    // 'bal' and Experimental 'ebal' with forward) for evaluator parity.
    EvalPreset bal  = { "bal",  classic, { 1, 4, 2, 2 } };
    EvalPreset ebal = { "ebal", exper,   { 1, 4, 2, 2, 2 } };
    for (unsigned long long b : budgets) {
        AgentSpec a = makePresetAgent("Bud" + budgetLabel(b) + "-bal", abIdx, bal, 64);
        a.nodeBudget = b; r.push_back(a);
        AgentSpec e = makePresetAgent("Bud" + budgetLabel(b) + "-ebal", abIdx, ebal, 64);
        e.nodeBudget = b; r.push_back(e);
    }

    // Opt-in FEATURE ABLATION: the same bounded search with each optimization toggled,
    // so the Elo/telemetry tables show each feature's gain in isolation. Fixed node
    // budget + unbounded depth so the more efficient variant reaches a higher eff-depth.
    // Built for BOTH evaluators (C = Classic, E = Experimental) for parity.
    if (ablate) {
        const unsigned long long ab = 100000ULL;
        struct Flag { const char* tag; bool useAB, tt, ord; int asp; };
        Flag flags[] = {
            { "base",  true,  false, false, 0  },
            { "noAB",  false, false, false, 0  },
            { "ord",   true,  false, true,  0  },
            { "TT",    true,  true,  false, 0  },
            { "TTord", true,  true,  true,  0  },
            { "asp50", true,  false, true,  50 },
        };
        struct EvCase { const char* pfx; const EvalPreset* p; } evs[] = { { "AblC-", &bal }, { "AblE-", &ebal } };
        for (const EvCase& ev : evs)
            for (const Flag& f : flags) {
                AgentSpec a = makePresetAgent(string(ev.pfx) + f.tag, abIdx, *ev.p, 64);
                a.nodeBudget = ab;
                a.useAlphaBeta = f.useAB;
                a.useTT = f.tt;
                a.useMoveOrder = f.ord;
                a.aspirationWindow = f.asp;
                r.push_back(a);
            }
    }

    // Opt-in FORWARD-WEIGHT STUDY: Experimental agents identical except for the forward
    // weight (0 = a Classic-equivalent control), fixed budget + unbounded depth, so a
    // tournament ranks advance weights by playing strength (the analog of turn-swing,
    // but measured as Elo rather than eval units).
    if (forwardStudy) {
        const unsigned long long ab = 100000ULL;
        for (int fwd : { 0, 1, 2, 4, 8 }) {
            EvalPreset p = { "fwd", exper, { 1, 4, 1, 1, fwd } };
            AgentSpec a = makePresetAgent("Fwd" + std::to_string(fwd), abIdx, p, 64);
            a.nodeBudget = ab;
            r.push_back(a);
        }
    }

    return r;
}

// ---- Elo fit from recorded results (indices into the roster) ----
struct GameRec { int a, b; double sa; };
static void fitElo(const std::vector<GameRec>& results, int n, std::vector<double>& ratingsOut) {
    ratingsOut.assign(n, 1500.0);
    double K = 12.0;
    for (int pass = 0; pass < 2000; pass++) {
        for (size_t r = 0; r < results.size(); r++)
            eloUpdate(ratingsOut[results[r].a], ratingsOut[results[r].b], results[r].sa, K);
        if (pass % 200 == 199) K *= 0.7;
    }
    if (n > 0) {
        double mean = 0.0; for (int i = 0; i < n; i++) mean += ratingsOut[i]; mean /= n;
        for (int i = 0; i < n; i++) ratingsOut[i] += 1500.0 - mean;
    }
}

// ---- Minimal JSONL field extractors (our writer emits flat, simple objects) ----
static bool jsonStr(const string& line, const string& key, string& out) {
    auto k = line.find("\"" + key + "\":\"");
    if (k == string::npos) return false;
    auto s = k + key.size() + 4;
    auto e = line.find('"', s);
    if (e == string::npos) return false;
    out = line.substr(s, e - s);
    return true;
}
static bool jsonNum(const string& line, const string& key, double& out) {
    auto k = line.find("\"" + key + "\":");
    if (k == string::npos) return false;
    auto s = k + key.size() + 3;
    if (s < line.size() && line[s] == '"') return false;   // it's a string field
    try { out = std::stod(line.substr(s)); } catch (...) { return false; }
    return true;
}
// Parse a JSON array of numbers, "key":[1,2,3], appending the values to out.
static bool jsonNumArr(const string& line, const string& key, std::vector<double>& out) {
    auto k = line.find("\"" + key + "\":[");
    if (k == string::npos) return false;
    auto s = k + key.size() + 4;
    auto e = line.find(']', s);
    if (e == string::npos) return false;
    out.clear();
    std::stringstream ss(line.substr(s, e - s));
    string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) continue;
        try { out.push_back(std::stod(tok)); } catch (...) {}
    }
    return true;
}

// Load a dataset extracted by rank.exe's "extract" subcommand: one JSON object
// per line, either {"ver":1,"label":f,"f":[...]} (dense, v1 aggregates) or
// {"ver":2,"stm":+-1,"label":f,"idx":[...]} (sparse piece-square "on" indices,
// v2; stm is applied to MLV2_STM separately since it isn't part of "idx"). Rows
// whose declared ver doesn't match featVer are skipped (a mixed-version replay
// file is not an error, just a partial yield). Lets a model be trained on real
// historical games played by the existing rank.exe agent pool instead of a
// freshly self-played, hand-picked teacher.
static bool loadReplayDataset(const string& path, int featVer, int featCount,
                              std::vector<std::vector<float> >& X, std::vector<float>& Y,
                              string& err) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) { err = "cannot open " + path; return false; }
    string line;
    long long lineNo = 0, skipped = 0;
    while (std::getline(f, line)) {
        lineNo++;
        if (line.empty()) continue;
        double verD = 0.0, labelD = 0.0;
        if (!jsonNum(line, "ver", verD) || !jsonNum(line, "label", labelD)) { skipped++; continue; }
        if ((int)verD != featVer) { skipped++; continue; }
        std::vector<float> feat(featCount, 0.0f);
        if (featVer == 2) {
            double stm = 0.0;
            jsonNum(line, "stm", stm);
            std::vector<double> idxs;
            jsonNumArr(line, "idx", idxs);
            for (double d : idxs) { int i = (int)d; if (i >= 0 && i < featCount) feat[i] = 1.0f; }
            if (MLV2_STM < featCount) feat[MLV2_STM] = (float)stm;
        } else {
            std::vector<double> vals;
            if (!jsonNumArr(line, "f", vals)) { skipped++; continue; }
            for (size_t i = 0; i < vals.size() && (int)i < featCount; i++) feat[i] = (float)vals[i];
        }
        X.push_back(feat);
        Y.push_back((float)labelD);
    }
    if (X.empty()) {
        err = "no usable rows in " + path + " (" + std::to_string(skipped) + " of "
            + std::to_string(lineNo) + " skipped: wrong feature version or malformed)";
        return false;
    }
    if (skipped > 0)
        cout << "  (skipped " << skipped << "/" << lineNo << " rows: wrong feature version or malformed)\n";
    return true;
}

// ============================================================
// TOURNAMENT: agent allowlist (optional roster subset)
// ============================================================
// Split a "a,b,c" list into trimmed, non-empty tokens.
static std::vector<std::string> parseCsvList(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i <= s.size()) {
        size_t c = s.find(',', i);
        size_t end = (c == std::string::npos) ? s.size() : c;
        size_t a = i, b = end;
        while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
        while (b > a && (s[b-1] == ' ' || s[b-1] == '\t')) b--;
        if (b > a) out.push_back(s.substr(a, b - a));
        if (c == std::string::npos) break;
        i = c + 1;
    }
    return out;
}

// Reduce a full roster to just the allowlisted names (preserving the full roster's
// order). Empty `only` returns the full roster unchanged (today's behavior). Any
// requested name not present is reported, so a typo or a missing --depths rung / model
// surfaces loudly instead of silently shrinking the field.
static std::vector<AgentSpec> filterRosterByName(const std::vector<AgentSpec>& full,
                                                 const std::vector<std::string>& only) {
    if (only.empty()) return full;
    std::vector<AgentSpec> out;
    for (size_t i = 0; i < full.size(); i++)
        for (size_t k = 0; k < only.size(); k++)
            if (only[k] == full[i].name) { out.push_back(full[i]); break; }
    for (size_t k = 0; k < only.size(); k++) {
        bool found = false;
        for (size_t i = 0; i < full.size(); i++)
            if (only[k] == full[i].name) { found = true; break; }
        if (!found)
            cout << "WARNING: --only name '" << only[k]
                 << "' not in roster (check --depths / models)\n";
    }
    return out;
}

// ============================================================
// TOURNAMENT: play one shard (with per-move timing + search telemetry)
// ============================================================
// Streaming per-agent search statistics. Raw accumulators (count, sum, sum-of-squares,
// min, max) are emitted so a multi-shard rate step can merge them and recover exact
// mean/stddev/range; budget-kind counts give the node/time/depth breakdown.
struct AgentStats {
    long long n = 0;
    double sumEff = 0, sqEff = 0, minEff = 1e18, maxEff = 0;
    double sumNodes = 0, maxNodes = 0;
    double sumBranch = 0;
    long long kNode = 0, kTime = 0, kDepth = 0;
    void add(double eff, double nodes, double branch, int kind) {
        n++; sumEff += eff; sqEff += eff*eff;
        if (eff < minEff) minEff = eff; if (eff > maxEff) maxEff = eff;
        sumNodes += nodes; if (nodes > maxNodes) maxNodes = nodes;
        sumBranch += branch;
        if (kind == BUDGET_NODE) kNode++; else if (kind == BUDGET_TIME) kTime++; else kDepth++;
    }
};

int tournamentPlay(const string& boardFile, const std::vector<int>& depths,
                   int gamesPerPair, unsigned seed, int shard, int ofK,
                   unsigned long long nodeBudget, const string& outFile,
                   const std::vector<std::string>& only,
                   double timeBudgetMs,
                   const std::vector<unsigned long long>& budgets,
                   bool ablate, bool forwardStudy) {
    PRNT = 0;
    bool hasVal = mlLoadSlot(0, "models/lin_value.txt");
    bool hasPol = mlLoadSlot(1, "models/lin_policy.txt");
    std::vector<AgentSpec> roster = filterRosterByName(
        buildTournamentRoster(depths, hasVal, hasPol, budgets, ablate, forwardStudy), only);
    int n = (int)roster.size();

    g_nodeBudget = nodeBudget;
    g_timeBudgetMs = timeBudgetMs;
    srand(seed * 1000u + (unsigned)shard + 1u);

    std::vector<double> msTotal(n, 0.0), msMax(n, 0.0);
    std::vector<long long> moveCnt(n, 0);
    std::vector<AgentStats> stat(n);

    std::ofstream out(outFile, std::ios::app);
    long long gIndex = 0, played = 0;
    typedef std::chrono::steady_clock clock;

    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            for (int g = 0; g < gamesPerPair; g++) {
                long long my = gIndex++;
                if (my % ofK != shard) continue;
                bool iWhite = (g % 2 == 0);
                int wi = iWhite ? i : j, bi = iWhite ? j : i;

                reloadBoard(boardFile);
                int victor = None;
                for (int h = 0; h < 400; h++) {
                    int side = (h % 2 == 0) ? White : Black;
                    int ai = (side == White) ? wi : bi;
                    g_lastNodes = 0;   // reset so non-minimax agents (Greedy/Policy) are skipped below
                    auto t0 = clock::now();
                    victor = agentChooseMove(roster[ai], side);
                    double dt = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
                    msTotal[ai] += dt; moveCnt[ai]++; if (dt > msMax[ai]) msMax[ai] = dt;
                    if (roster[ai].brain == BRAIN_SEARCH && g_lastNodes > 1) {
                        double branch = (g_lastNodes > g_lastLeafs)
                            ? (double)(g_lastNodes - 1) / (double)(g_lastNodes - g_lastLeafs) : 0.0;
                        stat[ai].add(g_lastEffDepth, (double)g_lastNodes, branch, g_lastBudgetKind);
                    }
                    if (gameOutcome(victor)) break;
                }
                int oc = gameOutcome(victor);
                double scoreWhite = (oc == 1) ? 1.0 : (oc == 2) ? 0.0 : 0.5;
                double sa = iWhite ? scoreWhite : (1.0 - scoreWhite);
                out << "{\"a\":\"" << roster[i].name << "\",\"b\":\"" << roster[j].name
                    << "\",\"sa\":" << sa << "}\n";
                played++;
            }

    for (int k = 0; k < n; k++)
        if (moveCnt[k] > 0) {
            out << "{\"timing\":1,\"name\":\"" << roster[k].name << "\",\"ms_total\":"
                << msTotal[k] << ",\"moves\":" << moveCnt[k] << ",\"ms_max\":" << msMax[k];
            const AgentStats& s = stat[k];
            if (s.n > 0)
                out << ",\"sn\":" << s.n << ",\"se\":" << s.sumEff << ",\"qe\":" << s.sqEff
                    << ",\"mine\":" << s.minEff << ",\"maxe\":" << s.maxEff
                    << ",\"snod\":" << s.sumNodes << ",\"mnod\":" << s.maxNodes
                    << ",\"sbr\":" << s.sumBranch
                    << ",\"kn\":" << s.kNode << ",\"kt\":" << s.kTime << ",\"kd\":" << s.kDepth;
            out << "}\n";
        }

    emitResourceLine(out);

    g_nodeBudget = 0;
    g_timeBudgetMs = 0.0;
    cout << "shard " << shard << "/" << ofK << ": played " << played << " games -> " << outFile << "\n";
    return 0;
}

// ============================================================
// TOURNAMENT: rate from recorded results + write champion
// ============================================================
static void writeChampion(const AgentSpec& champ, int elo, double msMove) {
    std::ofstream c("agents/champion.txt");
    if (c.is_open())
        c << "Champion: " << agentDescribe(champ) << "\nElo: " << elo
          << "\nms/move: " << msMove << "\n";

    std::ofstream p("agents/champion_params.txt");
    if (!p.is_open()) return;
    p << "# Champion bot, drop into minimax_params.txt to play it (pick MiniMax, use file).\n";
    if (champ.brain != BRAIN_SEARCH || champ.explorer != explorerIndexByName("AlphaBeta")) {
        p << "# NOTE: champion is " << agentDescribe(champ)
          << " (not an AlphaBeta agent); not expressible as minimax_params.\n";
        return;
    }
    int ev = champ.evaluator;
    const char* sides[2] = { "white", "black" };
    for (int s = 0; s < 2; s++) {
        p << sides[s] << "_eval=" << ev << "\n";
        p << sides[s] << "_depth=" << champ.depth << "\n";
        for (int i = 0; i < g_evaluators[ev].paramCount; i++)
            p << sides[s] << "_" << g_evaluators[ev].params[i].key << "=" << champ.evalParams[i] << "\n";
        p << sides[s] << "_opener=0\n";
    }
}

// ============================================================
// RUN ARCHIVE + AGENT REGISTRY
// ============================================================
// A "run" is one tournament invocation with a fixed config, archived immutably under
// runs/<id>/. The agent registry (agents/registry.{jsonl,md}) is the append-only union
// of every agent ever rated, so a subset run never erases knowledge of the others.

// Current UTC time formatted with `fmt` (strftime spec).
static string utcTime(const char* fmt) {
    time_t t = time(nullptr);
    struct tm g;
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), fmt, &g);
    return string(buf);
}
string makeRunId()            { return utcTime("%Y%m%dT%H%M%SZ"); }
static string nowUtcString()  { return utcTime("%Y-%m-%dT%H:%M:%SZ"); }

// 64-bit FNV-1a over a byte buffer (continuable via the `h` seed).
static unsigned long long fnv1a64(const char* p, size_t n, unsigned long long h) {
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)p[i]; h *= 1099511628211ULL; }
    return h;
}

// The model file a learned agent depends on (so a retrain changes its spec hash), or
// "" for a non-learned agent. Slot 0 = value model, slot 1 = policy model (the
// tournament's fixed convention; see tournamentPlay/Rate).
static string slotFile(int slot) {
    if (slot == 0) return "models/lin_value.txt";
    if (slot == 1) return "models/lin_policy.txt";
    return "";
}
static string agentModelFile(const AgentSpec& a) {
    if (a.brain == BRAIN_POLICY && a.chooser == chooserIndexByName("LearnedPolicy"))
        return slotFile(a.modelSlot);
    if (a.brain == BRAIN_SEARCH && a.evaluator == learnedValueIndex())
        return slotFile(a.evalParams[0]);   // learned value preset carries the slot in p[0]
    return "";
}

// Stable identity for "this agent's behavior": its structural AgentSpec fields plus,
// for a learned agent, the content of its model file. A retrain, a param change, or a
// bugfix that alters a structural field all produce a new hash, which registry.md then
// flags as "changed".
static string agentSpecHash(const AgentSpec& a) {
    std::ostringstream s;
    s << "b" << a.brain << "x" << a.explorer << "v" << a.evaluator
      << "d" << a.depth << "c" << a.chooser << "p" << a.chooserParam << "m" << a.modelSlot << "P";
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) s << a.evalParams[i] << ",";
    string base = s.str();
    unsigned long long h = fnv1a64(base.data(), base.size(), 1469598103934665603ULL);
    string mf = agentModelFile(a);
    if (!mf.empty()) {
        std::ifstream f(mf, std::ios::binary);
        if (f.is_open()) {
            std::ostringstream ss; ss << f.rdbuf();
            string content = ss.str();
            h = fnv1a64(content.data(), content.size(), h);
        }
    }
    char buf[20];
    snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
    return string(buf);
}

// runs/<id>/config.json + the notes.md header (pre-run note). Called once per run.
void writeRunConfig(const string& runId, const std::vector<int>& depths,
                    unsigned long long nodeBudget, int gamesPerPair, unsigned seed,
                    int workers, const string& board,
                    const std::vector<std::string>& only, const string& note) {
    ensureDir("runs");
    ensureDir("runs/" + runId);
    string created = nowUtcString();

    std::ofstream c("runs/" + runId + "/config.json");
    if (c.is_open()) {
        c << "{\n  \"run_id\": \"" << runId << "\",\n  \"created_utc\": \"" << created << "\",\n";
        c << "  \"depths\": [";
        for (size_t i = 0; i < depths.size(); i++) c << (i ? ", " : "") << depths[i];
        c << "],\n  \"node_budget\": " << nodeBudget << ",\n  \"games_per_pair\": " << gamesPerPair
          << ",\n  \"seed\": " << seed << ",\n  \"workers\": " << workers
          << ",\n  \"board\": \"" << dsJsonEscape(board) << "\",\n  \"only\": [";
        for (size_t i = 0; i < only.size(); i++) c << (i ? ", " : "") << "\"" << dsJsonEscape(only[i]) << "\"";
        c << "],\n  \"note\": \"" << dsJsonEscape(note) << "\"\n}\n";
    }

    std::ofstream m("runs/" + runId + "/notes.md");
    if (m.is_open()) {
        m << "# Run " << runId << "\n\n";
        m << "- Created (UTC): " << created << "\n";
        m << "- Config: see config.json\n\n";
        m << "## Pre-run notes (" << created << ")\n\n";
        m << (note.empty() ? "(none)" : note) << "\n";
    }
}

int runNote(const string& runId, const string& note) {
    string dir = "runs/" + runId;
    std::ifstream probe(dir + "/notes.md");
    if (!probe.is_open()) { cout << "No such run: " << dir << "/notes.md not found\n"; return 1; }
    probe.close();
    std::ofstream m(dir + "/notes.md", std::ios::app);
    if (!m.is_open()) { cout << "Cannot append to " << dir << "/notes.md\n"; return 1; }
    string stamp = nowUtcString();
    m << "\n## Note (" << stamp << ")\n\n" << note << "\n";
    cout << "Appended note to " << dir << "/notes.md\n";
    return 0;
}

// Copy the merged results file into the run's immutable archive.
static void archiveRunResults(const string& runId, const string& inFile) {
    std::ifstream in(inFile, std::ios::binary);
    if (!in.is_open()) return;
    std::ofstream out("runs/" + runId + "/results.jsonl", std::ios::binary);
    if (!out.is_open()) return;
    out << in.rdbuf();
}

// This run's Elo table, in ranked order, as a TSV.
static void writeRunElo(const string& runId, const std::vector<AgentSpec>& roster,
                        const std::vector<int>& order, const std::vector<double>& ratings,
                        const std::vector<double>& msMove, const std::vector<double>& msMax,
                        const std::vector<long long>& games) {
    std::ofstream e("runs/" + runId + "/elo.tsv");
    if (!e.is_open()) return;
    e << "elo\tms_per_move\tms_max\tgames\tname\tdesc\n";
    for (size_t k = 0; k < order.size(); k++) {
        int i = order[k];
        e << (int)ratings[i] << "\t" << msMove[i] << "\t" << msMax[i] << "\t" << games[i]
          << "\t" << roster[i].name << "\t" << agentDescribe(roster[i]) << "\n";
    }
}

// Append one observation per agent to agents/registry.jsonl (the append-only union).
static void appendRegistry(const string& runId, const std::vector<AgentSpec>& roster,
                           const std::vector<int>& order, const std::vector<double>& ratings,
                           const std::vector<double>& msMove, const std::vector<long long>& games,
                           const string& created) {
    for (size_t k = 0; k < order.size(); k++) {
        int i = order[k];
        std::ostringstream js;
        js << "{\"run_id\":\"" << runId << "\",\"name\":\"" << roster[i].name
           << "\",\"spec_hash\":\"" << agentSpecHash(roster[i]) << "\",\"elo\":" << (int)ratings[i]
           << ",\"ms_per_move\":" << msMove[i] << ",\"games\":" << games[i]
           << ",\"desc\":\"" << dsJsonEscape(agentDescribe(roster[i])) << "\",\"created_utc\":\"" << created << "\"}";
        dsAppendLine("agents/registry.jsonl", js.str());
    }
}

// Regenerate agents/registry.md: fold registry.jsonl by agent name (last observation
// wins), ranked by last-known Elo, flagging agents whose spec_hash changed vs. their
// previous observation. This is the always-complete picture of every agent ever rated.
static void writeRegistryRollup() {
    struct Rollup { string name, hash, prevHash, lastRun, desc; int elo; int runs; };
    std::map<string, Rollup> by;
    std::vector<string> order;   // insertion order of first sighting

    std::ifstream f("agents/registry.jsonl");
    if (!f.is_open()) return;
    string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        string nm, hash, run, desc; double elo = 0;
        if (!jsonStr(line, "name", nm)) continue;
        jsonStr(line, "spec_hash", hash);
        jsonStr(line, "run_id", run);
        jsonStr(line, "desc", desc);
        jsonNum(line, "elo", elo);
        if (!by.count(nm)) { by[nm] = Rollup{ nm, "", "", "", "", 0, 0 }; order.push_back(nm); }
        Rollup& r = by[nm];
        r.prevHash = r.hash;   // previous observation's hash
        r.hash = hash; r.lastRun = run; r.desc = desc; r.elo = (int)elo; r.runs++;
    }

    std::sort(order.begin(), order.end(),
              [&](const string& a, const string& b){ return by[a].elo > by[b].elo; });

    std::ofstream m("agents/registry.md");
    if (!m.is_open()) return;
    m << "# Agent registry\n\n";
    m << "Append-only union of every agent ever rated (regenerated each tournament rate).\n";
    m << "A subset run only ADDS observations, so this stays complete. `changed` flags an\n";
    m << "agent whose spec_hash differs from its previous observation (a retrain, a param\n";
    m << "change, or a structural bugfix).\n\n";
    m << "| agent | last elo | runs | last run | spec_hash | changed | desc |\n";
    m << "|-------|---------:|-----:|----------|-----------|---------|------|\n";
    for (size_t i = 0; i < order.size(); i++) {
        const Rollup& r = by[order[i]];
        bool changed = (r.runs > 1 && !r.prevHash.empty() && r.prevHash != r.hash);
        m << "| " << r.name << " | " << r.elo << " | " << r.runs << " | " << r.lastRun
          << " | " << r.hash << " | " << (changed ? "yes" : "no") << " | " << r.desc << " |\n";
    }
}

// One self-describing summary line per run in runs/index.jsonl (the master log).
static void appendRunIndex(const string& runId, const string& created, int nAgents,
                           long long nGames, const string& champion, int champElo) {
    std::ostringstream js;
    js << "{\"run_id\":\"" << runId << "\",\"created_utc\":\"" << created
       << "\",\"agents\":" << nAgents << ",\"games\":" << nGames
       << ",\"champion\":\"" << dsJsonEscape(champion) << "\",\"champ_elo\":" << champElo << "}";
    dsAppendLine("runs/index.jsonl", js.str());
}

int tournamentRate(const std::vector<int>& depths, const string& inFile,
                   const std::vector<std::string>& only,
                   const string& runId, const string& note,
                   const std::vector<unsigned long long>& budgets,
                   bool ablate, bool forwardStudy) {
    (void)note;
    PRNT = 0;
    bool hasVal = mlLoadSlot(0, "models/lin_value.txt");
    bool hasPol = mlLoadSlot(1, "models/lin_policy.txt");
    std::vector<AgentSpec> roster = filterRosterByName(
        buildTournamentRoster(depths, hasVal, hasPol, budgets, ablate, forwardStudy), only);
    int n = (int)roster.size();

    std::map<string, int> idx;
    for (int i = 0; i < n; i++) idx[roster[i].name] = i;

    std::vector<GameRec> results;
    std::vector<double> msTotal(n, 0.0), msMax(n, 0.0);
    std::vector<long long> moveCnt(n, 0), gamesCnt(n, 0);
    std::vector<AgentStats> stat(n);   // merged search telemetry per agent

    double resPeakMB = 0.0, resCpuS = 0.0;   // process resource summary (peak / total)
    std::ifstream f(inFile);
    if (!f.is_open()) { cout << "Cannot open " << inFile << "\n"; return 1; }
    string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line.find("\"resource\"") != string::npos) {
            double pm = 0, cs = 0;
            jsonNum(line, "peak_ws_mb", pm); jsonNum(line, "cpu_s", cs);
            if (pm > resPeakMB) resPeakMB = pm;   // peak across shards
            resCpuS += cs;                        // total CPU across shards
        } else if (line.find("\"timing\"") != string::npos) {
            string nm; double mt = 0, mv = 0, mx = 0;
            if (jsonStr(line, "name", nm) && idx.count(nm)) {
                int k = idx[nm];
                jsonNum(line, "ms_total", mt); jsonNum(line, "moves", mv); jsonNum(line, "ms_max", mx);
                msTotal[k] += mt; moveCnt[k] += (long long)mv; if (mx > msMax[k]) msMax[k] = mx;
                double sn = 0;
                if (jsonNum(line, "sn", sn) && sn > 0) {
                    double se=0,qe=0,mine=0,maxe=0,snod=0,mnod=0,sbr=0,kn=0,kt=0,kd=0;
                    jsonNum(line,"se",se); jsonNum(line,"qe",qe); jsonNum(line,"mine",mine);
                    jsonNum(line,"maxe",maxe); jsonNum(line,"snod",snod); jsonNum(line,"mnod",mnod);
                    jsonNum(line,"sbr",sbr); jsonNum(line,"kn",kn); jsonNum(line,"kt",kt); jsonNum(line,"kd",kd);
                    AgentStats& s = stat[k];
                    s.n += (long long)sn; s.sumEff += se; s.sqEff += qe;
                    if (mine < s.minEff) s.minEff = mine; if (maxe > s.maxEff) s.maxEff = maxe;
                    s.sumNodes += snod; if (mnod > s.maxNodes) s.maxNodes = mnod;
                    s.sumBranch += sbr;
                    s.kNode += (long long)kn; s.kTime += (long long)kt; s.kDepth += (long long)kd;
                }
            }
        } else {
            string an, bn; double sa = 0;
            if (jsonStr(line, "a", an) && jsonStr(line, "b", bn) &&
                idx.count(an) && idx.count(bn) && jsonNum(line, "sa", sa)) {
                GameRec r; r.a = idx[an]; r.b = idx[bn]; r.sa = sa; results.push_back(r);
                gamesCnt[r.a]++; gamesCnt[r.b]++;
            }
        }
    }
    cout << "Loaded " << results.size() << " games over " << n << " agents from " << inFile << "\n";

    std::vector<double> ratings;
    fitElo(results, n, ratings);

    std::vector<int> order(n);
    for (int i = 0; i < n; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b){ return ratings[a] > ratings[b]; });

    // A subset run (only non-empty) must NOT clobber the full-roster snapshot in
    // agents/library.txt + champion*.txt. Those are refreshed only on a full run; the
    // subset's table is preserved instead under runs/<id>/elo.tsv + the registry.
    bool fullRun = only.empty();

    std::vector<double> msMove(n, 0.0);
    cout << "\n   Elo  ms/move   max ms  games  agent\n";
    cout << "  ----  -------  -------  -----  -----------------------------------\n";
    std::ofstream lib;
    if (fullRun) lib.open("agents/library.txt");
    char buf[256];
    for (int k = 0; k < n; k++) {
        int i = order[k];
        double mm = moveCnt[i] ? msTotal[i] / moveCnt[i] : 0.0;
        msMove[i] = mm;
        snprintf(buf, sizeof(buf), "  %4d  %7.2f  %7.1f  %5lld  %s",
                 (int)ratings[i], mm, msMax[i], gamesCnt[i], agentDescribe(roster[i]).c_str());
        cout << buf << "\n";
        if (lib.is_open()) lib << (int)ratings[i] << "\t" << mm << "\t" << agentDescribe(roster[i]) << "\n";
        std::ostringstream js;
        js << "{\"name\":\"" << roster[i].name << "\",\"elo\":" << (int)ratings[i]
           << ",\"ms_per_move\":" << mm << ",\"ms_max\":" << msMax[i]
           << ",\"games\":" << gamesCnt[i] << ",\"desc\":\"" << dsJsonEscape(agentDescribe(roster[i])) << "\"}";
        dsAppendLine("data/agents.jsonl", js.str());
    }

    // Search telemetry: effective depth (mean +/- std [min..max]), nodes/move, branching
    // factor, and the node/time/depth budget-kind breakdown. The decisive readout for
    // "is depth 6 really searching as deep as depth 8/10, and does more budget help?".
    bool anyStats = false;
    for (int k = 0; k < n; k++) if (stat[k].n > 0) { anyStats = true; break; }
    if (anyStats) {
        cout << "\n  eff-depth (mean+-std [min..max])   nodes/mv   branch   budget(N/T/D%)  agent\n";
        cout << "  --------------------------------  ---------  -------  --------------  ---------------\n";
        for (int kk = 0; kk < n; kk++) {
            int i = order[kk];
            const AgentStats& s = stat[i];
            if (s.n == 0) continue;
            double mean = s.sumEff / s.n;
            double var  = s.sqEff / s.n - mean*mean; if (var < 0) var = 0;
            double sd   = std::sqrt(var);
            double avgNodes = s.sumNodes / s.n;
            double avgBranch = s.sumBranch / s.n;
            double tot = (double)(s.kNode + s.kTime + s.kDepth); if (tot < 1) tot = 1;
            char b2[320];
            snprintf(b2, sizeof(b2),
                "  %5.2f +- %4.2f [%4.1f..%4.1f]        %9.0f  %7.2f  %3.0f/%3.0f/%3.0f     %s",
                mean, sd, s.minEff, s.maxEff, avgNodes, avgBranch,
                100.0*s.kNode/tot, 100.0*s.kTime/tot, 100.0*s.kDepth/tot,
                agentDescribe(roster[i]).c_str());
            cout << b2 << "\n";
        }
    }

    if (resPeakMB > 0.0 || resCpuS > 0.0)
        cout << "\nResources: peak working set " << resPeakMB << " MB, total CPU "
             << resCpuS << " s (process-level; one process serves all agents)\n";

    string champName; int champElo = 0;
    if (n > 0) {
        int champ = order[0];
        double mm = moveCnt[champ] ? msTotal[champ] / moveCnt[champ] : 0.0;
        champName = agentDescribe(roster[champ]);
        champElo = (int)ratings[champ];
        if (fullRun) writeChampion(roster[champ], champElo, mm);
        cout << "\nChampion: " << champName << "  (Elo " << champElo << ", " << mm << " ms/move)\n";
    }

    // Archive this run (timestamped, immutable) and update the agent registry union.
    if (!runId.empty()) {
        string created = nowUtcString();
        archiveRunResults(runId, inFile);
        writeRunElo(runId, roster, order, ratings, msMove, msMax, gamesCnt);
        appendRegistry(runId, roster, order, ratings, msMove, gamesCnt, created);
        writeRegistryRollup();
        appendRunIndex(runId, created, n, (long long)results.size(), champName, champElo);
    }

    if (fullRun) cout << "Wrote agents/library.txt, agents/champion*.txt, data/agents.jsonl\n";
    else         cout << "Subset run: left agents/library.txt + champion*.txt untouched; results in runs/" << runId << "/\n";
    if (!runId.empty()) cout << "Archived run under runs/" << runId << "/ and updated agents/registry.{jsonl,md}\n";
    cout << "NOTE: absolute ms/move is inflated by parallel shard contention; relative order is informative.\n";
    return 0;
}

// Best 1-ply white-centric eval for `side` to move, with the given (turn-zeroed) params.
static double tempoBest1ply(int side, int evaluator, const int* params) {
    Move mv[ML_MAX_MOVES];
    int nm = generateMoves(side, mv);
    if (nm == 0) return (side == White) ? -1e9 : 1e9;
    if (side == White) {
        double best = -1e18;
        for (int i = 0; i < nm; i++) {
            bool cap = simulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx);
            double s = evaluateBoard(Black, evaluator, params);
            unsimulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx, cap);
            if (s > best) best = s;
        }
        return best;
    }
    double best = 1e18;
    for (int i = 0; i < nm; i++) {
        bool cap = simulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx);
        double s = evaluateBoard(White, evaluator, params);
        unsimulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx, cap);
        if (s < best) best = s;
    }
    return best;
}

int turnSwing(const string& boardFile, int games, int depth, unsigned seed,
              int chipW, int wallW, int colW, int fwdW) {
    (void)depth;                          // 1-ply tempo measure (cheap, no full search)
    PRNT = 0;
    srand(seed);
    // Use Experimental (== Classic when forward == 0) so the forward term can be varied.
    int evalr = evaluatorIndexByName("Experimental");
    if (chipW <= 0) chipW = 4;            // reference: chip weight per piece
    int params0[MAX_EVAL_PARAMS] = { 0, chipW, wallW, colW, fwdW }; // turn term zeroed for the measure
    cout << "Measuring with weights chip=" << chipW << " wall=" << wallW
         << " column=" << colW << " forward=" << fwdW << "\n";

    AgentSpec gen = agentMakeSearch("gen", explorerIndexByName("AlphaBeta"), evalr, 2, 0);
    gen.randomMoveProb = 0.25;            // noise -> position diversity

    std::vector<double> swings;
    for (int g = 0; g < games; g++) {
        reloadBoard(boardFile);
        int victor = None;
        for (int h = 0; h < 200 && !gameOutcome(victor); h++) {
            int side = (h % 2 == 0) ? White : Black;
            if (h >= 2 && !canWinWhite() && !canWinBlack()) {
                double sw = tempoBest1ply(White, evalr, params0);
                double sb = tempoBest1ply(Black, evalr, params0);
                // Skip near-win sentinels (evaluateBoard flags positions one step from
                // the goal row): keep only ordinary positional/material swings.
                if (std::fabs(sw) < 100000.0 && std::fabs(sb) < 100000.0)
                    swings.push_back(sw - sb);
            }
            victor = agentChooseMove(gen, side);
        }
    }

    if (swings.empty()) { cout << "turn-swing: no positions sampled.\n"; return 1; }
    double sum = 0, sq = 0, lo = 1e18, hi = -1e18;
    for (double v : swings) { sum += v; sq += v*v; if (v < lo) lo = v; if (v > hi) hi = v; }
    double mean = sum / swings.size();
    double var = sq / swings.size() - mean*mean; if (var < 0) var = 0;
    double sd = std::sqrt(var);
    cout << "Turn-swing over " << swings.size() << " positions (white-centric eval units):\n";
    cout << "  mean=" << mean << "  std=" << sd << "  [" << lo << ".." << hi << "]\n";
    cout << "  chip weight reference = " << chipW << " per piece (a tempo ~ "
         << (mean / chipW) << " captures)\n";
    cout << "  recommended turn weight ~ " << (int)std::lround(mean / 2.0)
         << " (half the side-to-move swing)\n";
    return 0;
}

// ============================================================
// SPEED BENCHMARK: precise per-move cost of evaluator variants vs the learned model
// ============================================================
// Times thousands of repeated move selections on a fixed set of mid-game positions
// (high-resolution clock), so sub-millisecond costs resolve. Reports us/move and
// nodes/move, and maps the learned 1-ply model's time onto each AlphaBeta variant's
// depth ladder so "what depth runs in the learned model's time" is answerable.
struct GState { char b[SIZE][SIZE]; int wc, bc, cd, we, be; };
static void snapState(GState& s) {
    for (int y=0;y<SIZE;y++) for (int x=0;x<SIZE;x++) s.b[x][y]=board[x][y];
    s.wc=g_whiteCount; s.bc=g_blackCount; s.cd=g_chipDiff; s.we=g_whiteAtEnd; s.be=g_blackAtEnd;
}
static void restoreState(const GState& s) {
    for (int y=0;y<SIZE;y++) for (int x=0;x<SIZE;x++) board[x][y]=s.b[x][y];
    g_whiteCount=s.wc; g_blackCount=s.bc; g_chipDiff=s.cd; g_whiteAtEnd=s.we; g_blackAtEnd=s.be;
}

int speedBench(const string& boardFile, int positions, double msPerAgent, unsigned seed, int maxDepth,
               int levelReps, int levelWarmup) {
    if (maxDepth < 1) maxDepth = 6;
    if (levelReps < 1) levelReps = 1;
    if (levelWarmup < 0) levelWarmup = 0;
    PRNT = 0; srand(seed);
    bool hasVal = mlLoadSlot(0, "models/lin_value.txt");
    bool hasPol = mlLoadSlot(1, "models/lin_policy.txt");
    // Sparse piece-square value model (feature v2) -> slot 2, so one run compares
    // the full-scan learned leaf (v1, slot 0) against the incremental one (v2).
    bool hasPst = mlLoadSlot(2, "models/pst_value.txt");

    // Build a set of distinct mid-game positions with White to move.
    AgentSpec rnd = agentMakePolicy("rnd", chooserIndexByName("TieredRandom"), 0, 0);
    std::vector<GState> pos;
    for (int i = 0; i < positions; i++) {
        reloadBoard(boardFile);
        int K = 6 + (rand() % 12); K &= ~1;          // even -> White to move
        int v = None;
        for (int h = 0; h < K && !gameOutcome(v); h++)
            v = agentChooseMove(rnd, (h % 2 == 0) ? White : Black);
        if (gameOutcome(v)) { i--; continue; }       // skip finished games
        GState s; snapState(s); pos.push_back(s);
    }
    if (pos.empty()) { cout << "speed: no positions.\n"; return 1; }
    cout << "Speed benchmark over " << pos.size() << " mid-game positions (seed "
         << seed << "), ~" << msPerAgent << " ms/agent (us/move, lower = faster):\n\n";

    int abIdx = explorerIndexByName("AlphaBeta"), grIdx = explorerIndexByName("Greedy");
    int classic = evaluatorIndexByName("Classic"), exper = evaluatorIndexByName("Experimental");

    struct Bench { string name; AgentSpec a; bool search; };
    std::vector<Bench> bs;
    if (hasPol) bs.push_back({ "LearnedPolicy (1-ply)",
        agentMakePolicy("lp", chooserIndexByName("LearnedPolicy"), 0, 1), false });
    if (hasVal) { AgentSpec g = agentMakeSearch("lv", grIdx, learnedValueIndex(), 1, 0);
        bs.push_back({ "Greedy+LearnedValue (1-ply)", g, false }); }

    struct Var { const char* tag; int ev; int p[5]; };
    Var vars[] = {
        { "chip            ", classic, { 0, 4, 0, 0, 0 } },
        { "chip+forward    ", exper,   { 0, 4, 0, 0, 2 } },
        { "chip+structures ", classic, { 0, 4, 2, 2, 0 } },
        { "chip+struct+fwd ", exper,   { 0, 4, 2, 2, 2 } },
    };
    for (Var& v : vars)
        for (int d = 1; d <= maxDepth; d++) {
            AgentSpec a = agentMakeSearch("ab", abIdx, v.ev, d, 0);
            for (int k = 0; k < MAX_EVAL_PARAMS; k++) a.evalParams[k] = (k < 5) ? v.p[k] : 0;
            bs.push_back({ string(v.tag) + "d" + std::to_string(d), a, true });
        }
    // Learned-value AB ladders: v1 (dense aggregates, full feature scan + two
    // move generations at every leaf) vs v2 (sparse piece-square, incremental
    // g_mlAcc accumulator; the leaf is a tanh of a maintained scalar). us/move
    // over nodes/move is the per-node cost these two differ on.
    if (hasVal)
        for (int d = 1; d <= maxDepth; d++)
            bs.push_back({ string("learned-v1-scan  d") + std::to_string(d),
                agentMakeSearch("lv1", abIdx, learnedValueIndex(), d, 0), true });
    if (hasPst)
        for (int d = 1; d <= maxDepth; d++)
            bs.push_back({ string("learned-v2-incr  d") + std::to_string(d),
                agentMakeSearch("lv2", abIdx, learnedValueIndex(), d, 2), true });

    typedef std::chrono::steady_clock hclock;   // monotonic (QPC on MSVC)
    char buf[160];
    snprintf(buf, sizeof(buf), "  %-26s %12s %14s", "agent", "us/move", "nodes/move");
    cout << buf << "\n  " << string(54, '-') << "\n";
    for (Bench& bch : bs) {
        double el = 0; long reps = 0; unsigned long long nodes = 0;
        while (el < msPerAgent) {
            for (GState& s : pos) {
                if (el >= msPerAgent) break;
                restoreState(s);
                g_lastNodes = 0;
                auto t0 = hclock::now();
                agentChooseMove(bch.a, White);
                el += std::chrono::duration<double, std::milli>(hclock::now() - t0).count();
                reps++; nodes += g_lastNodes;
            }
        }
        double us = reps ? el * 1000.0 / reps : 0.0;
        if (bch.search) snprintf(buf, sizeof(buf), "  %-26s %12.3f %14.0f",
                                 bch.name.c_str(), us, reps ? (double)nodes / reps : 0.0);
        else            snprintf(buf, sizeof(buf), "  %-26s %12.3f %14s",
                                 bch.name.c_str(), us, "-");
        cout << buf << "\n";
    }

    // ---- Heuristic eval-level ladder: v1 / v2 / v3 (g_evalLevel) ----
    // Reconstructs prior generations of the heuristic leaf:
    //   v1 = full-board chipDiff() rescan + full evalPosFull scan per leaf
    //   v2 = incremental g_chipDiff + full evalPosFull scan per leaf
    //   v3 = incremental g_chipDiff + cached g_evalPos (the shipping engine)
    // Nonzero wall/column weights so evalPosFull does real work (wall/col = 0
    // short-circuits the structure scan and would hide the cost being compared).
    // Fixed reps + warmup, levels interleaved per position so drift hits all
    // levels equally; per-rep samples kept so dispersion is visible.
    {
        struct LVar { const char* tag; int ev; int p[5]; };
        LVar lvars[] = {
            // Champion weights (w0,l0): the structure scan short-circuits, so the
            // leaf is nearly all chip term -- v1->v2 here is the historically
            // relevant chip-count speedup, and v2->v3 should be ~0 (sanity row).
            { "classic(t1,c4,w0,l0)",   classic, { 1, 4, 0, 0, 0 } },
            { "classic(t1,c4,w2,l2)",   classic, { 1, 4, 2, 2, 0 } },
            { "exp(t1,c4,w2,l2,f2)",    exper,   { 1, 4, 2, 2, 2 } },
        };
        static const char* lvlName[3] = { "v1 fullchip+fullpos",
                                          "v2 chipincr+fullpos",
                                          "v3 fully-incr      " };
        cout << "\nEval-level ladder (" << levelReps << " reps x " << pos.size()
             << " positions per level, " << levelWarmup
             << " warmup pass(es), levels interleaved):\n";

        // Equivalence self-check: every level must produce the same end board and
        // the same node count on every position (same search, only leaf cost
        // differs). Runs as the warmup pass so the timed reps start warm.
        bool checkOk = true;
        int warmPasses = (levelWarmup < 1) ? 1 : levelWarmup;   // >= 1: the check must run
        for (int w = 0; w < warmPasses; w++) {
            for (LVar& v : lvars) {
                for (int d = 1; d <= maxDepth; d++) {
                    AgentSpec a = agentMakeSearch("lvl", abIdx, v.ev, d, 0);
                    for (int k = 0; k < MAX_EVAL_PARAMS; k++) a.evalParams[k] = (k < 5) ? v.p[k] : 0;
                    for (GState& s : pos) {
                        GState ref; unsigned long long refNodes = 0;
                        for (int lvl = 1; lvl <= 3; lvl++) {
                            restoreState(s);
                            g_evalLevel = lvl; g_lastNodes = 0;
                            agentChooseMove(a, White);
                            if (lvl == 1) { snapState(ref); refNodes = g_lastNodes; }
                            else {
                                GState now; snapState(now);
                                bool same = (g_lastNodes == refNodes);
                                for (int y = 0; y < SIZE && same; y++)
                                    for (int x = 0; x < SIZE; x++)
                                        if (now.b[x][y] != ref.b[x][y]) { same = false; break; }
                                if (!same) checkOk = false;
                            }
                        }
                    }
                }
            }
        }
        g_evalLevel = 3;
        cout << "  equivalence self-check (same end board + node count across levels): "
             << (checkOk ? "PASS" : "FAIL") << "\n";
        if (!checkOk) {
            cout << "  FAIL: a reconstructed level diverged; timings below are NOT comparable.\n";
        }

        snprintf(buf, sizeof(buf), "  %-22s %-20s %10s %10s %10s %12s",
                 "agent", "level", "mean us", "median us", "min us", "nodes/move");
        cout << buf << "\n  " << string(90, '-') << "\n";
        for (LVar& v : lvars) {
            for (int d = 1; d <= maxDepth; d++) {
                AgentSpec a = agentMakeSearch("lvl", abIdx, v.ev, d, 0);
                for (int k = 0; k < MAX_EVAL_PARAMS; k++) a.evalParams[k] = (k < 5) ? v.p[k] : 0;
                std::vector<double> samp[3];
                unsigned long long nodes[3] = { 0, 0, 0 };
                for (int r = 0; r < levelReps; r++)
                    for (GState& s : pos)
                        for (int lvl = 1; lvl <= 3; lvl++) {
                            restoreState(s);
                            g_evalLevel = lvl; g_lastNodes = 0;
                            auto t0 = hclock::now();
                            agentChooseMove(a, White);
                            double us = std::chrono::duration<double, std::micro>(hclock::now() - t0).count();
                            samp[lvl-1].push_back(us);
                            nodes[lvl-1] += g_lastNodes;
                        }
                g_evalLevel = 3;
                double mean[3];
                for (int L = 0; L < 3; L++) {
                    std::vector<double>& sp = samp[L];
                    std::sort(sp.begin(), sp.end());
                    double sum = 0; for (double x : sp) sum += x;
                    mean[L] = sum / sp.size();
                    double med = sp[sp.size()/2], mn = sp.front();
                    string aname = string(v.tag) + " d" + std::to_string(d);
                    snprintf(buf, sizeof(buf), "  %-22s %-20s %10.2f %10.2f %10.2f %12.0f",
                             (L == 0) ? aname.c_str() : "",
                             lvlName[L], mean[L], med, mn,
                             (double)nodes[L] / sp.size());
                    cout << buf << "\n";
                }
                snprintf(buf, sizeof(buf),
                         "  %-22s v1->v2 %+.1f%% (chip)   v2->v3 %+.1f%% (structure)",
                         "", (mean[1] - mean[0]) / mean[0] * 100.0,
                         (mean[2] - mean[1]) / mean[1] * 100.0);
                cout << buf << "\n";
            }
        }
    }
    return 0;
}

int runTournament(const string& boardFile, int gamesPerPair, unsigned seed) {
    std::vector<int> depths = { 2, 4, 6, 8, 10 };
    const string out = "data/tourney.jsonl";
    std::vector<std::string> noFilter;
    string runId = makeRunId();
    { std::ofstream clear(out, std::ios::trunc); }   // fresh results file
    writeRunConfig(runId, depths, 300000ULL, gamesPerPair, seed, 1, boardFile, noFilter,
                   "single-process runTournament");
    tournamentPlay(boardFile, depths, gamesPerPair, seed, 0, 1, 300000ULL, out, noFilter);
    return tournamentRate(depths, out, noFilter, runId, "");
}

// ============================================================
// MANIFEST (JSON + Markdown)
// ============================================================
void writeManifest(const std::vector<ModelRecord>& records) {
    std::ofstream j("models/manifest.json");
    if (j.is_open()) {
        j << "[\n";
        for (size_t i = 0; i < records.size(); i++) {
            const ModelRecord& r = records[i];
            j << "  {\"id\":\"" << r.id << "\",\"type\":\"" << r.type << "\",\"head\":\"" << r.head
              << "\",\"regime\":\"" << r.regime << "\",\"conditions\":\"" << dsJsonEscape(r.conditions)
              << "\",\"epoch\":" << r.epoch << ",\"games\":" << r.games
              << ",\"loss\":" << r.loss << ",\"winrate\":" << r.winrate << ",\"elo\":" << (int)r.elo
              << ",\"path\":\"" << dsJsonEscape(r.path) << "\"}" << (i + 1 < records.size() ? "," : "") << "\n";
        }
        j << "]\n";
    }
    std::ofstream m("models/manifest.md");
    if (m.is_open()) {
        m << "# Model checkpoint manifest\n\n";
        m << "| id | type | head | regime | epoch | games | loss | winrate | elo~ | path |\n";
        m << "|----|------|------|--------|-------|-------|------|---------|------|------|\n";
        for (size_t i = 0; i < records.size(); i++) {
            const ModelRecord& r = records[i];
            m << "| " << r.id << " | " << r.type << " | " << r.head << " | " << r.regime << " | "
              << r.epoch << " | " << r.games << " | " << r.loss << " | " << r.winrate << " | "
              << (int)r.elo << " | " << r.path << " |\n";
        }
    }
}

// ============================================================
// REGIME REGISTRY + DOC EXPORT
// ============================================================
const RegimeDef g_regimes[] = {
    { "selfplay-supervised", "Self-play games labeled by outcome; fit a linear value model." },
    { "imitate",             "Behavioral cloning: a teacher's chosen move trains the policy move-rater." },
    { "tdleaf",              "TD-Leaf(lambda) self-play bootstrap of a value model. (future)" },
    { "population",          "Other-play tournaments, Elo-tie labeling, multi-condition runs. (future)" },
    { "tournament",          "Round-robin of composed agents; prints an Elo table." },
    { "docs",                "Regenerate the auto-doc tables from the live registries." },
};
const int g_regimeCount = (int)(sizeof(g_regimes) / sizeof(g_regimes[0]));

static void emitTables(std::ostream& o) {
    o << "### Board-state evaluators\n\n| name | params | incremental |\n|------|--------|-------------|\n";
    for (int i = 0; i < g_evalCount; i++) {
        o << "| " << g_evaluators[i].name << " | " << g_evaluators[i].paramCount << " | "
          << (g_evaluators[i].incremental ? "yes" : "no") << " |\n";
    }
    o << "\n### Move-tree explorers\n\n| name | description |\n|------|-------------|\n";
    for (int i = 0; i < g_explorerCount; i++)
        o << "| " << g_explorers[i].name << " | " << g_explorers[i].desc << " |\n";
    o << "\n### Move choosers / policies\n\n| name | description |\n|------|-------------|\n";
    for (int i = 0; i < g_chooserCount; i++)
        o << "| " << g_choosers[i].name << " | " << g_choosers[i].desc << " |\n";
    o << "\n### Model architectures\n\n| name | implemented | description |\n|------|-------------|-------------|\n";
    for (int i = 0; i < g_modelTypeCount; i++)
        o << "| " << g_modelTypes[i].name << " | " << (g_modelTypes[i].implemented ? "yes" : "no")
          << " | " << g_modelTypes[i].desc << " |\n";
    o << "\n### Training regimes\n\n| name | description |\n|------|-------------|\n";
    for (int i = 0; i < g_regimeCount; i++)
        o << "| " << g_regimes[i].name << " | " << g_regimes[i].desc << " |\n";
    o << "\n### Value features (v" << mlValueFeatureVersion() << ", "
      << mlValueFeatureCount() << ")\n\n";
    for (int i = 0; i < mlValueFeatureCount(); i++)
        o << (i ? ", " : "") << mlValueFeatureName(i);
    o << "\n\n### Value features (v2, " << MLV2_FEATURES << ", sparse piece-square)\n\n"
      << "One binary input per (color, square) plus side to move: "
      << mlValueFeatureNameV2(mlSqW(0, 0)) << ".." << mlValueFeatureNameV2(mlSqW(SIZE-1, SIZE-1))
      << " (64), " << mlValueFeatureNameV2(mlSqB(0, 0)) << ".." << mlValueFeatureNameV2(mlSqB(SIZE-1, SIZE-1))
      << " (64), " << mlValueFeatureNameV2(MLV2_STM)
      << ". A move changes 2-3 inputs, which is what the incremental g_mlAcc search "
      << "accumulator exploits (train with selfplay-supervised --feature-version 2).";
    o << "\n\n### Move features (v" << mlMoveFeatureVersion() << ", "
      << mlMoveFeatureCount() << ")\n\n";
    for (int i = 0; i < mlMoveFeatureCount(); i++)
        o << (i ? ", " : "") << mlMoveFeatureName(i);
    o << "\n";
}

int exportDocs(const string& mlMdPath) {
    const string BEGIN = "<!-- AUTODOC:BEGIN -->";
    const string END   = "<!-- AUTODOC:END -->";

    // Read existing file (if any).
    string content;
    { std::ifstream f(mlMdPath); if (f.is_open()) { std::string line; while (std::getline(f, line)) content += line + "\n"; } }

    std::ostringstream gen;
    gen << BEGIN << "\n\n_Auto-generated by `train.exe docs` from the live registries. Do not edit by hand._\n\n";
    emitTables(gen);
    gen << "\n" << END;

    auto b = content.find(BEGIN);
    auto e = content.find(END);
    if (b != string::npos && e != string::npos && e > b) {
        content = content.substr(0, b) + gen.str() + content.substr(e + END.size());
    } else {
        if (!content.empty() && content.back() != '\n') content += "\n";
        content += "\n" + gen.str() + "\n";
    }
    std::ofstream out(mlMdPath);
    if (!out.is_open()) { cout << "Could not write " << mlMdPath << "\n"; return 1; }
    out << content;

    // Also emit a machine-readable registries JSON.
    std::ofstream rj("models/registries.json");
    if (rj.is_open()) {
        rj << "{\n  \"evaluators\": [";
        for (int i = 0; i < g_evalCount; i++) rj << (i?", ":"") << "\"" << g_evaluators[i].name << "\"";
        rj << "],\n  \"explorers\": [";
        for (int i = 0; i < g_explorerCount; i++) rj << (i?", ":"") << "\"" << g_explorers[i].name << "\"";
        rj << "],\n  \"choosers\": [";
        for (int i = 0; i < g_chooserCount; i++) rj << (i?", ":"") << "\"" << g_choosers[i].name << "\"";
        rj << "],\n  \"model_types\": [";
        for (int i = 0; i < g_modelTypeCount; i++) rj << (i?", ":"") << "\"" << g_modelTypes[i].name << "\"";
        rj << "],\n  \"regimes\": [";
        for (int i = 0; i < g_regimeCount; i++) rj << (i?", ":"") << "\"" << g_regimes[i].name << "\"";
        rj << "],\n  \"value_feature_version\": " << mlValueFeatureVersion()
           << ",\n  \"move_feature_version\": " << mlMoveFeatureVersion() << "\n}\n";
    }
    cout << "Regenerated auto-doc tables in " << mlMdPath << " and models/registries.json\n";
    return 0;
}
