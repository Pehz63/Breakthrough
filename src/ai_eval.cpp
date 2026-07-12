#include "ai_eval.h"
#include "board_analysis.h"
#include "ml_eval.h"

// ============================================================
// Shared evaluation pieces
// ============================================================
// The two shipped evaluators (Classic, Experimental) share the same structure:
//   leaf score = near-win shortcut, else  turn + chipDiff*chip + positional
// where "positional" is the wall/column structure terms plus (Experimental) an
// "Forward" term. The positional part is the only piece that needs a full board
// scan, so it is the part maintained incrementally during a minimax search (see
// g_evalPos and evalPosLocal below). nearWinCheck and the turn/chip combine are
// cheap and shared so the full and incremental paths can never diverge.

// Near-win shortcut: returns WhiteWin / BlackWin if the position is already
// decided one move from a goal row, otherwise 0. Identical to the original
// in-evaluator logic. Declared in ai_eval.h so learned evaluators and 1-ply
// explorers share the exact same decided-position logic.
int nearWinCheck(int turnColor) {
    int x, y;
    if (turnColor == White) {
        y = SIZE-2;
        for (x = 0; x < SIZE; x++)
            if (board[x][y] == WHITE) return WhiteWin;
        y = 1;
        for (x = 0; x < SIZE; x++)
            if ((board[x][y] == BLACK) && (x == 0 || board[x-1][y-1] != WHITE) && (x == SIZE-1 || board[x+1][y-1] != WHITE))
                return BlackWin;
    } else if (turnColor == Black) {
        y = 1;
        for (x = 0; x < SIZE; x++)
            if (board[x][y] == BLACK) return BlackWin;
        y = SIZE-2;
        for (x = 0; x < SIZE; x++)
            if ((board[x][y] == WHITE) && (x == 0 || board[x-1][y+1] != BLACK) && (x == SIZE-1 || board[x+1][y+1] != BLACK))
                return WhiteWin;
    }
    return 0;
}

// Contribution of one orthogonally-adjacent pair given the live board. Same-row
// pairs use the wall weight, same-column pairs use the column weight; signed +
// for a White pair, - for a Black pair, 0 if the two squares differ or are empty.
static inline int pairContrib(int ax, int ay, int bx, int by, int wallW, int colW) {
    char a = board[ax][ay];
    if (a == EMPTY || board[bx][by] != a) return 0;
    int w = (ay == by) ? wallW : colW;   // same row -> horizontal/wall, else vertical/column
    return (a == WHITE) ? w : -w;
}

// Structure contribution "owned" by cell (x,y): its right pair (x+1,y) and its
// upper pair (x,y+1). Mirrors the original scan, where the owner loop runs over
// x,y in [0, SIZE-2], so row SIZE-1 walls and column SIZE-1 columns are reached
// only as the neighbor of a lower/left owner. Owners outside that range own
// nothing.
static inline int structOwner(int x, int y, int wallW, int colW) {
    if (x < 0 || y < 0 || x > SIZE-2 || y > SIZE-2) return 0;
    return pairContrib(x, y, x+1, y, wallW, colW) + pairContrib(x, y, x, y+1, wallW, colW);
}

// Per-square forward contribution: reward a piece for how far it has pushed
// toward its goal row (White up to SIZE-1, Black down to 0).
static inline int forwardContrib(int x, int y, int fwdW) {
    char c = board[x][y];
    if (c == WHITE) return fwdW * y;
    if (c == BLACK) return -fwdW * (SIZE-1 - y);
    return 0;
}

// ============================================================
// Advanced-evaluator terms
// ============================================================
// The Advanced evaluator extends the standard incremental layout with the
// feature terms below. Parameter slots (a paramCount >= ADV_PARAMS declares
// the full layout; the letters are the ranking ID codec's):
//   p[0]=turn t     p[1]=chip c     p[2]=wall w      p[3]=column l
//   p[4]=forward f  p[5]=support d  p[6]=center e    p[7]=mobility m
//   p[8]=hole h     p[9]=control b  p[10]=open o     p[11]=race r
//   p[12]=overext x p[13]=noise n   p[14]=noiseseed s p[15]=racewin g
// Every term is white-centric (positive favors White) and mirrored for Black
// (Docs/axioms.md D5). Each helper serves BOTH the full board scan
// (evalPosFull) and the per-move local delta (evalPosLocal), so the two paths
// cannot diverge (the theory-13 lesson).
#define ADV_PARAMS 16
enum AdvParamIdx {
    ADV_SUPPORT = 5, ADV_CENTER = 6, ADV_MOBILITY = 7, ADV_HOLE = 8,
    ADV_CONTROL = 9, ADV_OPEN = 10, ADV_RACE = 11, ADV_OVEREXT = 12,
    ADV_NOISE = 13, ADV_NOISESEED = 14, ADV_RACEWIN = 15
};

