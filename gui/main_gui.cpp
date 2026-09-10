// main_gui.cpp - Raylib + raygui front end for Breakthrough.
//
// A GUI layer over the engine that never touches an engine global itself. The
// game position lives here as a GuiPos (gui/gui_engine.h), human moves are
// checked and applied with the pure guiIsLegal/guiApplyMove helpers, and every
// engine computation (an agent's move, the live analysis behind the arrows and
// the eval bar) is a job for the engine service in gui/gui_engine.cpp, which
// runs it on its own thread (native) or in bounded slices per frame (web). The
// window therefore keeps drawing and responding while any agent thinks or any
// analysis runs.
//
// A side is Human or an agent: any AgentSpec the canonical-ID grammar
// (src/ranking.h) expresses, chosen from the agent library (gui/gui_library.h:
// champions, presets, standings, favorites, recent) or built in the agent editor.
//
// Two panel modes share one board: the full panel (native default: Play /
// Analysis / View tabs) and simple mode (the web build's only mode: play White or
// Black against an Easy / Medium / Hard preset, or watch two presets play).
//
// Native builds also accept --capture <png> to render a few seconds into a hidden
// window and save a screenshot, without ever showing a window (see ParseArgs).
//
// Sections (grep "// ==="):
//   LAYOUT / THEMES          window constants, piece and board color themes
//   APP STATE                players, position + history, pacing, analysis/view settings
//   HELPERS                  text measuring/wrapping, status, names, settings persistence
//   PLAYERS                  SetPlayerFromId, presets, simple-mode matchups
//   GAME FLOW                StartGame / ApplyMove / Undo / AI move request + finalize
//   ANALYSIS                 keeping the engine's analysis pointed at the current position
//   UPDATE                   per-frame input and state machine
//   LAYOUT COMPUTE + MAPPING board geometry, square <-> screen (with flip)
//   BOARD RENDERING          squares, pieces, last move, hints, arrows, eval bar, badges
//   WIDGETS                  deferred dropdowns, steppers (bar+number designs)
//   PANEL: PLAY / ANALYSIS / VIEW / SIMPLE
//   AGENT EDITOR             structured editor + canonical id box
//   LIBRARY                  champions / presets / standings / favorites / recent
//   MODEL PICKER             model slot catalog
//   MAIN LOOP                UpdateDrawFrame, capture mode, main

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// raylib.h defines WHITE/BLACK as Color macros, which collide with globals.h's
// board macros (#define WHITE 'W', BLACK 'B'). We need the board macros, so drop
// the raylib color macros and use explicit Color literals for drawing instead.
#undef WHITE
#undef BLACK

#include "globals.h"
#include "ai_eval.h"    // g_evaluators / g_evalCount
#include "agents.h"     // AgentSpec, agentMakeSearch, learnedValueIndex
#include "explorers.h"  // g_explorers
#include "choosers.h"   // g_choosers
#include "ai_random.h"  // g_openers
#include "ranking.h"    // rankAgentId / rankAgentFromId (portable, links on web too)
#include "gui_engine.h"
#include "gui_library.h"
// Not including ml_eval.h: its class Model collides with raylib's struct Model.
// gui_engine.cpp (no raylib) owns every model-slot operation instead.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// ============================================================
// LAYOUT / THEMES
// ============================================================
static const int INIT_W = 1280;
static const int INIT_H = 820;
static const int MIN_W  = 900;
static const int MIN_H  = 640;

static const int TOP         = 44;    // top bar height
static const int MARGIN      = 30;    // around the board, for coordinates
static const int PANEL_W     = 300;   // left panel width
static const int RIGHT_STRIP = 136;   // badges + per-side readouts
static const int EVALBAR_W   = 18;

static int       g_cell = 64, g_boardX = 0, g_boardY = 0, g_boardPx = 0;
static Rectangle g_panelRect = { 0, 0, 0, 0 };
static Rectangle g_evalBarRect = { 0, 0, 0, 0 };

static const Color COL_BG      = {  34,  36,  44, 255 };
static const Color COL_PANEL   = {  20,  22,  28, 255 };
static const Color COL_LABEL   = { 200, 200, 210, 255 };
static const Color COL_DIM     = { 130, 134, 148, 255 };
static const Color COL_ACCENT  = { 250, 210,  70, 255 };
static const Color COL_ERR     = { 240, 110, 110, 255 };
static const Color COL_SEL     = { 250, 210,  70, 200 };
static const Color COL_HINT    = {  70, 200, 120, 170 };
static const Color COL_HOVER   = { 255, 255, 255,  45 };
static const Color COL_LAST    = { 250, 220,  90,  80 };
static const Color COL_TRK     = {  46,  49,  60, 255 };
static const Color COL_FILL    = {  86, 158, 222, 255 };
static const Color COL_BRD     = {  92,  96, 110, 255 };
static const Color COL_NUM     = { 236, 239, 246, 255 };

struct PieceTheme { const char *name; Color wFill, wEdge, bFill, bEdge; };
static const PieceTheme PIECE_THEMES[] = {
    { "Classic",       { 245, 242, 232, 255 }, { 120, 120, 120, 255 }, {  32,  34,  40, 255 }, { 200, 200, 200, 255 } },
    { "Red / Blue",    { 222,  70,  62, 255 }, { 120,  26,  22, 255 }, {  58, 110, 222, 255 }, {  20,  44, 118, 255 } },
    { "Blue / Red",    {  58, 110, 222, 255 }, {  20,  44, 118, 255 }, { 222,  70,  62, 255 }, { 120,  26,  22, 255 } },
    { "Gold / Purple", { 238, 196,  74, 255 }, { 130,  96,  18, 255 }, { 124,  72, 186, 255 }, {  52,  24,  96, 255 } },
};
static const int PIECE_THEME_COUNT = (int)(sizeof(PIECE_THEMES) / sizeof(PIECE_THEMES[0]));

struct BoardTheme { const char *name; Color light, dark; };
static const BoardTheme BOARD_THEMES[] = {
    { "Wood",  { 222, 210, 180, 255 }, { 140, 110,  78, 255 } },
    { "Slate", { 200, 206, 214, 255 }, { 112, 122, 140, 255 } },
    { "Green", { 236, 237, 210, 255 }, { 118, 150,  86, 255 } },
    { "Blue",  { 222, 228, 232, 255 }, { 120, 150, 172, 255 } },
};
static const int BOARD_THEME_COUNT = (int)(sizeof(BOARD_THEMES) / sizeof(BOARD_THEMES[0]));

// Arrow colors by rank: best, second, third, the rest.
static const Color ARROW_COLORS[4] = {
    {  84, 210, 120, 225 }, { 240, 190,  70, 205 }, { 236, 132,  64, 190 }, { 170, 176, 196, 165 },
};
static const Color COL_REPLY = { 230,  70,  70, 170 };

// ============================================================
// APP STATE
// ============================================================
enum class AppState { WaitingForHuman, WaitingBeforeAI, ComputingAI, GameOver, Stopped };

struct PlayerConfig {
    bool        isHuman = true;
    AgentSpec   spec;
    std::string id;       // canonical id (meaningful only when !isHuman)
    std::string label;    // optional friendly name (preset or favorite), display only
};

static AppState     g_state = AppState::Stopped;
static PlayerConfig g_white, g_black;
static GuiPos       g_pos;
static std::vector<GuiPos>  g_history;      // position before each played move
static std::vector<GuiMove> g_moves;        // moves played so far
static GuiMove      g_lastMove;
static int          g_winner = None;
static char         g_boardFile[128] = "boards/board1.txt";
static std::string  g_gameBoardFile;        // the board the running game started from
static std::string  g_status = "Welcome.";
static bool         g_statusErr = false;

// AI move in flight.
static int    g_aiRequest = 0;
static int    g_aiSide = White;
static double g_aiStart = 0.0;

// Per-side readout from that side's last agent move (the agent's own view).
struct SideReadout { bool has = false; bool hasImm = false, hasDown = false; int imm = 0, down = 0;
                     unsigned long long nodes = 0; double effDepth = 0, ms = 0; bool byOpener = false; };
static SideReadout g_readW, g_readB;

// Pacing.
static int    g_speedIndex = 2;          // 0=Step 1=0.25x 2=1x 3=4x 4=Instant
static double g_aiTimer = 0.0;
static bool   g_paused = false;
static bool   g_stepRequested = false;
static bool   g_delay2s = false;
static const double SPEED_DELAY[5] = { 0.0, 1.0, 0.25, 0.0625, 0.0 };
static const char  *SPEED_NAME[5]  = { "Step", "0.25x", "1x", "4x", "Instant" };

// Human input.
static bool g_hasSel = false, g_dragging = false;
static int  g_selX = 0, g_selY = 0;

// Analysis settings (what drives the arrows and the eval bar).
struct AnaSettings {
    bool on = true;
    int  arrows = 3;
    bool reply = true;
    bool labels = true;
    bool evalBar = true;
    int  evaluator = 0;
    int  params[MAX_EVAL_PARAMS] = { 0 };
    int  modelSlot = 0;
    bool tt = true, ord = true, qs = false;
    int  maxDepth = 30;
    int  maxSeconds = 60;
};
static AnaSettings g_ana;
static int         g_anaRequest = -1;
static GuiPos      g_anaPos;
static unsigned    g_anaCfgHash = 0;
static EngAnalysis g_anaSnap;
static bool        g_anaSnapValid = false;   // snapshot matches the current position
static bool        g_anaWeights = false;     // show the heuristic weight rows

// View settings.
static bool g_showPanel = true;
static int  g_panelTab = 0;                 // 0 Play, 1 Analysis, 2 View
static int  g_pieceTheme = 0, g_boardTheme = 0;
static bool g_flip = false;
static bool g_coords = true, g_showLast = true, g_showHints = true, g_showReadouts = true;
static int  g_stepStyle = 0;
static bool g_simple = false;
static int  g_simpleMode = 0;               // 0 play White, 1 play Black, 2 watch
static int  g_simpleLevel = 1;              // 0 easy, 1 medium, 2 hard

// Modals.
struct EditorState {
    int       side = -1;                    // -1 closed, 0 White, 1 Black
    AgentSpec working;
    char      idBuf[512] = "";
    bool      idEdit = false;
    std::string idError;
    int       seededForEval = -1;
    bool      editExplorer = false, editChooser = false, editEval = false, editOpener = false;
    bool      dilute = false;
    int       dilPct = 0, nodeBudgetK = 0, timeBudgetInt = 0, openerSel = 0;
    int       histScroll = 0, histActive = -1;
    Vector2   scroll = { 0, 0 };
    float     contentH = 600;
};
static EditorState g_ed;

struct LibraryState {
    bool  open = false;
    int   side = 1;
    int   tab = 0;                          // 0 Champions 1 Presets 2 Standings 3 Favorites 4 Recent
    char  filter[64] = "";
    bool  filterEdit = false;
    int   head = 0;                         // standings head filter, 0 = all
    bool  headEdit = false;
    int   scroll = 0, active = -1, focus = -1;
    std::string parsedId;                   // id the cached parse below belongs to
    bool  parseOk = false;
    std::string parseErr;
    AgentSpec parsed;
    char  favName[64] = "";
    bool  favEdit = false;
};
static LibraryState g_lib;

struct ModelPickerState {
    bool open = false;
    int  target = 0;                        // 0 = agent editor, 1 = analysis evaluator
    char filter[48] = "";
    bool filterEdit = false;
    bool ratedOnly = false;
    int  scroll = 0, active = -1, focus = -1;
};
static ModelPickerState g_mp;

static bool g_saveFavOpen = false;
static int  g_saveFavSide = 0;
static char g_saveFavName[64] = "";

// Capture mode (native): render into a hidden window and save a PNG.
struct CaptureOpts { bool on = false; std::string out; int frames = 120; int w = INIT_W, h = INIT_H;
                     std::string scenario; std::string moves; };
static CaptureOpts g_cap;

static bool g_anyTextEdit = false;          // a text box has keyboard focus this frame

// ============================================================
// HELPERS
// ============================================================
static void SetStatus(const std::string &s, bool err = false) { g_status = s; g_statusErr = err; }

static int TextW(const std::string &s, int size) { return MeasureText(s.c_str(), size); }

// Truncate to fit maxW pixels at `size`, with a trailing "...". Binary search on
// the kept length: MeasureText is linear in the glyph table, and list views call
// this for every row.
static std::string FitText(const std::string &s, float maxW, int size) {
    if (TextW(s, size) <= maxW) return s;
    size_t lo = 0, hi = s.size();
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (TextW(s.substr(0, mid) + "...", size) <= maxW) lo = mid;
        else hi = mid - 1;
    }
    return s.substr(0, lo) + "...";
}

// Wrap at '.', ',' and ' ' boundaries (canonical ids have no spaces), hard-
// breaking any single token wider than a line.
static std::vector<std::string> WrapText(const std::string &s, float maxW, int size) {
    std::vector<std::string> out;
    std::string line, tok;
    auto flushTok = [&]() {
        if (tok.empty()) return;
        if (!line.empty() && TextW(line + tok, size) > maxW) { out.push_back(line); line.clear(); }
        while (tok.size() > 1 && TextW(tok, size) > maxW) {
            size_t k = tok.size();
            while (k > 1 && TextW(tok.substr(0, k), size) > maxW) k--;
            out.push_back(tok.substr(0, k));
            tok = tok.substr(k);
        }
        line += tok;
        tok.clear();
    };
    for (size_t i = 0; i < s.size(); i++) {
        tok += s[i];
        if (s[i] == '.' || s[i] == ',' || s[i] == ' ') flushTok();
    }
    flushTok();
    if (!line.empty()) out.push_back(line);
    return out;
}

static float DrawWrapped(const std::string &s, float x, float y, float w, int size, Color c) {
    std::vector<std::string> lines = WrapText(s, w, size);
    for (size_t i = 0; i < lines.size(); i++) {
        DrawText(lines[i].c_str(), (int)x, (int)y, size, c);
        y += size + 3;
    }
    return y;
}

static bool ContainsNoCase(const std::string &hay, const char *needle) {
    if (!needle || !*needle) return true;
    std::string h = hay, n = needle;
    for (size_t i = 0; i < h.size(); i++) h[i] = (char)std::tolower((unsigned char)h[i]);
    for (size_t i = 0; i < n.size(); i++) n[i] = (char)std::tolower((unsigned char)n[i]);
    return h.find(n) != std::string::npos;
}

static const char *SideName(int side) { return side == White ? "White" : "Black"; }

// Forced wins as +WIN / -WIN, otherwise the signed white-centric number.
static std::string FormatEval(int v) {
    if (v >= WhiteWin - 1024) return "+WIN";
    if (v <= BlackWin + 1024) return "-WIN";
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%+d", v);
    return buf;
}

static std::string FormatNodes(unsigned long long n) {
    char buf[32];
    if (n >= 1000000ULL) std::snprintf(buf, sizeof(buf), "%.1fM", n / 1e6);
    else if (n >= 1000ULL) std::snprintf(buf, sizeof(buf), "%.0fk", n / 1e3);
    else std::snprintf(buf, sizeof(buf), "%llu", n);
    return buf;
}

static std::string PlayerName(const PlayerConfig &c) {
    if (c.isHuman) return "Human";
    if (!c.label.empty()) return c.label;
    return libShortName(c.id);
}

static bool IsAlphaBeta(int explorer) {
    return explorer >= 0 && explorer < g_explorerCount && std::strcmp(g_explorers[explorer].name, "AlphaBeta") == 0;
}
static bool IsGumbel(int explorer) {
    return explorer >= 0 && explorer < g_explorerCount && std::strcmp(g_explorers[explorer].name, "GumbelMCTS") == 0;
}
static int ChooserIndexByName(const char *n) {
    for (int i = 0; i < g_chooserCount; i++) if (std::strcmp(g_choosers[i].name, n) == 0) return i;
    return -1;
}
static int EvaluatorIndexByName(const char *n) {
    for (int i = 0; i < g_evalCount; i++) if (std::strcmp(g_evaluators[i].name, n) == 0) return i;
    return -1;
}

