// gui_engine.cpp - see gui_engine.h for the threading contract.
//
// Sections (grep "// ==="):
//   RULES              pure GuiPos helpers (UI-thread safe)
//   ENGINE STATE       engine-thread-only helpers: load a position, diff a move, model slots
//   MOVE JOB           one agent move, mirroring src/ranking.cpp's playOneGame dispatch
//   ANALYSIS JOB       iterative-deepening multi-line analysis, one root move per step
//   SERVICE            job queue, abort protocol, native thread / web inline driver

#include "gui_engine.h"
#include "ai_eval.h"        // g_evaluators, g_evalCount
#include "ai_random.h"      // g_openers
#include "explorers.h"      // g_explorers
#include "choosers.h"       // g_choosers
#include "ml_eval.h"        // mlLoadSlot (safe here: this file includes no raylib header)
#include "ranking.h"        // rankSlotFile: the one slot-file naming convention
#include "transposition.h"  // ttClear
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>

#if !defined(__EMSCRIPTEN__)
#define GUI_ENGINE_THREADED 1
#include <thread>
#include <mutex>
#include <condition_variable>
#else
#define GUI_ENGINE_THREADED 0
#endif

typedef std::chrono::steady_clock EngClock;
static double msSince(EngClock::time_point t0) {
    return std::chrono::duration<double, std::milli>(EngClock::now() - t0).count();
}

// ============================================================
// RULES -- pure GuiPos helpers (UI-thread safe)
// ============================================================
bool guiIsLegal(const GuiPos& p, int sx, int sy, int dx) {
    char me  = (p.side == White) ? WHITE : BLACK;
    int  dir = (p.side == White) ? 1 : -1;
    int  dy  = sy + dir;
    if (sx < 0 || sx >= SIZE || sy < 0 || sy >= SIZE) return false;
    if (dx < 0 || dx >= SIZE || dy < 0 || dy >= SIZE) return false;
    if (dx < sx - 1 || dx > sx + 1) return false;
    if (p.b[sx][sy] != me) return false;
    char dest = p.b[dx][dy];
    if (dest == me) return false;                      // blocked by own piece
    if (dx == sx && dest != EMPTY) return false;       // straight moves never capture
    return true;
}

// Capture-first per piece, the same priority the engine's generator uses.
int guiLegalMoves(const GuiPos& p, GuiMove* out) {
    int n = 0;
    int dir = (p.side == White) ? 1 : -1;
    char opp = (p.side == White) ? BLACK : WHITE;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            int ny = y + dir;
            if (ny < 0 || ny >= SIZE) continue;
            int tries[3] = { -1, 1, 0 };
            // captures first, then empty diagonals, then straight
            for (int pass = 0; pass < 3; pass++)
                for (int t = 0; t < 3; t++) {
                    int dx = x + tries[t];
                    if (!guiIsLegal(p, x, y, dx)) continue;
                    bool cap = (p.b[dx][ny] == opp);
                    bool straight = (dx == x);
                    int want = cap ? 0 : (straight ? 2 : 1);
                    if (want != pass) continue;
                    if (n < GUI_MAX_MOVES) { out[n].sx = x; out[n].sy = y; out[n].dx = dx; out[n].dy = ny; n++; }
                }
        }
    return n;
}

void guiCountPieces(const GuiPos& p, int& white, int& black) {
    white = black = 0;
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            if (p.b[x][y] == WHITE) white++;
            else if (p.b[x][y] == BLACK) black++;
        }
}

// A side wins by reaching the far row or by capturing every opposing piece. A
// side left to move with no legal move loses (the engine's greedy explorer uses
// the same convention); the GUI declares it here so no agent is ever asked to
// move from a dead position.
int guiWinner(const GuiPos& p) {
    int w, b;
    guiCountPieces(p, w, b);
    for (int x = 0; x < SIZE; x++) {
        if (p.b[x][SIZE - 1] == WHITE) return White;
        if (p.b[x][0] == BLACK)        return Black;
    }
    if (b == 0) return White;
    if (w == 0) return Black;
    GuiMove mv[GUI_MAX_MOVES];
    if (guiLegalMoves(p, mv) == 0) return (p.side == White) ? Black : White;
    return None;
}

