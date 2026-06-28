#pragma once

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <ctime>
#include <limits.h>

using std::cout;
using std::cin;
using std::endl;
using std::flush;
using std::string;
using std::fstream;
using std::ifstream;
using std::map;

#define EMPTY   '.'
#define WHITE   'W'
#define BLACK   'B'
#define SIZE    8

// Maximum number of parameters any board evaluator can declare (see ai_eval.h).
// Defined here so callers that thread evaluator parameter arrays only need globals.h.
#define MAX_EVAL_PARAMS 8

extern int PRNT;
extern int p1Default;
extern int p2Default;
extern int p3Default;
extern int p4Default;
extern int p5Default;
extern char chipChr[3];
extern char board[SIZE][SIZE];
extern unsigned long long int nodesWhite;
extern unsigned long long int nodesBlack;
extern int g_whiteCount;
extern int g_blackCount;
extern int g_chipDiff;
extern int g_whiteAtEnd;
extern int g_blackAtEnd;

// Incremental evaluation state (maintained during a minimax search):
//   g_evalPos          running positional score (structure + advance) of the board
//   g_evalIncremental  true while an incremental search is active (gates make/unmake updates)
//   g_activeParams     the active evaluator's weight array
//   g_activeParamCount its parameter count
extern int g_evalPos;
extern bool g_evalIncremental;
extern const int* g_activeParams;
extern int g_activeParamCount;

// Last minimax best-line ("predicted downstream") evaluations, white-centric.
// Set by miniMaxWhite/Black from the root alpha/beta; surfaced by the UIs.
extern int g_downEvalWhite;
extern int g_downEvalBlack;

// Per-move search node budget. g_nodeBudget = 0 means unlimited (default; console/GUI
// unchanged). When > 0, miniMaxWhite/Black seed g_nodeDeadline = nodes + g_nodeBudget at
// the start of a search, and maxAlphaBeta/minAlphaBeta treat a node as a leaf once the
// per-move node count reaches the deadline. Lets "depth D" agents stay bounded so a
// depth-laddered tournament up to depth 10 is tractable.
extern unsigned long long g_nodeBudget;
extern unsigned long long g_nodeDeadline;

// Console toggle: 1 = print per-move board evaluations, 0 = hide them.
extern int SHOW_EVAL;

enum VictorEnum {None = 0, White = 1, Black = -1, WhiteWin = INT_MAX-1, BlackWin = INT_MIN+1};
enum PlayerEnum {NullPlayer = -1, Human = 0, UniformRandom = 1, TieredRandom = 2, SmartRandom = 3, MiniMax = 4};
enum OpenerEnum {NullOpener = -1, StandardOpener = 0, OffensiveOpener = 1, DefensiveOpener = 2};

bool loadMinimaxParams(const string&, int&, int&, int*, int&, const string&);
string getBoard();
bool reloadBoard(string);
void printBoard();
void getSettings(int&, int&, int&, int*, int&, int&, int&, int&, int*, int&, int&, int&, int&);
void printVictor(int, int, int, int);

int countChips(int);
int countChips();
int chipDiff(int);
int chipDiff();
int findWinWhite();
int findWinBlack();
bool canWinWhite();
bool canWinBlack();

int moveWhite(int, int, int, const int*, int);
int moveBlack(int, int, int, const int*, int);
int playerMove(int);
bool tryMoveWhite(int, int, int, bool);
bool tryMoveBlack(int, int, int, bool);
int playMoveWhite(int, int, int);
int playMoveBlack(int, int, int);

bool tryMoveQuickWhite(int, int, int);
bool tryMoveQuickBlack(int, int, int);
bool simulateMoveWhite(int, int, int);
bool simulateMoveBlack(int, int, int);
void unsimulateMoveWhite(int, int, int, bool);
void unsimulateMoveBlack(int, int, int, bool);

int countMovesWhite();
int countMovesBlack();
int pureRandomMoveWhite();
int pureRandomMoveBlack();
int tieredRandomMoveWhite();
int tieredRandomMoveBlack();
int smartRandomMoveWhite(int);
int smartRandomMoveBlack(int);

bool playOpenerWhite(int);
bool playOpenerBlack(int);
int miniMaxWhite(int, int, const int*, unsigned long long int&, unsigned long long int&);
int miniMaxBlack(int, int, const int*, unsigned long long int&, unsigned long long int&);
int minAlphaBeta(int, int, int, int, int, const int*, unsigned long long int&, unsigned long long int&);
int maxAlphaBeta(int, int, int, int, int, const int*, unsigned long long int&, unsigned long long int&);
int evaluateBoard(int, int, const int*);
int evaluateBoard(int, int, int, int, int);
int evalPosFull(const int*, int);
int evalPosLocal(int, int, int, int);
int evalLeaf(int, int, const int*);
void evalBeginSearch(int, const int*);
void evalEndSearch();
int immediateEvalForDisplay(bool, int, const int*);
void mlAutoLoadDefaultSlots();  // load default trained models into slots (see ml_eval.h)
