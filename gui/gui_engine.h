// gui_engine.h - the GUI's engine service and its pure game-state helpers.
//
// The engine keeps its whole state in process globals (board, piece counts,
// incremental accumulators, the transposition table, killer tables, model
// slots). A search mutates those globals for its whole duration, so exactly one
// thread may ever touch them. In the GUI that thread is the ENGINE THREAD owned
// by this module (native), and the UI thread never reads or writes an engine
// global at all:
//
//   - The UI keeps the authoritative game position itself as a GuiPos, and
//     checks and applies human moves with the pure guiIsLegal/guiApplyMove
//     helpers below, which read only the GuiPos they are handed.
//   - Every engine computation is a JOB carrying a copy of the position: an
//     agent move (engRequestMove) or a continuous analysis of one position
//     (engStartAnalysis). The engine thread loads the position into the engine
//     globals, runs the job, and publishes a result the UI polls each frame.
//   - Model slots are loaded on the engine thread too, the first time a job
//     needs one, so a slow model load never stalls a frame either.
//
// Jobs run one at a time. A move request pre-empts analysis: the running
// analysis step is aborted and retried after the move. The abort works by
// forcing the search's node deadline (see engTick in gui_engine.cpp).
//
// Web (Emscripten, no threads): the same API runs the work inline from
// engTick(), a bounded slice of analysis per frame and a move job in one frame.
//
// This header deliberately includes no raylib header, and gui_engine.cpp is the
// only GUI file that includes ml_eval.h (whose class Model collides with
// raylib's struct Model, see gui/CLAUDE.md).
#pragma once
#include "globals.h"
#include "agents.h"
#include <string>
#include <vector>

// ============================================================
// POSITIONS AND RULES (pure: safe on the UI thread)
// ============================================================
struct GuiPos {
    char b[SIZE][SIZE];   // [col][row], EMPTY / WHITE / BLACK, same layout as the engine board
    int  side;            // side to move: White or Black
    int  halfMove;        // half-moves played since the game started
};

// A move as (source square, destination column). The destination row is implied
// by the mover (one row forward), and stored in dy for convenience.
struct GuiMove {
    int sx = -1, sy = -1, dx = -1, dy = -1;
    bool valid() const { return sx >= 0; }
    bool operator==(const GuiMove& o) const { return sx == o.sx && sy == o.sy && dx == o.dx; }
};

static const int GUI_MAX_MOVES = 64;

bool guiIsLegal(const GuiPos& p, int sx, int sy, int dx);          // for p.side
int  guiLegalMoves(const GuiPos& p, GuiMove* out);                  // capacity GUI_MAX_MOVES
int  guiApplyMove(GuiPos& p, const GuiMove& m);                     // flips side; returns guiWinner after it
int  guiWinner(const GuiPos& p);                                    // White / Black / None
void guiCountPieces(const GuiPos& p, int& white, int& black);
std::string guiMoveText(const GuiMove& m);                          // engine notation, e.g. "a1b"
bool guiLoadBoardFile(const std::string& path, GuiPos& out, std::string& err);

// ============================================================
// ENGINE SERVICE
// ============================================================
struct EngMoveResult {
    int      request = 0;
    GuiMove  move;                  // invalid when no move was produced (see error)
    bool     byOpener = false;      // the agent's identity-level opener played it
    bool     hasImm = false;        // static eval of the position it moved from,
    int      imm = 0;               //   by its own evaluator (search brains only)
    bool     hasDown = false;       // its predicted best-line eval (alpha-beta only)
    int      down = 0;
    unsigned long long nodes = 0;
    double   effDepth = 0.0;
    double   ms = 0.0;
    std::string error;
};

struct EngLine {
    GuiMove move;
    int     score = 0;              // white-centric
    GuiMove reply;                  // the opponent's best reply at that depth (may be invalid)
};

struct EngAnalysisConfig {
    int    evaluator = 0;
    int    params[MAX_EVAL_PARAMS] = { 0 };
    int    modelSlot = 0;           // LearnedValue only: wired into params[0] by the engine
    bool   useTT = true;
    bool   useMoveOrder = true;
    bool   useQuiescence = false;
    int    maxDepth = 64;
    double maxSeconds = 0.0;        // 0 = no wall-clock limit
};

struct EngAnalysis {
    int  request = -1;
    int  side = White;              // side to move in the analysed position
    int  depth = 0;                 // deepest COMPLETED depth (0 = none yet)
    int  working = 0;               // depth in progress (0 = idle)
    int  workingDone = 0, workingTotal = 0;
    bool hasStatic = false;
    int  staticEval = 0;            // white-centric, side to move's turn term included
    std::vector<EngLine> lines;     // every root move at `depth`, best first for `side`
    unsigned long long nodes = 0;
    double ms = 0.0;
    bool finished = false;
    std::string error;
};

void engInit(unsigned seed);        // after mlAutoLoadDefaultSlots, before the first job
void engShutdown();                 // aborts any running job and joins the thread
void engTick();                     // UI thread, once per frame
void engNewGame();                  // clears the TT and the retain purses before the next job
void engInvalidateModel(int slot);  // reload this slot from disk the next time a job needs it

int  engRequestMove(const GuiPos& p, const AgentSpec& spec);   // returns the request id
bool engMoveBusy();
bool engTakeMoveResult(EngMoveResult& out);                    // once per finished request
void engCancelMove();                                          // discard the in-flight move

int  engStartAnalysis(const GuiPos& p, const EngAnalysisConfig& cfg);   // replaces any other
void engStopAnalysis();
bool engAnalysisSnapshot(EngAnalysis& out);                    // false when none was started
bool engAnalysisRunning();
