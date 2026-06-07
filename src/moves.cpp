#include "moves.h"
#include "ai_random.h"
#include "ai_minimax.h"
#include "ai_eval.h"

int moveWhite(int whitePlayer, int w1, int w2, int w3, int w4, int w5, int wOpener) {  //Call appropriate move function for given parameters
    unsigned long long int nodes = 0;
    unsigned long long int leafs = 0;
    switch (whitePlayer) {
      case UniformRandom:
        if (playOpenerWhite(wOpener))
            return evaluateBoard(Black, w2, w3, w4, w5);
        return pureRandomMoveWhite();
        break;

      case TieredRandom:
        if (playOpenerWhite(wOpener))
            return evaluateBoard(Black, w2, w3, w4, w5);
        return tieredRandomMoveWhite();
        break;

      case SmartRandom:
        if (playOpenerWhite(wOpener))
            return evaluateBoard(Black, w2, w3, w4, w5);
        return smartRandomMoveWhite(w1);
        break;

      case MiniMax:
        if (PRNT > 1)
            cout << "Getting move for White (MiniMax)..." << endl;
        if (playOpenerWhite(wOpener))
            return evaluateBoard(Black, w2, w3, w4, w5);
        return miniMaxWhite(w1, w2, w3, w4, w5, nodes, leafs);
        break;

      default: //Human
        return playerMove(White);
        break;
    }
    cout << "Error calling moveWhite." << endl;
    return BlackWin;
}
int moveBlack(int blackPlayer, int b1, int b2, int b3, int b4, int b5, int bOpener) {  //Call appropriate move function for given parameters
    unsigned long long int nodes = 0;
    unsigned long long int leafs = 0;
    switch (blackPlayer) {
      case UniformRandom:
        if (playOpenerBlack(bOpener))
            return evaluateBoard(White, b2, b3, b4, b5);
        return pureRandomMoveBlack();
        break;

      case TieredRandom:
        if (playOpenerBlack(bOpener))
            return evaluateBoard(White, b2, b3, b4, b5);
        return tieredRandomMoveBlack();
        break;

      case SmartRandom:
        if (playOpenerBlack(bOpener))
            return evaluateBoard(White, b2, b3, b4, b5);
        return smartRandomMoveBlack(b1);
        break;

      case MiniMax:
        if (PRNT > 1)
            cout << "Getting move for Black (MiniMax)..." << endl;
        if (playOpenerBlack(bOpener))
            return evaluateBoard(White, b2, b3, b4, b5);
        return miniMaxBlack(b1, b2, b3, b4, b5, nodes, leafs);
        break;

      default: //Human
        return playerMove(Black);
        break;
    }
    cout << "Error calling moveBlack." << endl;
    return WhiteWin;
}

int playerMove(int playerColor) {  //Get and perform a player's move for given color and returns victory#
    if (playerColor != White && playerColor != Black)
    {
        cout << "Error: Invalid player color." << endl;
        return None;
    }

    char moveX1Chr, moveX2Chr;
    int moveX1, moveY, moveX2;
    bool valid = false;
    int victor = 0;

    //While move isn't valid, get move from player:
    while (!valid)
    {
        //Tell player to input move:
        cout << "Input move for ";
        if (playerColor == White)
            cout << "White: ";
        else if (playerColor == Black)
            cout << "Black: ";
        cin.clear();
        cin.ignore(69, '\n');
        cin >> moveX1Chr >> moveY >> moveX2Chr;
        moveX1 = moveX1Chr - 'a';
        moveX2 = moveX2Chr - 'a';
        //cout << "X1Chr = " << moveX1Chr << "\tX1 = " << moveX1 << "\nY = " << moveY << "\nX2Chr = " << moveX2Chr << "\tX2 = " << moveX2 << endl;

        if (playerColor == White)
        {
            valid = tryMoveWhite(moveX1, moveY, moveX2, true);
            if (valid)
                victor = playMoveWhite(moveX1, moveY, moveX2);
        }
        else if (playerColor == Black)
        {
            valid = tryMoveBlack(moveX1, moveY, moveX2, true);
            if (valid)
                victor = playMoveBlack(moveX1, moveY, moveX2);
        }
    }

    return victor;
}

