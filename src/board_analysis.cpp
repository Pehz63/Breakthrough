#include "board_analysis.h"
#include "moves.h"

int countChips(int y) {  //Count all chips for given row
    int count = 0;
    for (int x = 0; x < SIZE; x++)
        if (board[x][y] != EMPTY)
            count++;
    return count;
}
int countChips() {  //Count all chips
    int count = 0;
    for (int y = 0; y < SIZE; y++)
        count += countChips(y);
    return count;
}
int chipDiff(int y) {  //Count the chip difference in a given row white+ black-
    int count = 0;
    for (int x = 0; x < SIZE; x++)
    {
        if (board[x][y] == WHITE)
            count++;
        else if (board[x][y] == BLACK)
            count--;
    }
    return count;
}
int chipDiff() {  //Count the chip difference on the board
    int count = 0;
    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++)
        {
            if (board[x][y] == WHITE)
                count++;
            else if (board[x][y] == BLACK)
                count--;
        }
    }
    return count;
}
// Remaining capacity (Docs/axioms.md Lemma B): the sum over a side's pieces of
// each piece's row distance to its own goal row. Strictly decreases every ply.
// Not an evaluator weight: the white-centric capacity advantage is linearly
// dependent on existing terms (capacityBlack - capacityWhite == forwardSum -
// (SIZE-1)*chipDiff, code-verified in tests/test_eval.cpp), so these helpers
// exist for analysis tools (distance-to-win search, theory 18's outcome
// correlation), not for the eval registry.
int capacityWhite() {  //Sum of (SIZE-1 - y) over all white pieces
    int cap = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] == WHITE)
                cap += SIZE-1 - y;
    return cap;
}
int capacityBlack() {  //Sum of y over all black pieces
    int cap = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] == BLACK)
                cap += y;
    return cap;
}
int findWinWhite() {  //Returns -1 if no win or column number if white can win
    int y = SIZE-2;
    int moveList[SIZE*3];
    int availableMoves = 0;

    //Loop through board spaces in furthest row:
    for (int x = 0; x < SIZE; x++)
        if (board[x][y] == WHITE) //If space has our piece, move it:
            for (int z = x - 1; z <= x+1; z++) //Try each direction:
                if (tryMoveWhite(x, y, z, false)) //Move is valid, list and count it:
                    moveList[availableMoves++] = x;

    //If no win, return:
    if (availableMoves == 0)
        return -1;

    //Else, return a random move:
    return moveList[rand()%availableMoves];
}
int findWinBlack() {  //Returns -1 if no win or column number if black can win
    int y = 1;
    int moveList[SIZE*3];
    int availableMoves = 0;

    //Loop through board spaces in furthest row:
    for (int x = 0; x < SIZE; x++)
        if (board[x][y] == BLACK) //If space has our piece, move it:
            for (int z = x - 1; z <= x+1; z++) //Try each direction:
                if (tryMoveBlack(x, y, z, false)) //Move is valid, list and count it:
                    moveList[availableMoves++] = x;

    //If no win, return:
    if (availableMoves == 0)
        return -1;

    //Else, return a random move:
    return moveList[rand()%availableMoves];
}
bool canWinWhite() { //Returns 0 if no win or 1 if white can/did win
    return g_whiteAtEnd > 0 || g_blackCount == 0;
}
bool canWinBlack() { //Returns 0 if no win or 1 if black can/did win
    return g_blackAtEnd > 0 || g_whiteCount == 0;
}