static void SeedEvalDefaults(int evaluator, int *params) {
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) params[i] = 0;
    if (evaluator < 0 || evaluator >= g_evalCount) return;
    const EvalDef &e = g_evaluators[evaluator];
    for (int i = 0; i < e.paramCount; i++) params[i] = e.params[i].def;
}

// ---- Settings persistence ----
// Players persist as their canonical id; everything else as a small integer.
static void SaveAllSettings() {
    if (g_cap.on) return;
    libSetSettingInt("white.human", g_white.isHuman ? 1 : 0);
    libSetSetting("white.id", g_white.id);
    libSetSetting("white.label", g_white.label);
    libSetSettingInt("black.human", g_black.isHuman ? 1 : 0);
    libSetSetting("black.id", g_black.id);
    libSetSetting("black.label", g_black.label);
    libSetSetting("board", g_boardFile);
    libSetSettingInt("ana.on", g_ana.on);
    libSetSettingInt("ana.arrows", g_ana.arrows);
    libSetSettingInt("ana.reply", g_ana.reply);
    libSetSettingInt("ana.labels", g_ana.labels);
    libSetSettingInt("ana.evalbar", g_ana.evalBar);
    libSetSettingInt("ana.evaluator", g_ana.evaluator);
    std::string ps;
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) { if (i) ps += ","; ps += std::to_string(g_ana.params[i]); }
    libSetSetting("ana.params", ps);
    libSetSettingInt("ana.model", g_ana.modelSlot);
    libSetSettingInt("ana.tt", g_ana.tt);
    libSetSettingInt("ana.ord", g_ana.ord);
    libSetSettingInt("ana.qs", g_ana.qs);
    libSetSettingInt("ana.depth", g_ana.maxDepth);
    libSetSettingInt("ana.seconds", g_ana.maxSeconds);
    libSetSettingInt("view.pieces", g_pieceTheme);
    libSetSettingInt("view.board", g_boardTheme);
    libSetSettingInt("view.flip", g_flip);
    libSetSettingInt("view.coords", g_coords);
    libSetSettingInt("view.last", g_showLast);
    libSetSettingInt("view.hints", g_showHints);
    libSetSettingInt("view.readouts", g_showReadouts);
    libSetSettingInt("view.sliders", g_stepStyle);
    libSetSettingInt("view.panel", g_showPanel);
    libSetSettingInt("view.tab", g_panelTab);
    libSetSettingInt("view.simple", g_simple);
    libSetSettingInt("simple.mode", g_simpleMode);
    libSetSettingInt("simple.level", g_simpleLevel);
    libSetSettingInt("pace.speed", g_speedIndex);
    libSetSettingInt("pace.delay2s", g_delay2s);
}

static void ClampSettings() {
    if (g_pieceTheme < 0 || g_pieceTheme >= PIECE_THEME_COUNT) g_pieceTheme = 0;
    if (g_boardTheme < 0 || g_boardTheme >= BOARD_THEME_COUNT) g_boardTheme = 0;
    if (g_ana.evaluator < 0 || g_ana.evaluator >= g_evalCount) g_ana.evaluator = 0;
    if (g_ana.arrows < 0) g_ana.arrows = 0;
    if (g_ana.arrows > 8) g_ana.arrows = 8;
    if (g_ana.maxDepth < 1) g_ana.maxDepth = 1;
    if (g_ana.maxDepth > 60) g_ana.maxDepth = 60;
    if (g_speedIndex < 0 || g_speedIndex > 4) g_speedIndex = 2;
    if (g_simpleMode < 0 || g_simpleMode > 2) g_simpleMode = 0;
    if (g_simpleLevel < 0 || g_simpleLevel > 2) g_simpleLevel = 1;
    if (g_panelTab < 0 || g_panelTab > 2) g_panelTab = 0;
}

// ============================================================
// PLAYERS
// ============================================================
// Adopt a canonical id for a side. The parse validates the grammar and each
// learned model's file hash; the engine reloads the model slot from disk before
// the agent's next move (engInvalidateModel), so a retrained file is picked up.
static bool SetPlayerFromId(PlayerConfig &c, const std::string &id, const std::string &label, std::string &err) {
    RankAgent a;
    if (!rankAgentFromId(id, a, err)) return false;
    c.isHuman = false;
    c.spec = a.spec;
    c.id = a.id.empty() ? rankAgentId(a.spec) : a.id;
    c.label = label;
    if (c.spec.brain == BRAIN_SEARCH && c.spec.evaluator == learnedValueIndex()) engInvalidateModel(c.spec.modelSlot);
    if (c.spec.brain == BRAIN_POLICY) engInvalidateModel(c.spec.modelSlot);
    if (!g_cap.on) libRemember(c.id);
    return true;
}

static void SetPlayerFromSpec(PlayerConfig &c, const AgentSpec &s, const std::string &label) {
    c.isHuman = false;
    c.spec = s;
    c.id = rankAgentId(s);
    c.label = label;
    engInvalidateModel(s.modelSlot);
    if (!g_cap.on) libRemember(c.id);
}

// The GUI's historical default (depth-8 alpha-beta over Classic), used only if
// no preset or saved agent can be loaded.
static void SetFallbackAgent(PlayerConfig &c) {
    int ab = 0, classic = EvaluatorIndexByName("Classic");
    for (int i = 0; i < g_explorerCount; i++) if (IsAlphaBeta(i)) ab = i;
    c.isHuman = false;
    c.spec = agentMakeSearch("gui", ab, classic < 0 ? 0 : classic, 8, 0);
    c.id = rankAgentId(c.spec);
    c.label = "";
}

static bool SetPlayerFromPreset(PlayerConfig &c, const char *role) {
    const LibEntry *e = libPresetByRole(role);
    if (!e) { SetStatus(std::string("gui/presets.txt has no '") + role + "' preset", true); return false; }
    std::string err;
    if (!SetPlayerFromId(c, e->id, e->label, err)) {
        SetStatus(std::string("Preset '") + role + "' failed: " + err, true);
        return false;
    }
    return true;
}

static void StartGame();

// Simple mode: the matchup follows the mode and difficulty buttons directly.
static void ApplySimpleMatchup() {
    static const char *LEVEL_ROLE[3] = { "easy", "medium", "hard" };
    bool ok = true;
    if (g_simpleMode == 2) {
        ok = SetPlayerFromPreset(g_white, "watch_white") && ok;
        ok = SetPlayerFromPreset(g_black, "watch_black") && ok;
    } else {
        PlayerConfig &human = (g_simpleMode == 0) ? g_white : g_black;
        PlayerConfig &ai    = (g_simpleMode == 0) ? g_black : g_white;
        human = PlayerConfig();
        human.isHuman = true;
        ok = SetPlayerFromPreset(ai, LEVEL_ROLE[g_simpleLevel]);
    }
    if (!ok && !g_white.isHuman && g_white.id.empty()) SetFallbackAgent(g_white);
    if (!ok && !g_black.isHuman && g_black.id.empty()) SetFallbackAgent(g_black);
    g_flip = (g_simpleMode == 1);
    StartGame();
}

// ============================================================
// GAME FLOW
// ============================================================
static bool IsHumanSide(int side) { return side == White ? g_white.isHuman : g_black.isHuman; }

static void NextTurnState() {
    if (IsHumanSide(g_pos.side)) g_state = AppState::WaitingForHuman;
    else { g_state = AppState::WaitingBeforeAI; g_aiTimer = 0.0; }
}

static void StartGame() {
    engCancelMove();
    GuiPos p;
    std::string err;
    if (!guiLoadBoardFile(g_boardFile, p, err)) { SetStatus(err, true); return; }
    g_pos = p;
    g_history.clear();
    g_moves.clear();
    g_lastMove = GuiMove();
    g_hasSel = g_dragging = false;
    g_paused = false;
    g_stepRequested = false;
    g_readW = SideReadout();
    g_readB = SideReadout();
    g_gameBoardFile = g_boardFile;
    engNewGame();
    g_winner = guiWinner(g_pos);
    if (g_winner != None) { g_state = AppState::GameOver; SetStatus("That position is already decided."); return; }
    NextTurnState();
    SetStatus("New game. White to move.");
}

static void ApplyMove(const GuiMove &m) {
    g_history.push_back(g_pos);
    g_moves.push_back(m);
    g_winner = guiApplyMove(g_pos, m);
    g_lastMove = m;
    g_hasSel = g_dragging = false;
    if (g_winner != None) {
        g_state = AppState::GameOver;
        SetStatus(std::string(SideName(g_winner)) + " wins.");
        return;
    }
    NextTurnState();
}

// Take back to the last position where a human was to move (one move in a
// human-vs-human game, two against an agent). In an agent-vs-agent game, one
// move, and the game pauses.
static void Undo() {
    if (g_history.empty()) return;
    engCancelMove();
    int humans = (g_white.isHuman ? 1 : 0) + (g_black.isHuman ? 1 : 0);
    do {
        g_pos = g_history.back();
        g_history.pop_back();
        g_moves.pop_back();
    } while (humans > 0 && !g_history.empty() && !IsHumanSide(g_pos.side));
    g_lastMove = g_moves.empty() ? GuiMove() : g_moves.back();
    g_winner = None;
    g_hasSel = g_dragging = false;
    if (humans == 0) g_paused = true;
    NextTurnState();
    SetStatus("Move taken back.");
}

static void TryHumanMove(int sx, int sy, int x, int y) {
    int dir = (g_pos.side == White) ? 1 : -1;
    if (y != sy + dir) { SetStatus("Pieces move one row forward, straight or diagonally.", true); return; }
    if (!guiIsLegal(g_pos, sx, sy, x)) {
        if (x == sx && g_pos.b[x][y] != EMPTY) SetStatus("Straight moves cannot capture.", true);
        else SetStatus("Illegal move.", true);
        return;
    }
    GuiMove m;
    m.sx = sx; m.sy = sy; m.dx = x; m.dy = y;
    SetStatus(std::string(SideName(g_pos.side)) + " played " + guiMoveText(m) + ".");
    ApplyMove(m);
}

static void LaunchAIMove() {
    PlayerConfig &cfg = (g_pos.side == White) ? g_white : g_black;
    g_aiSide = g_pos.side;
    g_aiRequest = engRequestMove(g_pos, cfg.spec);
    g_aiStart = GetTime();
    g_state = AppState::ComputingAI;
}

static void FinalizeAIMove(const EngMoveResult &r) {
    if (!r.move.valid()) {
        g_state = AppState::Stopped;
        SetStatus(std::string(SideName(g_aiSide)) + " agent failed: " + r.error, true);
        return;
    }
    SideReadout &rd = (g_aiSide == White) ? g_readW : g_readB;
    rd.has = true;
    rd.hasImm = r.hasImm; rd.imm = r.imm;
    rd.hasDown = r.hasDown; rd.down = r.down;
    rd.nodes = r.nodes; rd.effDepth = r.effDepth; rd.ms = r.ms; rd.byOpener = r.byOpener;
    SetStatus(std::string(SideName(g_aiSide)) + " played " + guiMoveText(r.move) +
              (r.byOpener ? " (opener)." : "."));
    ApplyMove(r.move);
}

// Matchup classification drives the pacing controls: with a human in the game
// there is nothing to pace (slow AI paces itself; a fast AI can optionally be held
// to a 2s minimum), while AI vs AI gets the full speed/pause/step/restart set.
struct Matchup { int humans; bool aiVsAi; bool aiSlow; };
static Matchup ClassifyMatchup() {
    Matchup mu;
    mu.humans = (g_white.isHuman ? 1 : 0) + (g_black.isHuman ? 1 : 0);
    mu.aiVsAi = (mu.humans == 0);
    const PlayerConfig &ai = g_white.isHuman ? g_black : g_white;
    bool budgeted = ai.spec.nodeBudget > 0 || ai.spec.timeBudgetMs > 0;
    mu.aiSlow = !ai.isHuman && ai.spec.brain == BRAIN_SEARCH && IsAlphaBeta(ai.spec.explorer) &&
                ai.spec.depth > 5 && !budgeted;
    return mu;
}

// ============================================================
// ANALYSIS
// ============================================================
static EngAnalysisConfig AnalysisConfig() {
    EngAnalysisConfig c;
    c.evaluator = g_ana.evaluator;
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) c.params[i] = g_ana.params[i];
    c.modelSlot = g_ana.modelSlot;
    c.useTT = g_ana.tt;
    c.useMoveOrder = g_ana.ord;
    c.useQuiescence = g_ana.qs;
    c.maxDepth = g_ana.maxDepth;
    c.maxSeconds = (double)g_ana.maxSeconds;
    return c;
}

static unsigned AnalysisHash() {
    unsigned h = 2166136261u;
    auto mix = [&](int v) { h ^= (unsigned)v; h *= 16777619u; };
    mix(g_ana.evaluator);
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) mix(g_ana.params[i]);
    mix(g_ana.modelSlot); mix(g_ana.tt); mix(g_ana.ord); mix(g_ana.qs);
    mix(g_ana.maxDepth); mix(g_ana.maxSeconds);
    return h;
}

static bool SamePos(const GuiPos &a, const GuiPos &b) {
    return a.side == b.side && std::memcmp(a.b, b.b, sizeof(a.b)) == 0;
}

static bool AnalysisWanted() {
    if (!g_ana.on || (g_ana.arrows == 0 && !g_ana.evalBar)) return false;
    return g_state == AppState::WaitingForHuman || g_state == AppState::WaitingBeforeAI ||
           g_state == AppState::ComputingAI || g_state == AppState::Stopped;
}

// Keep the engine's analysis on the displayed position with the current
// settings: restart it when either changes, stop it when unwanted.
static void UpdateAnalysis() {
    if (!AnalysisWanted()) {
        if (g_anaRequest >= 0) { engStopAnalysis(); g_anaRequest = -1; }
        g_anaSnapValid = false;
        return;
    }
    unsigned h = AnalysisHash();
    if (g_anaRequest < 0 || !SamePos(g_anaPos, g_pos) || h != g_anaCfgHash) {
        g_anaPos = g_pos;
        g_anaCfgHash = h;
        g_anaRequest = engStartAnalysis(g_pos, AnalysisConfig());
    }
    EngAnalysis s;
    if (engAnalysisSnapshot(s) && s.request == g_anaRequest) { g_anaSnap = s; g_anaSnapValid = true; }
    else g_anaSnapValid = false;
}

static void AnalysisUseAgentEvaluator(const PlayerConfig &c) {
    if (c.isHuman || c.spec.brain != BRAIN_SEARCH) {
        SetStatus("That side has no search evaluator to copy.", true);
        return;
    }
    g_ana.evaluator = c.spec.evaluator;
    for (int i = 0; i < MAX_EVAL_PARAMS; i++) g_ana.params[i] = c.spec.evalParams[i];
    g_ana.modelSlot = c.spec.modelSlot;
    g_ana.qs = c.spec.useQuiescence;
    SetStatus("Analysis now uses " + PlayerName(c) + "'s evaluator.");
}

// ============================================================
// UPDATE
// ============================================================
static bool AnyModalOpen() { return g_ed.side >= 0 || g_lib.open || g_mp.open || g_saveFavOpen; }

static void OpenAgentEditor(int side, const AgentSpec *from);
static void OpenLibrary(int side);

static bool ScreenToSquare(Vector2 m, int &x, int &y);

static void HandleKeys() {
    if (g_anyTextEdit) return;
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (g_mp.open) g_mp.open = false;
        else if (g_saveFavOpen) g_saveFavOpen = false;
        else if (g_lib.open) g_lib.open = false;
        else if (g_ed.side >= 0) g_ed.side = -1;
        else g_hasSel = false;
        return;
    }
    if (AnyModalOpen()) return;
    if (IsKeyPressed(KEY_TAB)) g_showPanel = !g_showPanel;
    if (IsKeyPressed(KEY_A))   g_ana.on = !g_ana.on;
    if (IsKeyPressed(KEY_E))   g_showReadouts = !g_showReadouts;
    if (IsKeyPressed(KEY_F))   g_flip = !g_flip;
    if (IsKeyPressed(KEY_U) || (IsKeyPressed(KEY_Z) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)))) Undo();
    if (IsKeyPressed(KEY_N))   StartGame();
    if (IsKeyPressed(KEY_SPACE) && ClassifyMatchup().aiVsAi) g_paused = !g_paused;
}

