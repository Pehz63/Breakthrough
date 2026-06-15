#include "catch.hpp"
#include "helpers.h"

// Full-game outcome tests using puzzle board files loaded via reloadBoard().
// Run tests.exe from the project root so relative paths resolve correctly.
// The game loop is bounded (20 half-moves); hitting the limit fails the test.

// puzzle5.txt: 3 White vs 8 Black -- heavy Black material advantage.
// Black MiniMax (depth 2) should overpower White TieredRandom quickly.
TEST_CASE("puzzle5 - Black MiniMax beats White TieredRandom") {
    bool loaded = reloadBoard("boards\\puzzle5.txt");
    REQUIRE(loaded == true);
    PRNT = 0;

    int outcome = runGame(
        TieredRandom, 0, 0, 0, 0, 0,
        MiniMax,      2, 0, 1, 0, 0,
        20
    );
    INFO("outcome == -1 means the game timed out (did not finish in 20 half-moves)");
    REQUIRE(outcome == 1); // Black wins
}

// puzzle7.txt: 4 White vs 8 Black -- Black material advantage.
// Black MiniMax (depth 2) should win against White TieredRandom.
TEST_CASE("puzzle7 - Black MiniMax beats White TieredRandom") {
    bool loaded = reloadBoard("boards\\puzzle7.txt");
    REQUIRE(loaded == true);
    PRNT = 0;

    int outcome = runGame(
        TieredRandom, 0, 0, 0, 0, 0,
        MiniMax,      2, 0, 1, 0, 0,
        20
    );
    INFO("outcome == -1 means the game timed out (did not finish in 20 half-moves)");
    REQUIRE(outcome == 1); // Black wins
}

// Synthetic position: White near-win advantage.
// 3 White pieces at y=5 (two steps from winning row), only 1 Black at y=2.
// White MiniMax (depth 2) should advance and win well within 20 half-moves.
TEST_CASE("White MiniMax beats Black TieredRandom from winning position") {
    clearBoard();
    // White pieces close to the win row (y=7); black far from their win row (y=0).
    board[2][5] = WHITE; board[3][5] = WHITE; board[4][5] = WHITE;
    board[3][2] = BLACK;
    g_whiteCount = 3; g_blackCount = 1; g_chipDiff = 2;
    PRNT = 0;

    int outcome = runGame(
        MiniMax,      2, 0, 1, 0, 0,
        TieredRandom, 0, 0, 0, 0, 0,
        20
    );
    INFO("outcome == -1 means the game timed out (did not finish in 20 half-moves)");
    REQUIRE(outcome == 0); // White wins
}
