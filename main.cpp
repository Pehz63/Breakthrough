// Breakthrough by Zeph Johnson Started 2/3/2021
// 02/24/2021 V3.0: Fully featured Breakthrough game with various options for CPUs.
// Now has more features in board evaluation function.
// Simply run program and use the console to see and interact with the board by typing.

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include <limits.h>

//using namespace std;
using std::cout;
using std::cin;
using std::endl;
using std::flush;
using std::string;
using std::fstream;

#define EMPTY   '.'
#define WHITE   'W'
#define BLACK   'B'
#define SIZE    8
int PRNT = -1;

int p1Default = 1;
int p2Default = 1;
int p3Default = 1;
int p4Default = 0;
int p5Default = 0;

char chipChr[3] = {'.', 'W', 'B'};
enum VictorEnum {None = 0, White = 1, Black = -1, WhiteWin = INT_MAX-1, BlackWin = INT_MIN+1};
enum PlayerEnum {NullPlayer = -1, Human = 0, UniformRandom = 1, TieredRandom = 2, SmartRandom = 3, MiniMax = 4};
enum OpenerEnum {NullOpener = -1, StandardOpener = 0, OffensiveOpener = 1, DefensiveOpener = 2};

char board[SIZE][SIZE];
unsigned long long int nodesWhite = 0;
unsigned long long int nodesBlack = 0;

string getBoard(); //Loads the board from a user-given file
bool reloadBoard(string); //Reloads the board from the given file
void printBoard(); //Print the board
void getSettings(int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&); //Gets the player and game settings from the user
void printVictor(int, int, int, int); //Display the victor

int countChips(int); //Count all chips for given row
int countChips(); //Count all chips
int chipDiff(int); //Count the chip difference in a given row white+ black-
int chipDiff(); //Count the chip difference on the board
int findWinWhite(); //Returns -1 if no win or column number if white can win
int findWinBlack(); //Returns -1 if no win or column number if black can win
bool canWinWhite(); //Returns 0 if no win or 1 if white can/did win
bool canWinBlack(); //Returns 0 if no win or 1 if black can/did win

int moveWhite(int, int, int, int, int, int, int); //Call appropriate move function for given parameters
int moveBlack(int, int, int, int, int, int, int); //Call appropriate move function for given parameters
int playerMove(int); //Get and perform a player's move for given color and returns victory#
bool tryMoveWhite(int, int, int, bool); //Returns if white can make the given move and prints result for user
bool tryMoveBlack(int, int, int, bool); //Returns if black can make the given move and prints result for user
int playMoveWhite(int, int, int); //Lets white play the given move and returns victory#
int playMoveBlack(int, int, int); //Lets black play the given move and returns victory#

bool tryMoveQuickWhite(int, int, int); //Returns if white can make the given move without checking parameters except z
bool tryMoveQuickBlack(int, int, int); //Returns if black can make the given move without checking parameters except z
bool simulateMoveWhite(int, int, int); //Simulates the given move for white and returns 1 if it was a capture
bool simulateMoveBlack(int, int, int); //Simulates the given move for black and returns 1 if it was a capture
void unsimulateMoveWhite(int, int, int, bool); //Undoes the given move for white
void unsimulateMoveBlack(int, int, int, bool); //Undoes the given move for black

int countMovesWhite(); //Count possible moves for white
int countMovesBlack(); //Count possible moves for black
int pureRandomMoveWhite(); //Get a uniform-random move for white (simple ai)
int pureRandomMoveBlack(); //Get a uniform-random move for black (simple ai)
int tieredRandomMoveWhite(); //Get a random move for white, prioritizing wins, capturing a piece, then random moves
int tieredRandomMoveBlack(); //Get a random move for black, prioritizing wins, capturing a piece, then random moves
int smartRandomMoveWhite(int); //Get a random move for white, prioritizing wins, capturing furthest piece, then moving furthest 4 pieces
int smartRandomMoveBlack(int); //Get a random move for black, prioritizing wins, capturing furthest piece, then moving furthest 4 pieces