static void HandleBoardInput() {
    if (g_state != AppState::WaitingForHuman || AnyModalOpen()) { g_dragging = false; return; }
    Vector2 m = GetMousePosition();
    bool overPanel = g_showPanel && CheckCollisionPointRec(m, g_panelRect);
    char me = (g_pos.side == White) ? WHITE : BLACK;
    int x, y;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !overPanel && ScreenToSquare(m, x, y)) {
        if (g_pos.b[x][y] == me) {
            g_hasSel = true; g_selX = x; g_selY = y; g_dragging = true;
        } else if (g_hasSel) {
            TryHumanMove(g_selX, g_selY, x, y);
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && g_dragging) {
        g_dragging = false;
        if (!overPanel && ScreenToSquare(m, x, y) && !(x == g_selX && y == g_selY) && g_state == AppState::WaitingForHuman)
            TryHumanMove(g_selX, g_selY, x, y);
    }
}

static void Update() {
    HandleKeys();
    HandleBoardInput();

    if (g_state == AppState::WaitingBeforeAI) {
        g_aiTimer += GetFrameTime();
        Matchup mu = ClassifyMatchup();
        bool go = false;
        if (mu.aiVsAi) {
            bool stepMode = (g_speedIndex == 0);
            if (g_stepRequested) go = true;
            else if (!g_paused && !stepMode && g_aiTimer >= SPEED_DELAY[g_speedIndex]) go = true;
        } else {
            double need = (!mu.aiSlow && g_delay2s) ? 2.0 : 0.0;
            if (g_aiTimer >= need) go = true;
        }
        if (go) { g_stepRequested = false; LaunchAIMove(); }
    }

    if (g_state == AppState::ComputingAI) {
        EngMoveResult r;
        if (engTakeMoveResult(r) && r.request == g_aiRequest) FinalizeAIMove(r);
    }

    UpdateAnalysis();
    engTick();

    static double lastSave = 0.0;
    if (GetTime() - lastSave > 1.0) { SaveAllSettings(); if (!g_cap.on) libSaveSettings(); lastSave = GetTime(); }
}

// ============================================================
// LAYOUT COMPUTE + MAPPING
// ============================================================
static void ComputeLayout() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    int left = g_showPanel ? PANEL_W : 0;
    int evalSpace = (g_ana.on && g_ana.evalBar) ? EVALBAR_W + 12 : 0;
    int areaX = left + evalSpace;
    int areaW = W - areaX - RIGHT_STRIP;
    int byW = (areaW - 2 * MARGIN) / SIZE;
    int byH = (H - TOP - 2 * MARGIN) / SIZE;
    g_cell = byW < byH ? byW : byH;
    if (g_cell < 8) g_cell = 8;
    g_boardPx = g_cell * SIZE;
    g_boardX = areaX + (areaW - g_boardPx) / 2;
    g_boardY = TOP + (H - TOP - g_boardPx) / 2;
    g_panelRect = Rectangle{ 0, (float)TOP, (float)PANEL_W, (float)(H - TOP) };
    g_evalBarRect = Rectangle{ (float)(g_boardX - MARGIN + 2 - EVALBAR_W - 4), (float)g_boardY,
                               (float)EVALBAR_W, (float)g_boardPx };
}

// Board (x = column, y = row, row 0 = White's back rank) to screen. Unflipped,
// White sits at the bottom; flipped, Black does.
static int ScreenCol(int x) { return g_flip ? SIZE - 1 - x : x; }
static int ScreenRow(int y) { return g_flip ? y : SIZE - 1 - y; }
static Rectangle SquareRect(int x, int y) {
    return Rectangle{ (float)(g_boardX + ScreenCol(x) * g_cell), (float)(g_boardY + ScreenRow(y) * g_cell),
                      (float)g_cell, (float)g_cell };
}
static Vector2 SquareCenter(int x, int y) {
    Rectangle r = SquareRect(x, y);
    return Vector2{ r.x + r.width / 2, r.y + r.height / 2 };
}
static bool ScreenToSquare(Vector2 m, int &x, int &y) {
    if (m.x < g_boardX || m.y < g_boardY) return false;
    int sc = (int)((m.x - g_boardX) / g_cell), sr = (int)((m.y - g_boardY) / g_cell);
    if (sc < 0 || sc >= SIZE || sr < 0 || sr >= SIZE) return false;
    x = g_flip ? SIZE - 1 - sc : sc;
    y = g_flip ? sr : SIZE - 1 - sr;
    return true;
}

// ============================================================
// BOARD RENDERING
// ============================================================
static void DrawPieceAt(Vector2 c, float r, char who) {
    const PieceTheme &t = PIECE_THEMES[g_pieceTheme];
    Color fill = (who == WHITE) ? t.wFill : t.bFill;
    Color edge = (who == WHITE) ? t.wEdge : t.bEdge;
    DrawCircleV(Vector2{ c.x + r * 0.06f, c.y + r * 0.10f }, r, Color{ 0, 0, 0, 60 });   // soft shadow
    DrawCircleV(c, r, fill);
    DrawCircleLines((int)c.x, (int)c.y, r, edge);
    DrawCircleLines((int)c.x, (int)c.y, r * 0.72f, Color{ edge.r, edge.g, edge.b, 110 });
}

// Filled triangle regardless of winding (raylib culls clockwise triangles).
static void DrawTri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0) DrawTriangle(a, b, c, col);
    else           DrawTriangle(a, c, b, col);
}

static void DrawArrow(Vector2 a, Vector2 b, float width, Color col, bool dashed) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1) return;
    float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    Vector2 s   = { a.x + ux * g_cell * 0.22f, a.y + uy * g_cell * 0.22f };
    Vector2 tip = { b.x - ux * g_cell * 0.14f, b.y - uy * g_cell * 0.14f };
    float headLen = width * 2.4f, headW = width * 1.9f;
    Vector2 base = { tip.x - ux * headLen, tip.y - uy * headLen };
    if (dashed) {
        float segLen = width * 1.6f, total = std::sqrt((base.x - s.x) * (base.x - s.x) + (base.y - s.y) * (base.y - s.y));
        for (float t = 0; t < total; t += segLen * 2) {
            float e = std::min(t + segLen, total);
            DrawLineEx(Vector2{ s.x + ux * t, s.y + uy * t }, Vector2{ s.x + ux * e, s.y + uy * e }, width, col);
        }
    } else {
        DrawLineEx(s, base, width, col);
    }
    DrawTri(tip, Vector2{ base.x + nx * headW, base.y + ny * headW }, Vector2{ base.x - nx * headW, base.y - ny * headW }, col);
}

static void DrawScorePill(Vector2 at, const std::string &txt, Color edge) {
    int fs = g_cell / 5;
    if (fs < 11) fs = 11;
    if (fs > 16) fs = 16;
    int tw = TextW(txt, fs);
    Rectangle r = { at.x - tw / 2.0f - 5, at.y - fs / 2.0f - 3, (float)tw + 10, (float)fs + 6 };
    DrawRectangleRounded(r, 0.5f, 6, Color{ 20, 22, 28, 215 });
    DrawRectangleRoundedLinesEx(r, 0.5f, 6, 1.5f, edge);
    DrawText(txt.c_str(), (int)(r.x + 5), (int)(r.y + 3), fs, COL_NUM);
}

static void DrawArrows() {
    if (!g_ana.on || g_ana.arrows <= 0 || !g_anaSnapValid || g_anaSnap.depth < 1) return;
    if (g_state == AppState::GameOver) return;
    const std::vector<EngLine> &L = g_anaSnap.lines;
    int n = std::min((int)L.size(), g_ana.arrows);
    // Reply to the best line first, underneath everything.
    if (g_ana.reply && n > 0 && L[0].reply.valid())
        DrawArrow(SquareCenter(L[0].reply.sx, L[0].reply.sy), SquareCenter(L[0].reply.dx, L[0].reply.dy),
                  g_cell * 0.07f, COL_REPLY, true);
    for (int k = n - 1; k >= 0; k--) {
        Color c = ARROW_COLORS[k < 3 ? k : 3];
        float w = g_cell * (k == 0 ? 0.13f : (k == 1 ? 0.10f : 0.08f));
        Vector2 a = SquareCenter(L[k].move.sx, L[k].move.sy), b = SquareCenter(L[k].move.dx, L[k].move.dy);
        DrawArrow(a, b, w, c, false);
    }
    if (g_ana.labels) {
        for (int k = n - 1; k >= 0; k--) {
            Vector2 a = SquareCenter(L[k].move.sx, L[k].move.sy), b = SquareCenter(L[k].move.dx, L[k].move.dy);
            Vector2 at = { a.x + (b.x - a.x) * 0.45f, a.y + (b.y - a.y) * 0.45f };
            DrawScorePill(at, FormatEval(L[k].score), ARROW_COLORS[k < 3 ? k : 3]);
        }
    }
}

static void DrawBoard() {
    const BoardTheme &bt = BOARD_THEMES[g_boardTheme];
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            Rectangle r = SquareRect(x, y);
            DrawRectangleRec(r, ((x + y) % 2 == 0) ? bt.dark : bt.light);
        }

    if (g_showLast && g_lastMove.valid()) {
        DrawRectangleRec(SquareRect(g_lastMove.sx, g_lastMove.sy), COL_LAST);
        DrawRectangleRec(SquareRect(g_lastMove.dx, g_lastMove.dy), COL_LAST);
    }

    // Hover highlight while a human is to move.
    Vector2 m = GetMousePosition();
    int hx, hy;
    if (g_state == AppState::WaitingForHuman && !AnyModalOpen() &&
        !(g_showPanel && CheckCollisionPointRec(m, g_panelRect)) && ScreenToSquare(m, hx, hy))
        DrawRectangleRec(SquareRect(hx, hy), COL_HOVER);

    float pr = g_cell * 0.36f;
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            char who = g_pos.b[x][y];
            if (who != WHITE && who != BLACK) continue;
            if (g_dragging && g_hasSel && x == g_selX && y == g_selY) continue;
            DrawPieceAt(SquareCenter(x, y), pr, who);
        }

    if (g_hasSel) {
        DrawRectangleLinesEx(SquareRect(g_selX, g_selY), 4, COL_SEL);
        if (g_showHints) {
            int fwd = g_selY + ((g_pos.side == White) ? 1 : -1);
            for (int dx = -1; dx <= 1; dx++)
                if (guiIsLegal(g_pos, g_selX, g_selY, g_selX + dx)) {
                    Vector2 c = SquareCenter(g_selX + dx, fwd);
                    if (g_pos.b[g_selX + dx][fwd] != EMPTY) DrawCircleLines((int)c.x, (int)c.y, g_cell * 0.42f, COL_HINT);
                    else DrawCircleV(c, g_cell * 0.15f, COL_HINT);
                }
        }
    }

    DrawArrows();

    if (g_dragging && g_hasSel) DrawPieceAt(GetMousePosition(), pr, g_pos.b[g_selX][g_selY]);

    DrawRectangleLinesEx(Rectangle{ (float)g_boardX, (float)g_boardY, (float)g_boardPx, (float)g_boardPx }, 2, COL_LABEL);
    if (g_coords) {
        int fs = g_cell / 4;
        if (fs < 12) fs = 12; else if (fs > 20) fs = 20;
        for (int x = 0; x < SIZE; x++) {
            const char *lbl = TextFormat("%c", 'a' + x);
            Rectangle r = SquareRect(x, 0);
            int tx = (int)(r.x + g_cell / 2 - TextW(lbl, fs) / 2);
            DrawText(lbl, tx, g_boardY - fs - 5, fs, COL_LABEL);
            DrawText(lbl, tx, g_boardY + g_boardPx + 5, fs, COL_LABEL);
        }
        for (int y = 0; y < SIZE; y++) {
            const char *lbl = TextFormat("%d", y);
            Rectangle r = SquareRect(0, y);
            DrawText(lbl, g_boardX - fs - 6, (int)(r.y + g_cell / 2 - fs / 2), fs, COL_LABEL);
        }
    }
}

// The analysis eval as a fill fraction for White. Learned evaluators output
// within +/-out_scale (900 in the shipped model files), so they map linearly.
// The heuristics score about one chip weight per piece and are unbounded, so
// they go through tanh at a scale of 2.5 chips.
static float EvalFraction(int v) {
    if (v >= WhiteWin - 1024) return 1.0f;
    if (v <= BlackWin + 1024) return 0.0f;
    double f;
    if (g_ana.evaluator == learnedValueIndex()) f = 0.5 + 0.5 * v / 900.0;
    else {
        int chip = g_ana.params[1] > 0 ? g_ana.params[1] : 4;
        f = 0.5 + 0.5 * std::tanh(v / (chip * 2.5));
    }
    return (float)(f < 0 ? 0 : (f > 1 ? 1 : f));
}

static bool CurrentEval(int &v) {
    if (g_state == AppState::GameOver) { v = (g_winner == White) ? WhiteWin : BlackWin; return true; }
    if (!g_anaSnapValid) return false;
    if (g_anaSnap.depth >= 1 && !g_anaSnap.lines.empty()) { v = g_anaSnap.lines[0].score; return true; }
    if (g_anaSnap.hasStatic) { v = g_anaSnap.staticEval; return true; }
    return false;
}

static void DrawEvalBar() {
    if (!g_ana.on || !g_ana.evalBar) return;
    const PieceTheme &t = PIECE_THEMES[g_pieceTheme];
    Rectangle r = g_evalBarRect;
    DrawRectangleRec(r, t.bFill);
    int v = 0;
    bool have = CurrentEval(v);
    float f = have ? EvalFraction(v) : 0.5f;
    float wh = r.height * f;
    Rectangle w = g_flip ? Rectangle{ r.x, r.y, r.width, wh } : Rectangle{ r.x, r.y + r.height - wh, r.width, wh };
    DrawRectangleRec(w, t.wFill);
    DrawLineEx(Vector2{ r.x - 2, r.y + r.height / 2 }, Vector2{ r.x + r.width + 2, r.y + r.height / 2 }, 1, COL_ACCENT);
    DrawRectangleLinesEx(r, 1, COL_BRD);
    if (have) {
        std::string s = FormatEval(v);
        int fs = 11;
        bool whiteAhead = v >= 0;
        // Print at the leading side's end of the bar.
        bool atBottom = (whiteAhead != g_flip);
        int tw = TextW(s, fs);
        int tx = (int)(r.x + r.width / 2 - tw / 2);
        int ty = atBottom ? (int)(r.y + r.height + 4) : (int)(r.y - fs - 4);
        DrawText(s.c_str(), tx, ty, fs, COL_LABEL);
    }
}

static void DrawCountBadge(int bx, int by, char who, int count, bool toMove, bool thinking) {
    const PieceTheme &t = PIECE_THEMES[g_pieceTheme];
    const int bwd = 58, bht = 28;
    Rectangle r = { (float)bx, (float)by, (float)bwd, (float)bht };
    DrawRectangleRounded(r, 0.5f, 8, Color{ 58, 62, 74, 235 });
    if (toMove) DrawRectangleRoundedLinesEx(r, 0.5f, 8, 2.5f, COL_ACCENT);
    int cyc = by + bht / 2, cxc = bx + 15;
    Color fill = (who == WHITE) ? t.wFill : t.bFill, edge = (who == WHITE) ? t.wEdge : t.bEdge;
    DrawCircle(cxc, cyc, 9, fill);
    DrawCircleLines(cxc, cyc, 9, edge);
    DrawText(TextFormat("%d", count), bx + 30, by + 6, 17, COL_NUM);
    if (toMove) {
        const char *s = thinking ? TextFormat("thinking %.1fs", GetTime() - g_aiStart) : "to move";
        DrawText(s, bx + bwd + 6, by + 8, 12, COL_ACCENT);
    }
}

