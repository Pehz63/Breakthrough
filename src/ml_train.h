#pragma once
#include "globals.h"
#include "agents.h"
#include <vector>

// ============================================================
// ML training, Elo, tournaments, checkpoints, doc export
// ============================================================
// Reusable trainer logic shared by tools/train_main.cpp and tests/test_ml.cpp.
// All regimes write open data files (data/*.jsonl) and human-readable rollups
// (models/manifest.{json,md}, agents/library.txt). Heavy model types train in
// Python and export into the same C++ model-file format; the regimes here cover
// the Phase-1 linear value + policy models entirely in C++.

// ---- Elo ----
double eloExpected(double ra, double rb);
void   eloUpdate(double& ra, double& rb, double scoreA, double k);

// ---- Training-regime registry (drives docs + the CLI subcommands) ----
struct RegimeDef { const char* name; const char* desc; };
extern const RegimeDef g_regimes[];
extern const int       g_regimeCount;

// ---- Manifest record (one per saved checkpoint) ----
struct ModelRecord {
    string id, type, head, regime, conditions, path;
    int    epoch, games;
    double loss, winrate, elo;
};

// ---- Regimes (return 0 on success) ----
// Supervised value model: generate self-play games with a fixed generator, label
// positions by outcome, fit a linear value model, checkpoint + manifest each epoch.
// The generator/teacher evaluator is selectable (genEval = "Classic" or
// "Experimental") with optional weight overrides (genParams; empty = registry
// defaults). The chosen teacher spec is written into the model file (`teacher=`),
// the manifest conditions, and data/models.jsonl so the model self-documents its lineage.
// featVer selects the value feature set: 1 = dense aggregates (MLV_FEATURES),
// 2 = sparse piece-square (MLV2_FEATURES, the layout the incremental g_mlAcc
// search accumulator requires). l2 (0 = off) applies weight decay in the SGD step.
//
// Dilution decay: the teacher's random-move probability starts at genRandom and
// linearly decays to genRandomFloor over genRandomDecayPlies half-moves (0 =
// off, the historical constant-probability behavior), then holds at the floor.
// Rationale: early exploration for position diversity, but late-game moves (the
// ones closest to, and most informative about, the eventual outcome) are played
// for real rather than continuing to be randomized.
//
// Self-play bootstrap: if genModelPath is non-empty, the teacher IS a
// previously-trained value model wrapped in genModelExplorer ("alphabeta" at
// depth genDepth, or "greedy"), loaded into a reserved scratch slot
// (ML_SLOTS-2), instead of the fixed Classic/Experimental heuristic
// (genEval/genParams are then ignored). Lets one generation's output become the
// next generation's generator.
//
// Data source: if fromDataFile is non-empty, self-play generation is skipped
// entirely and (games/epochs-of-generation/genDepth/genRandom/genModelPath/...)
// are ignored; positions + labels are read from that file instead (see
// rank.exe's "extract" subcommand, which replays sampled historical matches from
// ranking/matches.jsonl -- the existing diverse, already-Elo-differentiated
// agent pool -- deterministically by seed and emits this format). The file's
// declared feature version must match featVer.
int trainSupervisedValue(const string& outDir, const string& boardFile, int games,
                         int epochs, double lr, int ckptEvery, int genDepth,
                         double genRandom, unsigned seed,
                         const string& genEval = "Classic",
                         const std::vector<int>& genParams = {},
                         int featVer = 1,
                         double l2 = 0.0,
                         double genRandomFloor = 0.0,
                         int genRandomDecayPlies = 0,
                         const string& genModelPath = "",
                         const string& genModelExplorer = "alphabeta",
                         const string& fromDataFile = "");

// Imitation policy (behavioral cloning): a teacher (AlphaBeta + selectable evaluator)
// plays both sides; its chosen move is the positive label; fit a linear move-rater.
int trainImitationPolicy(const string& outFile, const string& boardFile, int games,
                         int epochs, double lr, int teacherDepth, unsigned seed,
                         const string& teacherEval = "Classic",
                         const std::vector<int>& teacherParams = {});

// Round-robin tournament of the default depth-laddered roster, single process
// (convenience wrapper: play shard 0/1 then rate). Prints an Elo table.
int runTournament(const string& boardFile, int gamesPerPair, unsigned seed);

