// gui_engine.cpp - see gui_engine.h for the threading contract.
//
// Sections (grep "// ==="):
//   RULES              pure GuiPos helpers (UI-thread safe)
//   ENGINE STATE       engine-thread-only helpers: load a position, diff a move, model slots
//   MOVE JOB           one agent move, mirroring src/ranking.cpp's playOneGame dispatch
//   ANALYSIS JOB       iterative-deepening multi-line analysis, one root move per step
//   SERVICE            job queue, abort protocol, native thread / web inline driver
//   WEB ENGINE WORKERS the page's move and analysis workers and their wire format

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
#include <emscripten.h>
#if !defined(GUI_ENGINE_WORKER)
#define GUI_ENGINE_CLIENT 1         // the page: jobs go to two engine workers
#endif
#endif
#if !defined(GUI_ENGINE_CLIENT)
#define GUI_ENGINE_CLIENT 0
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
    // Rows print 1-8, matching the board labels, while the engine indexes them 0-7.
    std::snprintf(buf, sizeof(buf), "%c%d%c", (char)('a' + m.sx), m.sy + 1, (char)('a' + m.dx));
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
    double computeMs = 0.0, depthComputeMs = 0.0;   // time inside computeStep only
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

// Why an analysis run ended (the web log's `stop` field).
enum AnaStop { ANA_GOING = 0, ANA_MAX_DEPTH, ANA_PROVEN, ANA_WEB_CAP, ANA_MAX_SECONDS };

#if !GUI_ENGINE_THREADED
// Web measurement log, read from the page as window.__anaLog (one record per
// completed depth or run end) and window.__moveLog (one per agent move job).
// Wall ms span frames, so they include rendering and the browser's idle time
// between slices. Compute ms count only the engine's own work.
// In an engine worker the record is posted to the page, which appends it to the
// same array (engWkSpawn). There `t` is on the worker's own clock.
EM_JS(void, webAnaLog, (int req, int depth, int moves, double depthWallMs, double depthComputeMs,
                        double wallMs, double computeMs, double nodes, int best, int second, int third,
                        int stop), {
    var rec = { req: req, depth: depth, moves: moves, depthWallMs: depthWallMs, depthComputeMs: depthComputeMs,
                wallMs: wallMs, computeMs: computeMs, nodes: nodes, best: best, second: second, third: third,
                stop: stop, t: performance.now() };
    if (typeof window === "undefined") { postMessage({ log: "__anaLog", rec: rec }); return; }
    var L = window.__anaLog || (window.__anaLog = []);
    if (L.length >= 20000) L.splice(0, 10000);
    L.push(rec);
});
EM_JS(void, webMoveLog, (int req, double ms, double nodes, double effDepth), {
    var rec = { req: req, ms: ms, nodes: nodes, effDepth: effDepth, t: performance.now() };
    if (typeof window === "undefined") { postMessage({ log: "__moveLog", rec: rec }); return; }
    var L = window.__moveLog || (window.__moveLog = []);
    if (L.length >= 20000) L.splice(0, 10000);
    L.push(rec);
});
#endif

// Web inline engine only: stop before a depth whose predicted cost per root
// move exceeds this many ms (0 = never), since each root move's search runs
// inside one frame. An engine worker runs with no cap.
static double s_webStepCapMs = 120.0;
void engSetWebStepCap(double ms) { s_webStepCapMs = ms < 0.0 ? 0.0 : ms; }

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
    int stop = ANA_GOING;
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
        double depthComputeMs = run.depthComputeMs;
        run.depthComputeMs = 0.0;
        int best = run.score[idx[0]];
        // A proven result for the side to move cannot change with more depth
        // except toward a faster win, which the arrows do not need.
        bool proven = isWinFor(side, best) || isWinFor(side == White ? Black : White, best);
        if (run.d >= run.cfg.maxDepth) { finished = true; stop = ANA_MAX_DEPTH; }
        else if (proven) { finished = true; stop = ANA_PROVEN; }
#if !GUI_ENGINE_THREADED
        // Web runs steps inside frames: stop before one root move's search would
        // stall a frame for long (the next depth costs roughly 4x this one).
        else if (s_webStepCapMs > 0.0 && run.lastDepthMs / n * 4.0 > s_webStepCapMs) {
            finished = true; stop = ANA_WEB_CAP;
        }
#endif
        if (!finished && run.cfg.maxSeconds > 0.0 && msSince(run.t0) > run.cfg.maxSeconds * 1000.0) {
            finished = true; stop = ANA_MAX_SECONDS;
        }