// Support: diagonal same-color adjacent pairs. The diagonal-behind piece is
// exactly the piece that recaptures after a capture (Docs/axioms.md Lemma C),
// so this is the defensive analog of the orthogonal wall/column terms. It
// merges todo.md's "defended pieces" and "diagonal phalanx" ideas, which
// describe the same pair geometry. Single-ownership convention mirroring
// structOwner: the LOWER piece (x,y), y in [0, SIZE-2], owns its up-right pair
// (x+1,y+1) and its up-left pair (x-1,y+1).
static inline int diagPairContrib(int ax, int ay, int bx, int by, int supW) {
    char a = board[ax][ay];
    if (a == EMPTY || board[bx][by] != a) return 0;
    return (a == WHITE) ? supW : -supW;
}
static inline int diagOwner(int x, int y, int supW) {
    if (x < 0 || x > SIZE-1 || y < 0 || y > SIZE-2) return 0;
    int s = 0;
    if (x+1 <= SIZE-1) s += diagPairContrib(x, y, x+1, y+1, supW);
    if (x-1 >= 0)      s += diagPairContrib(x, y, x-1, y+1, supW);
    return s;
}
// All diagonal pairs touching square (x,y): the two it owns plus the two owned
// by its lower diagonal neighbors. Summed over a move's two changed squares
// this yields the exact structure delta: a diagonal move's shared source-dest
// pair is counted by both squares, but it is 0 in both passes (before the move
// the destination is empty or enemy, after it the source is empty), so the
// double count cancels; a straight move's squares share no diagonal pair.
static inline int supportNeighbors(int x, int y, int supW) {
    int s = diagOwner(x, y, supW);
    if (y-1 >= 0) {
        if (x-1 >= 0)      s += diagPairContrib(x-1, y-1, x, y, supW);
        if (x+1 <= SIZE-1) s += diagPairContrib(x+1, y-1, x, y, supW);
    }
    return s;
}

// Center: like Forward but scaled by how central the file is (0 at the edges,
// SIZE/2-1 in the middle). An advanced center piece keeps two escape diagonals
// where an edge piece has one, so it is harder to wall off.
static inline int centerContrib(int x, int y, int ctrW) {
    char c = board[x][y];
    if (c == EMPTY) return 0;
    int cs = (x < SIZE-1-x) ? x : SIZE-1-x;
    return (c == WHITE) ? ctrW * y * cs : -ctrW * (SIZE-1 - y) * cs;
}

// Control: occupancy of the two rows nearest the opponent's home row (the
// actual contested breakthrough squares, Docs/axioms.md D10's neighborhood).
static inline int controlContrib(int x, int y, int conW) {
    char c = board[x][y];
    if (c == WHITE) return (y >= SIZE-3 && y <= SIZE-2) ? conW : 0;
    if (c == BLACK) return (y >= 1 && y <= 2) ? -conW : 0;
    return 0;
}

// Noise, form 1 (Noise > 0): a seeded random piece-square table. Each (color,
// square) gets a fixed pseudo-random value in [-noiseW, +noiseW] derived from
// the seed param, so the term is deterministic per seed and position (an agent
// using it stays deterministic) and incremental like any per-square term.
// CAUTION (theory 20): this form's board total scales with piece count
// (~sqrt(pieces)*noiseW typical) and its per-square values persist all game (a
// systematic square bias, not a tie-break); at material-scale weights it
// measured catastrophically bad. The bounded jitter below is the tie-only form.
static inline int noiseContrib(int x, int y, int noiseW, int seed) {
    char c = board[x][y];
    if (c == EMPTY) return 0;
    unsigned h = 2166136261u;
    h = (h ^ (unsigned)seed) * 16777619u;
    h = (h ^ (unsigned)(x*SIZE + y)) * 16777619u;
    h = (h ^ (unsigned)((c == WHITE) ? 1 : 2)) * 16777619u;
    h ^= h >> 13; h *= 2654435761u; h ^= h >> 16;
    int v = (int)(h % (unsigned)(2*noiseW + 1)) - noiseW;
    return (c == WHITE) ? v : -v;
}

// Noise, form 2 (Noise < 0): bounded per-position jitter, tie-only by
// construction. raw = the sum over occupied squares of noiseHashRaw (a
// different salt than noiseContrib so the streams decorrelate); the jitter is
// (raw mod (2*mag+1)) - mag, bounded in [-mag, +mag] EXACTLY regardless of
// piece count, deterministic per (seed, position), and re-rolled pseudo-
// randomly by every move (any make changes 2-3 raw terms, and the mod
// decorrelates the result). The leaf then returns
//     realEval * ADV_JITTER_SCALE + jitter
// (the developer's design): |jitter| <= 99 so two jitters differ by < 256,
// while two positions whose real evals differ by even 1 unit differ by >= 256
// after scaling -- the jitter can NEVER reverse a strict real preference, it
// reorders exact ties only. Win sentinels bypass the scaling (early return),
// and the worst-case scaled eval (~60k * 256 ~ 15M) stays far below them.
// raw lives in its own accumulator (g_noiseAcc, the g_mlAcc pattern), NOT in
// g_evalPos: the leaf applies a mod, so it cannot ride the shared linear sum.
#define ADV_JITTER_SCALE 256
unsigned noiseHashRaw(int seed, char c, int x, int y) {
    unsigned h = 2166136261u;
    h = (h ^ (unsigned)seed) * 16777619u;
    h = (h ^ (unsigned)(x*SIZE + y)) * 16777619u;
    h = (h ^ (unsigned)((c == WHITE) ? 3 : 4)) * 16777619u;   // salts 3/4 (PST form uses 1/2)
    h ^= h >> 13; h *= 2654435761u; h ^= h >> 16;
    return h;
}
unsigned long long noiseRawScan(int seed) {
    unsigned long long raw = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] != EMPTY)
                raw += noiseHashRaw(seed, board[x][y], x, y);
    return raw;
}
static inline int jitterValue(unsigned long long raw, int mag) {
    return (int)(raw % (unsigned long long)(2*mag + 1)) - mag;
}

