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
            seed);
    } else if (cmd == "imitate") {
        rc = trainImitationPolicy(
            getOpt(argc, argv, "--out", "models/lin_policy.txt"),
            board,
            getInt(argc, argv, "--games", 150),
            getInt(argc, argv, "--epochs", 15),
            getDbl(argc, argv, "--lr", 0.1),
            getInt(argc, argv, "--teacher-depth", 3),
            seed);
    } else if (cmd == "tournament") {
        rc = runTournament(board, getInt(argc, argv, "--games", 10), seed);
    } else if (cmd == "tournament-play") {
        rc = tournamentPlay(board, getDepths(argc, argv),
                            getInt(argc, argv, "--games", 10), seed,
                            getInt(argc, argv, "--shard", 0), getInt(argc, argv, "--of", 1),
                            (unsigned long long)getDbl(argc, argv, "--node-budget", 300000),
                            getOpt(argc, argv, "--out", "data/tourney.jsonl"),
                            getOnly(argc, argv));
    } else if (cmd == "tournament-rate") {
        rc = tournamentRate(getDepths(argc, argv), getOpt(argc, argv, "--in", "data/tourney.jsonl"),
                            getOnly(argc, argv), getOpt(argc, argv, "--run", ""),
                            getOpt(argc, argv, "--note", ""));
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