int guiApplyMove(GuiPos& p, const GuiMove& m) {
    char me = (p.side == White) ? WHITE : BLACK;
    int dy = m.sy + ((p.side == White) ? 1 : -1);
    p.b[m.sx][m.sy] = EMPTY;
    p.b[m.dx][dy] = me;
    p.side = (p.side == White) ? Black : White;
    p.halfMove++;
    return guiWinner(p);
}

std::string guiMoveText(const GuiMove& m) {
    if (!m.valid()) return "--";
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%d%c", (char)('a' + m.sx), m.sy, (char)('a' + m.dx));
    return buf;
}

// Same file layout reloadBoard reads: 64 whitespace-separated cells, top row
// (y = SIZE-1) first, each row left to right.
bool guiLoadBoardFile(const std::string& path, GuiPos& out, std::string& err) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) { err = "cannot open " + path; return false; }
    for (int y = SIZE - 1; y >= 0; y--)
        for (int x = 0; x < SIZE; x++) {
            char c = 0;
            if (!(f >> c)) { err = path + " is shorter than 64 cells"; return false; }
            if (c != EMPTY && c != WHITE && c != BLACK) { err = path + " has an invalid cell '" + std::string(1, c) + "'"; return false; }
            out.b[x][y] = c;
        }
    out.side = White;
    out.halfMove = 0;
    return true;
}

// ============================================================
// ENGINE STATE -- engine-thread-only helpers
// ============================================================
// Load a position into the engine globals, recomputing every incremental
// counter the way reloadBoard does. The per-search accumulators (g_evalPos,
// row counts, ML accumulators) are seeded by each search itself.
static void engSetBoard(const GuiPos& p) {
    std::memcpy(board, p.b, sizeof(board));
    g_whiteCount = g_blackCount = g_chipDiff = g_whiteAtEnd = g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) {
                g_whiteCount++; g_chipDiff++;
                if (y == SIZE - 1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++; g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
    g_useRootFilter = false;
}

// The move `side` made between two boards: the one square it vacated and the one
// it newly occupies.
static GuiMove diffBoards(const char before[SIZE][SIZE], const char after[SIZE][SIZE], int side) {
    char me = (side == White) ? WHITE : BLACK;
    GuiMove m;
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            if (before[x][y] == me && after[x][y] != me) { m.sx = x; m.sy = y; }
            if (before[x][y] != me && after[x][y] == me) { m.dx = x; m.dy = y; }
        }
    if (m.sx < 0 || m.dx < 0) return GuiMove();
    return m;
}

static int chooserIndex(const char* n) {
    for (int i = 0; i < g_chooserCount; i++) if (std::strcmp(g_choosers[i].name, n) == 0) return i;
    return -1;
}
static bool isAlphaBeta(int explorer) {
    return explorer >= 0 && explorer < g_explorerCount && std::strcmp(g_explorers[explorer].name, "AlphaBeta") == 0;
}

// Slot a spec reads its model from, or -1 when it uses none.
static int specModelSlot(const AgentSpec& s) {
    if (s.brain == BRAIN_POLICY && s.chooser == chooserIndex("LearnedPolicy")) return s.modelSlot;
    if (s.brain == BRAIN_SEARCH && s.evaluator == learnedValueIndex())       return s.modelSlot;
    return -1;
}

// Slots already loaded by this thread. A slot is loaded from its file on first
// use and then reused; engInvalidateModel drops it so the next use reloads.
static std::set<int> s_loadedSlots;

static bool ensureSlot(int slot, std::string& err) {
    if (slot < 0) return true;
    if (s_loadedSlots.count(slot)) return true;
    std::string f = rankSlotFile(slot);
    if (f.empty() || !mlLoadSlot(slot, f)) {
        err = "cannot load model " + (f.empty() ? std::string("(no file)") : f) + " into slot " + std::to_string(slot);
        return false;
    }
    s_loadedSlots.insert(slot);
    return true;
}