static void DrawSideInfo(int x, int y, int dir, const PlayerConfig &c, const SideReadout &rd) {
    // dir = +1: lines go downward from y; -1: upward (so the bottom side's lines
    // stack above its badge).
    std::vector<std::string> lines;
    lines.push_back(FitText(PlayerName(c), RIGHT_STRIP - 12, 12));
    if (g_showReadouts && rd.has) {
        if (rd.byOpener) lines.push_back("opener move");
        else {
            if (rd.hasImm) lines.push_back("now " + FormatEval(rd.imm));
            if (rd.hasDown) lines.push_back("pred " + FormatEval(rd.down));
            if (rd.nodes > 1) lines.push_back(TextFormat("d%.1f %s", rd.effDepth, FormatNodes(rd.nodes).c_str()));
        }
    }
    for (size_t i = 0; i < lines.size(); i++) {
        int yy = (dir > 0) ? y + (int)i * 15 : y - (int)(lines.size() - i) * 15;
        DrawText(lines[i].c_str(), x, yy, 12, i == 0 ? COL_LABEL : COL_DIM);
    }
}

static void DrawBadges() {
    if (g_boardPx <= 0) return;
    int w, b;
    guiCountPieces(g_pos, w, b);
    int bx = g_boardX + g_boardPx + 10;
    int topY = g_boardY, botY = g_boardY + g_boardPx - 28;
    bool live = (g_state != AppState::GameOver && g_state != AppState::Stopped);
    bool thinking = (g_state == AppState::ComputingAI);
    // Unflipped: Black's side is at the top of the screen.
    int blackY = g_flip ? botY : topY, whiteY = g_flip ? topY : botY;
    DrawCountBadge(bx, blackY, BLACK, b, live && g_pos.side == Black, thinking && g_aiSide == Black);
    DrawCountBadge(bx, whiteY, WHITE, w, live && g_pos.side == White, thinking && g_aiSide == White);
    DrawSideInfo(bx, g_flip ? whiteY + 34 : blackY + 34, +1, g_flip ? g_white : g_black, g_flip ? g_readW : g_readB);
    DrawSideInfo(bx, g_flip ? blackY - 6 : whiteY - 6, -1, g_flip ? g_black : g_white, g_flip ? g_readB : g_readW);
}

static void DrawGameOverBanner() {
    if (g_state != AppState::GameOver) return;
    std::string who = std::string(g_winner == White ? "WHITE" : "BLACK") + " WINS";
    int fs = 40;
    int tw = TextW(who, fs);
    int bx = g_boardX + (g_boardPx - tw) / 2 - 20;
    int by = g_boardY + g_boardPx / 2 - 34;
    DrawRectangle(bx, by, tw + 40, 68, Color{ 0, 0, 0, 190 });
    DrawText(who.c_str(), bx + 20, by + 14, fs, COL_ACCENT);
}

static void DrawTopBar() {
    int W = GetScreenWidth();
    DrawRectangle(0, 0, W, TOP, Color{ 26, 28, 35, 255 });
    DrawLine(0, TOP - 1, W, TOP - 1, Color{ 60, 64, 76, 255 });
    if (GuiButton(Rectangle{ 8, 8, 84, 28 }, g_showPanel ? "#118# Hide" : "#119# Panel")) g_showPanel = !g_showPanel;
    DrawText("Breakthrough", 104, 11, 22, COL_LABEL);

    std::string turn;
    Color tc = COL_LABEL;
    if (g_state == AppState::GameOver) { turn = std::string(SideName(g_winner)) + " wins"; tc = COL_ACCENT; }
    else if (g_state == AppState::ComputingAI) turn = std::string(SideName(g_aiSide)) + " is thinking...";
    else if (g_state == AppState::Stopped) turn = "Stopped";
    else turn = std::string(SideName(g_pos.side)) + " to move";
    if (g_paused && ClassifyMatchup().aiVsAi && g_state != AppState::GameOver) turn += " (paused)";
    int tw = TextW(turn, 20);
    int cx = g_boardX + g_boardPx / 2 - tw / 2;
    if (cx < 300) cx = 300;
    DrawText(turn.c_str(), cx, 12, 20, tc);

    std::string vs = PlayerName(g_white) + "  vs  " + PlayerName(g_black);
    vs = FitText(vs, W - (cx + tw + 40) - 12, 14);
    DrawText(vs.c_str(), W - TextW(vs, 14) - 12, 16, 14, COL_DIM);
}

// ============================================================
// WIDGETS
// ============================================================
// Deferred dropdowns: an open GuiDropdownBox list must draw after (on top of)
// everything around it, and every other control must ignore clicks while it is
// open. Controls queue here while a panel or modal draws, then FlushDropdowns
// draws the closed ones locked and the open one last.
struct DropReq { Rectangle r; std::string opts; int *val; bool *edit; };
static std::vector<DropReq> g_drops;

static void DeferDropdown(Rectangle r, const std::string &opts, int *val, bool *edit) {
    DropReq d; d.r = r; d.opts = opts; d.val = val; d.edit = edit;
    g_drops.push_back(d);
}

static bool AnyDeferredOpen() {
    for (size_t i = 0; i < g_drops.size(); i++) if (*g_drops[i].edit) return true;
    return false;
}

static void FlushDropdowns() {
    bool wasLocked = GuiIsLocked();
    int open = -1;
    for (size_t i = 0; i < g_drops.size(); i++) if (*g_drops[i].edit) open = (int)i;
    if (wasLocked) {
        for (size_t i = 0; i < g_drops.size(); i++) { *g_drops[i].edit = false; GuiDropdownBox(g_drops[i].r, g_drops[i].opts.c_str(), g_drops[i].val, false); }
        g_drops.clear();
        return;
    }
    if (open >= 0) GuiLock();
    for (size_t i = 0; i < g_drops.size(); i++) {
        if ((int)i == open) continue;
        if (GuiDropdownBox(g_drops[i].r, g_drops[i].opts.c_str(), g_drops[i].val, *g_drops[i].edit)) {
            for (size_t j = 0; j < g_drops.size(); j++) *g_drops[j].edit = false;
            *g_drops[i].edit = true;
        }
    }
    if (open >= 0) {
        GuiUnlock();
        if (GuiDropdownBox(g_drops[open].r, g_drops[open].opts.c_str(), g_drops[open].val, true)) *g_drops[open].edit = false;
    }
    g_drops.clear();
}

// Transport glyphs: forward = two right triangles (">>"), else bar + triangle ("|>").
static void DrawSpeedGlyph(Rectangle r, bool forward) {
    Color c = COL_LABEL;
    float cy = r.y + r.height / 2.0f, th = r.height * 0.28f, tw = th;
    if (forward) {
        float x0 = r.x + r.width / 2.0f - tw - 1;
        for (int i = 0; i < 2; i++) {
            float bx = x0 + i * (tw + 2);
            DrawTri(Vector2{ bx, cy - th }, Vector2{ bx, cy + th }, Vector2{ bx + tw, cy }, c);
        }
    } else {
        float bx = r.x + r.width / 2.0f - tw / 2.0f - 4;
        DrawRectangle((int)bx, (int)(cy - th), 3, (int)(2 * th), c);
        float tx = bx + 6;
        DrawTri(Vector2{ tx, cy - th }, Vector2{ tx, cy + th }, Vector2{ tx + tw, cy }, c);
    }
}

static void DrawStackedPM(float bx, float y, float bw, float h, int *val, int lo, int hi) {
    float bh = (h - 3) / 2.0f;
    if (GuiButton(Rectangle{ bx, y, bw, bh }, "+")) { if (*val < hi) (*val)++; }
    if (GuiButton(Rectangle{ bx, y + bh + 3, bw, bh }, "-")) { if (*val > lo) (*val)--; }
}

static void DrawFillBar(Rectangle r, int val, int lo, int hi, bool showNum) {
    DrawRectangleRec(r, COL_TRK);
    float frac = (hi > lo) ? (float)(val - lo) / (float)(hi - lo) : 0.0f;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    if (frac > 0) DrawRectangle((int)r.x, (int)r.y, (int)(r.width * frac), (int)r.height, COL_FILL);
    DrawRectangleLinesEx(r, 1, COL_BRD);
    if (showNum) {
        const char *s = TextFormat("%d", val);
        int tw = MeasureText(s, 16);
        DrawText(s, (int)(r.x + (r.width - tw) / 2), (int)(r.y + (r.height - 16) / 2), 16, COL_NUM);
    }
}

static void ScrubBar(Rectangle r, int *val, int lo, int hi) {
    if (GuiIsLocked()) return;
    Vector2 m = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, r)) {
        float frac = (m.x - r.x) / r.width;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        *val = lo + (int)(frac * (hi - lo) + 0.5f);
    }
}

// Numeric row designs (the "Sliders" switcher forces one on every row; 0 =
// per-row, each row uses its own assigned design).
enum StepStyle { STEP_BAR_NUM, STEP_SEGMENTS, STEP_NUMBAR, STEP_HANDLE, STEP_RULER, STEP_STYLE_COUNT };

// Edit-mode flags for the typeable designs, keyed by (owner, field). Owners:
// 0/1 the agent editor per side, 2 the analysis tab.
enum EdSlot {
    ED_DEPTH = 0, ED_ASPIRATION, ED_NODES, ED_TIME, ED_DEPTHCAP, ED_MODELSLOT, ED_RISK, ED_REM,
    ED_GCVISIT, ED_GCSCALE, ED_GM, ED_DILPCT, ED_DILDEPTH, ED_OPENERARG, ED_OPENERARG2,
    ED_ANA_DEPTH, ED_ANA_TIME, ED_ANA_ARROWS,
    ED_PARAMS_BASE,
    ED_SLOT_COUNT = ED_PARAMS_BASE + MAX_EVAL_PARAMS
};
static bool g_stepEdit[3][ED_SLOT_COUNT] = { { false } };

