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
    Clock::time_point s_timeStart;
    Clock::time_point s_timeDeadline;
    bool s_timeOn = false;
    // Sticky: set the first time a sampled clock read finds the deadline past.
    // See the comment on TIME_CHECK_MASK for why this flag is load-bearing.
    bool s_timeExpired = false;
}

// How often the wall-clock deadline is actually read, as a node-count mask.
//
// Sized from measurement, not guessed. steady_clock::now() costs 22.0 ns on this
// machine (measured 2026-09-01 over 20M calls against a 0.18 ns/iteration empty
// loop, two runs: 22.13 and 21.89 ns). The cheapest core in the roster costs
// about 270 ns/node at the d6/tt/ord head, so checking every 256 nodes adds
// 22/256 = 0.086 ns/node, under 0.04% of node cost, while bounding the sampling
// granularity at 256 * 270 ns = 69 us, under 0.05% of a 150 ms budget. Checking
// every node would cost 8%, and the previous 4096 left the granularity at 1.1 ms
// for no measurable saving over 256.
static const unsigned long long TIME_CHECK_MASK = 255ULL;

// True once the active per-move budget (node count or wall clock) is exhausted.
//
// The node deadline is exact and checked on EVERY node, so once it passes, every
// subsequent node returns immediately and the search unwinds at once.
//
// The wall clock cannot be read that often, so it is sampled. The sampling must
// be paired with the sticky s_timeExpired flag, and without it the time budget
// does not bind at all: a bare `(nodes & mask) == 0 && now() >= deadline` test
// lets the 255 nodes out of every 256 that do not sample the clock proceed to
// full recursion, so a search past its deadline keeps expanding the tree and
// merely prunes one node in 256. That is how a `time=150ms` agent could run an
// entire extra iteration past its budget. With the flag, the first sampled
// expiry converts every later node into an immediate leaf return.
// 1-based index, in root-scan order, of the root move that last raised alpha (or
// lowered beta). Compared against rootDeep it says whether the iteration's best
// move was genuinely searched or only statically scored after the budget tripped.
static int s_rootBestIdx = 0;

static inline bool budgetTripped(unsigned long long nodes) {
    if (g_nodeDeadline && nodes >= g_nodeDeadline) { s_budgetCause = BUDGET_NODE; return true; }
    if (s_timeOn) {
        if (s_timeExpired) { s_budgetCause = BUDGET_TIME; return true; }
        if ((nodes & TIME_CHECK_MASK) == 0 && Clock::now() >= s_timeDeadline) {
            s_timeExpired = true;
            s_budgetCause = BUDGET_TIME;
            return true;
        }
    }
    return false;
}

static const char* budgetKindName(int k) {
    return k == BUDGET_NODE ? "node" : k == BUDGET_TIME ? "time"
         : k == BUDGET_DEPTH ? "depth" : k == BUDGET_SIMS ? "sims" : "none";
}

// Seed the wall-clock state from g_timeBudgetMs at the start of a top-level search.
static inline void seedTimeBudget() {
    s_timeOn = (g_timeBudgetMs > 0.0);
    s_timeExpired = false;
    if (s_timeOn) {
        s_timeStart = Clock::now();
        s_timeDeadline = s_timeStart
            + std::chrono::duration_cast<Clock::duration>(
                  std::chrono::duration<double, std::milli>(g_timeBudgetMs));
    }
}

// Milliseconds elapsed in this search so far (0 when no wall budget is set).
static inline double elapsedMs() {
    if (!s_timeOn) return 0.0;
    return std::chrono::duration<double, std::milli>(Clock::now() - s_timeStart).count();
}

