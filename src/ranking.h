#pragma once
#include "agents.h"
#include <string>
#include <vector>
#include <iosfwd>

// ============================================================
// Persistent agent Elo ranking (rank.exe)
// ============================================================
// A standalone ranking system, separate from the ml_train tournament. Agents are
// identified by a canonical, human-readable ID string (the permanent key for the
// append-only match store ranking/matches.jsonl), listed in a hand-edited roster
// (ranking/roster.txt, one "anchor|on|off <id>" line each). The scheduler only
// plays each active pair's missing games, so adding one agent costs O(N) games
// and nothing is ever recomputed. Ratings come from a deterministic Bradley-Terry
// maximum-likelihood refit over the full store, anchored so the roster's anchor
// agent (UniformRandom by convention) sits at Elo 0.
//
// ID grammar (canonical form; parse rejects anything else and prints the fix).
// Each MODULE carries its own "@<version>", sourced from the codec tables in
// ranking.cpp, so changing one module's code re-identifies only the agents that
// use it (bump the version constant once, and every affected agent gets a new
// identity and a fresh rating; old games stay as history). A stale "@N" in the
// roster fails the canonical check and prints the current form to paste.
//   id      := head [ "." evalseg ] [ "." dilseg ]                (policy: head [ "." linpol ] [ "." dilseg ])
//   head    := ( "rand" | "tiered" | "smart(" N ")" | "policy"    (policy brains)
//              | "greedy" | "ab(" "d" N { "," flag } ")" ) "@" V  (search brains)
//   flag    := "noab" | "tt" | "ord" | "part" | "asp" N
//            | "nb" budget | "tb" N "ms" | "cap" N                (budget: 200k, 2m, raw)
//   evalseg := ( "classic(" weights ")" | "exp(" weights ")"      (search brains only)
//              | "learned(s" slot "," hash8 ")" ) "@" V           (LearnedValue; hash = weights)
//   linpol  := "linpol(s" slot "," hash8 ")"                      (policy-head model payload, no "@V")
//   weights := letter int { "," letter int }                      (ALL params, registry order)
//   dilseg  := "dil(r" pct [ "," "d" N ] ")" "@" V                (r5 = 5% diluted moves;
//                                                                  ,dN = those moves use a
//                                                                  depth-N search, 0<N<depth,
//                                                                  search head only; else random)
//   V       := positive int, the module's code version (from the codec tables)
// Examples: rand@1  smart(4)@1  ab(d6)@1.classic(t2,c10,w3,l2)@1
//           ab(d8,tt,ord,nb200k)@1.exp(t2,c10,w3,l2,f2)@1.dil(r5)@1
//           ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r30,d3)@1
//           greedy@1.learned(s0,ab12cd34)@1  policy@1.linpol(s1,9f3e21aa)

// A roster entry: the engine-playable spec plus the identity fields that
// AgentSpec cannot hold (the full canonical ID string, which embeds each
// module's code version, and the roster toggles).
struct RankAgent {
    AgentSpec   spec;
    std::string id;       // canonical ID, the permanent match-history key
    bool        active;   // "on" or "anchor" in the roster
    bool        anchor;   // the Elo-0 reference agent (exactly one per roster)
};

// One played game in ranking/matches.jsonl. Fields added after the first
// release default to -1 = "not recorded" so old rows keep parsing.
struct RankMatchRow {
    std::string w, b;              // white / black agent IDs
    char        r;                 // 'W', 'B', or 'D' (400-half-move cap; D never occurs in practice)
    int         plies;             // half-moves played
    double      wms, bms;          // per-side total move wall time (ms)
    int         wmv, bmv;          // per-side move counts
    double      wnod, bnod;        // per-side total search nodes (0 for policy brains)
    unsigned    seed;              // the srand seed this game was played with
    std::string board;             // starting board file
    int         par;               // shard count the game ran under (1 = clean wall timing)
    std::string ts, run;           // UTC write time, UTC run stamp
    int         wpc = -1, bpc = -1;        // end-of-game piece counts
    double      wcpu = -1.0, bcpu = -1.0;  // per-side process CPU time (ms), contention-safe
    double      wed = 0.0, bed = 0.0;      // per-side summed effective search depth
    int         wsn = 0, bsn = 0;          // per-side count of search moves behind wed/bed
};

// One not-yet-played game (colors already assigned, seed derived).
struct RankPendingGame {
    std::string w, b;
    unsigned    seed;
};

// Result of the Bradley-Terry fit (only agents with >= 1 game appear).
struct RankFit {
    std::vector<std::string> ids;
    std::vector<double>      elo;
    std::vector<double>      se;           // one standard error, Elo units
    std::vector<char>        provisional;  // 1 = component not connected to the anchor
    bool                     anchored;     // false = anchor had no games (mean-1000 fallback)
};

// ---- ID codec ----
// Emit the canonical ID for a spec: module versions come from the codec tables
// in ranking.cpp, and learned agents hash their model file. Parse validates,
// builds the spec, and enforces canonical form (which also rejects stale
// module versions); on failure returns false with a human-readable reason.
std::string rankAgentId(const AgentSpec& spec);
bool rankAgentFromId(const std::string& id, RankAgent& out, std::string& err);
// First 8 lowercase hex chars of FNV-1a-64 over a file's bytes ("" if unreadable).
std::string rankFileHash8(const std::string& path);
// Verify every g_evaluators entry has a codec row with unique weight letters.
bool rankEvalCodecComplete(std::string& err);

