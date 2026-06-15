// Breakthrough by Zeph Johnson Started 2/3/2021
// 02/24/2021 V3.0: Fully featured Breakthrough game with various options for CPUs.
// Now has more features in board evaluation function.
// Simply run program and use the console to see and interact with the board by typing.

#include "globals.h"
#include "board_io.h"
#include "settings.h"
#include "moves.h"
#include "ai_random.h"
#include "ai_minimax.h"

int main () {  //Play one game of Breakthrough
    srand(time(NULL));
    string boardFileStr = "boards\\board1.txt";
    int whitePlayer = NullPlayer, wOpener = NullOpener; //white enumerated player-type code
    int w1 = -1, w2 = -1, w3 = -1, w4 = -1, w5 = -1; //white parameters
    int blackPlayer = NullPlayer, bOpener = NullOpener; //black enumerated player-type code
    int b1 = -1, b2 = -1, b3 = -1, b4 = -1, b5 = -1; //black parameters
    int gameCount = -1;
    int testing = -2; //The color whose parameter will be changed
    int testingParam = 0; //The parameter that will be changed
    int paramMin = 1, paramMax = 10; //Used for testing parameters

    int activeTimer = time(NULL);
    int whiteGameTime = 0, blackGameTime = 0;
    int whiteTime = 0, blackTime = 0;
    int whiteScore = 0, blackScore = 0;
    int victor = None;

    //Get board from user:
    {
        ifstream check(boardFileStr);
        if (check.good()) {
            check.close();
            cout << "Use default board (boards\\board1.txt)? (1=yes, 0=no): ";
            int useDefault = 0; cin >> useDefault;
            if (useDefault == 1)
                reloadBoard(boardFileStr);
            else
                boardFileStr = getBoard();
        } else {
            boardFileStr = getBoard();
        }
    }

    cout << "Starting board:" << endl;
    printBoard();

    //Get player and game settings from user:
    getSettings(whitePlayer, w1, w2, w3, w4, w5, wOpener, blackPlayer, b1, b2, b3, b4, b5, bOpener, gameCount, testing, testingParam);

    if (whitePlayer == Human || blackPlayer == Human)
        cout << "\nExample move notation: \"c1d\" will move the piece at c1 forwards a space and into column d.\n" << endl;
    else
        cout << endl;

    if (testing == White)
        switch(testingParam) {
          case 1:
            paramMin = w1;
            break;
          case 2:
            paramMin = w2;
            break;
          case 3:
            paramMin = w3;
            break;
          case 4:
            paramMin = w4;
            break;
          case 5:
            paramMin = w5;
            break;
          default:
            paramMin = 1;
            break;
        }
    else if (testing == Black)
        switch(testingParam) {
          case 1:
            paramMin = b1;
            break;
          case 2:
            paramMin = b2;
            break;
          case 3:
            paramMin = b3;
            break;
          case 4:
            paramMin = b4;
            break;
          case 5:
            paramMin = b5;
            break;
          default:
            paramMin = 1;
            break;
        }

    for (int p = paramMin; p <= paramMax; p++)
    {
        if (testing == White)
            switch(testingParam) {
            case 1:
                w1 = p;
                break;
            case 2:
                w2 = p;
                break;
            case 3:
                w3 = p;
                break;
            case 4:
                w4 = p;
                break;
            case 5:
                w5 = p;
                break;
            default:
                w1 = p;
                break;
            }
        else if (testing == Black)
            switch(testingParam) {
            case 1:
                b1 = p;
                break;
            case 2:
                b2 = p;
                break;
            case 3:
                b3 = p;
                break;
            case 4:
                b4 = p;
                break;
            case 5:
                b5 = p;
                break;
            default:
                b1 = p;
                break;
            }
        else if (p > paramMin)
            return 0;

        //Play set until the game count is reached:
        for (int game = 0; game < gameCount; game++)
        {
            //Play game until a victor is decided:
            victor = None;
            if (PRNT > 0)
                cout << "\tGame  " << game << ":\t";
            while (victor < WhiteWin && victor > BlackWin)
            {
                if (PRNT > 1)
                {
                    cout << endl;
                    printBoard();
                    cout << "White game time:\t" << whiteGameTime << "\tBlack game time:\t" << blackGameTime << endl;
                    if (blackPlayer == MiniMax)
                        cout << "Board evaluation:\t" << victor;
                    cout << endl;
                }
                activeTimer = time(NULL);
                victor = moveWhite(whitePlayer, w1, w2, w3, w4, w5, wOpener);
                whiteGameTime += time(NULL) - activeTimer;
                if (victor < WhiteWin && victor > BlackWin)
                {
                    if (PRNT > 1)
                    {
                        cout << endl;
                        printBoard();
                        cout << "White game time:\t" << whiteGameTime << "\tBlack game time:\t" << blackGameTime << endl;
                        if (whitePlayer == MiniMax)
                            cout << "Board evaluation:\t" << victor;
                        cout << endl;
                    }
                    activeTimer = time(NULL);
                    victor = moveBlack(blackPlayer, b1, b2, b3, b4, b5, bOpener);
                    blackGameTime += time(NULL) - activeTimer;
                }
            }


            if (PRNT > 0)
            {
                cout << endl;
                printBoard();
                printVictor(victor, game, whiteGameTime, blackGameTime);
            }

            //Incrememt score:
            if (victor == WhiteWin)
                whiteScore++;
            else if (victor == BlackWin)
                blackScore++;
            if (!reloadBoard(boardFileStr))
                boardFileStr = getBoard();

            //Increment set times:
            whiteTime += whiteGameTime;
            blackTime += blackGameTime;
            //Reset game times:
            whiteGameTime = 0;
            blackGameTime = 0;
        }

        //Print game result:
        if (testing == None || p == paramMin || PRNT > 0)
        {
            cout << "w0\tw1\tw2\tw3\tw4\tw5\twOpener\n";
            cout << "b0\tb1\tb2\tb3\tb4\tb5\tbOpener\n";
            cout << "wt\tbt\twn\tbn\tgc\tws\tbs\n";
        }
        cout << whitePlayer << "\t" << w1 << "\t" << w2 << "\t" << w3 << "\t" << w4 << "\t" << w5 << "\t" << wOpener << "\n";
        cout << blackPlayer << "\t" << b1 << "\t" << b2 << "\t" << b3 << "\t" << b4 << "\t" << b5 << "\t" << bOpener << "\n";
        cout << whiteTime << "\t" << blackTime << "\t" << nodesWhite << "\t" << nodesBlack << "\t" << gameCount << "\t" << whiteScore << "\t" << blackScore << endl;
        if (testing == None) {
            cout << "\nWhite:\tBlack:\n" << whiteScore << "\t" << blackScore << endl;
            break;
        }

        whiteTime = 0;
        blackTime = 0;
        whiteScore = 0;
        blackScore = 0;
        nodesWhite = 0;
        nodesBlack = 0;
    }
    char exitCode;
    cout << "Progam finished, enter anything to close: ";
    cin >> exitCode;
    return 0;
}
