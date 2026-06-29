#include "ai_minimax.h"
#include "moves.h"
#include "board_analysis.h"
#include "ai_random.h"
#include "ai_eval.h"
#include "ml_features.h"      // generateMoves, Move, ML_MAX_MOVES
#include "transposition.h"    // TT (opt-in, gated by g_useTT)
#include "datastore.h"        // positionKey (TT hash)
#include <chrono>
#include <algorithm>          // std::stable_sort (ordered search path)

// RAII guard: seed the incremental positional accumulator from the current board
// on entry to a top-level search and tear it down on every exit path (including
// the "slower death" recursive call), so g_evalIncremental / g_activeParams never
// linger past the search.
namespace {
struct EvalSearchScope {
    EvalSearchScope(int evaluator, const int* params) { evalBeginSearch(evaluator, params); }
    ~EvalSearchScope() { evalEndSearch(); }
};
}

// === BUDGET / DEADLINE STATE ===
// Set true when a recursive node returns early because it hit a per-move budget
// (node or wall-clock). The iterative-deepening driver uses it to discard or keep an
// incomplete (budget-cut) iteration (see g_keepPartial).
static bool s_budgetHit = false;
static int  s_budgetCause = BUDGET_NONE;     // BUDGET_NODE / BUDGET_TIME when s_budgetHit

namespace {
    using Clock = std::chrono::steady_clock;
    Clock::time_point s_timeDeadline;
    bool s_timeOn = false;
}

// True once the active per-move budget (node count or wall clock) is exhausted. The
// time check is gated on a node-count mask so clock::now() is not called per node.
static inline bool budgetTripped(unsigned long long nodes) {
    if (g_nodeDeadline && nodes >= g_nodeDeadline) { s_budgetCause = BUDGET_NODE; return true; }
    if (s_timeOn && (nodes & 4095ULL) == 0 && Clock::now() >= s_timeDeadline) {
        s_budgetCause = BUDGET_TIME; return true;
    }
    return false;
}

static const char* budgetKindName(int k) {
    return k == BUDGET_NODE ? "node" : k == BUDGET_TIME ? "time"
         : k == BUDGET_DEPTH ? "depth" : "none";
}

// Seed s_timeOn / s_timeDeadline from g_timeBudgetMs at the start of a top-level search.
static inline void seedTimeBudget() {
    s_timeOn = (g_timeBudgetMs > 0.0);
    if (s_timeOn)
        s_timeDeadline = Clock::now()
            + std::chrono::duration_cast<Clock::duration>(
                  std::chrono::duration<double, std::milli>(g_timeBudgetMs));
}

// === MOVE ORDERING + TRANSPOSITION (opt-in) ===
// Killer moves (2 quiet refutations per ply) + a side/from/to history table, used by
// the ordered search path. Reset once per top-level search. Only touched when
// g_useMoveOrder / g_useTT are set, so default play is unaffected.
static const int MAXPLY = 128;
static int g_killerFrom[MAXPLY][2], g_killerTo[MAXPLY][2];
static int g_hist[2][64][64];

