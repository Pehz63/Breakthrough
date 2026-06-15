#include "catch.hpp"
#include "helpers.h"

// These tests call moveWhite/moveBlack with MiniMax and verify it makes the correct
// decision in positions where there is an obvious best move.
// MiniMax depth 1 is sufficient for forced-win-in-1 scenarios.

TEST_CASE("MiniMax - White forced win in 1") {
    clearBoard();
    // W at (3, SIZE-2): one step from reaching the winning row SIZE-1.
    // B at (0, 4): present so canWinWhite via g_blackCount==0 doesn't fire prematurely.
    board[3][SIZE-2] = WHITE;
    board[0][4]      = BLACK;
    g_whiteCount = 1; g_blackCount = 1; g_chipDiff = 0;

    int result = moveWhite(MiniMax, 1, 0, 1, 0, 0, StandardOpener);

    REQUIRE(result == WhiteWin);
    bool whiteAtWinRow = false;
    for (int x = 0; x < SIZE; x++)
        if (board[x][SIZE-1] == WHITE) whiteAtWinRow = true;
    REQUIRE(whiteAtWinRow == true);
}

TEST_CASE("MiniMax - Black forced win in 1") {
    clearBoard();
    // B at (3, 1): one step from reaching the winning row 0.
    // W at (0, 4): present so canWinBlack via g_whiteCount==0 doesn't fire prematurely.
    board[3][1]  = BLACK;
    board[0][4]  = WHITE;
    g_blackCount = 1; g_whiteCount = 1; g_chipDiff = 0;

    int result = moveBlack(MiniMax, 1, 0, 1, 0, 0, StandardOpener);

    REQUIRE(result == BlackWin);
    bool blackAtWinRow = false;
    for (int x = 0; x < SIZE; x++)
        if (board[x][0] == BLACK) blackAtWinRow = true;
    REQUIRE(blackAtWinRow == true);
}

TEST_CASE("MiniMax - White captures only black piece to win") {
    clearBoard();
    // W at (4, 0), B at (3, 1) only.
    // W can capture B diagonally -> g_blackCount==0 -> WhiteWin.
    // Any non-capturing move leaves B at (3,1) uncapturable and winning next turn.
    board[4][0] = WHITE;
    board[3][1] = BLACK;
    g_whiteCount = 1; g_blackCount = 1; g_chipDiff = 0;

    int result = moveWhite(MiniMax, 1, 0, 1, 0, 0, StandardOpener);

    REQUIRE(result == WhiteWin);
    REQUIRE(board[3][1] == WHITE);
    REQUIRE(board[4][0] == EMPTY);
    REQUIRE(g_blackCount == 0);
}