#if !GUI_ENGINE_THREADED
        webAnaLog(run.request, run.d, n, run.lastDepthMs, depthComputeMs, msSince(run.t0), run.computeMs,
                  (double)run.nodes, best, n > 1 ? run.score[idx[1]] : best, n > 2 ? run.score[idx[2]] : best, stop);
#endif
        run.d++;
        run.pos = 0;
    }
    if (!finished && run.cfg.maxSeconds > 0.0 && snap.depth >= 1 && msSince(run.t0) > run.cfg.maxSeconds * 1000.0) {
        finished = true; stop = ANA_MAX_SECONDS;
#if !GUI_ENGINE_THREADED
        int b0 = snap.lines.empty() ? 0 : snap.lines[0].score;
        webAnaLog(run.request, snap.depth, n, 0.0, 0.0, msSince(run.t0), run.computeMs, (double)run.nodes,
                  b0, snap.lines.size() > 1 ? snap.lines[1].score : b0, snap.lines.size() > 2 ? snap.lines[2].score : b0,
                  stop);
#endif
    }
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

// The page side of the web engine workers (WEB ENGINE WORKERS, below).
#if GUI_ENGINE_CLIENT
static bool clientActive();
static bool clientInit(unsigned seed);
static void clientShutdown();
static void clientTick();
static void clientNewGame();
static void clientInvalidate(int slot);
static void clientRequestMove(int id, const GuiPos& p, const AgentSpec& spec);
static void clientCancelMove();
static void clientStartAnalysis(int id, const GuiPos& p, const EngAnalysisConfig& cfg);
static void clientStopAnalysis();
#endif

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
#if !GUI_ENGINE_THREADED
        webMoveLog(job.request, r.ms, (double)r.nodes, r.effDepth);
#endif
        lk.lock();
        s_running = RUN_NONE;
        s_abortMove = false;
        if (job.request == s_moveWanted) { s_moveResult = r; s_moveReady = true; }
        return true;
    }
    if (s_run.active) {
        s_running = RUN_ANALYSIS;
        lk.unlock();
        EngClock::time_point st = EngClock::now();
        StepOut o = computeStep(s_run);
        double stepMs = msSince(st);
        lk.lock();
        s_running = RUN_NONE;
        if (s_abortAnalysis) {
            s_abortAnalysis = false;          // step discarded; retried unless replaced
        } else {
            s_run.computeMs += stepMs;
            s_run.depthComputeMs += stepMs;
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
#if GUI_ENGINE_CLIENT
    clientInit(s_seed);
#endif
}

void engShutdown() {
    if (!s_started) return;
#if GUI_ENGINE_CLIENT
    clientShutdown();
#endif
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
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientTick(); return; }
#endif
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
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientNewGame(); return; }
#endif
    s_newGame = true;
    notify();
}

void engInvalidateModel(int slot) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientInvalidate(slot); return; }
#endif
    s_invalidate.insert(slot);
    notify();
}

static void queueMove(int id, const GuiPos& p, const AgentSpec& spec) {
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
}

static void queueAnalysis(int id, const GuiPos& p, const EngAnalysisConfig& cfg) {
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
}

int engRequestMove(const GuiPos& p, const AgentSpec& spec) {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
    int id = s_nextRequest++;
#if GUI_ENGINE_CLIENT
    if (clientActive()) {
        s_moveWanted = id;
        s_moveReady = false;
        clientRequestMove(id, p, spec);
        return id;
    }
#endif
    queueMove(id, p, spec);
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
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientCancelMove(); return; }
#endif
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
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientStartAnalysis(id, p, cfg); return id; }
#endif
    queueAnalysis(id, p, cfg);
    return id;
}

void engStopAnalysis() {
#if GUI_ENGINE_THREADED
    Lock lk(s_mu);
#endif
#if GUI_ENGINE_CLIENT
    if (clientActive()) { clientStopAnalysis(); return; }
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
#if GUI_ENGINE_CLIENT
    if (clientActive())
        return s_anaQueuedReq > 0 && !(s_hasSnap && s_snap.request == s_anaQueuedReq && s_snap.finished);
#endif
    return s_anaQueued || (s_run.active && !s_anaStop);
}

