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
static double getDbl(int argc, char** argv, const char* key, double def) {
    const char* v = getOpt(argc, argv, key, nullptr);
    return v ? atof(v) : def;
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
    cout << "  extract    replay a sample of stored matches, capturing labeled value-model training data\n";
    cout << "  bookgen    mine an opening/refutation book from stored games between two agents\n";
    cout << "  pairgen    play FRESH games between two named agents, capturing labeled training data\n";
    cout << "  opener-bias  measure whether the symmetric random opener handicaps a deterministic champion\n";
    cout << "  opener-swap  color-swap recovery test: same random-opener snapshot played out twice with\n";
    cout << "               colors swapped, to separate 'position favors a color' from 'agent recovers better'\n";
    cout << "  posgen     build a deduped, stratified position pool (train + eval tiers) from stored games\n";
    cout << "  label      play a designed ladder of fresh games from every pool position (raw outcome rows)\n";
    cout << "  labelfit   fit per-position (mu, sigma) Elo-advantage labels from a raw label store\n";
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
    cout << "extract:  --out <file>, --feature-version 2, --sample 3000 (0 = all matching rows)\n";
    cout << "bookgen:  --a <line-owner id> --b <target id> --plies 60 --out models/book<N>.txt\n";
    cout << "          Replays the pair's stored games; keeps positions/moves from A's wins only.\n";
    cout << "          Roster the follower as '<head>.<eval>.opener(book,<N>)@1'.\n";
    cout << "pairgen:  --a <id> --b <id> --games 100 --out data/pairgen.jsonl, --feature-version 2,\n";
    cout << "          --dil-apply a|b|both|none (dilute that agent: --dil-start 0.3 --dil-floor 0.05\n";
    cout << "          --dil-decay-plies 30), --open-plies K (random first K half-moves),\n";
    cout << "          --open-side a|b|both (which side plays the random opener; both = default,\n";
    cout << "          a or b = only that agent, the other plays its own policy inside the window),\n";
    cout << "          --filter winner=a|b|any (emit only that agent's wins), --branch-tries T (rewind\n";
    cout << "          kept A-wins to a random A ply, try a different move, keep the tail if A wins\n";
    cout << "          again), --shard i --of k (vary --out per shard, then concatenate)\n";
    cout << "opener-bias:  --a <champion id> --b <id> --open-plies 6 --games N, --judge <id>\n";
    cout << "          (position scorer; default = --a. Use a learned agent to judge positions\n";
    cout << "          the champion's own coarse eval cannot). No data files written.\n";
    cout << "opener-swap:  --a <id> --b <id> --open-plies 6 --games N (snapshots). Plays each\n";
    cout << "          random-opener snapshot to conclusion twice with colors swapped; reports\n";
    cout << "          White-won-both / Black-won-both (color effect) vs a-won-both / b-won-both\n";
    cout << "          (agent effect). No data files written.\n";
    cout << "posgen:   --out-train/--out-eval <pool files> --train N --eval N (targets),\n";
    cout << "          --per-game 4 --min-ply 6 --max-ply 44. Replays a deterministic sample of\n";
    cout << "          the store into DISTINCT positions (enc + hash + side to move), stratified\n";
    cout << "          by ply band and material, eval tier = hash%17==0 (disjoint from train).\n";
    cout << "label:    --pool <pool file> --ladder <spec: 'rung i <id>' + 'pair wi bi games\n";
    cout << "          [mod k r]' lines> --out <raw store> --shard i --of k (by position),\n";
    cout << "          --resume (top up instead of truncating) --done <merged master store>\n";
    cout << "          --max-positions N (chunking). Plays the design from every position;\n";
    cout << "          raw outcome rows + a .meta.json freezing the rung-id mapping.\n";
    cout << "labelfit: --in <raw store> --pool <pool file> --ratings <snapshot tsv>\n";
    cout << "          --out <labels file> --min-rows 8 --rating-se (fold rating SEs into v).\n";
    cout << "          Per-position probit MLE -> mu/sd in Elo + SEs + QC tables. Rerunnable\n";
    cout << "          against any future ratings file (the raw store never changes).\n";
    cout << "\nExamples:\n";
    cout << "  rank.exe check\n";
    cout << "  rank.exe run --games 8\n";
    cout << "  rank.exe history --agent \"ab(d4\"\n";
    cout << "  rank.exe gauntlet --id \"ab(d5)@1.classic(t1,c4,w0,l0)@1\" --games 4\n";
    cout << "  rank.exe extract --out data/replay_v2.jsonl --feature-version 2 --sample 3000\n";
    cout << "  rank.exe pairgen --a \"ab(d2)@1.classic(t1,c4,w0,l0)@1\" --b \"ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2\" --games 100 --dil-apply a --out data/pg.jsonl\n";
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
    } else if (cmd == "extract") {
        rc = rankExtract(store, getOpt(argc, argv, "--out", "data/replay.jsonl"), board,
                         getInt(argc, argv, "--feature-version", 2),
                         getInt(argc, argv, "--sample", 0), seed);
    } else if (cmd == "bookgen") {
        rc = rankBookGen(store, board, getOpt(argc, argv, "--a", ""),
                         getOpt(argc, argv, "--b", ""),
                         getInt(argc, argv, "--plies", 60),
                         getOpt(argc, argv, "--out", ""));
    } else if (cmd == "pairgen") {
        string dilApply = getOpt(argc, argv, "--dil-apply", "none");
        RankDilOverride dil;
        dil.apply = (dilApply == "a") ? 1 : (dilApply == "b") ? 2
                  : (dilApply == "both") ? 3 : (dilApply == "none") ? 0 : -1;
        if (dil.apply < 0) { cout << "ERROR: --dil-apply must be a, b, both, or none\n"; return 1; }
        dil.start      = getDbl(argc, argv, "--dil-start", 0.3);
        dil.floorProb  = getDbl(argc, argv, "--dil-floor", 0.05);
        dil.decayPlies = getInt(argc, argv, "--dil-decay-plies", 30);
        string filt = getOpt(argc, argv, "--filter", "any");
        if (filt.rfind("winner=", 0) == 0) filt = filt.substr(7);
        int fw = (filt == "a") ? 1 : (filt == "b") ? 2 : (filt == "any") ? 0 : -1;
        if (fw < 0) { cout << "ERROR: --filter must be winner=a, winner=b, or any\n"; return 1; }
        string openSideStr = getOpt(argc, argv, "--open-side", "both");
        int openSide = (openSideStr == "a") ? 1 : (openSideStr == "b") ? 2
                     : (openSideStr == "both") ? 3 : (openSideStr == "none") ? 0 : -1;
        if (openSide < 0) { cout << "ERROR: --open-side must be a, b, both, or none\n"; return 1; }
        rc = rankPairGen(getOpt(argc, argv, "--a", ""), getOpt(argc, argv, "--b", ""),
                         getInt(argc, argv, "--games", 100),
                         getOpt(argc, argv, "--out", "data/pairgen.jsonl"), board,
                         getInt(argc, argv, "--feature-version", 2), seed, dil,
                         getInt(argc, argv, "--open-plies", 0), fw,
                         getInt(argc, argv, "--branch-tries", 0),
                         getInt(argc, argv, "--shard", 0), getInt(argc, argv, "--of", 1),
                         openSide);
    } else if (cmd == "opener-bias") {
        rc = rankOpenerBias(getOpt(argc, argv, "--a", ""), getOpt(argc, argv, "--b", ""),
                            getInt(argc, argv, "--games", 40), board,
                            getInt(argc, argv, "--open-plies", 6), seed,
                            getOpt(argc, argv, "--judge", ""));
    } else if (cmd == "opener-swap") {
        rc = rankOpenerSwap(getOpt(argc, argv, "--a", ""), getOpt(argc, argv, "--b", ""),
                            getInt(argc, argv, "--games", 40), board,
                            getInt(argc, argv, "--open-plies", 6), seed);
    } else if (cmd == "posgen") {
        rc = rankPosGen(store, board,
                        getOpt(argc, argv, "--out-train", "data/labels/pool_train.jsonl"),
                        getOpt(argc, argv, "--out-eval", "data/labels/pool_eval.jsonl"),
                        getInt(argc, argv, "--train", 24000),
                        getInt(argc, argv, "--eval", 1500),
                        getInt(argc, argv, "--per-game", 4),
                        getInt(argc, argv, "--min-ply", 6),
                        getInt(argc, argv, "--max-ply", 44), seed);
    } else if (cmd == "label") {
        rc = rankLabel(getOpt(argc, argv, "--pool", "data/labels/pool_train.jsonl"),
                       getOpt(argc, argv, "--ladder", "data/labels/ladder.txt"),
                       getOpt(argc, argv, "--out", "data/labels/raw.jsonl"),
                       seed, getInt(argc, argv, "--shard", 0), getInt(argc, argv, "--of", 1),
                       hasFlag(argc, argv, "--resume"), getOpt(argc, argv, "--done", ""),
                       getInt(argc, argv, "--max-positions", 0));
    } else if (cmd == "labelfit") {
        rc = rankLabelFit(getOpt(argc, argv, "--in", "data/labels/raw.jsonl"),
                          getOpt(argc, argv, "--pool", "data/labels/pool_train.jsonl"),
                          getOpt(argc, argv, "--ratings", "data/labels/ratings_snapshot.tsv"),
                          getOpt(argc, argv, "--out", "data/labels/labels.jsonl"),
                          getInt(argc, argv, "--min-rows", 8),
                          hasFlag(argc, argv, "--rating-se"));
    } else {
        cout << "Unknown command: " << cmd << "\n\n";
        usage();
        rc = 1;
    }
    mlClearSlots();
    return rc;
}
