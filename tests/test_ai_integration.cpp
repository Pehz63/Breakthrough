#include "catch.hpp"
#include "helpers.h"
#include <cstring>

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

    int params[MAX_EVAL_PARAMS] = { 0, 1, 0, 0 };  // Classic: turn, chip, wall, column
    int result = moveWhite(MiniMax, 1, 0, params, StandardOpener);

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

    int params[MAX_EVAL_PARAMS] = { 0, 1, 0, 0 };  // Classic: turn, chip, wall, column
    int result = moveBlack(MiniMax, 1, 0, params, StandardOpener);

    REQUIRE(result == BlackWin);
    bool blackAtWinRow = false;
    for (int x = 0; x < SIZE; x++)
        if (board[x][0] == BLACK) blackAtWinRow = true;
    REQUIRE(blackAtWinRow == true);
}

TEST_CASE("MiniMax - move ordering and TT preserve the search value") {
    // A non-terminal midgame position searched to a fixed depth must yield the same
    // best-line value (g_downEvalWhite) whether or not the optional efficiency
    // features (move ordering, transposition table) are enabled - they change how the
    // tree is explored, never the exact minimax value.
    clearBoard();
    int wcols[5] = {1,3,5,2,4}, wrows[5] = {5,5,5,6,6};
    int bcols[5] = {1,3,5,2,4}, brows[5] = {2,2,2,1,1};
    for (int i = 0; i < 5; i++) { board[wcols[i]][wrows[i]] = WHITE; board[bcols[i]][brows[i]] = BLACK; }
    g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;

    char snapshot[SIZE][SIZE];
    memcpy(snapshot, board, sizeof(board));

    auto runValue = [&](bool tt, bool ord) -> int {
        memcpy(board, snapshot, sizeof(board));
        g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;
        g_useTT = tt; g_useMoveOrder = ord; g_aspirationWindow = 0;
        int params[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
        moveWhite(MiniMax, 4, 0, params, StandardOpener);
        g_useTT = false; g_useMoveOrder = false;
        return g_downEvalWhite;
    };

    int base = runValue(false, false);
    REQUIRE(runValue(false, true) == base);   // move ordering only
    REQUIRE(runValue(true,  false) == base);  // transposition table only
    REQUIRE(runValue(true,  true)  == base);  // both
}

TEST_CASE("MiniMax - White captures only black piece to win") {
    clearBoard();
    // W at (4, 0), B at (3, 1) only.
    // W can capture B diagonally -> g_blackCount==0 -> WhiteWin.
    // Any non-capturing move leaves B at (3,1) uncapturable and winning next turn.
    board[4][0] = WHITE;
    board[3][1] = BLACK;
    g_whiteCount = 1; g_blackCount = 1; g_chipDiff = 0;

    int params[MAX_EVAL_PARAMS] = { 0, 1, 0, 0 };  // Classic: turn, chip, wall, column
    int result = moveWhite(MiniMax, 1, 0, params, StandardOpener);

    REQUIRE(result == WhiteWin);
    REQUIRE(board[3][1] == WHITE);
    REQUIRE(board[4][0] == EMPTY);
    REQUIRE(g_blackCount == 0);
}
