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
// ID grammar (canonical form; parse rejects anything else and prints the fix):
//   id      := head { "." segment } "." version
//   head    := "rand" | "tiered" | "smart(" N ")" | "policy"      (policy brains)
//            | "greedy" | "ab(" "d" N { "," flag } ")"            (search brains)
//   flag    := "noab" | "tt" | "ord" | "part" | "asp" N
//            | "nb" budget | "tb" N "ms" | "cap" N                (budget: 200k, 2m, raw)
//   segment := evalseg | dilseg
//   evalseg := "classic(" weights ")" | "exp(" weights ")"        (search brains only)
//            | "learned(s" slot "," hash8 ")"                     (LearnedValue)
//            | "linpol(s" slot "," hash8 ")"                      (policy-head model)
//   weights := letter int { "," letter int }                      (ALL params, registry order)
//   dilseg  := "dil(r" pct ")"                                    (r5 = 5% random moves)
//   version := "v" N                                              (required, always last)
// Examples: rand.v1  smart(4).v1  ab(d6).classic(t2,c10,w3,l2).v1
//           ab(d8,tt,ord,nb200k).exp(t2,c10,w3,l2,f2).dil(r5).v1
//           greedy.learned(s0,ab12cd34).v1  policy.linpol(s1,9f3e21aa).v1

// A roster entry: the engine-playable spec plus the identity fields that
// AgentSpec cannot hold (the full ID string and the user-bumped version).
struct RankAgent {
    AgentSpec   spec;
    std::string id;       // canonical ID, the permanent match-history key
    int         version;  // the trailing v<N> (identity salt, bumped by the user)
    bool        active;   // "on" or "anchor" in the roster
    bool        anchor;   // the Elo-0 reference agent (exactly one per roster)
};

// One played game in ranking/matches.jsonl.
struct RankMatchRow {
    std::string w, b;          // white / black agent IDs
    char        r;             // 'W', 'B', or 'D' (400-half-move cap)
    int         plies;         // half-moves played
    double      wms, bms;      // per-side total move time (ms)
    int         wmv, bmv;      // per-side move counts
    double      wnod, bnod;    // per-side total search nodes (0 for policy brains)
    unsigned    seed;          // the srand seed this game was played with
    std::string board;         // starting board file
    int         par;           // shard count the game ran under (1 = clean timing)
    std::string ts, run;       // UTC write time, UTC run stamp
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
// Emit the canonical ID for a spec + version (hashes the model file for learned
// agents). Parse validates, builds the spec, and enforces canonical form; on
// failure returns false with a human-readable reason in err.
std::string rankAgentId(const AgentSpec& spec, int version);
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
