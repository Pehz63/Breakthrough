#include "ai_random.h"
#include "moves.h"
#include "board_analysis.h"
#include "datastore.h"   // positionKey (book-opener lookup key)
#include <map>
#include <set>
#include <fstream>
#include <sstream>

// ============================================================
// UNIFORM RANDOM -- pureRandomMove*
// ============================================================
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

// ============================================================
// TIERED RANDOM -- tieredRandomMove*
// ============================================================
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

// ============================================================
// SMART RANDOM -- smartRandomMove*
// ============================================================
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

// ============================================================
// OPENERS -- playOpener*
// ============================================================
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

// ============================================================
// OPENER REGISTRY -- g_openers
// ============================================================
// The "rand" opener: play a uniform-random legal move for the agent's own first
// `arg` plies, then defer to the brain.
static bool openerRandom(int side, int ownPly, int arg, int& victor) {
    if (ownPly >= arg) return false;
    victor = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
    return true;
}

// The "book" opener: play the stored reply for the current position from the
// book file models/book<arg>.txt, handing off to the brain when out of book.
// A book line is "<positionKey hash, 16 hex digits> <sx> <sy> <dx>" (the
// destination row is implied by the mover's direction); '#' lines are
// comments. Entries are keyed by positionKey(sideToMove, false) -- the same
// canonical key the TT uses -- so an entry fires for whichever agent reaches
// the position with the move. Books load lazily once per process and are
// treated as IMMUTABLE data: the opener ID does not hash the file the way
// learned() hashes a model, so editing a book under the same <arg> would
// silently change that agent's identity-play mapping. Give a new book a new
// arg instead. A stored move that is not legal in the position (corrupt or
// stale book) is ignored: the opener returns false and the brain plays.
struct BookEntry { int sx, sy, dx; };
static std::map<int, std::map<unsigned long long, BookEntry> > s_books;
static std::set<int> s_bookLoadTried;

static const std::map<unsigned long long, BookEntry>* bookForSlot(int arg) {
    if (s_bookLoadTried.find(arg) == s_bookLoadTried.end()) {
        s_bookLoadTried.insert(arg);
        std::ostringstream fn;
        fn << "models/book" << arg << ".txt";
        std::ifstream f(fn.str().c_str());
        if (f.is_open()) {
            std::map<unsigned long long, BookEntry>& bk = s_books[arg];
            string line;
            while (std::getline(f, line)) {
                if (line.empty() || line[0] == '#') continue;
                std::istringstream ls(line);
                string hex;
                BookEntry e;
                if (!(ls >> hex >> e.sx >> e.sy >> e.dx)) continue;
                unsigned long long h = strtoull(hex.c_str(), nullptr, 16);
                if (bk.find(h) == bk.end()) bk[h] = e;
            }
        }
    }
    std::map<int, std::map<unsigned long long, BookEntry> >::const_iterator it = s_books.find(arg);
    return (it == s_books.end()) ? nullptr : &it->second;
}

static bool openerBook(int side, int ownPly, int arg, int& victor) {
    (void)ownPly;   // the book plays whenever the position matches, at any ply
    const std::map<unsigned long long, BookEntry>* bk = bookForSlot(arg);
    if (!bk) return false;
    unsigned long long h = positionKey(side, false).hash;
    std::map<unsigned long long, BookEntry>::const_iterator it = bk->find(h);
    if (it == bk->end()) return false;
    const BookEntry& e = it->second;
    // Bounds first: tryMoveQuick* skips bounds checks by design, and a corrupt
    // book must never index off the board.
    if (e.sx < 0 || e.sx >= SIZE || e.sy < 0 || e.sy >= SIZE
        || e.dx < 0 || e.dx >= SIZE || e.dx < e.sx - 1 || e.dx > e.sx + 1) return false;
    bool legal = (side == White) ? tryMoveQuickWhite(e.sx, e.sy, e.dx)
                                 : tryMoveQuickBlack(e.sx, e.sy, e.dx);
    if (!legal) return false;
    victor = (side == White) ? playMoveWhite(e.sx, e.sy, e.dx)
                             : playMoveBlack(e.sx, e.sy, e.dx);
    return true;
}

const OpenerDef g_openers[] = {
    { "Random", "rand", "uniform-random move for the agent's first <arg> plies, then hand off", true, openerRandom },
    { "Book",   "book", "opening-book follower: play models/book<arg>.txt's stored reply while the position is in book, then hand off", true, openerBook },
};
const int g_openerCount = (int)(sizeof(g_openers) / sizeof(g_openers[0]));

int openerIndexByIdName(const char* idName) {
    if (!idName) return -1;
    for (int i = 0; i < g_openerCount; i++) {
        const char* a = g_openers[i].idName;
        const char* b = idName;
        while (*a && *a == *b) { a++; b++; }
        if (*a == '\0' && *b == '\0') return i;
    }
    return -1;
}
