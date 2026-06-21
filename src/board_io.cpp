#include "board_io.h"
#include "ai_eval.h"

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
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) {
                g_whiteCount++;
                g_chipDiff++;
                if (y == SIZE-1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++;
                g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
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
    g_whiteCount = 0;
    g_blackCount = 0;
    g_chipDiff   = 0;
    g_whiteAtEnd = 0;
    g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) {
                g_whiteCount++;
                g_chipDiff++;
                if (y == SIZE-1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++;
                g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
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
// Load saved MiniMax settings for one side from a key=value file. Fills depth,
// evaluator (index into g_evaluators), opener, and the chosen evaluator's params
// (params must have room for MAX_EVAL_PARAMS ints). Each param is read from
// "<prefix>_<key>", falling back to the legacy "<prefix>_<key>_weight" name so
// older files (white_turn_weight, ...) still load. Missing values use the
// parameter's registry default; missing depth/opener stay -1/NullOpener so the
// caller can prompt for them.
bool loadMinimaxParams(const string& filename, int& depth, int& evaluator, int* params, int& opener, const string& prefix) {
    ifstream f(filename);
    if (!f.is_open()) return false;
    map<string, int> vals;
    string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == string::npos) continue;
        string key = line.substr(0, eq);
        try { vals[key] = std::stoi(line.substr(eq + 1)); } catch (...) {}
    }
    depth  = vals.count(prefix+"_depth")  ? vals[prefix+"_depth"]  : -1;
    opener = vals.count(prefix+"_opener") ? vals[prefix+"_opener"] : NullOpener;

    evaluator = vals.count(prefix+"_eval") ? vals[prefix+"_eval"] : 0;
    if (evaluator < 0 || evaluator >= g_evalCount) evaluator = 0;

    const EvalDef& e = g_evaluators[evaluator];
    for (int i = 0; i < e.paramCount; i++) {
        string k = prefix + "_" + e.params[i].key;
        if (vals.count(k))                 params[i] = vals[k];
        else if (vals.count(k + "_weight")) params[i] = vals[k + "_weight"];
        else                                params[i] = e.params[i].def;
    }
    return true;
}
