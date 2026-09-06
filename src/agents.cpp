#include "agents.h"
#include "explorers.h"
#include "choosers.h"
#include "ai_eval.h"
#include "ai_random.h"
#include "moves.h"
#include <cstring>

// ============================================================
// HELPERS
// ============================================================
int learnedValueIndex() {
    for (int i = 0; i < g_evalCount; i++)
        if (string(g_evaluators[i].name) == "LearnedValue") return i;
    return -1;
}

static void copyName(AgentSpec& a, const char* name) {
    std::strncpy(a.name, name ? name : "agent", sizeof(a.name) - 1);
    a.name[sizeof(a.name) - 1] = '\0';
}
static void seedEvalParams(AgentSpec& a, int evaluator) {
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) a.evalParams[i] = 0;
    if (evaluator >= 0 && evaluator < g_evalCount)
        for (int i = 0; i < g_evaluators[evaluator].paramCount; i++)
            a.evalParams[i] = g_evaluators[evaluator].params[i].def;
}
// Default the budget/feature fields to "inherit global, historical search behavior".
static void seedAgentDefaults(AgentSpec& a) {
    a.nodeBudget = 0;
    a.timeBudgetMs = 0.0;
    a.calTargetMs = 0.0;
    a.useAlphaBeta = true;
    a.useTT = false;
    a.useMoveOrder = false;
    a.useQuiescence = false;
    a.keepPartial = false;
    a.aspirationWindow = 0;
    a.iterMinRemain = 0;
    a.retainBudget = false;
    a.gumbelCVisit = 50;
    a.gumbelCScaleTenths = 10;
    a.gumbelRootM = 16;
}

AgentSpec agentMakeSearch(const char* name, int explorer, int evaluator, int depth, int modelSlot) {
    AgentSpec a;
    copyName(a, name);
    a.brain = BRAIN_SEARCH;
    a.explorer = explorer;
    a.evaluator = evaluator;
    a.depth = depth;
    a.chooser = 0;
    a.chooserParam = 0;
    a.modelSlot = modelSlot;
    a.randomMoveProb = 0.0;
    a.depthCap = 0;
    a.dilDepth = 0;
    a.openerKind = -1;
    a.openerArg = 0;
    a.openerArg2 = 0;
    a.openerArg = 0;
    seedAgentDefaults(a);
    seedEvalParams(a, evaluator);
    return a;
}
AgentSpec agentMakePolicy(const char* name, int chooser, int chooserParam, int modelSlot) {
    AgentSpec a;
    copyName(a, name);
    a.brain = BRAIN_POLICY;
    a.explorer = 0;
    a.evaluator = 0;
    a.depth = 1;
    a.chooser = chooser;
    a.chooserParam = chooserParam;
    a.modelSlot = modelSlot;
    a.randomMoveProb = 0.0;
    a.depthCap = 0;
    a.dilDepth = 0;
    a.openerKind = -1;
    a.openerArg = 0;
    a.openerArg2 = 0;
    a.openerArg = 0;
    seedAgentDefaults(a);
    seedEvalParams(a, 0);
    return a;
}

