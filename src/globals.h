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

enum VictorEnum {None = 0, White = 1, Black = -1, WhiteWin = INT_MAX-1, BlackWin = INT_MIN+1};
enum PlayerEnum {NullPlayer = -1, Human = 0, UniformRandom = 1, TieredRandom = 2, SmartRandom = 3, MiniMax = 4};
enum OpenerEnum {NullOpener = -1, StandardOpener = 0, OffensiveOpener = 1, DefensiveOpener = 2};

bool loadMinimaxParams(const string&, int&, int&, int&, int&, int&, int&, const string&);
string getBoard();
bool reloadBoard(string);
void printBoard();
void getSettings(int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&, int&);
void printVictor(int, int, int, int);

int countChips(int);
int countChips();
int chipDiff(int);
int chipDiff();
int findWinWhite();
int findWinBlack();
bool canWinWhite();
bool canWinBlack();

int moveWhite(int, int, int, int, int, int, int);
int moveBlack(int, int, int, int, int, int, int);
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
int miniMaxWhite(int, int, int, int, int, unsigned long long int&, unsigned long long int&);
int miniMaxBlack(int, int, int, int, int, unsigned long long int&, unsigned long long int&);
int minAlphaBeta(int, int, int, int, int, int, int, int, unsigned long long int&, unsigned long long int&);
int maxAlphaBeta(int, int, int, int, int, int, int, int, unsigned long long int&, unsigned long long int&);
int evaluateBoard(int, int, int, int, int);
