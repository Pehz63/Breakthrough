#include "settings.h"
#include "board_io.h"

void getSettings(int& whitePlayer, int& w1, int& w2, int& w3, int& w4, int& w5, int& wOpener, int& blackPlayer, int& b1, int& b2, int& b3, int& b4, int& b5, int& bOpener, int& gameCount, int& testing, int& testingParam) {  //Gets the player and game settings from the user
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
        w2 = p2Default;
        w3 = p3Default;
        w4 = p4Default;
        w5 = p5Default;
        break;

        case UniformRandom:
        w1 = p1Default;
        w2 = p2Default;
        w3 = p3Default;
        w4 = p4Default;
        w5 = p5Default;
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
        w2 = p2Default;
        w3 = p3Default;
        w4 = p4Default;
        w5 = p5Default;
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
        w2 = p2Default;
        w3 = p3Default;
        w4 = p4Default;
        w5 = p5Default;
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
            int fd=-1, ft=-1, fc=-1, fw=-1, fco=-1, fo=NullOpener;
            if (loadMinimaxParams("minimax_params.txt", fd, ft, fc, fw, fco, fo, "white")) {
                cout << "Loaded white params from minimax_params.txt:"
                     << "\n  depth=" << fd << "  turn=" << ft << "  chip=" << fc
                     << "  wall=" << fw << "  col=" << fco << "  opener=" << fo << "\n";
                cout << "Use these? (1=yes, 0=no): ";
                int useFile = 0; cin >> useFile;
                if (useFile == 1) { w1=fd; w2=ft; w3=fc; w4=fw; w5=fco; wOpener=fo; }
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
        //parameter 2 is weight of turnAdvantage
        while (w2 <= -1) {
            cout << "Enter the weight White (MiniMax) should use for turn advantage: ";
            cin >> w2;
            if (w2 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 3 is weight of chipAdvantage
        while (w3 <= -1) {
            cout << "Enter the weight White (MiniMax) should use for chip advantage: ";
            cin >> w3;
            if (w3 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 4 is weight of walls
        while (w4 <= -1) {
            cout << "Enter the weight White (MiniMax) should use for wall structures: ";
            cin >> w4;
            if (w4 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 5 is weight of columns
        while (w5 <= -1) {
            cout << "Enter the weight White (MiniMax) should use for column structures: ";
            cin >> w5;
            if (w5 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
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
        return getSettings(whitePlayer, w1, w2, w3, w4, w5, wOpener, blackPlayer, b1, b2, b3, b4, b5, bOpener, gameCount, testing, testingParam);
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
        b2 = p2Default;
        b3 = p3Default;
        b4 = p4Default;
        b5 = p5Default;
        break;

        case UniformRandom:
        b1 = p1Default;
        b2 = p2Default;
        b3 = p3Default;
        b4 = p4Default;
        b5 = p5Default;
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
        b2 = p2Default;
        b3 = p3Default;
        b4 = p4Default;
        b5 = p5Default;
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
        b2 = p2Default;
        b3 = p3Default;
        b4 = p4Default;
        b5 = p5Default;
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
            int fd=-1, ft=-1, fc=-1, fw=-1, fco=-1, fo=NullOpener;
            if (loadMinimaxParams("minimax_params.txt", fd, ft, fc, fw, fco, fo, "black")) {
                cout << "Loaded black params from minimax_params.txt:"
                     << "\n  depth=" << fd << "  turn=" << ft << "  chip=" << fc
                     << "  wall=" << fw << "  col=" << fco << "  opener=" << fo << "\n";
                cout << "Use these? (1=yes, 0=no): ";
                int useFile = 0; cin >> useFile;
                if (useFile == 1) { b1=fd; b2=ft; b3=fc; b4=fw; b5=fco; bOpener=fo; }
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
        //parameter 2 is weight of turnAdvantage
        while (b2 <= -1) {
            cout << "Enter the weight Black (MiniMax) should use for turn advantage: ";
            cin >> b2;
            if (b2 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 3 is weight of chipAdvantage
        while (b3 <= -1) {
            cout << "Enter the weight Black (MiniMax) should use for chip advantage: ";
            cin >> b3;
            if (b3 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 4 is weight of walls
        while (b4 <= -1) {
            cout << "Enter the weight Black (MiniMax) should use for wall structures: ";
            cin >> b4;
            if (b4 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
        //parameter 5 is weight of columns
        while (b5 <= -1) {
            cout << "Enter the weight Black (MiniMax) should use for column structures: ";
            cin >> b5;
            if (b5 <= -1)
                cout << "Invalid number: Enter an integer greater than -1.\n";
        }
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
        return getSettings(whitePlayer, w1, w2, w3, w4, w5, wOpener, blackPlayer, b1, b2, b3, b4, b5, bOpener, gameCount, testing, testingParam);
        break;
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
    //Get testingParam:
    while (testingParam <= 0 || testingParam > 5) {
        cout << "Enter parameter number you would like to test: ";
        cin >> testingParam;
        if (testingParam <= 0 || testingParam > 5)
            cout << "Invalid number: Enter an integer between 1 and 5 (inclusive).\n";
    }
    //Get PRNT:
    while (PRNT < 0) {
        cout << "Enter 0 for no printing, 1 to print moves, 2 to print board states: ";
        cin >> PRNT;
        if (PRNT < 0)
            cout << "Invalid number: Enter an integer between 0 and 2 (inclusive).\n";
    }
}
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
