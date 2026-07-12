#include "catch.hpp"
#include "helpers.h"
#include "ai_eval.h"   // g_evaluators / g_evalCount for per-evaluator param counts

// Recursively explore every legal move to `depth`, applying it with the engine's
// own simulate/unsimulate (the minimax make/unmake path) and counting any time the
// incrementally-maintained g_evalPos disagrees with a from-scratch evalPosFull.
// A single mismatch (after a make or after an unmake, at any node) is a bug in the
// delta math. Returns via the g_walkMismatch counter to keep Catch assertion
// counts sane despite the thousands of make/unmake pairs visited.
static int g_walkMismatch = 0;
static void walkAssert(int color, int depth, const int* params, int pc) {
    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
    if (depth <= 0) return;
    if (color == White) {
        for (int y = 0; y < SIZE-1; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != WHITE) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (z < 0 || z >= SIZE || !tryMoveQuickWhite(x, y, z)) continue;
                    bool cap = simulateMoveWhite(x, y, z);
                    walkAssert(Black, depth-1, params, pc);
                    unsimulateMoveWhite(x, y, z, cap);
                    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
                }
            }
    } else {
        for (int y = 1; y < SIZE; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != BLACK) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (z < 0 || z >= SIZE || !tryMoveQuickBlack(x, y, z)) continue;
                    bool cap = simulateMoveBlack(x, y, z);
                    walkAssert(White, depth-1, params, pc);
                    unsimulateMoveBlack(x, y, z, cap);
                    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
                }
            }
    }
}

// evaluateBoard(turnColor, turnWeight, chipDiffWeight, wallWeight, columnWeight)
// Returns WhiteWin/BlackWin immediately for near-win positions, otherwise:
//   g_chipDiff * chipDiffWeight + eval
// where eval includes +turnWeight (White) or -turnWeight (Black).
//
// Avoid placing W at y=SIZE-2 or B at y=1 in normal-eval tests;
// those trigger immediate WhiteWin/BlackWin returns.

TEST_CASE("evaluateBoard - chip differential") {
    clearBoard();

    SECTION("empty board: score equals turn weight only") {
        REQUIRE(evaluateBoard(White, 0, 1, 0, 0) == 0);
        REQUIRE(evaluateBoard(Black, 0, 1, 0, 0) == 0);
    }

    SECTION("one extra white piece: positive score") {
        board[3][3] = WHITE;
        g_chipDiff = 1;
        REQUIRE(evaluateBoard(White, 0, 1, 0, 0) == 1);
    }

    SECTION("one extra black piece: negative score") {
        board[3][4] = BLACK;
        g_chipDiff = -1;
        REQUIRE(evaluateBoard(White, 0, 1, 0, 0) == -1);
    }

    SECTION("chipDiffWeight scales the score") {
        board[3][3] = WHITE;
        board[4][3] = WHITE;
        g_chipDiff = 2;
        REQUIRE(evaluateBoard(White, 0, 3, 0, 0) == 6);
    }
}

TEST_CASE("evaluateBoard - turn weight") {
    clearBoard();

    SECTION("White's turn adds positive turn weight") {
        REQUIRE(evaluateBoard(White, 5, 0, 0, 0) == 5);
    }

    SECTION("Black's turn subtracts turn weight") {
        REQUIRE(evaluateBoard(Black, 5, 0, 0, 0) == -5);
    }
}

TEST_CASE("evaluateBoard - near-win detection") {
    clearBoard();

    SECTION("white piece at row SIZE-2 on White turn returns WhiteWin") {
        board[3][SIZE-2] = WHITE;
        g_chipDiff = 1; g_whiteCount = 1;
        REQUIRE(evaluateBoard(White, 0, 1, 0, 0) == WhiteWin);
    }

    SECTION("black piece at row 1 on Black turn returns BlackWin") {
        board[3][1] = BLACK;
        g_chipDiff = -1; g_blackCount = 1;
        REQUIRE(evaluateBoard(Black, 0, 1, 0, 0) == BlackWin);
    }
}