// ============================================================
// MOVE JOB -- one agent move
// ============================================================
struct MoveJob {
    int       request = 0;
    GuiPos    pos;
    AgentSpec spec;
};

// Mirrors src/ranking.cpp's playOneGame: the agent's identity-level opener gets
// the first say (with its own ply count halfMove/2 and the game clock halfMove),
// its brain plays otherwise, and a root filter an opener set is cleared after.
static EngMoveResult runMoveJob(const MoveJob& j) {
    EngMoveResult r;
    r.request = j.request;
    const AgentSpec& s = j.spec;
    if (!ensureSlot(specModelSlot(s), r.error)) return r;

    engSetBoard(j.pos);
    int side = j.pos.side;
    if (s.brain == BRAIN_SEARCH) {
        int params[MAX_EVAL_PARAMS];
        for (int i = 0; i < MAX_EVAL_PARAMS; i++) params[i] = s.evalParams[i];
        if (s.evaluator == learnedValueIndex()) params[0] = s.modelSlot;
        r.imm = evaluateBoard(side, s.evaluator, params);
        r.hasImm = true;
    }

    char before[SIZE][SIZE];
    std::memcpy(before, board, sizeof(before));
    EngClock::time_point t0 = EngClock::now();
    g_lastNodes = 0;
    g_lastEffDepth = 0.0;
    int victor = None;
    bool byOpener = false;
    if (s.openerKind >= 0 && s.openerKind < g_openerCount)
        byOpener = g_openers[s.openerKind].fn(side, j.pos.halfMove / 2, j.pos.halfMove,
                                              s.openerArg, s.openerArg2, victor);
    if (!byOpener) victor = agentChooseMove(s, side);
    g_useRootFilter = false;
    (void)victor;

    r.ms = msSince(t0);
    r.move = diffBoards(before, board, side);
    r.byOpener = byOpener;
    if (!r.move.valid()) r.error = "the agent produced no move";
    if (!byOpener && s.brain == BRAIN_SEARCH && isAlphaBeta(s.explorer)) {
        r.hasDown = true;
        r.down = (side == White) ? g_downEvalWhite : g_downEvalBlack;
    }
    if (!byOpener && s.brain == BRAIN_SEARCH) {
        r.nodes = g_lastNodes;
        r.effDepth = g_lastEffDepth;
    }
    return r;
}

// ============================================================
// ANALYSIS JOB -- iterative-deepening multi-line analysis
// ============================================================
// Every legal root move gets an exact score at every depth: the move is applied
// and the resulting position searched from the opponent's side to depth d-1
// with the ordinary top-level search (miniMaxWhite/Black), whose best-line value
// is that move's depth-d score and whose played reply is the expected answer.
// This costs about one full search per root move rather than one shared
// alpha-beta search, and in exchange every arrow carries a real score instead of
// only the best one being known. With the TT on, each depth reuses the entries
// the previous one left.
//
// The TT is one process-wide table keyed by a searcher context that mixes in all
// MAX_EVAL_PARAMS evaluator parameters (src/ai_minimax.cpp, setTTContext). No
// evaluator declares that many, so analysis writes a salt into the last slot:
// its entries live in a region no agent ever probes, and an agent playing in the
// same game never reads a score the analysis computed. The analysis still
// shares the table's capacity, so its stores can evict an agent's older entries.
static const int ANALYSIS_TT_SALT = 0x6A11;

struct AnalysisRun {
    bool   active = false;
    int    request = -1;
    GuiPos root;
    EngAnalysisConfig cfg;
    int    params[MAX_EVAL_PARAMS];
    std::vector<GuiMove> moves;
    std::vector<int>     order;         // root-move indices in search order for depth d
    std::vector<int>     score;         // depth-d scores being filled in
    std::vector<GuiMove> reply;
    int    d = 1, pos = 0;
    bool   staticDone = false;
    unsigned long long nodes = 0;
    EngClock::time_point t0, depthT0;
    double lastDepthMs = 0.0;
};

