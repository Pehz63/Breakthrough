#include "ml_train.h"
#include "explorers.h"
#include "choosers.h"
#include "ai_eval.h"
#include "ml_eval.h"
#include "ml_model.h"
#include "ml_features.h"
#include "datastore.h"
#include "board_io.h"
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

static int playGame(const AgentSpec& w, const AgentSpec& b, const string& boardFile,
                    int maxHalf, std::vector<PosCap>* cap) {
    reloadBoard(boardFile);
    int victor = None;
    for (int h = 0; h < maxHalf; h++) {
        int side = (h % 2 == 0) ? White : Black;
        if (cap) {
            PosCap pc;
            pc.f.resize(MLV_FEATURES);
            mlExtractValueFeatures(side, pc.f.data());
            pc.side = side;
            PosKey k = positionKey(side, true);
            pc.hash = k.hash;
            pc.enc  = k.enc;
            cap->push_back(pc);
        }
        victor = agentChooseMove(side == White ? w : b, side);
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

// ============================================================
// REGIME: SUPERVISED VALUE
// ============================================================
int trainSupervisedValue(const string& outDir, const string& boardFile, int games,
                         int epochs, double lr, int ckptEvery, int genDepth,
                         double genRandom, unsigned seed) {
    srand(seed);
    PRNT = 0;

    AgentSpec gen = agentMakeSearch("gen", explorerIndexByName("AlphaBeta"),
                                    evaluatorIndexByName("Classic"), genDepth, 0);
    gen.randomMoveProb = genRandom;       // dilution -> position diversity

    std::vector<std::vector<float> > X;
    std::vector<float> Y;
    std::vector<unsigned long long> H;     // position hash (parallel to X)
    std::vector<string> E;                 // position encoding
    std::vector<int> S;                    // side to move
    int wWins = 0, bWins = 0, draws = 0;
    for (int g = 0; g < games; g++) {
        std::vector<PosCap> gf;
        int v = playGame(gen, gen, boardFile, 400, &gf);
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
    if (X.empty()) { cout << "No data.\n"; return 1; }

    LinearModel model(HEAD_VALUE, mlValueFeatureVersion(), MLV_FEATURES, 900.0f);
    for (int i = 0; i < model.n; i++) model.w[i] = randWeight();

    std::vector<int> idx(X.size());
    for (size_t i = 0; i < idx.size(); i++) idx[i] = (int)i;

    std::vector<ModelRecord> records;
    string runId = "linval_" + std::to_string((long)time(nullptr));

    for (int e = 1; e <= epochs; e++) {
        shuffleVec(idx);
        double sum = 0.0;
        for (size_t i = 0; i < idx.size(); i++)
            sum += model.sgdLogisticStep(X[idx[i]].data(), MLV_FEATURES, Y[idx[i]], lr);
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
        r.conditions = "genDepth=" + std::to_string(genDepth) + ",genRandom=" + std::to_string(genRandom);
        r.path = path; r.epoch = e; r.games = games; r.loss = avg; r.winrate = wr; r.elo = elo;
        records.push_back(r);

        string ml = "{\"id\":\"" + r.id + "\",\"regime\":\"" + r.regime + "\",\"epoch\":" +
                    std::to_string(e) + ",\"loss\":" + std::to_string(avg) + ",\"winrate_vs_random\":" +
                    std::to_string(wr) + ",\"elo\":" + std::to_string(elo) + ",\"path\":\"" +
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
    // labels, evaluations). Capped so a big run does not write a huge file.
    {
        std::vector<int> sample(X.size());
        for (size_t i = 0; i < sample.size(); i++) sample[i] = (int)i;
        shuffleVec(sample);
        int cap = (int)std::min((size_t)800, sample.size());
        for (int s = 0; s < cap; s++) {
            int i = sample[s];
            int ev = (int)lround(tanh(model.forward(X[i].data(), MLV_FEATURES)) * model.outScale);
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
                         int epochs, double lr, int teacherDepth, unsigned seed) {
    srand(seed);
    PRNT = 0;

    AgentSpec teacher = agentMakeSearch("teacher", explorerIndexByName("AlphaBeta"),
                                        evaluatorIndexByName("Classic"), teacherDepth, 0);
    teacher.randomMoveProb = 0.10;       // mild noise for varied positions

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
                 std::to_string(wr) + ",\"path\":\"" + dsJsonEscape(outFile) + "\"}");
    return 0;
}

// ============================================================
// TOURNAMENT: roster + sharded play + Elo fit
// ============================================================
// Reasonable "preset" weightings for the simple evaluators, so the roster has many
// distinct bots from a handful of presets x depths. Params are the standard layout
// (turn, chip, wall, column, [advance]); a Learned preset carries the model slot.
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

std::vector<AgentSpec> buildTournamentRoster(const std::vector<int>& depths,
                                             bool hasValue, bool hasPolicy) {
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
    presets.push_back({ "Exp-adv",      exper,   { 1, 4, 1, 1, 2 } });
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
// TOURNAMENT: play one shard (with per-move timing)
// ============================================================
int tournamentPlay(const string& boardFile, const std::vector<int>& depths,
                   int gamesPerPair, unsigned seed, int shard, int ofK,
                   unsigned long long nodeBudget, const string& outFile,
                   const std::vector<std::string>& only) {
    PRNT = 0;
    bool hasVal = mlLoadSlot(0, "models/lin_value.txt");
    bool hasPol = mlLoadSlot(1, "models/lin_policy.txt");
    std::vector<AgentSpec> roster = filterRosterByName(buildTournamentRoster(depths, hasVal, hasPol), only);
    int n = (int)roster.size();

    g_nodeBudget = nodeBudget;
    srand(seed * 1000u + (unsigned)shard + 1u);

    std::vector<double> msTotal(n, 0.0), msMax(n, 0.0);
    std::vector<long long> moveCnt(n, 0);

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
                    auto t0 = clock::now();
                    victor = agentChooseMove(roster[ai], side);
                    double dt = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
                    msTotal[ai] += dt; moveCnt[ai]++; if (dt > msMax[ai]) msMax[ai] = dt;
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
        if (moveCnt[k] > 0)
            out << "{\"timing\":1,\"name\":\"" << roster[k].name << "\",\"ms_total\":"
                << msTotal[k] << ",\"moves\":" << moveCnt[k] << ",\"ms_max\":" << msMax[k] << "}\n";

    g_nodeBudget = 0;
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
                   const string& runId, const string& note) {
    (void)note;
    PRNT = 0;
    bool hasVal = mlLoadSlot(0, "models/lin_value.txt");
    bool hasPol = mlLoadSlot(1, "models/lin_policy.txt");
    std::vector<AgentSpec> roster = filterRosterByName(buildTournamentRoster(depths, hasVal, hasPol), only);
    int n = (int)roster.size();

    std::map<string, int> idx;
    for (int i = 0; i < n; i++) idx[roster[i].name] = i;

    std::vector<GameRec> results;
    std::vector<double> msTotal(n, 0.0), msMax(n, 0.0);
    std::vector<long long> moveCnt(n, 0), gamesCnt(n, 0);

    std::ifstream f(inFile);
    if (!f.is_open()) { cout << "Cannot open " << inFile << "\n"; return 1; }
    string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line.find("\"timing\"") != string::npos) {
            string nm; double mt = 0, mv = 0, mx = 0;
            if (jsonStr(line, "name", nm) && idx.count(nm)) {
                int k = idx[nm];
                jsonNum(line, "ms_total", mt); jsonNum(line, "moves", mv); jsonNum(line, "ms_max", mx);
                msTotal[k] += mt; moveCnt[k] += (long long)mv; if (mx > msMax[k]) msMax[k] = mx;
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
