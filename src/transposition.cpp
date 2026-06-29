#include "transposition.h"
#include <vector>
#include <algorithm>

// ~1M entries (power of two so the index is a mask). At sizeof(TTEntry)=24 this is
// ~24 MB, shared by the whole process.
static const size_t TT_BITS = 20;
static const size_t TT_SIZE = (size_t)1 << TT_BITS;
static const size_t TT_MASK = TT_SIZE - 1;

static std::vector<TTEntry> g_tt;
static uint8_t g_ttGen = 0;

static void ttEnsure() {
    if (g_tt.empty()) g_tt.assign(TT_SIZE, TTEntry{0, 0, 0, 0, 0, -1, -1});
}

void ttClear() {
    ttEnsure();
    std::fill(g_tt.begin(), g_tt.end(), TTEntry{0, 0, 0, 0, 0, -1, -1});
    g_ttGen = 0;
}

void ttNewSearch() {
    ttEnsure();
    g_ttGen++;
}

size_t ttBytes() { return TT_SIZE * sizeof(TTEntry); }

bool ttProbe(uint64_t key, int depthLeft, int alpha, int beta,
             int& score, int& fromSq, int& toSq) {
    ttEnsure();
    const TTEntry& e = g_tt[key & TT_MASK];
    fromSq = -1; toSq = -1;
    if (e.key != key) return false;      // empty or a collision on a different position
    fromSq = e.fromSq; toSq = e.toSq;    // usable for ordering regardless of depth
    if (e.depth < depthLeft) return false;
    if (e.flag == TT_EXACT)                     { score = e.score; return true; }
    if (e.flag == TT_LOWER && e.score >= beta)  { score = e.score; return true; }
    if (e.flag == TT_UPPER && e.score <= alpha) { score = e.score; return true; }
    return false;
}

void ttStore(uint64_t key, int depthLeft, int score, int flag, int fromSq, int toSq) {
    ttEnsure();
    TTEntry& e = g_tt[key & TT_MASK];
    // Keep a deeper, same-generation entry for this exact key; otherwise replace.
    if (e.key == key && e.gen == g_ttGen && e.depth > depthLeft) return;
    e.key = key; e.score = score; e.depth = (int16_t)depthLeft;
    e.flag = (uint8_t)flag; e.gen = g_ttGen;
    e.fromSq = (int8_t)fromSq; e.toSq = (int8_t)toSq;
}
