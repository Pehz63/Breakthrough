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
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <vector>

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
    cout << "  seal       roll the oversized live store tail into immutable sealed shards\n";
    cout << "  split      group the store into parts by who played each game (dry run unless --apply)\n";
    cout << "  matchup    regime-vs-regime matrix: actual vs Elo-expected score, and the residual\n";
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
    cout << "  --in ranking/matches.jsonl    the append-only match store (live tail; sealed shards read alongside it)\n";
    cout << "  --max-mb 48                   seal/split: largest part file to emit\n";
    cout << "  --group tdleaf_self --apply   split: id substring getting its own bucket, and commit the move\n";
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
    cout << "\nRating a NEW COHORT without disturbing the existing scale:\n";
    cout << "  rank.exe play --roster <roster+cohort> --cohort <ids.txt> --games 32\n";
    cout << "      --cohort <file>  schedule ONLY pairs touching a listed agent, so a cohort can be\n";
    cout << "                       played in without also topping up every roster-vs-roster pair\n";
    cout << "  rank.exe rate --roster <roster+cohort> --pin ranking/standings.tsv\n";
    cout << "      --pin <ratings/standings tsv>  hold those agents at their listed Elo and solve only\n";
    cout << "                       for the rest, so the reference scale stays fixed across a study.\n";
    cout << "                       SCREENING ONLY: pinned agents cannot move, so a pinned fit can\n";
    cout << "                       never dethrone a champion. Certify with a plain 'rate' (no --pin).\n";
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
                      seed, board, hasFlag(argc, argv, "--paired-openings"),
                      getOpt(argc, argv, "--cohort", ""));
    } else if (cmd == "rate") {
        rc = rankRate(roster, store, board, getOpt(argc, argv, "--pin", ""),
                      hasFlag(argc, argv, "--regime-balanced"));
    } else if (cmd == "run") {
        rc = rankPlay(roster, store, store, games, 0, 1, seed, board,
                      hasFlag(argc, argv, "--paired-openings"),
                      getOpt(argc, argv, "--cohort", ""));
        if (rc == 0) rc = rankRate(roster, store, board, getOpt(argc, argv, "--pin", ""));
    } else if (cmd == "seal") {
        string err;
        long long maxMb = (long long)getInt(argc, argv, "--max-mb", 48);
        int made = rankSealStore(store, maxMb * 1024 * 1024, err);
        if (made < 0) { cout << "ERROR: " << err << "\n"; return 1; }
        if (made == 0) {
            cout << store << " is under " << maxMb << " MB, nothing to seal\n";
        } else {
            cout << "sealed " << made << " shard(s) of at most " << maxMb << " MB; "
                 << store << " is now the live tail\n";
        }
        rc = 0;
    } else if (cmd == "split") {
        string err;
        string group = getOpt(argc, argv, "--group", "tdleaf_self");
        long long maxMb = (long long)getInt(argc, argv, "--max-mb", 48);
        bool apply = hasFlag(argc, argv, "--apply");
        RankSplitStats st;
        if (rankSplitStore(store, roster, group, maxMb * 1024 * 1024, apply, st, err) != 0) {
            cout << "ERROR: " << err << "\n";
            return 1;
        }
        const double mb = 1024.0 * 1024.0;
        cout << (apply ? "SPLIT" : "DRY RUN (pass --apply to perform it)") << "\n";
        cout << "  roster agents   " << st.rosterAgents << "\n";
        cout << "  retired agents  " << st.retired.size() << " (in the store, not in the roster)\n";
        if (st.malformed) cout << "  malformed rows  " << st.malformed << " (kept with the roster part)\n";
        cout << "\n  bucket                      rows        MB  parts\n";
        for (size_t i = 0; i < st.buckets.size(); i++) {
            const RankStoreBucket& bk = st.buckets[i];
            char buf[256];
            std::snprintf(buf, sizeof(buf), "  %-24s %9lld %9.1f  %d",
                          bk.name.c_str(), bk.rows, bk.bytes / mb, (int)bk.parts.size());
            cout << buf << "\n";
        }
        // Ranked by row count: the expensive retirees are what this is for.
        std::vector<std::pair<long long, string> > byRows;
        for (std::map<string, long long>::const_iterator it = st.retired.begin();
             it != st.retired.end(); ++it) byRows.push_back(std::make_pair(it->second, it->first));
        std::sort(byRows.begin(), byRows.end());
        std::reverse(byRows.begin(), byRows.end());
        const size_t show = byRows.size() < 10 ? byRows.size() : 10;
        if (show) cout << "\n  top retired agents by rows:\n";
        for (size_t i = 0; i < show; i++)
            cout << "    " << byRows[i].first << "\t" << byRows[i].second << "\n";
        if (byRows.size() > show)
            cout << "    ... and " << (byRows.size() - show) << " more\n";
        if (apply)
            cout << "\nwrote " << rankStoreIndexPath(store) << "; every part is still loaded,\n"
                 << "so ratings are unchanged until a part's line is removed from the index.\n";
        rc = 0;
    } else if (cmd == "canon") {
        // Print a roster / id-list file with every id rewritten into today's
        // canonical spelling, leaving state tokens, comments, blank lines and
        // column alignment alone. rank.exe is the single source of truth for the
        // rewrite, so a migration can never drift from what the parser accepts.
        std::ifstream f(roster.c_str());
        if (!f.is_open()) { cout << "ERROR: cannot read " << roster << "\n"; return 1; }
        std::string line;
        while (std::getline(f, line)) {
            std::string keep = line;
            while (!keep.empty() && (keep[keep.size()-1] == '\r' || keep[keep.size()-1] == '\n'))
                keep.erase(keep.size()-1);
            // Split off a trailing comment so an id is never confused with prose.
            std::string body = keep, tail;
            size_t hash = keep.find('#');
            if (hash != std::string::npos) { body = keep.substr(0, hash); tail = keep.substr(hash); }
            size_t a = body.find_first_not_of(" \t");
            if (a == std::string::npos) { cout << keep << "\n"; continue; }
            size_t b = body.find_first_of(" \t", a);
            if (b == std::string::npos) { cout << keep << "\n"; continue; }
            std::string state = body.substr(a, b - a);
            size_t c = body.find_first_not_of(" \t", b);
            if (c == std::string::npos) { cout << keep << "\n"; continue; }
            size_t d = body.find_last_not_of(" \t");
            std::string id = body.substr(c, d - c + 1);
            std::string up = rankUpgradeId(id);
            cout << state << std::string(b - a >= 8 ? 1 : 8 - state.size(), ' ') << up;
            if (!tail.empty()) cout << "  " << tail;
            cout << "\n";
        }
        rc = 0;
    } else if (cmd == "matchup") {
        rc = rankMatchup(roster, store, board, getInt(argc, argv, "--min-games", 200));
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
