#include "ai_minimax.h"
#include "moves.h"
#include "board_analysis.h"
#include "ai_random.h"
#include "ai_eval.h"

// RAII guard: seed the incremental positional accumulator from the current board
// on entry to a top-level search and tear it down on every exit path (including
// the "slower death" recursive call), so g_evalIncremental / g_activeParams never
// linger past the search.
namespace {
struct EvalSearchScope {
    EvalSearchScope(int evaluator, const int* params) { evalBeginSearch(evaluator, params); }
    ~EvalSearchScope() { evalEndSearch(); }
};
}

// Set true when a recursive node returns early because it hit g_nodeDeadline. The
// iterative-deepening driver uses it to discard an incomplete (budget-cut) iteration
// and keep the best move from the deepest iteration that finished within budget.
static bool s_budgetHit = false;

// One full root search for White to a fixed depth d. Fills the best move into
// mx/my/mz (mx = -1 if no legal move) and returns its white-centric score. Does NOT
// play the move. Shared by the single-shot and iterative-deepening drivers.
static int searchRootWhite(int d, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs) {
    int alpha = INT_MIN, beta = INT_MAX, eval; bool isCapture; mx = -1;
    for (int y = SIZE-2; y >= 0; y--)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) {
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(alpha, beta, 1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) { alpha = eval; mx = x; my = y; mz = z; }
            };
            if (x > 0       && board[x-1][ny] == BLACK) tryMove(x-1);
            if (x < SIZE-1  && board[x+1][ny] == BLACK) tryMove(x+1);
            if (x > 0       && board[x-1][ny] == EMPTY) tryMove(x-1);
            if (x < SIZE-1  && board[x+1][ny] == EMPTY) tryMove(x+1);
            if (               board[x  ][ny] == EMPTY) tryMove(x);
        }
    return alpha;
}

int miniMaxWhite(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for white
    nodes++;
    g_nodeDeadline = g_nodeBudget ? nodes + g_nodeBudget : 0; //per-move node cap (0=off)
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY = 0, moveX2 = 0; //Best move found so far
    int alpha = INT_MIN;
    int victor = 0;

    if (!g_nodeDeadline) {
        // Unbudgeted: a single full-depth search (identical to the original behavior).
        alpha = searchRootWhite(depth, evaluator, evalParams, moveX1, moveY, moveX2, nodes, leafs);
    } else {
        // Budgeted: iterative deepening. Keep the best move from the deepest iteration
        // that finished within the node budget; a partial (cut) iteration is discarded.
        for (int d = 1; d <= depth; d++) {
            s_budgetHit = false;
            int mx, my, mz;
            int a = searchRootWhite(d, evaluator, evalParams, mx, my, mz, nodes, leafs);
            if (mx == -1) break;                 // no legal move
            if (s_budgetHit && moveX1 != -1) break; // incomplete; keep the previous depth
            moveX1 = mx; moveY = my; moveX2 = mz; alpha = a;
            if (s_budgetHit) break;              // budget gone (kept this shallow result)
            if (nodes >= g_nodeDeadline) break;
            if (alpha >= WhiteWin - 1024 || alpha <= BlackWin + 1024) break; // decided
        }
    }
    if (moveX1 != -1) g_downEvalWhite = alpha; //best-line score for display
    if (moveX1 == -1)
    {
        cout << "Error finding move for miniMaxWhite.\n";
        cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);

        printBoard();
        nodesWhite += nodes;
        return tieredRandomMoveWhite();
    }

    //If in checkmate, try to find a slower death:
    if (alpha < BlackWin+1024 && depth > 1)
    {
        miniMaxWhite(depth-1, evaluator, evalParams, nodes, leafs);
        return alpha;
    }

    //Play chosen move:
    if (PRNT > 1)
        cout << "White (MiniMax) ";
    if (tryMoveWhite(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveWhite(moveX1, moveY, moveX2);
        if (PRNT > 1)
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "White (MiniMax) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);
            if (PRNT == 1)
                printBoard();
        }
    }
    if (victor == None)
    {
        if (alpha == WhiteWin)
            alpha--;
        else if (alpha == BlackWin)
            alpha++;
        nodesWhite += nodes;
        return alpha;
    }
    nodesWhite += nodes;
    return victor;
}
// One full root search for Black to a fixed depth d (minimizing white-centric score).
// Fills the best move into mx/my/mz (mx = -1 if none) and returns its score. No play.
static int searchRootBlack(int d, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs) {
    int alpha = INT_MIN, beta = INT_MAX, eval; bool isCapture; mx = -1;
    for (int y = 1; y <= SIZE-1; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) {
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(alpha, beta, 1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) { beta = eval; mx = x; my = y; mz = z; }
            };
            if (x > 0       && board[x-1][ny] == WHITE) tryMove(x-1);
            if (x < SIZE-1  && board[x+1][ny] == WHITE) tryMove(x+1);
            if (x > 0       && board[x-1][ny] == EMPTY) tryMove(x-1);
            if (x < SIZE-1  && board[x+1][ny] == EMPTY) tryMove(x+1);
            if (               board[x  ][ny] == EMPTY) tryMove(x);
        }
    return beta;
}