// ---- Roster ----
bool rankLoadRoster(std::istream& in, std::vector<RankAgent>& out, std::string& err);
bool rankLoadRosterFile(const std::string& path, std::vector<RankAgent>& out, std::string& err);

// ---- Match store ----
std::string rankFormatMatchRow(const RankMatchRow& m);
bool rankParseMatchRow(const std::string& line, RankMatchRow& out);
// Load rows from a JSONL file, keeping only rows whose board matches `board`
// (empty = keep all). Malformed lines are counted in skipped, never fatal.
bool rankLoadMatches(const std::string& file, const std::string& board,
                     std::vector<RankMatchRow>& out, int& skipped);

// ---- Scheduler ----
// Pending games = per active pair, gamesPerPair color-balanced targets minus what
// the store already holds. Deterministic: agents sorted by ID, per-game seeds
// derived from (whiteId, blackId, pairOrdinal, runSeed), so any shard split or
// scheduling order reproduces identical games.
std::vector<RankPendingGame> rankSchedule(const std::vector<RankAgent>& roster,
                                          const std::vector<RankMatchRow>& store,
                                          int gamesPerPair, unsigned runSeed);

// ---- Rating ----
// Bradley-Terry MM fit over all rows, anchored at anchorId = Elo 0, with a
// 0.5-virtual-game prior per played pair. Deterministic and order-independent.
void rankFitBT(const std::vector<RankMatchRow>& rows, const std::string& anchorId,
               RankFit& out);
// 1-D MLE for one candidate against fixed opponent Elos (the gauntlet fit).
// score[i] is the candidate's score (0/0.5/1) in game i vs oppElo[i].
double rankFitSingle(const std::vector<double>& oppElo, const std::vector<double>& score,
                     double& seOut);

// ---- Subcommand entry points (return a process exit code) ----
int rankCheck(const std::string& rosterFile, const std::string& storeFile,
              int gamesPerPair, const std::string& board);
int rankPlay(const std::string& rosterFile, const std::string& storeFile,
             const std::string& outFile, int gamesPerPair, int shard, int ofK,
             unsigned runSeed, const std::string& board);
int rankRate(const std::string& rosterFile, const std::string& storeFile,
             const std::string& board);
int rankHistory(const std::string& storeFile, const std::string& agentQuery,
                int lastN, const std::string& board);
int rankGauntlet(const std::string& rosterFile, const std::string& storeFile,
                 const std::string& candidateId, int gamesPerOpp, bool keep,
                 unsigned runSeed, const std::string& board);
// Replay a deterministic sample of storeFile's historical matches (filtered to
// board), capturing labeled value-model training positions to outFile in the
// format ml_train.cpp's selfplay-supervised --from-data reads. sampleN <= 0
// means replay everything. featVer selects which feature layout to extract (1
// dense aggregates, 2 sparse piece-square). Reuses the existing rated agent
// pool as a training data source instead of a bespoke generator.
int rankExtract(const std::string& storeFile, const std::string& outFile,
                const std::string& board, int featVer, int sampleN, unsigned seed);

// ---- Pair-play training-data generation (pairgen) ----
// A dilution override applied on top of the two agents' own specs during
// generation, so games between deterministic agents still vary without
// changing either agent's identity. apply: 0 = none, 1 = agent A, 2 = agent B,
// 3 = both (rankPairGen maps A/B to colors per game; inside the game runner
// the same field is a color mask, 1 = White, 2 = Black). The override REPLACES
// the chosen side's randomMoveProb each half-move with a linear decay from
// `start` at ply 0 to `floorProb` at ply `decayPlies` (held at the floor
// after); decayPlies <= 0 means a constant `start`. Diluted moves are fully
// random (dilDepth is not touched).
struct RankDilOverride {
    int    apply;
    double start, floorProb;
    int    decayPlies;
    RankDilOverride() : apply(0), start(0.0), floorProb(0.0), decayPlies(0) {}
};
// The per-ply dilution probability under that schedule (pure, testable).
double rankDilutedProb(double start, double floorProb, int decayPlies, int ply);

// Play `games` fresh games between two roster-style canonical IDs, alternating
// colors (A is White in even games), capturing labeled value-model training
// positions to outFile in the same format rankExtract writes (plus a
// <outFile>.meta.json provenance sidecar). Per-game seeds derive from the IDs +
// ordinal + runSeed, so runs and shard splits reproduce identical games.
// openPlies: both sides play uniform-random moves for the first N half-moves
// (position spread for deterministic pairs). filterWinner: 0 = keep every
// game, 1 = keep only games agent A won, 2 = only agent B (draws dropped when
// filtering). branchTries: after each kept game agent A won, make N attempts
// to rewind to a random ply where A was to move, substitute a different legal
// move, play out clean, and keep the tail positions only if A wins again
// (mined alternative winning lines). Games with g % ofK != shard are skipped.
int rankPairGen(const std::string& idA, const std::string& idB, int games,
                const std::string& outFile, const std::string& board, int featVer,
                unsigned runSeed, const RankDilOverride& dil, int openPlies,
                int filterWinner, int branchTries, int shard, int ofK);
