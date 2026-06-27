#pragma once
#include "globals.h"

// ============================================================
// Move choosers / policies (direct, no lookahead)
// ============================================================
// A chooser picks and plays a move for one side WITHOUT building a search tree:
// either a simple heuristic (the random family) or a learned move-rater (policy
// model). It plays the move and returns the victor code, like moveWhite/Black.
// This is the "Policy" half of the agent abstraction (the alternative to Search).
//
// To add a chooser: append a ChooserDef to g_choosers[] and write its fn.

struct ChooserDef {
    const char* name;
    const char* desc;
    // side: White/Black. modelSlot: policy model slot (learned choosers only).
    // param: chooser-specific knob (e.g. SmartRandom's furthest-N). Returns victor.
    int (*fn)(int side, int modelSlot, int param);
};

extern const ChooserDef g_choosers[];
extern const int        g_chooserCount;