// The Experimental evaluator (index 1) is a copy of Classic (index 0) plus an
// "Forward" weight, so with Forward = 0 it must score identically. This guards the
// "does the same thing as the current one" requirement.
TEST_CASE("evaluateBoard - Experimental matches Classic when Advance is 0") {
    clearBoard();
    // Pieces forming wall/column structures, clear of the near-win rows.
    board[2][2] = WHITE; board[3][2] = WHITE; board[2][3] = WHITE;
    board[5][5] = BLACK; board[5][4] = BLACK;
    g_whiteCount = 3; g_blackCount = 2; g_chipDiff = 1;

    int classic[MAX_EVAL_PARAMS]      = { 1, 4, 2, 3 };     // turn, chip, wall, column
    int experimental[MAX_EVAL_PARAMS] = { 1, 4, 2, 3, 0 };  // + Advance = 0

    REQUIRE(evaluateBoard(White, 1, experimental) == evaluateBoard(White, 0, classic));
    REQUIRE(evaluateBoard(Black, 1, experimental) == evaluateBoard(Black, 0, classic));
    // The 4-arg convenience overload also routes to Classic.
    REQUIRE(evaluateBoard(White, 0, classic) == evaluateBoard(White, 1, 4, 2, 3));
}

// The incremental accumulator (g_evalPos, maintained by simulate/unsimulate during
// search) must always equal a from-scratch positional recompute, and the fast
// leaf score must equal the full evaluator. These guard the optimization against
// silent strength regressions.
TEST_CASE("incremental evaluation matches full recompute") {
    // Classic params (turn,chip,wall,column) and Experimental (+advance), with the
    // positional weights turned on so every positional term is exercised.
    int classic[MAX_EVAL_PARAMS]      = { 1, 4, 2, 3 };
    int experimental[MAX_EVAL_PARAMS] = { 1, 4, 2, 3, 1 };

    SECTION("crafted position: captures and board-edge structures") {
        // Cluster around both edges (columns 0 and 7) with immediate captures,
        // clear of the near-win rows (no White on row 6, no Black on row 1).
        char layout[SIZE][SIZE];
        for (int x = 0; x < SIZE; x++)
            for (int y = 0; y < SIZE; y++) layout[x][y] = EMPTY;
        layout[0][4] = WHITE; layout[1][4] = WHITE; layout[0][3] = WHITE;
        layout[6][4] = WHITE; layout[7][4] = WHITE; layout[7][3] = WHITE;
        layout[0][5] = BLACK; layout[1][5] = BLACK; layout[2][5] = BLACK;
        layout[6][5] = BLACK; layout[7][5] = BLACK; layout[2][4] = BLACK;

        for (int e = 0; e <= 1; e++) {
            const int *p = (e == 0) ? classic : experimental;
            int pc = g_evaluators[e].paramCount;
            setupBoard(layout);
            evalBeginSearch(e, p);
            REQUIRE(g_evalPos == evalPosFull(p, pc));
            // Fast leaf score must match the full evaluator for both sides to move.
            REQUIRE(evalLeaf(White, e, p) == evaluateBoard(White, e, p));
            REQUIRE(evalLeaf(Black, e, p) == evaluateBoard(Black, e, p));
            g_walkMismatch = 0;
            walkAssert(White, 3, p, pc);
            walkAssert(Black, 3, p, pc);
            evalEndSearch();
            REQUIRE(g_walkMismatch == 0);
        }
    }

    SECTION("dense standard position: many walls and columns") {
        REQUIRE(reloadBoard("boards\\board1.txt") == true);
        for (int e = 0; e <= 1; e++) {
            const int *p = (e == 0) ? classic : experimental;
            int pc = g_evaluators[e].paramCount;
            REQUIRE(reloadBoard("boards\\board1.txt") == true);
            evalBeginSearch(e, p);
            REQUIRE(g_evalPos == evalPosFull(p, pc));
            g_walkMismatch = 0;
            walkAssert(White, 2, p, pc);
            evalEndSearch();
            REQUIRE(g_walkMismatch == 0);
        }
    }
}