static float StepperRow(int owner, int param, StepStyle assigned, float x, float y, float w,
                        const char *name, int *val, int lo, int hi, int barHi = -1) {
    if (barHi < 0) barHi = hi;
    StepStyle eff = (g_stepStyle == 0) ? assigned : (StepStyle)(g_stepStyle - 1);
    bool *edit = &g_stepEdit[owner][param];
    const float labelW = (w > 400) ? 100.0f : 72.0f;
    GuiLabel(Rectangle{ x, y, labelW - 6, 20 }, name);
    float cx = x + labelW, cw = w - labelW;
    const float bw = 18;
    float pmX = cx + cw - bw;
    float ctrlW = cw - bw - 6;
    float rowH = 24;
    switch (eff) {
    case STEP_BAR_NUM: {
        Rectangle bar = { cx, y + 1, ctrlW, 20 };
        DrawFillBar(bar, *val, lo, barHi, true);
        ScrubBar(bar, val, lo, hi);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_SEGMENTS: {
        int n = hi - lo + 1;
        if (n < 1) n = 1;
        if (n > 40) n = 40;
        float gap = 2, cwd = (ctrlW - gap * (n - 1)) / n;
        if (cwd < 2) cwd = 2;
        for (int i = 0; i < n; i++) {
            Rectangle seg = { cx + i * (cwd + gap), y + 2, cwd, 18 };
            DrawRectangleRec(seg, ((lo + i) <= *val) ? COL_FILL : COL_TRK);
            DrawRectangleLinesEx(seg, 1, COL_BRD);
            if (!GuiIsLocked() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), seg))
                *val = lo + i;
        }
        DrawText(TextFormat("%d", *val), (int)(cx + 3), (int)(y + 4), 14, COL_NUM);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_NUMBAR: {
        if (GuiValueBox(Rectangle{ cx, y, ctrlW, 18 }, NULL, val, lo, hi, *edit)) *edit = !*edit;
        if (*edit) g_anyTextEdit = true;
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        Rectangle bar = { cx, y + 21, ctrlW, 5 };
        DrawFillBar(bar, *val, lo, barHi, false);
        ScrubBar(bar, val, lo, hi);
        rowH = 30;
        break;
    }
    case STEP_HANDLE: {
        Rectangle track = { cx, y + 9, ctrlW, 4 };
        DrawRectangleRec(track, COL_TRK);
        DrawRectangleLinesEx(track, 1, COL_BRD);
        float frac = (barHi > lo) ? (float)(*val - lo) / (float)(barHi - lo) : 0.0f;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        float chipW = 30;
        Rectangle chip = { cx + frac * (ctrlW - chipW), y + 2, chipW, 18 };
        DrawRectangleRec(chip, COL_FILL);
        DrawRectangleLinesEx(chip, 1, COL_BRD);
        const char *s = TextFormat("%d", *val);
        DrawText(s, (int)(chip.x + (chipW - MeasureText(s, 14)) / 2), (int)(chip.y + 2), 14, COL_NUM);
        ScrubBar(Rectangle{ cx, y, ctrlW, 22 }, val, lo, hi);
        DrawStackedPM(pmX, y, bw, 22, val, lo, hi);
        break;
    }
    case STEP_RULER: {
        float ry = y + 18;
        DrawLineEx(Vector2{ cx, ry }, Vector2{ cx + ctrlW, ry }, 2, COL_BRD);
        int n = barHi - lo;
        if (n > 0) {
            int step = n > 50 ? (n + 49) / 50 : 1;
            for (int i = 0; i <= n; i += step) {
                float tx = cx + ctrlW * (float)i / n;
                float th = ((i / step) % 5 == 0) ? 6.0f : 3.0f;
                DrawLineEx(Vector2{ tx, ry }, Vector2{ tx, ry - th }, 1, COL_BRD);
            }
            float fr = (float)(*val - lo) / n;
            if (fr > 1) fr = 1;
            if (fr < 0) fr = 0;
            float mx = cx + ctrlW * fr;
            DrawTri(Vector2{ mx + 5, ry - 12 }, Vector2{ mx - 5, ry - 12 }, Vector2{ mx, ry - 3 }, COL_FILL);
            const char *s = TextFormat("%d", *val);
            int tw = MeasureText(s, 14);
            float lx = mx - tw / 2.0f;
            if (lx < cx) lx = cx;
            if (lx > cx + ctrlW - tw) lx = cx + ctrlW - tw;
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

static float CheckRow(float x, float y, const char *text, bool *v) {
    GuiCheckBox(Rectangle{ x, y + 2, 16, 16 }, text, v);
    return y + 24;
}

static float SectionTitle(float x, float y, float w, const char *text) {
    DrawText(text, (int)x, (int)y + 2, 14, COL_ACCENT);
    DrawLine((int)(x + TextW(text, 14) + 8), (int)(y + 10), (int)(x + w), (int)(y + 10), Color{ 60, 64, 76, 255 });
    return y + 22;
}

// ============================================================
// PANEL: PLAY
// ============================================================
static std::string BoardOptions(const std::vector<std::string> &files) {
    std::string s;
    for (size_t i = 0; i < files.size(); i++) {
        if (i) s += ";";
        std::string f = files[i];
        size_t p = f.find_last_of('/');
        s += (p == std::string::npos) ? f : f.substr(p + 1);
    }
    return s;
}

static float DrawSideCard(int side, float x, float y, float w) {
    PlayerConfig &c = (side == 0) ? g_white : g_black;
    const PieceTheme &t = PIECE_THEMES[g_pieceTheme];
    Color fill = side == 0 ? t.wFill : t.bFill, edge = side == 0 ? t.wEdge : t.bEdge;
    DrawCircle((int)x + 8, (int)y + 9, 7, fill);
    DrawCircleLines((int)x + 8, (int)y + 9, 7, edge);
    DrawText(side == 0 ? "WHITE" : "BLACK", (int)x + 22, (int)y + 2, 16, COL_LABEL);

    int sel = c.isHuman ? 0 : 1;
    float tgW = 64;
    GuiToggleGroup(Rectangle{ x + w - 2 * tgW - 2, y - 2, tgW, 22 }, "Human;Agent", &sel);
    if ((sel == 0) != c.isHuman) {
        if (sel == 0) c.isHuman = true;
        else if (!c.id.empty()) c.isHuman = false;
        else OpenLibrary(side);
        if (g_state != AppState::GameOver && g_state != AppState::ComputingAI) NextTurnState();
    }
    y += 26;
    if (c.isHuman) {
        DrawText("Plays by clicking or dragging pieces.", (int)x, (int)y, 13, COL_DIM);
        return y + 22;
    }
    std::string name = PlayerName(c);
    y = DrawWrapped(name, x, y, w, 14, COL_NUM);
    const LibEntry *st = libStandingsFor(c.id);
    if (st) DrawText(TextFormat("Elo %d +/-%d on its head, %d games", st->elo, st->pm, st->games), (int)x, (int)y, 12, COL_DIM);
    else    DrawText("not in ranking/standings.tsv", (int)x, (int)y, 12, COL_DIM);
    y += 18;
    float bw3 = (w - 8) / 3.0f;
    if (GuiButton(Rectangle{ x, y, bw3, 24 }, "#17# Library")) OpenLibrary(side);
    if (GuiButton(Rectangle{ x + bw3 + 4, y, bw3, 24 }, "#140# Edit")) OpenAgentEditor(side, nullptr);
    if (GuiButton(Rectangle{ x + 2 * (bw3 + 4), y, bw3, 24 }, "#186# Save")) {
        g_saveFavOpen = true;
        g_saveFavSide = side;
        std::strncpy(g_saveFavName, PlayerName(c).substr(0, 60).c_str(), sizeof(g_saveFavName) - 1);
        g_saveFavName[sizeof(g_saveFavName) - 1] = '\0';
    }
    return y + 32;
}

static float DrawPacing(float x, float y, float w) {
    Matchup mu = ClassifyMatchup();
    if (mu.aiVsAi) {
        GuiLabel(Rectangle{ x, y, 52, 22 }, "Speed");
        float sbw = 38;
        Rectangle slowBtn = { x + 58, y, sbw, 22 }, fastBtn = { x + w - sbw, y, sbw, 22 };
        if (GuiButton(slowBtn, "")) { if (g_speedIndex > 0) g_speedIndex--; }
        DrawSpeedGlyph(slowBtn, false);
        if (GuiButton(fastBtn, "")) { if (g_speedIndex < 4) g_speedIndex++; }
        DrawSpeedGlyph(fastBtn, true);
        const char *sn = SPEED_NAME[g_speedIndex];
        float nameX = slowBtn.x + sbw + 4, nameW = fastBtn.x - 4 - nameX;
        DrawText(sn, (int)(nameX + (nameW - MeasureText(sn, 16)) / 2), (int)(y + 3), 16, COL_LABEL);
        y += 28;
        float bw3 = (w - 8) / 3.0f;
        GuiToggle(Rectangle{ x, y, bw3, 26 }, g_paused ? "#131#" : "#132#", &g_paused);
        if (GuiButton(Rectangle{ x + bw3 + 4, y, bw3, 26 }, "#134#")) g_stepRequested = true;
        if (GuiButton(Rectangle{ x + 2 * (bw3 + 4), y, bw3, 26 }, "#211#")) StartGame();
        y += 32;
    } else if (mu.humans == 1 && !mu.aiSlow) {
        y = CheckRow(x, y, "Min 2s per agent move", &g_delay2s);
    }
    return y;
}

static void DrawMoveLog(float x, float y, float w, float bottom) {
    static Vector2 scroll = { 0, 0 };
    float h = bottom - y;
    if (h < 60) h = 60;
    float lineH = 18;
    int rows = ((int)g_moves.size() + 1) / 2;
    Rectangle bounds = { x, y, w, h };
    Rectangle content = { 0, 0, w - 16, rows * lineH + 8 };
    Rectangle view;
    GuiScrollPanel(bounds, "Moves", content, &scroll, &view);
    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    for (int r = 0; r < rows; r++) {
        int ty = (int)(view.y + scroll.y + r * lineH + 4);
        std::string s = TextFormat("%3d.  %-5s", r + 1, guiMoveText(g_moves[2 * r]).c_str());
        if (2 * r + 1 < (int)g_moves.size()) s += "  " + guiMoveText(g_moves[2 * r + 1]);
        DrawText(s.c_str(), (int)view.x + 6, ty, 14, COL_LABEL);
    }
    EndScissorMode();
}

static void DrawPlayTab(float x, float y, float w) {
    static bool boardEdit = false;
    static int boardSel = 0;
    static std::vector<std::string> files;
    static std::string opts;
    if (files.empty()) { files = libBoardFiles(); opts = BoardOptions(files); }

    y = DrawSideCard(0, x, y, w);
    if (GuiButton(Rectangle{ x + w / 2 - 50, y - 4, 100, 20 }, "#74# Swap")) {
        std::swap(g_white, g_black);
        if (g_state != AppState::GameOver && g_state != AppState::ComputingAI) NextTurnState();
    }
    y += 22;
    y = DrawSideCard(1, x, y, w);

    y = SectionTitle(x, y, w, "Game");
    for (size_t i = 0; i < files.size(); i++) if (files[i] == g_boardFile) boardSel = (int)i;
    GuiLabel(Rectangle{ x, y, 48, 24 }, "Board");
    Rectangle boardRect = { x + 52, y, w - 52, 24 };
    int prevSel = boardSel;
    DeferDropdown(boardRect, opts, &boardSel, &boardEdit);
    y += 30;
    if (boardSel != prevSel && boardSel >= 0 && boardSel < (int)files.size()) {
        std::strncpy(g_boardFile, files[boardSel].c_str(), sizeof(g_boardFile) - 1);
    }
    bool boardChanged = g_gameBoardFile != g_boardFile;
    float bw2 = (w - 6) / 2.0f;
    if (GuiButton(Rectangle{ x, y, bw2, 28 }, boardChanged ? "#211# New Game *" : "#211# New Game")) StartGame();
    if (GuiButton(Rectangle{ x + bw2 + 6, y, bw2, 28 }, "#72# Undo")) Undo();
    y += 34;
    if (boardChanged) { DrawText("New Game loads the selected board.", (int)x, (int)y, 12, COL_ACCENT); y += 16; }
    y = DrawPacing(x, y, w);

    Color sc = g_statusErr ? COL_ERR : COL_LABEL;
    y = DrawWrapped(g_status, x, y + 2, w, 13, sc) + 4;
    DrawMoveLog(x, y, w, (float)GetScreenHeight() - 10);
}

// ============================================================
// PANEL: ANALYSIS
// ============================================================
static std::string EvalOptions() {
    std::string s;
    for (int i = 0; i < g_evalCount; i++) { if (i) s += ";"; s += g_evaluators[i].name; }
    return s;
}

static void OpenModelPicker(int target);

static void DrawAnalysisTab(float x, float y, float w) {
    static bool evalEdit = false;
    y = CheckRow(x, y, "Analysis on (A)", &g_ana.on);
    y = StepperRow(2, ED_ANA_ARROWS, STEP_SEGMENTS, x, y, w, "Arrows", &g_ana.arrows, 0, 8);
    float half = w / 2;
    CheckRow(x, y, "Reply arrow", &g_ana.reply);
    y = CheckRow(x + half, y, "Scores", &g_ana.labels);
    y = CheckRow(x, y, "Eval bar", &g_ana.evalBar);

    y = SectionTitle(x, y + 4, w, "Evaluator");
    GuiLabel(Rectangle{ x, y, 48, 24 }, "Eval");
    int prevEval = g_ana.evaluator;
    DeferDropdown(Rectangle{ x + 52, y, w - 52, 24 }, EvalOptions(), &g_ana.evaluator, &evalEdit);
    y += 30;
    if (g_ana.evaluator != prevEval) SeedEvalDefaults(g_ana.evaluator, g_ana.params);
    float bw2 = (w - 6) / 2.0f;
    if (GuiButton(Rectangle{ x, y, bw2, 22 }, "Use White's")) AnalysisUseAgentEvaluator(g_white);
    if (GuiButton(Rectangle{ x + bw2 + 6, y, bw2, 22 }, "Use Black's")) AnalysisUseAgentEvaluator(g_black);
    y += 28;
    if (g_ana.evaluator == learnedValueIndex()) {
        std::string lbl = "#10# Model " + std::to_string(g_ana.modelSlot);
        if (GuiButton(Rectangle{ x, y, w, 24 }, lbl.c_str())) OpenModelPicker(1);
        y += 30;
        y = StepperRow(2, ED_RISK, STEP_BAR_NUM, x, y, w, "Risk", &g_ana.params[1], -50, 50);
    } else {
        y = CheckRow(x, y, "Show weights", &g_anaWeights);
        if (g_anaWeights) {
            const EvalDef &e = g_evaluators[g_ana.evaluator];
            for (int i = 0; i < e.paramCount; i++)
                y = StepperRow(2, ED_PARAMS_BASE + i, STEP_BAR_NUM, x, y, w, e.params[i].name,
                               &g_ana.params[i], e.params[i].lo, e.params[i].hi);
        }
    }

    y = SectionTitle(x, y + 2, w, "Search");
    y = StepperRow(2, ED_ANA_DEPTH, STEP_BAR_NUM, x, y, w, "Depth", &g_ana.maxDepth, 1, 40);
    y = StepperRow(2, ED_ANA_TIME, STEP_NUMBAR, x, y, w, "Seconds", &g_ana.maxSeconds, 0, 3600, 300);
    DrawText("Stops at either limit (0 seconds = no time limit).", (int)x, (int)y - 4, 12, COL_DIM);
    y += 14;
    float third = w / 3;
    CheckRow(x, y, "TT", &g_ana.tt);
    CheckRow(x + third, y, "Order", &g_ana.ord);
    y = CheckRow(x + 2 * third, y, "Quiesce", &g_ana.qs);

    y = SectionTitle(x, y + 4, w, "Lines");
    if (!g_ana.on) { DrawText("Analysis is off.", (int)x, (int)y, 13, COL_DIM); return; }
    if (!g_anaSnapValid) { DrawText("Waiting for the engine...", (int)x, (int)y, 13, COL_DIM); return; }
    const EngAnalysis &s = g_anaSnap;
    std::string head;
    if (s.depth > 0) head = "depth " + std::to_string(s.depth);
    else head = "depth -";
    if (s.working > 0) head += TextFormat("  (d%d %d/%d)", s.working, s.workingDone, s.workingTotal);
    else if (s.finished) head += "  done";
    head += "  " + FormatNodes(s.nodes) + " nodes  " + TextFormat("%.1fs", s.ms / 1000.0);
    DrawText(head.c_str(), (int)x, (int)y, 13, COL_LABEL);
    y += 18;
    if (s.hasStatic) { DrawText(("static " + FormatEval(s.staticEval)).c_str(), (int)x, (int)y, 13, COL_DIM); y += 18; }
    if (!s.error.empty()) { y = DrawWrapped(s.error, x, y, w, 12, COL_ERR); }
    float bottom = (float)GetScreenHeight() - 12;
    for (size_t i = 0; i < s.lines.size() && y + 16 < bottom; i++) {
        const EngLine &L = s.lines[i];
        std::string t = TextFormat("%2d. %-4s %7s", (int)i + 1, guiMoveText(L.move).c_str(), FormatEval(L.score).c_str());
        if (L.reply.valid()) t += "   " + guiMoveText(L.reply);
        Color c = (int)i < g_ana.arrows ? ARROW_COLORS[i < 3 ? i : 3] : COL_DIM;
        c.a = 255;
        DrawText(t.c_str(), (int)x, (int)y, 13, c);
        y += 16;
    }
}

// ============================================================
// PANEL: VIEW
// ============================================================
static std::string ThemeOptions(bool pieces) {
    std::string s;
    int n = pieces ? PIECE_THEME_COUNT : BOARD_THEME_COUNT;
    for (int i = 0; i < n; i++) { if (i) s += ";"; s += pieces ? PIECE_THEMES[i].name : BOARD_THEMES[i].name; }
    return s;
}

static void DrawViewTab(float x, float y, float w) {
    GuiLabel(Rectangle{ x, y, 60, 24 }, "Pieces");
    GuiComboBox(Rectangle{ x + 64, y, w - 64, 24 }, ThemeOptions(true).c_str(), &g_pieceTheme);
    y += 30;
    GuiLabel(Rectangle{ x, y, 60, 24 }, "Board");
    GuiComboBox(Rectangle{ x + 64, y, w - 64, 24 }, ThemeOptions(false).c_str(), &g_boardTheme);
    y += 34;
    y = CheckRow(x, y, "Flip board (F)", &g_flip);
    y = CheckRow(x, y, "Coordinates", &g_coords);
    y = CheckRow(x, y, "Highlight last move", &g_showLast);
    y = CheckRow(x, y, "Show legal moves", &g_showHints);
    y = CheckRow(x, y, "Agent readouts (E)", &g_showReadouts);
    y += 4;
    GuiLabel(Rectangle{ x, y, 60, 22 }, "Sliders");
    GuiComboBox(Rectangle{ x + 64, y, w - 64, 22 }, "Per-row;Bar+number;Segments;Number+bar;Handle;Ruler", &g_stepStyle);
    y += 32;
#if !defined(PLATFORM_WEB)
    bool simple = g_simple;
    y = CheckRow(x, y, "Simple mode (the web layout)", &simple);
    if (simple != g_simple) { g_simple = simple; if (g_simple) ApplySimpleMatchup(); }
#endif
    y = SectionTitle(x, y + 8, w, "Keys");
    const char *keys[] = { "Tab   show / hide the panel", "A     analysis on / off", "F     flip the board",
                           "E     agent readouts", "U     undo (also Ctrl+Z)", "N     new game",
                           "Space pause agent vs agent", "Esc   close a window" };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) { DrawText(keys[i], (int)x, (int)y, 13, COL_DIM); y += 17; }
}

// ============================================================
// PANEL: SIMPLE
// ============================================================
static void DrawSimplePanel(float x, float y, float w) {
    DrawText("Play Breakthrough", (int)x, (int)y, 20, COL_LABEL);
    y += 30;
    y = DrawWrapped("Reach the far row with any piece, or capture every enemy piece. "
                    "Pieces step one row forward, straight or diagonally, and capture diagonally.",
                    x, y, w, 13, COL_DIM) + 8;
    int mode = g_simpleMode;
    GuiToggleGroup(Rectangle{ x, y, (w - 4) / 3.0f - 1, 28 }, "Play White;Play Black;Watch", &mode);
    y += 36;
    int level = g_simpleLevel;
    if (g_simpleMode != 2) {
        GuiToggleGroup(Rectangle{ x, y, (w - 4) / 3.0f - 1, 28 }, "Easy;Medium;Hard", &level);
        y += 34;
        static const char *ROLE[3] = { "easy", "medium", "hard" };
        const LibEntry *e = libPresetByRole(ROLE[g_simpleLevel]);
        if (e) {
            std::string d = e->note;
            size_t c = d.find(": ");
            if (c != std::string::npos) d = d.substr(c + 2);
            y = DrawWrapped(d, x, y, w, 13, COL_DIM) + 6;
        }
    } else {
        y = DrawWrapped("Two agents with randomized openings play each other, so every game differs.",
                        x, y, w, 13, COL_DIM) + 6;
    }
    if (mode != g_simpleMode || level != g_simpleLevel) {
        g_simpleMode = mode;
        g_simpleLevel = level;
        ApplySimpleMatchup();
    }
    float bw2 = (w - 6) / 2.0f;
    if (GuiButton(Rectangle{ x, y, bw2, 30 }, "#211# New Game")) StartGame();
    if (GuiButton(Rectangle{ x + bw2 + 6, y, bw2, 30 }, "#72# Undo")) Undo();
    y += 38;
    bool hints = g_ana.on;
    y = CheckRow(x, y, "Show hints (arrows + eval bar)", &hints);
    g_ana.on = hints;
    y = CheckRow(x, y, "Flip board", &g_flip);
    y = DrawPacing(x, y + 4, w);
    Color sc = g_statusErr ? COL_ERR : COL_LABEL;
    y = DrawWrapped(g_status, x, y + 4, w, 13, sc) + 4;
    DrawMoveLog(x, y, w, (float)GetScreenHeight() - 10);
}

static void DrawPanel() {
    DrawRectangleRec(g_panelRect, COL_PANEL);
    DrawLineEx(Vector2{ g_panelRect.width, g_panelRect.y }, Vector2{ g_panelRect.width, g_panelRect.y + g_panelRect.height }, 2, Color{ 60, 64, 76, 255 });
    float x = 12, w = PANEL_W - 24, y = TOP + 10;
    static bool dropWasOpen = false;
    bool lockedHere = false;
    if (dropWasOpen && !GuiIsLocked()) { GuiLock(); lockedHere = true; }
    if (g_simple) {
        DrawSimplePanel(x, y, w);
    } else {
        GuiToggleGroup(Rectangle{ x, y, (w - 4) / 3.0f - 1, 26 }, "Play;Analysis;View", &g_panelTab);
        y += 38;
        if (g_panelTab == 0) DrawPlayTab(x, y, w);
        else if (g_panelTab == 1) DrawAnalysisTab(x, y, w);
        else DrawViewTab(x, y, w);
    }
    if (lockedHere) GuiUnlock();
    dropWasOpen = AnyDeferredOpen();
    FlushDropdowns();
}

// ============================================================
// AGENT EDITOR
// ============================================================
static const std::string &OpenerOptions() {
    static std::string s;
    if (s.empty()) { s = "None"; for (int i = 0; i < g_openerCount; i++) { s += ";"; s += g_openers[i].name; } }
    return s;
}
static const std::string &ExplorerOptions() {
    static std::string s;
    if (s.empty()) for (int i = 0; i < g_explorerCount; i++) { if (i) s += ";"; s += g_explorers[i].name; }
    return s;
}
static const std::string &ChooserOptions() {
    static std::string s;
    if (s.empty()) for (int i = 0; i < g_chooserCount; i++) { if (i) s += ";"; s += g_choosers[i].name; }
    return s;
}

static void EditorSyncScratch() {
    const AgentSpec &s = g_ed.working;
    g_ed.dilute = s.randomMoveProb > 0.0;
    g_ed.dilPct = (int)(s.randomMoveProb * 100.0 + 0.5);
    g_ed.nodeBudgetK = (int)(s.nodeBudget / 1000ULL);
    g_ed.timeBudgetInt = (int)s.timeBudgetMs;
    g_ed.openerSel = (s.openerKind >= 0 && s.openerKind < g_openerCount) ? s.openerKind + 1 : 0;
    g_ed.seededForEval = s.evaluator;
}

// Node budgets are edited in thousands (the id grammar writes 200k, 2m).
static void EditorPushScratch() {
    AgentSpec &s = g_ed.working;
    s.randomMoveProb = g_ed.dilute ? (double)g_ed.dilPct / 100.0 : 0.0;
    unsigned long long cur = s.nodeBudget;
    if ((int)(cur / 1000ULL) != g_ed.nodeBudgetK) s.nodeBudget = (unsigned long long)(g_ed.nodeBudgetK < 0 ? 0 : g_ed.nodeBudgetK) * 1000ULL;
    s.timeBudgetMs = (double)(g_ed.timeBudgetInt < 0 ? 0 : g_ed.timeBudgetInt);
    s.openerKind = (g_ed.openerSel <= 0) ? -1 : g_ed.openerSel - 1;
}

static void EditorSetIdText(const std::string &s) {
    std::strncpy(g_ed.idBuf, s.c_str(), sizeof(g_ed.idBuf) - 1);
    g_ed.idBuf[sizeof(g_ed.idBuf) - 1] = '\0';
}

static bool EditorTryIdText(const std::string &text) {
    RankAgent out;
    std::string err;
    if (!rankAgentFromId(text, out, err)) { g_ed.idError = err; return false; }
    g_ed.working = out.spec;
    g_ed.idError.clear();
    EditorSyncScratch();
    EditorSetIdText(rankAgentId(g_ed.working));
    return true;
}

static void OpenAgentEditor(int side, const AgentSpec *from) {
    PlayerConfig &c = (side == 0) ? g_white : g_black;
    g_ed.side = side;
    if (from) g_ed.working = *from;
    else if (!c.isHuman) g_ed.working = c.spec;
    else {
        PlayerConfig tmp;
        SetFallbackAgent(tmp);
        g_ed.working = tmp.spec;
    }
    g_ed.idError.clear();
    g_ed.idEdit = false;
    g_ed.editExplorer = g_ed.editChooser = g_ed.editEval = g_ed.editOpener = false;
    g_ed.histActive = -1;
    g_ed.histScroll = 0;
    g_ed.scroll = Vector2{ 0, 0 };
    EditorSyncScratch();
    EditorSetIdText(rankAgentId(g_ed.working));
}

static void DrawAgentEditor() {
    if (g_ed.side < 0) return;
    PlayerConfig &c = (g_ed.side == 0) ? g_white : g_black;
    float pw = 680, ph = 680;
    if (pw > GetScreenWidth() - 20) pw = (float)GetScreenWidth() - 20;
    if (ph > GetScreenHeight() - 20) ph = (float)GetScreenHeight() - 20;
    Rectangle win = { (GetScreenWidth() - pw) / 2.0f, (GetScreenHeight() - ph) / 2.0f, pw, ph };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 120 });

    bool lockAll = g_mp.open;
    if (lockAll) GuiLock();
    static bool dropWasOpen = false;
    bool lockedHere = false;
    if (!lockAll && dropWasOpen) { GuiLock(); lockedHere = true; }

    if (GuiWindowBox(win, g_ed.side == 0 ? "Edit agent: White" : "Edit agent: Black")) {
        g_ed.side = -1;
        if (lockedHere || lockAll) GuiUnlock();
        g_drops.clear();
        return;
    }
    DrawRectangle((int)win.x + 1, (int)win.y + 24, (int)win.width - 2, (int)win.height - 25, Color{ 30, 32, 40, 255 });
    float x = win.x + 12, y = win.y + 34, w = win.width - 24;

    // Canonical id box: typing and leaving the box (or Enter) parses it.
    DrawText("Canonical id (type or paste; the fields below follow it)", (int)x, (int)y, 12, COL_DIM);
    y += 15;
    bool wasEditing = g_ed.idEdit;
    if (GuiTextBox(Rectangle{ x, y, w, 26 }, g_ed.idBuf, sizeof(g_ed.idBuf), g_ed.idEdit)) g_ed.idEdit = !g_ed.idEdit;
    if (g_ed.idEdit) g_anyTextEdit = true;
    if (wasEditing && !g_ed.idEdit) EditorTryIdText(g_ed.idBuf);
    y += 30;
    if (!g_ed.idError.empty()) y = DrawWrapped(g_ed.idError, x, y, w, 12, COL_ERR);
    else { DrawText(FitText(libShortName(g_ed.idBuf), w, 13).c_str(), (int)x, (int)y, 13, COL_LABEL); y += 17; }
    y += 2;

    // Brain + opener.
    GuiLabel(Rectangle{ x, y, 42, 24 }, "Brain");
    int brainSel = g_ed.working.brain;
    GuiToggleGroup(Rectangle{ x + 46, y, 78, 24 }, "Search;Policy", &brainSel);
    if (brainSel != g_ed.working.brain) g_ed.working.brain = brainSel;
    GuiLabel(Rectangle{ x + 232, y, 54, 24 }, "Opener");
    DeferDropdown(Rectangle{ x + 290, y, w - 290, 24 }, OpenerOptions(), &g_ed.openerSel, &g_ed.editOpener);
    y += 30;
    if (g_ed.working.brain == BRAIN_SEARCH) {
        GuiLabel(Rectangle{ x, y, 52, 24 }, "Search");
        DeferDropdown(Rectangle{ x + 56, y, 150, 24 }, ExplorerOptions(), &g_ed.working.explorer, &g_ed.editExplorer);
        GuiLabel(Rectangle{ x + 232, y, 40, 24 }, "Eval");
        DeferDropdown(Rectangle{ x + 290, y, w - 290, 24 }, EvalOptions(), &g_ed.working.evaluator, &g_ed.editEval);
    } else {
        GuiLabel(Rectangle{ x, y, 52, 24 }, "Policy");
        DeferDropdown(Rectangle{ x + 56, y, w - 56, 24 }, ChooserOptions(), &g_ed.working.chooser, &g_ed.editChooser);
    }
    y += 34;

    // Scrollable body.
    Rectangle body = { x, y, w, (win.y + win.height - 54) - y };
    Rectangle content = { 0, 0, w - 16, g_ed.contentH };
    Rectangle view;
    GuiScrollPanel(body, NULL, content, &g_ed.scroll, &view);
    BeginScissorMode((int)view.x, (int)view.y, (int)view.width, (int)view.height);
    float cx = body.x + 6, cy = body.y + g_ed.scroll.y + 6, cw = body.width - 24, cy0 = cy;
    int side = g_ed.side;
    AgentSpec &s = g_ed.working;
    if (s.explorer < 0 || s.explorer >= g_explorerCount) s.explorer = 0;
    if (s.chooser < 0 || s.chooser >= g_chooserCount) s.chooser = 0;
    if (s.evaluator < 0 || s.evaluator >= g_evalCount) s.evaluator = 0;

    if (s.brain == BRAIN_SEARCH) {
        bool isAB = IsAlphaBeta(s.explorer), isGaz = IsGumbel(s.explorer);
        cy = StepperRow(side, ED_DEPTH, STEP_NUMBAR, cx, cy, cw, isGaz ? "Sims" : "Depth", &s.depth, 1, 1000000, isGaz ? 2000 : 25);
        if (isAB) {
            GuiLabel(Rectangle{ cx, cy, 46, 20 }, "Flags");
            float ckx = cx + 64, ckw = (cw - 64) / 6.0f;
            bool noAB = !s.useAlphaBeta;
            const char *names[6] = { "noab", "tt", "ord", "qs", "part", "retain" };
            bool *vals[6] = { &noAB, &s.useTT, &s.useMoveOrder, &s.useQuiescence, &s.keepPartial, &s.retainBudget };
            for (int i = 0; i < 6; i++) GuiCheckBox(Rectangle{ ckx + i * ckw, cy + 2, 16, 16 }, names[i], vals[i]);
            s.useAlphaBeta = !noAB;
            cy += 28;
            cy = StepperRow(side, ED_ASPIRATION, STEP_BAR_NUM, cx, cy, cw, "Margin", &s.aspirationWindow, 0, 500);
            cy = StepperRow(side, ED_NODES, STEP_NUMBAR, cx, cy, cw, "Nodes (k)", &g_ed.nodeBudgetK, 0, 100000, 2000);
            cy = StepperRow(side, ED_TIME, STEP_NUMBAR, cx, cy, cw, "Time ms", &g_ed.timeBudgetInt, 0, 600000, 1000);
            cy = StepperRow(side, ED_REM, STEP_BAR_NUM, cx, cy, cw, "Rem %", &s.iterMinRemain, 0, 99);
            cy = StepperRow(side, ED_DEPTHCAP, STEP_BAR_NUM, cx, cy, cw, "Max deep", &s.depthCap, 0, 100);
        }
        if (isGaz) {
            cy = StepperRow(side, ED_GCVISIT, STEP_NUMBAR, cx, cy, cw, "c_visit", &s.gumbelCVisit, 1, 5000, 1000);
            cy = StepperRow(side, ED_GCSCALE, STEP_NUMBAR, cx, cy, cw, "c_scale/10", &s.gumbelCScaleTenths, 1, 1000, 200);
            cy = StepperRow(side, ED_GM, STEP_BAR_NUM, cx, cy, cw, "Root m", &s.gumbelRootM, 2, 64);
        }
        if (s.evaluator != g_ed.seededForEval) {
            SeedEvalDefaults(s.evaluator, s.evalParams);
            g_ed.seededForEval = s.evaluator;
        }
        if (s.evaluator == learnedValueIndex()) {
            cy = StepperRow(side, ED_MODELSLOT, STEP_NUMBAR, cx, cy, cw, "Model", &s.modelSlot, 0, 4095, 2000);
            if (GuiButton(Rectangle{ cx + 64, cy - 4, 160, 22 }, "#10# Pick model...")) OpenModelPicker(0);
            cy += 26;
            cy = StepperRow(side, ED_RISK, STEP_BAR_NUM, cx, cy, cw, "Risk", &s.evalParams[1], -50, 50);
        } else {
            const EvalDef &e = g_evaluators[s.evaluator];
            for (int i = 0; i < e.paramCount; i++)
                cy = StepperRow(side, ED_PARAMS_BASE + i, (StepStyle)(i % STEP_STYLE_COUNT), cx, cy, cw,
                                e.params[i].name, &s.evalParams[i], e.params[i].lo, e.params[i].hi);
        }
    } else {
        int smartIdx = ChooserIndexByName("SmartRandom"), policyIdx = ChooserIndexByName("LearnedPolicy");
        if (s.chooser == smartIdx)
            cy = StepperRow(side, ED_MODELSLOT, STEP_SEGMENTS, cx, cy, cw, "Pieces", &s.chooserParam, 1, 16);
        else if (s.chooser == policyIdx) {
            cy = StepperRow(side, ED_MODELSLOT, STEP_NUMBAR, cx, cy, cw, "Model", &s.modelSlot, 0, 4095, 2000);
            if (GuiButton(Rectangle{ cx + 64, cy - 4, 160, 22 }, "#10# Pick model...")) OpenModelPicker(0);
            cy += 26;
        }
    }
    GuiCheckBox(Rectangle{ cx, cy + 2, 16, 16 }, "Dilute (random or shallower moves)", &g_ed.dilute);
    cy += 26;
    if (g_ed.dilute) {
        cy = StepperRow(side, ED_DILPCT, STEP_BAR_NUM, cx, cy, cw, "Prob %", &g_ed.dilPct, 1, 100);
        if (s.brain == BRAIN_SEARCH)
            cy = StepperRow(side, ED_DILDEPTH, STEP_BAR_NUM, cx, cy, cw, "Dil deep", &s.dilDepth, 0, 25);
    }
    if (g_ed.openerSel > 0) {
        int oi = g_ed.openerSel - 1;
        if (g_openers[oi].hasArg)
            cy = StepperRow(side, ED_OPENERARG, STEP_BAR_NUM, cx, cy, cw, g_openers[oi].argLabel, &s.openerArg, 0, 200);
        if (g_openers[oi].hasArg2)
            cy = StepperRow(side, ED_OPENERARG2, STEP_BAR_NUM, cx, cy, cw, "ply", &s.openerArg2, 0, 400);
    }
    g_ed.contentH = (cy - cy0) + 20;
    EndScissorMode();

    EditorPushScratch();
    if (!g_ed.idEdit) EditorSetIdText(rankAgentId(g_ed.working));

    // Footer.
    float footY = win.y + win.height - 44;
    float bw3 = (w - 12) / 3.0f;
    if (GuiButton(Rectangle{ x, footY, bw3, 32 }, "#112# Apply")) {
        std::string err;
        if (SetPlayerFromId(c, g_ed.idBuf, "", err)) {
            SetStatus(std::string(SideName(g_ed.side == 0 ? White : Black)) + " is now " + PlayerName(c) + ".");
            g_ed.side = -1;
            if (g_state != AppState::GameOver && g_state != AppState::ComputingAI) NextTurnState();
        } else g_ed.idError = err;
    }
    if (GuiButton(Rectangle{ x + bw3 + 6, footY, bw3, 32 }, "#17# Library...")) { int sd = g_ed.side; g_ed.side = -1; OpenLibrary(sd); }
    if (GuiButton(Rectangle{ x + 2 * (bw3 + 6), footY, bw3, 32 }, "#113# Cancel")) g_ed.side = -1;

    if (lockedHere) GuiUnlock();
    dropWasOpen = AnyDeferredOpen();
    FlushDropdowns();
    if (lockAll) GuiUnlock();
}

// ============================================================
// LIBRARY
// ============================================================
static void OpenLibrary(int side) {
    libLoad();
    g_lib.open = true;
    g_lib.side = side;
    g_lib.active = -1;
    g_lib.scroll = 0;
    g_lib.parsedId.clear();
}

struct LibRow { std::string text; const LibEntry *e; std::string id; };

static std::vector<LibRow> LibraryRows(float width) {
    std::vector<LibRow> rows;
    const char *flt = g_lib.filter;
    int fs = 14;
    auto add = [&](const std::string &prefix, const LibEntry *e, const std::string &id) {
        std::string label = e ? e->label : "";
        std::string text = prefix + libShortName(id);
        if (!ContainsNoCase(id + " " + label + " " + text, flt)) return;
        LibRow r;
        r.text = FitText(text, width - 30, fs);
        r.e = e;
        r.id = id;
        rows.push_back(r);
    };
    if (g_lib.tab == 0) {
        const std::vector<LibEntry> &v = libChampions();
        for (size_t i = 0; i < v.size(); i++)
            add(v[i].label + "  " + (v[i].hasElo ? std::to_string(v[i].elo) : std::string("----")) + "  ", &v[i], v[i].id);
    } else if (g_lib.tab == 1) {
        const std::vector<LibEntry> &v = libPresets();
        for (size_t i = 0; i < v.size(); i++)
            add(v[i].label + "  " + (v[i].hasElo ? std::to_string(v[i].elo) : std::string("----")) + "  ", &v[i], v[i].id);
    } else if (g_lib.tab == 2) {
        std::vector<std::string> heads = libHeads();
        std::string want = (g_lib.head > 0 && g_lib.head <= (int)heads.size()) ? heads[g_lib.head - 1] : "";
        const std::vector<LibEntry> &v = libStandings();
        for (size_t i = 0; i < v.size(); i++) {
            if (!want.empty() && v[i].head != want) continue;
            add(TextFormat("%5d  ", v[i].elo), &v[i], v[i].id);
        }
    } else if (g_lib.tab == 3) {
        const std::vector<LibEntry> &v = libFavorites();
        for (size_t i = 0; i < v.size(); i++) add(v[i].label + "  ", &v[i], v[i].id);
    } else {
        const std::vector<std::string> &v = libHistory();
        for (size_t i = 0; i < v.size(); i++) add("", libStandingsFor(v[i]), v[i]);
    }
    return rows;
}

static void DrawLibrary() {
    if (!g_lib.open) return;
    float pw = std::min(1120.0f, (float)GetScreenWidth() - 20), ph = std::min(720.0f, (float)GetScreenHeight() - 20);
    Rectangle win = { (GetScreenWidth() - pw) / 2.0f, (GetScreenHeight() - ph) / 2.0f, pw, ph };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 120 });
    bool lockAll = g_saveFavOpen;
    if (lockAll) GuiLock();
    static bool dropWasOpen = false;
    bool lockedHere = false;
    if (!lockAll && dropWasOpen) { GuiLock(); lockedHere = true; }

    std::string title = std::string("Agent library: choosing for ") + (g_lib.side == 0 ? "White" : "Black");
    if (GuiWindowBox(win, title.c_str())) {
        g_lib.open = false;
        if (lockedHere || lockAll) GuiUnlock();
        g_drops.clear();
        return;
    }
    DrawRectangle((int)win.x + 1, (int)win.y + 24, (int)win.width - 2, (int)win.height - 25, Color{ 30, 32, 40, 255 });
    float x = win.x + 12, y = win.y + 34, w = win.width - 24;

    int prevTab = g_lib.tab;
    GuiToggleGroup(Rectangle{ x, y, 120, 26 }, "#153# Champions;#157# Presets;#95# Standings;#186# Favorites;#184# Recent", &g_lib.tab);
    if (g_lib.tab != prevTab) { g_lib.active = -1; g_lib.scroll = 0; }
    y += 34;

    float listW = std::floor(w * 0.56f), detX = x + listW + 14, detW = w - listW - 14;
    GuiLabel(Rectangle{ x, y, 40, 24 }, "Filter");
    Rectangle filterR = { x + 44, y, (g_lib.tab == 2 ? listW * 0.45f : listW) - 44, 24 };
    if (GuiTextBox(filterR, g_lib.filter, sizeof(g_lib.filter), g_lib.filterEdit)) g_lib.filterEdit = !g_lib.filterEdit;
    if (g_lib.filterEdit) g_anyTextEdit = true;
    if (g_lib.tab == 2) {
        std::vector<std::string> heads = libHeads();
        std::string opts = "All heads";
        for (size_t i = 0; i < heads.size(); i++) opts += ";" + FitText(heads[i], listW * 0.55f - 30, 14);
        DeferDropdown(Rectangle{ x + listW * 0.45f + 6, y, listW * 0.55f - 6, 24 }, opts, &g_lib.head, &g_lib.headEdit);
    }
    y += 30;

    // List.
    float listH = win.y + win.height - 12 - y;
    std::vector<LibRow> rows = LibraryRows(listW);
    std::vector<const char *> ptr;
    for (size_t i = 0; i < rows.size(); i++) ptr.push_back(rows[i].text.c_str());
    int oldSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    int oldAlign = GuiGetStyle(LISTVIEW, TEXT_ALIGNMENT);
    GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiListViewEx(Rectangle{ x, y, listW, listH }, ptr.empty() ? nullptr : ptr.data(), (int)ptr.size(),
                  &g_lib.scroll, &g_lib.active, &g_lib.focus);
    GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, oldAlign);
    GuiSetStyle(DEFAULT, TEXT_SIZE, oldSize);
    if (rows.empty()) {
        std::string empty = g_lib.tab == 0 ? std::string("No champions parsed from ranking/CHAMPION.md.")
                          : g_lib.tab == 2 ? libStandingsNote()
                          : g_lib.tab == 3 ? std::string("No favorites yet: use Save on a player card.")
                          : std::string("Nothing here yet.");
        DrawWrapped(empty, x + 10, y + 10, listW - 20, 13, COL_DIM);
    }

    // Details.
    float dy = y;
    if (g_lib.tab == 2) { DrawWrapped(libStandingsNote(), detX, dy, detW, 12, COL_DIM); dy += 34; }
    if (g_lib.active >= 0 && g_lib.active < (int)rows.size()) {
        const LibRow &r = rows[g_lib.active];
        if (g_lib.parsedId != r.id) {
            g_lib.parsedId = r.id;
            RankAgent a;
            g_lib.parseOk = rankAgentFromId(r.id, a, g_lib.parseErr);
            if (g_lib.parseOk) g_lib.parsed = a.spec;
            std::string nm = (r.e && !r.e->label.empty()) ? r.e->label : libShortName(r.id);
            std::strncpy(g_lib.favName, nm.substr(0, 60).c_str(), sizeof(g_lib.favName) - 1);
        }
        std::string nm = (r.e && !r.e->label.empty()) ? r.e->label : "";
        if (!nm.empty()) { DrawText(FitText(nm, detW, 18).c_str(), (int)detX, (int)dy, 18, COL_NUM); dy += 24; }
        dy = DrawWrapped(libShortName(r.id), detX, dy, detW, 14, COL_LABEL) + 4;
        DrawText("Canonical id", (int)detX, (int)dy, 12, COL_DIM);
        dy += 15;
        dy = DrawWrapped(r.id, detX, dy, detW, 12, COL_LABEL) + 6;
        const LibEntry *st = libStandingsFor(r.id);
        if (st) {
            DrawText(TextFormat("Elo %d +/- %d   %d games   %.1f ms/move", st->elo, st->pm, st->games, st->cpuMs),
                     (int)detX, (int)dy, 13, COL_NUM);
            dy += 17;
            dy = DrawWrapped("head " + st->head + " (compare Elo only within one head)", detX, dy, detW, 12, COL_DIM) + 4;
        } else {
            DrawText("Not in the current standings.", (int)detX, (int)dy, 13, COL_DIM);
            dy += 18;
        }
        if (r.e && !r.e->note.empty() && g_lib.tab != 2) dy = DrawWrapped(r.e->note, detX, dy, detW, 12, COL_DIM) + 4;
        if (!g_lib.parseOk) dy = DrawWrapped("Cannot load: " + g_lib.parseErr, detX, dy, detW, 12, COL_ERR) + 4;
        dy += 6;

        float bw2 = (detW - 6) / 2.0f;
        if (!g_lib.parseOk) GuiDisable();
        std::string label = (r.e && !r.e->label.empty() && g_lib.tab != 2 && g_lib.tab != 4) ? r.e->label : "";
        if (GuiButton(Rectangle{ detX, dy, bw2, 30 }, "#149# Play as White")) {
            std::string err;
            if (SetPlayerFromId(g_white, r.id, label, err)) { g_lib.open = false; SetStatus("White is now " + PlayerName(g_white) + "."); }
            else SetStatus(err, true);
            if (g_state != AppState::GameOver && g_state != AppState::ComputingAI) NextTurnState();
        }
        if (GuiButton(Rectangle{ detX + bw2 + 6, dy, bw2, 30 }, "#149# Play as Black")) {
            std::string err;
            if (SetPlayerFromId(g_black, r.id, label, err)) { g_lib.open = false; SetStatus("Black is now " + PlayerName(g_black) + "."); }
            else SetStatus(err, true);
            if (g_state != AppState::GameOver && g_state != AppState::ComputingAI) NextTurnState();
        }
        dy += 36;
        if (GuiButton(Rectangle{ detX, dy, bw2, 26 }, "#140# Edit a copy...")) {
            AgentSpec sp = g_lib.parsed;
            int sd = g_lib.side;
            g_lib.open = false;
            OpenAgentEditor(sd, &sp);
        }
        if (GuiButton(Rectangle{ detX + bw2 + 6, dy, bw2, 26 }, "#92# Analyse with it")) {
            if (g_lib.parsed.brain == BRAIN_SEARCH) {
                PlayerConfig tmp;
                tmp.isHuman = false; tmp.spec = g_lib.parsed; tmp.id = r.id;
                AnalysisUseAgentEvaluator(tmp);
                g_panelTab = 1;
            } else SetStatus("A policy agent has no evaluator to analyse with.", true);
        }
        GuiEnable();
        dy += 34;
        dy = SectionTitle(detX, dy, detW, "Favorite");
        if (GuiTextBox(Rectangle{ detX, dy, detW - 96, 24 }, g_lib.favName, sizeof(g_lib.favName), g_lib.favEdit)) g_lib.favEdit = !g_lib.favEdit;
        if (g_lib.favEdit) g_anyTextEdit = true;
        if (GuiButton(Rectangle{ detX + detW - 90, dy, 90, 24 }, "#186# Save")) {
            libAddFavorite(g_lib.favName, r.id);
            SetStatus(std::string("Saved favorite '") + g_lib.favName + "'.");
        }
        dy += 30;
        if (g_lib.tab == 3 && GuiButton(Rectangle{ detX, dy, detW, 24 }, "#143# Remove from favorites")) {
            const std::vector<LibEntry> &fv = libFavorites();
            for (size_t i = 0; i < fv.size(); i++) if (fv[i].id == r.id) { libRemoveFavorite(i); break; }
            g_lib.active = -1;
        }
    } else {
        DrawWrapped("Select an agent to see its details.", detX, dy, detW, 13, COL_DIM);
    }

    if (lockedHere) GuiUnlock();
    dropWasOpen = AnyDeferredOpen();
    FlushDropdowns();
    if (lockAll) GuiUnlock();
}

