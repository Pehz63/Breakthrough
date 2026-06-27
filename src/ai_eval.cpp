#include "ai_eval.h"
#include "board_analysis.h"
#include "ml_eval.h"

// ============================================================
// Shared evaluation pieces
// ============================================================
// The two shipped evaluators (Classic, Experimental) share the same structure:
//   leaf score = near-win shortcut, else  turn + chipDiff*chip + positional
// where "positional" is the wall/column structure terms plus (Experimental) an
// "Advance" term. The positional part is the only piece that needs a full board
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

// Per-square advance contribution: reward a piece for how far it has pushed
// toward its goal row (White up to SIZE-1, Black down to 0).
static inline int advContrib(int x, int y, int advW) {
    char c = board[x][y];
    if (c == WHITE) return advW * y;
    if (c == BLACK) return -advW * (SIZE-1 - y);
    return 0;
}

// Full positional scan (structure + advance) for the current board and weights.
// This is the part of a leaf score that g_evalPos caches during search.
int evalPosFull(const int* p, int paramCount) {
    int wallW = p[2], colW = p[3];
    int advW  = (paramCount > 4) ? p[4] : 0;
    if (wallW == 0 && colW == 0 && advW == 0) return 0;   // nothing positional to sum

    int s = 0, x, y;
    if (wallW != 0 || colW != 0)
        for (y = 0; y < SIZE-1; y++)
            for (x = 0; x < SIZE-1; x++)
                s += structOwner(x, y, wallW, colW);
    if (advW != 0)
        for (y = 0; y < SIZE; y++)
            for (x = 0; x < SIZE; x++)
                s += advContrib(x, y, advW);
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

// Positional contribution of just the cells affected by a one-step move between
// (sx,sy) and (dx,dy): a small owner bounding box for structure (a superset of
// the affected owners, so unaffected owners cancel in the before/after diff) plus
// the per-square advance of the two changed squares. Called once before and once
// after the board mutation; the difference is the move's positional delta.
int evalPosLocal(int sx, int sy, int dx, int dy) {
    const int* p = g_activeParams;
    int wallW = p[2], colW = p[3];
    int advW  = (g_activeParamCount > 4) ? p[4] : 0;

    int s = 0;
    if (wallW != 0 || colW != 0) {
        int x0 = (sx < dx ? sx : dx) - 1, x1 = (sx > dx ? sx : dx);
        int y0 = (sy < dy ? sy : dy) - 1, y1 = (sy > dy ? sy : dy);
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0;
        if (x1 > SIZE-2) x1 = SIZE-2; if (y1 > SIZE-2) y1 = SIZE-2;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                s += structOwner(x, y, wallW, colW);
    }
    if (advW != 0)
        s += advContrib(sx, sy, advW) + advContrib(dx, dy, advW);
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

// "Experimental": Classic plus p[4] = advance weight (rewarding piece
// advancement). Identical to Classic when p[4] == 0. A worked template: edit this
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
// (p[0]=turn, p[1]=chip, p[2]=wall, p[3]=column, optional p[4]=advance) so the
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
        { "Advance", "adv",    1, 0, 10 },
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
