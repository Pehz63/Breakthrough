#pragma once
#include "globals.h"

// Board-state evaluators are pluggable. Each evaluator is described by an EvalDef
// in the g_evaluators registry (defined in ai_eval.cpp): a display name, a list of
// named integer parameters (with defaults and UI ranges), and a function that
// scores the current board reading those parameters from a plain int array.
//
// To add or change an evaluator you edit ONE table entry plus its function body in
// ai_eval.cpp. Both the console settings and the GUI render their controls from
// this table, so new parameters appear in both UIs automatically.
//
// MAX_EVAL_PARAMS (the cap on a single evaluator's parameter count) is defined in
// globals.h so param-array callers need only that header.

struct EvalParamDef {
    const char* name;   // shown in the UI (slider label / prompt text)
    const char* key;    // short key used in minimax_params.txt (e.g. "turn")
    int def;            // default value
    int lo;             // minimum (UI range / validation)
    int hi;             // maximum (UI range / validation)
};

struct EvalDef {
    const char* name;                          // dropdown label, e.g. "Classic"
    int paramCount;                            // number of params used (<= MAX_EVAL_PARAMS)
    EvalParamDef params[MAX_EVAL_PARAMS];
    int (*fn)(int turnColor, const int* p);    // leaf-node score (full recompute)
    bool incremental;                          // true if it uses the standard layout so the
                                               // g_evalPos accumulator can score it during search
};

extern const EvalDef g_evaluators[];
extern const int     g_evalCount;

// Near-win shortcut: WhiteWin / BlackWin if the position is decided one move from a
// goal row, else 0. Exposed so the learned evaluator and 1-ply explorers reuse the
// exact same decided-position logic as the Classic/Experimental leaf scores.
int nearWinCheck(int turnColor);
