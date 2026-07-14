// train.exe -- command-line front end for the modular ML toolchain.
//
// Subcommands (see g_regimes in ml_train.cpp for the registry):
//   selfplay-supervised  train a linear value model from self-play outcomes
//   imitate              train a linear policy move-rater by behavioral cloning
//   tournament           round-robin a mixed roster and print an Elo table
//   docs                 regenerate the auto-doc tables from the live registries
//
// All options are --key value; run "train.exe" with no args for usage.

#include "globals.h"
#include "ml_train.h"
#include "ml_eval.h"
#include <cstring>
#include <cstdlib>
#include <vector>

static const char* getOpt(int argc, char** argv, const char* key, const char* def) {
    for (int i = 2; i < argc - 1; i++)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return def;
}
static int    getInt(int argc, char** argv, const char* key, int def)    { const char* v = getOpt(argc, argv, key, nullptr); return v ? atoi(v) : def; }
static double getDbl(int argc, char** argv, const char* key, double def) { const char* v = getOpt(argc, argv, key, nullptr); return v ? atof(v) : def; }

// Parse a "1,2,4,6,8,10" depth list; falls back to the default ladder if absent/empty.
static std::vector<int> getDepths(int argc, char** argv) {
    std::vector<int> v;
    const char* s = getOpt(argc, argv, "--depths", nullptr);
    if (s) {
        string str = s;
        size_t i = 0;
        while (i <= str.size()) {
            size_t c = str.find(',', i);
            string tok = str.substr(i, (c == string::npos ? str.size() : c) - i);
            if (!tok.empty()) { try { v.push_back(std::stoi(tok)); } catch (...) {} }
            if (c == string::npos) break;
            i = c + 1;
        }
    }
    if (v.empty()) v = { 2, 4, 6, 8, 10 };
    return v;
}

// Parse an agent-name allowlist "AB6-Classic-chip,LearnedPolicy" into trimmed,
// non-empty tokens. Empty/absent -> empty vector -> full roster (default behavior).
static std::vector<std::string> getOnly(int argc, char** argv) {
    std::vector<std::string> v;
    const char* s = getOpt(argc, argv, "--only", nullptr);
    if (!s) return v;
    std::string str = s;
    size_t i = 0;
    while (i <= str.size()) {
        size_t c = str.find(',', i);
        size_t end = (c == std::string::npos) ? str.size() : c;
        size_t a = i, b = end;
        while (a < b && (str[a] == ' ' || str[a] == '\t')) a++;
        while (b > a && (str[b-1] == ' ' || str[b-1] == '\t')) b--;
        if (b > a) v.push_back(str.substr(a, b - a));
        if (c == std::string::npos) break;
        i = c + 1;
    }
    return v;
}

// Parse a "1000,10000,100000" node-budget list for the opt-in budget ladder.
static std::vector<unsigned long long> getBudgets(int argc, char** argv) {
    std::vector<unsigned long long> v;
    const char* s = getOpt(argc, argv, "--budgets", nullptr);
    if (!s) return v;
    string str = s; size_t i = 0;
    while (i <= str.size()) {
        size_t c = str.find(',', i);
        string tok = str.substr(i, (c == string::npos ? str.size() : c) - i);
        if (!tok.empty()) { try { v.push_back(std::stoull(tok)); } catch (...) {} }
        if (c == string::npos) break;
        i = c + 1;
    }
    return v;
}
// Parse a "1,4,2,2,2" weight list into a vector (empty/absent -> empty = registry defaults).
static std::vector<int> getIntList(int argc, char** argv, const char* key) {
    std::vector<int> v;
    const char* s = getOpt(argc, argv, key, nullptr);
    if (!s) return v;
    string str = s; size_t i = 0;
    while (i <= str.size()) {
        size_t c = str.find(',', i);
        string tok = str.substr(i, (c == string::npos ? str.size() : c) - i);
        if (!tok.empty()) { try { v.push_back(std::stoi(tok)); } catch (...) {} }
        if (c == string::npos) break;
        i = c + 1;
    }
    return v;
}
// Presence flag, e.g. "--ablate".
static bool hasFlag(int argc, char** argv, const char* key) {
    for (int i = 2; i < argc; i++) if (std::strcmp(argv[i], key) == 0) return true;
    return false;
}

