#include "catch.hpp"
#include "helpers.h"

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
