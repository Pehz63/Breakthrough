#include "catch.hpp"
#include "transposition.h"   // ttClear (searcher-context regression test)
#include "datastore.h"       // positionKey (PV-walk salt regression test)
#include "helpers.h"
#include <cstring>
#include <climits>

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
    //
    // The position must be MIDBOARD, and that is load-bearing rather than cosmetic.
    // Both ttStore calls in minAlphaBeta are guarded by !isNearWin(best), so from a
    // position where one side already has a piece on the far rank nearly every line
    // scores as a near-win and the table is never written at all. This test used such
    // a position until 2026-08-28 and its two TT arms were therefore comparing an
    // empty table against an empty table. The nodes guard below is what keeps that
    // from silently happening again: it fails if the table did not change the search.
    clearBoard();
    int wcols[5] = {1,3,5,2,4}, wrows[5] = {2,2,2,3,3};
    int bcols[5] = {1,3,5,2,4}, brows[5] = {5,5,5,4,4};
    for (int i = 0; i < 5; i++) { board[wcols[i]][wrows[i]] = WHITE; board[bcols[i]][brows[i]] = BLACK; }
    g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;

    char snapshot[SIZE][SIZE];
    memcpy(snapshot, board, sizeof(board));

    unsigned long long lastNodes = 0;
    auto runValue = [&](bool tt, bool ord) -> int {
        memcpy(board, snapshot, sizeof(board));
        g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;
        ttClear();
        g_useTT = tt; g_useMoveOrder = ord; g_aspirationWindow = 0;
        int params[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
        moveWhite(MiniMax, 6, 0, params, StandardOpener);
        g_useTT = false; g_useMoveOrder = false;
        lastNodes = g_lastNodes;
        return g_downEvalWhite;
    };

    int base = runValue(false, false);
    unsigned long long plainNodes = lastNodes;

    REQUIRE(runValue(false, true) == base);   // move ordering only
    unsigned long long ordNodes = lastNodes;
    REQUIRE(runValue(true,  false) == base);  // transposition table only
    unsigned long long ttNodes = lastNodes;
    REQUIRE(runValue(true,  true)  == base);  // both

    // Anti-vacuity guards: each feature must actually have done something, or the
    // equality assertions above are comparing a search against a copy of itself.
    REQUIRE(ordNodes != plainNodes);
    REQUIRE(ttNodes  != plainNodes);
}