// Whether the next iterative-deepening iteration can be expected to FIT in the
// wall clock that is left, given how this search's own iterations have grown.
//
// Why a plain "is the deadline past" test is not enough: under a non-binding
// depth ceiling, an iteration that finishes comfortably inside the budget is
// followed by one costing several times as much, and the deadline has not
// passed at the moment the decision is made. The cheapest core finishes depth 8
// at about 131 ms of a 150 ms budget, sees 19 ms left, and starts a depth 9 that
// cannot possibly fit. The sticky flag above bounds the damage to the budget,
// but the whole iteration is then discarded (a cut iteration is dropped unless
// g_keepPartial), so the time buys nothing.
//
// The growth factor is measured from THIS search's own last two iterations, not
// assumed. It has to be: the per-ply factor is a property of an evaluator's move
// ordering, and the measured 3.41 for the chip counter on the tt,ord path is
// that core's alone (the bare unbudgeted ladder gives 5.6 for the same search).
// Until two iterations have been timed there is nothing to extrapolate from, so
// the check abstains and the iteration runs -- which also guarantees a move.
//
// Node budgets deliberately do NOT get this check. Their in-recursion test is
// exact and per-node, so they overshoot by nothing, and adding a predictive
// stop would change every node-track agent's play.
static inline bool nextIterationFits(double prevIterMs, double lastIterMs) {
    if (!s_timeOn) return true;                       // no wall clock to fit inside
    if (!(prevIterMs > 0.05) || !(lastIterMs > 0.0)) return true;   // too little signal yet
    double growth = lastIterMs / prevIterMs;
    if (growth < 1.0) growth = 1.0;                   // never predict a cheaper next ply
    double predicted = lastIterMs * growth;
    double remaining = std::chrono::duration<double, std::milli>(s_timeDeadline - Clock::now()).count();
    return remaining > predicted;
}

// === MOVE ORDERING + TRANSPOSITION (opt-in) ===
// Killer moves (2 quiet refutations per ply) + a side/from/to history table, used by
// the ordered search path. Reset once per top-level search. Only touched when
// g_useMoveOrder / g_useTT are set, so default play is unaffected.
// Node-budget counterpart to nextIterationFits, and deliberately a static threshold
// rather than a prediction: g_iterMinRemain is the percentage of the node budget that
// must still be unspent before another deepening iteration is worth beginning. An
// iteration that cannot finish contributes nothing, because a budget-cut iteration is
// discarded (or, under g_keepPartial, adopted from a root scan whose tail was scored
// by a static evalLeaf rather than searched). Inert at 0 and inert without a node
// budget, so every existing agent behaves exactly as before.
static inline bool nodeIterationWorthStarting(unsigned long long nodes) {
    if (g_iterMinRemain <= 0 || !g_nodeDeadline) return true;
    if (nodes >= g_nodeDeadline) return false;
    double remainPct = 100.0 * (double)(g_nodeDeadline - nodes) / (double)g_nodeDeadline;
    return remainPct >= (double)g_iterMinRemain;
}

static const int MAXPLY = 128;
static int g_killerFrom[MAXPLY][2], g_killerTo[MAXPLY][2];
static int g_hist[2][64][64];

// === TRANSPOSITION SEARCHER CONTEXT ===
// The transposition table is ONE table for the whole process, keyed by the position
// hash. A position hash says WHICH POSITION was searched. It does not say WHO
// searched it, and a stored score is only meaningful to a searcher that would have
// computed the same number. Without this context both halves of that go wrong:
//
//   1. Cross-evaluator reads. White's agent stores a score its evaluator produced,
//      Black's agent probes the same position, gets a hit, and returns White's
//      evaluator's number as its own. The two agents are different players with
//      different value functions, so this is simply a wrong score, not a cache hit.
//   2. Cross-strength reads. ttProbe accepts any entry whose stored depth is at
//      least the prober's remaining depth, so a shallow agent reads a deeper
//      agent's entries and plays above its own depth.
//
// Mixing a searcher context into the key gives every distinct searcher its own
// disjoint region of the one table, which is the same semantics as handing each
// player a private table. It is composed of:
//
//   - the evaluator index and its full parameter array. That covers the learned
//     evaluator's model slot too, since agentChooseMove wires the slot into
//     evalParams[0].
//   - g_useQuiescence, because a quiescence score at a given remaining depth
//     extends captures past the horizon and a plain one does not, so the two are
//     different numbers for the same position and depth.
//   - the ROOT side to move. In a game the White player always searches from a
//     White-to-move root and the Black player from a Black-to-move root, so this
//     one value separates the two players even when they are otherwise identical.
//     It costs nothing inside a single search, where every ply of both parities
//     still shares one context and the whole point of the table is preserved.
//
// Without the root-side term, two agents sharing an evaluator still contaminate
// each other: measured 2026-08-27 while mining refutation lines, 132 of 238 mined
// lines stopped reproducing when the mined side stopped searching, and every one of
// those 132 had a `tt` opponent while 0 of 77 non-`tt` opponents were affected.
static uint64_t s_ttCtx = 0;

