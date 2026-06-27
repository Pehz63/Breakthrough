#include "explorers.h"
#include "ml_features.h"      // generateMoves, Move
#include "moves.h"
#include "ai_eval.h"
#include "ai_minimax.h"

// ============================================================
// GREEDY (1-ply)
// ============================================================
// Pick the move whose resulting board the evaluator scores best for `side`
// (White maximizes the white-centric score, Black minimizes). A move that reaches
// the goal row is an immediate win and is taken at once.
static int greedyExplore(int side, int evaluator, const int* params, int /*budget*/) {
    Move mv[ML_MAX_MOVES];
    int n = generateMoves(side, mv);
    if (n == 0) return (side == White) ? BlackWin : WhiteWin;   // no move = loss

    int bestIdx = 0;
    if (side == White) {
        int bestScore = INT_MIN;
        for (int i = 0; i < n; i++) {
            if (mv[i].dy == SIZE-1) { bestIdx = i; break; }     // immediate win
            bool cap = simulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx);
            int sc = evaluateBoard(Black, evaluator, params);
            unsimulateMoveWhite(mv[i].sx, mv[i].sy, mv[i].dx, cap);
            if (sc > bestScore) { bestScore = sc; bestIdx = i; }
        }
        return playMoveWhite(mv[bestIdx].sx, mv[bestIdx].sy, mv[bestIdx].dx);
    } else {
        int bestScore = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (mv[i].dy == 0) { bestIdx = i; break; }
            bool cap = simulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx);
            int sc = evaluateBoard(White, evaluator, params);
            unsimulateMoveBlack(mv[i].sx, mv[i].sy, mv[i].dx, cap);
            if (sc < bestScore) { bestScore = sc; bestIdx = i; }
        }
        return playMoveBlack(mv[bestIdx].sx, mv[bestIdx].sy, mv[bestIdx].dx);
    }
}

// ============================================================
// ALPHA-BETA (wraps the existing minimax)
// ============================================================
static int alphaBetaExplore(int side, int evaluator, const int* params, int budget) {
    unsigned long long nodes = 0, leafs = 0;
    if (budget < 1) budget = 1;
    return (side == White)
        ? miniMaxWhite(budget, evaluator, params, nodes, leafs)
        : miniMaxBlack(budget, evaluator, params, nodes, leafs);
}

// ============================================================
// REGISTRY
// ============================================================
const ExplorerDef g_explorers[] = {
    { "Greedy",    "1-ply: play the move the evaluator scores best (no lookahead).", greedyExplore   },
    { "AlphaBeta", "Alpha-beta minimax to a fixed depth (budget = depth).",          alphaBetaExplore },
};
const int g_explorerCount = (int)(sizeof(g_explorers) / sizeof(g_explorers[0]));
