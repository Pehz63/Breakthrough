#include "catch.hpp"
#include "helpers.h"

// Coordinate system reminder:
//   White starts at y=0,1 and wins by reaching y=SIZE-1 (7).
//   Black starts at y=6,7 and wins by reaching y=0.
//   findWinWhite() scans row SIZE-2 (6); findWinBlack() scans row 1.

TEST_CASE("canWinWhite") {
    clearBoard();

    SECTION("false when no white at end row and black pieces remain") {
        g_whiteAtEnd = 0;
        g_blackCount = 4;
        REQUIRE(canWinWhite() == false);
    }

    SECTION("true when a white piece is at the end row") {
        g_whiteAtEnd = 1;
        g_blackCount = 4;
        REQUIRE(canWinWhite() == true);
    }

    SECTION("true when all black pieces have been captured") {
        g_whiteAtEnd = 0;
        g_blackCount = 0;
        REQUIRE(canWinWhite() == true);
    }
}

TEST_CASE("canWinBlack") {
    clearBoard();

    SECTION("false when no black at end row and white pieces remain") {
        g_blackAtEnd = 0;
        g_whiteCount = 4;
        REQUIRE(canWinBlack() == false);
    }

    SECTION("true when a black piece is at the end row") {
        g_blackAtEnd = 1;
        g_whiteCount = 4;
        REQUIRE(canWinBlack() == true);
    }

    SECTION("true when all white pieces have been captured") {
        g_blackAtEnd = 0;
        g_whiteCount = 0;
        REQUIRE(canWinBlack() == true);
    }
}

TEST_CASE("findWinWhite") {
    clearBoard();

    SECTION("returns source column when a winning advance exists") {
        // W at (0, SIZE-2): can advance forward or diagonally-right to row SIZE-1.
        // All candidate moves store x=0, so result is deterministic regardless of rand().
        board[0][SIZE-2] = WHITE;
        REQUIRE(findWinWhite() == 0);
    }

    SECTION("returns -1 when no white piece is at row SIZE-2") {
        board[3][3] = WHITE;
        REQUIRE(findWinWhite() == -1);
    }

    SECTION("returns -1 when all three advance paths are blocked by own pieces") {
        board[1][SIZE-2] = WHITE;
        board[0][SIZE-1] = WHITE;
        board[1][SIZE-1] = WHITE;
        board[2][SIZE-1] = WHITE;
        REQUIRE(findWinWhite() == -1);
    }

    SECTION("returns source column when forward is blocked but diagonal is open") {
        // W at (0, SIZE-2), forward blocked by own piece, diagonal-right open.
        // Only valid move is diagonal-right (z=1), still stores x=0.
        board[0][SIZE-2] = WHITE;
        board[0][SIZE-1] = WHITE;
        REQUIRE(findWinWhite() == 0);
    }
}

TEST_CASE("findWinBlack") {
    clearBoard();

    SECTION("returns source column when a winning advance exists") {
        // B at (0, 1): can advance forward or diagonally-right to row 0.
        board[0][1] = BLACK;
        REQUIRE(findWinBlack() == 0);
    }

    SECTION("returns -1 when no black piece is at row 1") {
        board[3][5] = BLACK;
        REQUIRE(findWinBlack() == -1);
    }

    SECTION("returns -1 when all three advance paths are blocked by own pieces") {
        board[1][1] = BLACK;
        board[0][0] = BLACK;
        board[1][0] = BLACK;
        board[2][0] = BLACK;
        REQUIRE(findWinBlack() == -1);
    }
}
