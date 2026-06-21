#include "settings.h"
#include "board_io.h"
#include "ai_eval.h"

// ============================================================
// EVALUATOR SETTINGS -- defaultEvalParams / getEvaluatorSettings
// ============================================================
// Seed a side's evaluator parameters to the registry defaults for evaluator 0.
// Used for player types that don't prompt for an evaluator (Human and the random
// AIs), so their opener-fallback evaluation still has sensible weights.
static void defaultEvalParams(int& eval, int* params) {
    eval = 0;
    for (int i = 0; i < g_evaluators[0].paramCount; i++)
        params[i] = g_evaluators[0].params[i].def;
}

// Prompt for a MiniMax side's evaluator and its parameters. Anything already set
// (eval >= 0, or a param >= its minimum) is left untouched, so values loaded from
// minimax_params.txt are not re-asked.
static void getEvaluatorSettings(const char* color, const char* who, int& eval, int* params) {
    if (eval < 0) {
        cout << "\nEvaluators:";
        for (int i = 0; i < g_evalCount; i++)
            cout << "\n\t" << i << "\t= " << g_evaluators[i].name;
        cout << endl;
    }
    while (eval < 0 || eval >= g_evalCount) {
        cout << "Enter the evaluator " << color << " (" << who << ") should use: ";
        cin >> eval;
        if (eval < 0 || eval >= g_evalCount)
            cout << "Invalid number: Enter an integer between 0 and " << (g_evalCount-1) << " inclusive.\n";
    }
    const EvalDef& e = g_evaluators[eval];
    for (int i = 0; i < e.paramCount; i++) {
        const EvalParamDef& pd = e.params[i];
        while (params[i] < pd.lo || params[i] > pd.hi) {
            cout << "Enter the weight " << color << " (" << who << ") should use for "
                 << pd.name << " (" << pd.lo << "-" << pd.hi << "): ";
            cin >> params[i];
            if (params[i] < pd.lo || params[i] > pd.hi)
                cout << "Invalid number: Enter an integer between " << pd.lo << " and " << pd.hi << " inclusive.\n";
        }
    }
}

