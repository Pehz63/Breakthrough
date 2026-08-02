#pragma once
#include "agents.h"
#include <string>
#include <vector>
#include <map>
#include <set>
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
//   flag    := "noab" | "tt" | "ord" | "qs" | "part" | "asp" N
//            | "nb" budget | "tb" N "ms" | "cap" N                (budget: 200k, 2m, raw)
//   evalseg := ( "classic(" weights ")" | "exp(" weights ")"      (search brains only)
//              | "learned(s" slot "," hash8 [ "," arch ] ")" ) "@" V   (LearnedValue)
//   arch    = regime "," mutype "," shape [ ",sig" shape ] ",con" pct
//   regime  = HOW THE MODEL WAS PRODUCED, read from its file's `teacher=` line:
//             "tdleaf_self"   TD-Leaf(lambda) on self-play games
//             "pool_games"    outcomes from games replayed out of the ranked pool
//             "teacher_games" outcomes from a fixed heuristic teacher's self-play
//             "model_games"   outcomes from a previously-trained model's self-play
//             "position_elo"  per-position Elo labels (position-oracle pipeline)
//             "weight_merge"  weight averaging and/or mirror symmetrisation
//             "unknown"       provenance lost with the model file
//             "value"|"dist"  SUPERSEDED model-type tokens: parsed, never emitted
//   mutype  = "mlp" | "lin"      shape = dash-separated layer widths, e.g. 129-512-8-1
//   con<N>  = percent connectivity, currently always con100 (reserved for sparsity)
//   The arch fields are DESCRIPTIVE: identity is still (slot, hash), and they are
//   derived from the file that hash covers. The legacy two-arg form is still accepted
//   and expands to the rich form on parse, so pre-existing match rows keep matching.
//   Regime replaced model-type on 2026-08-01: two agents can share an architecture
//   and be nothing alike (a TD-Leaf model and a pool-replay model are both
//   value,lin,129-1), so the id names the pipeline instead. It lives in the id
//   rather than only in the model file because the id is stamped into every
//   append-only store row and outlives the file -- slot9.txt was overwritten on
//   2026-07-30 and its teacher= line went with it, making that agent's regime
//   unrecoverable. canonicalizeLearnedIds() re-derives stale descriptors on read,
//   so stored rows written under the old tokens keep matching.
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
//           greedy@1.learned(s0,ab12cd34,value,lin,30-1,con100)@1
//           ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974,dist,mlp,129-512-8-1,sig129-64-1,con100)@1
//           policy@1.linpol(s1,9f3e21aa)

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
    std::vector<char>        pinned;       // 1 = rating was held FIXED (rankFitBTPinned only)
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
//
// The store is SHARDED (see rankStoreShardPath). Sealed shards are read first,
// in index order, then `file` itself as the live tail, so the row order is the
// same append order a single unsharded file would have given.
bool rankLoadMatches(const std::string& file, const std::string& board,
                     std::vector<RankMatchRow>& out, int& skipped);

// ---- Match store sharding ----
// The store is append-only and grows without bound, which one file cannot carry:
// a host that caps file size (GitHub rejects blobs over 100 MB) stops accepting
// it, and every commit re-stores the whole file because a growing file deltas
// badly. So the store is a chain of fixed-size SEALED shards plus a live tail:
//
//   ranking/matches.0001.jsonl   sealed, immutable, never appended to again
//   ranking/matches.0002.jsonl   sealed
//   ranking/matches.jsonl        the live tail: every write still appends HERE
//
// Only the tail changes, so each sealed shard is stored exactly once, forever.
// Shard indices are contiguous from 1, which is what lets the loader find them
// by probing successive names instead of enumerating a directory.
//
// Writers are unaffected: `play` and the run_rank.ps1 shard merge keep appending
// to the tail, and sealing is a separate deliberate step.
std::string rankStoreShardPath(const std::string& storeFile, int index);

// Split the live tail into sealed shards of at most maxBytes each, leaving the
// remainder as the new tail. No-op (returns 0) when the tail is already under
// maxBytes. Verifies that the rewritten chain holds exactly the line count the
// tail had before touching the original, and aborts leaving the tail untouched
// if it does not, because this store is never regenerated.
// Returns the number of shards created, or -1 on error.
int rankSealStore(const std::string& storeFile, long long maxBytes, std::string& err);

