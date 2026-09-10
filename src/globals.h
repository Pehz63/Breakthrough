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
#define MAX_EVAL_PARAMS 20

// Capacity of the root-move whitelist below. Matches ML_MAX_MOVES (ml_features.h),
// which is the same bound the move generator writes into, and is defined here so
// globals.h stays free of ML includes. A position can never have more legal moves
// than this: 16 pieces x 3 directions = 48 is the structural maximum.
#define ROOT_FILTER_MAX 64

// "Not set yet" sentinel for evaluator parameter arrays awaiting user input.
// Must lie below every parameter's registry minimum: getEvaluatorSettings
// prompts only for params outside their [lo, hi] range, and evaluators with
// negative minimums (Advanced allows -99) made the old -1 sentinel a valid
// value, silently skipping those prompts.
#define EVAL_PARAM_UNSET INT_MIN

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
//   g_evalPos          running positional score (structure + forward) of the board
//   g_evalIncremental  true while an incremental search is active (gates make/unmake updates)
//   g_activeParams     the active evaluator's weight array
//   g_activeParamCount its parameter count
extern int g_evalPos;
extern bool g_evalIncremental;
extern const int* g_activeParams;
extern int g_activeParamCount;

// Per-row piece counts, maintained by simulate/unsimulate only while
// g_evalRowCounts is set (the Advanced evaluator's Race / RaceWin terms need the
// board's row extremes at each leaf; a move touches at most 3 row counts).
// Seeded per search by evalBeginSearch, inert everywhere else.
extern int g_rowCountW[SIZE];
extern int g_rowCountB[SIZE];
extern bool g_evalRowCounts;

// Bounded per-position jitter state (Advanced evaluator, Noise < 0): the
// running sum of every occupied square's noiseHashRaw value, maintained by
// simulate/unsimulate while g_noiseIncremental is set (2-3 adds per move, the
// g_mlAcc pattern) and read at the leaf as (g_noiseAcc mod (2*mag+1)) - mag.
// Unsigned wrap-around in the intermediate adds is well-defined and cancels
// exactly on unmake. g_noiseSeed is latched per search by evalBeginSearch.
extern unsigned long long g_noiseAcc;
extern bool g_noiseIncremental;
extern int g_noiseSeed;
unsigned noiseHashRaw(int, char, int, int);
unsigned long long noiseRawScan(int);

// Benchmark-only eval-level selector (default 3 = current shipping behavior).
// Reconstructs prior generations of the heuristic leaf for speed measurement:
//   1 = full-board chip rescan (chipDiff()) + full evalPosFull scan per leaf
//   2 = incremental g_chipDiff + full evalPosFull scan per leaf
//   3 = incremental g_chipDiff + cached g_evalPos (the normal engine path)
// Set only by train.exe's speed benchmark and restored to 3 afterward; inert
// in the console, GUI, tests, and tournaments.
extern int g_evalLevel;

// Incremental ML value state (maintained during a minimax search when the
// LearnedValue evaluator holds a sparse piece-square model, feature version 2):
//   g_mlAcc          running dot product of the board's piece-square inputs with
//                    the model weights, bias included (double so add/subtract
//                    drift stays far below the eval's integer resolution)
//   g_mlIncremental  true while an ML-incremental search is active (gates the
//                    make/unmake updates in moves.cpp)
//   g_mlWeights      the active model's weight array (indexed by mlSqW/mlSqB)
// Seeded/cleared by mlIncrementalBegin/End (ml_eval.cpp) via evalBegin/EndSearch.
extern double g_mlAcc;
extern bool g_mlIncremental;
extern const float* g_mlWeights;

// NNUE-style vector accumulator for an MLP mu head (feature version 2). When the
// LearnedValue slot holds an MLP (or a DistModel/Residual wrapping one), the scalar
// g_mlAcc above is replaced by a per-first-hidden-unit accumulator of the layer-0
// pre-activations, and only the (small) remaining layers run at each leaf:
//   g_mlAccDim     first-hidden width H; 0 = the scalar/linear path above is active,
//                  >0 = MLP vector path with this many units
//   g_mlAccVec     length-H accumulator: B[0][j] + sum of layer-0 weights of the
//                  occupied piece-squares (side-to-move EXCLUDED, applied at read
//                  time), doubles so add/subtract drift stays negligible
//   g_mlL0ByInput  layer-0 weights stored INPUT-major ([MLV2_FEATURES][H], i.e.
//                  g_mlL0ByInput[idx*H + j]) so a touched input idx is a contiguous
//                  length-H column add/subtract; column idx == MLV2_STM is the
//                  side-to-move column, added only at leaf read
// g_mlAccVec / g_mlL0ByInput point into std::vector buffers owned by ml_eval.cpp,
// resized once per search by mlIncrementalBegin (never on the make/unmake hot path).
extern int g_mlAccDim;
extern double* g_mlAccVec;
extern const float* g_mlL0ByInput;

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

