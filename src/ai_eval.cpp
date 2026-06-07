#include "ai_eval.h"
#include "board_analysis.h"

int evaluateBoard(int turnColor, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight) { //Evaluates the board assuming it's given color's turn, with parameters for multipliers
    int y = 0, x = 0;
    int eval = 0;

    //If a piece is one away from the end of the board and can move or can't be captured, that color won:
    if (turnColor == White)
    {
        eval += turnWeight;
        //If current color has a piece that can move to victory, they won:
        y = SIZE-2;
        for (x = 0; x < SIZE; x++)
            if (board[x][y] == WHITE)
                return WhiteWin;
        //If the opponent color has a piece that can move to victory and we can't capture it, they won:
        y = 1;
        for (x = 0; x < SIZE; x++)
            if ((board[x][y] == BLACK) && (x == 0 || board[x-1][y-1] != WHITE) && (x == SIZE-1 || board[x+1][y-1] != WHITE))
                return BlackWin;
    }
    else if (turnColor == Black)
    {
        eval -= turnWeight;
        //If current color has a piece that can move to victory, they won:
        y = 1;
        for (x = 0; x < SIZE; x++)
            if (board[x][y] == BLACK)
                return BlackWin;
        //If the opponent color has a piece that can move to victory and we can't capture it, they won:
        y = SIZE-2;
        for (x = 0; x < SIZE; x++)
            if ((board[x][y] == WHITE) && (x == 0 || board[x-1][y+1] != BLACK) && (x == SIZE-1 || board[x+1][y+1] != BLACK))
                return WhiteWin;
    }

    //Look for strong structures:
    if (wallWeight != 0 || columnWeight != 0) {
        for (y = 0; y < SIZE-1; y++) {
            for (x = 0; x < SIZE-1; x++) { //Loop through board places:
                if (board[x][y] != EMPTY) {
                    if (x < SIZE-1 && board[x+1][y] == board[x][y]) { //Wall structure
                        if (board[x][y] == WHITE)
                            eval += wallWeight;
                        else
                            eval -= wallWeight;
                    }
                    if (y < SIZE-1 && board[x][y+1] == board[x][y]) { //Column structure
                        if (board[x][y] == WHITE)
                            eval += columnWeight;
                        else
                            eval -= columnWeight;
                    }
                }
            }
        }
    }

    //Look for strong vertical structures:

    return g_chipDiff*chipDiffWeight + eval;
}