// ============================================================
// MOVE SELECTION (composes explorer/chooser + dilution)
// ============================================================
int agentChooseMove(const AgentSpec& a, int side) {
    // Training-compute meter (src/train_budget.h). Cleared here so a move that
    // never searches (a policy brain, a diluted random move) contributes zero
    // rather than the previous move's count, and summed after the explorer runs
    // so a trainer can report the search nodes a whole run consumed. One 64-bit
    // add per move: nothing inside the search sees it.
    g_lastNodes = 0;

    // Dilution: with probability randomMoveProb, weaken this move. The diluted move is
    // either a fully random move (dilDepth <= 0) or a shallower depth-dilDepth search
    // (dilDepth > 0, search brain only) for a plausible-but-weaker blunder.
    bool dilute = (a.randomMoveProb > 0.0
                   && ((double)rand() / (double)RAND_MAX) < a.randomMoveProb);
    if (dilute && (a.dilDepth <= 0 || a.brain != BRAIN_SEARCH))
        return (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();

    if (a.brain == BRAIN_POLICY) {
        int c = (a.chooser >= 0 && a.chooser < g_chooserCount) ? a.chooser : 0;
        return g_choosers[c].fn(side, a.modelSlot, a.chooserParam);
    }

    // SEARCH brain.
    int depth = a.depth;
    if (dilute && a.dilDepth > 0) depth = a.dilDepth;              // stochastic depth dilution
    if (a.depthCap > 0 && depth > a.depthCap) depth = a.depthCap;   // dilution
    int params[MAX_EVAL_PARAMS];
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) params[i] = a.evalParams[i];
    if (a.evaluator == learnedValueIndex()) params[0] = a.modelSlot; // wire the model in
    int e = (a.explorer >= 0 && a.explorer < g_explorerCount) ? a.explorer : 0;

    // Apply this agent's per-search budgets/feature toggles, restoring the globals
    // afterward so one tournament can mix agents with different settings.
    unsigned long long savedNode = g_nodeBudget; double savedTime = g_timeBudgetMs;
    bool savedAB = g_useAlphaBeta, savedTT = g_useTT, savedMO = g_useMoveOrder, savedKP = g_keepPartial;
    bool savedQS = g_useQuiescence;
    int savedAsp = g_aspirationWindow;
    int savedIMR = g_iterMinRemain;
    double savedCVisit = g_gumbelCVisit, savedCScale = g_gumbelCScale;
    int savedRootM = g_gumbelRootM;
    // `retain`: this side's unspent nodes from earlier moves are added to the cap
    // for this one. The purse is per side so both agents in a game keep their own,
    // and it is read here and written back below, once the search reports what it
    // actually spent. Inert unless the agent has a node budget of its own.
    const int carrySlot = (side == White) ? 0 : 1;
    const bool retaining = a.retainBudget && a.nodeBudget != 0;
    unsigned long long effBudget = a.nodeBudget;
    if (retaining) effBudget = a.nodeBudget + g_nodeCarry[carrySlot];
    if (a.nodeBudget)        g_nodeBudget = effBudget;
    // Wall-clock purse, same contract as the node one and independent of it: an
    // agent may carry a node budget, a time budget, or both, and each banks only
    // its own remainder.
    const bool retainingTime = a.retainBudget && a.timeBudgetMs > 0.0;
    double effTimeMs = a.timeBudgetMs;
    if (retainingTime) effTimeMs = a.timeBudgetMs + g_timeCarry[carrySlot];
    if (a.timeBudgetMs > 0.0) g_timeBudgetMs = effTimeMs;
    g_useAlphaBeta = a.useAlphaBeta;
    g_useTT = a.useTT;
    g_useMoveOrder = a.useMoveOrder;
    g_useQuiescence = a.useQuiescence;
    g_keepPartial = a.keepPartial;
    g_aspirationWindow = a.aspirationWindow;
    g_iterMinRemain = a.iterMinRemain;
    g_gumbelCVisit = (double)a.gumbelCVisit;
    g_gumbelCScale = (double)a.gumbelCScaleTenths / 10.0;
    g_gumbelRootM = a.gumbelRootM;

    int victor = g_explorers[e].fn(side, a.evaluator, params, depth);
    g_trainNodesTotal += g_lastNodes;
    if (retaining) {
        // g_lastNodes is what this move actually searched. A budget can be
        // overshot slightly (the deadline is only tested every TIME_CHECK_MASK
        // nodes), so clamp rather than wrapping the unsigned subtraction.
        g_nodeCarry[carrySlot] = (g_lastNodes < effBudget) ? (effBudget - g_lastNodes) : 0ULL;
    }
    if (retainingTime) {
        // Same clamp, same reason: the wall clock is sampled every
        // TIME_CHECK_MASK nodes, so a search can finish a little past its
        // deadline and must bank nothing rather than a negative remainder.
        double left = effTimeMs - g_lastSearchMs;
        g_timeCarry[carrySlot] = (left > 0.0) ? left : 0.0;
    }

    g_nodeBudget = savedNode; g_timeBudgetMs = savedTime;
    g_useAlphaBeta = savedAB; g_useTT = savedTT; g_useMoveOrder = savedMO;
    g_useQuiescence = savedQS;
    g_keepPartial = savedKP; g_aspirationWindow = savedAsp;
    g_iterMinRemain = savedIMR;
    g_gumbelCVisit = savedCVisit; g_gumbelCScale = savedCScale; g_gumbelRootM = savedRootM;
    return victor;
}

