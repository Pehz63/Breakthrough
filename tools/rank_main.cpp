// rank.exe -- persistent agent Elo ranking front end (see src/ranking.h).
//
// Agents live in ranking/roster.txt (one "anchor|on|off <id>" line each), games
// accumulate forever in ranking/matches.jsonl keyed by canonical agent IDs, and
// ratings come from an anchored Bradley-Terry refit. Adding one agent to the
// roster only schedules its missing pairings (O(N) games), never a replay.
//
// All options are --key value; run "rank.exe" with no args for usage.

#include "globals.h"
#include "ranking.h"
#include "ml_eval.h"
#include <cstring>
#include <cstdlib>

static const char* getOpt(int argc, char** argv, const char* key, const char* def) {
    for (int i = 2; i < argc - 1; i++)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return def;
}
static int getInt(int argc, char** argv, const char* key, int def) {
    const char* v = getOpt(argc, argv, key, nullptr);
    return v ? atoi(v) : def;
}
static bool hasFlag(int argc, char** argv, const char* key) {
    for (int i = 2; i < argc; i++) if (std::strcmp(argv[i], key) == 0) return true;
    return false;
}

static void usage() {
    cout << "Breakthrough agent Elo ranking\n\n";
    cout << "Usage: rank.exe <command> [--key value ...]\n\n";
    cout << "Commands:\n";
    cout << "  check      validate the roster, print model hashes + the pending-game count\n";
    cout << "  play       play this shard's pending games, append them to the store\n";
    cout << "  rate       Bradley-Terry refit from the store -> ranking/ratings.tsv + report.md\n";
    cout << "  run        serial play then rate (the everyday command)\n";
    cout << "  history    per-opponent record + recent games for one agent\n";
    cout << "  gauntlet   rate one candidate id vs the frozen pool (O(N) games, for hill climbing)\n";
    cout << "\nCommon options (defaults):\n";
    cout << "  --roster ranking/roster.txt   editable agent list: 'anchor|on|off <id>' lines\n";
    cout << "  --in ranking/matches.jsonl    the append-only match store\n";
    cout << "  --board boards/board1.txt     starting board (history is kept per board)\n";
    cout << "  --games 8                     target games per pair (play/run/check),\n";
    cout << "                                or games per opponent (gauntlet)\n";
    cout << "  --seed 1                      run seed (per-game seeds derive from it)\n";
    cout << "\nplay:     --out <file> (default = --in), --shard i --of k (process sharding)\n";
    cout << "history:  --agent <id or unique prefix>, --last 20\n";
    cout << "gauntlet: --id <candidate id>, --keep (append to the store instead of scratch)\n";
    cout << "\nExamples:\n";
    cout << "  rank.exe check\n";
    cout << "  rank.exe run --games 8\n";
    cout << "  rank.exe history --agent \"ab(d4\"\n";
    cout << "  rank.exe gauntlet --id \"ab(d5).classic(t1,c4,w0,l0).v1\" --games 4\n";
    cout << "  (use tools\\run_rank.ps1 -Workers 8 to shard play across processes)\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 0; }
    string cmd = argv[1];
    string roster = getOpt(argc, argv, "--roster", "ranking/roster.txt");
    string store  = getOpt(argc, argv, "--in", "ranking/matches.jsonl");
    string board  = getOpt(argc, argv, "--board", "boards/board1.txt");
    int games     = getInt(argc, argv, "--games", 8);
    unsigned seed = (unsigned)getInt(argc, argv, "--seed", 1);

    int rc = 0;
    if (cmd == "check") {
        rc = rankCheck(roster, store, games, board);
    } else if (cmd == "play") {
        rc = rankPlay(roster, store, getOpt(argc, argv, "--out", store.c_str()),
                      games, getInt(argc, argv, "--shard", 0), getInt(argc, argv, "--of", 1),
                      seed, board);
    } else if (cmd == "rate") {
        rc = rankRate(roster, store, board);
    } else if (cmd == "run") {
        rc = rankPlay(roster, store, store, games, 0, 1, seed, board);
        if (rc == 0) rc = rankRate(roster, store, board);
    } else if (cmd == "history") {
        rc = rankHistory(store, getOpt(argc, argv, "--agent", ""),
                         getInt(argc, argv, "--last", 20), board);
    } else if (cmd == "gauntlet") {
        rc = rankGauntlet(roster, store, getOpt(argc, argv, "--id", ""), games,
                          hasFlag(argc, argv, "--keep"), seed, board);
    } else {
        cout << "Unknown command: " << cmd << "\n\n";
        usage();
        rc = 1;
    }
    mlClearSlots();
    return rc;
}
