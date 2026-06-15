#include "catch.hpp"
#include "helpers.h"

// tryMoveWhite(x1, y, x2, showMsg): piece at board[x1][y] moves to board[x2][y+1]
// tryMoveQuickWhite(x1, y, x2): same but skips source-piece and bounds checks on y

TEST_CASE("tryMoveWhite - valid moves") {
    clearBoard();

    SECTION("forward move to empty square") {
        board[3][3] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 3, false) == true);
    }

    SECTION("diagonal-left move to empty square") {
        board[3][3] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 2, false) == true);
    }

    SECTION("diagonal-right move to empty square") {
        board[3][3] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 4, false) == true);
    }

    SECTION("diagonal capture of black piece") {
        board[3][3] = WHITE;
        board[4][4] = BLACK;
        REQUIRE(tryMoveWhite(3, 3, 4, false) == true);
    }
}

TEST_CASE("tryMoveWhite - invalid moves") {
    clearBoard();

    SECTION("blocked by own piece (forward)") {
        board[3][3] = WHITE;
        board[3][4] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 3, false) == false);
    }

    SECTION("blocked by own piece (diagonal)") {
        board[3][3] = WHITE;
        board[4][4] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 4, false) == false);
    }

    SECTION("cannot capture straight ahead") {
        board[3][3] = WHITE;
        board[3][4] = BLACK;
        REQUIRE(tryMoveWhite(3, 3, 3, false) == false);
    }

    SECTION("destination column too far left") {
        board[3][3] = WHITE;
        REQUIRE(tryMoveWhite(3, 3, 1, false) == false);
    }

    SECTION("destination column off left edge") {
        board[0][3] = WHITE;
        REQUIRE(tryMoveWhite(0, 3, -1, false) == false);
    }

    SECTION("destination column off right edge") {
        board[7][3] = WHITE;
        REQUIRE(tryMoveWhite(7, 3, 8, false) == false);
    }

    SECTION("source row at SIZE-1 (no row above)") {
        board[3][SIZE-1] = WHITE;
        g_whiteAtEnd = 1;
        REQUIRE(tryMoveWhite(3, SIZE-1, 3, false) == false);
    }

    SECTION("no piece at source square") {
        REQUIRE(tryMoveWhite(3, 3, 3, false) == false);
    }
}

TEST_CASE("tryMoveBlack - valid moves") {
    clearBoard();

    SECTION("forward move to empty square") {
        board[3][5] = BLACK;
        REQUIRE(tryMoveBlack(3, 5, 3, false) == true);
    }

    SECTION("diagonal-left move to empty square") {
        board[3][5] = BLACK;
        REQUIRE(tryMoveBlack(3, 5, 2, false) == true);
    }

    SECTION("diagonal capture of white piece") {
        board[3][5] = BLACK;
        board[4][4] = WHITE;
        REQUIRE(tryMoveBlack(3, 5, 4, false) == true);
    }
}

TEST_CASE("tryMoveBlack - invalid moves") {
    clearBoard();

    SECTION("blocked by own piece (forward)") {
        board[3][5] = BLACK;
        board[3][4] = BLACK;
        REQUIRE(tryMoveBlack(3, 5, 3, false) == false);
    }

    SECTION("cannot capture straight ahead") {
        board[3][5] = BLACK;
        board[3][4] = WHITE;
        REQUIRE(tryMoveBlack(3, 5, 3, false) == false);
    }

    SECTION("destination column off left edge") {
        board[0][5] = BLACK;
        REQUIRE(tryMoveBlack(0, 5, -1, false) == false);
    }

    SECTION("source row at 0 (no row below)") {
        board[3][0] = BLACK;
        g_blackAtEnd = 1;
        REQUIRE(tryMoveBlack(3, 0, 3, false) == false);
    }

    SECTION("no piece at source square") {
        REQUIRE(tryMoveBlack(3, 5, 3, false) == false);
    }
}

TEST_CASE("tryMoveQuickWhite") {
    clearBoard();

    SECTION("forward to empty: valid") {
        board[3][3] = WHITE;
        REQUIRE(tryMoveQuickWhite(3, 3, 3) == true);
    }

    SECTION("diagonal to enemy (capture): valid") {
        board[3][3] = WHITE;
        board[4][4] = BLACK;
        REQUIRE(tryMoveQuickWhite(3, 3, 4) == true);
    }

    SECTION("destination column out of bounds: invalid") {
        board[0][3] = WHITE;
        REQUIRE(tryMoveQuickWhite(0, 3, -1) == false);
    }

    SECTION("destination blocked by own piece: invalid") {
        board[3][3] = WHITE;
        board[3][4] = WHITE;
        REQUIRE(tryMoveQuickWhite(3, 3, 3) == false);
    }

    SECTION("forward to enemy (no straight capture): invalid") {
        board[3][3] = WHITE;
        board[3][4] = BLACK;
        REQUIRE(tryMoveQuickWhite(3, 3, 3) == false);
    }
}

TEST_CASE("tryMoveQuickBlack") {
    clearBoard();

    SECTION("forward to empty: valid") {
        board[3][5] = BLACK;
        REQUIRE(tryMoveQuickBlack(3, 5, 3) == true);
    }

    SECTION("destination blocked by own piece: invalid") {
        board[3][5] = BLACK;
        board[3][4] = BLACK;
        REQUIRE(tryMoveQuickBlack(3, 5, 3) == false);
    }

    SECTION("forward to enemy (no straight capture): invalid") {
        board[3][5] = BLACK;
        board[3][4] = WHITE;
        REQUIRE(tryMoveQuickBlack(3, 5, 3) == false);
    }
}