bool playOpenerWhite(int); //If opponent hasn't advanced too far, plays the next opening move and returns 1.
bool playOpenerBlack(int); //If opponent hasn't advanced too far, plays the next opening move and returns 1.
int miniMaxWhite(int, int, int, int, int, unsigned long long int&, unsigned long long int&); //Get a minimax move for white
int miniMaxBlack(int, int, int, int, int, unsigned long long int&, unsigned long long int&); //Get a minimax move for black
int minAlphaBeta(int, int, int, int, int, int, int, int, unsigned long long int&, unsigned long long int&); //Given a depth, recursively calculates the opponent's best next move
int maxAlphaBeta(int, int, int, int, int, int, int, int, unsigned long long int&, unsigned long long int&); //Given a depth, recursively calculates the AI's best next move
int evaluateBoard(int, int, int, int, int); //Evaluates the board assuming it's given color's turn, with parameters for multipliers

int main () {  //Play one game of Breakthrough
    srand(time(NULL));
    string boardFileStr = "";
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
    if (!reloadBoard(boardFileStr))
        boardFileStr = getBoard();
        
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

string getBoard() {  //Get the board from a specified file
    fstream boardFile;
    string boardFileStr;
    bool valid = false;

    //Get file name from user and open:
    while (!valid)
    {
        cout << "Enter name of file to read initial board state: ";
        cin >> boardFileStr;
        boardFile.open(boardFileStr);
        
        //Read the file into board[][]:
        for (int y = SIZE-1; y >= 0; y--)
            for (int x = 0; x < SIZE; x++)
                boardFile >> board[x][y];
        
        //Validate board:
        valid = true;
        for (int y = SIZE-1; y >= 0; y--)
            for (int x = 0; x < SIZE; x++)
                if (board[x][y] != EMPTY && board[x][y] != WHITE && board[x][y] != BLACK)
                {
                    valid = false;
                    y = -1;
                    cout << "Invalid file. Board place not read properly. Empty is \"" << EMPTY << "\", White is \"" << WHITE << "\", Black is \"" << BLACK << "\".\n";
                    break;
                }
    }
    boardFile.close();
    return boardFileStr;
}
bool reloadBoard(string boardFileStr) {  //Reloads the board from the given file
    fstream boardFile;
    boardFile.open(boardFileStr);
    
    //Reset board to blank:
    for (int y = SIZE-1; y >= 0; y--)
        for (int x = 0; x < SIZE; x++)
            board[x][y] = ' ';
    
    //Read the file into board[][]:
    for (int y = SIZE-1; y >= 0; y--)
        for (int x = 0; x < SIZE; x++)
            boardFile >> board[x][y];
    
    //Validate board:
    for (int y = SIZE-1; y >= 0; y--)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] != EMPTY && board[x][y] != WHITE && board[x][y] != BLACK)
            {
                cout << "Error reloading board.\n";
                return false;
            }
    boardFile.close();
    return true;
}
void printBoard() {  //Print the board
    cout << "    a b c d e f g h\n  / - - - - - - - - \\";
    for (int y = SIZE-1; y >= 0; y--)
    {
        cout << endl << y << " | ";
        for (int x = 0; x < SIZE; x++)
            cout << board[x][y] << ' ';
        cout << "| " << y;
    }
    cout << "\n  \\ - - - - - - - - /\n    a b c d e f g h" << endl;
    return;
}
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
    //Loop through board spaces in furthest row:
    int y = SIZE-1;
    for (int x = 0; x < SIZE; x++)
        if (board[x][y] == WHITE) //If space has our piece, we won
            return true;
    //Else if no win, make sure opponent still has pieces:
    for (y = SIZE-1; y >= 0; y--)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] == BLACK) //If space has their piece, we haven't won
                return false;
    //Else opponent can't move, we won
    return true;
}
bool canWinBlack() { //Returns 0 if no win or 1 if black can/did win
    //Loop through board spaces in furthest row:
    int y = 0;
    for (int x = 0; x < SIZE; x++)
        if (board[x][y] == BLACK) //If space has our piece, we won
            return true;
    //Else if no win, make sure opponent still has pieces:
    for (y = 0; y <= SIZE-1; y++)
        for (int x = 0; x < SIZE; x++)
            if (board[x][y] == WHITE) //If space has their piece, we haven't won
                return false;
    //Else opponent can't move, we won
    return true;
}

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
    
    //Play the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY+1] = WHITE;
    
    //If white reached the end, they won:
    if (moveY >= SIZE-2)
        return WhiteWin;
    
    //If white captured a piece, make sure it wasn't the last:
    if (isCapture)
    {
        //Loop through board places until a black piece is found:
        for (int y = SIZE-1; y >= 0; y--)
            for (int x = 0; x <= SIZE-1; x++)
                if (board[x][y] == BLACK)
                    return None;
        //No black piece found, white won by domination:
        return WhiteWin;
    }
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
    
    //Play the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY-1] = BLACK;
    
    //If black reached the end, they won:
    if (moveY <= 1)
        return BlackWin;
    
    //If black captured a piece, make sure it wasn't the last:
    if (isCapture)
    {
        //Loop through board places until a white piece is found:
        for (int y = 0; y <= SIZE-1; y++)
            for (int x = 0; x <= SIZE-1; x++)
                if (board[x][y] == BLACK)
                    return None;
        //No white piece found, black won by domination:
        return BlackWin;
    }
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
    
    //Simulate the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY+1] = WHITE;
    
    return isCapture;
}
bool simulateMoveBlack(int moveX1, int moveY, int moveX2) { //Simulates the given move for black and returns 1 if it was a capture
    bool isCapture;
    //If it's a capture, we have to return this later
    isCapture = (board[moveX2][moveY-1] == WHITE);
    
    //Simulate the move:
    board[moveX1][moveY] = EMPTY;
    board[moveX2][moveY-1] = BLACK;
    
    return isCapture;
}
void unsimulateMoveWhite(int moveX1, int moveY, int moveX2, bool isCapture) { //Undoes the given move for white
    //Undo the simulated move:
    board[moveX1][moveY] = WHITE;
    //If it's a capture, replace captured piece
    if (isCapture)
        board[moveX2][moveY+1] = BLACK;
    else
        board[moveX2][moveY+1] = EMPTY;
    return;
}
void unsimulateMoveBlack(int moveX1, int moveY, int moveX2, bool isCapture) { //Undoes the given move for black
    //Undo the simulated move:
    board[moveX1][moveY] = BLACK;
    //If it's a capture, replace captured piece
    if (isCapture)
        board[moveX2][moveY-1] = WHITE;
    else
        board[moveX2][moveY-1] = EMPTY;
    return;
}