// Mobility: a piece's count of legal moves (straight needs an empty square,
// diagonals need empty-or-enemy, all on-board). A tempo/flexibility proxy.
static inline int mobilityContrib(int x, int y, int mobW) {
    char c = board[x][y];
    if (c == WHITE) {
        if (y+1 > SIZE-1) return 0;   // goal-row piece: game already decided
        int n = (board[x][y+1] == EMPTY) ? 1 : 0;
        if (x-1 >= 0      && board[x-1][y+1] != WHITE) n++;
        if (x+1 <= SIZE-1 && board[x+1][y+1] != WHITE) n++;
        return mobW * n;
    }
    if (c == BLACK) {
        if (y-1 < 0) return 0;
        int n = (board[x][y-1] == EMPTY) ? 1 : 0;
        if (x-1 >= 0      && board[x-1][y-1] != BLACK) n++;
        if (x+1 <= SIZE-1 && board[x+1][y-1] != BLACK) n++;
        return -mobW * n;
    }
    return 0;
}

// Overextension: an advanced piece (past the midline) with no friendly piece
// on either diagonal-behind square, i.e. nobody recaptures its captor (Lemma
// C geometry). A positive weight penalizes one's own overextended pieces,
// discriminating a supported advance from a bare one.
static inline int overextContrib(int x, int y, int oxW) {
    char c = board[x][y];
    if (c == WHITE) {
        if (y < SIZE/2) return 0;
        bool def = (x-1 >= 0      && board[x-1][y-1] == WHITE)
                || (x+1 <= SIZE-1 && board[x+1][y-1] == WHITE);
        return def ? 0 : -oxW;
    }
    if (c == BLACK) {
        if (y > SIZE/2 - 1) return 0;
        bool def = (x-1 >= 0      && board[x-1][y+1] == BLACK)
                || (x+1 <= SIZE-1 && board[x+1][y+1] == BLACK);
        return def ? 0 : oxW;
    }
    return 0;
}

// Mobility and overextension of a piece depend only on squares within one row
// and one column of it, so a move can change them only for pieces inside the
// one-square bounding box around its two changed squares. The local delta sums
// the whole box: the cell set is coordinate-defined (identical before and
// after the move), covers every affected piece, and cells whose contribution
// did not change cancel in the before/after subtraction. One box sum also
// avoids double-counting the cells shared by the two squares' neighborhoods.
static int boxTermsContrib(int x0, int y0, int x1, int y1, int mobW, int oxW) {
    int lox = ((x0 < x1) ? x0 : x1) - 1; if (lox < 0) lox = 0;
    int hix = ((x0 > x1) ? x0 : x1) + 1; if (hix > SIZE-1) hix = SIZE-1;
    int loy = ((y0 < y1) ? y0 : y1) - 1; if (loy < 0) loy = 0;
    int hiy = ((y0 > y1) ? y0 : y1) + 1; if (hiy > SIZE-1) hiy = SIZE-1;
    int s = 0;
    for (int y = loy; y <= hiy; y++)
        for (int x = lox; x <= hix; x++) {
            if (board[x][y] == EMPTY) continue;
            if (mobW != 0) s += mobilityContrib(x, y, mobW);
            if (oxW  != 0) s += overextContrib(x, y, oxW);
        }
    return s;
}

// Hole: back-rank columns that admit a winning outpost (Docs/axioms.md D10).
// An enemy piece one row from the home row at column x can be captured only
// from the home-row squares (x-1, home) and (x+1, home); if no on-board guard
// square holds a friendly piece, the column is an outpost-admitting hole. A
// positive weight penalizes one's own holes.
static inline int holeColContrib(int col, bool forWhite, int holeW) {
    if (col < 0 || col > SIZE-1) return 0;
    if (forWhite) {
        bool guarded = (col-1 >= 0      && board[col-1][0] == WHITE)
                    || (col+1 <= SIZE-1 && board[col+1][0] == WHITE);
        return guarded ? 0 : -holeW;
    }
    bool guarded = (col-1 >= 0      && board[col-1][SIZE-1] == BLACK)
                || (col+1 <= SIZE-1 && board[col+1][SIZE-1] == BLACK);
    return guarded ? 0 : holeW;
}
// Hole status depends only on home-row occupancy, so a move affects it only
// when a changed square lies on a home row, and then only for the two columns
// whose guard squares include that square. A move's two squares sit on
// adjacent rows, so at most one of them is on any given home row.
static int holeLocalContrib(int sx, int sy, int dx, int dy, int holeW) {
    int s = 0;
    if (sy == 0)      s += holeColContrib(sx-1, true, holeW)  + holeColContrib(sx+1, true, holeW);
    if (dy == 0)      s += holeColContrib(dx-1, true, holeW)  + holeColContrib(dx+1, true, holeW);
    if (sy == SIZE-1) s += holeColContrib(sx-1, false, holeW) + holeColContrib(sx+1, false, holeW);
    if (dy == SIZE-1) s += holeColContrib(dx-1, false, holeW) + holeColContrib(dx+1, false, holeW);
    return s;
}

