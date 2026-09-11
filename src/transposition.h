#pragma once
#include "globals.h"
#include <cstdint>
#include <cstddef>

// ============================================================
// Transposition table (opt-in; gated by g_useTT)
// ============================================================
// A fixed-size, always-replace hash table of previously searched positions, keyed by
// the 64-bit FNV-1a position hash (datastore positionKey). Lets an alpha-beta search
// reuse a subtree's score/bound and order the previous best move first, which is what
// makes iterative deepening's deeper iterations cheap. Disabled by default so the
// console/GUI and the existing tests are unaffected.

enum TTFlag { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    uint64_t key;       // full position hash (0 = empty slot)
    int32_t  score;     // stored value (never a near-win sentinel; see ttStore)
    int16_t  depth;     // remaining depth (depth - level) this score is valid for
    uint8_t  flag;      // TTFlag
    uint8_t  gen;       // search generation (for replacement preference)
    int8_t   fromSq;    // best move source  sy*8+sx  (-1 = none)
    int8_t   toSq;      // best move dest    dy*8+dx  (-1 = none)
};

void   ttClear();                 // wipe every slot
void   ttNewSearch();             // bump the generation (call once per top-level search)
size_t ttBytes();                 // analytic table size in bytes (per-feature memory)

// Probe. Always fills fromSq/toSq with the stored best move for ordering (-1 if the
// slot does not hold this key). Returns true and sets `score` only when the stored
// entry is deep enough AND its bound permits an immediate [alpha,beta] cutoff.
bool ttProbe(uint64_t key, int depthLeft, int alpha, int beta,
             int& score, int& fromSq, int& toSq);

// Store (always-replace, preferring deeper/newer). Callers must skip near-win scores.
void ttStore(uint64_t key, int depthLeft, int score, int flag, int fromSq, int toSq);

// Read-only access for training code that walks the tree a search left behind
// (TD-Leaf's TreeStrap backup and ordinal move ranking, src/ml_tdleaf.cpp). The
// search itself never calls these, so a rated agent's behavior cannot depend on
// them. ttPeek copies the slot holding `key` into `out` and returns true only
// when the slot's key matches. ttGeneration is the generation the most recent
// ttNewSearch set, so `out.gen == ttGeneration()` means "stored by the latest
// top-level search" rather than left over from an earlier move. The generation
// is 8 bits and ttClear resets it to 0, so the test is exact for the first 255
// searches after a clear.
bool    ttPeek(uint64_t key, TTEntry& out);
uint8_t ttGeneration();
