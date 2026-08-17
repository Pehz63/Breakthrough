#pragma once
#include "globals.h"
#include "ml_eval.h"
#include <fstream>
#include <string>

// Portable existence check for a test guarding against clobbering a file it
// does not own (e.g. before writing scratch data to a slot path).
inline bool fileExists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

// ============================================================
// Test-suite scratch model slots
// ============================================================
// Every test that writes a throwaway model file to a slot MUST take its slot
// number from this list, not a bare `ML_SLOTS - N` expression written inline.
// This is the whole fix: a slot number picked ad hoc in each test file is a
// number nobody can grep reliably (this list itself replaced a set of
// ML_SLOTS-N literals that, despite a careful audit, still collided twice --
// ML_SLOTS-5 and ML_SLOTS-6 were each independently claimed by two different
// tests). One enumerated list in one header is what a reviewer -- or the
// compiler, via ODR on the enumerator names -- can't fail to see. All values
// fall inside the reserved scratch range (ML_RESERVED_SLOTS, ml_eval.h), which
// rankSlotFile() (src/ranking.h) routes to models/scratch/ instead of
// models/sweep/, so even a mistaken reuse here can never touch a live roster
// agent's model file the way slot 6/7/9 were lost (see todo.md's
// Elo/Tournaments section). Add a new enumerator, at the next unused offset,
// when a new test needs a scratch slot; never reuse another test's value
// unless that test's own comment says it deliberately shares it.
enum TestScratchSlot {
    kScratchSlotScheduler      = ML_SLOTS - 1,  // "ranking scheduler - legacy-form..." + "...cohort filter..." (deliberately shared: the second test reads the model the first one saved)
    kScratchSlotRegimeTokenA   = ML_SLOTS - 2,  // "ranking id - regime token replaces..." (first slot)
    kScratchSlotRegimeTokenB   = ML_SLOTS - 3,  // "ranking id - regime token replaces..." (second slot)
    kScratchSlotRisk           = ML_SLOTS - 4,  // "ranking id - learned() optional risk= weight"
    kScratchSlotLoadModelsOk   = ML_SLOTS - 5,  // "ranking - rankLoadAgentModels" (loads-successfully case)
    kScratchSlotLoadModelsMiss = ML_SLOTS - 6,  // "ranking - rankLoadAgentModels" (missing-file case)
    kScratchSlotGumbelZeroSearch = ML_SLOTS - 7,  // "GumbelRootInfo::searchValue - diverges from rootValue..."
    kScratchSlotGumbelZeroLoad   = ML_SLOTS - 8,  // "trainGumbelZero - Pass 1 sanity..." (post-training reload)
};

// Copy layout[x][y] into board[][] and recalculate all global counters.
// Sets PRNT=0 to silence all output during tests.
inline void setupBoard(const char layout[SIZE][SIZE]) {
    PRNT = 0;
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            board[x][y] = layout[x][y];
            if (board[x][y] == WHITE) {
                g_whiteCount++;
                g_chipDiff++;
                if (y == SIZE-1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++;
                g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
}

// Clear board to EMPTY and zero all counters.
inline void clearBoard() {
    PRNT = 0;
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            board[x][y] = EMPTY;
}

// Play a game from the current board state.
// Returns 0=White wins, 1=Black wins, -1=timeout (maxHalfMoves exceeded).
// Uses a bounded for loop; no infinite-loop risk.
inline int runGame(int whiteType, int w1, int w2, int w3, int w4, int w5,
                   int blackType, int b1, int b2, int b3, int b4, int b5,
                   int maxHalfMoves) {
    // w2..w5 / b2..b5 are the Classic evaluator weights (turn, chip, wall, column).
    int wParams[MAX_EVAL_PARAMS] = { w2, w3, w4, w5 };
    int bParams[MAX_EVAL_PARAMS] = { b2, b3, b4, b5 };
    int victor;
    for (int h = 0; h < maxHalfMoves; h++) {
        if (h % 2 == 0) {
            victor = moveWhite(whiteType, w1, 0, wParams, StandardOpener);
        } else {
            victor = moveBlack(blackType, b1, 0, bParams, StandardOpener);
        }
        if (victor == WhiteWin) return 0;
        if (victor == BlackWin) return 1;
    }
    return -1;
}
