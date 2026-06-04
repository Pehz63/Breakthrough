#include "ai_minimax.h"
#include "moves.h"
#include "board_analysis.h"
#include "ai_random.h"

int miniMaxWhite(int depth, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for white
    nodes++;
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
            if (board[x][y] == WHITE) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveQuickWhite(x, y, z))
                    { //Move is valid, evaluate its sub-tree:
                        isCapture = simulateMoveWhite(x, y, z);
                        eval = minAlphaBeta(alpha, beta, 1, depth, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
                        unsimulateMoveWhite(x, y, z, isCapture);

                        if (eval > alpha) //If this is the best child thus far, save it
                        {
                            alpha = eval;
                            moveX1 = x;
                            moveY = y;
                            moveX2 = z;
                        }
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
        miniMaxWhite(depth-1, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
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
int miniMaxBlack(int depth, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for black
    nodes++;
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
            if (board[x][y] == BLACK) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveQuickBlack(x, y, z))
                    { //Move is valid, evaluate its sub-tree:
                        isCapture = simulateMoveBlack(x, y, z);
                        eval = maxAlphaBeta(alpha, beta, 1, depth, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
                        unsimulateMoveBlack(x, y, z, isCapture);

                        if (eval < beta) //If this is the best child thus far, save it
                        {
                            beta = eval;
                            moveX1 = x;
                            moveY = y;
                            moveX2 = z;
                        }
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
        miniMaxBlack(depth-1, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
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
int maxAlphaBeta(int alpha, int beta, int level, int depth, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the AI's best next move
    nodes++;
    if (level == depth) //Base case: Node is a leaf, use SEF.
    {
        leafs++;
        return evaluateBoard(White, turnWeight, chipDiffWeight, wallWeight, columnWeight);
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
            if (board[x][y] == WHITE) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveQuickWhite(x, y, z))
                    { //Move is valid, evaluate its sub-tree:
                        isCapture = simulateMoveWhite(x, y, z);
                        eval = minAlphaBeta(alpha, beta, level+1, depth, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
                        unsimulateMoveWhite(x, y, z, isCapture);

                        if (eval > alpha) //If this is the best child thus far, save it
                            alpha = eval;
                        if (alpha >= beta) //If this sub-tree won't be played, prune it
                            return beta;
                    }
    }
    if (alpha > WhiteWin-1024) //If alpha is a winning move, have it decay at each level to favor longer checkmates
        alpha--;
    return alpha;
}
int minAlphaBeta(int alpha, int beta, int level, int depth, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the opponent's best next move
    nodes++;
    if (level == depth) //Base case: Node is a leaf, use SEF.
    {
        leafs++;
        return evaluateBoard(Black, turnWeight, chipDiffWeight, wallWeight, columnWeight);
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
            if (board[x][y] == BLACK) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveQuickBlack(x, y, z))
                    { //Move is valid, evaluate its sub-tree:
                        isCapture = simulateMoveBlack(x, y, z);
                        eval = maxAlphaBeta(alpha, beta, level+1, depth, turnWeight, chipDiffWeight, wallWeight, columnWeight, nodes, leafs);
                        unsimulateMoveBlack(x, y, z, isCapture);

                        if (eval < beta) //If this is the best child thus far, save it
                            beta = eval;
                        if (beta <= alpha) //If this sub-tree won't be played, prune it
                            return alpha;
                    }
    }
    if (beta < BlackWin+1024) //If beta is a winning move, have it decay at each level to favor longer checkmates
        beta++;
    return beta;
}
