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

// Full positional scan (structure + forward) for the current board and weights.
// This is the part of a leaf score that g_evalPos caches during search.
int evalPosFull(const int* p, int paramCount) {
    int wallW = p[2], colW = p[3];
    int fwdW  = (paramCount > 4) ? p[4] : 0;
    if (wallW == 0 && colW == 0 && fwdW == 0) return 0;   // nothing positional to sum

    int s = 0, x, y;
    if (wallW != 0 || colW != 0)
        for (y = 0; y < SIZE-1; y++)
            for (x = 0; x < SIZE-1; x++)
                s += structOwner(x, y, wallW, colW);
    if (fwdW != 0)
        for (y = 0; y < SIZE; y++)
            for (x = 0; x < SIZE; x++)
                s += forwardContrib(x, y, fwdW);
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
    return s;
}

void evalBeginSearch(int evaluator, const int* params) {
    if (evaluator < 0 || evaluator >= g_evalCount) evaluator = 0;
    g_evalIncremental  = g_evaluators[evaluator].incremental;
    g_activeParams     = params;
    g_activeParamCount = g_evaluators[evaluator].paramCount;
    if (g_evalIncremental) g_evalPos = evalPosFull(params, g_activeParamCount);
}

void evalEndSearch() {
    g_evalIncremental = false;
    g_activeParams = nullptr;
    g_activeParamCount = 0;
}

// Fast leaf score for minimax. When an incremental search is active, reuse the
// maintained g_evalPos; otherwise fall back to the full evaluator. Incremental
// evaluators promise the standard layout: p[0]=turn, p[1]=chip.
int evalLeaf(int turnColor, int evaluator, const int* p) {
    if (!g_evalIncremental) return evaluateBoard(turnColor, evaluator, p);
    int nw = nearWinCheck(turnColor);
    if (nw) return nw;
    int turnTerm = (turnColor == White) ? p[0] : -p[0];
    return g_chipDiff * p[1] + turnTerm + g_evalPos;
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

// "LearnedValue": delegates to a machine-learned value model. p[0] = model slot
// (see ml_eval.h). Non-incremental: the full feature scan runs at each leaf via the
// evalLeaf fallback path, so `incremental` must stay false in its registry entry.
static int evalLearnedValue(int turnColor, const int* p) {
    return mlValueScore(turnColor, p[0]);
}

// Evaluator registry. Add an evaluator by appending an EvalDef here and writing its
// function above; both the console and GUI pick it up automatically. Set
// `incremental` true only if the evaluator follows the standard layout
// (p[0]=turn, p[1]=chip, p[2]=wall, p[3]=column, optional p[4]=forward) so the
// shared g_evalPos accumulator scores it correctly.
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