struct StepOut {
    int     idx = -1;
    int     score = 0;
    GuiMove reply;
    unsigned long long nodes = 0;
    bool    hasStatic = false;
    int     staticEval = 0;
    std::string error;
};

static bool betterFor(int side, int a, int b) { return side == White ? a > b : a < b; }

static void initRun(AnalysisRun& run, int request, const GuiPos& p, const EngAnalysisConfig& cfg) {
    run = AnalysisRun();
    run.active = true;
    run.request = request;
    run.root = p;
    run.cfg = cfg;
    if (run.cfg.maxDepth < 1) run.cfg.maxDepth = 1;
    if (run.cfg.maxDepth > 60) run.cfg.maxDepth = 60;
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) run.params[i] = cfg.params[i];
    if (cfg.evaluator == learnedValueIndex()) run.params[0] = cfg.modelSlot;
    run.params[MAX_EVAL_PARAMS - 1] = ANALYSIS_TT_SALT;
    GuiMove mv[GUI_MAX_MOVES];
    int n = guiLegalMoves(p, mv);
    run.moves.assign(mv, mv + n);
    run.order.resize(n);
    for (int i = 0; i < n; i++) run.order[i] = i;
    run.score.assign(n, 0);
    run.reply.assign(n, GuiMove());
    run.t0 = run.depthT0 = EngClock::now();
}

// Score one root move at the run's current depth. Touches engine globals only.
static StepOut computeStep(const AnalysisRun& run) {
    StepOut o;
    const EngAnalysisConfig& c = run.cfg;
    if (c.evaluator == learnedValueIndex() && !ensureSlot(c.modelSlot, o.error)) return o;
    if (!run.staticDone) {
        engSetBoard(run.root);
        o.staticEval = evaluateBoard(run.root.side, c.evaluator, run.params);
        o.hasStatic = true;
    }
    if (run.moves.empty()) return o;

    int i = run.order[run.pos];
    o.idx = i;
    GuiPos child = run.root;
    int w = guiApplyMove(child, run.moves[i]);
    if (w == White) { o.score = WhiteWin; return o; }    // decided by this move: no search
    if (w == Black) { o.score = BlackWin; return o; }

    engSetBoard(child);
    int opp = child.side;
    if (run.d == 1) {
        o.score = evaluateBoard(opp, c.evaluator, run.params);
        return o;
    }

    // Search settings for this step, restored afterwards (the agentChooseMove
    // convention, so a later move job starts from the defaults it expects).
    unsigned long long savedNode = g_nodeBudget; double savedTime = g_timeBudgetMs;
    bool savedAB = g_useAlphaBeta, savedTT = g_useTT, savedMO = g_useMoveOrder;
    bool savedQS = g_useQuiescence, savedKP = g_keepPartial;
    int  savedAsp = g_aspirationWindow, savedIMR = g_iterMinRemain;
    g_nodeBudget = 0; g_timeBudgetMs = 0.0;
    g_useAlphaBeta = true; g_useTT = c.useTT; g_useMoveOrder = c.useMoveOrder;
    g_useQuiescence = c.useQuiescence; g_keepPartial = false;
    g_aspirationWindow = 0; g_iterMinRemain = 0;

    char before[SIZE][SIZE];
    std::memcpy(before, board, sizeof(before));
    unsigned long long nodes = 0, leafs = 0;
    if (opp == White) miniMaxWhite(run.d - 1, c.evaluator, run.params, nodes, leafs);
    else              miniMaxBlack(run.d - 1, c.evaluator, run.params, nodes, leafs);
    o.score = (opp == White) ? g_downEvalWhite : g_downEvalBlack;
    o.reply = diffBoards(before, board, opp);
    o.nodes = nodes;

    g_nodeBudget = savedNode; g_timeBudgetMs = savedTime;
    g_useAlphaBeta = savedAB; g_useTT = savedTT; g_useMoveOrder = savedMO;
    g_useQuiescence = savedQS; g_keepPartial = savedKP;
    g_aspirationWindow = savedAsp; g_iterMinRemain = savedIMR;
    return o;
}