static void DrawSaveFavorite() {
    if (!g_saveFavOpen) return;
    PlayerConfig &c = (g_saveFavSide == 0) ? g_white : g_black;
    Rectangle r = { GetScreenWidth() / 2.0f - 200, GetScreenHeight() / 2.0f - 80, 400, 160 };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 100 });
    int res = GuiTextInputBox(r, "Save agent as favorite", "Name:", "Save;Cancel", g_saveFavName, sizeof(g_saveFavName), NULL);
    g_anyTextEdit = true;
    if (res == 1) {
        if (!c.isHuman) { libAddFavorite(g_saveFavName, c.id); c.label = g_saveFavName; SetStatus(std::string("Saved favorite '") + g_saveFavName + "'."); }
        g_saveFavOpen = false;
    } else if (res == 0 || res == 2) {
        g_saveFavOpen = false;
    }
}

// ============================================================
// MODEL PICKER
// ============================================================
static void OpenModelPicker(int target) {
    libStartModelScan();
    g_mp.open = true;
    g_mp.target = target;
    g_mp.active = -1;
}

static void DrawModelPicker() {
    if (!g_mp.open) return;
    float pw = std::min(960.0f, (float)GetScreenWidth() - 40), ph = std::min(640.0f, (float)GetScreenHeight() - 40);
    Rectangle win = { (GetScreenWidth() - pw) / 2.0f, (GetScreenHeight() - ph) / 2.0f, pw, ph };
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 0, 0, 0, 100 });
    if (GuiWindowBox(win, "Pick a model")) { g_mp.open = false; return; }
    DrawRectangle((int)win.x + 1, (int)win.y + 24, (int)win.width - 2, (int)win.height - 25, Color{ 30, 32, 40, 255 });
    float x = win.x + 12, y = win.y + 34, w = win.width - 24;
    if (!libModelScanDone()) {
        DrawText(TextFormat("Scanning model slot files... %d / 4096", libModelScanProgress()), (int)x, (int)y, 16, COL_LABEL);
        return;
    }
    GuiLabel(Rectangle{ x, y, 40, 24 }, "Filter");
    if (GuiTextBox(Rectangle{ x + 44, y, 300, 24 }, g_mp.filter, sizeof(g_mp.filter), g_mp.filterEdit)) g_mp.filterEdit = !g_mp.filterEdit;
    if (g_mp.filterEdit) g_anyTextEdit = true;
    GuiCheckBox(Rectangle{ x + 360, y + 4, 16, 16 }, "Rated models only", &g_mp.ratedOnly);
    y += 32;
    DrawText(" slot  type      feat   best Elo   teacher", (int)x, (int)y, 13, COL_DIM);
    y += 18;
    // Rows are rebuilt only when the filter, the rated-only switch, the catalog
    // or the width changes: there are thousands of model files.
    const std::vector<LibModel> &ms = libModels();
    static std::vector<std::string> rows;
    static std::vector<int> slots;
    static std::string cacheKey;
    std::string key = std::string(g_mp.filter) + "|" + (g_mp.ratedOnly ? "1" : "0") + "|" +
                      std::to_string(ms.size()) + "|" + std::to_string((int)w);
    if (key != cacheKey) {
        cacheKey = key;
        rows.clear();
        slots.clear();
        for (size_t i = 0; i < ms.size(); i++) {
            const LibModel &m = ms[i];
            if (g_mp.ratedOnly && m.bestElo < 0) continue;
            char elo[16];
            if (m.bestElo >= 0) std::snprintf(elo, sizeof(elo), "%6d", m.bestElo);
            else std::snprintf(elo, sizeof(elo), "     -");
            char head[96];
            std::snprintf(head, sizeof(head), "%5d  %-8s  %4s   %s   ", m.slot, m.type.c_str(), m.features.c_str(), elo);
            std::string t = std::string(head) + m.teacher;
            if (!ContainsNoCase(t + " " + m.bestId, g_mp.filter)) continue;
            rows.push_back(FitText(t, w * 0.62f - 30, 14));
            slots.push_back((int)i);
        }
        if (g_mp.active >= (int)rows.size()) g_mp.active = -1;
    }
    std::vector<const char *> ptr;
    for (size_t i = 0; i < rows.size(); i++) ptr.push_back(rows[i].c_str());
    float listW = std::floor(w * 0.62f), listH = win.y + win.height - 12 - y;
    int oldSize = GuiGetStyle(DEFAULT, TEXT_SIZE);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    int oldAlign = GuiGetStyle(LISTVIEW, TEXT_ALIGNMENT);
    GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
    GuiListViewEx(Rectangle{ x, y, listW, listH }, ptr.empty() ? nullptr : ptr.data(), (int)ptr.size(), &g_mp.scroll, &g_mp.active, &g_mp.focus);
    GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, oldAlign);
    GuiSetStyle(DEFAULT, TEXT_SIZE, oldSize);

    float detX = x + listW + 14, detW = w - listW - 14, dy = y;
    DrawText(TextFormat("%d model files found", (int)ms.size()), (int)detX, (int)dy, 13, COL_DIM);
    dy += 22;
    if (g_mp.active >= 0 && g_mp.active < (int)slots.size()) {
        const LibModel &m = ms[slots[g_mp.active]];
        DrawText(TextFormat("Slot %d", m.slot), (int)detX, (int)dy, 18, COL_NUM);
        dy += 24;
        dy = DrawWrapped(m.file, detX, dy, detW, 12, COL_LABEL);
        dy = DrawWrapped(TextFormat("%s, %s features, %.1f KB", m.type.c_str(), m.features.empty() ? "?" : m.features.c_str(), m.bytes / 1024.0),
                         detX, dy, detW, 12, COL_LABEL) + 4;
        dy = DrawWrapped("teacher: " + m.teacher, detX, dy, detW, 12, COL_DIM) + 4;
        if (m.bestElo >= 0) {
            dy = DrawWrapped(TextFormat("best rated agent: Elo %d", m.bestElo), detX, dy, detW, 13, COL_NUM);
            dy = DrawWrapped(libShortName(m.bestId), detX, dy, detW, 12, COL_DIM) + 4;
        } else dy = DrawWrapped("no rated agent uses this slot", detX, dy, detW, 12, COL_DIM) + 4;
        bool joint = (m.type == "joint");
        if (joint) dy = DrawWrapped("A joint model is for the GumbelMCTS explorer.", detX, dy, detW, 12, COL_ACCENT) + 4;
        if (GuiButton(Rectangle{ detX, dy + 6, detW, 30 }, "#112# Use this model")) {
            if (g_mp.target == 0 && g_ed.side >= 0) g_ed.working.modelSlot = m.slot;
            else if (g_mp.target == 1) { g_ana.modelSlot = m.slot; g_ana.evaluator = learnedValueIndex(); }
            g_mp.open = false;
        }
    }
}