// Open file: a file with no own piece on the own half (rows 0..SIZE/2-1 for
// White) while the enemy has any piece in the file is a clear lane for their
// advance. A positive weight penalizes files open against oneself.
static int openFileContrib(int col, int openW) {
    bool wOwnHalf = false, wAny = false, bOwnHalf = false, bAny = false;
    for (int y = 0; y < SIZE; y++) {
        char c = board[col][y];
        if (c == WHITE)      { wAny = true; if (y <= SIZE/2 - 1) wOwnHalf = true; }
        else if (c == BLACK) { bAny = true; if (y >= SIZE/2)     bOwnHalf = true; }
    }
    int s = 0;
    if (!wOwnHalf && bAny) s -= openW;
    if (!bOwnHalf && wAny) s += openW;
    return s;
}
// A move changes occupancy in at most two files (one for a straight move).
static int openLocalContrib(int sx, int dx, int openW) {
    int s = openFileContrib(sx, openW);
    if (dx != sx) s += openFileContrib(dx, openW);
    return s;
}

// Row extremes for the Race / RaceWin leaf terms: the least/most advanced
// occupied row per side (-1 when a side has no pieces). The counter version
// reads the incrementally-maintained g_rowCountW/B; the scan version reads
// the board (used by the full evaluator outside an incremental search).
static void rowExtremesFromCounts(int& maxW, int& minW, int& maxB, int& minB) {
    maxW = minW = maxB = minB = -1;
    for (int y = 0; y < SIZE; y++) {
        if (g_rowCountW[y] > 0) { if (minW < 0) minW = y; maxW = y; }
        if (g_rowCountB[y] > 0) { if (minB < 0) minB = y; maxB = y; }
    }
}
static void rowExtremesByScan(int& maxW, int& minW, int& maxB, int& minB) {
    maxW = minW = maxB = minB = -1;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            char c = board[x][y];
            if (c == WHITE)      { if (minW < 0) minW = y; maxW = y; }
            else if (c == BLACK) { if (minB < 0) minB = y; maxB = y; }
        }
}

// RaceWin: the exact decided-race detector from Docs/axioms.md D9/D14.
// White's most advanced piece is a passed runner iff no Black piece stands on
// a higher row (D9: it can then never be captured or blocked, and can advance
// every move). With d = SIZE-1 - maxW moves to go, D14 gives: White TO MOVE
// wins outright if no Black piece is within d-1 rows of Black's goal
// (minB >= d) and White has at least d pieces (a capture-all defense needs one
// capture per piece). When White is NOT to move, Black gets one extra reply,
// so both margins tighten by one (minB >= d+1, whiteCount >= d+1): after any
// Black move, minB and whiteCount each drop by at most one while the runner
// stays passed and uncapturable, re-establishing the to-move conditions, so
// the win follows by induction on D14. Mirrored for Black. The two sides'
// win conditions are mutually exclusive (they imply contradictory bounds on
// the board), so check order does not matter. Returns the same WhiteWin /
// BlackWin sentinels as nearWinCheck.
static int raceWinCheck(int turnColor, int maxW, int minW, int maxB, int minB) {
    if (maxW < 0 || minB < 0) return 0;   // a side has no pieces: already decided (A9/D6)
    if (maxB <= maxW) {                   // White has a passed runner
        int d = SIZE-1 - maxW;
        int need = d + ((turnColor == White) ? 0 : 1);
        if (d >= 1 && minB >= need && g_whiteCount >= need) return WhiteWin;
    }
    if (minW >= minB) {                   // Black has a passed runner
        int d = minB;
        int need = d + ((turnColor == Black) ? 0 : 1);
        if (d >= 1 && (SIZE-1) - maxW >= need && g_blackCount >= need) return BlackWin;
    }
    return 0;
}

// Race: the soft race-distance differential (positive = White's closest piece
// is nearer its goal than Black's closest is to Black's). The cheap D14 proxy
// from todo.md, ignoring passedness and tactics.
static inline int raceTermContrib(int raceW, int maxW, int minB) {
    if (raceW == 0 || maxW < 0 || minB < 0) return 0;
    return raceW * (minB - (SIZE-1 - maxW));
}

