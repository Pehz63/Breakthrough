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
//
// Model architecture: modelType = "linear" (default, the historical path) or
// "mlp" (a hand-written ReLU net; mlpHidden = hidden-layer widths, e.g. {32} or
// {32,16}, defaulting to {32} when empty). residualSkip adds a frozen chip-count
// skip connection (ResidualModel) around the chosen inner model: 0 = off (plain
// model, unchanged), > 0 = a fixed skip weight of that value, < 0 = auto-calibrate
// the skip from a material-only logistic pre-fit on the training data. The inner
// model then learns only the residual on top of material.
//
// Validation split: valSplit (0 = off, the historical all-data path) holds out that
// fraction of positions as a validation set (deterministic split by the run seed).
// SGD runs on the training remainder; each epoch prints the mean validation loss,
// and the final stratified-loss printout is computed on the held-out set (so the
// theory-24 equal-material measure is a generalization number, not in-sample). With
// earlyStop set, the saved model is the epoch with the lowest validation loss rather
// than the last epoch.
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
                         const string& fromDataFile = "",
                         const string& modelType = "linear",
                         const std::vector<int>& mlpHidden = {},
                         double residualSkip = 0.0,
                         double valSplit = 0.0,
                         bool earlyStop = false);

// Weight symmetrization + seed ensembling for LINEAR v2 value models: load K
// trained models, optionally project each onto its left-right mirror symmetry
// (w'[sq] = (w[sq] + w[mirror sq]) / 2 per color plane; the rules and the
// standard start are mirror symmetric, so the true value function is too and
// the anti-symmetric part is pure noise), then average the K weight vectors
// and biases into ONE model saved to outDir + ".txt". For a linear model the
// ensemble IS the weight average, so this stays fully incremental in search.
// Directly attacks the theory-8 training-seed noise band. Returns 0 on
// success; rejects non-linear or non-v2 inputs and mismatched shapes.
int trainEnsemble(const std::vector<string>& modelFiles, bool mirror,
                  const string& outDir);

// ---- Position-oracle distributional regime + evaluation ----
// Train a DistModel (mu + log-sigma heads) on a raw playout label store
// (rank.exe label): per row, features come from decoding the pool position's
// enc, and the players' Elo gap d plus rating-SE variance v are recomputed AT
// LOAD TIME from ratingsFile joined through the store meta's frozen ladder id
// array, so the same raw store retrains cleanly under any future ratings
// snapshot. Loss is the shared probitPoint BCE (DistModel::trainStepRow), so
// the trainer and the per-position label fit can never diverge. The
// validation split is by POSITION (hash % 1000 < valSplit * 1000), never by
// row, since every row of a position shares its latent advantage. Per epoch:
// train/val NLL and the mean predicted sigma in Elo. The final report adds
// calibration by |d| bucket, sigma stratified by |material diff|, and two
// fixed nulls fit on the train rows (gap-only and intercept-only) that the
// model must beat on held-out rows for the board to be adding anything.
// muType/sType are "linear" or "mlp" (mlpHidden-style width lists, default
// {32}). useEloSe folds rating standard errors into v. labelsFile switches
// to the secondary Gaussian mode: SE-weighted regression on fitted labels
// instead of raw outcomes. outPath is the model base name (final model at
// outPath.txt, checkpoints at outPath_ckptN.txt).
int trainDistValue(const string& outPath, const string& rawStore,
                   const string& poolFile, const string& ratingsFile,
                   int epochs, double lr, double lrSigma, double l2,
                   unsigned seed, const string& muType,
                   const std::vector<int>& muHidden, const string& sType,
                   const std::vector<int>& sHidden, double valSplit,
                   bool earlyStop, int ckptEvery, bool useEloSe,
                   const string& labelsFile = "");

// Score positions with a saved model and print them ranked by mean White
// advantage. posFile: a JSONL file with "enc" fields (pool or labels format)
// or bare 65-char enc lines. boardsCsv: comma-separated board .txt files
// scored with stm ('w' or 'b') to move. A DistModel prints mean +- SD in Elo
// plus P(win | equal opponents); any other value model prints its squashed
// eval as a fallback. Decided positions print a WIN marker.
int scoreBoards(const string& modelPath, const string& posFile,
                const string& boardsCsv, char stm);

// Evaluate a dist model against calibrated baselines on the held-out eval
// tier. Baselines: the oracle's root search score (depth oracleDepth at
// oracleBudget nodes, tt+ord, champion Classic weights), models/pst_value.txt,
// and the Classic static eval, each mapped to the Elo scale by a 1-parameter
// least-squares fit on a calibN-position calibration subsample of TRAIN-tier
// labels (never eval positions, scores winsorized at the 2nd/98th
// percentiles), each with one global sigma fit on the subsample's raw rows.
// Metrics on the eval tier: MAE + RMSE + Spearman vs measured mu (ok-flagged
// labels), per-row outcome NLL under each predictor's (mu, sigma), and SD
// validity (correlation of predicted sigma with measured sd). Prints the
// VERDICT line for the oracle claim: the model must beat the calibrated
// oracle baseline on BOTH outcome NLL and mu MAE.
int distEval(const string& modelPath, const string& labelsEval,
             const string& rawEval, const string& poolEval,
             const string& ratingsFile, const string& labelsTrain,
             const string& rawTrain, int calibN = 800, int oracleDepth = 8,
             unsigned long long oracleBudget = 2000000ULL);

// Measure an MLP value head's first-hidden-layer sparsity and per-move activation
// churn, to quantify how much a future second-accumulated-layer (dead-ReLU delta)
// optimization could save. Loads modelPath (a DistModel's mu head, or a bare MLP
// value head), samples up to maxPositions from poolFile (JSONL "enc" positions),
// and reports: (a) static dead-ReLU fraction (units with relu(z)==0 per position),
// and (b) per-move activation churn -- the mean number of first-hidden units whose
// activation changes across a legal move, with side-to-move HELD FIXED so only the
// board move's effect is measured (side-to-move is a separate, cheaply-handled
// perturbation; the shipped first-layer accumulator already applies it at read
// time). The second-layer delta's speedup ceiling is H / churn. Returns 0 on success.
int mlpSparsity(const string& modelPath, const string& poolFile, int maxPositions);

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
