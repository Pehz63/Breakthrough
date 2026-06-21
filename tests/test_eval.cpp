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
// "Advance" weight, so with Advance = 0 it must score identically. This guards the
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
