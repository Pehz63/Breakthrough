// main_gui.cpp - Raylib + raygui front end for Breakthrough.
//
// This is a thin, additive GUI layer over the existing console engine. It never
// calls getSettings(), playerMove(), or printBoard(). Instead it:
//   - reads the global board[col][row] each frame to render,
//   - drives human moves from mouse clicks via tryMove*/playMove*,
//   - drives AI moves via agentChooseMove(AgentSpec, side) -- the same composition
//     the ranked agent pool uses, so a side can be set to any agent expressible by
//     AgentSpec (any explorer/evaluator, budgets, search feature toggles, a
//     learned model, a policy brain, dilution, an opener) -- run on a background
//     thread (native) so the window stays responsive while it thinks,
//   - detects wins by scanning the goal rows + piece counts (robust regardless
//     of which engine path made the move).
//
// agents.cpp/explorers.cpp/choosers.cpp (build_gui.bat AND build_web.bat) give
// both platforms the AgentSpec composition above and the structured dropdown/
// slider agent editor (DrawAgentEditor). ranking.cpp (build_gui.bat only) adds
// the canonical-ID codec (rankAgentId/rankAgentFromId, src/ranking.h) on top: a
// free-text id box that round-trips through ranking/roster.txt's exact ID
// grammar, plus a persisted history of previously-used ids. It cannot link into
// the web build (unconditionally #include <windows.h>), so every symbol/block
// that needs it is guarded by "#if !defined(PLATFORM_WEB)" -- search that string
// in this file for the exact boundary. The web build still gets full AgentSpec
// selection through the same structured editor, just not the id textbox/history.
//
// The same source compiles to a native window (while !WindowShouldClose) and to
// the web via Emscripten (emscripten_set_main_loop), see main() at the bottom.
//
// Sections (grep "// ==="):
//   LAYOUT / WINDOW CONSTANTS
//   COLORS
//   REGISTRY LOOKUPS     IsAlphaBetaExplorer / ExplorerIndexByName / ChooserIndexByName
//   APP STATE            PlayerConfig (Human, or an AgentSpec + its canonical id)
//   AGENT HISTORY        validated, disk-persisted memory of previously-used agent ids
//   AGENT EDITOR STATE   the "Edit Agent..." popup's working copy + scratch/sync helpers
//   AI WORKER + VIEW     SyncView (render snapshot) / background-thread search
//   PACING STATE
//   HELPERS              SetStatus / CheckWinner / LogMove / DiffMove / AgentSummary
//   GAME FLOW            AfterMove / StartGame / Launch+FinalizeAIMove / HandleHumanClick
//   UPDATE               per-frame state machine
//   LAYOUT COMPUTE       ComputeLayout
//   BOARD RENDERING      DrawPiece / DrawBoard
//   STEPPER WIDGETS      DrawSpeedGlyph / DrawStackedPM / DrawFillBar / ScrubBar / StepperRow
//   PLAYER CONFIG + PANEL  DrawPlayerBlock / DrawPanel
//   AGENT EDITOR POPUP   DrawAgentEditor: id textbox + history + dropdowns + steppers
//   COUNT BADGES + EVAL  DrawCountBadge / FormatEval / DrawEvalReadout / DrawPieceCounts
//   GAME OVER            DrawGameOverBanner
//   MAIN LOOP            UpdateDrawFrame / main

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// raylib.h defines WHITE/BLACK as Color macros, which collide with globals.h's
// board macros (#define WHITE 'W', BLACK 'B'). We need the board macros, so drop
// the raylib color macros and use explicit Color literals for drawing instead.
#undef WHITE
#undef BLACK

#include "globals.h"
#include "ai_eval.h"    // evaluator registry (g_evaluators / g_evalCount / MAX_EVAL_PARAMS)
#include "agents.h"     // AgentSpec, agentChooseMove, agentMakeSearch/Policy, learnedValueIndex, agentDescribe
#include "explorers.h"  // g_explorers / g_explorerCount
#include "choosers.h"   // g_choosers / g_chooserCount
#include "ai_random.h"  // g_openers / g_openerCount / OpenerDef
// agents.h/explorers.h/choosers.h are portable (no <windows.h>), so both native and
// web link agents.cpp/explorers.cpp/choosers.cpp (build_gui.bat / build_web.bat) and
// every side can be driven by a full AgentSpec on both platforms. ranking.h is
// native-only: ranking.cpp unconditionally #include <windows.h> (GetProcessTimes),
// which does not compile under Emscripten, so only the native build gets the
// canonical-ID codec (rankAgentId/rankAgentFromId) -- the free-text id box and its
// history are native-only for the same reason (search "PLATFORM_WEB" in this file).
#if !defined(PLATFORM_WEB)
#include "ranking.h"    // rankAgentId / rankAgentFromId / rankLoadAgentModels
#endif
// Not including ml_eval.h on EITHER platform: it drags in ml_model.h's "class Model"
// (the ML system's base model type), which collides with raylib.h's own "struct
// Model" (a 3D model asset) -- the same kind of vendored-header name collision
// WHITE/BLACK already work around above, but this one is a type name, not a macro,
// so #undef can't fix it. GUI_MODEL_SLOTS mirrors ml_eval.h's ML_SLOTS, and
// mlLoadSlot is forward-declared locally where needed, for that reason.

#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <fstream>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#else
#include <thread>
#include <atomic>
#endif

// ============================================================
// LAYOUT / WINDOW CONSTANTS
// ============================================================
// Initial window size (the window is resizable; geometry below is recomputed
// each frame from the live window size).
static const int INIT_W = 1024;
static const int INIT_H = 768;
static const int MIN_W  = 820;
static const int MIN_H  = 620;

static const int TOP      = 44;    // top bar height (title + Options toggle)
static const int MARGIN   = 28;    // space around the board for row/col labels
static const int PANEL_W  = 210;   // left options panel width
static const int BADGE_STRIP = 64; // right strip reserved for piece-count badges

// Board geometry, recomputed every frame by ComputeLayout().
static int       g_cell    = 64;
static int       g_boardX  = 0;
static int       g_boardY  = 0;
static int       g_boardPx = 0;
static Rectangle g_panelRect = { 0, 0, 0, 0 };

// ============================================================
// COLORS
// ============================================================
// Drawing colors (explicit literals; raylib WHITE/BLACK macros were undef'd)
static const Color COL_LIGHT   = { 222, 210, 180, 255 };
static const Color COL_DARK    = { 140, 110,  78, 255 };
static const Color COL_BG      = {  34,  36,  44, 255 };
static const Color COL_WPIECE  = { 245, 242, 232, 255 };
static const Color COL_WEDGE   = { 120, 120, 120, 255 };
static const Color COL_BPIECE  = {  32,  34,  40, 255 };
static const Color COL_BEDGE   = { 200, 200, 200, 255 };
static const Color COL_SEL     = { 250, 210,  70, 200 };
static const Color COL_HINT    = {  70, 200, 120, 170 };
static const Color COL_HOVER   = { 255, 255, 255,  45 };
static const Color COL_LABEL   = { 200, 200, 210, 255 };
// Stepper-control palette (bar track / fill / border / number text)
static const Color COL_TRK     = {  46,  49,  60, 255 };
static const Color COL_FILL    = {  86, 158, 222, 255 };
static const Color COL_BRD     = {  92,  96, 110, 255 };
static const Color COL_NUM     = { 236, 239, 246, 255 };

// Mirrors src/ml_eval.h's ML_SLOTS (1024). Kept as a separate constant rather than
// including ml_eval.h -- see the include comment above for why.
static const int GUI_MODEL_SLOTS = 1024;

// ============================================================
// REGISTRY LOOKUPS
// ============================================================
// Small name-based lookups into the pluggable-axis registries (src/explorers.h,
// src/choosers.h), used by both the panel and the agent editor. Registries are
// compile-time constant tables, so a linear scan is cheap and these need no cache.
static bool IsAlphaBetaExplorer(int idx) {
    return idx >= 0 && idx < g_explorerCount && string(g_explorers[idx].name) == "AlphaBeta";
}
static int ExplorerIndexByName(const char *n) {
    for (int i = 0; i < g_explorerCount; i++) if (string(g_explorers[i].name) == n) return i;
    return -1;
}
static int ChooserIndexByName(const char *n) {
    for (int i = 0; i < g_chooserCount; i++) if (string(g_choosers[i].name) == n) return i;
    return -1;
}
static int EvaluatorIndexByName(const char *n) {
    for (int i = 0; i < g_evalCount; i++) if (string(g_evaluators[i].name) == n) return i;
    return -1;
}

// ============================================================
// APP STATE
// ============================================================
enum class AppState { Settings, WaitingForHuman, WaitingBeforeAI, ComputingAI, GameOver };

// A side is either Human (click-driven) or an Agent: a full AgentSpec (the same
// composition the ranked agent pool uses), plus its canonical id string cached
// alongside it (kept in sync via rankAgentId whenever spec changes).
struct PlayerConfig {
    bool      isHuman = false;
    AgentSpec spec;
    string    id;      // meaningful only when !isHuman
};

// The GUI's historical default: MiniMax depth 8 over Classic, spelled as an agent.
static PlayerConfig MakeDefaultBlackAgent() {
    PlayerConfig c;
    c.isHuman = false;
    int ab = ExplorerIndexByName("AlphaBeta");
    int classic = EvaluatorIndexByName("Classic");
    if (ab < 0) ab = 0;
    if (classic < 0) classic = 0;
    c.spec = agentMakeSearch("gui", ab, classic, 8, 0);
#if !defined(PLATFORM_WEB)
    c.id = rankAgentId(c.spec);            // canonical id: round-trips through the id textbox
#else
    c.id = agentDescribe(c.spec);          // no codec on web; a readable summary is enough (no textbox to round-trip through)
#endif
    return c;
}

static AppState     g_state = AppState::Settings;
static PlayerConfig g_white;   // main() sets isHuman = true (the historical default)
static PlayerConfig g_black;   // main() sets this to MakeDefaultBlackAgent()
static int          g_turn   = White;   // whose move it is
static int          g_winner = None;