int pureRandomMoveWhite() {  //Get a uniform-random move for white (simple ai)
    int moveX1, moveY, moveX2;
    int moveList[(SIZE-1)*SIZE*3][3];
    int availableMoves = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every possible move:
    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
            if (board[x][y] == WHITE) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveWhite(x, y, z, false))
                    { //Move is valid, list and count it:
                        moveList[availableMoves][0] = x;
                        moveList[availableMoves][1] = y;
                        moveList[availableMoves][2] = z;
                        availableMoves++;
                    }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        if (PRNT > 0)
        {
            cout << "Error finding a move.\n";
            printBoard();
        }
        return tieredRandomMoveWhite();
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "White (Uniform Random) ";
    if (tryMoveWhite(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveWhite(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "White (Uniform Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}
int pureRandomMoveBlack() {  //Get a uniform-random move for black (simple ai)
    int moveX1, moveY, moveX2;
    int moveList[(SIZE-1)*SIZE*3][3];
    int availableMoves = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every possible move:
    for (int y = 0; y < SIZE; y++)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
            if (board[x][y] == BLACK) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveBlack(x, y, z, false))
                    { //Move is valid, list and count it:
                        moveList[availableMoves][0] = x;
                        moveList[availableMoves][1] = y;
                        moveList[availableMoves][2] = z;
                        availableMoves++;
                    }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        if (PRNT > 0)
        {
            cout << "Error finding a move.\n";
            printBoard();
        }
        return tieredRandomMoveBlack();
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "Black (Uniform Random) ";
    if (tryMoveBlack(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveBlack(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "Black (Uniform Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}

int tieredRandomMoveWhite() {  //Get a random move for white, prioritizing wins, capturing a piece, then random moves
    int moveX1, moveY, moveX2;
    int moveList[(SIZE-2)*SIZE*3][3];
    enum MoveType {AnyMove, CaptureMove, WinMove};
    int moveType = AnyMove;
    int availableMoves = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every possible move:
    for (int y = SIZE-2; y >= 0; y--)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] == WHITE) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveWhite(x, y, z, false))
                    { //Move is valid, list and count it:
                        if (y == SIZE-2)
                        { //Move is a victory, prioritize it
                            moveType = WinMove;
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                        else if (board[z][y+1] == BLACK)
                        { //Move is a capture, prioritize it
                            if (moveType == AnyMove)
                            { //First discovered capture, remove previous non-captures
                                moveType = CaptureMove;
                                availableMoves = 0;
                            }
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                        else if (moveType == AnyMove)
                        { //Move is a standard move
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                    }
        }
        if (y == SIZE-2 && availableMoves > 0)
        { //If we found a winning move, play it:
            break;
        }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        cout << "Error finding a move.\n";
        printBoard();
        return BlackWin;
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "White (Tiered Random) ";
    if (tryMoveWhite(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveWhite(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "White (Tiered Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}
int tieredRandomMoveBlack() {  //Get a random move for black, prioritizing wins, capturing a piece, then random moves
    int moveX1, moveY, moveX2;
    int moveList[(SIZE-2)*SIZE*3][3];
    enum MoveType {AnyMove, CaptureMove, WinMove};
    int moveType = AnyMove;
    int availableMoves = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every possible move:
    for (int y = 1; y <= SIZE-1; y++)
    {
        for (int x = 0; x < SIZE; x++) //Loop through board spaces:
        {
            if (board[x][y] == BLACK) //If space has our piece, try to move:
                for (int z = x-1; z <= x+1; z++) //Try each direction:
                    if (tryMoveBlack(x, y, z, false))
                    { //Move is valid, list and count it:
                        if (y == 1)
                        { //Move is a victory, prioritize it
                            moveType = WinMove;
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                        else if (board[z][y-1] == WHITE)
                        { //Move is a capture, prioritize it
                            if (moveType == AnyMove)
                            { //First discovered capture, remove previous non-captures
                                moveType = CaptureMove;
                                availableMoves = 0;
                            }
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                        else if (moveType == AnyMove)
                        { //Move is a standard move
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
                    }
        }
        if (y == 1 && availableMoves > 0)
        { //If we found a winning move, play it:
            break;
        }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        cout << "Error finding a move.\n";
        printBoard();
        return WhiteWin;
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "Black (Tiered Random) ";
    if (tryMoveBlack(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveBlack(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "Black (Tiered Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}

int smartRandomMoveWhite(int furthestCount) {  //Get a random move for white, prioritizing wins, capturing furthest piece, then moving furthest 4 pieces
    int moveX1, moveY, moveX2;
    int moveList[12*3][3];
    int availableMoves = 0;
    int forwardPieces = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every move of prioritized category:
    moveX1 = findWinWhite();
    if (moveX1 > -1)
    { //There is a winning move, list available directions for winning piece:
        moveY = SIZE-2;
        for (int z = moveX1 - 1; z <= moveX1+1; z++)
        {
            if (tryMoveWhite(moveX1, moveY, z, false))
            {
                moveList[availableMoves][0] = moveX1;
                moveList[availableMoves][1] = moveY;
                moveList[availableMoves][2] = z;
                availableMoves++;
            }
        }
    }
    else //if no winning move, find another move:
    {
        //List and count first row of defensive captures:
        for (int y = 0; y <= SIZE-3; y++)
        {
            for (int x = 0; x < SIZE; x++) //Loop through board spaces:
                if (board[x][y] == WHITE) //If space has our piece, try to move:
                    for (int z = x-1; z <= x+1; z+=2) //Try each direction:
                        if (tryMoveWhite(x, y, z, false) && board[z][y+1] == BLACK)
                        { //Move is a valid capture, list and count it:
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
            
            //If a capture was found, stop searching:
            if (availableMoves > 0)
            {
                break;
            }
        }
        
        //If no capture move, then list and count at least 4 furthest moves: 
        if (availableMoves == 0)
        {
            for (int y = SIZE-3; y >= 0; y--)
            {
                for (int x = 0; x < SIZE; x++) //Loop through board spaces:
                    if (board[x][y] == WHITE) //If space has our piece, try to move:
                        for (int z = x-1; z <= x+1; z+=2) //Try each direction:
                            if (tryMoveWhite(x, y, z, false))
                            { //Move is valid, list and count it:
                                if (availableMoves == 0 || (moveList[availableMoves-1][0] != x || moveList[availableMoves-1][1] != y))
                                    forwardPieces++; //This is new piece, count it
                                moveList[availableMoves][0] = x;
                                moveList[availableMoves][1] = y;
                                moveList[availableMoves][2] = z;
                                availableMoves++;
                            }
                
                //If enough pieces found, stop searching:
                if (forwardPieces >= furthestCount)
                {
                    break;
                }
            }
        }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        if (PRNT > 0)
        {
            cout << "Error finding a move.\n";
            printBoard();
        }
        return tieredRandomMoveWhite();
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "White (Smart Random) ";
    if (tryMoveWhite(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveWhite(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "White (Smart Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}
int smartRandomMoveBlack(int furthestCount) {  //Get a random move for black, prioritizing wins, capturing furthest piece, then moving furthest 4 pieces
    int moveX1, moveY, moveX2;
    int moveList[12*3][3];
    int availableMoves = 0;
    int forwardPieces = 0;
    int chosenMove = 0;
    int victor = 0;
    
    //List and count every move of prioritized category:
    moveX1 = findWinBlack();
    if (moveX1 > -1)
    { //There is a winning move, list available directions for winning piece:
        moveY = 1;
        for (int z = moveX1 - 1; z <= moveX1+1; z++)
        {
            if (tryMoveBlack(moveX1, moveY, z, false))
            {
                moveList[availableMoves][0] = moveX1;
                moveList[availableMoves][1] = moveY;
                moveList[availableMoves][2] = z;
                availableMoves++;
            }
        }
    }
    else //if no winning move, find another move:
    {
        //List and count first row of defensive captures:
        for (int y = SIZE-1; y >= 2; y--)
        {
            for (int x = 0; x < SIZE; x++) //Loop through board spaces:
                if (board[x][y] == BLACK) //If space has our piece, try to move:
                    for (int z = x-1; z <= x+1; z+=2) //Try each direction:
                        if (tryMoveBlack(x, y, z, false) && board[z][y-1] == WHITE)
                        { //Move is a valid capture, list and count it:
                            moveList[availableMoves][0] = x;
                            moveList[availableMoves][1] = y;
                            moveList[availableMoves][2] = z;
                            availableMoves++;
                        }
            
            //If a capture was found, stop searching:
            if (availableMoves > 0)
            {
                break;
            }
        }
        
        //If no capture move, then list and count at least 4 furthest moves: 
        if (availableMoves == 0)
        {
            for (int y = 2; y <= SIZE-1; y++)
            {
                for (int x = 0; x < SIZE; x++) //Loop through board spaces:
                    if (board[x][y] == BLACK) //If space has our piece, try to move:
                        for (int z = x-1; z <= x+1; z+=2) //Try each direction:
                            if (tryMoveBlack(x, y, z, false))
                            { //Move is valid, list and count it:
                                if (availableMoves == 0 || (moveList[availableMoves-1][0] != x || moveList[availableMoves-1][1] != y))
                                    forwardPieces++; //This is new piece, count it
                                moveList[availableMoves][0] = x;
                                moveList[availableMoves][1] = y;
                                moveList[availableMoves][2] = z;
                                availableMoves++;
                            }
                
                //If enough pieces found, stop searching:
                if (forwardPieces >= furthestCount)
                {
                    break;
                }
            }
        }
    }
    
    //Choose a random move from the list:
    if (availableMoves < 1)
    {
        if (PRNT > 0)
        {
            cout << "Error finding a move.\n";
            printBoard();
        }
        return tieredRandomMoveBlack();
    }
    chosenMove = rand() % availableMoves;
    moveX1 = moveList[chosenMove][0];
    moveY  = moveList[chosenMove][1];
    moveX2 = moveList[chosenMove][2];
    
    //Play chosen move:
    if (PRNT > 1)
        cout << "Black (Smart Random) ";
    if (tryMoveBlack(moveX1, moveY, moveX2, false))
    { //Move is valid, play it:
        if (PRNT > 1)
            cout << "played: ";
        victor = playMoveBlack(moveX1, moveY, moveX2);
    }
    else
    { //Move is invalid, report it:
        victor = None;
        if (PRNT > 0)
        {
            if (PRNT == 1)
                cout << "Black (Smart Random) ";
            cout << "move Invalid, tried: ";
            cout << (char)('a'+moveX1) << moveY << (char)('a'+moveX2);
            if (PRNT == 1)
                printBoard();
        }
    }
    return victor;
}

int evaluateBoard(int turnColor, int turnWeight, int chipDiffWeight, int wallWeight, int columnWeight) { //Evaluates the board assuming it's given color's turn, with parameters for multipliers
    int y = 0, x = 0;
    int eval = 0;
    
    //If a piece is at the end of the board, that color won:
    for (x = 0; x < SIZE; x++)
        if (board[x][y] == BLACK)
            return BlackWin;
    y = SIZE-1;
    for (x = 0; x < SIZE; x++)
        if (board[x][y] == WHITE)
            return WhiteWin;
    
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
    
    return chipDiff()*chipDiffWeight + eval;
}

bool playOpenerWhite(int openerCode) { //If opponent hasn't advanced too far, plays the next opening move and returns 1.
    if (openerCode <= StandardOpener)
        return false;

    //If our half of the board has their piece, don't play next opener move:
    for (int y = 0; y <= 3; y++)
        for (int x = 0; x <= SIZE-1; x++)
            if (board[x][y] == BLACK)
                return false;
    
    //Else, play our next opener move:
    if (openerCode == OffensiveOpener) {
        for (int x = SIZE-1; x >= 0; x--)
        {
            if (board[x][1] == WHITE)
            {
                if (x >= SIZE-2)
                {
                    playMoveWhite(x, 1, x-1);
                    return true;
                }
                if (x >= 3 && x <= SIZE-4)
                {
                    playMoveWhite(x, 1, x);
                    return true;
                }
                if (x <= 1)
                {
                    playMoveWhite(x, 1, x+1);
                    return true;
                }
            }
        }
        return false;
    }
    if (openerCode == DefensiveOpener) {
        if (board[SIZE-1][0] == WHITE)
        {
            if (board[SIZE-2][1] == WHITE)
                playMoveWhite(SIZE-2, 1, SIZE-3);
            else
                playMoveWhite(SIZE-1, 0, SIZE-2);
            return true;
        }
        if (board[0][0] == WHITE)
        {
            if (board[1][1] == WHITE)
                playMoveWhite(1, 1, 2);
            else
                playMoveWhite(0, 0, 1);
            return true;
        }
        if (board[SIZE-1][1] == WHITE)
        {
            playMoveWhite(SIZE-1, 1, SIZE-2);
            return true;
        }
        if (board[0][1] == WHITE)
        {
            playMoveWhite(0, 1, 1);
            return true;
        }
        //If none of previous moves worked, opener is done:
        return false;
    }
    return false;
}
bool playOpenerBlack(int openerCode) { //If opponent hasn't advanced too far, plays the next opening move and returns 1.
    if (openerCode <= StandardOpener)
        return false;

    //If our half of the board has their piece, don't play next opener move:
    for (int y = SIZE-1; y >= SIZE-4; y--)
        for (int x = 0; x <= SIZE-1; x++)
            if (board[x][y] == WHITE)
                return false;
    
    //Else, play our next opener move:
    if (openerCode == OffensiveOpener) {
        for (int x = SIZE-1; x >= 0; x--)
        {
            if (board[x][SIZE-2] == BLACK)
            {
                if (x >= SIZE-2)
                {
                    playMoveBlack(x, SIZE-2, x-1);
                    return true;
                }
                if (x >= 3 && x <= SIZE-4)
                {
                    playMoveBlack(x, SIZE-2, x);
                    return true;
                }
                if (x <= 1)
                {
                    playMoveBlack(x, SIZE-2, x+1);
                    return true;
                }
            }
        }
        return false;
    }
    if (openerCode == DefensiveOpener) {
        if (board[SIZE-1][SIZE-1] == BLACK)
        {
            if (board[SIZE-2][SIZE-2] == BLACK)
                playMoveBlack(SIZE-2, SIZE-2, SIZE-3);
            else
                playMoveBlack(SIZE-1, SIZE-1, SIZE-2);
            return true;
        }
        if (board[0][SIZE-1] == BLACK)
        {
            if (board[1][SIZE-2] == BLACK)
                playMoveBlack(1, SIZE-2, 2);
            else
                playMoveBlack(0, SIZE-1, 1);
            return true;
        }
        if (board[SIZE-1][SIZE-2] == BLACK)
        {
            playMoveBlack(SIZE-1, SIZE-2, SIZE-2);
            return true;
        }
        if (board[0][SIZE-2] == BLACK)
        {
            playMoveBlack(0, SIZE-2, 1);
            return true;
        }
        //If none of previous moves worked, opener is done:
        return false;
    }
    return false;
}

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
