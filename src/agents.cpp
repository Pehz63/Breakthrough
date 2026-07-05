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
    a.useAlphaBeta = true;
    a.useTT = false;
    a.useMoveOrder = false;
    a.keepPartial = false;
    a.aspirationWindow = 0;
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
    seedAgentDefaults(a);
    seedEvalParams(a, 0);
    return a;
}

// ============================================================
// MOVE SELECTION (composes explorer/chooser + dilution)
// ============================================================
int agentChooseMove(const AgentSpec& a, int side) {
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
    int savedAsp = g_aspirationWindow;
    if (a.nodeBudget)        g_nodeBudget = a.nodeBudget;
    if (a.timeBudgetMs > 0.0) g_timeBudgetMs = a.timeBudgetMs;
    g_useAlphaBeta = a.useAlphaBeta;
    g_useTT = a.useTT;
    g_useMoveOrder = a.useMoveOrder;
    g_keepPartial = a.keepPartial;
    g_aspirationWindow = a.aspirationWindow;

    int victor = g_explorers[e].fn(side, a.evaluator, params, depth);

    g_nodeBudget = savedNode; g_timeBudgetMs = savedTime;
    g_useAlphaBeta = savedAB; g_useTT = savedTT; g_useMoveOrder = savedMO;
    g_keepPartial = savedKP; g_aspirationWindow = savedAsp;
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
    if (a.depthCap > 0)         s += " cap=" + std::to_string(a.depthCap);
    if (a.brain == BRAIN_SEARCH) {
        if (a.nodeBudget)         s += " nb=" + std::to_string(a.nodeBudget);
        if (a.timeBudgetMs > 0.0) s += " tb=" + std::to_string((long)a.timeBudgetMs) + "ms";
        // Feature flags: list only the non-default (i.e. enabled extras / disabled AB).
        string flags;
        if (!a.useAlphaBeta)       flags += "noAB,";
        if (a.useTT)               flags += "TT,";
        if (a.useMoveOrder)        flags += "ord,";
        if (a.keepPartial)         flags += "part,";
        if (a.aspirationWindow > 0) flags += "asp" + std::to_string(a.aspirationWindow) + ",";
        if (!flags.empty()) { flags.pop_back(); s += " [" + flags + "]"; }
    }
    return s;
}