// ============================================================
// DESCRIPTION
// ============================================================
string agentDescribe(const AgentSpec& a) {
    string s = string(a.name) + ": ";
    bool usesModel = false;
    if (a.brain == BRAIN_POLICY) {
        int c = (a.chooser >= 0 && a.chooser < g_chooserCount) ? a.chooser : 0;
        s += "Policy(" + string(g_choosers[c].name) + ")";
        usesModel = (string(g_choosers[c].name) == "LearnedPolicy");
    } else {
        int e = (a.explorer >= 0 && a.explorer < g_explorerCount) ? a.explorer : 0;
        int v = (a.evaluator >= 0 && a.evaluator < g_evalCount) ? a.evaluator : 0;
        s += string(g_explorers[e].name) + "(" + g_evaluators[v].name + ", d" + std::to_string(a.depth) + ")";
        usesModel = (a.evaluator == learnedValueIndex());
    }
    if (usesModel) s += " slot=" + std::to_string(a.modelSlot);
    if (a.randomMoveProb > 0.0) s += " rnd=" + std::to_string(a.randomMoveProb);
    if (a.dilDepth > 0)         s += " dil-d" + std::to_string(a.dilDepth);
    if (a.openerKind >= 0 && a.openerKind < g_openerCount) {
        s += string(" opener=") + g_openers[a.openerKind].idName;
        if (g_openers[a.openerKind].hasArg) {
            s += "(" + std::to_string(a.openerArg);
            if (g_openers[a.openerKind].hasArg2 && a.openerArg2 > 0)
                s += "," + std::to_string(a.openerArg2);
            s += ")";
        }
    }
    if (a.depthCap > 0)         s += " cap=" + std::to_string(a.depthCap);
    if (a.brain == BRAIN_SEARCH) {
        if (a.nodeBudget)         s += " nb=" + std::to_string(a.nodeBudget);
        if (a.retainBudget)       s += " retain";
        if (a.timeBudgetMs > 0.0) s += " tb=" + std::to_string((long)a.timeBudgetMs) + "ms";
        // Feature flags: list only the non-default (i.e. enabled extras / disabled AB).
        string flags;
        if (!a.useAlphaBeta)       flags += "noAB,";
        if (a.useTT)               flags += "TT,";
        if (a.useMoveOrder)        flags += "ord,";
        if (a.useQuiescence)       flags += "qs,";
        if (a.keepPartial)         flags += "part,";
        if (a.aspirationWindow > 0) flags += "asp" + std::to_string(a.aspirationWindow) + ",";
        if (a.iterMinRemain > 0) flags += "rem" + std::to_string(a.iterMinRemain) + ",";
        if (a.gumbelCVisit != 50)         flags += "cvisit" + std::to_string(a.gumbelCVisit) + ",";
        if (a.gumbelCScaleTenths != 10)   flags += "cscale" + std::to_string(a.gumbelCScaleTenths) + ",";
        if (a.gumbelRootM != 16)          flags += "m" + std::to_string(a.gumbelRootM) + ",";
        if (!flags.empty()) { flags.pop_back(); s += " [" + flags + "]"; }
    }
    return s;
}