// ============================================================
// Advanced evaluator
// ============================================================
static int advIdx() {
    for (int i = 0; i < g_evalCount; i++)
        if (string(g_evaluators[i].name) == "Advanced") return i;
    return -1;
}
// Build an Advanced param array from the 16 named weights (t,c,w,l,f, d,e,m,
// h,b, o,r,x,n,s, g in registry slot order).
static void advParams(int* p, int t, int c, int w, int l, int f,
                      int d, int e, int m, int h, int b,
                      int o, int r, int x, int n, int s, int g) {
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) p[i] = 0;
    p[0] = t; p[1] = c; p[2] = w; p[3] = l; p[4] = f;
    p[5] = d; p[6] = e; p[7] = m; p[8] = h; p[9] = b;
    p[10] = o; p[11] = r; p[12] = x; p[13] = n; p[14] = s; p[15] = g;
}

TEST_CASE("Advanced matches Experimental/Classic when the added weights are 0") {
    clearBoard();
    board[2][2] = WHITE; board[3][2] = WHITE; board[2][3] = WHITE;
    board[5][5] = BLACK; board[5][4] = BLACK;
    g_whiteCount = 3; g_blackCount = 2; g_chipDiff = 1;
    int ai = advIdx();
    REQUIRE(ai >= 0);

    int classic[MAX_EVAL_PARAMS]      = { 1, 4, 2, 3 };
    int experimental[MAX_EVAL_PARAMS] = { 1, 4, 2, 3, 2 };
    int adv[MAX_EVAL_PARAMS];
    advParams(adv, 1, 4, 2, 3, 2,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0);

    REQUIRE(evaluateBoard(White, ai, adv) == evaluateBoard(White, 1, experimental));
    REQUIRE(evaluateBoard(Black, ai, adv) == evaluateBoard(Black, 1, experimental));
    adv[4] = 0;   // forward off too -> Classic
    REQUIRE(evaluateBoard(White, ai, adv) == evaluateBoard(White, 0, classic));
    REQUIRE(evaluateBoard(Black, ai, adv) == evaluateBoard(Black, 0, classic));
}

