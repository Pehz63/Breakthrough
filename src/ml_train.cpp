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

// ============================================================
// REGISTRY LOOKUPS + SMALL UTILITIES
// ============================================================
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
// TOURNAMENT + RATING
// ============================================================
void rateAgents(std::vector<AgentSpec>& agents, std::vector<double>& ratingsOut,
                const string& boardFile, int gamesPerPair, unsigned seed) {
    srand(seed);
    int n = (int)agents.size();
    struct GR { int a, b; double sa; };
    std::vector<GR> results;

    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            for (int g = 0; g < gamesPerPair; g++) {
                bool iWhite = (g % 2 == 0);
                int wi = iWhite ? i : j, bi = iWhite ? j : i;
                int oc = gameOutcome(playGame(agents[wi], agents[bi], boardFile, 400, nullptr));
                double scoreWhite = (oc == 1) ? 1.0 : (oc == 2) ? 0.0 : 0.5;
                double sa = iWhite ? scoreWhite : (1.0 - scoreWhite);
                GR r; r.a = i; r.b = j; r.sa = sa; results.push_back(r);
            }

    ratingsOut.assign(n, 1500.0);
    double K = 12.0;
    for (int pass = 0; pass < 2000; pass++) {
        for (size_t r = 0; r < results.size(); r++)
            eloUpdate(ratingsOut[results[r].a], ratingsOut[results[r].b], results[r].sa, K);
        if (pass % 200 == 199) K *= 0.7;
    }
    double mean = 0.0; for (int i = 0; i < n; i++) mean += ratingsOut[i]; mean /= (n ? n : 1);
    for (int i = 0; i < n; i++) ratingsOut[i] += 1500.0 - mean;
}

int runTournament(const string& boardFile, int gamesPerPair, unsigned seed) {
    PRNT = 0;
    bool haveVal = mlLoadSlot(0, "models/lin_value.txt");
    bool havePol = mlLoadSlot(1, "models/lin_policy.txt");

    int abIdx = explorerIndexByName("AlphaBeta");
    int grIdx = explorerIndexByName("Greedy");
    int classic = evaluatorIndexByName("Classic");

    std::vector<AgentSpec> roster;
    roster.push_back(agentMakePolicy("UniformRandom", chooserIndexByName("UniformRandom"), 0, 0));
    roster.push_back(agentMakePolicy("TieredRandom", chooserIndexByName("TieredRandom"), 0, 0));
    roster.push_back(agentMakeSearch("Greedy-Classic", grIdx, classic, 1, 0));
    roster.push_back(agentMakeSearch("AlphaBeta-Classic-d2", abIdx, classic, 2, 0));
    if (haveVal) {
        roster.push_back(agentMakeSearch("Greedy-Learned", grIdx, learnedValueIndex(), 1, 0));
        roster.push_back(agentMakeSearch("AlphaBeta-Learned-d2", abIdx, learnedValueIndex(), 2, 0));
    }
    if (havePol)
        roster.push_back(agentMakePolicy("LearnedPolicy", chooserIndexByName("LearnedPolicy"), 0, 1));

    std::vector<double> ratings;
    cout << "Rating " << roster.size() << " agents (" << gamesPerPair << " games/pair)...\n";
    rateAgents(roster, ratings, boardFile, gamesPerPair, seed);

    std::vector<int> order(roster.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b){ return ratings[a] > ratings[b]; });

    cout << "\n  Elo   Agent\n  ----  -----------------------------------\n";
    std::ofstream lib("agents/library.txt");
    for (size_t k = 0; k < order.size(); k++) {
        int i = order[k];
        cout << "  " << (int)ratings[i] << "  " << agentDescribe(roster[i]) << "\n";
        if (lib.is_open()) lib << (int)ratings[i] << "\t" << agentDescribe(roster[i]) << "\n";
        dsAppendLine("data/agents.jsonl",
                     "{\"name\":\"" + string(roster[i].name) + "\",\"elo\":" +
                     std::to_string((int)ratings[i]) + ",\"desc\":\"" +
                     dsJsonEscape(agentDescribe(roster[i])) + "\"}");
    }
    cout << "\nWrote agents/library.txt and data/agents.jsonl\n";
    return 0;
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