// Per-move wall-clock budget in milliseconds. g_timeBudgetMs = 0 means off (default).
// When > 0, miniMaxWhite/Black seed a steady_clock deadline at the start of a search
// and maxAlphaBeta/minAlphaBeta treat a node as a leaf once the deadline passes
// (checked on a node-count mask to avoid per-node clock reads). Composes with the
// node budget: whichever cap trips first ends the search.
extern double g_timeBudgetMs;

// Per-search feature toggles, set by agentChooseMove (saved/restored around the call)
// so an agent can enable/disable an optimization for ablation comparisons. Defaults
// (true/0) reproduce the historical behavior for the console and GUI.
extern bool g_useAlphaBeta;     // false = full minimax (no alpha/beta cutoffs)
extern bool g_useTT;            // transposition table probe/store
extern bool g_useMoveOrder;     // TT/killer/history move ordering (capture-first always on)
extern bool g_useQuiescence;    // captures-only stand-pat extension at depth leaves
extern bool g_keepPartial;      // keep a budget-cut iteration's best move instead of discarding
extern int  g_aspirationWindow; // 0 = full window; >0 = aspiration half-width at the root
// Percentage of the NODE budget that must still be unspent for iterative deepening
// to begin another iteration. 0 disables the check, which is the historical
// behaviour: start every iteration and let the budget cut it mid-flight.
// Per-ply node cost grows about 4x here, so an iteration started with less than
// roughly 76% of the budget left cannot finish, and every node it spends is thrown
// away (or, under g_keepPartial, adopted on a score that is partly unsearched).
// Only consulted when a node budget is set; a wall-clock budget has its own
// predictive check in nextIterationFits.
extern int  g_iterMinRemain;

// Per-side unspent-node carry, the state behind the `retain` ab() flag. A budgeted
// search that stops early (the g_iterMinRemain gate declined an iteration, the
// deep= ceiling was reached, or nearWinCheck short-circuited) leaves part of its
// per-move node cap unused. With retain on, that remainder is added to the SAME
// side's cap on its next move instead of being forfeited, so the budget is
// conserved per GAME rather than per move: total nodes over a game stay bounded by
// (per-move cap) x (plies), while individual moves may spend several caps' worth.
// Indexed by rankRetainSlot(side): 0 = White, 1 = Black, so the two agents in one
// game keep separate purses. agentChooseMove owns both the read and the write;
// nothing in the search itself touches these. Reset with retainResetCarry() at the
// start of every game -- a carry that survives into the next game would make an
// agent's play depend on which games the worker happened to run first, the same
// defect the per-game ttClear() exists to prevent.
extern unsigned long long g_nodeCarry[2];
// Wall-clock counterpart to g_nodeCarry, in milliseconds. `retain` on a time-
// budgeted head banks whatever the search did not spend and adds it to the same
// side's next move, exactly as the node purse does. It exists because the time
// side has its own pre-iteration gate (nextIterationFits, src/ai_minimax.cpp),
// which DECLINES an iteration it predicts will not fit and therefore leaves the
// tail of the budget unspent -- the same condition rem= creates on the node side.
extern double g_timeCarry[2];
void retainResetCarry();

// Root-move whitelist, the "filter mode" of the cluster-book opener (`cbook`,
// src/ai_random.cpp). When g_useRootFilter is set, searchRootWhite/searchRootBlack
// skip any root candidate not listed in g_rootMoveWhitelist, so the agent's normal
// node/time budget is spent going deeper on fewer moves. This is a ROOT-only
// restriction: the recursive maxAlphaBeta/minAlphaBeta never consult it, matching
// how every opener only ever decides the current ply. A move is stored as the same
// (source x, source y, destination x) triple the root loop and the book files use,
// since destination y is implied by the side to move.
//
// The flag is one-shot by convention: the opener sets it, the caller clears it right
// after the move is chosen (src/ranking.cpp's playOneGame and playoutCapture), so a
// restriction can never leak into a later ply. It is never set with an empty list,
// because that would leave the search no legal root move at all.
extern bool g_useRootFilter;
extern int  g_rootMoveWhitelist[ROOT_FILTER_MAX][3];   // [sx, sy, dx]
extern int  g_rootMoveWhitelistCount;
// True when (sx,sy,dx) survives the current filter. Always true when the filter is off.
bool rootMoveAllowed(int sx, int sy, int dx);