int miniMaxBlack(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for black
    nodes++;
    g_nodeDeadline = g_nodeBudget ? nodes + g_nodeBudget : 0; //per-move node cap (0=off)
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY = 0, moveX2 = 0; //Best move found so far
    int beta = INT_MAX;
    int victor = 0;

    if (!g_nodeDeadline) {
        beta = searchRootBlack(depth, evaluator, evalParams, moveX1, moveY, moveX2, nodes, leafs);
    } else {
        for (int d = 1; d <= depth; d++) {
            s_budgetHit = false;
            int mx, my, mz;
            int b = searchRootBlack(d, evaluator, evalParams, mx, my, mz, nodes, leafs);
            if (mx == -1) break;
            if (s_budgetHit && moveX1 != -1) break;
            moveX1 = mx; moveY = my; moveX2 = mz; beta = b;
            if (s_budgetHit) break;
            if (nodes >= g_nodeDeadline) break;
            if (beta <= BlackWin + 1024 || beta >= WhiteWin - 1024) break; // decided
        }
    }
    if (moveX1 != -1) g_downEvalBlack = beta; //best-line score for display
    if (moveX1 == -1)
    {
        cout << "Error finding move for miniMaxBlack.\n";
        cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);
        printBoard();
        nodesBlack += nodes;
        return tieredRandomMoveBlack();
    }

    //If in checkmate, try to find a slower death:
    if (beta > WhiteWin-1024 && depth > 1)
    {
        miniMaxBlack(depth-1, evaluator, evalParams, nodes, leafs);
        return beta;
    }

    //Play chosen move:
    if (PRNT > 1)
        cout << "Black (MiniMax) ";
    if (tryMoveBlack(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveBlack(moveX1, moveY, moveX2);
        if (PRNT > 1)
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "Black (MiniMax) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs);
            if (PRNT == 1)
                printBoard();
        }
    }
    //Return victory
    if (victor == None)
    {
        if (beta == WhiteWin)
            beta--;
        else if (beta == BlackWin)
            beta++;
        nodesBlack += nodes;
        return beta;
    }
    nodesBlack += nodes;
    return victor;
}
int maxAlphaBeta(int alpha, int beta, int level, int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the AI's best next move
    nodes++;
    if (level == depth || (g_nodeDeadline && nodes >= g_nodeDeadline)) //Leaf: depth reached or node budget hit
    {
        if (g_nodeDeadline && nodes >= g_nodeDeadline && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinWhite()) return WhiteWin;
        if (canWinBlack()) return BlackWin;
        return evalLeaf(White, evaluator, evalParams);
    }
    if (canWinWhite())
    {
        leafs++;
        return WhiteWin;
    }
    if (canWinBlack())
    {
        leafs++;
        return BlackWin;
    }

    int eval; //Evaluation of this board so far
    bool isCapture;

    //Find the best child by corecursive call:

    //Loop through every possible move to evaluate its sub-tree:
    for (int y = SIZE-2; y >= 0; y--)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) -> bool {
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(alpha, beta, level+1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) alpha = eval;
                return alpha >= beta;
            };
            if (x > 0       && board[x-1][ny] == BLACK && tryMove(x-1)) return beta; //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == BLACK && tryMove(x+1)) return beta; //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return beta; //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return beta; //Diagonal-right
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return beta; //Forward
        }
    }
    if (alpha > WhiteWin-1024) //If alpha is a winning move, have it decay at each level to favor longer checkmates
        alpha--;
    return alpha;
}
int minAlphaBeta(int alpha, int beta, int level, int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the opponent's best next move
    nodes++;
    if (level == depth || (g_nodeDeadline && nodes >= g_nodeDeadline)) //Leaf: depth reached or node budget hit
    {
        if (g_nodeDeadline && nodes >= g_nodeDeadline && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinBlack()) return BlackWin;
        if (canWinWhite()) return WhiteWin;
        return evalLeaf(Black, evaluator, evalParams);
    }
    if (canWinBlack())
    {
        leafs++;
        return BlackWin;
    }
    if (canWinWhite())
    {
        leafs++;
        return WhiteWin;
    }

    int eval; //Evaluation of this board so far
    bool isCapture;

    //Find the best child by corecursive call:

    //Loop through every possible move to evaluate its sub-tree:
    for (int y = 1; y <= SIZE-1; y++)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) -> bool {
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(alpha, beta, level+1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) beta = eval;
                return beta <= alpha;
            };
            if (x > 0       && board[x-1][ny] == WHITE && tryMove(x-1)) return alpha; //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == WHITE && tryMove(x+1)) return alpha; //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return alpha; //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return alpha; //Diagonal-right
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return alpha; //Forward
        }
    }
    if (beta < BlackWin+1024) //If beta is a winning move, have it decay at each level to favor longer checkmates
        beta++;
    return beta;
}