static bool isWinFor(int side, int score) {
    return side == White ? score >= WhiteWin - 1024 : score <= BlackWin + 1024;
}

// Fold a step's result into the run and the published snapshot. Returns false
// once the run is finished. Runs with the service lock held (native).
static bool commitStep(AnalysisRun& run, const StepOut& o, EngAnalysis& snap) {
    snap.request = run.request;
    snap.side = run.root.side;
    if (!o.error.empty()) {
        snap.error = o.error;
        snap.finished = true; snap.working = 0;
        run.active = false;
        return false;
    }
    if (o.hasStatic) { run.staticDone = true; snap.hasStatic = true; snap.staticEval = o.staticEval; }
    if (run.moves.empty()) {
        snap.finished = true; snap.working = 0;
        run.active = false;
        return false;
    }
    run.score[o.idx] = o.score;
    run.reply[o.idx] = o.reply;
    run.nodes += o.nodes;
    run.pos++;
    int n = (int)run.moves.size();
    snap.nodes = run.nodes;
    snap.ms = msSince(run.t0);
    snap.working = run.d;
    snap.workingDone = run.pos;
    snap.workingTotal = n;

    bool finished = false;
    if (run.pos == n) {
        int side = run.root.side;
        std::vector<int> idx(n);
        for (int i = 0; i < n; i++) idx[i] = i;
        std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) {
            return betterFor(side, run.score[a], run.score[b]);
        });
        snap.depth = run.d;
        snap.lines.clear();
        for (int k = 0; k < n; k++) {
            EngLine L;
            L.move = run.moves[idx[k]];
            L.score = run.score[idx[k]];
            L.reply = run.reply[idx[k]];
            snap.lines.push_back(L);
        }
        run.order = idx;                      // best-first order for the next depth
        run.lastDepthMs = msSince(run.depthT0);
        run.depthT0 = EngClock::now();
        int best = run.score[idx[0]];
        // A proven result for the side to move cannot change with more depth
        // except toward a faster win, which the arrows do not need.
        bool proven = isWinFor(side, best) || isWinFor(side == White ? Black : White, best);
        if (run.d >= run.cfg.maxDepth || proven) finished = true;
#if !GUI_ENGINE_THREADED
        // Web runs steps inside frames: stop before one root move's search would
        // stall a frame for long (the next depth costs roughly 4x this one).
        if (run.lastDepthMs / n * 4.0 > 120.0) finished = true;
#endif
        run.d++;
        run.pos = 0;
    }
    if (run.cfg.maxSeconds > 0.0 && snap.depth >= 1 && msSince(run.t0) > run.cfg.maxSeconds * 1000.0)
        finished = true;
    if (finished) {
        snap.finished = true;
        snap.working = 0;
        run.active = false;
        return false;
    }
    return true;
}

// ============================================================
// SERVICE -- job queue, abort protocol, native thread / web inline driver
// ============================================================
// Abort protocol. A search can only be stopped from inside: budgetTripped()
// (src/ai_minimax.cpp) ends it once nodes >= g_nodeDeadline, and g_nodeDeadline
// is re-seeded at the start of every top-level search. So to abort, the UI
// thread writes g_nodeDeadline = 1 while the job it wants stopped is running,
// which turns every remaining node into an immediate leaf; the search unwinds
// in microseconds and its result is discarded. Cut searches never store into
// the TT (every ttStore is guarded by !s_budgetHit), so nothing leaks.
//
// The write is only ever made with the lock held and only while `s_running`
// names the job being aborted, and the engine thread changes s_running only
// with the lock held. That ordering is what guarantees a poke meant for an
// analysis step can never land on the move search that follows it: by the time
// the engine thread starts that search, the pending abort has been consumed
// under the lock, and the search's own seeding of g_nodeDeadline comes after.
// A poke that lands between two searches of one step is overwritten by the next
// seeding, so engTick re-pokes every frame until the job acknowledges.
// (A plain 64-bit store: the only engine global the UI thread ever writes.)
enum RunKind { RUN_NONE = 0, RUN_MOVE, RUN_ANALYSIS };