static void setTTContext(int rootSide, int evaluator, const int* evalParams) {
    uint64_t h = 1469598103934665603ULL;
    #define TT_MIX(v) do { uint64_t _v = (uint64_t)(int64_t)(v); \
        for (int _b = 0; _b < 8; _b++) { h ^= (unsigned char)(_v >> (_b * 8)); h *= 1099511628211ULL; } \
    } while (0)
    TT_MIX(rootSide);
    TT_MIX(evaluator);
    TT_MIX(g_useQuiescence ? 1 : 0);
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) TT_MIX(evalParams ? evalParams[i] : 0);
    #undef TT_MIX
    s_ttCtx = h;
}

uint64_t ttSearchContext() { return s_ttCtx; }

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

// === QUIESCENCE (opt-in, g_useQuiescence) ===
// Captures-only stand-pat extension entered at true depth leaves (level == depth,
// not budget-cut pseudo-leaves), so a fixed-depth search stops mid-exchange less
// often. Each recursion consumes a capture, and captures strictly shrink material
// (A7), so the extension terminates on its own; QS_MAX_PLY is a belt-and-braces
// cap. Quiescence nodes are counted against the node budget like any other node
// (one nodes++ per capture-child visited; the entry position was already counted
// by the calling leaf). Stand-pat (the side to move may decline all captures and
// keep the static eval) is the standard heuristic; like all quiescence it is
// blind to zugzwang, which Breakthrough's forced-advance rule makes possible --
// this is an Elo experiment, not a soundness proof. canWin checks at each qnode
// keep the same near-win sentinel semantics as the plain leaf, and the win-decay
// on return preserves the fastest-win preference.
static const int QS_MAX_PLY = 32;

static int quiesceMin(int alpha, int beta, int qdepth, int evaluator, const int* evalParams,
                      unsigned long long& nodes, unsigned long long& leafs);

static int quiesceMax(int alpha, int beta, int qdepth, int evaluator, const int* evalParams,
                      unsigned long long& nodes, unsigned long long& leafs) {
    if (canWinWhite()) { leafs++; return WhiteWin; }
    if (canWinBlack()) { leafs++; return BlackWin; }
    int standPat = evalLeaf(White, evaluator, evalParams);
    if (qdepth >= QS_MAX_PLY) { leafs++; return standPat; }
    if (standPat > alpha) alpha = standPat;
    if (g_useAlphaBeta && alpha >= beta) { leafs++; return beta; }

    bool tried = false, stop = false;
    for (int y = SIZE-2; y >= 0 && !stop; y--) {
        for (int x = 0; x < SIZE && !stop; x++) {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryCap = [&](int z) -> bool {   // true = stop exploring (cutoff/budget)
                tried = true;
                nodes++;
                if (budgetTripped(nodes)) { s_budgetHit = true; return true; }
                bool isCapture = simulateMoveWhite(x, y, z);
                int eval = quiesceMin(g_useAlphaBeta ? alpha : INT_MIN,
                                      g_useAlphaBeta ? beta  : INT_MAX,
                                      qdepth+1, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) alpha = eval;
                return g_useAlphaBeta && alpha >= beta;
            };
            if (x > 0      && board[x-1][ny] == BLACK && tryCap(x-1)) { stop = true; break; }
            if (x < SIZE-1 && board[x+1][ny] == BLACK && tryCap(x+1)) { stop = true; break; }
        }
    }
    if (!tried) { leafs++; return alpha; }        // quiet position: stand-pat
    if (g_useAlphaBeta && alpha >= beta) return beta;
    if (alpha > WhiteWin-1024) alpha--;
    return alpha;
}