// Full positional scan (structure + forward + the Advanced per-square and
// per-column terms) for the current board and weights. This is the part of a
// leaf score that g_evalPos caches during search. Race / RaceWin are NOT here:
// they are min/max reads, not sums, so they live in the leaf (evalLeaf /
// evalAdvanced) fed by row extremes.
int evalPosFull(const int* p, int paramCount) {
    int wallW = p[2], colW = p[3];
    int fwdW  = (paramCount > 4) ? p[4] : 0;
    bool adv  = (paramCount >= ADV_PARAMS);
    int supW  = adv ? p[ADV_SUPPORT]   : 0;
    int ctrW  = adv ? p[ADV_CENTER]    : 0;
    int mobW  = adv ? p[ADV_MOBILITY]  : 0;
    int holeW = adv ? p[ADV_HOLE]      : 0;
    int conW  = adv ? p[ADV_CONTROL]   : 0;
    int openW = adv ? p[ADV_OPEN]      : 0;
    int oxW   = adv ? p[ADV_OVEREXT]   : 0;
    int noiW  = adv ? p[ADV_NOISE]     : 0;
    int seed  = adv ? p[ADV_NOISESEED] : 0;

    int s = 0, x, y;
    if (wallW != 0 || colW != 0)
        for (y = 0; y < SIZE-1; y++)
            for (x = 0; x < SIZE-1; x++)
                s += structOwner(x, y, wallW, colW);
    if (fwdW != 0)
        for (y = 0; y < SIZE; y++)
            for (x = 0; x < SIZE; x++)
                s += forwardContrib(x, y, fwdW);
    if (supW != 0)
        for (y = 0; y < SIZE-1; y++)
            for (x = 0; x < SIZE; x++)
                s += diagOwner(x, y, supW);
    if (ctrW != 0 || conW != 0 || noiW > 0)
        for (y = 0; y < SIZE; y++)
            for (x = 0; x < SIZE; x++) {
                if (ctrW != 0) s += centerContrib(x, y, ctrW);
                if (conW != 0) s += controlContrib(x, y, conW);
                if (noiW > 0) s += noiseContrib(x, y, noiW, seed);   // PST form only; noiW < 0 = jitter, a leaf term
            }
    if (mobW != 0 || oxW != 0)
        for (y = 0; y < SIZE; y++)
            for (x = 0; x < SIZE; x++) {
                if (mobW != 0) s += mobilityContrib(x, y, mobW);
                if (oxW  != 0) s += overextContrib(x, y, oxW);
            }
    if (holeW != 0)
        for (x = 0; x < SIZE; x++)
            s += holeColContrib(x, true, holeW) + holeColContrib(x, false, holeW);
    if (openW != 0)
        for (x = 0; x < SIZE; x++)
            s += openFileContrib(x, openW);
    return s;
}

// ============================================================
// Incremental search state
// ============================================================
// During a minimax search the positional score is kept up to date in g_evalPos
// instead of rescanning the board at every leaf. evalBeginSearch seeds it from
// the root board; simulate/unsimulate call evalPosLocal to apply each move's
// local delta; evalLeaf combines it with the (already-incremental) chip diff and
// the turn term. Only evaluators flagged `incremental` use this path; others fall
// back to a full evaluateBoard at the leaf.

// Sum of the (up to 4) orthogonally-adjacent same-color pairs that touch square
// (x,y), counted under the SAME single-ownership convention as evalPosFull /
// structOwner: a pair is scored once, "owned" by its lower-left cell, and only if
// that owner lies in [0, SIZE-2] on both axes. That excludes top-row walls and
// rightmost-column columns exactly as the full scan does, so summing this over
// the two squares a move changes reproduces evalPosFull's total incrementally.
static inline int neighborStruct(int x, int y, int wallW, int colW) {
    int s = 0;
    // Right pair (x,y)-(x+1,y), owner (x,y).
    if (x <= SIZE-2 && y <= SIZE-2) s += pairContrib(x, y, x+1, y, wallW, colW);
    // Left pair (x-1,y)-(x,y), owner (x-1,y).
    if (x-1 >= 0    && y <= SIZE-2) s += pairContrib(x-1, y, x, y, wallW, colW);
    // Upper pair (x,y)-(x,y+1), owner (x,y).
    if (x <= SIZE-2 && y <= SIZE-2) s += pairContrib(x, y, x, y+1, wallW, colW);
    // Lower pair (x,y-1)-(x,y), owner (x,y-1).
    if (x <= SIZE-2 && y-1 >= 0)    s += pairContrib(x, y-1, x, y, wallW, colW);
    return s;
}

// Positional contribution of just the cells affected by a one-step move between
// (sx,sy) and (dx,dy): a true neighbor-local structure delta (the orthogonal
// pairs touching the two changed squares) plus the per-square forward score of
// those squares. Called once before and once after the board mutation; the
// difference is the move's positional delta. Both squares' four-neighbor pairs
// are summed: for a diagonal move the two squares are not orthogonally adjacent
// so no pair is shared; for a straight move they are adjacent, but a straight
// move only lands on an empty square (dest empty before, source empty after), so
// the shared source-dest pair is 0 in both passes and cancels. Every other
// changed pair is counted once per pass, so the before/after subtraction yields
// exactly the move's structure delta (guarded by the equivalence test in
// tests/test_eval.cpp).
int evalPosLocal(int sx, int sy, int dx, int dy) {
    const int* p = g_activeParams;
    int wallW = p[2], colW = p[3];
    int fwdW  = (g_activeParamCount > 4) ? p[4] : 0;

    int s = 0;
    if (wallW != 0 || colW != 0)
        s += neighborStruct(sx, sy, wallW, colW) + neighborStruct(dx, dy, wallW, colW);
    if (fwdW != 0)
        s += forwardContrib(sx, sy, fwdW) + forwardContrib(dx, dy, fwdW);
    if (g_activeParamCount >= ADV_PARAMS) {
        int supW  = p[ADV_SUPPORT],  ctrW  = p[ADV_CENTER], mobW = p[ADV_MOBILITY];
        int holeW = p[ADV_HOLE],     conW  = p[ADV_CONTROL], openW = p[ADV_OPEN];
        int oxW   = p[ADV_OVEREXT],  noiW  = p[ADV_NOISE],  seed = p[ADV_NOISESEED];
        if (supW != 0)
            s += supportNeighbors(sx, sy, supW) + supportNeighbors(dx, dy, supW);
        if (ctrW != 0)
            s += centerContrib(sx, sy, ctrW) + centerContrib(dx, dy, ctrW);
        if (conW != 0)
            s += controlContrib(sx, sy, conW) + controlContrib(dx, dy, conW);
        if (noiW > 0)   // PST form only; noiW < 0 = jitter, tracked by g_noiseAcc instead
            s += noiseContrib(sx, sy, noiW, seed) + noiseContrib(dx, dy, noiW, seed);
        if (mobW != 0 || oxW != 0)
            s += boxTermsContrib(sx, sy, dx, dy, mobW, oxW);
        if (holeW != 0)
            s += holeLocalContrib(sx, sy, dx, dy, holeW);
        if (openW != 0)
            s += openLocalContrib(sx, dx, openW);
    }
    return s;
}