TEST_CASE("Advanced per-feature terms score crafted positions correctly") {
    int ai = advIdx();
    REQUIRE(ai >= 0);
    int p[MAX_EVAL_PARAMS];

    SECTION("support: diagonal same-color pairs, both colors, both diagonals, edges") {
        clearBoard();
        board[2][2] = WHITE; board[3][3] = WHITE; board[1][3] = WHITE;  // 2 white pairs
        board[5][5] = BLACK; board[6][6] = BLACK;                       // 1 black pair
        g_whiteCount = 3; g_blackCount = 2; g_chipDiff = 1;
        advParams(p, 0, 0, 0, 0, 0,  5, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 2*5 - 1*5);

        clearBoard();
        board[0][0] = WHITE; board[1][1] = WHITE;   // edge pair, owner (0,0)
        board[7][3] = WHITE; board[6][4] = WHITE;   // edge pair, owner (7,3) up-left
        g_whiteCount = 4; g_blackCount = 0; g_chipDiff = 4;
        REQUIRE(evaluateBoard(White, ai, p) == 2*5);
    }

    SECTION("center: forwardness scaled by file centrality") {
        clearBoard();
        board[3][4] = WHITE;   // y=4, min(3,4)=3
        g_whiteCount = 1; g_blackCount = 0; g_chipDiff = 1;
        advParams(p, 0, 0, 0, 0, 0,  0, 2, 0, 0, 0,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 2*4*3);
        board[3][4] = EMPTY; board[0][4] = WHITE;   // edge file: centrality 0
        REQUIRE(evaluateBoard(White, ai, p) == 0);
        clearBoard();
        board[4][3] = BLACK;   // 7-y=4, min(4,3)=3
        g_whiteCount = 0; g_blackCount = 1; g_chipDiff = -1;
        REQUIRE(evaluateBoard(White, ai, p) == -2*4*3);
    }

    SECTION("mobility: legal move counts, blocking, captures count") {
        clearBoard();
        board[3][3] = WHITE;   // 3 moves
        g_whiteCount = 1; g_blackCount = 0; g_chipDiff = 1;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 1, 0, 0,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 3);
        board[3][3] = EMPTY; board[0][3] = WHITE;   // edge: 2 moves
        REQUIRE(evaluateBoard(White, ai, p) == 2);
        clearBoard();
        board[3][3] = WHITE; board[4][4] = BLACK;   // white can capture (counts), fwd+diag open
        g_whiteCount = 1; g_blackCount = 1; g_chipDiff = 0;
        // white: fwd (3,4) + diag (2,4) + capture (4,4) = 3; black: fwd (4,3) + (3,3) + (5,3) = 3
        REQUIRE(evaluateBoard(White, ai, p) == 3 - 3);
        board[3][4] = BLACK; g_blackCount = 2; g_chipDiff = -1;
        // white fwd blocked (3,4)=B: diag (2,4) + capture (4,4) + capture (3,4) counts? no:
        // straight fwd needs EMPTY (blocked), diagonals (2,4) empty + (4,4) enemy = 2... plus
        // (3,4) is straight ahead, not diagonal. White = 2.
        // black (4,4): fwd (4,3) + diags (3,3)=W + (5,3) = 3; black (3,4): fwd (3,3)=W blocked,
        // diags (2,3) + (4,3) = 2. Black total 5.
        REQUIRE(evaluateBoard(White, ai, p) == 2 - 5);
    }

    SECTION("hole: unguarded back-rank columns (D10), edges included") {
        clearBoard();   // empty board: 8 holes each side, cancels
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 2, 0,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 0);
        board[0][0] = WHITE;   // guards column 1 only -> 7 white holes vs 8 black
        g_whiteCount = 1; g_blackCount = 0; g_chipDiff = 1;
        REQUIRE(evaluateBoard(White, ai, p) == (-7 + 8) * 2);
        board[0][0] = EMPTY; board[1][0] = WHITE;   // guards columns 0 and 2 -> 6 holes
        REQUIRE(evaluateBoard(White, ai, p) == (-6 + 8) * 2);
        board[4][7] = BLACK;   // guards black columns 3 and 5 -> black 6 holes
        g_blackCount = 1; g_chipDiff = 0;
        REQUIRE(evaluateBoard(White, ai, p) == (-6 + 6) * 2);
    }

    SECTION("control: occupancy of the two rows nearest the opponent's home row") {
        clearBoard();
        board[2][5] = WHITE;   // row 5 = SIZE-3: contested
        g_whiteCount = 1; g_blackCount = 0; g_chipDiff = 1;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 3,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 3);
        board[5][2] = BLACK;   // row 2: contested for Black
        g_blackCount = 1; g_chipDiff = 0;
        REQUIRE(evaluateBoard(White, ai, p) == 0);
        board[4][3] = WHITE;   // row 3: not contested
        g_whiteCount = 2; g_chipDiff = 1;
        REQUIRE(evaluateBoard(White, ai, p) == 0);
    }

    SECTION("open: file empty on own half but enemy-populated") {
        clearBoard();
        board[3][5] = WHITE; board[3][6] = BLACK;
        g_whiteCount = 1; g_blackCount = 1; g_chipDiff = 0;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  3, 0, 0, 0, 0, 0);
        // file 3: no white on rows 0-3 + black present -> open against White (-3);
        // black holds its own half of the file, so nothing against Black.
        REQUIRE(evaluateBoard(White, ai, p) == -3);
        board[3][1] = WHITE;   // white now holds its own half of file 3
        g_whiteCount = 2; g_chipDiff = 1;
        REQUIRE(evaluateBoard(White, ai, p) == 0);
    }

    SECTION("race: closest-piece distance differential") {
        clearBoard();
        board[0][5] = WHITE;                 // dW = 2
        board[5][3] = BLACK; board[6][7] = BLACK;   // dB = 3
        g_whiteCount = 1; g_blackCount = 2; g_chipDiff = -1;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 2, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 2 * (3 - 2));
        board[5][3] = EMPTY; board[5][1] = BLACK;   // dB = 1 (row 1 is fine on Black's turn view? no: use White turn)
        // Black at row 1 makes nearWinCheck(White) fire BlackWin only if unguarded;
        // guard it with whites on row 0 so the race term stays observable.
        board[4][0] = WHITE; board[6][0] = WHITE;
        g_whiteCount = 3; g_chipDiff = 1;
        REQUIRE(evaluateBoard(White, ai, p) == 2 * (1 - 2));
    }

    SECTION("overextension: advanced piece with no diagonal-behind defender") {
        clearBoard();
        board[2][5] = WHITE;   // past midline, no defender
        g_whiteCount = 1; g_blackCount = 0; g_chipDiff = 1;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 4, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == -4);
        board[1][4] = WHITE;   // defends (2,5), but is itself advanced + bare
        g_whiteCount = 2; g_chipDiff = 2;
        REQUIRE(evaluateBoard(White, ai, p) == -4);
        board[0][3] = WHITE;   // row 3: not advanced, defends (1,4)
        g_whiteCount = 3; g_chipDiff = 3;
        REQUIRE(evaluateBoard(White, ai, p) == 0);
        clearBoard();
        board[5][2] = BLACK;   // black mirror
        g_whiteCount = 0; g_blackCount = 1; g_chipDiff = -1;
        REQUIRE(evaluateBoard(White, ai, p) == 4);
    }

    SECTION("noise: deterministic per seed, inert at n=0, bounded, seed-dependent") {
        clearBoard();
        board[2][2] = WHITE; board[3][3] = WHITE; board[5][5] = BLACK; board[6][4] = BLACK;
        g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0;
        advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0);
        REQUIRE(evaluateBoard(White, ai, p) == 0);          // n=0: no noise at all
        p[13] = 5; p[14] = 1;                               // n=5, seed 1
        int v1 = evaluateBoard(White, ai, p);
        REQUIRE(evaluateBoard(White, ai, p) == v1);         // deterministic
        REQUIRE(v1 >= -5*4); REQUIRE(v1 <= 5*4);            // bounded by n * pieces
        bool anyDiff = false;
        for (int s = 2; s <= 6 && !anyDiff; s++) {
            p[14] = s;
            if (evaluateBoard(White, ai, p) != v1) anyDiff = true;
        }
        REQUIRE(anyDiff);                                   // the seed actually matters
    }
}

