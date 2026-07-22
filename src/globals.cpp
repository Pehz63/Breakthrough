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
int g_rowCountW[SIZE] = { 0 };
int g_rowCountB[SIZE] = { 0 };
bool g_evalRowCounts = false;
unsigned long long g_noiseAcc = 0;
bool g_noiseIncremental = false;
int g_noiseSeed = 0;
int g_evalLevel = 3;   // benchmark-only leaf-generation selector (see globals.h)
double g_mlAcc = 0.0;
bool g_mlIncremental = false;
const float* g_mlWeights = nullptr;
int g_mlAccDim = 0;                        // 0 = scalar/linear path; >0 = MLP first-hidden width
double* g_mlAccVec = nullptr;              // length-g_mlAccDim layer-0 pre-activation accumulator
const float* g_mlL0ByInput = nullptr;      // input-major layer-0 weights (idx*g_mlAccDim + unit)
int g_downEvalWhite = 0;
int g_downEvalBlack = 0;
unsigned long long g_nodeBudget = 0;      // 0 = unlimited
unsigned long long g_nodeDeadline = 0;    // per-search cutoff (set by miniMax*)
double g_timeBudgetMs = 0.0;              // 0 = off

bool g_useAlphaBeta = true;
bool g_useTT = false;
bool g_useMoveOrder = false;
bool g_useQuiescence = false;
bool g_keepPartial = false;
int  g_aspirationWindow = 0;

double g_lastEffDepth = 0.0;
int    g_lastBudgetKind = BUDGET_NONE;
unsigned long long g_lastNodes = 0;
unsigned long long g_lastLeafs = 0;
int SHOW_EVAL = -1;