// ============================================================
// AGENT HISTORY -- validated, disk-persisted memory of previously-used agent ids
// ============================================================
// Native-only: every function here validates through rankAgentFromId, which needs
// ranking.cpp (see the include comment near the top of this file). There is no
// free-text id entry on web to build a history of in the first place (see
// DrawAgentEditor), so this whole section is compiled out there.
#if !defined(PLATFORM_WEB)
// An agent id isn't side-specific, so both White's and Black's editors share one
// list. Plain text, one canonical id per line, most-recent-first: the project's
// existing local-config convention (minimax_params.txt), gitignored (it's local
// MRU state, not project history).
static const char  *AGENT_HISTORY_FILE = "gui_agent_history.txt";
static const size_t AGENT_HISTORY_MAX  = 30;
static std::vector<string> g_agentHistory;

static void LoadAgentHistory() {
    g_agentHistory.clear();
    std::ifstream f(AGENT_HISTORY_FILE);
    if (!f.is_open()) return;
    string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty()) continue;
        RankAgent out; string err;
        if (rankAgentFromId(line, out, err))   // drop stale entries silently (e.g. a
            g_agentHistory.push_back(line);    // deleted model file) -- same tolerance
        if (g_agentHistory.size() >= AGENT_HISTORY_MAX) break;   // rankLoadRoster has
    }
}

static void SaveAgentHistory() {
    std::ofstream f(AGENT_HISTORY_FILE, std::ios::trunc);
    if (!f.is_open()) return;
    for (size_t i = 0; i < g_agentHistory.size(); i++) f << g_agentHistory[i] << "\n";
}

// Move (or insert) a validated canonical id to the front of the history and
// persist it. Called after every successful Apply in the agent editor.
static void RememberAgentId(const string &id) {
    for (size_t i = 0; i < g_agentHistory.size(); i++)
        if (g_agentHistory[i] == id) { g_agentHistory.erase(g_agentHistory.begin() + i); break; }
    g_agentHistory.insert(g_agentHistory.begin(), id);
    if (g_agentHistory.size() > AGENT_HISTORY_MAX) g_agentHistory.resize(AGENT_HISTORY_MAX);
    SaveAgentHistory();
}
#endif // !PLATFORM_WEB (AGENT HISTORY)

// ============================================================
// AGENT EDITOR STATE -- the "Edit Agent..." popup's working copy
// ============================================================
// One popup at a time (side: -1 closed, 0 = White, 1 = Black), editing a working
// copy of the side's AgentSpec that is only committed back to g_white/g_black on
// Apply. Most fields are edited directly (StepperRow/GuiDropdownBox writing straight
// into `working`'s own int fields); a handful need a scratch mirror because their
// AgentSpec type/unit doesn't match a raygui widget's int* (a node-budget
// unsigned long long, a dilution probability stored as 0..1 double, an opener
// index offset by "None"). Those are synced from `working` only when the popup
// (re)opens or a text/history id replaces `working` wholesale; every other frame
// the scratch fields are themselves authoritative and get folded back into
// `working` once per frame (PushEditorScratchIntoWorking), same one-way-while-open
// convention on both sides.
struct AgentEditorState {
    int       side = -1;
    AgentSpec working;
    char      idBuf[400] = "";
    bool      idEdit = false;
    string    idError;
    int       seededForEval = -1;   // evaluator whose defaults are loaded in working.evalParams

    // Dropdown edit-mode flags (single-open-at-a-time within the popup).
    bool editExplorer = false, editChooser = false, editEval = false, editOpener = false;

    // Scratch mirrors (see comment above).
    bool dilute = false;
    int  dilPct = 0;            // mirrors working.randomMoveProb * 100
    int  nodeBudgetInt = 0;      // mirrors working.nodeBudget
    int  timeBudgetInt = 0;      // mirrors working.timeBudgetMs
    int  openerSel = 0;          // 0 = None, else 1 + working.openerKind

    // History list + scroll-panel widget state.
    int     histScroll = 0;
    int     histActive = -1;
    Vector2 scroll = { 0, 0 };
};
static AgentEditorState g_ed;

static void SyncEditorScratchFromWorking() {
    const AgentSpec &s = g_ed.working;
    g_ed.dilute = s.randomMoveProb > 0.0;
    g_ed.dilPct = (int)(s.randomMoveProb * 100.0 + 0.5);
    g_ed.nodeBudgetInt = (int)s.nodeBudget;
    g_ed.timeBudgetInt = (int)s.timeBudgetMs;
    g_ed.openerSel = (s.openerKind >= 0 && s.openerKind < g_openerCount) ? s.openerKind + 1 : 0;
    g_ed.seededForEval = s.evaluator;   // parsed/loaded specs already carry valid params
}

static void PushEditorScratchIntoWorking() {
    AgentSpec &s = g_ed.working;
    s.randomMoveProb = g_ed.dilute ? (double)g_ed.dilPct / 100.0 : 0.0;
    s.nodeBudget      = (unsigned long long)(g_ed.nodeBudgetInt < 0 ? 0 : g_ed.nodeBudgetInt);
    s.timeBudgetMs    = (double)(g_ed.timeBudgetInt < 0 ? 0 : g_ed.timeBudgetInt);
    s.openerKind      = (g_ed.openerSel <= 0) ? -1 : g_ed.openerSel - 1;
}

#if !defined(PLATFORM_WEB)
// Parse `text` as a canonical agent id; on success adopt it as the working spec
// (and resync every scratch mirror from it), on failure record the parser's error
// for display. Shared by the id textbox (on blur) and the history list (on click).
// Native-only: needs rankAgentFromId (ranking.cpp).
static void TryApplyIdText(const string &text) {
    RankAgent out; string err;
    if (rankAgentFromId(text, out, err)) {
        g_ed.working = out.spec;
        g_ed.idError.clear();
        SyncEditorScratchFromWorking();
        string canon = rankAgentId(g_ed.working);
        std::strncpy(g_ed.idBuf, canon.c_str(), sizeof(g_ed.idBuf) - 1);
        g_ed.idBuf[sizeof(g_ed.idBuf) - 1] = '\0';
    } else {
        g_ed.idError = err;
    }
}
#endif

// Open the popup for `side`, seeding the working copy from that side's live spec.
static void OpenAgentEditor(int side) {
    PlayerConfig &c = (side == 0) ? g_white : g_black;
    if (c.isHuman) return;
    g_ed.side = side;
    g_ed.working = c.spec;
    g_ed.idError.clear();
    g_ed.idEdit = false;
    g_ed.editExplorer = g_ed.editChooser = g_ed.editEval = g_ed.editOpener = false;
    g_ed.histActive = -1;
    g_ed.histScroll = 0;
    g_ed.scroll = { 0, 0 };
    SyncEditorScratchFromWorking();
#if !defined(PLATFORM_WEB)
    string canon = rankAgentId(g_ed.working);
    std::strncpy(g_ed.idBuf, canon.c_str(), sizeof(g_ed.idBuf) - 1);
    g_ed.idBuf[sizeof(g_ed.idBuf) - 1] = '\0';
#endif
}

// Load whichever model slot(s) a spec's brain needs (LearnedValue's model, or
// LearnedPolicy's), one unified name for both platforms. Native reuses
// rankLoadAgentModels (src/ranking.h), which itself reuses rank.exe's exact
// slot-file convention. Web cannot link ranking.cpp, so it gets a small
// portable reimplementation of just that slot-file naming convention (mirrors
// the private slotFile() in src/ranking.cpp) plus a direct call to the
// otherwise-portable mlLoadSlot (src/ml_eval.h) -- forward-declared rather than
// included, for the same Model-collision reason GUI_MODEL_SLOTS is.
#if !defined(PLATFORM_WEB)
static bool GuiLoadAgentModels(const AgentSpec &spec, string &err) {
    return rankLoadAgentModels(spec, err);
}
#else
bool mlLoadSlot(int slot, const string &path);   // src/ml_eval.h; see comment above
static string GuiSlotFile(int slot) {
    if (slot == 0) return "models/lin_value.txt";
    if (slot == 1) return "models/lin_policy.txt";
    if (slot == 2) return "models/pst_value.txt";
    if (slot >= 3 && slot < GUI_MODEL_SLOTS) return "models/sweep/slot" + std::to_string(slot) + ".txt";
    return "";
}
static bool GuiLoadAgentModels(const AgentSpec &spec, string &err) {
    int slot = -1;
    if (spec.brain == BRAIN_POLICY && spec.chooser == ChooserIndexByName("LearnedPolicy")) slot = spec.modelSlot;
    if (spec.brain == BRAIN_SEARCH && spec.evaluator == learnedValueIndex())               slot = spec.modelSlot;
    if (slot < 0) return true;   // this agent uses no model slot
    string f = GuiSlotFile(slot);
    if (f.empty() || !mlLoadSlot(slot, f)) {
        err = "cannot load model " + f + " into slot " + std::to_string(slot);
        return false;
    }
    return true;
}
#endif

// ============================================================
// AI WORKER + RENDER VIEW
// ============================================================
// The AI search runs on a background thread (native only) so the window keeps
// redrawing and stays responsive while it thinks, instead of freezing inside
// agentChooseMove(). The search mutates the engine globals (board and the
// piece counts, via simulate/unsimulate) while it runs, so the renderer must not
// read those globals mid-search. Instead the draw code reads a snapshot -- the
// "view" -- that only the main thread writes (via SyncView), taken before the
// worker launches (pre-move position stays on screen) and refreshed after it
// finishes (post-move position revealed). The web build has no pthreads here, so
// it keeps the synchronous path (the worker runs inline and the window still
// stalls during the search).
static char g_viewBoard[SIZE][SIZE];
static int  g_viewWCount = 0;
static int  g_viewBCount = 0;
static char g_aiPrev[SIZE][SIZE];    // board before the in-flight AI move, for DiffMove

// Copy the live engine globals into the view the renderer reads. Main thread only.
static void SyncView() {
    std::memcpy(g_viewBoard, board, sizeof(g_viewBoard));
    g_viewWCount = g_whiteCount;
    g_viewBCount = g_blackCount;
}

#if !defined(PLATFORM_WEB)
static std::thread       g_aiThread;
static std::atomic<bool> g_aiDone{ false };   // worker -> main: search finished
static bool              g_aiRunning = false; // main-thread-only: a worker is live
#endif

// If an AI worker is in flight, block until it finishes and clear the flags.
// Used before any main-thread action that reloads or mutates the engine globals
// (New Game, program exit) so it cannot race the search.
static void JoinAiIfRunning() {
#if !defined(PLATFORM_WEB)
    if (g_aiRunning) {
        g_aiThread.join();
        g_aiRunning = false;
        g_aiDone.store(false, std::memory_order_release);
    }
#endif
}