TEST_CASE("Advanced RaceWin detector: D14 witnesses fire, near-misses do not") {
    int ai = advIdx();
    REQUIRE(ai >= 0);
    int p[MAX_EVAL_PARAMS];
    advParams(p, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 1);   // racewin only

    SECTION("white passed runner, mover margins (d=2)") {
        clearBoard();
        board[0][5] = WHITE; board[7][0] = WHITE;           // runner d=2, Nw=2
        board[4][3] = BLACK; board[5][3] = BLACK;           // maxB=3 (passed), minB=3
        g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0;
        REQUIRE(evaluateBoard(White, ai, p) == WhiteWin);   // minB>=2, Nw>=2: fires
        int vB = evaluateBoard(Black, ai, p);               // needs 3: Nw=2 too few
        REQUIRE(vB != WhiteWin);
        REQUIRE(vB != BlackWin);
        board[6][0] = WHITE; g_whiteCount = 3; g_chipDiff = 1;
        REQUIRE(evaluateBoard(Black, ai, p) == WhiteWin);   // non-mover margin now met
    }

    SECTION("near miss: runner not passed") {
        clearBoard();
        board[0][5] = WHITE; board[7][0] = WHITE; board[6][0] = WHITE;
        board[4][3] = BLACK; board[3][6] = BLACK;           // black above the runner: maxB=6
        g_whiteCount = 3; g_blackCount = 2; g_chipDiff = 1;
        int v = evaluateBoard(White, ai, p);
        REQUIRE(v != WhiteWin);
        REQUIRE(v != BlackWin);
    }

    SECTION("near miss: a defender is within range (minB < d)") {
        clearBoard();
        board[0][4] = WHITE; board[7][0] = WHITE; board[6][0] = WHITE;  // runner d=3, Nw=3
        board[4][2] = BLACK;                                // minB=2 < 3
        g_whiteCount = 3; g_blackCount = 1; g_chipDiff = 2;
        int v = evaluateBoard(White, ai, p);
        REQUIRE(v != WhiteWin);
        REQUIRE(v != BlackWin);
    }

    SECTION("black passed runner mirror (d=2)") {
        clearBoard();
        board[5][2] = BLACK; board[0][7] = BLACK;           // runner d=2, Nb=2
        board[3][4] = WHITE; board[4][4] = WHITE;           // minW=4 (passed), 7-maxW=3
        g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0;
        REQUIRE(evaluateBoard(Black, ai, p) == BlackWin);   // mover margins met
        int vW = evaluateBoard(White, ai, p);               // needs 3: Nb=2 too few
        REQUIRE(vW != BlackWin);
        REQUIRE(vW != WhiteWin);
    }
}

