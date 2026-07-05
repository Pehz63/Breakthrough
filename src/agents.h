#pragma once
#include "globals.h"

// ============================================================
// Agents: composition of the pluggable axes
// ============================================================
// An AgentSpec is a complete "player": a brain that is either a SEARCH (a
// move-tree explorer + a board-state evaluator) or a POLICY (a direct move
// chooser), plus optional strength dilution. agentChooseMove() plays one move for
// a side and returns the victor code, so the same agent drives tournaments, data
// generation, and (later) the GUI opponent.

enum BrainKind { BRAIN_SEARCH = 0, BRAIN_POLICY = 1 };

struct AgentSpec {
    char   name[48];
    int    brain;                       // BrainKind

    // SEARCH brain:
    int    explorer;                    // index into g_explorers
    int    evaluator;                   // index into g_evaluators
    int    evalParams[MAX_EVAL_PARAMS]; // evaluator weights
    int    depth;                       // search budget

    // POLICY brain:
    int    chooser;                     // index into g_choosers
    int    chooserParam;                // chooser knob (e.g. SmartRandom N)

    int    modelSlot;                   // model slot for learned eval / policy

    // Strength dilution (to spread an Elo ladder):
    double randomMoveProb;              // chance per move to dilute (see dilDepth)
    int    depthCap;                    // cap search depth (<=0 = no cap)
    int    dilDepth;                    // dilution move: 0 = a fully random move (default),
                                        // >0 = a shallower depth-N search instead (search brain only)

    // Per-agent search budgets (0 = inherit the global g_nodeBudget / g_timeBudgetMs):
    unsigned long long nodeBudget;      // per-move node cap for this agent's search
    double timeBudgetMs;                // per-move wall-clock cap (ms) for this agent

    // Per-feature toggles (defaults reproduce the historical search; see globals.h):
    bool   useAlphaBeta;                // false = full minimax (ablation baseline)
    bool   useTT;                       // transposition table
    bool   useMoveOrder;                // killer/history/TT move ordering
    bool   keepPartial;                 // keep a budget-cut iteration's best move
    int    aspirationWindow;            // 0 = full window; >0 = aspiration half-width
};

// Construct common agents (evalParams seeded from the evaluator's registry defaults).
AgentSpec agentMakeSearch(const char* name, int explorer, int evaluator, int depth, int modelSlot);
AgentSpec agentMakePolicy(const char* name, int chooser, int chooserParam, int modelSlot);

// Play one move for `side` using this agent; returns the victor code.
int agentChooseMove(const AgentSpec& a, int side);

// One-line human-readable description (used in the agent library / manifest).
string agentDescribe(const AgentSpec& a);

// Index of the LearnedValue evaluator in g_evaluators (so callers can wire the
// model slot into its parameter array). -1 if not present.
int learnedValueIndex();