// ---- Splitting the store by who played the game ----
// A screening study leaves behind agents that were never promoted, and their
// games outlive them: the store holds every game ever played, so a sweep that
// tried 25 candidates and kept 16 keeps carrying the other nine forever.
//
// Splitting groups rows into parts by WHO played them, so a group can later be
// dropped (or kept out of a clone) by deleting one file, without any row being
// destroyed now. Rows are classified by their two agents:
//
//   roster          both agents are in the roster
//   retired_<tag>   otherwise, and some agent's id contains the group substring
//   retired_other   otherwise
//
// The tagged bucket wins over retired_other when a row touches both, so a row
// appears in exactly one part and the parts sum to the original store.
struct RankStoreBucket {
    std::string              name;
    long long                rows = 0;
    long long                bytes = 0;
    std::vector<std::string> parts;   // files written, in load order
};

struct RankSplitStats {
    std::vector<RankStoreBucket>     buckets;
    long long                        malformed = 0;    // unparseable rows, kept with the roster part
    long long                        rosterAgents = 0;
    std::map<std::string, long long> retired;          // retired id -> rows it appears in
};

// Rewrite the store as grouped parts plus an index listing them, capping every
// part at maxBytes so no single file can outgrow what a host will accept.
// Nothing is deleted: every row lands in exactly one part, and the parts are all
// still loaded, so ratings are unchanged until a part is deliberately removed.
// With apply=false this only measures and writes nothing.
// Returns 0 on success, -1 on error.
int rankSplitStore(const std::string& storeFile, const std::string& rosterFile,
                   const std::string& groupMatch, long long maxBytes, bool apply,
                   RankSplitStats& out, std::string& err);

// ---- Regimes and matchups ----
// A REGIME is how an agent was produced, read off its canonical id: the third
// field of `learned(slot,hash,<regime>,...)` for learned agents, and the
// evaluator name for heuristic ones. It is already stamped into every stored
// match row, so no extra labelling is needed to group by it.
//
// Regimes exist because Elo cannot represent a non-transitive matchup. A
// Bradley-Terry fit assigns ONE strength per agent and therefore assumes that
// if A beats B and B beats C, A beats C by the implied margin. Real agent
// populations violate this: an agent trained by self-play from some champion
// learns to beat THAT champion's style, so it punishes agents resembling its
// ancestor harder than it punishes unrelated ones. Measured here 2026-08-01:
// the TD-Leaf self-play cohort beat `learned-other` agents 70.5% but the
// chip counter only 63.6% -- so the chip counter's rating rose relative to
// other learned agents purely by being in a pool that contained that cohort,
// and fell 33 places when the cohort left. That is a modelling failure, not
// sampling noise, and no number of games fixes it.
std::string rankAgentRegime(const std::string& id);

// One regime-vs-regime cell. `a` and `b` are regime labels with a <= b, and the
// tallies are from A's perspective with both colours combined, so the White
// advantage cancels instead of showing up as a matchup effect.
struct RankMatchupCell {
    std::string a, b;
    double      games = 0;
    double      score = 0;      // A's score: 1 per win, 0.5 per draw
    double      expected = 0;   // A's Elo-expected score, summed per game
};

// Aggregate rows into regime-vs-regime cells, using `fit` for the expected
// score. residual = score/games - expected/games is the quantity of interest:
// it is how far the one-number model is off for that pairing, so a large
// residual marks a matchup Elo is actively misreporting.
void rankMatchupByRegime(const std::vector<RankMatchRow>& rows, const RankFit& fit,
                         std::vector<RankMatchupCell>& out);

// ---- Store part index ----
// "ranking/matches.jsonl" -> "ranking/matches.index.txt". One part filename per
// line (relative to the store's directory), in load order, '#' comments allowed.
// Hand-editable on purpose: dropping a group of games is deleting its line.
std::string rankStoreIndexPath(const std::string& storeFile);

// Every file holding rows, in load order. Uses the index when one exists, else
// falls back to probing the contiguous sealed-shard chain. The live tail is
// always last.
void rankStoreParts(const std::string& storeFile, std::vector<std::string>& out);