// ============================================================
// WEB ENGINE WORKERS -- the page's move and analysis workers
// ============================================================
// On the web the page runs no engine work itself. It starts two copies of
// build/web/engine_worker.js (this file compiled with -DGUI_ENGINE_WORKER, plus
// gui/engine_worker_pre.js): a MOVE worker for agent moves and an ANALYSIS
// worker for the arrows and the eval bar. Each worker has its own engine
// globals, model slots, and transposition table, so analysis entries never
// evict an agent's, and neither job runs inside a frame.
//
// A worker reads a command only between two units of work (one move job, or one
// root move's search), so a search cannot be cut short from the page: there is
// no shared memory to poke g_nodeDeadline through (SharedArrayBuffer needs
// cross-origin isolation headers, which GitHub Pages cannot send). Instead:
//   - MOVE: a move job is bounded by its agent's own budget. A cancelled move
//     runs to completion and its result is dropped by request id.
//   - ANALYSIS: the worker acknowledges every analysis command when it reads
//     it. An acknowledgement more than WK_ANA_RESPAWN_MS late means the worker
//     is inside a long root-move search, so the page terminates it, starts a
//     fresh one, and resends the command. Only the analysis TT is lost.
// If a worker cannot be started, reports an error, or has not finished loading
// WK_START_MS after it was started, the page falls back to the inline engine
// above for the rest of the session. A worker loads in well under a second on
// an idle machine, but wasm compilation and startup can be starved for far
// longer when the CPU is saturated (three headless Chromes rendering in
// software took 13 s to over 2 minutes), and a move must not wait on that.
//
// Wire format: each message is a flat byte array of trivially copyable fields
// in a fixed order. Both ends are this file built by the same compiler.
#if defined(__EMSCRIPTEN__)
enum WkCmd { CMD_INIT = 1, CMD_MOVE, CMD_CANCEL_MOVE, CMD_NEW_GAME, CMD_INVALIDATE, CMD_ANA_START, CMD_ANA_STOP };
enum WkMsg { MSG_READY = 1, MSG_MOVE, MSG_SNAP, MSG_ACK, MSG_ERROR = 99 };

struct WkWriter {
    std::vector<unsigned char> b;
    template <class T> void pod(const T& v) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
        b.insert(b.end(), p, p + sizeof(T));
    }
    void str(const std::string& s) { pod((int)s.size()); b.insert(b.end(), s.begin(), s.end()); }
};

struct WkReader {
    const unsigned char* p;
    int  n, at;
    bool ok;
    WkReader(const unsigned char* p_, int n_) : p(p_), n(n_), at(0), ok(true) {}
    template <class T> T pod() {
        T v;
        if (!ok || at + (int)sizeof(T) > n) { ok = false; return T(); }
        std::memcpy(&v, p + at, sizeof(T));
        at += (int)sizeof(T);
        return v;
    }
    std::string str() {
        int k = pod<int>();
        if (!ok || k < 0 || at + k > n) { ok = false; return std::string(); }
        std::string s(reinterpret_cast<const char*>(p + at), k);
        at += k;
        return s;
    }
};

static void putMoveResult(WkWriter& w, const EngMoveResult& r) {
    w.pod(r.request); w.pod(r.move); w.pod(r.byOpener); w.pod(r.hasImm); w.pod(r.imm);
    w.pod(r.hasDown); w.pod(r.down); w.pod(r.nodes); w.pod(r.effDepth); w.pod(r.ms); w.str(r.error);
}

static EngMoveResult getMoveResult(WkReader& r) {
    EngMoveResult o;
    o.request = r.pod<int>(); o.move = r.pod<GuiMove>(); o.byOpener = r.pod<bool>(); o.hasImm = r.pod<bool>();
    o.imm = r.pod<int>(); o.hasDown = r.pod<bool>(); o.down = r.pod<int>();
    o.nodes = r.pod<unsigned long long>(); o.effDepth = r.pod<double>(); o.ms = r.pod<double>(); o.error = r.str();
    return o;
}

static void putSnap(WkWriter& w, const EngAnalysis& a) {
    w.pod(a.request); w.pod(a.side); w.pod(a.depth); w.pod(a.working); w.pod(a.workingDone); w.pod(a.workingTotal);
    w.pod(a.hasStatic); w.pod(a.staticEval); w.pod(a.nodes); w.pod(a.ms); w.pod(a.finished); w.str(a.error);
    w.pod((int)a.lines.size());
    for (size_t i = 0; i < a.lines.size(); i++) w.pod(a.lines[i]);
}