// ============================================================
// MAIN LOOP
// ============================================================
static RenderTexture2D g_capRT;
static int g_frame = 0;

static void DrawAll() {
    ClearBackground(COL_BG);
    DrawEvalBar();
    DrawBoard();
    DrawBadges();
    DrawGameOverBanner();
    bool modal = AnyModalOpen();
    if (g_showPanel) {
        if (modal) GuiLock();
        DrawPanel();
        if (modal) GuiUnlock();
    }
    if (modal) GuiLock();
    DrawTopBar();
    if (modal) GuiUnlock();
    DrawAgentEditor();
    DrawLibrary();
    DrawSaveFavorite();
    DrawModelPicker();
}

static double g_lastWorkMs = 0.0;      // this frame's update + draw time, before the frame-rate wait

static void UpdateDrawFrame() {
    double t0 = GetTime();
    g_anyTextEdit = g_lib.filterEdit || g_lib.favEdit || g_ed.idEdit || g_mp.filterEdit || g_saveFavOpen;
    ComputeLayout();
    Update();
    BeginDrawing();
    if (g_cap.on) {
        BeginTextureMode(g_capRT);
        DrawAll();
        EndTextureMode();
    } else {
        DrawAll();
    }
    g_lastWorkMs = (GetTime() - t0) * 1000.0;
    EndDrawing();
    g_frame++;
}