TEST_CASE("MiniMax - the transposition table never hands one searcher another's score") {
    // The TT is ONE process-wide table keyed by the position hash. A position hash
    // says WHICH position was searched. It does not say WHO searched it, so without
    // a searcher context an entry stored by one player is readable by the other:
    // a different evaluator reads a number its own value function never produced,
    // and a shallower search reads a deeper one's entries and plays above its depth.
    // ttClear runs once per GAME, not per move, so both players of a ranked game
    // really do share one table and the defect changed match results.
    //
    // The shape here matters. Two searches from the SAME root never collide, because
    // White's tree is "P then a white move" and Black's is "P then a black move", so
    // an earlier version of this test passed with the fix reverted and proved
    // nothing. The collision needs an actual game: White searches from P and PLAYS,
    // and Black then searches from the resulting P', which is a node INSIDE White's
    // tree, stored there at a remaining depth greater than Black's own.
    // Both sides in midboard, so neither canWin* short-circuit fires and both
    // searches actually run. (A position with a piece one step from the back row
    // makes moveWhite return a winning move without ever entering the search, which
    // silently makes a TT test vacuous.)
    clearBoard();
    int wcols[5] = {1,3,5,2,4}, wrows[5] = {2,2,2,3,3};
    int bcols[5] = {1,3,5,2,4}, brows[5] = {5,5,5,4,4};
    for (int i = 0; i < 5; i++) { board[wcols[i]][wrows[i]] = WHITE; board[bcols[i]][brows[i]] = BLACK; }
    g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;

    struct Snap { char sq[SIZE][SIZE]; int wc, bc, cd, we, be; };
    auto take = [](Snap& s) {
        memcpy(s.sq, board, sizeof(board));
        s.wc = g_whiteCount; s.bc = g_blackCount; s.cd = g_chipDiff;
        s.we = g_whiteAtEnd; s.be = g_blackAtEnd;
    };
    auto put = [](const Snap& s) {
        memcpy(board, s.sq, sizeof(board));
        g_whiteCount = s.wc; g_blackCount = s.bc; g_chipDiff = s.cd;
        g_whiteAtEnd = s.we; g_blackAtEnd = s.be;
    };

    Snap start; take(start);
    int whiteParams[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
    int otherParams[MAX_EVAL_PARAMS] = { 0, 97, 0, 0 };   // a very different value function
    int blackParams[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
    const int WHITE_DEPTH = 5, BLACK_DEPTH = 3;           // Black shallower, so probes hit

    // White searches from the start position and plays. Capture where that leaves
    // the board, so Black's own search can be run from the identical position
    // both with and without White's entries sitting in the table.
    auto whiteMoveTo = [&](const int* params, Snap& after) {
        ttClear();
        g_useTT = true; g_useMoveOrder = true; g_aspirationWindow = 0;
        put(start);
        moveWhite(MiniMax, WHITE_DEPTH, 0, params, StandardOpener);
        take(after);
    };

    auto blackSearch = [&](const Snap& from, bool keepWhitesEntries,
                           const int* whiteFirstParams, unsigned long long& nodesOut) -> int {
        g_useTT = true; g_useMoveOrder = true; g_aspirationWindow = 0;
        if (keepWhitesEntries) {
            ttClear();
            put(start);
            moveWhite(MiniMax, WHITE_DEPTH, 0, whiteFirstParams, StandardOpener);
        } else {
            ttClear();
        }
        put(from);
        moveBlack(MiniMax, BLACK_DEPTH, 0, blackParams, StandardOpener);
        nodesOut = g_lastNodes;
        int v = g_downEvalBlack;
        g_useTT = false; g_useMoveOrder = false;
        return v;
    };

    Snap afterSame, afterCross;
    whiteMoveTo(blackParams, afterSame);
    whiteMoveTo(otherParams, afterCross);

    unsigned long long nClean = 0, nDirty = 0;

    // 1. A prior White search with the SAME evaluator must not reach Black. They are
    //    still two different players, and this half is what made 132 of 238 mined
    //    refutation lines stop reproducing (theory 54).
    int cleanSame = blackSearch(afterSame, false, blackParams, nClean);
    int dirtySame = blackSearch(afterSame, true,  blackParams, nDirty);
    REQUIRE(dirtySame == cleanSame);
    REQUIRE(nDirty == nClean);

    // 2. Nor may one with a DIFFERENT evaluator, which additionally hands Black a
    //    score its own value function would never have produced.
    int cleanCross = blackSearch(afterCross, false, otherParams, nClean);
    int dirtyCross = blackSearch(afterCross, true,  otherParams, nDirty);
    REQUIRE(dirtyCross == cleanCross);
    REQUIRE(nDirty == nClean);
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
    //
    // Midboard for the same reason as the non-quiescence version above: from a
    // position where a piece already sits on the far rank, canWin* answers at the
    // root's children and the whole search collapses to a handful of nodes, which
    // makes every arm of this test identical for reasons that have nothing to do
    // with tt or ord. Measured on the position this test used until 2026-08-28: 17
    // nodes at depth 6, unchanged by either flag. The nodes guards below fail if
    // that ever comes back.
    clearBoard();
    int wcols[5] = {1,3,5,2,4}, wrows[5] = {2,2,2,3,3};
    int bcols[5] = {1,3,5,2,4}, brows[5] = {5,5,5,4,4};
    for (int i = 0; i < 5; i++) { board[wcols[i]][wrows[i]] = WHITE; board[bcols[i]][brows[i]] = BLACK; }
    g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;

    char snapshot[SIZE][SIZE];
    memcpy(snapshot, board, sizeof(board));

    unsigned long long lastNodes = 0;
    auto runValue = [&](bool tt, bool ord) -> int {
        memcpy(board, snapshot, sizeof(board));
        g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;
        ttClear();
        g_useTT = tt; g_useMoveOrder = ord; g_aspirationWindow = 0; g_useQuiescence = true;
        int params[MAX_EVAL_PARAMS] = { 0, 4, 2, 2 };
        moveWhite(MiniMax, 6, 0, params, StandardOpener);
        g_useTT = false; g_useMoveOrder = false; g_useQuiescence = false;
        lastNodes = g_lastNodes;
        return g_downEvalWhite;
    };

    int base = runValue(false, false);
    unsigned long long plainNodes = lastNodes;

    REQUIRE(runValue(false, true) == base);   // move ordering only
    unsigned long long ordNodes = lastNodes;
    REQUIRE(runValue(true,  false) == base);  // transposition table only
    unsigned long long ttNodes = lastNodes;
    REQUIRE(runValue(true,  true)  == base);  // both

    REQUIRE(ordNodes != plainNodes);
    REQUIRE(ttNodes  != plainNodes);
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

TEST_CASE("MiniMax - a reader outside the search must xor in ttSearchContext") {
    // The searcher context above gives each player a disjoint region of the one
    // table, which is exactly why anything OUTSIDE ai_minimax.cpp that wants to
    // read what a search stored has to apply the same salt. Probing the bare
    // position hash matches nothing.
    //
    // This is not hypothetical. TD-Leaf reconstructs the principal variation by
    // walking stored best-moves from the root outward (walkPV, src/ml_tdleaf.cpp),
    // and it probed the bare hash. From the day the salt landed until 2026-09-09
    // every one of those probes missed, the walk stopped at its first step, and
    // the regime trained on the position one ply after the root instead of on a
    // depth-d leaf: mean PV depth 1 of 12, 100% truncated. Fixing the probe took
    // the same run to 5.62 of 12. No rostered checkpoint was affected, because
    // every tdleaf core predates the salt, so this asserts the contract rather
    // than guarding a number.
    clearBoard();
    int wcols[5] = {1,3,5,2,4}, wrows[5] = {2,2,2,3,3};
    int bcols[5] = {1,3,5,2,4}, brows[5] = {5,5,5,4,4};
    for (int i = 0; i < 5; i++) { board[wcols[i]][wrows[i]] = WHITE; board[bcols[i]][brows[i]] = BLACK; }
    g_whiteCount = 5; g_blackCount = 5; g_chipDiff = 0; g_whiteAtEnd = 0; g_blackAtEnd = 0;

    int params[MAX_EVAL_PARAMS] = { 0, 100, 0, 0 };
    g_useAlphaBeta = true; g_useTT = true; g_useMoveOrder = true;
    ttClear();
    ttNewSearch();

    // Search AND play, then probe the position the move led to. The root itself
    // is never stored (searchRootWhite stores only what maxAlphaBeta/minAlphaBeta
    // write at interior nodes), so the first entry a PV walk can find is the
    // root's child, which is exactly where walkPV starts.
    moveWhite(MiniMax, 4, 0, params, StandardOpener);

    uint64_t bare = (uint64_t)positionKey(Black, false).hash;
    uint64_t salted = bare ^ ttSearchContext();
    REQUIRE(ttSearchContext() != 0ULL);

    int sc = 0, f = -1, t = -1;
    ttProbe(salted, 0, INT_MIN, INT_MAX, sc, f, t);
    CHECK(f >= 0);
    CHECK(t >= 0);

    int sc2 = 0, f2 = -1, t2 = -1;
    ttProbe(bare, 0, INT_MIN, INT_MAX, sc2, f2, t2);
    CHECK(f2 == -1);
    CHECK(t2 == -1);

    g_useAlphaBeta = false; g_useTT = false; g_useMoveOrder = false;
}