static bool getSnap(WkReader& r, EngAnalysis& a) {
    a.request = r.pod<int>(); a.side = r.pod<int>(); a.depth = r.pod<int>(); a.working = r.pod<int>();
    a.workingDone = r.pod<int>(); a.workingTotal = r.pod<int>(); a.hasStatic = r.pod<bool>();
    a.staticEval = r.pod<int>(); a.nodes = r.pod<unsigned long long>(); a.ms = r.pod<double>();
    a.finished = r.pod<bool>(); a.error = r.str();
    int n = r.pod<int>();
    if (!r.ok || n < 0 || n > GUI_MAX_MOVES) return false;
    a.lines.resize(n);
    for (int i = 0; i < n; i++) a.lines[i] = r.pod<EngLine>();
    return r.ok;
}
#endif

#if defined(GUI_ENGINE_WORKER)
// ---- worker side: called by gui/engine_worker_pre.js ----
EM_JS(void, wkPost, (int kind, const unsigned char* p, int n), {
    var b = HEAPU8.slice(p, p + n);
    postMessage({ kind: kind, bytes: b }, [b.buffer]);
});

static void wkSend(int kind, const WkWriter& w) {
    static const unsigned char none = 0;
    wkPost(kind, w.b.empty() ? &none : &w.b[0], (int)w.b.size());
}

static void wkAck(int seq) { WkWriter w; w.pod(seq); wkSend(MSG_ACK, w); }

extern "C" EMSCRIPTEN_KEEPALIVE void wk_message(const unsigned char* p, int n) {
    WkReader r(p, n);
    int cmd = r.pod<int>();
    switch (cmd) {
    case CMD_INIT: {
        unsigned seed = r.pod<unsigned>();
        mlAutoLoadDefaultSlots();
        s_webStepCapMs = 0.0;               // nothing here shares a frame with rendering
        engInit(seed);
        wkSend(MSG_READY, WkWriter());
        break;
    }
    case CMD_MOVE: {
        int id = r.pod<int>();
        GuiPos pos = r.pod<GuiPos>();
        AgentSpec spec = r.pod<AgentSpec>();
        if (r.ok) queueMove(id, pos, spec);
        break;
    }
    case CMD_CANCEL_MOVE:
        s_moveQueued = false;
        s_moveWanted = 0;
        s_moveReady = false;
        break;
    case CMD_NEW_GAME:
        s_newGame = true;
        break;
    case CMD_INVALIDATE: {
        int slot = r.pod<int>();
        if (r.ok) s_invalidate.insert(slot);
        break;
    }
    case CMD_ANA_START: {
        int seq = r.pod<int>();
        int id = r.pod<int>();
        GuiPos pos = r.pod<GuiPos>();
        EngAnalysisConfig cfg = r.pod<EngAnalysisConfig>();
        if (r.ok) queueAnalysis(id, pos, cfg);
        wkAck(seq);
        break;
    }
    case CMD_ANA_STOP: {
        int seq = r.pod<int>();
        s_anaQueued = false;
        s_anaStop = true;
        s_snap.working = 0;
        wkAck(seq);
        break;
    }
    default:
        break;
    }
}

// What the page last saw of the analysis, so a snapshot is posted when the
// depth, request, or state changes, and progress within a depth at most every
// 100 ms.
static int  s_postedReq = -1, s_postedDepth = -1, s_postedWorking = -1, s_postedDone = -1;
static bool s_postedFinished = false;
static EngClock::time_point s_postedAt;

// One unit of work. Returns 1 when there may be more.
extern "C" EMSCRIPTEN_KEEPALIVE int wk_pump() {
    Lock lk;
    bool did = runOne(lk);
    if (s_moveReady) {
        WkWriter w;
        putMoveResult(w, s_moveResult);
        wkSend(MSG_MOVE, w);
        s_moveReady = false;
        s_moveWanted = 0;
    }
    if (s_hasSnap) {
        const EngAnalysis& a = s_snap;
        bool changed = a.request != s_postedReq || a.depth != s_postedDepth || a.working != s_postedWorking ||
                       a.finished != s_postedFinished;
        bool progress = a.workingDone != s_postedDone && msSince(s_postedAt) >= 100.0;
        if (changed || progress) {
            WkWriter w;
            putSnap(w, a);
            wkSend(MSG_SNAP, w);
            s_postedReq = a.request; s_postedDepth = a.depth; s_postedWorking = a.working;
            s_postedDone = a.workingDone; s_postedFinished = a.finished;
            s_postedAt = EngClock::now();
        }
    }
    return did ? 1 : 0;
}
#endif