// ---- Scheduler ----
// Pending games = per active pair, gamesPerPair color-balanced targets minus what
// the store already holds. Deterministic: agents sorted by ID, per-game seeds
// derived from (whiteId, blackId, pairOrdinal, runSeed), so any shard split or
// scheduling order reproduces identical games.
// pairedOpenings: give both games of a colour-swapped couple ONE seed (derived from
// the canonically ordered pair), so agents wearing the same `.opener(rand,K)` play
// the SAME random opening with colours reversed. Compares recovery from equal
// ground instead of opening luck. Inert when neither side consumes rand().
// cohort (optional): when non-null, schedule ONLY pairs with at least one member
// in this set. Lets a new cohort be played into an existing roster without also
// topping up every roster-vs-roster pair to gamesPerPair -- which at 32/pair on
// the current store would be ~702,000 games unrelated to the cohort.
std::vector<RankPendingGame> rankSchedule(const std::vector<RankAgent>& roster,
                                          const std::vector<RankMatchRow>& store,
                                          int gamesPerPair, unsigned runSeed,
                                          bool pairedOpenings = false,
                                          const std::set<std::string>* cohort = nullptr);

// ---- Rating ----
// Bradley-Terry MM fit over all rows, anchored at anchorId = Elo 0, with a
// 0.5-virtual-game prior per played pair. Deterministic and order-independent.
// regimeBalanced weights each pair by 1/(agents in A's regime * agents in B's
// regime), rescaled to the original total game count, so every regime bloc
// contributes equally however many agents wear it. Off by default because it
// changes every rating: use it when the question is "how strong is this agent
// against the space of strategies" rather than "against this particular pool".
// Its SEs are effective-sample SEs, not game counts.
void rankFitBT(const std::vector<RankMatchRow>& rows, const std::string& anchorId,
               RankFit& out, bool regimeBalanced = false);
// Same MM fit with a SUBSET of ratings HELD FIXED (`pinned`: id -> frozen Elo).
// Rates a cohort of new agents on the existing roster's scale without letting
// them move it, so the previous fit's numbers stay comparable for the whole of a
// study. Uses cohort-vs-cohort games as well as cohort-vs-roster, so it resolves
// the cohort's internal ordering -- which N separate gauntlets cannot do.
// SCREENING ONLY: the champions' ratings are inputs here, so a pinned fit can
// never dethrone one. Certification stays the full unpinned refit
// (ranking/CHAMPION.md rule 1), run deliberately as a study's last step.
void rankFitBTPinned(const std::vector<RankMatchRow>& rows,
                     const std::map<std::string,double>& pinned,
                     RankFit& out);
// 1-D MLE for one candidate against fixed opponent Elos (the gauntlet fit).
// score[i] is the candidate's score (0/0.5/1) in game i vs oppElo[i].
double rankFitSingle(const std::vector<double>& oppElo, const std::vector<double>& score,
                     double& seOut);

// ---- Subcommand entry points (return a process exit code) ----
int rankCheck(const std::string& rosterFile, const std::string& storeFile,
              int gamesPerPair, const std::string& board);
// cohortFile (optional): path to a plain list of agent ids. When given, only
// pairs touching one of those agents are scheduled (see rankSchedule's `cohort`).
int rankPlay(const std::string& rosterFile, const std::string& storeFile,
             const std::string& outFile, int gamesPerPair, int shard, int ofK,
             unsigned runSeed, const std::string& board, bool pairedOpenings = false,
             const std::string& cohortFile = "");
// pinFile (optional): a ratings.tsv / standings.tsv whose agents are held at
// their listed Elo (rankFitBTPinned) instead of being re-solved. Screening only.
// regimeBalanced: run the fit with every regime bloc weighted equally (see
// rankFitBT). Writes its own ranking/*_balanced.* family, never the canonical
// files, because it answers a different question than the certified table.
int rankRate(const std::string& rosterFile, const std::string& storeFile,
             const std::string& board, const std::string& pinFile = "",
             bool regimeBalanced = false);
int rankHistory(const std::string& storeFile, const std::string& agentQuery,
                int lastN, const std::string& board);
// Print the regime-vs-regime matchup matrix (see rankMatchupByRegime). Reports
// actual score, Elo-expected score, and the residual between them, plus a
// per-regime roster census, since a residual only biases the table to the
// extent that regime is over-represented in the pool. minGames drops cells too
// thin to read.
int rankMatchup(const std::string& rosterFile, const std::string& storeFile,
                const std::string& board, int minGames);
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

