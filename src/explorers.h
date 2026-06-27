#pragma once
#include "globals.h"

// ============================================================
// Move-tree explorers (search strategies)
// ============================================================
// An explorer decides HOW a board-state evaluator is used to choose and play a
// move for one side: it plays the move on the live board and returns the victor
// code (None / WhiteWin / BlackWin), exactly like moveWhite/Black. Explorers are
// the "search" half of a Search brain; the evaluator is the other half.
//
// To add an explorer: append an ExplorerDef to g_explorers[] and write its fn.
// Any AgentSpec can then select it by index, and the docs pick it up.

struct ExplorerDef {
    const char* name;
    const char* desc;
    // side: White/Black. evaluator + params: the board-state evaluator to use.
    // budget: search depth (or analogous effort). Returns the victor code.
    int (*fn)(int side, int evaluator, const int* params, int budget);
};

extern const ExplorerDef g_explorers[];
extern const int         g_explorerCount;
