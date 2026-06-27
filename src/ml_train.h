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
int trainSupervisedValue(const string& outDir, const string& boardFile, int games,
                         int epochs, double lr, int ckptEvery, int genDepth,
                         double genRandom, unsigned seed);

// Imitation policy (behavioral cloning): a teacher (AlphaBeta+Classic) plays both
// sides; its chosen move is the positive label; fit a linear move-rater.
int trainImitationPolicy(const string& outFile, const string& boardFile, int games,
                         int epochs, double lr, int teacherDepth, unsigned seed);

// Round-robin tournament of a default mixed roster (loads models/lin_value.txt and
// models/lin_policy.txt into slots if present); prints an Elo table, writes
// agents.jsonl + agents/library.txt + manifest rows.
int runTournament(const string& boardFile, int gamesPerPair, unsigned seed);

// Regenerate the auto-doc region of mlMdPath and models/registries.json from the
// live registries (evaluators, explorers, choosers, model types, regimes, features).
int exportDocs(const string& mlMdPath);

// ---- Lower-level building blocks (also used by tests) ----
// Rate an explicit roster in place (ratings recentered to mean 1500).
void rateAgents(std::vector<AgentSpec>& agents, std::vector<double>& ratingsOut,
                const string& boardFile, int gamesPerPair, unsigned seed);

// Write/refresh the manifest from a record list.
void writeManifest(const std::vector<ModelRecord>& records);