static int quiesceMin(int alpha, int beta, int qdepth, int evaluator, const int* evalParams,
                      unsigned long long& nodes, unsigned long long& leafs) {
    if (canWinBlack()) { leafs++; return BlackWin; }
    if (canWinWhite()) { leafs++; return WhiteWin; }
    int standPat = evalLeaf(Black, evaluator, evalParams);
    if (qdepth >= QS_MAX_PLY) { leafs++; return standPat; }
    if (standPat < beta) beta = standPat;
    if (g_useAlphaBeta && beta <= alpha) { leafs++; return alpha; }

    bool tried = false, stop = false;
    for (int y = 1; y <= SIZE-1 && !stop; y++) {
        for (int x = 0; x < SIZE && !stop; x++) {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryCap = [&](int z) -> bool {   // true = stop exploring (cutoff/budget)
                tried = true;
                nodes++;
                if (budgetTripped(nodes)) { s_budgetHit = true; return true; }
                bool isCapture = simulateMoveBlack(x, y, z);
                int eval = quiesceMax(g_useAlphaBeta ? alpha : INT_MIN,
                                      g_useAlphaBeta ? beta  : INT_MAX,
                                      qdepth+1, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) beta = eval;
                return g_useAlphaBeta && beta <= alpha;
            };
            if (x > 0      && board[x-1][ny] == WHITE && tryCap(x-1)) { stop = true; break; }
            if (x < SIZE-1 && board[x+1][ny] == WHITE && tryCap(x+1)) { stop = true; break; }
        }
    }
    if (!tried) { leafs++; return beta; }         // quiet position: stand-pat
    if (g_useAlphaBeta && beta <= alpha) return alpha;
    if (beta < BlackWin+1024) beta++;
    return beta;
}