// Calibrate the turn-advantage weight: over self-play-sampled positions, measure the
// 1-ply white-centric eval swing between "White to move" and "Black to move" (with the
// turn term zeroed) and report its mean/stddev plus a recommended turn weight.
int turnSwing(const string& boardFile, int games, int depth, unsigned seed,
              int chipW = 4, int wallW = 2, int colW = 2, int fwdW = 0);

// Precise per-move speed of the learned model vs AlphaBeta evaluator variants
// (chip / chip+forward / chip+structures / chip+struct+forward) across a depth ladder.
// Also runs the heuristic eval-level ladder (g_evalLevel 1/2/3: full chip rescan /
// incremental chip + full structure scan / fully incremental) at nonzero structure
// weights, with fixed levelReps timed reps per position (levelWarmup discarded warmup
// passes), interleaved per position so drift hits all levels equally, and an
// equivalence self-check (same end board + node count across levels) that must PASS
// before the ladder numbers are trustworthy.
int speedBench(const string& boardFile, int positions, double msPerAgent, unsigned seed, int maxDepth = 6,
               int levelReps = 8, int levelWarmup = 1);

// ---- Depth-laddered, process-shardable tournament ----
// Build the deterministic roster (heuristics + SmartRandom variants + LearnedPolicy,
// plus Greedy and AlphaBeta over a table of evaluator presets at each depth). hasValue/
// hasPolicy gate the learned agents so play and rate build an identical roster.
std::vector<AgentSpec> buildTournamentRoster(const std::vector<int>& depths,
                                             bool hasValue, bool hasPolicy,
                                             const std::vector<unsigned long long>& budgets = {},
                                             bool ablate = false, bool forwardStudy = false);

// Play this shard's share of the full round-robin (game index % ofK == shard),
// appending result rows {a,b,sa} and per-agent timing rows {timing,name,ms_total,
// moves,ms_max} to outFile. nodeBudget caps per-move search nodes (0 = unlimited).
// `only` is an optional agent-name allowlist: if non-empty the roster is reduced to
// just those names (play and rate must pass the SAME list so the rosters match).
int tournamentPlay(const string& boardFile, const std::vector<int>& depths,
                   int gamesPerPair, unsigned seed, int shard, int ofK,
                   unsigned long long nodeBudget, const string& outFile,
                   const std::vector<std::string>& only,
                   double timeBudgetMs = 0.0,
                   const std::vector<unsigned long long>& budgets = {},
                   bool ablate = false, bool forwardStudy = false);

// Read all result + timing rows from inFile, fit Elo, print the table
// (Elo | ms/move | max ms | games | agent), append data/agents.jsonl, and (only on a
// FULL run, i.e. `only` empty) write agents/library.txt + the champion files. `only`
// reduces the roster identically to tournamentPlay. When `runId` is non-empty the run
// is archived under runs/<runId>/ (elo.tsv + results.jsonl), the per-agent rows are
// appended to agents/registry.jsonl, agents/registry.md is regenerated, and a summary
// line is appended to runs/index.jsonl. `note` is unused here (the pre-run note lives
// in config.json / notes.md, written by writeRunConfig).
int tournamentRate(const std::vector<int>& depths, const string& inFile,
                   const std::vector<std::string>& only,
                   const string& runId, const string& note,
                   const std::vector<unsigned long long>& budgets = {},
                   bool ablate = false, bool forwardStudy = false);

// ---- Run archive (timestamped, append-only history of tournament runs) ----
// Mint a UTC run id of the form yyyyMMddTHHmmssZ (matches the PowerShell driver).
string makeRunId();

// Create runs/<runId>/ and write config.json (the exact run configuration) plus the
// notes.md header with the pre-run note. Called once per run by the driver (via the
// run-config subcommand) and by the single-process runTournament wrapper.
void writeRunConfig(const string& runId, const std::vector<int>& depths,
                    unsigned long long nodeBudget, int gamesPerPair, unsigned seed,
                    int workers, const string& board,
                    const std::vector<std::string>& only, const string& note);

// Append a timestamped section to runs/<runId>/notes.md. For attaching a realization
// after the fact (e.g. "CPU was throttled, ignore ms/move"). No recompute.
int runNote(const string& runId, const string& note);

// Regenerate the auto-doc region of mlMdPath and models/registries.json from the
// live registries (evaluators, explorers, choosers, model types, regimes, features).
int exportDocs(const string& mlMdPath);

// Write/refresh the manifest from a record list.
void writeManifest(const std::vector<ModelRecord>& records);