static void resetSearchHeuristics() {
    for (int p = 0; p < MAXPLY; p++) { g_killerFrom[p][0]=g_killerFrom[p][1]=-1;
                                       g_killerTo[p][0]=g_killerTo[p][1]=-1; }
    for (int s = 0; s < 2; s++) for (int a = 0; a < 64; a++) for (int b = 0; b < 64; b++) g_hist[s][a][b]=0;
}
static inline bool isNearWin(int score) {
    return score > WhiteWin - 1024 || score < BlackWin + 1024;
}
static void recordKiller(int level, int from, int to) {
    if (level < 0 || level >= MAXPLY) return;
    if (g_killerFrom[level][0]==from && g_killerTo[level][0]==to) return;
    g_killerFrom[level][1]=g_killerFrom[level][0]; g_killerTo[level][1]=g_killerTo[level][0];
    g_killerFrom[level][0]=from; g_killerTo[level][0]=to;
}
// Order move indices: TT best move, then captures, then killers, then history score.
static void orderMoves(const Move* mv, int n, int side, int level,
                       int ttFrom, int ttTo, int* order) {
    long long pr[ML_MAX_MOVES];
    int s = (side == White) ? 0 : 1;
    bool ply = (level >= 0 && level < MAXPLY);
    for (int i = 0; i < n; i++) {
        int from = mv[i].sy*8 + mv[i].sx, to = mv[i].dy*8 + mv[i].dx;
        long long p;
        if (from == ttFrom && to == ttTo)      p = (1LL<<40);
        else if (mv[i].capture)                p = (1LL<<32);
        else if (g_useMoveOrder && ply &&
                 ((g_killerFrom[level][0]==from && g_killerTo[level][0]==to) ||
                  (g_killerFrom[level][1]==from && g_killerTo[level][1]==to)))
                                               p = (1LL<<31);
        else                                   p = g_useMoveOrder ? g_hist[s][from][to] : 0;
        pr[i] = p; order[i] = i;
    }
    std::stable_sort(order, order+n, [&](int a, int b){ return pr[a] > pr[b]; });
}

// Ordered/TT-enabled recursive search (mirrors maxAlphaBeta/minAlphaBeta). Entered via
// the dispatch at the top of those functions when g_useTT || g_useMoveOrder.
static int maxAlphaBetaOrdered(int alpha, int beta, int level, int depth, int evaluator,
                               const int* evalParams, unsigned long long& nodes, unsigned long long& leafs) {
    nodes++;
    bool budgetCut = budgetTripped(nodes);
    if (level == depth || budgetCut) {
        if (budgetCut && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinWhite()) return WhiteWin;
        if (canWinBlack()) return BlackWin;
        return evalLeaf(White, evaluator, evalParams);
    }
    if (canWinWhite()) { leafs++; return WhiteWin; }
    if (canWinBlack()) { leafs++; return BlackWin; }

    int depthLeft = depth - level;
    uint64_t key = 0; int ttFrom = -1, ttTo = -1;
    if (g_useTT) {
        key = (uint64_t)positionKey(White, false).hash;
        int sc;
        if (ttProbe(key, depthLeft, alpha, beta, sc, ttFrom, ttTo)) { leafs++; return sc; }
    }

    Move mv[ML_MAX_MOVES];
    int n = generateMoves(White, mv);
    if (n == 0) return alpha;
    int order[ML_MAX_MOVES];
    orderMoves(mv, n, White, level, ttFrom, ttTo, order);

    int origAlpha = alpha, best = INT_MIN, bestFrom = -1, bestTo = -1;
    for (int oi = 0; oi < n; oi++) {
        const Move& m = mv[order[oi]];
        bool isCapture = simulateMoveWhite(m.sx, m.sy, m.dx);
        int eval = minAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                g_useAlphaBeta ? beta  : INT_MAX,
                                level+1, depth, evaluator, evalParams, nodes, leafs);
        unsimulateMoveWhite(m.sx, m.sy, m.dx, isCapture);
        if (eval > best) { best = eval; bestFrom = m.sy*8+m.sx; bestTo = m.dy*8+m.dx; }
        if (eval > alpha) alpha = eval;
        if (g_useAlphaBeta && alpha >= beta) {
            if (g_useMoveOrder && !m.capture) {
                recordKiller(level, bestFrom, bestTo);
                if (level >= 0 && level < MAXPLY) g_hist[0][bestFrom][bestTo] += depthLeft*depthLeft;
            }
            if (g_useTT && !s_budgetHit && !isNearWin(best))
                ttStore(key, depthLeft, best, TT_LOWER, bestFrom, bestTo);
            return beta;
        }
    }
    if (g_useTT && !s_budgetHit && !isNearWin(best))
        ttStore(key, depthLeft, best, (best > origAlpha) ? TT_EXACT : TT_UPPER, bestFrom, bestTo);
    if (alpha > WhiteWin-1024) alpha--;
    return alpha;
}
static int minAlphaBetaOrdered(int alpha, int beta, int level, int depth, int evaluator,
                               const int* evalParams, unsigned long long& nodes, unsigned long long& leafs) {
    nodes++;
    bool budgetCut = budgetTripped(nodes);
    if (level == depth || budgetCut) {
        if (budgetCut && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinBlack()) return BlackWin;
        if (canWinWhite()) return WhiteWin;
        return evalLeaf(Black, evaluator, evalParams);
    }
    if (canWinBlack()) { leafs++; return BlackWin; }
    if (canWinWhite()) { leafs++; return WhiteWin; }

    int depthLeft = depth - level;
    uint64_t key = 0; int ttFrom = -1, ttTo = -1;
    if (g_useTT) {
        key = (uint64_t)positionKey(Black, false).hash;
        int sc;
        if (ttProbe(key, depthLeft, alpha, beta, sc, ttFrom, ttTo)) { leafs++; return sc; }
    }

    Move mv[ML_MAX_MOVES];
    int n = generateMoves(Black, mv);
    if (n == 0) return beta;
    int order[ML_MAX_MOVES];
    orderMoves(mv, n, Black, level, ttFrom, ttTo, order);

    int origBeta = beta, best = INT_MAX, bestFrom = -1, bestTo = -1;
    for (int oi = 0; oi < n; oi++) {
        const Move& m = mv[order[oi]];
        bool isCapture = simulateMoveBlack(m.sx, m.sy, m.dx);
        int eval = maxAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                g_useAlphaBeta ? beta  : INT_MAX,
                                level+1, depth, evaluator, evalParams, nodes, leafs);
        unsimulateMoveBlack(m.sx, m.sy, m.dx, isCapture);
        if (eval < best) { best = eval; bestFrom = m.sy*8+m.sx; bestTo = m.dy*8+m.dx; }
        if (eval < beta) beta = eval;
        if (g_useAlphaBeta && beta <= alpha) {
            if (g_useMoveOrder && !m.capture) {
                recordKiller(level, bestFrom, bestTo);
                if (level >= 0 && level < MAXPLY) g_hist[1][bestFrom][bestTo] += depthLeft*depthLeft;
            }
            if (g_useTT && !s_budgetHit && !isNearWin(best))
                ttStore(key, depthLeft, best, TT_UPPER, bestFrom, bestTo);
            return alpha;
        }
    }
    if (g_useTT && !s_budgetHit && !isNearWin(best))
        ttStore(key, depthLeft, best, (best < origBeta) ? TT_EXACT : TT_LOWER, bestFrom, bestTo);
    if (beta < BlackWin+1024) beta++;
    return beta;
}

