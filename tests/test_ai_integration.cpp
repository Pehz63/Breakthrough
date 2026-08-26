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

TEST_CASE("MiniMax - quiescence resolves the leaf exchange (horizon fix)") {
    // W c5 can capture B b6, but b6 is defended by B a7: the exchange is even.
    // A depth-1 search sees only the capture's +1 material at its leaf; with
    // quiescence the recapture is resolved, so the root value drops to equal
    // material. The move can stay the same - the claim is about the VALUE.
    // W f3 exists so White has quiet alternatives (which hang nothing once c5
    // trades itself off; keeping c5 in place is punished by qs via b6xc5).
    clearBoard();
    board[2][4] = WHITE;   // c5
    board[5][2] = WHITE;   // f3
    board[1][5] = BLACK;   // b6 (capturable by c5)
    board[0][6] = BLACK;   // a7 (defends b6)
    g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0;

    int params[MAX_EVAL_PARAMS] = { 0, 4, 0, 0 };  // Classic: turn 0, chip 4

    char snapshot[SIZE][SIZE];
    memcpy(snapshot, board, sizeof(board));
    auto rootValue = [&](bool qs) -> int {
        memcpy(board, snapshot, sizeof(board));
        g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;
        g_useQuiescence = qs;
        moveWhite(MiniMax, 1, 0, params, StandardOpener);
        g_useQuiescence = false;
        return g_downEvalWhite;
    };

    REQUIRE(rootValue(false) == 4);   // horizon: the capture looks like +1 chip
    REQUIRE(rootValue(true)  == 0);   // qs: the recapture resolves it to equal
}

TEST_CASE("MiniMax - quiescence value is search-path invariant (tt/ord)") {
    // With quiescence enabled, move ordering and the transposition table must
    // still preserve the exact search value, like they do for the plain leaf.
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
        g_useTT = tt; g_useMoveOrder = ord; g_aspirationWindow = 0; g_useQuiescence = true;
        int params[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
        moveWhite(MiniMax, 4, 0, params, StandardOpener);
        g_useTT = false; g_useMoveOrder = false; g_useQuiescence = false;
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

// ============================================================
// ROOT-MOVE FILTER (globals.h g_useRootFilter, the cbook opener's mechanism)
// ============================================================
// The filter is the whole point of "filter mode": narrow the root move list and
// let the same budget go deeper on what is left. These tests check the two
// things a caller depends on, at two settings each, per the project's
// instrument-validation rule: the filter changes what the search does, and it
// cannot make the search return a move outside the whitelist.

TEST_CASE("root filter - restricts the chosen move and the nodes searched") {
    clearBoard();
    // A wide-open midboard position, so several root moves are available and the
    // unrestricted search has a genuine choice to be constrained away from.
    board[2][2] = WHITE; board[4][2] = WHITE; board[6][2] = WHITE;
    board[2][5] = BLACK; board[4][5] = BLACK; board[6][5] = BLACK;
    g_whiteCount = 3; g_blackCount = 3; g_chipDiff = 0;
    char snap[SIZE][SIZE];
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];

    int params[MAX_EVAL_PARAMS] = { 0, 1, 0, 0 };
    unsigned long long nodesOff = 0, leafsOff = 0, nodesOn = 0, leafsOn = 0;

    g_useRootFilter = false;
    miniMaxWhite(4, 0, params, nodesOff, leafsOff);

    // Setting 2: one single allowed root move, deliberately not the piece the
    // unrestricted search would most likely prefer.
    setupBoard(snap);
    g_rootMoveWhitelist[0][0] = 2; g_rootMoveWhitelist[0][1] = 2; g_rootMoveWhitelist[0][2] = 1;
    g_rootMoveWhitelistCount = 1;
    g_useRootFilter = true;
    miniMaxWhite(4, 0, params, nodesOn, leafsOn);
    g_useRootFilter = false;

    // One root move instead of nine must cost strictly fewer nodes.
    REQUIRE(nodesOn < nodesOff);
}

TEST_CASE("root filter - rootMoveAllowed is inert when the flag is off") {
    g_useRootFilter = false;
    g_rootMoveWhitelistCount = 1;
    g_rootMoveWhitelist[0][0] = 0; g_rootMoveWhitelist[0][1] = 0; g_rootMoveWhitelist[0][2] = 0;
    // Off: everything is allowed, including a move not on the list.
    REQUIRE(rootMoveAllowed(5, 5, 5) == true);
    g_useRootFilter = true;
    REQUIRE(rootMoveAllowed(5, 5, 5) == false);
    REQUIRE(rootMoveAllowed(0, 0, 0) == true);
    g_useRootFilter = false;
}

TEST_CASE("root filter - the search plays a whitelisted move, not its free choice") {
    clearBoard();
    // White can capture at (3,3) from (2,2), which a chip-weighted search prefers.
    // Whitelisting only the quiet (6,2)->(6,3) advance must override that.
    board[2][2] = WHITE; board[6][2] = WHITE;
    board[3][3] = BLACK; board[0][6] = BLACK;
    g_whiteCount = 2; g_blackCount = 2; g_chipDiff = 0;
    char snap[SIZE][SIZE];
    for (int y = 0; y < SIZE; y++) for (int x = 0; x < SIZE; x++) snap[x][y] = board[x][y];

    int params[MAX_EVAL_PARAMS] = { 0, 1, 0, 0 };
    g_useRootFilter = false;
    moveWhite(MiniMax, 3, 0, params, StandardOpener);
    const bool tookCaptureUnfiltered = (board[3][3] == WHITE);

    setupBoard(snap);
    g_rootMoveWhitelist[0][0] = 6; g_rootMoveWhitelist[0][1] = 2; g_rootMoveWhitelist[0][2] = 6;
    g_rootMoveWhitelistCount = 1;
    g_useRootFilter = true;
    moveWhite(MiniMax, 3, 0, params, StandardOpener);
    g_useRootFilter = false;

    REQUIRE(tookCaptureUnfiltered);          // unfiltered: it grabs the piece
    REQUIRE(board[6][3] == WHITE);           // filtered: it plays the only allowed move
    REQUIRE(board[3][3] == BLACK);           // and leaves the capture on the board
}