// Forward declarations (defined below with the registry) so evalBeginSearch and
// evalLeaf can recognize the LearnedValue and Advanced evaluators without
// depending on agents.cpp (which not every binary links).
static int evalLearnedValue(int turnColor, const int* p);
static int evalAdvanced(int turnColor, const int* p);

// True when any weight the g_evalPos accumulator would maintain is nonzero.
// With all of them zero the accumulator is pure overhead (the chip-count speed
// study measured +18 to +35% us/move at the champion's w0,l0 weights), so
// evalBeginSearch leaves the incremental path off and the leaf falls back to
// the full evaluator, whose scans all short-circuit on zero weights.
static bool posWeightsActive(const int* p, int paramCount) {
    if (p[2] != 0 || p[3] != 0) return true;
    if (paramCount > 4 && p[4] != 0) return true;
    if (paramCount >= ADV_PARAMS)
        return p[ADV_SUPPORT] != 0 || p[ADV_CENTER] != 0 || p[ADV_MOBILITY] != 0
            || p[ADV_HOLE] != 0 || p[ADV_CONTROL] != 0 || p[ADV_OPEN] != 0
            || p[ADV_OVEREXT] != 0 || p[ADV_NOISE] > 0;   // jitter (< 0) is not a g_evalPos term
    return false;
}

void evalBeginSearch(int evaluator, const int* params) {
    if (evaluator < 0 || evaluator >= g_evalCount) evaluator = 0;
    // g_evalLevel < 3 (benchmark-only) forces the non-incremental leaf so the
    // g_evalPos accumulator is neither seeded nor maintained in make/unmake:
    // a reconstructed older level must not pay to maintain state it ignores.
    // The same rule gates on the weights themselves: an all-zero positional mix
    // would maintain an accumulator that is always 0.
    g_evalIncremental  = g_evaluators[evaluator].incremental && g_evalLevel >= 3
                       && posWeightsActive(params, g_evaluators[evaluator].paramCount);
    g_activeParams     = params;
    g_activeParamCount = g_evaluators[evaluator].paramCount;
    if (g_evalIncremental) g_evalPos = evalPosFull(params, g_activeParamCount);
    // The Advanced Race/RaceWin terms read row extremes at every leaf; maintain
    // per-row piece counts across make/unmake instead of rescanning. Gated on
    // the same benchmark-level rule and on the terms actually being enabled.
    g_evalRowCounts = (g_evaluators[evaluator].fn == evalAdvanced) && g_evalLevel >= 3
                    && (params[ADV_RACE] != 0 || params[ADV_RACEWIN] != 0);
    if (g_evalRowCounts) {
        for (int y = 0; y < SIZE; y++) { g_rowCountW[y] = 0; g_rowCountB[y] = 0; }
        for (int y = 0; y < SIZE; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] == WHITE)      g_rowCountW[y]++;
                else if (board[x][y] == BLACK) g_rowCountB[y]++;
            }
    }
    // The bounded jitter (Noise < 0) reads the raw hash sum at every leaf;
    // maintain it across make/unmake instead of rescanning (same level rule).
    g_noiseIncremental = (g_evaluators[evaluator].fn == evalAdvanced) && g_evalLevel >= 3
                       && params[ADV_NOISE] < 0;
    if (g_noiseIncremental) {
        g_noiseSeed = params[ADV_NOISESEED];
        g_noiseAcc = noiseRawScan(g_noiseSeed);
    }
    // LearnedValue with a sparse piece-square model (feature v2) gets its own
    // accumulator, maintained by the same make/unmake hooks as g_evalPos. Any
    // other model leaves g_mlIncremental false and the leaf falls back to the
    // full-scan mlValueScore path.
    if (g_evaluators[evaluator].fn == evalLearnedValue)
        mlIncrementalBegin(params[0]);
}

void evalEndSearch() {
    g_evalIncremental = false;
    g_evalRowCounts = false;
    g_noiseIncremental = false;
    g_activeParams = nullptr;
    g_activeParamCount = 0;
    mlIncrementalEnd();
}

