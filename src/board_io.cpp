#include "board_io.h"

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
bool loadMinimaxParams(const string& filename, int& depth, int& turnW, int& chipW, int& wallW, int& colW, int& opener, const string& prefix) {
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
    depth  = vals.count(prefix+"_depth")         ? vals[prefix+"_depth"]         : -1;
    turnW  = vals.count(prefix+"_turn_weight")   ? vals[prefix+"_turn_weight"]   : -1;
    chipW  = vals.count(prefix+"_chip_weight")   ? vals[prefix+"_chip_weight"]   : -1;
    wallW  = vals.count(prefix+"_wall_weight")   ? vals[prefix+"_wall_weight"]   : -1;
    colW   = vals.count(prefix+"_column_weight") ? vals[prefix+"_column_weight"] : -1;
    opener = vals.count(prefix+"_opener")        ? vals[prefix+"_opener"]        : NullOpener;
    return true;
}