// === ROOT SEARCH (WHITE) ===
// One full root search for White to a fixed depth d within the window [alpha0,beta0].
// Fills the best move into mx/my/mz (mx = -1 if no legal move / fail-low) and returns
// its white-centric score. Reports rootDeep (root moves begun before the budget hit)
// and rootTotal (all root moves seen) for the fractional effective-depth readout.
// Does NOT play the move. Shared by the single-shot and iterative-deepening drivers.
static int searchRootWhite(int d, int alpha0, int beta0, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs,
                           int& rootDeep, int& rootTotal) {
    int alpha = alpha0, beta = beta0, eval; bool isCapture; mx = -1;
    for (int y = SIZE-2; y >= 0; y--)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) -> bool {
                rootTotal++; if (!s_budgetHit) rootDeep++;
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) { alpha = eval; mx = x; my = y; mz = z; }
                return g_useAlphaBeta && alpha >= beta;
            };
            if (x > 0       && board[x-1][ny] == BLACK && tryMove(x-1)) return alpha;
            if (x < SIZE-1  && board[x+1][ny] == BLACK && tryMove(x+1)) return alpha;
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return alpha;
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return alpha;
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return alpha;
        }
    return alpha;
}

int miniMaxWhite(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for white
    nodes++;
    g_nodeDeadline = g_nodeBudget ? nodes + g_nodeBudget : 0; //per-move node cap (0=off)
    seedTimeBudget();
    if (g_useTT) ttNewSearch();
    if (g_useTT || g_useMoveOrder) resetSearchHeuristics();
    bool budgeted = g_nodeDeadline || s_timeOn;
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY = 0, moveX2 = 0; //Best move found so far
    int alpha = INT_MIN;
    int victor = 0;
    int completedDepth = 0;
    double cutFraction = 0.0;
    int budgetKind = BUDGET_DEPTH;

    if (!budgeted) {
        // Unbudgeted: a single full-depth search (identical to the original behavior).
        int rd = 0, rt = 0;
        alpha = searchRootWhite(depth, INT_MIN, INT_MAX, evaluator, evalParams,
                                moveX1, moveY, moveX2, nodes, leafs, rd, rt);
        completedDepth = depth;
        budgetKind = BUDGET_DEPTH;
    } else {
        // Budgeted: iterative deepening sharing one node/time pool. Keep the best move
        // from the deepest iteration that finished within budget; a cut iteration is
        // discarded unless g_keepPartial adopts its (provisional) best move.
        for (int d = 1; d <= depth; d++) {
            s_budgetHit = false;
            s_budgetCause = BUDGET_NONE;
            int mx = -1, my = 0, mz = 0, rootDeep = 0, rootTotal = 0;
            int alphaPrev = alpha;
            int a;
            bool aspir = g_useAlphaBeta && g_aspirationWindow > 0 && completedDepth >= 1
                         && alphaPrev > BlackWin + 1024 && alphaPrev < WhiteWin - 1024;
            if (aspir) {
                int lo = alphaPrev - g_aspirationWindow, hi = alphaPrev + g_aspirationWindow;
                a = searchRootWhite(d, lo, hi, evaluator, evalParams, mx, my, mz,
                                    nodes, leafs, rootDeep, rootTotal);
                if (!s_budgetHit && (mx == -1 || a <= lo || a >= hi)) { // window failed: re-search
                    rootDeep = 0; rootTotal = 0;
                    a = searchRootWhite(d, INT_MIN, INT_MAX, evaluator, evalParams, mx, my, mz,
                                        nodes, leafs, rootDeep, rootTotal);
                }
            } else {
                a = searchRootWhite(d, INT_MIN, INT_MAX, evaluator, evalParams, mx, my, mz,
                                    nodes, leafs, rootDeep, rootTotal);
            }

            if (mx == -1 && !s_budgetHit) break;                   // no legal move

            if (s_budgetHit) {
                budgetKind = (s_budgetCause == BUDGET_TIME) ? BUDGET_TIME : BUDGET_NODE;
                cutFraction = rootTotal > 0 ? (double)rootDeep / rootTotal : 0.0;
                bool adopt = (moveX1 == -1) || (g_keepPartial && mx != -1 && a > alphaPrev);
                if (adopt && mx != -1) { moveX1 = mx; moveY = my; moveX2 = mz; alpha = a; }
                break;                                             // budget gone
            }

            moveX1 = mx; moveY = my; moveX2 = mz; alpha = a;       // full iteration completed
            completedDepth = d;
            if (g_nodeDeadline && nodes >= g_nodeDeadline) { budgetKind = BUDGET_NODE; break; }
            if (alpha >= WhiteWin - 1024 || alpha <= BlackWin + 1024) { budgetKind = BUDGET_DEPTH; break; }
        }
    }
    g_lastEffDepth = completedDepth + cutFraction;
    g_lastBudgetKind = budgetKind;
    g_lastNodes = nodes; g_lastLeafs = leafs;
    if (moveX1 != -1) g_downEvalWhite = alpha; //best-line score for display
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
        miniMaxWhite(depth-1, evaluator, evalParams, nodes, leafs);
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
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs)
                 << "\tEff-depth: " << g_lastEffDepth << " (" << budgetKindName(g_lastBudgetKind) << ")";
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