// raygui's built-in style is light. This dark palette matches the app's own
// panel colors, so labels, lists and scroll panels drawn by raygui sit on the
// same ground as the text the app draws itself. DEFAULT base properties apply
// to every control.
static void ApplyDarkStyle() {
    struct { int prop; unsigned rgba; } base[] = {
        { BORDER_COLOR_NORMAL,   0x5c606eff }, { BASE_COLOR_NORMAL,   0x2e313cff }, { TEXT_COLOR_NORMAL,   0xd2d4dcff },
        { BORDER_COLOR_FOCUSED,  0x8ab4e8ff }, { BASE_COLOR_FOCUSED,  0x3a4152ff }, { TEXT_COLOR_FOCUSED,  0xf0f2f8ff },
        { BORDER_COLOR_PRESSED,  0x569edeff }, { BASE_COLOR_PRESSED,  0x2d5a86ff }, { TEXT_COLOR_PRESSED,  0xffffffff },
        { BORDER_COLOR_DISABLED, 0x454854ff }, { BASE_COLOR_DISABLED, 0x262830ff }, { TEXT_COLOR_DISABLED, 0x6a6e7aff },
    };
    for (size_t i = 0; i < sizeof(base) / sizeof(base[0]); i++) GuiSetStyle(DEFAULT, base[i].prop, (int)base[i].rgba);
    GuiSetStyle(DEFAULT, LINE_COLOR, (int)0x5c606eff);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, (int)0x1e2028ff);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);
    GuiSetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(COL_LABEL));
    GuiSetStyle(LISTVIEW, LIST_ITEMS_HEIGHT, 22);
}

// ---- Capture mode (native) ----
// --capture <png>   render into a hidden window and save the last frame
// --frames N        frames to run first (default 120, 2 s at 60 fps)
// --size WxH        window size (default 1280x820)
// --scenario a,b    comma-separated setup steps, see ApplyScenario
// --moves m1,m2     moves to play first, in engine notation (e.g. c1d)
static void ParseArgs(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        std::string v = (i + 1 < argc) ? argv[i + 1] : "";
        if (a == "--capture") { g_cap.on = true; g_cap.out = v; i++; }
        else if (a == "--frames") { g_cap.frames = std::atoi(v.c_str()); i++; }
        else if (a == "--size") { std::sscanf(v.c_str(), "%dx%d", &g_cap.w, &g_cap.h); i++; }
        else if (a == "--scenario") { g_cap.scenario = v; i++; }
        else if (a == "--moves") { g_cap.moves = v; i++; }
    }
}

static void PlayMovesText(const std::string &list) {
    size_t p = 0;
    while (p < list.size()) {
        size_t q = list.find(',', p);
        std::string t = list.substr(p, q == std::string::npos ? std::string::npos : q - p);
        p = (q == std::string::npos) ? list.size() : q + 1;
        if (t.size() < 3 || g_state == AppState::GameOver) continue;
        GuiMove m;
        m.sx = t[0] - 'a'; m.sy = t[1] - '0'; m.dx = t[2] - 'a';
        m.dy = m.sy + (g_pos.side == White ? 1 : -1);
        if (guiIsLegal(g_pos, m.sx, m.sy, m.dx)) ApplyMove(m);
    }
}

static void ApplyScenario(const std::string &list) {
    size_t p = 0;
    while (p < list.size()) {
        size_t q = list.find(',', p);
        std::string t = list.substr(p, q == std::string::npos ? std::string::npos : q - p);
        p = (q == std::string::npos) ? list.size() : q + 1;
        if (t == "library")        OpenLibrary(1);
        else if (t == "standings") { OpenLibrary(1); g_lib.tab = 2; g_lib.active = 0; }
        else if (t == "presets")   { OpenLibrary(1); g_lib.tab = 1; g_lib.active = 2; }
        else if (t == "editor")    OpenAgentEditor(1, nullptr);
        else if (t == "models")    { OpenAgentEditor(1, nullptr); OpenModelPicker(0); }
        else if (t == "analysis")  g_panelTab = 1;
        else if (t == "view")      g_panelTab = 2;
        else if (t == "simple")    { g_simple = true; ApplySimpleMatchup(); }
        else if (t == "aivai")     { SetPlayerFromPreset(g_white, "watch_white"); SetPlayerFromPreset(g_black, "watch_black"); g_speedIndex = 3; StartGame(); }
        else if (t == "red")       g_pieceTheme = 1;
        else if (t == "flip")      g_flip = true;
        else if (t == "nopanel")   g_showPanel = false;
        else if (t == "hard")      { SetPlayerFromPreset(g_black, "hard"); }
    }
}

static void LoadStartupSettings() {
    libLoadSettings();
    g_ana.on = libSettingInt("ana.on", 1) != 0;
    g_ana.arrows = libSettingInt("ana.arrows", 3);
    g_ana.reply = libSettingInt("ana.reply", 1) != 0;
    g_ana.labels = libSettingInt("ana.labels", 1) != 0;
    g_ana.evalBar = libSettingInt("ana.evalbar", 1) != 0;
    // Default analysis evaluator: the Hard preset's evaluator when it names a
    // learned model, else Classic.
    int lv = learnedValueIndex();
    int defEval = EvaluatorIndexByName("Classic");
    int defSlot = 0;
    const LibEntry *hard = libPresetByRole("hard");
    if (hard) {
        RankAgent a;
        std::string err;
        if (rankAgentFromId(hard->id, a, err) && a.spec.brain == BRAIN_SEARCH) { defEval = a.spec.evaluator; defSlot = a.spec.modelSlot; }
    }
    g_ana.evaluator = libSettingInt("ana.evaluator", defEval < 0 ? 0 : defEval);
    SeedEvalDefaults(g_ana.evaluator, g_ana.params);
    std::string ps = libSetting("ana.params", "");
    if (!ps.empty()) {
        int i = 0;
        size_t p = 0;
        while (p <= ps.size() && i < MAX_EVAL_PARAMS) {
            size_t q = ps.find(',', p);
            g_ana.params[i++] = std::atoi(ps.substr(p, q == std::string::npos ? std::string::npos : q - p).c_str());
            if (q == std::string::npos) break;
            p = q + 1;
        }
    }
    g_ana.modelSlot = libSettingInt("ana.model", g_ana.evaluator == lv ? defSlot : 0);
    g_ana.tt = libSettingInt("ana.tt", 1) != 0;
    g_ana.ord = libSettingInt("ana.ord", 1) != 0;
    g_ana.qs = libSettingInt("ana.qs", 0) != 0;
    g_ana.maxDepth = libSettingInt("ana.depth", 30);
    g_ana.maxSeconds = libSettingInt("ana.seconds", 60);
    g_pieceTheme = libSettingInt("view.pieces", 0);
    g_boardTheme = libSettingInt("view.board", 0);
    g_flip = libSettingInt("view.flip", 0) != 0;
    g_coords = libSettingInt("view.coords", 1) != 0;
    g_showLast = libSettingInt("view.last", 1) != 0;
    g_showHints = libSettingInt("view.hints", 1) != 0;
    g_showReadouts = libSettingInt("view.readouts", 1) != 0;
    g_stepStyle = libSettingInt("view.sliders", 0);
    g_showPanel = libSettingInt("view.panel", 1) != 0;
    g_panelTab = libSettingInt("view.tab", 0);
    g_simple = libSettingInt("view.simple", 0) != 0;
    g_simpleMode = libSettingInt("simple.mode", 0);
    g_simpleLevel = libSettingInt("simple.level", 1);
    g_speedIndex = libSettingInt("pace.speed", 2);
    g_delay2s = libSettingInt("pace.delay2s", 0) != 0;
    std::string board = libSetting("board", "boards/board1.txt");
    std::strncpy(g_boardFile, board.c_str(), sizeof(g_boardFile) - 1);
    ClampSettings();

    // Players: the saved ids, else White human vs the Medium preset.
    std::string err;
    g_white = PlayerConfig();
    if (libSettingInt("white.human", 1) == 0 && !SetPlayerFromId(g_white, libSetting("white.id", ""), libSetting("white.label", ""), err))
        g_white.isHuman = true;
    g_black = PlayerConfig();
    bool blackHuman = libSettingInt("black.human", 0) != 0;
    if (!blackHuman) {
        std::string id = libSetting("black.id", "");
        if (id.empty() || !SetPlayerFromId(g_black, id, libSetting("black.label", ""), err))
            if (!SetPlayerFromPreset(g_black, "medium")) SetFallbackAgent(g_black);
    }
}

#if defined(PLATFORM_WEB)
// Page URL options, so a link can open straight into a matchup:
//   ?mode=white|black|watch   &level=easy|medium|hard   &hints=0|1
// Packed as (mode+1) | (level+1) << 2 | (hints+1) << 4, 0 = not given.
EM_JS(int, WebUrlOptionsPacked, (), {
    var p = new URLSearchParams(window.location.search);
    var modes = { white: 0, black: 1, watch: 2 };
    var levels = { easy: 0, medium: 1, hard: 2 };
    var m = p.get('mode');
    var l = p.get('level');
    var h = p.get('hints');
    var mi = (m in modes) ? modes[m] + 1 : 0;
    var li = (l in levels) ? levels[l] + 1 : 0;
    var hi = (h === '1') ? 2 : ((h === '0') ? 1 : 0);
    return mi | (li << 2) | (hi << 4);
});

static void ApplyWebUrlOptions() {
    int packed = WebUrlOptionsPacked();
    if (packed & 3)         g_simpleMode = (packed & 3) - 1;
    if ((packed >> 2) & 3)  g_simpleLevel = ((packed >> 2) & 3) - 1;
    if ((packed >> 4) & 3)  g_ana.on = ((packed >> 4) & 3) == 2;
}
#endif

int main(int argc, char **argv) {
    std::srand((unsigned)time(0));
    PRNT = 0;
    ParseArgs(argc, argv);
    mlAutoLoadDefaultSlots();
    engInit((unsigned)time(0));
    libLoad();

    if (g_cap.on) {
        // Capture mode: defaults only (never read or write the user's settings).
        SeedEvalDefaults(g_ana.evaluator, g_ana.params);
        const LibEntry *hard = libPresetByRole("hard");
        RankAgent a;
        std::string err;
        if (hard && rankAgentFromId(hard->id, a, err)) { g_ana.evaluator = a.spec.evaluator; g_ana.modelSlot = a.spec.modelSlot; }
        g_white.isHuman = true;
        if (!SetPlayerFromPreset(g_black, "medium")) SetFallbackAgent(g_black);
    } else {
        LoadStartupSettings();
    }
#if defined(PLATFORM_WEB)
    // The web page is simple mode only, and starts without hints so a first game
    // against the agent is not played off its own arrows.
    g_simple = true;
    g_ana.on = false;
    ApplyWebUrlOptions();
#endif

    unsigned flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT;
    if (g_cap.on) flags = FLAG_WINDOW_HIDDEN;
    SetConfigFlags(flags);
    InitWindow(g_cap.on ? g_cap.w : INIT_W, g_cap.on ? g_cap.h : INIT_H, "Breakthrough");
    if (!g_cap.on) SetWindowMinSize(MIN_W, MIN_H);
    SetExitKey(KEY_NULL);   // Esc closes windows inside the GUI instead
    ApplyDarkStyle();
    GuiEnableTooltip();

    if (g_simple) ApplySimpleMatchup();
    else StartGame();

    if (g_cap.on) {
        g_capRT = LoadRenderTexture(g_cap.w, g_cap.h);
        SetMouseOffset(-100000, -100000);    // no hover effects from wherever the cursor is
        PlayMovesText(g_cap.moves);
        ApplyScenario(g_cap.scenario);
    }

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    double longestFrameMs = 0.0;
    while (!WindowShouldClose()) {
        UpdateDrawFrame();
        // Reported in capture mode as a responsiveness check: the UI thread's
        // longest frame of work, which an engine stall would show directly.
        if (g_frame > 5 && g_lastWorkMs > longestFrameMs) longestFrameMs = g_lastWorkMs;
        if (g_cap.on && g_frame >= g_cap.frames) {
            std::printf("capture: %d frames, longest frame %.1f ms, %d moves played\n",
                        g_frame, longestFrameMs, (int)g_moves.size());
            Image img = LoadImageFromTexture(g_capRT.texture);
            ImageFlipVertical(&img);
            ExportImage(img, g_cap.out.c_str());
            UnloadImage(img);
            break;
        }
    }
    SaveAllSettings();
    if (!g_cap.on) libSaveSettings();
    engShutdown();
    if (g_cap.on) UnloadRenderTexture(g_capRT);
#endif
    CloseWindow();
    return 0;
}