bool tryMoveWhite(int moveX1, int moveY, int moveX2, bool usr) {  //Returns if white can make the given move and prints result for user
    if (moveX1 >= 0 && moveX1 <= SIZE-1 && moveY >= 0 && moveY <= SIZE-2 && moveX2 >= 0 && moveX2 <= SIZE-1 && moveX2 >= moveX1-1 && moveX2 <= moveX1+1)
    { //Given index is on the board
        if (board[moveX1][moveY] == WHITE)
        { //This is the player's piece
            if (board[moveX2][moveY+1] != WHITE)
            { //This move isn't blocked by own piece
                if (!(moveX1 == moveX2 && board[moveX2][moveY+1] == BLACK))
                { //This move isn't trying to capture forwards
                    return true;
                }
                else if (usr)
                    cout << "Invalid move: Cannot capture forwards." << endl;
            }
            else if (usr)
                cout << "Invalid move: Move blocked by own piece." << endl;
        }
        else if (usr)
            cout << "Invalid move: Move lacks your piece." << endl;
    }
    else if (usr)
        cout << "Invalid move: Move out of bounds. Input in the form: \"d1c\" to move piece at d1 to place c2." << endl;
    return false;
}
bool tryMoveBlack(int moveX1, int moveY, int moveX2, bool usr) {  //Returns if black can make the given move and prints result for user
    if (moveX1 >= 0 && moveX1 <= SIZE-1 && moveY >= 1 && moveY <= SIZE-1 && moveX2 >= 0 && moveX2 <= SIZE-1 && moveX2 >= moveX1-1 && moveX2 <= moveX1+1)
    { //Given index is on the board
        if (board[moveX1][moveY] == BLACK)
        { //This is the player's piece
            if (board[moveX2][moveY-1] != BLACK)
            { //This move isn't blocked by own piece
                if (!(moveX1 == moveX2 && board[moveX2][moveY-1] == WHITE))
                { //This move isn't trying to capture forwards
                    return true;
                }
                else if (usr)
                    cout << "Invalid move: Cannot capture forwards." << endl;
            }
            else if (usr)
                cout << "Invalid move: Move blocked by own piece." << endl;
        }
        else if (usr)
            cout << "Invalid move: Move lacks your piece." << endl;
    }
    else if (usr)
        cout << "Invalid move: Move out of bounds. Input in the form: \"d1c\" to move piece at d1 to place c2." << endl;
    return false;
}
int playMoveWhite(int moveX1, int moveY, int moveX2) { //Lets white play the given move and returns victory#
    bool isCapture;
    int victor = None;

    //Show move notation:
    if (PRNT > 0)
        cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
    if (PRNT > 1)
        cout << endl;
    else if (PRNT > 0)
        cout << ", ";

    //If it's a capture, we have to check if a piece remains
    isCapture = (board[moveX2][moveY+1] == BLACK);
    if (isCapture) {
        g_blackCount--;
        g_chipDiff++;
    }

    //Play the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY+1] = WHITE;

    //If white reached the end, they won:
    if (moveY >= SIZE-2)
        return WhiteWin;

    //If white captured the last black piece, white won by domination:
    if (isCapture && g_blackCount == 0)
        return WhiteWin;
    return None;
}
int playMoveBlack(int moveX1, int moveY, int moveX2) { //Lets black play the given move and returns victory#
    bool isCapture;
    int victor = None;

    //Show move notation:
    if (PRNT > 0)
        cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
    if (PRNT > 1)
        cout << endl;
    else if (PRNT > 0)
        cout << ", ";

    //If it's a capture, we have to check if a piece remains
    isCapture = (board[moveX2][moveY-1] == WHITE);
    if (isCapture) {
        g_whiteCount--;
        g_chipDiff--;
    }

    //Play the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY-1] = BLACK;

    //If black reached the end, they won:
    if (moveY <= 1)
        return BlackWin;

    //If black captured the last white piece, black won by domination:
    if (isCapture && g_whiteCount == 0)
        return BlackWin;
    return None;
}

bool tryMoveQuickWhite(int moveX1, int moveY, int moveX2) { //Returns if white can make the given move without checking parameters except z
    if (moveX2 >= 0 && moveX2 <= SIZE-1) //Given index is on the board
        if (board[moveX2][moveY+1] != WHITE) //This move isn't blocked by own piece
            if ((moveX1 != moveX2) || (board[moveX2][moveY+1] == EMPTY)) //This move isn't trying to capture forwards
                return true;
    return false;
}
bool tryMoveQuickBlack(int moveX1, int moveY, int moveX2) { //Returns if black can make the given move without checking parameters except z
    if (moveX2 >= 0 && moveX2 <= SIZE-1) //Given index is on the board
        if (board[moveX2][moveY-1] != BLACK) //This move isn't blocked by own piece
            if ((moveX1 != moveX2) || (board[moveX2][moveY-1] == EMPTY)) //This move isn't trying to capture forwards
                return true;
    return false;
}
bool simulateMoveWhite(int moveX1, int moveY, int moveX2) { //Simulates the given move for white and returns 1 if it was a capture
    bool isCapture;
    //If it's a capture, we have to return this later
    isCapture = (board[moveX2][moveY+1] == BLACK);
    if (isCapture) {
        g_blackCount--;
        g_chipDiff++;
    }

    //Simulate the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY+1] = WHITE;
    if (moveY+1 == SIZE-1) g_whiteAtEnd++;

    return isCapture;
}
bool simulateMoveBlack(int moveX1, int moveY, int moveX2) { //Simulates the given move for black and returns 1 if it was a capture
    bool isCapture;
    //If it's a capture, we have to return this later
    isCapture = (board[moveX2][moveY-1] == WHITE);
    if (isCapture) {
        g_whiteCount--;
        g_chipDiff--;
        if (moveY-1 == SIZE-1) g_whiteAtEnd--;
    }

    //Simulate the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY-1] = BLACK;
    if (moveY-1 == 0) g_blackAtEnd++;

    return isCapture;
}
void unsimulateMoveWhite(int moveX1, int moveY, int moveX2, bool isCapture) { //Undoes the given move for white
    if (moveY+1 == SIZE-1) g_whiteAtEnd--;
    //Undo the simulated move:
    board[moveX1][moveY] = WHITE;
    //If it's a capture, replace captured piece
    if (isCapture) {
        g_blackCount++;
        g_chipDiff--;
        board[moveX2][moveY+1] = BLACK;
    } else
        board[moveX2][moveY+1] = EMPTY;
    return;
}
void unsimulateMoveBlack(int moveX1, int moveY, int moveX2, bool isCapture) { //Undoes the given move for black
    if (moveY-1 == 0) g_blackAtEnd--;
    //Undo the simulated move:
    board[moveX1][moveY] = BLACK;
    //If it's a capture, replace captured piece
    if (isCapture) {
        g_whiteCount++;
        g_chipDiff++;
        if (moveY-1 == SIZE-1) g_whiteAtEnd++;
        board[moveX2][moveY-1] = WHITE;
    } else
        board[moveX2][moveY-1] = EMPTY;
    return;
}
