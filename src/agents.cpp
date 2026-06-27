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
    seedEvalParams(a, 0);
    return a;
}

// ============================================================
// MOVE SELECTION (composes explorer/chooser + dilution)
// ============================================================
int agentChooseMove(const AgentSpec& a, int side) {
    // Dilution: occasionally throw a fully random move to weaken the agent.
    if (a.randomMoveProb > 0.0 && ((double)rand() / (double)RAND_MAX) < a.randomMoveProb)
        return (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();

    if (a.brain == BRAIN_POLICY) {
        int c = (a.chooser >= 0 && a.chooser < g_chooserCount) ? a.chooser : 0;
        return g_choosers[c].fn(side, a.modelSlot, a.chooserParam);
    }

    // SEARCH brain.
    int depth = a.depth;
    if (a.depthCap > 0 && depth > a.depthCap) depth = a.depthCap;   // dilution
    int params[MAX_EVAL_PARAMS];
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) params[i] = a.evalParams[i];
    if (a.evaluator == learnedValueIndex()) params[0] = a.modelSlot; // wire the model in
    int e = (a.explorer >= 0 && a.explorer < g_explorerCount) ? a.explorer : 0;
    return g_explorers[e].fn(side, a.evaluator, params, depth);
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
    if (a.depthCap > 0)         s += " cap=" + std::to_string(a.depthCap);
    return s;
}