#if GUI_ENGINE_CLIENT
// ---- page side ----
enum { WK_MOVE = 0, WK_ANA = 1 };
static const double WK_ANA_RESPAWN_MS = 150.0;
static const double WK_START_MS = 4000.0;

// Start (or restart) worker `which`. Its messages queue in window.__engWk.q
// until engTick drains them; a replaced worker's late messages are dropped.
// Measurement records it posts go straight to the page's log arrays.
EM_JS(int, engWkSpawn, (int which), {
    try {
        var W = window.__engWk || (window.__engWk = { w: [null, null], q: [], spawns: 0 });
        if (W.w[which]) W.w[which].terminate();
        var w = new Worker("engine_worker.js");
        w.onmessage = function (e) {
            var d = e.data;
            if (d.log) {
                var L = window[d.log] || (window[d.log] = []);
                if (L.length >= 20000) L.splice(0, 10000);
                d.rec.worker = which;
                L.push(d.rec);
                return;
            }
            if (W.w[which] === w) W.q.push({ which: which, kind: d.kind, bytes: d.bytes });
        };
        w.onerror = function (e) {
            if (W.w[which] === w) W.q.push({ which: which, kind: 99, bytes: new Uint8Array(0) });
            if (e && e.preventDefault) e.preventDefault();
        };
        W.w[which] = w;
        W.spawns++;
        return 1;
    } catch (err) {
        return 0;
    }
});

EM_JS(void, engWkSend, (int which, const unsigned char* p, int n), {
    var W = window.__engWk;
    if (!W || !W.w[which]) return;
    var b = HEAPU8.slice(p, p + n);
    W.w[which].postMessage(b, [b.buffer]);
});

// Pops one queued message into buf. Returns its length (-1 = none), and
// writes the worker index and message kind into meta[0] and meta[1].
EM_JS(int, engWkPop, (unsigned char* buf, int cap, int* meta), {
    var W = window.__engWk;
    if (!W || !W.q.length) return -1;
    var m = W.q.shift();
    var n = Math.min(m.bytes.length, cap);
    HEAPU8.set(m.bytes.subarray(0, n), buf);
    HEAP32[meta >> 2] = m.which;
    HEAP32[(meta >> 2) + 1] = m.kind;
    return n;
});

EM_JS(void, engWkStopAll, (), {
    var W = window.__engWk;
    if (!W) return;
    for (var i = 0; i < W.w.length; i++) if (W.w[i]) { W.w[i].terminate(); W.w[i] = null; }
    W.q = [];
});

static bool     s_cl = false;                 // the workers are running (else the inline engine)
static unsigned s_clSeed = 1;
static int      s_clSeq = 0;
static bool     s_clReady[2] = { false, false };    // each worker has finished loading
static double   s_clSpawnAt[2] = { 0.0, 0.0 };     // when each worker was last started
static int      s_clAnaPending = 0;           // seq of the analysis command awaiting its ack (0 = none)
static double   s_clAnaSentAt = 0.0;
static std::vector<unsigned char> s_clAnaLast;    // the last analysis command, resent after a restart
static GuiPos   s_clMovePos, s_clAnaPos;          // kept for the inline fallback
static AgentSpec s_clMoveSpec;
static EngAnalysisConfig s_clAnaCfg;

static bool clientActive() { return s_cl; }

static void clSend(int which, const WkWriter& w) { engWkSend(which, &w.b[0], (int)w.b.size()); }

static void clInitWorker(int which) {
    WkWriter w;
    w.pod((int)CMD_INIT);
    w.pod(s_clSeed);
    clSend(which, w);
    s_clReady[which] = false;
    s_clSpawnAt[which] = emscripten_get_now();
}

// An analysis command carries its seq at byte 4, restamped on every send.
static void clSendAna(const std::vector<unsigned char>& bytes) {
    s_clAnaLast = bytes;
    int seq = ++s_clSeq;
    std::memcpy(&s_clAnaLast[4], &seq, sizeof(seq));
    s_clAnaPending = seq;
    s_clAnaSentAt = emscripten_get_now();
    engWkSend(WK_ANA, &s_clAnaLast[0], (int)s_clAnaLast.size());
}

// ?engine=inline in the page URL keeps the engine on the page, for comparing
// the two paths in one build (tools/web_ana_bench.ps1).
EM_JS(int, engWkWanted, (), {
    return new URLSearchParams(window.location.search).get("engine") === "inline" ? 0 : 1;
});