// Fast leaf score for minimax. When an incremental search is active, reuse the
// maintained accumulators (g_mlAcc for a sparse learned model, g_evalPos for
// the heuristic evaluators, g_rowCountW/B for the Advanced Race/RaceWin leaf
// terms); otherwise fall back to the full evaluator. Incremental heuristic
// evaluators promise the standard layout: p[0]=turn, p[1]=chip.
int evalLeaf(int turnColor, int evaluator, const int* p) {
    if (g_mlIncremental) return mlLeafScore(turnColor);
    bool adv = (g_evaluators[evaluator].fn == evalAdvanced);
    if (!g_evalIncremental && !(adv && (g_evalRowCounts || g_noiseIncremental))) {
        // Level 1 (benchmark-only reconstruction of the pre-incremental leaf):
        // full-board chipDiff() rescan instead of the g_chipDiff counter, plus
        // the same nearWinCheck / turn / evalPosFull terms as the evaluator fn,
        // so levels differ ONLY in how the chip term is obtained. Heuristic
        // evaluators only (the standard p[0]=turn, p[1]=chip layout).
        if (g_evalLevel == 1 && g_evaluators[evaluator].incremental) {
            int nw = nearWinCheck(turnColor);
            if (nw) return nw;
            int race = 0;
            if (adv && (p[ADV_RACEWIN] != 0 || p[ADV_RACE] != 0)) {
                int maxW, minW, maxB, minB;
                rowExtremesByScan(maxW, minW, maxB, minB);
                if (p[ADV_RACEWIN] != 0) {
                    int rw = raceWinCheck(turnColor, maxW, minW, maxB, minB);
                    if (rw) return rw;
                }
                race = raceTermContrib(p[ADV_RACE], maxW, minB);
            }
            int turnTerm = (turnColor == White) ? p[0] : -p[0];
            int s = chipDiff() * p[1] + turnTerm
                  + evalPosFull(p, g_evaluators[evaluator].paramCount) + race;
            if (adv && p[ADV_NOISE] < 0)
                return s * ADV_JITTER_SCALE
                     + jitterValue(noiseRawScan(p[ADV_NOISESEED]), -p[ADV_NOISE]);
            return s;
        }
        return evaluateBoard(turnColor, evaluator, p);
    }
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    int race = 0;
    if (adv && g_evalRowCounts) {
        int maxW, minW, maxB, minB;
        rowExtremesFromCounts(maxW, minW, maxB, minB);
        if (p[ADV_RACEWIN] != 0) {
            int rw = raceWinCheck(turnColor, maxW, minW, maxB, minB);
            if (rw) return rw;
        }
        race = raceTermContrib(p[ADV_RACE], maxW, minB);
    }
    int turnTerm = (turnColor == White) ? p[0] : -p[0];
    int pos = g_evalIncremental ? g_evalPos
            : evalPosFull(p, g_evaluators[evaluator].paramCount);
    int s = g_chipDiff * p[1] + turnTerm + pos + race;
    if (adv && p[ADV_NOISE] < 0) {
        unsigned long long raw = g_noiseIncremental ? g_noiseAcc
                                                    : noiseRawScan(p[ADV_NOISESEED]);
        return s * ADV_JITTER_SCALE + jitterValue(raw, -p[ADV_NOISE]);
    }
    return s;
}

// ============================================================
// The evaluators
// ============================================================
// "Classic": the original heuristic. p[0]=turn, p[1]=chip, p[2]=wall, p[3]=column.
static int evalClassic(int turnColor, const int* p) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    int turnTerm = (turnColor == White) ? p[0] : -p[0];
    return g_chipDiff * p[1] + turnTerm + evalPosFull(p, 4);
}

// "Experimental": Classic plus p[4] = forward weight (rewarding piece
// forwardness). Identical to Classic when p[4] == 0. A worked template: edit this
// body and its registry entry below to build your own evaluator.
static int evalExperimental(int turnColor, const int* p) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    int turnTerm = (turnColor == White) ? p[0] : -p[0];
    return g_chipDiff * p[1] + turnTerm + evalPosFull(p, 5);
}

// "Advanced": Experimental plus the feature terms of the ADV_* layout (see the
// Advanced-evaluator terms section above). Identical to Experimental when
// every added weight is 0 (and to Classic when forward is 0 too). Race and
// RaceWin are not part of the g_evalPos positional sum (they need the board's
// row extremes, a min/max rather than a local sum), so this full path scans
// for them here while the incremental leaf reads them from the g_rowCountW/B
// counters (see evalLeaf).
static int evalAdvanced(int turnColor, const int* p) {
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    int race = 0;
    if (p[ADV_RACEWIN] != 0 || p[ADV_RACE] != 0) {
        int maxW, minW, maxB, minB;
        rowExtremesByScan(maxW, minW, maxB, minB);
        if (p[ADV_RACEWIN] != 0) {
            int rw = raceWinCheck(turnColor, maxW, minW, maxB, minB);
            if (rw) return rw;
        }
        race = raceTermContrib(p[ADV_RACE], maxW, minB);
    }
    int turnTerm = (turnColor == White) ? p[0] : -p[0];
    int s = g_chipDiff * p[1] + turnTerm + evalPosFull(p, ADV_PARAMS) + race;
    // Noise < 0: bounded per-position jitter, tie-only via the x256 scaling
    // (see the jitter block in the Advanced-evaluator terms section).
    if (p[ADV_NOISE] < 0)
        return s * ADV_JITTER_SCALE
             + jitterValue(noiseRawScan(p[ADV_NOISESEED]), -p[ADV_NOISE]);
    return s;
}