// ---- Opening/refutation book generation (bookgen) ----
// Mine a book from stored games between two agents: replay every stored match
// between idA and idB (either color assignment), and at each ply where A was
// to move within the first maxPlies half-moves, record the canonical position
// key -> the move A played. Games whose replayed result mismatches the stored
// result are skipped whole (the extract determinism-drift guard). First-seen
// wins on key conflicts; conflicting entries are counted and reported (they
// mark cross-game search-state variance in the source games, theory 19b).
// Output: a text book file the "book" opener (src/ai_random.cpp) reads --
// '#' provenance header, then one "<hash hex16> <sx> <sy> <dx>" line per
// position. Write it to models/book<N>.txt and roster the follower as
// ".opener(book,<N>)@1".
int rankBookGen(const std::string& storeFile, const std::string& board,
                const std::string& idA, const std::string& idB,
                int maxPlies, const std::string& outFile);

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
// <outFile>.meta.json provenance sidecar, which also records A's color-split
// record over ALL played games -- a_white_games/a_white_wins/a_white_draws and
// the _black_ equivalents -- so a color asymmetry doesn't hide inside an
// aggregate a_wins/b_wins tally; B's per-color record is the complement).
// Per-game seeds derive from the IDs + ordinal + runSeed, so runs and shard
// splits reproduce identical games.
// openPlies: play uniform-random moves for the first N half-moves (position
// spread for deterministic pairs). openSide masks WHICH side plays those random
// openers, mapped A/B -> color per game exactly like dil.apply: 1 = agent A,
// 2 = agent B, 3 = both (the default, back-compat symmetric opener), 0 = off.
// The unmasked side consults its own brain even inside the opener window, so an
// asymmetric opener handicaps only one agent (used to test whether the symmetric
// opener inflates head-to-head results). filterWinner: 0 = keep every game,
// 1 = keep only games agent A won, 2 = only agent B (draws dropped when
// filtering). branchTries: after each kept game agent A won, make N attempts
// to rewind to a random ply where A was to move, substitute a different legal
// move, play out clean, and keep the tail positions only if A wins again
// (mined alternative winning lines). Games with g % ofK != shard are skipped.
int rankPairGen(const std::string& idA, const std::string& idB, int games,
                const std::string& outFile, const std::string& board, int featVer,
                unsigned runSeed, const RankDilOverride& dil, int openPlies,
                int filterWinner, int branchTries, int shard, int ofK,
                int openSide = 3);

// Mechanism measurement for the opener-bias study: for `games` seeded games
// between agent A (the deterministic champion) and agent B, replay the first
// openPlies half-moves and, at each ply where A is to move, compare the position
// A reaches under a forced random opener move against the position A reaches
// under its OWN chosen move. Both resulting positions are scored with the
// opponent to move (so the turn term cancels) by the JUDGE agent's search
// (judgeId, empty = A itself). delta = judge-value(own) - judge-value(random),
// A-relative; a positive delta means the random opener left A objectively worse
// off than it would have chosen. A judge with real positional weights (e.g. a
// learned PST) discriminates opener positions that A's own coarse eval cannot.
// Tabulates mean delta and the fraction of A-plies/games past a materiality
// threshold, split by A's color. Read-only (writes no data files).
int rankOpenerBias(const std::string& idA, const std::string& idB, int games,
                   const std::string& board, int openPlies, unsigned runSeed,
                   const std::string& judgeId = "");

// Color-swap recovery test: for `games` seeded random-opener snapshots (both
// sides play openPlies uniform-random half-moves from the start board, neither
// agent's brain involved yet), play the SAME snapshot out to conclusion TWICE --
// once with A as White / B as Black, once with the assignment swapped -- and
// classify the pair of outcomes into one of four buckets: White won both
// (the position favors whoever is White, a color/position effect, not agent
// skill), Black won both (same, favoring Black), A won both (A recovers from
// this exact position better than B, regardless of color), or B won both. A
// draw in either continuation makes that snapshot inconclusive (tallied
// separately, not classified). This isolates "does one agent recover from a
// bad position better" from "does this random position simply favor a color,"
// which a single-continuation measurement (rankOpenerBias) cannot distinguish.
int rankOpenerSwap(const std::string& idA, const std::string& idB, int games,
                   const std::string& board, int openPlies, unsigned runSeed);

// ---- Position-oracle label pipeline (posgen / label / labelfit) ----
// Measures per-position Elo advantage empirically: posgen builds a pool of
// distinct positions from stored games, label plays DESIGNED fresh games from
// each position between rated ladder agents at controlled Elo gaps, and
// labelfit turns the raw outcomes into per-position (mu, sigma) labels via the
// shared probitPoint math (ml_model.h). Raw rows are keyed by rung indices
// whose id list is frozen in the store's .meta.json sidecar, so labels can be
// re-fit under any future ratings snapshot without replaying a single game.