// Ordered/TT-enabled recursive search (mirrors maxAlphaBeta/minAlphaBeta). Entered via
// the dispatch at the top of those functions when g_useTT || g_useMoveOrder.
static int maxAlphaBetaOrdered(int alpha, int beta, int level, int depth, int evaluator,
                               const int* evalParams, unsigned long long& nodes, unsigned long long& leafs) {
    nodes++;
    bool budgetCut = budgetTripped(nodes);
    if (level == depth || budgetCut) {
        if (budgetCut && level != depth) s_budgetHit = true;
        if (g_useQuiescence && level == depth && !budgetCut)
            return quiesceMax(alpha, beta, 0, evaluator, evalParams, nodes, leafs);
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
        key = (uint64_t)positionKey(White, false).hash ^ s_ttCtx;
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
        if (g_useQuiescence && level == depth && !budgetCut)
            return quiesceMin(alpha, beta, 0, evaluator, evalParams, nodes, leafs);
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
        key = (uint64_t)positionKey(Black, false).hash ^ s_ttCtx;
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
//
// THE ROOT SCAN IS DELIBERATELY UNORDERED, and two separate things depend on it.
// orderMoves runs at interior nodes only, so root candidates are always visited
// in the same board-geometry order and a tie is always broken by whichever
// equal-scoring move that order reaches first.
//  (1) It is what bounds the effect of the transposition table on a
//      deterministic agent's reply, which is what lets rankSchedule's
//      pairGameTarget cap a deterministic pair at 2 games instead of replaying
//      it. It does NOT make the reply independent of the table on its own:
//      interior-node ordering is table-driven, and ttProbe does not check
//      e.gen, so a search reads entries its own earlier searches left and
//      whether those survived depends on what else stored into their slots.
//      The reply's independence from that is MEASURED, not argued: 1,836
//      replay-games with one side going silent, across four node budgets from
//      200k to 8m, 0 divergences (2026-09-09, theory 71). Regression test:
//      tests/test_determinism.cpp. Ordering the root is an obvious search
//      improvement and would remove the bound: rerun that test if you do it,
//      and expect the 2-game cap to need revisiting.
//  (2) It is also why a budget-cut iteration covers an arbitrary geometric
//      slice of the root moves rather than a promising one. See g_keepPartial
//      and theory 64.
static int searchRootWhite(int d, int alpha0, int beta0, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs,
                           int& rootDeep, int& rootTotal) {
    int alpha = alpha0, beta = beta0, eval; bool isCapture; mx = -1;
    s_rootBestIdx = 0;
    for (int y = SIZE-2; y >= 0; y--)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            auto tryMove = [&](int z) -> bool {
                // Root-move filter (cbook opener). Skipped moves are not counted in
                // rootTotal either, so the fractional effective-depth readout stays a
                // fraction of the moves this search actually had to get through.
                if (!rootMoveAllowed(x, y, z)) return false;
                rootTotal++; if (!s_budgetHit) rootDeep++;
                isCapture = simulateMoveWhite(x, y, z);
                eval = minAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveWhite(x, y, z, isCapture);
                if (eval > alpha) { alpha = eval; mx = x; my = y; mz = z; s_rootBestIdx = rootTotal; }
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
    setTTContext(White, evaluator, evalParams);
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
    for (int i = 0; i <= MAX_PROFILE_DEPTH; i++) g_nodesAtDepth[i] = 0;
    g_lastPartAdopt = 0;

    if (!budgeted) {
        // Unbudgeted: a single full-depth search (identical to the original behavior).
        int rd = 0, rt = 0;
        alpha = searchRootWhite(depth, INT_MIN, INT_MAX, evaluator, evalParams,
                                moveX1, moveY, moveX2, nodes, leafs, rd, rt);
        completedDepth = depth;
        budgetKind = BUDGET_DEPTH;
        if (depth >= 0 && depth <= MAX_PROFILE_DEPTH) g_nodesAtDepth[depth] = nodes;
    } else {
        // Budgeted: iterative deepening sharing one node/time pool. Keep the best move
        // from the deepest iteration that finished within budget; a cut iteration is
        // discarded unless g_keepPartial adopts its (provisional) best move.
        double prevIterMs = 0.0, lastIterMs = 0.0;
        for (int d = 1; d <= depth; d++) {
            // Wall clock only: decline an iteration this search's own measured
            // per-ply growth says cannot fit (see nextIterationFits).
            if (d > 1 && !nextIterationFits(prevIterMs, lastIterMs)) {
                budgetKind = BUDGET_TIME;
                break;
            }
            if (d > 1 && !nodeIterationWorthStarting(nodes)) {
                budgetKind = BUDGET_NODE;
                break;
            }
            double iterStartMs = elapsedMs();
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
                if (adopt && mx != -1) {
                    moveX1 = mx; moveY = my; moveX2 = mz; alpha = a;
                    g_lastPartAdopt = (s_rootBestIdx > rootDeep) ? 2 : 1;
                }
                break;                                             // budget gone
            }

            moveX1 = mx; moveY = my; moveX2 = mz; alpha = a;       // full iteration completed
            completedDepth = d;
            if (d <= MAX_PROFILE_DEPTH) g_nodesAtDepth[d] = nodes;
            prevIterMs = lastIterMs;
            lastIterMs = elapsedMs() - iterStartMs;
            if (g_nodeDeadline && nodes >= g_nodeDeadline) { budgetKind = BUDGET_NODE; break; }
            if (alpha >= WhiteWin - 1024 || alpha <= BlackWin + 1024) { budgetKind = BUDGET_DEPTH; break; }
        }
    }
    g_lastEffDepth = completedDepth + cutFraction;
    g_lastBudgetKind = budgetKind;
    g_lastNodes = nodes; g_lastLeafs = leafs; g_lastSearchMs = elapsedMs();
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
// Unordered for the same two reasons, and with the same warning: see the block
// above searchRootWhite before you order either of them.
static int searchRootBlack(int d, int alpha0, int beta0, int evaluator, const int* evalParams,
                           int& mx, int& my, int& mz,
                           unsigned long long int& nodes, unsigned long long int& leafs,
                           int& rootDeep, int& rootTotal) {
    int alpha = alpha0, beta = beta0, eval; bool isCapture; mx = -1;
    s_rootBestIdx = 0;
    for (int y = 1; y <= SIZE-1; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            auto tryMove = [&](int z) -> bool {
                // Root-move filter (cbook opener), see searchRootWhite.
                if (!rootMoveAllowed(x, y, z)) return false;
                rootTotal++; if (!s_budgetHit) rootDeep++;
                isCapture = simulateMoveBlack(x, y, z);
                eval = maxAlphaBeta(g_useAlphaBeta ? alpha : INT_MIN,
                                    g_useAlphaBeta ? beta  : INT_MAX,
                                    1, d, evaluator, evalParams, nodes, leafs);
                unsimulateMoveBlack(x, y, z, isCapture);
                if (eval < beta) { beta = eval; mx = x; my = y; mz = z; s_rootBestIdx = rootTotal; }
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
    setTTContext(Black, evaluator, evalParams);
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
    for (int i = 0; i <= MAX_PROFILE_DEPTH; i++) g_nodesAtDepth[i] = 0;
    g_lastPartAdopt = 0;

    if (!budgeted) {
        int rd = 0, rt = 0;
        beta = searchRootBlack(depth, INT_MIN, INT_MAX, evaluator, evalParams,
                               moveX1, moveY, moveX2, nodes, leafs, rd, rt);
        completedDepth = depth;
        budgetKind = BUDGET_DEPTH;
        if (depth >= 0 && depth <= MAX_PROFILE_DEPTH) g_nodesAtDepth[depth] = nodes;
    } else {
        double prevIterMs = 0.0, lastIterMs = 0.0;
        for (int d = 1; d <= depth; d++) {
            // Mirrors searchRootWhite's driver: see nextIterationFits.
            if (d > 1 && !nextIterationFits(prevIterMs, lastIterMs)) {
                budgetKind = BUDGET_TIME;
                break;
            }
            if (d > 1 && !nodeIterationWorthStarting(nodes)) {
                budgetKind = BUDGET_NODE;
                break;
            }
            double iterStartMs = elapsedMs();
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
                if (adopt && mx != -1) {
                    moveX1 = mx; moveY = my; moveX2 = mz; beta = b;
                    g_lastPartAdopt = (s_rootBestIdx > rootDeep) ? 2 : 1;
                }
                break;
            }

            moveX1 = mx; moveY = my; moveX2 = mz; beta = b;
            completedDepth = d;
            if (d <= MAX_PROFILE_DEPTH) g_nodesAtDepth[d] = nodes;
            prevIterMs = lastIterMs;
            lastIterMs = elapsedMs() - iterStartMs;
            if (g_nodeDeadline && nodes >= g_nodeDeadline) { budgetKind = BUDGET_NODE; break; }
            if (beta <= BlackWin + 1024 || beta >= WhiteWin - 1024) { budgetKind = BUDGET_DEPTH; break; }
        }
    }
    g_lastEffDepth = completedDepth + cutFraction;
    g_lastBudgetKind = budgetKind;
    g_lastNodes = nodes; g_lastLeafs = leafs; g_lastSearchMs = elapsedMs();
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
        if (g_useQuiescence && level == depth && !budgetCut)
            return quiesceMax(alpha, beta, 0, evaluator, evalParams, nodes, leafs);
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
        if (g_useQuiescence && level == depth && !budgetCut)
            return quiesceMin(alpha, beta, 0, evaluator, evalParams, nodes, leafs);
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