// "LearnedValue": delegates to a machine-learned value model. p[0] = model slot
// (see ml_eval.h). Its registry `incremental` flag stays false (that flag drives
// the heuristic g_evalPos path). Instead, when the slot holds a sparse
// piece-square model (feature v2), evalBeginSearch enables the separate g_mlAcc
// accumulator and evalLeaf reads it via mlLeafScore; any other model (feature v1
// aggregates) runs this full feature scan at each leaf via the evalLeaf fallback.
static int evalLearnedValue(int turnColor, const int* p) {
    return mlValueScore(turnColor, p[0]);
}

// Evaluator registry. Add an evaluator by appending an EvalDef here and writing its
// function above; both the console and GUI pick it up automatically. Set
// `incremental` true only if the evaluator follows the standard layout
// (p[0]=turn, p[1]=chip, p[2]=wall, p[3]=column, optional p[4]=forward, and,
// when paramCount >= ADV_PARAMS, the full Advanced slot layout) so the shared
// g_evalPos accumulator scores it correctly.
const EvalDef g_evaluators[] = {
    { "Classic", 4, {
        { "Turn",   "turn",   1, 0, 10 },
        { "Chip",   "chip",   4, 0, 10 },
        { "Wall",   "wall",   0, 0, 10 },
        { "Column", "column", 0, 0, 10 },
      }, evalClassic, true },
    { "Experimental", 5, {
        { "Turn",    "turn",   1, 0, 10 },
        { "Chip",    "chip",   4, 0, 10 },
        { "Wall",    "wall",   0, 0, 10 },
        { "Column",  "column", 0, 0, 10 },
        { "Forward", "forward", 1, 0, 10 },
      }, evalExperimental, true },
    { "LearnedValue", 1, {
        { "Model", "model", 0, 0, ML_SLOTS-1 },
      }, evalLearnedValue, false },
    // Advanced is appended AFTER LearnedValue so existing registry indices stay
    // stable: minimax_params.txt persists the evaluator as a raw index
    // (board_io.cpp loadMinimaxParams), and saved files carry eval=2 for
    // LearnedValue.
    { "Advanced", ADV_PARAMS, {
        { "Turn",      "turn",      1,   0, 99 },
        { "Chip",      "chip",      4, -99, 99 },
        { "Wall",      "wall",      0, -99, 99 },
        { "Column",    "column",    0, -99, 99 },
        { "Forward",   "forward",   0, -99, 99 },
        { "Support",   "support",   0, -99, 99 },
        { "Center",    "center",    0, -99, 99 },
        { "Mobility",  "mobility",  0, -99, 99 },
        { "Hole",      "hole",      0, -99, 99 },
        { "Control",   "control",   0, -99, 99 },
        { "Open",      "open",      0, -99, 99 },
        { "Race",      "race",      0, -99, 99 },
        { "Overext",   "overext",   0, -99, 99 },
        { "Noise",     "noise",     0, -99, 99 },   // >0 PST noise, <0 bounded tie-only jitter of magnitude -n
        { "NoiseSeed", "noiseseed", 0,   0, 9999 },
        { "RaceWin",   "racewin",   1,   0, 1 },
      }, evalAdvanced, true },
};
const int g_evalCount = (int)(sizeof(g_evaluators) / sizeof(g_evaluators[0]));

// ============================================================
// DISPATCH + DISPLAY -- evaluateBoard / immediateEvalForDisplay
// ============================================================
// Dispatcher: score the board with the chosen evaluator (index clamped to valid range).
int evaluateBoard(int turnColor, int evaluator, const int* p) {
    if (evaluator < 0 || evaluator >= g_evalCount) evaluator = 0;
    return g_evaluators[evaluator].fn(turnColor, p);
}

// Convenience overload for the original four-weight call (used by tests and any
// caller that just wants the Classic heuristic).
int evaluateBoard(int turnColor, int turn, int chip, int wall, int col) {
    int p[MAX_EVAL_PARAMS] = { turn, chip, wall, col };
    return evalClassic(turnColor, p);
}

// White-centric static eval of the current board for UI display. A MiniMax side
// uses its own evaluator and weights; everyone else (human / random, whose param
// arrays may be all-zero and meaningless) falls back to Classic with the
// registry-default weights so the number is still meaningful. turnColor is fixed
// to White so the sign convention stays consistent (positive favors White).
int immediateEvalForDisplay(bool isMiniMax, int evaluator, const int* params) {
    if (isMiniMax)
        return evaluateBoard(White, evaluator, params);
    int defs[MAX_EVAL_PARAMS] = { 0 };
    for (int i = 0; i < g_evaluators[0].paramCount; i++)
        defs[i] = g_evaluators[0].params[i].def;
    return evaluateBoard(White, 0, defs);
}