// ============================================================
// PACING STATE
// ============================================================
static int    g_speedIndex = 2;          // 0=Step 1=0.25x 2=1x 3=4x 4=Instant
static double g_aiTimer = 0.0;
static bool   g_paused = false;
static bool   g_stepRequested = false;
static bool   g_delay2s = false;         // human vs fast AI: hold AI to >=2s/move

// Snapshot of settings captured at StartGame(). Used to detect mid-game changes
// so the panel can prompt the user to press New Game before they take effect.
// A side's canonical id is a faithful serialization of everything about it that
// affects play, so comparing isHuman + id is equivalent to (and simpler than)
// comparing every AgentSpec field individually.
struct SettingsSnapshot {
    bool   whiteIsHuman, blackIsHuman;
    string whiteId, blackId;
    char   boardFile[128];
};
static SettingsSnapshot g_snap;

// delay per speed index (seconds); index 0 (Step) handled separately, 4 = instant
static const double SPEED_DELAY[5] = { 0.0, 1.0, 0.25, 0.0625, 0.0 };
static const char  *SPEED_NAME[5]  = { "Step", "0.25x", "1x", "4x", "Instant" };

// Move log
static std::vector<std::string> g_log;
static Vector2 g_logScroll = { 0, 0 };

// Human selection
static bool g_hasSel = false;
static int  g_selX = 0, g_selY = 0;

// Misc UI
static char g_boardFile[128] = "boards/board1.txt";
static char g_status[160]    = "Configure players, then press Start.";

// raygui edit-mode flag for the Board file textbox (the only remaining textbox
// left in the main panel -- the type/opener/eval dropdowns moved into the agent
// editor popup, which keeps its own edit-mode flags in AgentEditorState).
static bool g_editBoardFile = false;

// Left overlay panel visibility (toggled by the Options button / Tab key).
static bool g_showPanel = true;

// Per-side board-evaluation readout (shown under the count badges). White-centric:
// a positive number favors White. "imm" is the immediate static eval of the
// position that side faced; "down" is a MiniMax side's predicted best-line eval.
struct EvalReadout { int imm = 0; int down = 0; bool hasImm = false; bool hasDown = false; };
static EvalReadout g_evalW, g_evalB;
static bool        g_showEval = true;   // toggled by the panel checkbox / E key

// Stepper-control prototypes. Each numeric parameter row can be drawn in one of
// several "slider + buttons" designs so the developer can compare them and pick a
// favorite. The style switcher (g_stepStyle) forces one design across all rows;
// 0 = per-row (each row uses its own assigned design).
// Each style is a genuinely different way to show the SAME bounded integer: all
// show both a bar and the number, and all step with a "+" (up) above a "-" (down).
//   BAR_NUM  - continuous fill bar with the number printed on it
//   SEGMENTS - discrete LED-style segment meter (click a cell to set)
//   NUMBAR   - a typeable number box with a slim proportional bar beneath it
//   HANDLE   - a thin track with a chip handle that carries the number
//   RULER    - a ticked ruler with a marker pointing at the value
enum StepStyle { STEP_BAR_NUM, STEP_SEGMENTS, STEP_NUMBAR, STEP_HANDLE,
                 STEP_RULER, STEP_STYLE_COUNT };
static int g_stepStyle = 0;

// Edit-mode flags for the typeable stepper styles (spinner / value box) inside the
// agent editor popup, keyed by side + a named field slot so AgentEditorState
// stays unchanged. Slots 7..7+MAX_EVAL_PARAMS-1 are the selected evaluator's
// weight rows (mirrors the old evaluator-params layout); everything else names
// one specific AgentSpec field. See DrawAgentEditor.
enum EdSlot {
    ED_DEPTH = 0, ED_ASPIRATION = 1, ED_NODES = 2, ED_TIME = 3, ED_DEPTHCAP = 4,
    ED_MODELSLOT = 5, ED_RISK = 6, ED_PARAMS_BASE = 7,             // params: [7, 7+MAX_EVAL_PARAMS)
    ED_DILPCT = 7 + MAX_EVAL_PARAMS, ED_DILDEPTH, ED_OPENERARG, ED_OPENERARG2,
    ED_SLOT_COUNT
};
static bool g_stepEdit[2][ED_SLOT_COUNT] = { { false } };

// ============================================================
// HELPERS -- SetStatus / CheckWinner / LogMove / DiffMove / AgentSummary
// ============================================================
static void SetStatus(const char *msg) {
    std::strncpy(g_status, msg, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
}

static void TakeSnapshot() {
    g_snap.whiteIsHuman = g_white.isHuman;
    g_snap.whiteId      = g_white.isHuman ? "" : g_white.id;
    g_snap.blackIsHuman = g_black.isHuman;
    g_snap.blackId      = g_black.isHuman ? "" : g_black.id;
    std::strncpy(g_snap.boardFile, g_boardFile, sizeof(g_snap.boardFile));
}

static bool SnapMatches() {
    return g_snap.whiteIsHuman == g_white.isHuman &&
           g_snap.blackIsHuman == g_black.isHuman &&
           g_snap.whiteId == (g_white.isHuman ? "" : g_white.id) &&
           g_snap.blackId == (g_black.isHuman ? "" : g_black.id) &&
           std::strcmp(g_snap.boardFile, g_boardFile) == 0;
}

// Robust win check, independent of engine return codes (openers can return a
// heuristic score, and playMove* does not maintain g_whiteAtEnd/g_blackAtEnd).
static int CheckWinner() {
    bool wEnd = false, bEnd = false;
    for (int x = 0; x < SIZE; x++) {
        if (board[x][SIZE - 1] == WHITE) wEnd = true;
        if (board[x][0] == BLACK)        bEnd = true;
    }
    if (wEnd || g_blackCount == 0) return White;
    if (bEnd || g_whiteCount == 0) return Black;
    return None;
}

static void LogMove(int color, int x1, int y1, int x2) {
    // Matches engine notation: srcColLetter, srcRow, destColLetter (e.g. "a0b").
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%2d. %c %c%d%c",
                  (int)g_log.size() + 1,
                  (color == White ? 'W' : 'B'),
                  (char)('a' + x1), y1, (char)('a' + x2));
    g_log.push_back(buf);
}

// Reconstruct an AI move by diffing the board snapshot taken before the move.
// Exactly one of the mover's squares is emptied (source) and one square gains
// the mover's color (dest). Returns false if no move was detected.
static bool DiffMove(const char prev[SIZE][SIZE], int color, int &x1, int &y1, int &x2) {
    char me = (color == White) ? WHITE : BLACK;
    int sx = -1, sy = -1, dx = -1, dy = -1;
    for (int x = 0; x < SIZE; x++) {
        for (int y = 0; y < SIZE; y++) {
            if (prev[x][y] == me && board[x][y] != me) { sx = x; sy = y; }
            if (prev[x][y] != me && board[x][y] == me) { dx = x; dy = y; }
        }
    }
    if (sx < 0 || dx < 0) return false;
    x1 = sx; y1 = sy; x2 = dx;
    (void)dy;
    return true;
}

// Short label for a side: "Human", or its canonical agent id (elided to fit a
// narrow field -- the full id is always available in the agent editor).
static string AgentSummary(const PlayerConfig &c) {
    if (c.isHuman) return "Human";
    if (c.id.size() <= 22) return c.id;
    return c.id.substr(0, 22) + "...";
}

// ============================================================
// GAME FLOW -- AfterMove / StartGame / Launch+FinalizeAIMove / HandleHumanClick
// ============================================================
// Shared half-move clock: 0 at game start, incremented once per applied move
// (human or AI). Mirrors src/ranking.cpp's playOneGame `h` counter exactly, so
// an agent's identity-level opener (AgentSpec::openerKind, the `.opener(rand,...)`
// /`.opener(book,...)` id segment) plays out here the same way it does in the
// ranked pool instead of being silently inert (agentChooseMove itself never reads
// openerKind -- only playOneGame/playoutCapture do, so the GUI must replicate
// that dispatch, see AiWorker below).
static int g_halfMove = 0;

// Advance to the next turn / state after a move has been applied.
static void AfterMove() {
    SyncView();  // the move (human or AI) is applied to the globals: refresh the view
    g_hasSel = false;
    g_halfMove++;
    g_winner = CheckWinner();
    if (g_winner != None) { g_state = AppState::GameOver; return; }

    g_turn = (g_turn == White) ? Black : White;
    bool isHuman = (g_turn == White) ? g_white.isHuman : g_black.isHuman;
    if (isHuman) {
        g_state = AppState::WaitingForHuman;
    } else {
        g_state = AppState::WaitingBeforeAI;
        g_aiTimer = 0.0;
    }
}

static void StartGame() {
    JoinAiIfRunning();  // never reload the board out from under a running search
    if (!reloadBoard(g_boardFile)) {
        SetStatus("Could not load board file. Check the path.");
        return;
    }
    g_log.clear();
    g_logScroll = { 0, 0 };
    g_hasSel = false;
    g_winner = None;
    g_paused = false;
    g_stepRequested = false;
    g_turn = White;
    g_halfMove = 0;
    g_aiTimer = 0.0;
    g_evalW = EvalReadout{};
    g_evalB = EvalReadout{};
    g_editBoardFile = false;

    SyncView();  // render the freshly loaded position

    g_state = g_white.isHuman ? AppState::WaitingForHuman : AppState::WaitingBeforeAI;
    TakeSnapshot();
    SetStatus("Game started. White to move.");
}

// Config the in-flight worker reads. Snapshotted from g_white/g_black at launch
// so the still-interactive options panel cannot mutate the mover's settings out
// from under the running search.
static PlayerConfig g_aiCfg;
static int          g_aiSide = White;
static int          g_aiHalfMove = 0;         // g_halfMove at launch (opener's ownPly/halfMove)
static bool         g_aiPlayedByOpener = false; // set by the worker, read after it joins