#if GUI_ENGINE_THREADED
static std::mutex              s_mu;
static std::condition_variable s_cv;
static std::thread             s_thread;
typedef std::unique_lock<std::mutex> Lock;
#else
struct NoLock { void unlock() {} void lock() {} };
typedef NoLock Lock;
#endif

static bool     s_started = false;
static bool     s_quit = false;
static int      s_running = RUN_NONE;
static bool     s_abortAnalysis = false;
static bool     s_abortMove = false;
static bool     s_newGame = false;
static std::set<int> s_invalidate;
static unsigned s_seed = 1;

static int      s_nextRequest = 1;
static bool     s_moveQueued = false;
static MoveJob  s_moveJob;
static int      s_moveWanted = 0;         // request id the UI is waiting for (0 = none)
static bool     s_moveReady = false;
static EngMoveResult s_moveResult;

static bool     s_anaQueued = false;
static int      s_anaQueuedReq = -1;
static GuiPos   s_anaQueuedPos;
static EngAnalysisConfig s_anaQueuedCfg;
static bool     s_anaStop = false;
static AnalysisRun s_run;                 // engine thread only
static EngAnalysis s_snap;                // published, under the lock
static bool     s_hasSnap = false;

static void pokeLocked() {
    if ((s_abortAnalysis && s_running == RUN_ANALYSIS) || (s_abortMove && s_running == RUN_MOVE))
        g_nodeDeadline = 1;
}

static void notify() {
#if GUI_ENGINE_THREADED
    s_cv.notify_one();
#endif
}

// One unit of engine work, chosen by priority: a new-game reset, then a queued
// move, then one analysis step. Called with the lock held; releases it while
// the work runs. Returns false when there was nothing to do.
static bool runOne(Lock& lk) {
    if (s_newGame) {
        s_newGame = false;
        lk.unlock();
        ttClear();
        retainResetCarry();
        lk.lock();
        return true;
    }
    if (!s_invalidate.empty()) {
        for (std::set<int>::iterator it = s_invalidate.begin(); it != s_invalidate.end(); ++it)
            s_loadedSlots.erase(*it);
        s_invalidate.clear();
    }
    if (s_anaStop) { s_anaStop = false; s_run.active = false; }
    if (s_anaQueued) {
        s_anaQueued = false;
        initRun(s_run, s_anaQueuedReq, s_anaQueuedPos, s_anaQueuedCfg);
        s_snap = EngAnalysis();
        s_snap.request = s_run.request;
        s_snap.side = s_run.root.side;
        s_snap.working = 1;
        s_snap.workingTotal = (int)s_run.moves.size();
        s_hasSnap = true;
    }
    if (s_moveQueued) {
        MoveJob job = s_moveJob;
        s_moveQueued = false;
        s_running = RUN_MOVE;
        lk.unlock();
        EngMoveResult r = runMoveJob(job);
        lk.lock();
        s_running = RUN_NONE;
        s_abortMove = false;
        if (job.request == s_moveWanted) { s_moveResult = r; s_moveReady = true; }
        return true;
    }
    if (s_run.active) {
        s_running = RUN_ANALYSIS;
        lk.unlock();
        StepOut o = computeStep(s_run);
        lk.lock();
        s_running = RUN_NONE;
        if (s_abortAnalysis) {
            s_abortAnalysis = false;          // step discarded; retried unless replaced
        } else {
            commitStep(s_run, o, s_snap);
        }
        return true;
    }
    return false;
}

