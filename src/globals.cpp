#include "globals.h"

int PRNT = -1;

int p1Default = 1;
int p2Default = 1;
int p3Default = 1;
int p4Default = 0;
int p5Default = 0;

char chipChr[3] = {'.', 'W', 'B'};
char board[SIZE][SIZE];
unsigned long long int nodesWhite = 0;
unsigned long long int nodesBlack = 0;
int g_whiteCount = 0;
int g_blackCount = 0;
int g_chipDiff   = 0;
int g_whiteAtEnd = 0;
int g_blackAtEnd = 0;
int g_evalPos = 0;
bool g_evalIncremental = false;
const int* g_activeParams = nullptr;
int g_activeParamCount = 0;