TEST_CASE("capacity identity: blackCap - whiteCap == forwardSum - (SIZE-1)*chipDiff") {
    int fwdOnly[MAX_EVAL_PARAMS] = { 0, 0, 0, 0, 1 };
    SECTION("crafted board") {
        clearBoard();
        board[0][0] = WHITE; board[2][3] = WHITE; board[5][6] = WHITE;
        board[7][7] = BLACK; board[4][2] = BLACK;
        g_whiteCount = 3; g_blackCount = 2; g_chipDiff = 1;
        REQUIRE(capacityBlack() - capacityWhite()
                == evalPosFull(fwdOnly, 5) - (SIZE-1) * chipDiff());
    }
    SECTION("standard start") {
        REQUIRE(reloadBoard("boards\\board1.txt") == true);
        REQUIRE(capacityBlack() - capacityWhite()
                == evalPosFull(fwdOnly, 5) - (SIZE-1) * chipDiff());
        // Lemma B start value: 8 pieces at distance 7 + 8 at distance 6 = 104/side.
        REQUIRE(capacityWhite() == 104);
        REQUIRE(capacityBlack() == 104);
    }
}

// The Advanced incremental walk: with EVERY term enabled, the accumulator, the
// row counters, and the fast leaf must all agree with a from-scratch recompute
// at every node across make/unmake.
static void walkAssertAdv(int color, int depth, const int* params, int ai) {
    int pc = g_evaluators[ai].paramCount;
    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
    int cw[SIZE] = {0}, cb[SIZE] = {0};
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) cw[y]++;
            else if (board[x][y] == BLACK) cb[y]++;
        }
    for (int y = 0; y < SIZE; y++)
        if (g_rowCountW[y] != cw[y] || g_rowCountB[y] != cb[y]) { g_walkMismatch++; break; }
    if (evalLeaf(White, ai, params) != evaluateBoard(White, ai, params)) g_walkMismatch++;
    if (evalLeaf(Black, ai, params) != evaluateBoard(Black, ai, params)) g_walkMismatch++;
    if (depth <= 0) return;
    if (color == White) {
        for (int y = 0; y < SIZE-1; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != WHITE) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (z < 0 || z >= SIZE || !tryMoveQuickWhite(x, y, z)) continue;
                    bool cap = simulateMoveWhite(x, y, z);
                    walkAssertAdv(Black, depth-1, params, ai);
                    unsimulateMoveWhite(x, y, z, cap);
                    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
                }
            }
    } else {
        for (int y = 1; y < SIZE; y++)
            for (int x = 0; x < SIZE; x++) {
                if (board[x][y] != BLACK) continue;
                for (int z = x-1; z <= x+1; z++) {
                    if (z < 0 || z >= SIZE || !tryMoveQuickBlack(x, y, z)) continue;
                    bool cap = simulateMoveBlack(x, y, z);
                    walkAssertAdv(White, depth-1, params, ai);
                    unsimulateMoveBlack(x, y, z, cap);
                    if (g_evalPos != evalPosFull(params, pc)) g_walkMismatch++;
                }
            }
    }
}