static bool clientInit(unsigned seed) {
    s_clSeed = seed;
    if (!engWkWanted()) return false;
    s_cl = engWkSpawn(WK_MOVE) && engWkSpawn(WK_ANA);
    if (!s_cl) { engWkStopAll(); return false; }
    clInitWorker(WK_MOVE);
    clInitWorker(WK_ANA);
    return true;
}

static void clientShutdown() { engWkStopAll(); s_cl = false; }

// A worker failed: run everything inline from now on, picking up the move and
// the analysis that were in flight.
static void clientFallback() {
    EM_ASM({ console.warn("Engine worker failed or was too slow to load; running the engine on the page instead."); });
    engWkStopAll();
    s_cl = false;
    if (s_moveWanted != 0 && !s_moveReady) queueMove(s_moveWanted, s_clMovePos, s_clMoveSpec);
    if (s_anaQueuedReq > 0) queueAnalysis(s_anaQueuedReq, s_clAnaPos, s_clAnaCfg);
}

static void clientNewGame() {
    WkWriter w;
    w.pod((int)CMD_NEW_GAME);
    clSend(WK_MOVE, w);
    clSend(WK_ANA, w);
}

static void clientInvalidate(int slot) {
    WkWriter w;
    w.pod((int)CMD_INVALIDATE);
    w.pod(slot);
    clSend(WK_MOVE, w);
    clSend(WK_ANA, w);
}

static void clientRequestMove(int id, const GuiPos& p, const AgentSpec& spec) {
    s_clMovePos = p;
    s_clMoveSpec = spec;
    WkWriter w;
    w.pod((int)CMD_MOVE);
    w.pod(id);
    w.pod(p);
    w.pod(spec);
    clSend(WK_MOVE, w);
}

static void clientCancelMove() {
    WkWriter w;
    w.pod((int)CMD_CANCEL_MOVE);
    clSend(WK_MOVE, w);
}

static void clientStartAnalysis(int id, const GuiPos& p, const EngAnalysisConfig& cfg) {
    s_anaQueuedReq = id;                      // what engAnalysisRunning and the fallback read
    s_clAnaPos = p;
    s_clAnaCfg = cfg;
    WkWriter w;
    w.pod((int)CMD_ANA_START);
    w.pod(0);                                 // seq, stamped by clSendAna
    w.pod(id);
    w.pod(p);
    w.pod(cfg);
    clSendAna(w.b);
}

static void clientStopAnalysis() {
    s_anaQueuedReq = 0;
    s_snap.working = 0;
    WkWriter w;
    w.pod((int)CMD_ANA_STOP);
    w.pod(0);
    clSendAna(w.b);
}

static void clientTick() {
    static unsigned char buf[16384];
    int meta[2] = { 0, 0 };
    int n;
    while ((n = engWkPop(buf, (int)sizeof(buf), meta)) >= 0) {
        int which = meta[0], kind = meta[1];
        if (kind == MSG_ERROR) { clientFallback(); return; }
        WkReader r(buf, n);
        if (which == WK_MOVE && kind == MSG_MOVE) {
            EngMoveResult res = getMoveResult(r);
            if (r.ok && s_moveWanted != 0 && res.request == s_moveWanted) { s_moveResult = res; s_moveReady = true; }
        } else if (kind == MSG_READY) {
            s_clReady[which] = true;
            if (which == WK_ANA) s_clAnaSentAt = emscripten_get_now();   // the grace period starts once it can read
        } else if (which == WK_ANA && kind == MSG_SNAP) {
            EngAnalysis a;
            if (getSnap(r, a)) { s_snap = a; s_hasSnap = true; }
        } else if (which == WK_ANA && kind == MSG_ACK) {
            int seq = r.pod<int>();
            if (r.ok && seq == s_clAnaPending) s_clAnaPending = 0;
        }
    }
    double now = emscripten_get_now();
    for (int i = 0; i < 2; i++)
        if (!s_clReady[i] && now - s_clSpawnAt[i] > WK_START_MS) { clientFallback(); return; }
    if (s_clAnaPending && s_clReady[WK_ANA] && now - s_clAnaSentAt > WK_ANA_RESPAWN_MS) {
        if (!engWkSpawn(WK_ANA)) { clientFallback(); return; }
        clInitWorker(WK_ANA);
        clSendAna(s_clAnaLast);
    }
}
#endif
