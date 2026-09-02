#pragma once
#include "globals.h"
#include <string>
#include <vector>

// ============================================================
// TRAINING-COMPUTE METER AND WALL-CLOCK RUNG LADDER
// ============================================================
// One meter shared by every regime entry point (tdleaf, gumbelzero,
// selfplay-supervised, dist-value, and the hill climber's driver script), so a
// rung ladder means the same thing in all of them and a checkpoint's cost can
// be read off the file it was written into.
//
// Three things live here, and they exist for three separate reasons.
//
// 1. WALL CLOCK is the normalization unit. Regimes are compared at equal
//    training spend, so the run needs marks it can checkpoint at and a hard
//    stop it cannot run past. `wallCkptAt` holds the ascending second marks
//    (2h / 4h / 8h = 7200,14400,28800) and `wallStopSec` the stop.
//
// 2. NODES make the number portable. Wall clock measures this machine under
//    this much contention; a node count does not. `nodes` accumulates
//    g_lastNodes over every search the run performs, fed by agentChooseMove's
//    g_trainNodesTotal counter.
//
// 3. UNITS are what the regime itself counts (games for the online regimes,
//    iterations for the climber). Recorded so a checkpoint says what it
//    actually did, not what its command line asked for.
//
// All three are written into a checkpoint's provenance line by tbStamp, and
// read back out of one by tbParsePrior, which is what makes --resume work: a
// resumed run continues the SAME cumulative ladder rather than restarting it,
// so an 8h checkpoint extended to 16h is stamped 16h and not 8h.
//
// The provenance stamp is deliberately written per SAVE rather than once at
// the top of a run. Writing it up front from the requested configuration is
// how every TD-Leaf checkpoint from one run came to claim the same game count
// regardless of which rung it was (see Docs/corrections.md and
// plans/budget-parity-plan-1-steady-meridian.md P5).

struct TrainBudget {
    // ---- configuration ----
    std::vector<double> wallCkptAt;   // cumulative second marks, ascending; empty = no wall ladder
    double wallStopSec;               // cumulative hard stop in seconds; 0 = no wall-clock stop

    // ---- carried in from a --resume checkpoint (0 for a fresh run) ----
    double             priorSec;
    unsigned long long priorNodes;
    long long          priorUnits;

    // ---- live state ----
    double             startClock;    // steady-clock seconds latched by tbBegin
    unsigned long long baseNodes;     // g_trainNodesTotal at tbBegin
    long long          units;         // units completed by THIS process
    size_t             nextMark;      // index into wallCkptAt of the next unwritten mark
    bool               running;

    TrainBudget()
        : wallStopSec(0.0), priorSec(0.0), priorNodes(0), priorUnits(0),
          startClock(0.0), baseNodes(0), units(0), nextMark(0), running(false) {}
};

// Fill the defaults (everything off), so a caller that never sets a wall flag
// gets a meter that only measures and never stops anything.
TrainBudget tbDefaults();

// Parse "7200,14400,28800" into ascending, de-duplicated seconds. Returns false
// (leaving `out` untouched) on a malformed or non-positive entry.
bool tbParseMarks(const std::string& csv, std::vector<double>& out, std::string& err);

// Latch the start clock and the node baseline. Call once, immediately before
// the regime's own loop, AFTER any setup that should not count as training.
void tbBegin(TrainBudget& b);

// Cumulative seconds: this process's elapsed time plus any resumed prior.
double tbElapsed(const TrainBudget& b);

// Cumulative search nodes: this process's accumulated g_lastNodes plus prior.
unsigned long long tbNodes(const TrainBudget& b);

// Cumulative units (games, iterations): this process's plus prior.
long long tbUnits(const TrainBudget& b);

// True once the cumulative wall clock has reached wallStopSec. Always false
// when no stop is configured.
bool tbShouldStop(const TrainBudget& b);

// If a wall mark has come due, consume it and return true with its NOMINAL
// value in markOut (7200, not the 7203.4 actually elapsed), so a rung's file
// name is the same across seeds and regimes. Several marks can come due
// between two calls when one unit runs long; each call consumes exactly one,
// so call it in a while loop if the caller wants every rung written.
bool tbTakeDueMark(TrainBudget& b, double& markOut);

// Checkpoint path for a wall mark: outPath + "_t<seconds>.txt".
std::string tbMarkPath(const std::string& outPath, double markSec);

// The provenance fragment recording what was ACTUALLY spent, e.g.
//   ",games=1500,secs=7203.4,nodes=41288301"
// `unitName` is the regime's own word for a unit ("games", "iters").
std::string tbStamp(const TrainBudget& b, const char* unitName);

// Read a prior run's spend back out of a provenance line, so --resume
// continues the same cumulative ladder. Recognises the keys tbStamp writes and
// leaves any field it cannot find at zero. Returns true if at least one was
// found, which is also the check for "this file was written by a stamped run".
bool tbParsePrior(const std::string& teacher, TrainBudget& b, const char* unitName);