TEST_CASE("Advanced incremental evaluation matches full recompute (all terms on)") {
    int ai = advIdx();
    REQUIRE(ai >= 0);
    int p[MAX_EVAL_PARAMS];
    // Every weight nonzero so every term's delta path is exercised, noise seeded.
    advParams(p, 1, 4, 2, 3, 1,  2, 1, 1, 3, 2,  2, 2, 2, 2, 7, 1);

    SECTION("crafted position: captures and board-edge structures") {
        char layout[SIZE][SIZE];
        for (int x = 0; x < SIZE; x++)
            for (int y = 0; y < SIZE; y++) layout[x][y] = EMPTY;
        layout[0][4] = WHITE; layout[1][4] = WHITE; layout[0][3] = WHITE;
        layout[6][4] = WHITE; layout[7][4] = WHITE; layout[7][3] = WHITE;
        layout[0][5] = BLACK; layout[1][5] = BLACK; layout[2][5] = BLACK;
        layout[6][5] = BLACK; layout[7][5] = BLACK; layout[2][4] = BLACK;

        setupBoard(layout);
        evalBeginSearch(ai, p);
        REQUIRE(g_evalPos == evalPosFull(p, g_evaluators[ai].paramCount));
        REQUIRE(evalLeaf(White, ai, p) == evaluateBoard(White, ai, p));
        REQUIRE(evalLeaf(Black, ai, p) == evaluateBoard(Black, ai, p));
        g_walkMismatch = 0;
        walkAssertAdv(White, 3, p, ai);
        walkAssertAdv(Black, 3, p, ai);
        evalEndSearch();
        REQUIRE(g_walkMismatch == 0);
    }

    SECTION("dense standard position") {
        REQUIRE(reloadBoard("boards\\board1.txt") == true);
        evalBeginSearch(ai, p);
        REQUIRE(g_evalPos == evalPosFull(p, g_evaluators[ai].paramCount));
        g_walkMismatch = 0;
        walkAssertAdv(White, 2, p, ai);
        evalEndSearch();
        REQUIRE(g_walkMismatch == 0);
    }

    SECTION("zero-weight gating: all-zero positional mix stays non-incremental") {
        REQUIRE(reloadBoard("boards\\board1.txt") == true);
        int champ[MAX_EVAL_PARAMS];
        advParams(champ, 1, 4, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0);
        evalBeginSearch(ai, champ);
        REQUIRE_FALSE(g_evalIncremental);   // nothing for the accumulator to maintain
        REQUIRE_FALSE(g_evalRowCounts);     // race/racewin off too
        REQUIRE(evalLeaf(White, ai, champ) == evaluateBoard(White, ai, champ));
        evalEndSearch();
        // Classic at the champion's w0,l0 weights gets the same gating fix.
        int cl[MAX_EVAL_PARAMS] = { 1, 4, 0, 0 };
        evalBeginSearch(0, cl);
        REQUIRE_FALSE(g_evalIncremental);
        REQUIRE(evalLeaf(White, 0, cl) == evaluateBoard(White, 0, cl));
        evalEndSearch();
    }
}