// === ROOT SEARCH (BLACK) ===
// One full root search for Black to a fixed depth d within the window [alpha0,beta0]
// (minimizing white-centric score). Fills the best move into mx/my/mz (mx = -1 if none
// / fail-high) and returns its score. Reports rootDeep/rootTotal like searchRootWhite.
static int searchRootBlack(int d, int alpha0, int beta0, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs,
                           int& rootDeep, int& rootTotal) {
    int alpha = alpha0, beta = beta0, eval; bool isCapture; mx = -1;
    for (int y = 1; y <= SIZE-1; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) -> bool {
                rootTotal++; if (!s_budgetHit) rootDeep++;
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) { beta = eval; mx = x; my = y; mz = z; }
                return g_useAlphaBeta && beta <= alpha;
            };
            if (x > 0       && board[x-1][ny] == WHITE && tryMove(x-1)) return beta;
            if (x < SIZE-1  && board[x+1][ny] == WHITE && tryMove(x+1)) return beta;
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return beta;
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return beta;
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return beta;
        }
    return beta;
}

int miniMaxBlack(int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Get a minimax move for black
    nodes++;
    g_nodeDeadline = g_nodeBudget ? nodes + g_nodeBudget : 0; //per-move node cap (0=off)
    seedTimeBudget();
    if (g_useTT) ttNewSearch();
    if (g_useTT || g_useMoveOrder) resetSearchHeuristics();
    bool budgeted = g_nodeDeadline || s_timeOn;
    EvalSearchScope evalScope(evaluator, evalParams); //seed + auto-teardown of g_evalPos
    int moveX1 = -1, moveY = 0, moveX2 = 0; //Best move found so far
    int beta = INT_MAX;
    int victor = 0;
    int completedDepth = 0;
    double cutFraction = 0.0;
    int budgetKind = BUDGET_DEPTH;

    if (!budgeted) {
        int rd = 0, rt = 0;
        beta = searchRootBlack(depth, INT_MIN, INT_MAX, evaluator, evalParams,
                               moveX1, moveY, moveX2, nodes, leafs, rd, rt);
        completedDepth = depth;
        budgetKind = BUDGET_DEPTH;
    } else {
        for (int d = 1; d <= depth; d++) {
            s_budgetHit = false;
            s_budgetCause = BUDGET_NONE;
            int mx = -1, my = 0, mz = 0, rootDeep = 0, rootTotal = 0;
            int betaPrev = beta;
            int b;
            bool aspir = g_useAlphaBeta && g_aspirationWindow > 0 && completedDepth >= 1
                         && betaPrev > BlackWin + 1024 && betaPrev < WhiteWin - 1024;
            if (aspir) {
                int lo = betaPrev - g_aspirationWindow, hi = betaPrev + g_aspirationWindow;
                b = searchRootBlack(d, lo, hi, evaluator, evalParams, mx, my, mz,
                                    nodes, leafs, rootDeep, rootTotal);
                if (!s_budgetHit && (mx == -1 || b <= lo || b >= hi)) {
                    rootDeep = 0; rootTotal = 0;
                    b = searchRootBlack(d, INT_MIN, INT_MAX, evaluator, evalParams, mx, my, mz,
                                        nodes, leafs, rootDeep, rootTotal);
                }
            } else {
                b = searchRootBlack(d, INT_MIN, INT_MAX, evaluator, evalParams, mx, my, mz,
                                    nodes, leafs, rootDeep, rootTotal);
            }

            if (mx == -1 && !s_budgetHit) break;

            if (s_budgetHit) {
                budgetKind = (s_budgetCause == BUDGET_TIME) ? BUDGET_TIME : BUDGET_NODE;
                cutFraction = rootTotal > 0 ? (double)rootDeep / rootTotal : 0.0;
                bool adopt = (moveX1 == -1) || (g_keepPartial && mx != -1 && b < betaPrev);
                if (adopt && mx != -1) { moveX1 = mx; moveY = my; moveX2 = mz; beta = b; }
                break;
            }

            moveX1 = mx; moveY = my; moveX2 = mz; beta = b;
            completedDepth = d;
            if (g_nodeDeadline && nodes >= g_nodeDeadline) { budgetKind = BUDGET_NODE; break; }
            if (beta <= BlackWin + 1024 || beta >= WhiteWin - 1024) { budgetKind = BUDGET_DEPTH; break; }
        }
    }
    g_lastEffDepth = completedDepth + cutFraction;
    g_lastBudgetKind = budgetKind;
    g_lastNodes = nodes; g_lastLeafs = leafs;
    if (moveX1 != -1) g_downEvalBlack = beta; //best-line score for display
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
        miniMaxBlack(depth-1, evaluator, evalParams, nodes, leafs);
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
            cout << "Nodes visited: " << nodes << "\tAverage branching factor: " << (double) (nodes-1)/(nodes-leafs)
                 << "\tEff-depth: " << g_lastEffDepth << " (" << budgetKindName(g_lastBudgetKind) << ")";
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

// === RECURSIVE ALPHA-BETA ===
int maxAlphaBeta(int alpha, int beta, int level, int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the AI's best next move
    if (g_useTT || g_useMoveOrder)
        return maxAlphaBetaOrdered(alpha, beta, level, depth, evaluator, evalParams, nodes, leafs);
    nodes++;
    bool budgetCut = budgetTripped(nodes);
    if (level == depth || budgetCut) //Leaf: depth reached or budget hit
    {
        if (budgetCut && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinWhite()) return WhiteWin;
        if (canWinBlack()) return BlackWin;
        return evalLeaf(White, evaluator, evalParams);
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
        {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) -> bool {
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    level+1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) alpha = eval;
                return g_useAlphaBeta && alpha >= beta;
            };
            if (x > 0       && board[x-1][ny] == BLACK && tryMove(x-1)) return beta; //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == BLACK && tryMove(x+1)) return beta; //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return beta; //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return beta; //Diagonal-right
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return beta; //Forward
        }
    }
    if (alpha > WhiteWin-1024) //If alpha is a winning move, have it decay at each level to favor longer checkmates
        alpha--;
    return alpha;
}
int minAlphaBeta(int alpha, int beta, int level, int depth, int evaluator, const int* evalParams, unsigned long long int& nodes, unsigned long long int& leafs) { //Given a depth, recursively calculates the opponent's best next move
    if (g_useTT || g_useMoveOrder)
        return minAlphaBetaOrdered(alpha, beta, level, depth, evaluator, evalParams, nodes, leafs);
    nodes++;
    bool budgetCut = budgetTripped(nodes);
    if (level == depth || budgetCut) //Leaf: depth reached or budget hit
    {
        if (budgetCut && level != depth) s_budgetHit = true;
        leafs++;
        if (canWinBlack()) return BlackWin;
        if (canWinWhite()) return WhiteWin;
        return evalLeaf(Black, evaluator, evalParams);
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
        {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) -> bool {
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    level+1, depth, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) beta = eval;
                return g_useAlphaBeta && beta <= alpha;
            };
            if (x > 0       && board[x-1][ny] == WHITE && tryMove(x-1)) return alpha; //Capture-left
            if (x < SIZE-1  && board[x+1][ny] == WHITE && tryMove(x+1)) return alpha; //Capture-right
            if (x > 0       && board[x-1][ny] == EMPTY && tryMove(x-1)) return alpha; //Diagonal-left
            if (x < SIZE-1  && board[x+1][ny] == EMPTY && tryMove(x+1)) return alpha; //Diagonal-right
            if (               board[x  ][ny] == EMPTY && tryMove(x)  ) return alpha; //Forward
        }
    }
    if (beta < BlackWin+1024) //If beta is a winning move, have it decay at each level to favor longer checkmates
        beta++;
    return beta;
}
