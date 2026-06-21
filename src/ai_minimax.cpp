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

int miniMaxWhite(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for white
    nodes++;
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY, moveX2; //Best move found so far
    int eval;
    int alpha = INT_MIN; //Evaluation of this board so far
    int beta = INT_MAX;
    int victor = 0;
    bool isCapture; //Used to undo captures

    //Find the best child by corecursive call and play it:

    //Loop through every possible move to evaluate its sub-tree:
    for (int y = SIZE-2; y >= 0; y--)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) {
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(alpha, beta, 1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) { alpha = eval; moveX1 = x; moveY = y; moveX2 = z; }
            };
            if (x > 0       && board[x-1][ny] == BLACK) tryMove(x-1); //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == BLACK) tryMove(x+1); //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY) tryMove(x-1); //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY) tryMove(x+1); //Diagonal-right
            if (               board[x  ][ny] == EMPTY) tryMove(x);   //Forward
        }
    }
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
int miniMaxBlack(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for black
    nodes++;
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY, moveX2; //Best move found so far
    int eval;
    int alpha = INT_MIN;
    int beta = INT_MAX; //Evaluation of this board so far
    int victor = 0;
    bool isCapture; //Used to undo captures

    //Find the best child by corecursive call:

    //Loop through every possible move to evaluate its sub-tree:
    for (int y = 1; y <= SIZE-1; y++)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) {
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(alpha, beta, 1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) { beta = eval; moveX1 = x; moveY = y; moveX2 = z; }
            };
            if (x > 0       && board[x-1][ny] == WHITE) tryMove(x-1); //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == WHITE) tryMove(x+1); //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY) tryMove(x-1); //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY) tryMove(x+1); //Diagonal-right
            if (               board[x  ][ny] == EMPTY) tryMove(x);   //Forward
        }
    }
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
    if (level == depth) //Base case: Node is a leaf, use SEF.
    {
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
    if (level == depth) //Base case: Node is a leaf, use SEF.
    {
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
