#pragma once
#include "globals.h"

// ============================================================
// Datastore: append-only JSONL + canonical position keys
// ============================================================
// The C++ side writes open, append-only data files (no dependencies, crash-safe)
// that DuckDB / pandas later query in place. Entity streams live under data/:
//   runs, models, agents, games, positions, evaluations, labels.
// Curated rollups (manifest, agent library) are written by the trainer.

// Canonical key for the current board position.
struct PosKey {
    string             enc;    // 65 chars: 64 squares (y-major) + side-to-move
    unsigned long long hash;   // 64-bit FNV-1a of enc (join key)
};

// Build the canonical key for the current board. If mirrorFold is true, the board
// is folded to its lexicographically-smaller left-right mirror so symmetric
// positions share one key (useful for aggregating evaluations).
PosKey positionKey(int sideToMove, bool mirrorFold);

// Decode a positionKey enc string (64 y-major square chars + a 'W'/'B'
// side-to-move char) onto the global board, rebuilding the incremental
// counters (g_whiteCount/g_blackCount/g_chipDiff/g_whiteAtEnd/g_blackAtEnd)
// exactly the way reloadBoard seeds them, so play can start from the decoded
// position. Returns false (board untouched) on malformed input: wrong length,
// an invalid square char, or an invalid side char.
bool decodePositionEnc(const string& enc, int& sideToMove);

// Append one already-formatted JSON object (single line) to a file, creating it
// (and parent dirs are assumed to exist). Adds the trailing newline.
void dsAppendLine(const string& file, const string& jsonLine);

// Minimal JSON string escaper for values that might contain quotes/backslashes.
string dsJsonEscape(const string& s);
