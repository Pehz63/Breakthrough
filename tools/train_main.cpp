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

static const char* getOpt(int argc, char** argv, const char* key, const char* def) {
    for (int i = 2; i < argc - 1; i++)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return def;
}
static int    getInt(int argc, char** argv, const char* key, int def)    { const char* v = getOpt(argc, argv, key, nullptr); return v ? atoi(v) : def; }
static double getDbl(int argc, char** argv, const char* key, double def) { const char* v = getOpt(argc, argv, key, nullptr); return v ? atof(v) : def; }

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
    cout << "  train.exe tournament --games 4\n";
    cout << "  train.exe docs --ml ML.md\n";
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
    } else if (cmd == "tournament" || cmd == "rate") {
        rc = runTournament(board, getInt(argc, argv, "--games", 4), seed);
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