// ============================================================
// GAME SETTINGS -- getSettings
// ============================================================
void getSettings(int& whitePlayer, int& w1, int& wEval, int* wParams, int& wOpener, int& blackPlayer, int& b1, int& bEval, int* bParams, int& bOpener, int& gameCount, int& testing, int& testingParam) {  //Gets the player and game settings from the user
    //Get whitePlayer:
    if (whitePlayer <= NullPlayer) {
        cout << "\n\t0\t= Human";
        cout << "\n\t1\t= Uniform Random";
        cout << "\n\t2\t= Tiered Random";
        cout << "\n\t3\t= Smart Random";
        cout << "\n\t4\t= MiniMax";
    }
    else
        cout << endl;
    while (whitePlayer <= NullPlayer) {
        cout << "\nChoose who will control White: ";
        cin >> whitePlayer;
    }
    switch(whitePlayer) {
        case Human:
        w1 = p1Default;
        defaultEvalParams(wEval, wParams);
        break;

        case UniformRandom:
        w1 = p1Default;
        defaultEvalParams(wEval, wParams);
        //parameter opener is opener
        if (wOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (wOpener <= NullOpener) {
            cout << "Enter the opener White (Uniform Random) should use: ";
            cin >> wOpener;
            if (wOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case TieredRandom:
        w1 = p1Default;
        defaultEvalParams(wEval, wParams);
        //parameter opener is opener
        if (wOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (wOpener <= NullOpener) {
            cout << "Enter the opener White (Tiered Random) should use: ";
            cin >> wOpener;
            if (wOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case SmartRandom:
        defaultEvalParams(wEval, wParams);
        //parameter 1 is # of pieces forward:
        while (w1 <= 0) {
            cout << "Enter how many pieces White (Smart Random) should have forward: ";
            cin >> w1;
            if (w1 <= 0)
                cout << "Invalid number: Enter an integer greater than 0.\n";
        }
        //parameter opener is opener
        if (wOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (wOpener <= NullOpener) {
            cout << "Enter the opener White (Smart Random) should use: ";
            cin >> wOpener;
            if (wOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case MiniMax:
        {
            int fd=-1, fe=-1, fo=NullOpener;
            int fp[MAX_EVAL_PARAMS];
            if (loadMinimaxParams("minimax_params.txt", fd, fe, fp, fo, "white")) {
                const EvalDef& e = g_evaluators[fe];
                cout << "Loaded white params from minimax_params.txt:"
                     << "\n  depth=" << fd << "  evaluator=" << e.name;
                for (int i = 0; i < e.paramCount; i++)
                    cout << "  " << e.params[i].name << "=" << fp[i];
                cout << "  opener=" << fo << "\n";
                cout << "Use these? (1=yes, 0=no): ";
                int useFile = 0; cin >> useFile;
                if (useFile == 1) {
                    w1 = fd; wEval = fe; wOpener = fo;
                    for (int i = 0; i < e.paramCount; i++) wParams[i] = fp[i];
                }
            }
        }
        //parameter 1 is depth:
        while (w1 <= 0) {
            cout << "Depths:\tGame time, without structure params:\n";
            cout << "<8\t 1   second\n";
            cout << " 8\t10   seconds\n";
            cout << " 9\t 0.5 minute\n";
            cout << "10\t 5   minutes\n";
            cout << "11\t25   minutes\n";
            cout << "Depths:\tGame time, with structure params:\n";
            cout << "<6\t 1  second\n";
            cout << " 6\t10  seconds\n";
            cout << " 7\t1.5 minutes\n";
            cout << " 8\t 7  minutes\n";
            cout << " 9\t40  minutes\n";
            cout << "Enter the depth White (MiniMax) should search: ";
            cin >> w1;
            if (w1 <= 0)
                cout << "Invalid number: Enter an integer greater than 0.\n";
        }
        //Evaluator and its weights:
        getEvaluatorSettings("White", "MiniMax", wEval, wParams);
        //parameter opener is opener
        if (wOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (wOpener <= NullOpener) {
            cout << "Enter the opener White (MiniMax) should use: ";
            cin >> wOpener;
            if (wOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        default:
        cout << "Invalid code: Enter an integer between 0 and 4 inclusive.\n";
        whitePlayer = NullPlayer;
        return getSettings(whitePlayer, w1, wEval, wParams, wOpener, blackPlayer, b1, bEval, bParams, bOpener, gameCount, testing, testingParam);
        break;
    }

    //Get blackPlayer:
    if (blackPlayer <= NullPlayer) {
        cout << "\n\t0\t= Human";
        cout << "\n\t1\t= Uniform Random";
        cout << "\n\t2\t= Tiered Random";
        cout << "\n\t3\t= Smart Random";
        cout << "\n\t4\t= MiniMax";
    }
    else
        cout << endl;
    while (blackPlayer <= NullPlayer) {
        cout << "\nChoose who will control Black: ";
        cin >> blackPlayer;
    }
    switch(blackPlayer) {
        case Human:
        b1 = p1Default;
        defaultEvalParams(bEval, bParams);
        break;

        case UniformRandom:
        b1 = p1Default;
        defaultEvalParams(bEval, bParams);
        //parameter opener is opener
        if (bOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (bOpener <= NullOpener) {
            cout << "Enter the opener Black (Uniform Random) should use: ";
            cin >> bOpener;
            if (bOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case TieredRandom:
        b1 = p1Default;
        defaultEvalParams(bEval, bParams);
        //parameter opener is opener
        if (bOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (bOpener <= NullOpener) {
            cout << "Enter the opener Black (Tiered Random) should use: ";
            cin >> bOpener;
            if (bOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case SmartRandom:
        defaultEvalParams(bEval, bParams);
        //parameter 1 is # of pieces forward
        while (b1 <= 0) {
            cout << "Enter how many pieces Black (Smart Random) should have forward: ";
            cin >> b1;
            if (b1 <= 0)
                cout << "Invalid number: Enter an integer greater than 0.\n";
        }
        //parameter opener is opener
        if (bOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (bOpener <= NullOpener) {
            cout << "Enter the opener Black (Smart Random) should use: ";
            cin >> bOpener;
            if (bOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        case MiniMax:
        {
            int fd=-1, fe=-1, fo=NullOpener;
            int fp[MAX_EVAL_PARAMS];
            if (loadMinimaxParams("minimax_params.txt", fd, fe, fp, fo, "black")) {
                const EvalDef& e = g_evaluators[fe];
                cout << "Loaded black params from minimax_params.txt:"
                     << "\n  depth=" << fd << "  evaluator=" << e.name;
                for (int i = 0; i < e.paramCount; i++)
                    cout << "  " << e.params[i].name << "=" << fp[i];
                cout << "  opener=" << fo << "\n";
                cout << "Use these? (1=yes, 0=no): ";
                int useFile = 0; cin >> useFile;
                if (useFile == 1) {
                    b1 = fd; bEval = fe; bOpener = fo;
                    for (int i = 0; i < e.paramCount; i++) bParams[i] = fp[i];
                }
            }
        }
        //parameter 1 is depth
        while (b1 <= 0) {
            cout << "Depths:\tGame time, without structure params:\n";
            cout << "<8\t 1   second\n";
            cout << " 8\t10   seconds\n";
            cout << " 9\t 0.5 minute\n";
            cout << "10\t 5   minutes\n";
            cout << "11\t25   minutes\n";
            cout << "Depths:\tGame time, with structure params:\n";
            cout << "<6\t 1  second\n";
            cout << " 6\t10  seconds\n";
            cout << " 7\t1.5 minutes\n";
            cout << " 8\t 7  minutes\n";
            cout << " 9\t40  minutes\n";
            cout << "Enter the depth Black (MiniMax) should search: ";
            cin >> b1;
            if (b1 <= 0)
                cout << "Invalid number: Enter an integer greater than 0.\n";
        }
        //Evaluator and its weights:
        getEvaluatorSettings("Black", "MiniMax", bEval, bParams);
        //parameter opener is opener
        if (bOpener <= NullOpener)
            cout << "Opening codes:\tStandard = " << StandardOpener << "\tOffensive = " << OffensiveOpener << "\tDefensive = " << DefensiveOpener << endl;
        while (bOpener <= NullOpener) {
            cout << "Enter the opener Black (MiniMax) should use: ";
            cin >> bOpener;
            if (bOpener <= NullOpener)
                cout << "Invalid number: Enter an integer between 0 and 2 inclusive.\n";
        }
        break;

        default:
        cout << "Invalid code: Enter an integer between 0 and 4 inclusive.\n";
        blackPlayer = NullPlayer;
        return getSettings(whitePlayer, w1, wEval, wParams, wOpener, blackPlayer, b1, bEval, bParams, bOpener, gameCount, testing, testingParam);
        break;
    }

    //Get gameCount:
    while (gameCount <= 0) {
        cout << "Enter how many games you would like played: ";
        cin >> gameCount;
        if (gameCount <= 0)
            cout << "Invalid number: Enter an integer greater than 0.\n";
    }
    //Get testing:
    while (testing != None && testing != White && testing != Black) {
        cout << "Enter 0 if no testing, 1 if testing White, -1 if testing Black: ";
        cin >> testing;
        if (testing != None && testing != White && testing != Black)
            cout << "Invalid number: Enter an integer between -1 and 1 (inclusive).\n";
    }
    //Get testingParam: index 1 = depth, 2..(1+paramCount) = the tested side's evaluator params.
    if (testing == None) {
        testingParam = 1;
    } else {
        int testedEval = (testing == White) ? wEval : bEval;
        if (testedEval < 0 || testedEval >= g_evalCount) testedEval = 0;
        int maxParam = 1 + g_evaluators[testedEval].paramCount;
        while (testingParam <= 0 || testingParam > maxParam) {
            cout << "Enter parameter number you would like to test (1=depth, 2.." << maxParam << "=evaluator weights): ";
            cin >> testingParam;
            if (testingParam <= 0 || testingParam > maxParam)
                cout << "Invalid number: Enter an integer between 1 and " << maxParam << " (inclusive).\n";
        }
    }
    //Get PRNT:
    while (PRNT < 0) {
        cout << "Enter 0 for no printing, 1 to print moves, 2 to print board states: ";
        cin >> PRNT;
        if (PRNT < 0)
            cout << "Invalid number: Enter an integer between 0 and 2 (inclusive).\n";
    }
    //Get SHOW_EVAL:
    while (SHOW_EVAL < 0) {
        cout << "Show board evaluations? (0=no, 1=yes): ";
        cin >> SHOW_EVAL;
        if (SHOW_EVAL < 0)
            cout << "Invalid number: Enter 0 or 1.\n";
    }
}
// ============================================================
// VICTOR DISPLAY -- printVictor
// ============================================================
void printVictor(int victor, int game, int whiteGameTime, int blackGameTime) {  //Display the victor
    cout << "Game " << game << ":\t";
    cout << "Victor code: " << victor << endl;
    if (victor >= WhiteWin)
        cout << "White";
    else if (victor <= BlackWin)
        cout << "Black";
    else
        cout << "Nobody";
    cout << " has won!!" << endl;
    cout << "White time:\tBlack time:\n" << whiteGameTime << "\t\t" << blackGameTime << endl << endl;
}