// The heavy search itself. Runs on the worker thread (native) or inline (web).
// Mirrors src/ranking.cpp's playOneGame dispatch: try the agent's identity-level
// opener first (if any), and only fall back to its brain (agentChooseMove) when
// the opener declines or there is none. Mutates the engine globals (board, counts,
// g_downEval*); the renderer stays on the pre-move view until FinalizeAIMove()
// syncs the result back.
static void AiWorker() {
    const AgentSpec &spec = g_aiCfg.spec;
    int victor = None;
    g_aiPlayedByOpener = false;
    if (spec.openerKind >= 0 && spec.openerKind < g_openerCount) {
        g_aiPlayedByOpener = g_openers[spec.openerKind].fn(
            g_aiSide, g_aiHalfMove / 2, g_aiHalfMove, spec.openerArg, spec.openerArg2, victor);
    }
    if (!g_aiPlayedByOpener) agentChooseMove(spec, g_aiSide);
#if !defined(PLATFORM_WEB)
    g_aiDone.store(true, std::memory_order_release);
#endif
}

// Kick off the AI move. Captures what the finalizer needs from the pre-move
// position (board snapshot for the move diff, immediate eval readout), freezes
// that position into the view, then launches the search: on a background thread
// natively, or inline on web.
static void LaunchAIMove() {
    std::memcpy(g_aiPrev, board, sizeof(g_aiPrev));

    PlayerConfig &cfg = (g_turn == White) ? g_white : g_black;
    EvalReadout  &ev  = (g_turn == White) ? g_evalW : g_evalB;
    bool hasEval = (cfg.spec.brain == BRAIN_SEARCH);

    // Immediate static eval must be read now, before the search touches the board.
    if (g_showEval) {
        ev.imm = immediateEvalForDisplay(hasEval, cfg.spec.evaluator, cfg.spec.evalParams);
        ev.hasImm = true;
    }

    SyncView();  // keep the pre-move position on screen while the search runs

    // Freeze the mover's side + config so the live panel can't race the worker.
    g_aiSide = g_turn;
    g_aiCfg  = cfg;
    g_aiHalfMove = g_halfMove;

#if !defined(PLATFORM_WEB)
    g_aiDone.store(false, std::memory_order_release);
    g_aiRunning = true;
    g_aiThread = std::thread(AiWorker);
#else
    AiWorker();  // synchronous: no pthreads on this web build
#endif
}

// Complete a finished AI move: record the downstream eval, log the move by
// diffing against the pre-move snapshot, reveal the new position, then advance
// the turn. Runs on the main thread after the worker has joined.
static void FinalizeAIMove() {
    EvalReadout &ev = (g_turn == White) ? g_evalW : g_evalB;
    // Reads g_aiCfg (the frozen snapshot the worker actually played from), not the
    // live g_white/g_black -- the panel stays interactive during the search, so by
    // the time the worker joins the live config may already differ.
    bool hasDown = g_showEval &&
                   g_aiCfg.spec.brain == BRAIN_SEARCH &&
                   IsAlphaBetaExplorer(g_aiCfg.spec.explorer) &&
                   !g_aiPlayedByOpener;   // an opener-played move never populated g_downEval*

    if (hasDown) {
        ev.down = (g_turn == White) ? g_downEvalWhite : g_downEvalBlack;
        ev.hasDown = true;
    } else {
        ev.hasDown = false;
    }

    int x1, y1, x2;
    if (DiffMove(g_aiPrev, g_turn, x1, y1, x2)) LogMove(g_turn, x1, y1, x2);

    AfterMove();  // refreshes the view to the post-move position and advances the turn
}

// Handle a board click while waiting for a human move.
static void HandleHumanClick(int col, int by) {
    char me = (g_turn == White) ? WHITE : BLACK;

    // Clicking own piece (re)selects the source.
    if (board[col][by] == me) {
        g_hasSel = true;
        g_selX = col;
        g_selY = by;
        return;
    }

    if (!g_hasSel) return;  // no source chosen yet

    // Destination must be exactly one forward row from the source.
    int fwd = (g_turn == White) ? g_selY + 1 : g_selY - 1;
    if (by != fwd) { SetStatus("Pieces move one row forward (diagonal or straight)."); return; }

    bool ok = (g_turn == White) ? tryMoveWhite(g_selX, g_selY, col, false)
                                : tryMoveBlack(g_selX, g_selY, col, false);
    if (!ok) { SetStatus("Illegal move."); return; }

    // Capture the immediate eval of the position the human faced (no downstream).
    if (g_showEval) {
        EvalReadout &ev = (g_turn == White) ? g_evalW : g_evalB;
        ev.imm = immediateEvalForDisplay(false, 0, nullptr);
        ev.hasImm = true;
        ev.hasDown = false;
    }

    if (g_turn == White) playMoveWhite(g_selX, g_selY, col);
    else                 playMoveBlack(g_selX, g_selY, col);
    LogMove(g_turn, g_selX, g_selY, col);
    AfterMove();
}

// Matchup classification drives the pacing controls: with a human in the game
// there is nothing to pace (slow AI paces itself; a fast AI can optionally be held
// to a 2s minimum), while AI vs AI gets the full speed/pause/step/restart set.
struct Matchup { int humans; bool aiVsAi; bool aiSlow; };
static Matchup ClassifyMatchup() {
    bool wH = g_white.isHuman, bH = g_black.isHuman;
    int humans = (wH ? 1 : 0) + (bH ? 1 : 0);
    const PlayerConfig &ai = wH ? g_black : g_white;   // an AI side (if any)
    Matchup mu;
    mu.humans = humans;
    mu.aiVsAi = (humans == 0);
    // Slow enough to self-pace: an alpha-beta search past depth 5. Greedy (1-ply,
    // ignores its budget) and policy brains are always fast regardless of depth.
    mu.aiSlow = !ai.isHuman && ai.spec.brain == BRAIN_SEARCH &&
                IsAlphaBetaExplorer(ai.spec.explorer) && ai.spec.depth > 5;
    return mu;
}

// ============================================================
// UPDATE -- per-frame state machine
// ============================================================
static void Update() {
    // Tab toggles the options overlay; E toggles the evaluation readouts.
    if (IsKeyPressed(KEY_TAB)) g_showPanel = !g_showPanel;
    if (IsKeyPressed(KEY_E))   g_showEval  = !g_showEval;

    // Board clicks (only meaningful while waiting for a human, not over a
    // textbox in edit mode, not while the agent editor popup is open, and not
    // over the panel overlay).
    Vector2 m = GetMousePosition();
    if (g_state == AppState::WaitingForHuman &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !g_editBoardFile && g_ed.side < 0 &&
        !(g_showPanel && CheckCollisionPointRec(m, g_panelRect))) {
        int sc = (int)((m.x - g_boardX) / g_cell);  // screen column
        int sr = (int)((m.y - g_boardY) / g_cell);  // screen row (0 = top)
        if (m.x >= g_boardX && m.y >= g_boardY && sc >= 0 && sc < SIZE && sr >= 0 && sr < SIZE) {
            int by = SIZE - 1 - sr;  // board row (y=SIZE-1 at top)
            HandleHumanClick(sc, by);
        }
    }

    // AI pacing.
    if (g_state == AppState::WaitingBeforeAI) {
        g_aiTimer += GetFrameTime();
        Matchup mu = ClassifyMatchup();
        bool go = false;
        if (mu.aiVsAi) {
            bool stepMode = (g_speedIndex == 0);
            if (g_stepRequested)                                                      go = true;
            else if (!g_paused && !stepMode && g_aiTimer >= SPEED_DELAY[g_speedIndex]) go = true;
        } else {
            // Human vs AI: no pause/step. Slow AI moves as soon as its search ends;
            // a fast AI is optionally held to a 2s minimum via the delay checkbox.
            double need = (!mu.aiSlow && g_delay2s) ? 2.0 : 0.0;
            if (g_aiTimer >= need) go = true;
        }
        if (go) {
            g_state = AppState::ComputingAI;
            g_stepRequested = false;
            LaunchAIMove();   // starts the search (background thread on native)
        }
    }

    // While computing, the search runs off the render thread (native), so we only
    // finalize once the worker signals done -- the window keeps redrawing the
    // pre-move view in the meantime. On web the worker already ran inline, so this
    // finalizes immediately.
    if (g_state == AppState::ComputingAI) {
#if !defined(PLATFORM_WEB)
        if (g_aiDone.load(std::memory_order_acquire)) {
            g_aiThread.join();
            g_aiRunning = false;
            FinalizeAIMove();
        }
#else
        FinalizeAIMove();
#endif
    }
}

// ============================================================
// LAYOUT COMPUTE -- ComputeLayout (recomputed each frame from window size)
// ============================================================
static void ComputeLayout() {
    int W = GetScreenWidth();
    int H = GetScreenHeight();

    // Reserve the panel strip on the left only while it is shown, so the board
    // sits beside the panel rather than under it. Reserve a strip on the right for
    // the piece-count badges so they never sit on top of the board.
    int leftReserve = g_showPanel ? PANEL_W : 0;
    int rightReserve = BADGE_STRIP;

    // Largest square board that fits in the remaining area, leaving label margins.
    int byW = (W - leftReserve - rightReserve - 2 * MARGIN) / SIZE;
    int byH = (H - TOP - 2 * MARGIN) / SIZE;
    g_cell = byW < byH ? byW : byH;
    if (g_cell < 8) g_cell = 8;            // never collapse to nothing
    g_boardPx = g_cell * SIZE;

    // Center the board within the area between the panel and the badge strip.
    g_boardX = leftReserve + (W - leftReserve - rightReserve - g_boardPx) / 2;
    g_boardY = TOP + (H - TOP - g_boardPx) / 2;

    g_panelRect = Rectangle{ 0, (float)TOP, (float)PANEL_W, (float)(H - TOP) };
}

// ============================================================
// BOARD RENDERING -- DrawPiece / DrawBoard
// ============================================================
static void DrawPiece(int cx, int cy, char who) {
    float r = g_cell * 0.36f;
    if (who == WHITE) {
        DrawCircle(cx, cy, r, COL_WPIECE);
        DrawCircleLines(cx, cy, r, COL_WEDGE);
    } else {
        DrawCircle(cx, cy, r, COL_BPIECE);
        DrawCircleLines(cx, cy, r, COL_BEDGE);
    }
}