// Gumbel MCTS search-shape constants (src/ai_gumbel.cpp), set by agentChooseMove
// (saved/restored around the call, same convention as the AB toggles above) from
// AgentSpec's gumbelCVisit/gumbelCScaleTenths/gumbelRootM. Inert for every other
// explorer. Defaults reproduce the paper's own defaults (c_visit=50, c_scale=1.0,
// m=16), so a gaz(sims=N)@1 id with no cvisit=/cscale=/m= flags behaves exactly as
// before these were made per-agent.
extern double g_gumbelCVisit;
extern double g_gumbelCScale;
extern int    g_gumbelRootM;

// Per-move search telemetry, written by miniMaxWhite/Black and read by the UIs and the
// tournament. g_lastEffDepth is fractional: completedDepth + (root moves searched in the
// cut iteration / total legal root moves), so 5.7 = depth 5 done, 70% into depth 6.
// BUDGET_SIMS is Gumbel MCTS's own cap (src/ai_gumbel.cpp): a `gaz(sims=N)`
// search ends when its simulation allowance runs out, which is none of the
// three alpha-beta caps. Appended rather than inserted so every stored value
// keeps its meaning.
enum BudgetKind { BUDGET_NONE = 0, BUDGET_DEPTH = 1, BUDGET_NODE = 2, BUDGET_TIME = 3,
                  BUDGET_SIMS = 4 };
extern double g_lastEffDepth;
extern int    g_lastBudgetKind;   // BudgetKind: which cap ended the last search
extern unsigned long long g_lastNodes;
// Wall-clock milliseconds the last top-level search actually consumed, the time
// analogue of g_lastNodes. Zero when the search had no wall budget (elapsedMs
// only runs the clock when one is set), which is also what makes time `retain`
// inert without a time budget.
extern double g_lastSearchMs;
extern unsigned long long g_lastLeafs;

// The salt ttStore/ttProbe mix into every key inside the last top-level search
// (root side, evaluator, eval params, quiescence), so each player gets a disjoint
// region of the one process-wide table. Anything OUTSIDE ai_minimax.cpp that wants
// to read entries that search left must xor this in, or it probes with a key the
// search never wrote. TD-Leaf's PV walk is the one such reader.
uint64_t ttSearchContext();

// Per-iteration node profile of the last search. g_nodesAtDepth[d] is the CUMULATIVE
// node count at the instant iterative deepening finished depth d, so the cost of the
// depth-d iteration alone is g_nodesAtDepth[d] - g_nodesAtDepth[d-1]. A depth that
// never completed reads 0, which is why the array is cleared at the top of every
// search. A non-iterative (fixed-depth, unbudgeted) search fills only its own depth.
// This is the only place the shape of the deepening ladder is observable from
// outside the searcher: g_lastEffDepth collapses it to one number.
// What `part` (g_keepPartial) actually adopted on the last search:
//   0 = nothing adopted (the deepest iteration finished, or the cut one lost a > alphaPrev)
//   1 = adopted a root move that WAS searched to the cut iteration's full depth
//   2 = adopted a root move examined AFTER the budget tripped, whose score is a
//       static evalLeaf taken one ply in with no reply searched, not a depth-d value
// searchRootWhite/Black do not stop at the budget: they keep walking the remaining
// root moves, and budgetTripped short-circuits each one to a leaf eval. Those scores
// are biased high for the side to move (the refutation is never searched), so they
// can win the `a > alphaPrev` adoption test on merit they do not have. Value 2 counts
// exactly that case.
extern int g_lastPartAdopt;

#define MAX_PROFILE_DEPTH 64
extern unsigned long long g_nodesAtDepth[MAX_PROFILE_DEPTH + 1];

// Process-lifetime sum of g_lastNodes over every agentChooseMove search. The
// per-move telemetry above is overwritten by the next move, so a trainer that
// wants to report the search nodes a whole training run consumed has nothing to
// read; this counter is that total. Accumulated in agents.cpp (one add per
// move, no effect on any search), read through src/train_budget.h. Never reset
// mid-process: callers take a baseline and subtract.
extern unsigned long long g_trainNodesTotal;

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
int capacityWhite();
int capacityBlack();
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