static void usage() {
    cout << "Breakthrough ML trainer\n\n";
    cout << "Usage: train.exe <command> [--key value ...]\n\n";
    cout << "Commands:\n";
    for (int i = 0; i < g_regimeCount; i++)
        cout << "  " << g_regimes[i].name << "\n      " << g_regimes[i].desc << "\n";
    cout << "\nCommon options:\n";
    cout << "  --board <file>   starting board (default boards/board1.txt)\n";
    cout << "  --seed <n>       RNG seed (default: time)\n";
    cout << "\nExamples:\n";
    cout << "  train.exe selfplay-supervised --out models/lin_value --games 300 --epochs 12\n";
    cout << "  train.exe selfplay-supervised --out models/pst_value --feature-version 2 --games 300 --epochs 6\n";
    cout << "      (--feature-version 2 = sparse piece-square features, the incremental-search layout)\n";
    cout << "  train.exe selfplay-supervised --out models/pst_boot1 --feature-version 2 --gen-model models/pst_value.txt --gen-model-explorer alphabeta --gen-depth 4 --games 200\n";
    cout << "      (self-play bootstrap: the teacher IS a previously-trained model instead of a heuristic)\n";
    cout << "  train.exe selfplay-supervised --out models/pst_replay --feature-version 2 --from-data data/replay_v2.jsonl\n";
    cout << "      (fit on positions replayed from ranking/matches.jsonl by rank.exe extract, instead of fresh self-play)\n";
    cout << "  --l2 <f>                     weight decay in the SGD step (0 = off, the historical behavior)\n";
    cout << "  --model-type linear|mlp      inner value-model architecture (default linear)\n";
    cout << "  --mlp-hidden \"32\" or \"32,16\"  MLP hidden-layer widths (default 32 when --model-type mlp)\n";
    cout << "  --residual-skip <f>          frozen chip-count skip: 0=off, >0 fixed weight, <0 auto-calibrate\n";
    cout << "  --val-split <f>              hold out this fraction as a validation set (0=off); prints per-epoch val loss + held-out stratified loss\n";
    cout << "  --early-stop                 with --val-split, keep the lowest-validation-loss epoch as the saved model\n";
    cout << "  train.exe selfplay-supervised --out models/res_mlp --feature-version 2 --from-data data/replay_v2.jsonl --model-type mlp --mlp-hidden 32 --residual-skip -1\n";
    cout << "      (nonlinear residual value head: chipCount skip + an MLP learning the rest)\n";
    cout << "  --gen-random-floor <f> --gen-random-decay-plies <n>  linearly decay teacher dilution from\n";
    cout << "      --gen-random to --gen-random-floor over that many half-moves (0 plies = off, constant dilution)\n";
    cout << "  train.exe imitate --out models/lin_policy.txt --games 150 --epochs 15\n";
    cout << "  train.exe tournament --games 10                  (single-process, default depths)\n";
    cout << "  train.exe tournament-play --shard 0 --of 8 --depths \"2,4,6,8,10\" --games 10 --node-budget 300000\n";
    cout << "  train.exe tournament-rate --depths \"2,4,6,8,10\" --in data/tourney.jsonl\n";
    cout << "  (or use tools/run_tournament.ps1 to shard across all CPUs in parallel)\n";
    cout << "  train.exe run-note --run <id> --note \"CPU throttled, ignore ms/move\"\n";
    cout << "  train.exe docs --ml ML.md\n";
    cout << "\nTournament options:\n";
    cout << "  --only \"n1,n2,..\"  restrict the roster to these agent names (default: full roster)\n";
    cout << "  --run <id>         archive the run under runs/<id>/ (rate phase)\n";
    cout << "  --note \"text\"      pre-run note stored in the run config / notes (run-config)\n";
    cout << "  --time-budget-ms n per-move wall-clock budget (0 = off; composes with --node-budget)\n";
    cout << "  --budgets \"a,b,c\"  add a budget-ladder of unbounded-depth agents, one per node budget\n";
    cout << "  --ablate           add a feature-ablation family (AB/TT/ordering/aspiration toggles), both evaluators\n";
    cout << "  --forward-study    add a forward-weight study family (Experimental, forward in 0/1/2/4/8)\n";
    cout << "  train.exe turn-swing --games 60 --chip 4 --wall 2 --col 2 --forward 2   (calibrate a turn weight)\n";
    cout << "  train.exe speed --positions 24 --ms 150   (per-move us of learned model vs AB variants/depths;\n";
    cout << "                  --reps 8 --warmup 1 also run the heuristic eval-level ladder v1/v2/v3\n";
    cout << "                  (full chip rescan / incremental chip / fully incremental) with a self-check)\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 0; }
    string cmd = argv[1];
    unsigned seed = (unsigned)getInt(argc, argv, "--seed", (int)time(nullptr));
    string board = getOpt(argc, argv, "--board", "boards/board1.txt");

    int rc = 0;
    if (cmd == "selfplay-supervised") {
        rc = trainSupervisedValue(
            getOpt(argc, argv, "--out", "models/lin_value"),
            board,
            getInt(argc, argv, "--games", 300),
            getInt(argc, argv, "--epochs", 12),
            getDbl(argc, argv, "--lr", 0.05),
            getInt(argc, argv, "--ckpt-every", 3),
            getInt(argc, argv, "--gen-depth", 2),
            getDbl(argc, argv, "--gen-random", 0.2),
            seed,
            getOpt(argc, argv, "--gen-eval", "Classic"),
            getIntList(argc, argv, "--gen-params"),
            getInt(argc, argv, "--feature-version", 1),
            getDbl(argc, argv, "--l2", 0.0),
            getDbl(argc, argv, "--gen-random-floor", 0.0),
            getInt(argc, argv, "--gen-random-decay-plies", 0),
            getOpt(argc, argv, "--gen-model", ""),
            getOpt(argc, argv, "--gen-model-explorer", "alphabeta"),
            getOpt(argc, argv, "--from-data", ""),
            getOpt(argc, argv, "--model-type", "linear"),
            getIntList(argc, argv, "--mlp-hidden"),
            getDbl(argc, argv, "--residual-skip", 0.0),
            getDbl(argc, argv, "--val-split", 0.0),
            hasFlag(argc, argv, "--early-stop"));
    } else if (cmd == "imitate") {
        rc = trainImitationPolicy(
            getOpt(argc, argv, "--out", "models/lin_policy.txt"),
            board,
            getInt(argc, argv, "--games", 150),
            getInt(argc, argv, "--epochs", 15),
            getDbl(argc, argv, "--lr", 0.1),
            getInt(argc, argv, "--teacher-depth", 3),
            seed,
            getOpt(argc, argv, "--teacher-eval", "Classic"),
            getIntList(argc, argv, "--teacher-params"));
    } else if (cmd == "tournament") {
        rc = runTournament(board, getInt(argc, argv, "--games", 10), seed);
    } else if (cmd == "tournament-play") {
        rc = tournamentPlay(board, getDepths(argc, argv),
                            getInt(argc, argv, "--games", 10), seed,
                            getInt(argc, argv, "--shard", 0), getInt(argc, argv, "--of", 1),
                            (unsigned long long)getDbl(argc, argv, "--node-budget", 300000),
                            getOpt(argc, argv, "--out", "data/tourney.jsonl"),
                            getOnly(argc, argv),
                            getDbl(argc, argv, "--time-budget-ms", 0.0),
                            getBudgets(argc, argv), hasFlag(argc, argv, "--ablate"),
                            hasFlag(argc, argv, "--forward-study"));
    } else if (cmd == "tournament-rate") {
        rc = tournamentRate(getDepths(argc, argv), getOpt(argc, argv, "--in", "data/tourney.jsonl"),
                            getOnly(argc, argv), getOpt(argc, argv, "--run", ""),
                            getOpt(argc, argv, "--note", ""),
                            getBudgets(argc, argv), hasFlag(argc, argv, "--ablate"),
                            hasFlag(argc, argv, "--forward-study"));
    } else if (cmd == "speed") {
        rc = speedBench(board, getInt(argc, argv, "--positions", 24),
                        getDbl(argc, argv, "--ms", 150.0), seed,
                        getInt(argc, argv, "--maxdepth", 6),
                        getInt(argc, argv, "--reps", 8),
                        getInt(argc, argv, "--warmup", 1));
    } else if (cmd == "turn-swing") {
        rc = turnSwing(board, getInt(argc, argv, "--games", 60),
                       getInt(argc, argv, "--depth", 4), seed,
                       getInt(argc, argv, "--chip", 4),
                       getInt(argc, argv, "--wall", 2),
                       getInt(argc, argv, "--col", 2),
                       getInt(argc, argv, "--forward", 0));
    } else if (cmd == "run-config") {
        const char* rid = getOpt(argc, argv, "--run", nullptr);
        string runId = rid ? string(rid) : makeRunId();
        writeRunConfig(runId, getDepths(argc, argv),
                       (unsigned long long)getDbl(argc, argv, "--node-budget", 300000),
                       getInt(argc, argv, "--games", 10), seed,
                       getInt(argc, argv, "--workers", 1), board,
                       getOnly(argc, argv), getOpt(argc, argv, "--note", ""));
        cout << "Wrote runs/" << runId << "/config.json + notes.md\n";
        rc = 0;
    } else if (cmd == "run-note") {
        rc = runNote(getOpt(argc, argv, "--run", ""), getOpt(argc, argv, "--note", ""));
    } else if (cmd == "docs") {
        rc = exportDocs(getOpt(argc, argv, "--ml", "ML.md"));
    } else {
        cout << "Unknown command: " << cmd << "\n\n";
        usage();
        rc = 1;
    }

    mlClearSlots();
    return rc;
}