#if GUI_ENGINE_THREADED
static void engineThreadMain() {
    std::srand(s_seed);                   // rand() state is per thread on MSVC
    Lock lk(s_mu);
    while (!s_quit) {
        if (!runOne(lk))
            s_cv.wait(lk, [] {
                return s_quit || s_newGame || s_moveQueued || s_anaQueued || s_anaStop || s_run.active;
            });
    }
}
#endif

void engInit(unsigned seed) {
    if (s_started) return;
    s_seed = seed ? seed : 1;
    s_started = true;
#if GUI_ENGINE_THREADED
    s_thread = std::thread(engineThreadMain);
#else
    std::srand(s_seed);
#endif
}

void engShutdown() {
    if (!s_started) return;
#if GUI_ENGINE_THREADED
    {
        Lock lk(s_mu);
        s_quit = true;
        s_moveQueued = false;
        s_run.active = false;
        s_abortMove = s_abortAnalysis = true;
        pokeLocked();
    }
    s_cv.notify_one();
    if (s_thread.joinable()) s_thread.join();
#endif
    s_started = false;
}

void engTick() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
    pokeLocked();
#else
    // Web: do the work inline. A move job runs whole (one frame), analysis runs
    // steps for a bounded slice of the frame. The step cap matters where the
    // clock does not advance inside a task (headless Chrome's virtual time),
    // which would otherwise make the slice loop forever.
    Lock lk;
    EngClock::time_point t0 = EngClock::now();
    for (int steps = 0; steps < 64 && msSince(t0) < 10.0; steps++) {
        bool hadMove = s_moveQueued;
        if (!runOne(lk)) break;
        if (hadMove) break;
    }
#endif
}

void engNewGame() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    s_newGame = true;
    notify();
}

void engInvalidateModel(int slot) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    s_invalidate.insert(slot);
    notify();
}

int engRequestMove(const GuiPos& p, const AgentSpec& spec) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    int id = s_nextRequest++;
    s_moveJob.request = id;
    s_moveJob.pos = p;
    s_moveJob.spec = spec;
    s_moveQueued = true;
    s_moveWanted = id;
    s_moveReady = false;
    if (s_running == RUN_ANALYSIS) {        // let the move start now, not after the step
        s_abortAnalysis = true;
#if GUI_ENGINE_THREADED
        pokeLocked();
#endif
    }
    notify();
    return id;
}

bool engMoveBusy() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    return s_moveWanted != 0 && !s_moveReady;
}

bool engTakeMoveResult(EngMoveResult& out) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    if (!s_moveReady) return false;
    out = s_moveResult;
    s_moveReady = false;
    s_moveWanted = 0;
    return true;
}

void engCancelMove() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    s_moveQueued = false;
    s_moveWanted = 0;
    s_moveReady = false;
    if (s_running == RUN_MOVE) {
        s_abortMove = true;
#if GUI_ENGINE_THREADED
        pokeLocked();
#endif
    }
}

int engStartAnalysis(const GuiPos& p, const EngAnalysisConfig& cfg) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    int id = s_nextRequest++;
    s_anaQueued = true;
    s_anaQueuedReq = id;
    s_anaQueuedPos = p;
    s_anaQueuedCfg = cfg;
    s_anaStop = false;
    if (s_running == RUN_ANALYSIS) {
        s_abortAnalysis = true;
#if GUI_ENGINE_THREADED
        pokeLocked();
#endif
    }
    notify();
    return id;
}

void engStopAnalysis() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    s_anaQueued = false;
    s_anaStop = true;
    if (s_running == RUN_ANALYSIS) {
        s_abortAnalysis = true;
#if GUI_ENGINE_THREADED
        pokeLocked();
#endif
    }
    s_snap.working = 0;
    notify();
}

bool engAnalysisSnapshot(EngAnalysis& out) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    if (!s_hasSnap) return false;
    out = s_snap;
    return true;
}

bool engAnalysisRunning() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    return s_anaQueued || (s_run.active && !s_anaStop);
}
