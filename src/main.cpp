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
#include "ai_eval.h"

// ============================================================
// EVAL DISPLAY -- formatEval / printEvalLine
// ============================================================
// Format a white-centric eval: forced wins as +WIN / -WIN, else the raw number.
static string formatEval(int v) {
    if (v >= WhiteWin - 1024)      return "+WIN";
    if (v <= BlackWin + 1024)      return "-WIN";
    return std::to_string(v);
}

// Print one side's evaluation line: the immediate static eval ("now") and, for a
// MiniMax side, the predicted best-line eval ("pred"). White-centric: a positive
// number favors White. Forced wins are shown as +WIN / -WIN.
static void printEvalLine(const char* side, int imm, bool isMiniMax, int down) {
    cout << side << " eval: now=" << formatEval(imm);
    if (isMiniMax)
        cout << "  pred=" << formatEval(down);
    cout << endl;
}

int main () {  //Play one game of Breakthrough
    srand(time(NULL));
    string boardFileStr = "boards\\board1.txt";
    int whitePlayer = NullPlayer, wOpener = NullOpener; //white enumerated player-type code
    int w1 = -1, wEval = -1;                 //white depth/furthest + evaluator index
    int wParams[MAX_EVAL_PARAMS];            //white evaluator weights
    int blackPlayer = NullPlayer, bOpener = NullOpener; //black enumerated player-type code
    int b1 = -1, bEval = -1;                 //black depth/furthest + evaluator index
    int bParams[MAX_EVAL_PARAMS];            //black evaluator weights
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) { wParams[i] = -1; bParams[i] = -1; } //sentinels: prompt unless loaded
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
    getSettings(whitePlayer, w1, wEval, wParams, wOpener, blackPlayer, b1, bEval, bParams, bOpener, gameCount, testing, testingParam);

    if (whitePlayer == Human || blackPlayer == Human)
        cout << "\nExample move notation: \"c1d\" will move the piece at c1 forwards a space and into column d.\n" << endl;
    else
        cout << endl;

    // testingParam: 1 = depth, 2..(1+paramCount) = the tested side's evaluator weights.
    if (testing == White)
        paramMin = (testingParam == 1) ? w1 : wParams[testingParam - 2];
    else if (testing == Black)
        paramMin = (testingParam == 1) ? b1 : bParams[testingParam - 2];

    for (int p = paramMin; p <= paramMax; p++)
    {
        if (testing == White) {
            if (testingParam == 1) w1 = p; else wParams[testingParam - 2] = p;
        }
        else if (testing == Black) {
            if (testingParam == 1) b1 = p; else bParams[testingParam - 2] = p;
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
                }
                // Snapshot White's immediate eval before the move; read its
                // predicted downstream eval after.
                int immW = SHOW_EVAL ? immediateEvalForDisplay(whitePlayer == MiniMax, wEval, wParams) : 0;
                activeTimer = time(NULL);
                victor = moveWhite(whitePlayer, w1, wEval, wParams, wOpener);
                whiteGameTime += time(NULL) - activeTimer;
                if (SHOW_EVAL)
                    printEvalLine("White", immW, whitePlayer == MiniMax, g_downEvalWhite);
                if (victor < WhiteWin && victor > BlackWin)
                {
                    if (PRNT > 1)
                    {
                        cout << endl;
                        printBoard();
                        cout << "White game time:\t" << whiteGameTime << "\tBlack game time:\t" << blackGameTime << endl;
                    }
                    int immB = SHOW_EVAL ? immediateEvalForDisplay(blackPlayer == MiniMax, bEval, bParams) : 0;
                    activeTimer = time(NULL);
                    victor = moveBlack(blackPlayer, b1, bEval, bParams, bOpener);
                    blackGameTime += time(NULL) - activeTimer;
                    if (SHOW_EVAL)
                        printEvalLine("Black", immB, blackPlayer == MiniMax, g_downEvalBlack);
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

        //Print game result. Each side: player, depth, evaluator + its weights, opener.
        {
            int we = (wEval >= 0 && wEval < g_evalCount) ? wEval : 0;
            int be = (bEval >= 0 && bEval < g_evalCount) ? bEval : 0;
            cout << "white: player=" << whitePlayer << " depth=" << w1 << " eval=" << g_evaluators[we].name;
            for (int i = 0; i < g_evaluators[we].paramCount; i++)
                cout << " " << g_evaluators[we].params[i].name << "=" << wParams[i];
            cout << " opener=" << wOpener << "\n";
            cout << "black: player=" << blackPlayer << " depth=" << b1 << " eval=" << g_evaluators[be].name;
            for (int i = 0; i < g_evaluators[be].paramCount; i++)
                cout << " " << g_evaluators[be].params[i].name << "=" << bParams[i];
            cout << " opener=" << bOpener << "\n";
        }
        if (testing == None || p == paramMin || PRNT > 0)
            cout << "wt\tbt\twn\tbn\tgc\tws\tbs\n";
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