// Build a position pool: replay a deterministic sample of storeFile's matches
// (the extract replay loop + determinism-drift guard) and emit DISTINCT
// positions as JSONL rows {"enc","h","ply","stm","md","seen"}, deduped by enc,
// stratified by ply band ({6-10, 11-16, 17-24, 25-34, 35-44}, quota = target/5
// each) and |material diff| bucket ({0,1,2,>=3}, cap = bandQuota/3), skipping
// decided (nearWinCheck) positions and plies outside [minPly, maxPly], with at
// most perGameCap positions kept per game. Positions whose hash % 17 == 0 go
// to outEval, the rest to outTrain (hash-disjoint tiers by construction). If
// an out file already exists its encs pre-seed the dedup set and new rows are
// APPENDED (targets count new rows), so a second pass over another store
// merges cleanly.
int rankPosGen(const std::string& storeFile, const std::string& board,
               const std::string& outTrain, const std::string& outEval,
               int targetTrain, int targetEval, int perGameCap,
               int minPly, int maxPly, unsigned seed);

// A labeling-ladder spec (hand-edited text): "rung <i> <canonical id>" lines
// declare the ladder agents (indices sequential from 0, ids copied verbatim
// from ratings.tsv), and "pair <wi> <bi> <games> [mod <k> <r>]" lines declare
// the per-position design: play <games> games with rung wi as White and rung
// bi as Black; the optional mod clause restricts the pairing to positions with
// hash % k == r (the premium-rung ration knob). '#' starts a comment.
struct LabelRung    { std::string id; };
struct LabelPairing { int wi, bi, games, modK, modR; };
bool rankLoadLadder(std::istream& in, std::vector<LabelRung>& rungs,
                    std::vector<LabelPairing>& pairs, std::string& err);

// Play the ladder design over a position pool, appending raw outcome rows
// {"h","wi","bi","g","seed","y","p"} to outFile plus a <outFile>.meta.json
// sidecar whose "ladder" id array is the AUTHORITATIVE rung-index mapping for
// that store (the hand-edited ladder file may change later; the meta copy may
// not, and a resumed run hard-errors on a mismatch). Positions are sharded by
// pool index (idx % ofK == shard) so each position's whole seed stream lives
// in one shard; per-game seeds fold in the ids, pairing ordinal, and position
// hash, so any worker count reproduces identical games. Without --resume an
// existing outFile is truncated; with resume, rows already in outFile (and in
// doneFile, the merged master, if given) are counted per (h, wi, bi) and only
// the missing games are played (exact top-up). maxPositions > 0 caps how many
// positions may have NEW games played this invocation (chunked multi-day
// runs). Validation: every rung id must parse and its model slots load; rungs
// with an identity opener are rejected (opener ply counters assume games start
// at ply 0); pairings between two deterministic rungs are rejected (they
// replay one game). Draws (400-half cap) are dropped and tallied in the meta.
int rankLabel(const std::string& poolFile, const std::string& ladderFile,
              const std::string& outFile, unsigned runSeed, int shard, int ofK,
              bool resume, const std::string& doneFile, int maxPositions);

// 2-parameter probit MLE for ONE position's raw outcomes: d[i]/v[i] in logit
// units, y[i] in {0,1}. Fits (mu, s = log sigma) by adaptive gradient descent
// with s projected into [PROBIT_S_MIN, PROBIT_S_MAX] (mu into [-10, 10]),
// then standard errors from the numeric observed Fisher information (99.0
// sentinels when the information matrix is degenerate, e.g. unanimous
// outcomes). Returns iterations used.
int rankFitMuSigma(const std::vector<double>& d, const std::vector<double>& v,
                   const std::vector<double>& y, double& mu, double& s,
                   double& seMu, double& seS);

// Join a raw label store to a ratings snapshot and fit every position: emits
// labels JSONL {"enc","h","mu_elo","sd_elo","se_mu","se_sd","n","nll","flags"}
// (flags: ok | allwin | allloss | clamped | thin) in pool-file order, plus a
// per-pairing QC table (games, empirical White-win rate, mean fitted p) and a
// pooled intercept-only baseline NLL. Rung Elos come from the store meta's
// ladder id array joined to ratingsFile (elo + pm columns, id last); with
// useRatingSe, v = (pmW^2 + pmB^2) in logit^2 rides into every fit. This is
// also the free re-fit path after a future ratings refit: same raw rows, new
// ratingsFile. Positions with fewer than minRows raw rows are skipped (and
// any fit under 32 rows is tagged thin).
int rankLabelFit(const std::string& storeFile, const std::string& poolFile,
                 const std::string& ratingsFile, const std::string& outFile,
                 int minRows, bool useRatingSe);