static void DrawBoard() {
    // Squares + pieces.
    for (int sr = 0; sr < SIZE; sr++) {
        int by = SIZE - 1 - sr;
        for (int x = 0; x < SIZE; x++) {
            int px = g_boardX + x * g_cell;
            int py = g_boardY + sr * g_cell;
            Color sq = ((x + by) % 2 == 0) ? COL_DARK : COL_LIGHT;
            DrawRectangle(px, py, g_cell, g_cell, sq);

            char who = g_viewBoard[x][by];   // snapshot, safe while the AI worker runs
            if (who == WHITE || who == BLACK)
                DrawPiece(px + g_cell / 2, py + g_cell / 2, who);
        }
    }

    // Selection highlight + legal-destination hints.
    if (g_hasSel) {
        int selSr = SIZE - 1 - g_selY;
        DrawRectangleLinesEx(
            Rectangle{ (float)(g_boardX + g_selX * g_cell), (float)(g_boardY + selSr * g_cell),
                       (float)g_cell, (float)g_cell }, 4, COL_SEL);
        int fwd = (g_turn == White) ? g_selY + 1 : g_selY - 1;
        for (int dx = -1; dx <= 1; dx++) {
            int nx = g_selX + dx;
            if (nx < 0 || nx >= SIZE || fwd < 0 || fwd >= SIZE) continue;
            bool ok = (g_turn == White) ? tryMoveWhite(g_selX, g_selY, nx, false)
                                        : tryMoveBlack(g_selX, g_selY, nx, false);
            if (ok) {
                int hsr = SIZE - 1 - fwd;
                DrawCircle(g_boardX + nx * g_cell + g_cell / 2,
                           g_boardY + hsr * g_cell + g_cell / 2, g_cell * 0.16f, COL_HINT);
            }
        }
    }

    // Hover highlight while a human is to move (not over the open overlay).
    if (g_state == AppState::WaitingForHuman) {
        Vector2 m = GetMousePosition();
        bool overPanel = g_showPanel && CheckCollisionPointRec(m, g_panelRect);
        int sc = (int)((m.x - g_boardX) / g_cell);
        int sr = (int)((m.y - g_boardY) / g_cell);
        if (!overPanel && m.x >= g_boardX && m.y >= g_boardY &&
            sc >= 0 && sc < SIZE && sr >= 0 && sr < SIZE)
            DrawRectangle(g_boardX + sc * g_cell, g_boardY + sr * g_cell, g_cell, g_cell, COL_HOVER);
    }

    // Border + labels (font scales with cell size).
    DrawRectangleLinesEx(Rectangle{ (float)g_boardX, (float)g_boardY,
                                      (float)g_boardPx, (float)g_boardPx }, 2, COL_LABEL);
    int fs = g_cell / 4;
    if (fs < 12) fs = 12; else if (fs > 20) fs = 20;
    for (int x = 0; x < SIZE; x++) {
        const char *lbl = TextFormat("%c", 'a' + x);
        int tx = g_boardX + x * g_cell + g_cell / 2 - fs / 4;
        DrawText(lbl, tx, g_boardY - fs - 4, fs, COL_LABEL);
        DrawText(lbl, tx, g_boardY + g_boardPx + 4, fs, COL_LABEL);
    }
    for (int sr = 0; sr < SIZE; sr++) {
        int by = SIZE - 1 - sr;
        const char *lbl = TextFormat("%d", by);
        int ty = g_boardY + sr * g_cell + g_cell / 2 - fs / 2;
        DrawText(lbl, g_boardX - fs - 6, ty, fs, COL_LABEL);   // left only (right strip holds the badges)
    }
}

// ============================================================
// STEPPER WIDGETS -- DrawSpeedGlyph / DrawStackedPM / DrawFillBar / ScrubBar / StepperRow
// ============================================================
// Draw a transport glyph centered in r: forward = two right triangles
// (">>" fast-forward), otherwise a bar + one right triangle ("|>" slow motion).
// Triangle vertices are ordered (top-back, bottom-back, tip) so raylib renders
// them (it expects counter-clockwise winding in screen space).
static void DrawSpeedGlyph(Rectangle r, bool forward) {
    Color c = { 50, 52, 60, 255 };   // dark, to contrast the light raygui button
    float cy = r.y + r.height / 2.0f;
    float th = r.height * 0.28f;     // triangle half-height
    float tw = th;                   // triangle width
    if (forward) {
        float x0 = r.x + r.width / 2.0f - tw - 1;
        for (int i = 0; i < 2; i++) {
            float bx = x0 + i * (tw + 2);
            DrawTriangle(Vector2{ bx, cy - th }, Vector2{ bx, cy + th }, Vector2{ bx + tw, cy }, c);
        }
    } else {
        float bx = r.x + r.width / 2.0f - tw / 2.0f - 4;
        DrawRectangle((int)bx, (int)(cy - th), 3, (int)(2 * th), c);
        float tx = bx + 6;
        DrawTriangle(Vector2{ tx, cy - th }, Vector2{ tx, cy + th }, Vector2{ tx + tw, cy }, c);
    }
}

// "+" (up) stacked above "-" (down): the canonical increment/decrement for a
// number. Drawn at the right of every stepper design so they share one gesture.
static void DrawStackedPM(float bx, float y, float bw, float h, int *val, int lo, int hi) {
    float bh = (h - 3) / 2.0f;
    if (GuiButton(Rectangle{ bx, y, bw, bh }, "+")) { if (*val < hi) (*val)++; }
    if (GuiButton(Rectangle{ bx, y + bh + 3, bw, bh }, "-")) { if (*val > lo) (*val)--; }
}

// Track + proportional fill, with the number optionally centered on the bar.
static void DrawFillBar(Rectangle r, int val, int lo, int hi, bool showNum) {
    DrawRectangleRec(r, COL_TRK);
    float frac = (hi > lo) ? (float)(val - lo) / (float)(hi - lo) : 0.0f;
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    if (frac > 0) DrawRectangle((int)r.x, (int)r.y, (int)(r.width * frac), (int)r.height, COL_FILL);
    DrawRectangleLinesEx(r, 1, COL_BRD);
    if (showNum) {
        const char *s = TextFormat("%d", val);
        int tw = MeasureText(s, 16);
        DrawText(s, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - 16) / 2), 16, COL_NUM);
    }
}

// Click/drag anywhere on a bar to set its value by horizontal position. Ignored
// while the gui is locked (e.g. a dropdown list is open over the panel).
static void ScrubBar(Rectangle r, int *val, int lo, int hi) {
    if (GuiIsLocked()) return;
    Vector2 m = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, r)) {
        float frac = (m.x - r.x) / r.width;
        if (frac < 0) frac = 0; if (frac > 1) frac = 1;
        *val = lo + (int)(frac * (hi - lo) + 0.5f);
    }
}

