#pragma once
#include "globals.h"

// Copy layout[x][y] into board[][] and recalculate all global counters.
// Sets PRNT=0 to silence all output during tests.
inline void setupBoard(const char layout[SIZE][SIZE]) {
    PRNT = 0;
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            board[x][y] = layout[x][y];
            if (board[x][y] == WHITE) {
                g_whiteCount++;
                g_chipDiff++;
                if (y == SIZE-1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++;
                g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
}

// Clear board to EMPTY and zero all counters.
inline void clearBoard() {
    PRNT = 0;
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            board[x][y] = EMPTY;
}

// Play a game from the current board state.
// Returns 0=White wins, 1=Black wins, -1=timeout (maxHalfMoves exceeded).
// Uses a bounded for loop; no infinite-loop risk.
inline int runGame(int whiteType, int w1, int w2, int w3, int w4, int w5,
                   int blackType, int b1, int b2, int b3, int b4, int b5,
                   int maxHalfMoves) {
    // w2..w5 / b2..b5 are the Classic evaluator weights (turn, chip, wall, column).
    int wParams[MAX_EVAL_PARAMS] = { w2, w3, w4, w5 };
    int bParams[MAX_EVAL_PARAMS] = { b2, b3, b4, b5 };
    int victor;
    for (int h = 0; h < maxHalfMoves; h++) {
        if (h % 2 == 0) {
            victor = moveWhite(whiteType, w1, 0, wParams, StandardOpener);
        } else {
            victor = moveBlack(blackType, b1, 0, bParams, StandardOpener);
        }
        if (victor == WhiteWin) return 0;
        if (victor == BlackWin) return 1;
    }
    return -1;
}