// Modular numeric control: draws a labeled integer in one of several distinct
// bar+number designs, updates *val (clamped to [lo,hi]), and returns the next y.
// `barHi` caps the bar's proportional range (defaults to hi); values stepped or
// typed above it are preserved (e.g. Depth's bar tops out at 25 while the value
// can go higher). `side`/`param` index the per-row edit-mode flags.
static float StepperRow(int side, int param, StepStyle assigned, float x, float y,
                        float w, const char *name, int *val, int lo, int hi,
                        int barHi = -1) {
    if (barHi < 0) barHi = hi;
    StepStyle eff = (g_stepStyle == 0) ? assigned : (StepStyle)(g_stepStyle - 1);
    bool *edit = &g_stepEdit[side][param];

    const float labelW = 56;       // includes a gap so the name never touches the control
    GuiLabel(Rectangle{ x, y, labelW - 6, 20 }, name);
    float cx = x + labelW;
    float cw = w - labelW;
    const float bw = 18;        // stacked +/- column width
    float pmX = cx + cw - bw;   // x of the +/- column
    float ctrlW = cw - bw - 6;  // width left of the +/- column
    float rowH = 24;

    switch (eff) {
    case STEP_BAR_NUM: {                  // fill bar with the number on it
        Rectangle bar = { cx, y + 1, ctrlW, 20 };
        DrawFillBar(bar, *val, lo, barHi, true);
        ScrubBar(bar, val, lo, hi);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_SEGMENTS: {                 // discrete LED-style meter
        int n = hi - lo + 1; if (n < 1) n = 1; if (n > 40) n = 40;
        float gap = 2;
        float cwd = (ctrlW - gap * (n - 1)) / n; if (cwd < 2) cwd = 2;
        for (int i = 0; i < n; i++) {
            Rectangle seg = { cx + i * (cwd + gap), y + 2, cwd, 18 };
            DrawRectangleRec(seg, ((lo + i) <= *val) ? COL_FILL : COL_TRK);
            DrawRectangleLinesEx(seg, 1, COL_BRD);
            if (!GuiIsLocked() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(GetMousePosition(), seg))
                *val = lo + i;
        }
        DrawText(TextFormat("%d", *val), (int)(cx + 3), (int)(y + 4), 14, COL_NUM);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_NUMBAR: {                   // typeable number + slim underbar
        if (GuiValueBox(Rectangle{ cx, y, ctrlW, 18 }, NULL, val, lo, hi, *edit)) *edit = !*edit;
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        Rectangle bar = { cx, y + 21, ctrlW, 5 };
        DrawFillBar(bar, *val, lo, barHi, false);
        ScrubBar(bar, val, lo, hi);
        rowH = 30;
        break;
    }
    case STEP_HANDLE: {                   // thin track + chip handle with number
        Rectangle track = { cx, y + 9, ctrlW, 4 };
        DrawRectangleRec(track, COL_TRK);
        DrawRectangleLinesEx(track, 1, COL_BRD);
        float frac = (barHi > lo) ? (float)(*val - lo) / (float)(barHi - lo) : 0.0f;
        if (frac < 0) frac = 0; if (frac > 1) frac = 1;
        float chipW = 26;
        Rectangle chip = { cx + frac * (ctrlW - chipW), y + 2, chipW, 18 };
        DrawRectangleRec(chip, COL_FILL);
        DrawRectangleLinesEx(chip, 1, COL_BRD);
        const char *s = TextFormat("%d", *val);
        int tw = MeasureText(s, 14);
        DrawText(s, (int)(chip.x + (chipW - tw) / 2), (int)(chip.y + 2), 14, COL_NUM);
        ScrubBar(Rectangle{ cx, y, ctrlW, 22 }, val, lo, hi);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_RULER: {                    // ticked ruler + value marker
        float ry = y + 18;
        DrawLineEx(Vector2{ cx, ry }, Vector2{ cx + ctrlW, ry }, 2, COL_BRD);
        int n = hi - lo;
        if (n > 0) {
            for (int i = 0; i <= n; i++) {
                float tx = cx + ctrlW * (float)i / n;
                float th = (i % 5 == 0) ? 6.0f : 3.0f;
                DrawLineEx(Vector2{ tx, ry }, Vector2{ tx, ry - th }, 1, COL_BRD);
            }
            float mx = cx + ctrlW * (float)(*val - lo) / n;
            DrawTriangle(Vector2{ mx + 5, ry - 12 }, Vector2{ mx - 5, ry - 12 },
                         Vector2{ mx, ry - 3 }, COL_FILL);
            const char *s = TextFormat("%d", *val);
            int tw = MeasureText(s, 14);
            float lx = mx - tw / 2.0f;
            if (lx < cx) lx = cx; if (lx > cx + ctrlW - tw) lx = cx + ctrlW - tw;
            DrawText(s, (int)lx, (int)(y - 1), 14, COL_NUM);
        }
        ScrubBar(Rectangle{ cx, y + 6, ctrlW, 18 }, val, lo, hi);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        rowH = 28;
        break;
    }
    default: break;
    }

    if (*val < lo) *val = lo;
    if (*val > hi) *val = hi;
    return y + rowH + 8;
}

// ============================================================
// PLAYER CONFIG + PANEL -- DrawPlayerBlock / DrawPanel
// ============================================================
// One side's config block: a Human/Agent toggle, and -- when Agent -- a short
// summary of the current canonical id plus an "Edit Agent..." button opening the
// full editor (structured dropdowns/sliders + the id textbox live there; the main
// panel has no room for a canonical id, some of which run past 100 characters).
// `side` is 0 for White, 1 for Black.
static float DrawPlayerBlock(const char *title, int side, PlayerConfig &c,
                             float x, float y, float w) {
    GuiLabel(Rectangle{ x, y, w, 18 }, title);
    y += 20;

    int sel = c.isHuman ? 0 : 1;
    // GuiToggleGroup's bounds is the size of ONE item, not the whole group (each
    // subsequent item is placed at bounds.x += bounds.width + GROUP_PADDING) --
    // so the per-item width must be half the row, not the full row width.
    GuiToggleGroup(Rectangle{ x, y, w / 2.0f - 2, 24 }, "Human;Agent", &sel);
    c.isHuman = (sel == 0);
    y += 30;

    if (!c.isHuman) {
        GuiLabel(Rectangle{ x, y, w, 18 }, AgentSummary(c).c_str());
        y += 20;
        if (GuiButton(Rectangle{ x, y, w, 26 }, "Edit Agent...")) OpenAgentEditor(side);
        y += 32;
    }
    return y + 6;
}

static void DrawPanel() {
    // Opaque background (the board now sits beside the panel), plus a right edge.
    DrawRectangleRec(g_panelRect, Color{ 20, 22, 28, 255 });
    DrawLineEx(Vector2{ g_panelRect.width, g_panelRect.y },
               Vector2{ g_panelRect.width, g_panelRect.y + g_panelRect.height }, 2, COL_LABEL);

    float x = g_panelRect.x + 12;
    float w = (float)PANEL_W - 24;
    float y = (float)TOP + 12;

    // Slider-design switcher: "Per-row" shows each parameter in its own design,
    // any other choice forces that design on every row (applies to every StepperRow
    // drawn anywhere, including inside the agent editor popup). GuiComboBox cycles
    // in place, so it needs no overlay/lock handling.
    GuiLabel(Rectangle{ x, y, 52, 22 }, "Sliders");
    GuiComboBox(Rectangle{ x + 58, y, w - 58, 22 },
                "Per-row;Bar+number;Segments;Number+bar;Handle;Ruler", &g_stepStyle);
    y += 28;

    y = DrawPlayerBlock("WHITE player", 0, g_white, x, y, w);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 12;
    y = DrawPlayerBlock("BLACK player", 1, g_black, x, y, w);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 14;

    // Board file
    GuiLabel(Rectangle{ x, y, 52, 26 }, "Board");
    if (GuiTextBox(Rectangle{ x + 58, y, w - 58, 26 }, g_boardFile, sizeof(g_boardFile), g_editBoardFile))
        g_editBoardFile = !g_editBoardFile;
    y += 32;

    // Settings-changed notice (only while a game is running).
    bool liveGame = (g_state != AppState::Settings && g_state != AppState::GameOver);
    if (liveGame && !SnapMatches()) {
        GuiLabel(Rectangle{ x, y, w, 18 }, "Settings changed.");
        y += 20;
    }

    // Start / New Game
    if (GuiButton(Rectangle{ x, y, w, 30 },
                  g_state == AppState::Settings ? "Start Game" : "New Game")) {
        StartGame();
    }
    y += 38;

    // Pacing / game controls depend on the matchup. AI vs AI gets the full set
    // (speed, pause/resume, step, restart); a human vs a fast AI gets just an
    // optional 2s-per-move floor; a human vs a slow (self-pacing) AI or human vs
    // human needs no pacing controls at all.
    Matchup mu = ClassifyMatchup();
    if (mu.aiVsAi) {
        // Speed: slow-motion (|>) slower, fast-forward (>> double arrow) faster,
        // custom-drawn since raygui has no such glyphs. Preset name shown between.
        GuiLabel(Rectangle{ x, y, 52, 22 }, "Speed");
        float sbw = 38;
        Rectangle slowBtn = { x + 58, y, sbw, 22 };
        Rectangle fastBtn = { x + w - sbw, y, sbw, 22 };
        if (GuiButton(slowBtn, "")) { if (g_speedIndex > 0) g_speedIndex--; }
        DrawSpeedGlyph(slowBtn, false);
        if (GuiButton(fastBtn, "")) { if (g_speedIndex < 4) g_speedIndex++; }
        DrawSpeedGlyph(fastBtn, true);
        if (g_speedIndex < 0) g_speedIndex = 2;
        const char *sn = SPEED_NAME[g_speedIndex];
        int snw = MeasureText(sn, 16);
        float nameX = slowBtn.x + sbw + 4;
        float nameW = fastBtn.x - 4 - nameX;
        DrawText(sn, (int)(nameX + (nameW - snw) / 2), (int)(y + 3), 16, COL_LABEL);
        y += 28;

        // Transport row: play/pause toggle, step (next), restart, as icon buttons.
        // ("#131#" play, "#132#" pause, "#134#" next, "#211#" restart.)
        float bw3 = (w - 8) / 3.0f;
        GuiToggle(Rectangle{ x, y, bw3, 28 }, g_paused ? "#131#" : "#132#", &g_paused);
        if (GuiButton(Rectangle{ x + bw3 + 4, y, bw3, 28 }, "#134#")) g_stepRequested = true;
        if (GuiButton(Rectangle{ x + 2 * (bw3 + 4), y, bw3, 28 }, "#211#")) StartGame();
        y += 34;
    } else if (mu.humans == 1 && !mu.aiSlow) {
        GuiCheckBox(Rectangle{ x, y, 18, 18 }, "", &g_delay2s);
        GuiLabel(Rectangle{ x + 24, y, w - 24, 18 }, "Min 2s per AI move");
        y += 26;
    }

    // Evaluation readout toggle (also bound to the E key). Readouts appear under
    // the count badges; hide them for a hint-free PvP / PvC game.
    GuiCheckBox(Rectangle{ x, y, 18, 18 }, "", &g_showEval);
    GuiLabel(Rectangle{ x + 24, y, w - 24, 18 }, "Show evaluations (E)");
    y += 26;

    // Status line (piece counts are shown on the board itself).
    GuiLabel(Rectangle{ x, y, w, 20 }, TextFormat("%s to move", g_turn == White ? "White" : "Black"));
    y += 22;
    GuiLabel(Rectangle{ x, y, w, 20 }, g_status);
    y += 26;

    // Move log (scrolling), fills the remaining panel height.
    float logTop = y;
    float logH = (float)GetScreenHeight() - logTop - 12;
    if (logH < 60) logH = 60;
    float lineH = 18;
    Rectangle logBounds = { x, logTop, w, logH };
    Rectangle content = { 0, 0, w - 16, (float)g_log.size() * lineH + 8 };
    Rectangle view;
    GuiScrollPanel(logBounds, "Move Log", content, &g_logScroll, &view);
    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    for (size_t i = 0; i < g_log.size(); i++) {
        DrawText(g_log[i].c_str(),
                 (int)(logBounds.x + 6),
                 (int)(logBounds.y + g_logScroll.y + (float)i * lineH + 4),
                 14, COL_LABEL);
    }
    EndScissorMode();
}

// ============================================================
// AGENT EDITOR POPUP -- DrawAgentEditor
// ============================================================
// Semicolon-joined dropdown option strings built once from the pluggable-axis
// registries (explorers.h/choosers.h/ai_eval.h/ai_random.h) and cached, the same
// pattern the old evaluator dropdown used. "None" is prepended to the opener list
// so index 0 can mean "no opener" (AgentSpec::openerKind < 0).
static const string &OpenerOptions() {
    static string s;
    if (s.empty()) { s = "None"; for (int i = 0; i < g_openerCount; i++) { s += ";"; s += g_openers[i].name; } }
    return s;
}
static const string &ExplorerOptions() {
    static string s;
    if (s.empty()) for (int i = 0; i < g_explorerCount; i++) { if (i) s += ";"; s += g_explorers[i].name; }
    return s;
}
static const string &ChooserOptions() {
    static string s;
    if (s.empty()) for (int i = 0; i < g_chooserCount; i++) { if (i) s += ";"; s += g_choosers[i].name; }
    return s;
}
static const string &EvalOptions() {
    static string s;
    if (s.empty()) for (int i = 0; i < g_evalCount; i++) { if (i) s += ";"; s += g_evaluators[i].name; }
    return s;
}

// The full agent editor: id textbox + validation, recent-history list, fixed
// dropdown row(s), and a scrollable body of StepperRow/checkbox fields covering
// every axis the canonical-ID grammar exposes (src/ranking.h). No-op when closed.
static void DrawAgentEditor() {
    if (g_ed.side < 0) return;
    PlayerConfig &c = (g_ed.side == 0) ? g_white : g_black;

    float pw = 620, ph = 600;
    if (pw > (float)GetScreenWidth() - 20)  pw = (float)GetScreenWidth() - 20;
    if (ph > (float)GetScreenHeight() - 20) ph = (float)GetScreenHeight() - 20;
    Rectangle win = { (GetScreenWidth() - pw) / 2.0f, (GetScreenHeight() - ph) / 2.0f, pw, ph };

    // Dim the rest of the screen so the popup reads as modal.
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 120 });

    const char *title = (g_ed.side == 0) ? "Edit Agent -- White" : "Edit Agent -- Black";
    if (GuiWindowBox(win, title)) { g_ed.side = -1; return; }

    float x = win.x + 12, y = win.y + 36, w = win.width - 24;

#if !defined(PLATFORM_WEB)
    // ---- Canonical id textbox ---- (native-only: needs the ranking.cpp codec)
    bool wasEditing = g_ed.idEdit;
    if (GuiTextBox(Rectangle{ x, y, w, 26 }, g_ed.idBuf, sizeof(g_ed.idBuf), g_ed.idEdit))
        g_ed.idEdit = !g_ed.idEdit;
    if (wasEditing && !g_ed.idEdit) TryApplyIdText(g_ed.idBuf);   // finished editing: validate
    y += 30;
    if (!g_ed.idError.empty()) {
        DrawText(g_ed.idError.c_str(), (int)x, (int)y, 12, Color{ 240, 110, 110, 255 });
        y += 18;
    }
    y += 4;

    // ---- Recent history ---- (native-only, same reason)
    GuiLabel(Rectangle{ x, y, w, 18 }, "Recent:");
    y += 20;
    {
        string list;
        for (size_t i = 0; i < g_agentHistory.size(); i++) {
            if (i) list += ";";
            string s = g_agentHistory[i];
            if (s.size() > 70) s = s.substr(0, 68) + "...";
            list += s;
        }
        int prevActive = g_ed.histActive;
        GuiListView(Rectangle{ x, y, w, 70 }, list.empty() ? NULL : list.c_str(),
                    &g_ed.histScroll, &g_ed.histActive);
        if (g_ed.histActive != prevActive && g_ed.histActive >= 0 &&
            g_ed.histActive < (int)g_agentHistory.size()) {
            std::strncpy(g_ed.idBuf, g_agentHistory[g_ed.histActive].c_str(), sizeof(g_ed.idBuf) - 1);
            g_ed.idBuf[sizeof(g_ed.idBuf) - 1] = '\0';
            g_ed.idEdit = false;
            TryApplyIdText(g_ed.idBuf);
        }
        y += 78;
    }
#endif

    // ---- Fixed row A: Brain + Opener ----
    // GuiToggleGroup's bounds is the size of ONE item (see DrawPlayerBlock), so a
    // 78px-wide item gives a ~160px total span for the 2-item Search/Policy group.
    GuiLabel(Rectangle{ x, y, 42, 24 }, "Brain");
    int brainSel = g_ed.working.brain;
    GuiToggleGroup(Rectangle{ x + 44, y, 78, 24 }, "Search;Policy", &brainSel);
    g_ed.working.brain = brainSel;

    GuiLabel(Rectangle{ x + 224, y, 54, 24 }, "Opener");
    Rectangle openerRect = { x + 280, y, w - 280, 24 };
    y += 30;

    // ---- Fixed row B: Explorer+Evaluator (Search) or Chooser (Policy) ----
    Rectangle explorerRect{ 0, 0, 0, 0 }, evalRect{ 0, 0, 0, 0 }, chooserRect{ 0, 0, 0, 0 };
    if (g_ed.working.brain == BRAIN_SEARCH) {
        GuiLabel(Rectangle{ x, y, 52, 24 }, "Search");
        explorerRect = { x + 56, y, 140, 24 };
        GuiLabel(Rectangle{ x + 206, y, 40, 24 }, "Eval");
        evalRect = { x + 248, y, w - 248, 24 };
    } else {
        GuiLabel(Rectangle{ x, y, 52, 24 }, "Policy");
        chooserRect = { x + 56, y, w - 56, 24 };
    }
    y += 34;

    // Deferred: draw dropdowns last, on top, single-open-at-a-time (same convention
    // the old panel used for its type/opener/eval dropdowns).
    struct EdDrop { Rectangle r; const char *opts; int *val; bool *edit; };
    std::vector<EdDrop> drops;
    drops.push_back(EdDrop{ openerRect, OpenerOptions().c_str(), &g_ed.openerSel, &g_ed.editOpener });
    if (g_ed.working.brain == BRAIN_SEARCH) {
        drops.push_back(EdDrop{ explorerRect, ExplorerOptions().c_str(), &g_ed.working.explorer, &g_ed.editExplorer });
        drops.push_back(EdDrop{ evalRect, EvalOptions().c_str(), &g_ed.working.evaluator, &g_ed.editEval });
    } else {
        drops.push_back(EdDrop{ chooserRect, ChooserOptions().c_str(), &g_ed.working.chooser, &g_ed.editChooser });
    }
    bool anyDropOpen = g_ed.editOpener || g_ed.editExplorer || g_ed.editEval || g_ed.editChooser;
    if (anyDropOpen) GuiLock();
    int openDropIdx = -1;
    for (size_t i = 0; i < drops.size(); i++) if (*drops[i].edit) { openDropIdx = (int)i; break; }
    for (size_t i = 0; i < drops.size(); i++) {
        if ((int)i == openDropIdx) continue;
        if (GuiDropdownBox(drops[i].r, drops[i].opts, drops[i].val, *drops[i].edit)) {
            for (size_t j = 0; j < drops.size(); j++) *drops[j].edit = false;
            *drops[i].edit = true;
        }
    }
    if (anyDropOpen) GuiUnlock();
    if (openDropIdx >= 0) {
        if (GuiDropdownBox(drops[openDropIdx].r, drops[openDropIdx].opts,
                            drops[openDropIdx].val, *drops[openDropIdx].edit))
            *drops[openDropIdx].edit = false;
    }
    if (g_ed.working.explorer < 0 || g_ed.working.explorer >= g_explorerCount) g_ed.working.explorer = 0;
    if (g_ed.working.chooser  < 0 || g_ed.working.chooser  >= g_chooserCount)  g_ed.working.chooser  = 0;
    if (g_ed.working.evaluator < 0 || g_ed.working.evaluator >= g_evalCount)   g_ed.working.evaluator = 0;

    // ---- Scrollable body: every numeric/checkbox field ----
    Rectangle bodyBounds = { x, y, w, (win.y + win.height - 54) - y };
    Rectangle bodyContent = { 0, 0, w - 16, 950 };   // generous fixed virtual height (see gui/CLAUDE.md)
    Rectangle bodyView;
    GuiScrollPanel(bodyBounds, NULL, bodyContent, &g_ed.scroll, &bodyView);
    BeginScissorMode((int)bodyView.x, (int)bodyView.y, (int)bodyView.width, (int)bodyView.height);
    float cx = bodyBounds.x + 4, cy = bodyBounds.y + g_ed.scroll.y + 4, cw = bodyBounds.width - 20;

    if (g_ed.working.brain == BRAIN_SEARCH) {
        bool isAB = IsAlphaBetaExplorer(g_ed.working.explorer);
        cy = StepperRow(g_ed.side, ED_DEPTH, STEP_NUMBAR, cx, cy, cw, "Depth", &g_ed.working.depth, 1, 1000000, 25);

        if (isAB) {
            GuiLabel(Rectangle{ cx, cy, 46, 20 }, "Flags");
            float ckx = cx + 50, ckw = (cw - 50) / 5.0f;
            const char *ckNames[5] = { "noAB", "TT", "ord", "QS", "part" };
            bool noAB = !g_ed.working.useAlphaBeta;
            bool *ckVals[5] = { &noAB, &g_ed.working.useTT, &g_ed.working.useMoveOrder,
                                 &g_ed.working.useQuiescence, &g_ed.working.keepPartial };
            for (int i = 0; i < 5; i++) {
                GuiCheckBox(Rectangle{ ckx + i * ckw, cy + 2, 16, 16 }, "", ckVals[i]);
                DrawText(ckNames[i], (int)(ckx + i * ckw + 20), (int)(cy + 4), 12, COL_LABEL);
            }
            g_ed.working.useAlphaBeta = !noAB;
            cy += 26;

            cy = StepperRow(g_ed.side, ED_ASPIRATION, STEP_BAR_NUM, cx, cy, cw, "Aspir", &g_ed.working.aspirationWindow, 0, 500);
            cy = StepperRow(g_ed.side, ED_NODES, STEP_NUMBAR, cx, cy, cw, "Nodes", &g_ed.nodeBudgetInt, 0, 5000000, 500000);
            cy = StepperRow(g_ed.side, ED_TIME, STEP_NUMBAR, cx, cy, cw, "TimeMs", &g_ed.timeBudgetInt, 0, 600000, 60000);
            cy = StepperRow(g_ed.side, ED_DEPTHCAP, STEP_BAR_NUM, cx, cy, cw, "MaxCap", &g_ed.working.depthCap, 0, 100);
        }

        // Evaluator selection + its weights, reseeding to the registry defaults
        // whenever the selected evaluator changes (mirrors the old SeedEvalParams).
        if (g_ed.working.evaluator != g_ed.seededForEval) {
            const EvalDef &e0 = g_evaluators[g_ed.working.evaluator];
            for (int i = 0; i < e0.paramCount; i++) g_ed.working.evalParams[i] = e0.params[i].def;
            g_ed.seededForEval = g_ed.working.evaluator;
        }
        if (g_ed.working.evaluator == learnedValueIndex()) {
            cy = StepperRow(g_ed.side, ED_MODELSLOT, STEP_NUMBAR, cx, cy, cw, "Slot", &g_ed.working.modelSlot, 0, GUI_MODEL_SLOTS - 1, 200);
            cy = StepperRow(g_ed.side, ED_RISK, STEP_BAR_NUM, cx, cy, cw, "Risk", &g_ed.working.evalParams[1], -50, 50);
        } else {
            const EvalDef &e = g_evaluators[g_ed.working.evaluator];
            for (int i = 0; i < e.paramCount; i++) {
                StepStyle st = (StepStyle)(i % STEP_STYLE_COUNT);
                cy = StepperRow(g_ed.side, ED_PARAMS_BASE + i, st, cx, cy, cw, e.params[i].name,
                                &g_ed.working.evalParams[i], e.params[i].lo, e.params[i].hi);
            }
        }
    } else {
        int smartIdx = ChooserIndexByName("SmartRandom");
        int policyIdx = ChooserIndexByName("LearnedPolicy");
        if (g_ed.working.chooser == smartIdx) {
            cy = StepperRow(g_ed.side, ED_MODELSLOT, STEP_SEGMENTS, cx, cy, cw, "Forward", &g_ed.working.chooserParam, 1, 16);
        } else if (g_ed.working.chooser == policyIdx) {
            cy = StepperRow(g_ed.side, ED_MODELSLOT, STEP_NUMBAR, cx, cy, cw, "Slot", &g_ed.working.modelSlot, 0, GUI_MODEL_SLOTS - 1, 200);
        }
    }

    // Dilution
    GuiCheckBox(Rectangle{ cx, cy, 16, 16 }, "", &g_ed.dilute);
    DrawText("Dilute", (int)(cx + 22), (int)(cy + 2), 14, COL_LABEL);
    cy += 24;
    if (g_ed.dilute) {
        cy = StepperRow(g_ed.side, ED_DILPCT, STEP_BAR_NUM, cx, cy, cw, "Prob%", &g_ed.dilPct, 1, 100);
        if (g_ed.working.brain == BRAIN_SEARCH)
            cy = StepperRow(g_ed.side, ED_DILDEPTH, STEP_BAR_NUM, cx, cy, cw, "DilDep", &g_ed.working.dilDepth, 0, 25);
    }

    // Opener args (label taken from the opener registry, see src/ai_random.h)
    if (g_ed.openerSel > 0) {
        int oi = g_ed.openerSel - 1;
        if (g_openers[oi].hasArg)
            cy = StepperRow(g_ed.side, ED_OPENERARG, STEP_BAR_NUM, cx, cy, cw, g_openers[oi].argLabel, &g_ed.working.openerArg, 0, 200);
        if (g_openers[oi].hasArg2)
            cy = StepperRow(g_ed.side, ED_OPENERARG2, STEP_BAR_NUM, cx, cy, cw, "ply", &g_ed.working.openerArg2, 0, 400);
    }
    (void)cy;
    EndScissorMode();

    // Fold scratch fields into `working` and, on native, refresh the id preview
    // (unless the textbox currently has edit focus -- don't stomp on in-progress
    // typing). Web has no id textbox to refresh.
    PushEditorScratchIntoWorking();
#if !defined(PLATFORM_WEB)
    if (!g_ed.idEdit) {
        string canon = rankAgentId(g_ed.working);
        std::strncpy(g_ed.idBuf, canon.c_str(), sizeof(g_ed.idBuf) - 1);
        g_ed.idBuf[sizeof(g_ed.idBuf) - 1] = '\0';
    }
#endif

    // ---- Apply / Cancel ----
    // Native re-validates the textbox's current text one more time (it is the
    // single source of truth, kept in sync with the structured fields above) and
    // adopts it; web has no text form, so the structured `working` spec commits
    // directly.
    float footY = win.y + win.height - 44;
    float bw2 = (w - 8) / 2.0f;
    if (GuiButton(Rectangle{ x, footY, bw2, 32 }, "Apply")) {
#if !defined(PLATFORM_WEB)
        RankAgent out; string err;
        if (rankAgentFromId(g_ed.idBuf, out, err)) {
            c.spec = out.spec;
            c.id = rankAgentId(c.spec);
            string loadErr;
            if (!GuiLoadAgentModels(c.spec, loadErr)) SetStatus(("Model load failed: " + loadErr).c_str());
            else                                       SetStatus(("Agent set: " + c.id).c_str());
            RememberAgentId(c.id);
            g_ed.idError.clear();
            g_ed.side = -1;
        } else {
            g_ed.idError = err;
        }
#else
        c.spec = g_ed.working;
        c.id = agentDescribe(c.spec);
        string loadErr;
        if (!GuiLoadAgentModels(c.spec, loadErr)) SetStatus(("Model load failed: " + loadErr).c_str());
        else                                       SetStatus(("Agent set: " + c.id).c_str());
        g_ed.side = -1;
#endif
    }
    if (GuiButton(Rectangle{ x + bw2 + 8, footY, bw2, 32 }, "Cancel")) {
        g_ed.side = -1;
    }
}

// ============================================================
// COUNT BADGES + EVAL -- DrawCountBadge / FormatEval / DrawEvalReadout / DrawPieceCounts
// ============================================================
// Emblematic piece-count badges drawn on the board itself (so they stay visible
// when the panel is hidden): a small piece icon + count, white near White's side
// (top of the board) and black near Black's side (bottom).
static void DrawCountBadge(int bx, int by, char who, int count) {
    const int bwd = 52, bht = 26;
    // Mid-gray pill so both a light and a dark piece icon read clearly on it.
    DrawRectangleRounded(Rectangle{ (float)bx, (float)by, (float)bwd, (float)bht }, 0.5f, 8,
                         Color{ 58, 62, 74, 235 });
    int cyc = by + bht / 2, cxc = bx + 15;
    float r = 8;
    if (who == WHITE) { DrawCircle(cxc, cyc, r, COL_WPIECE); DrawCircleLines(cxc, cyc, r, COL_WEDGE); }
    else              { DrawCircle(cxc, cyc, r, COL_BPIECE); DrawCircleLines(cxc, cyc, r, COL_BEDGE); }
    DrawText(TextFormat("%d", count), bx + 28, by + 5, 16, COL_NUM);
}

// Format a white-centric eval for display: forced wins as +WIN / -WIN, else the
// signed number. Writes into the caller's buffer and returns it.
static const char *FormatEval(int v, char *buf, int n) {
    if (v >= WhiteWin - 1024)      std::snprintf(buf, n, "+WIN");
    else if (v <= BlackWin + 1024) std::snprintf(buf, n, "-WIN");
    else                           std::snprintf(buf, n, "%+d", v);
    return buf;
}

// Draw a side's eval readout (one or two short lines) starting at (bx, by).
// "now" is the immediate static eval; "pred" the MiniMax best-line eval.
static void DrawEvalReadout(int bx, int by, const EvalReadout &ev) {
    if (!ev.hasImm) return;
    char buf[16];
    DrawText(TextFormat("now %s", FormatEval(ev.imm, buf, sizeof(buf))), bx, by, 12, COL_LABEL);
    if (ev.hasDown)
        DrawText(TextFormat("pred %s", FormatEval(ev.down, buf, sizeof(buf))), bx, by + 14, 12, COL_LABEL);
}

static void DrawPieceCounts() {
    if (g_boardPx <= 0) return;
    // In the reserved strip just to the right of the board, so nothing overlaps the
    // squares. Orientation: White starts on the low rows and moves up (its pieces
    // sit at the bottom of the screen); Black sits at the top.
    int bx = g_boardX + g_boardPx + 8;
    DrawCountBadge(bx, g_boardY,                  BLACK, g_viewBCount);  // Black's side (top)
    DrawCountBadge(bx, g_boardY + g_boardPx - 26, WHITE, g_viewWCount);  // White's side (bottom)

    // Eval readouts under each badge (Black just below the top badge, White above
    // the bottom badge so the two never collide on a short board).
    if (g_showEval) {
        DrawEvalReadout(bx, g_boardY + 30, g_evalB);
        int wy = g_boardY + g_boardPx - 26 - (g_evalW.hasDown ? 32 : 18);
        if (wy > g_boardY + 60) DrawEvalReadout(bx, wy, g_evalW);
    }
}

// ============================================================
// GAME OVER -- DrawGameOverBanner
// ============================================================
static void DrawGameOverBanner() {
    if (g_state != AppState::GameOver) return;
    const char *who = (g_winner == White) ? "WHITE WINS" : "BLACK WINS";
    int fs = 40;
    int tw = MeasureText(who, fs);
    int bx = g_boardX + (g_boardPx - tw) / 2 - 20;
    int by = g_boardY + g_boardPx / 2 - 34;
    DrawRectangle(bx, by, tw + 40, 68, Color{ 0, 0, 0, 190 });
    DrawText(who, bx + 20, by + 14, fs, Color{ 255, 220, 90, 255 });
}

// ============================================================
// MAIN LOOP -- UpdateDrawFrame / main
// ============================================================
static void UpdateDrawFrame() {
    ComputeLayout();
    Update();

    BeginDrawing();
    ClearBackground(COL_BG);

    DrawBoard();
    DrawPieceCounts();      // emblematic counts on the board (visible with panel hidden)
    if (g_showPanel) {
        bool editorOpen = (g_ed.side >= 0);
        if (editorOpen) GuiLock();   // the popup is modal: panel widgets underneath stay inert
        DrawPanel();
        if (editorOpen) GuiUnlock();
    }
    DrawAgentEditor();      // no-op when closed; dim overlay + popup on top when open
    DrawGameOverBanner();   // on top so the win banner stays readable

    // Top bar: Options/Hide toggle + title (above the overlay, always visible).
    if (GuiButton(Rectangle{ 8, 8, 96, 28 }, g_showPanel ? "Hide" : "Options"))
        g_showPanel = !g_showPanel;
    DrawText("Breakthrough", 116, 11, 22, COL_LABEL);
    DrawText(TextFormat("%s  vs  %s", AgentSummary(g_white).c_str(), AgentSummary(g_black).c_str()),
             320, 15, 16, COL_LABEL);

    EndDrawing();
}

int main() {
    std::srand((unsigned)time(0));
    PRNT = 0;  // silence engine console output
    mlAutoLoadDefaultSlots();  // make LearnedValue usable if a trained model exists
#if !defined(PLATFORM_WEB)
    LoadAgentHistory();        // restore the validated, disk-persisted id history (native-only)
#endif

    // Default matchup: Human (White) vs the historical MiniMax-depth-8-Classic
    // default, now spelled as an agent.
    g_white.isHuman = true;
    g_black = MakeDefaultBlackAgent();
    { string err; GuiLoadAgentModels(g_black.spec, err); }   // no-op for Classic (no model slot)

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INIT_W, INIT_H, "Breakthrough");
    SetWindowMinSize(MIN_W, MIN_H);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    // Start the game immediately. If the board file is missing, StartGame() sets an
    // error status and leaves the state at Settings so the user can fix the path.
    // The empty-board fallback below only runs when StartGame() itself couldn't load.
    if (!reloadBoard(g_boardFile)) {
        for (int x = 0; x < SIZE; x++)
            for (int yy = 0; yy < SIZE; yy++) board[x][yy] = EMPTY;
    }
    StartGame();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) UpdateDrawFrame();
    JoinAiIfRunning();  // don't tear down while a search thread is still live
#endif

    CloseWindow();
    return 0;
}
