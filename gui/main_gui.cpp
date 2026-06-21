// main_gui.cpp - Raylib + raygui front end for Breakthrough.
//
// This is a thin, additive GUI layer over the existing console engine. It never
// calls getSettings(), playerMove(), or printBoard(). Instead it:
//   - reads the global board[col][row] each frame to render,
//   - drives human moves from mouse clicks via tryMove*/playMove*,
//   - drives AI moves with a single moveWhite()/moveBlack() call per turn,
//   - detects wins by scanning the goal rows + piece counts (robust regardless
//     of which engine path made the move).
//
// The same source compiles to a native window (while !WindowShouldClose) and to
// the web via Emscripten (emscripten_set_main_loop), see main() at the bottom.

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// raylib.h defines WHITE/BLACK as Color macros, which collide with globals.h's
// board macros (#define WHITE 'W', BLACK 'B'). We need the board macros, so drop
// the raylib color macros and use explicit Color literals for drawing instead.
#undef WHITE
#undef BLACK

#include "globals.h"
#include "ai_eval.h"   // evaluator registry (g_evaluators / g_evalCount / MAX_EVAL_PARAMS)

#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdlib>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------
enum class AppState { Settings, WaitingForHuman, WaitingBeforeAI, ComputingAI, GameOver };

struct PlayerConfig {
    int  type   = TieredRandom;   // PlayerEnum value (Human..MiniMax map to 0..4)
    int  opener = StandardOpener; // 0..2
    // MiniMax search depth
    int  depth = 8;
    // Evaluator selection + its parameters (filled from the g_evaluators registry).
    int  evaluator = 0;                  // index into g_evaluators
    int  evalParams[MAX_EVAL_PARAMS] = { 0 };
    int  seededFor = -1;                 // evaluator whose defaults are loaded in evalParams
    // SmartRandom furthest-N pieces
    int  furthest = 4;
    // raygui edit-mode flag for the depth spinner (typed entry)
    bool editDepth = false;
};

// Load the registry defaults for c.evaluator into c.evalParams whenever the
// selected evaluator changes (also seeds the initial values). Keeps each side's
// weights meaningful after switching evaluators in the dropdown.
static void SeedEvalParams(PlayerConfig &c) {
    if (c.seededFor == c.evaluator) return;
    if (c.evaluator < 0 || c.evaluator >= g_evalCount) c.evaluator = 0;
    const EvalDef &e = g_evaluators[c.evaluator];
    for (int i = 0; i < e.paramCount; i++) c.evalParams[i] = e.params[i].def;
    c.seededFor = c.evaluator;
}

static AppState     g_state = AppState::Settings;
static PlayerConfig g_white;
static PlayerConfig g_black;
static int          g_turn   = White;   // whose move it is
static int          g_winner = None;

// Pacing
static int    g_speedIndex = 2;          // 0=Step 1=0.25x 2=1x 3=4x 4=Instant
static double g_aiTimer = 0.0;
static bool   g_paused = false;
static bool   g_stepRequested = false;
static bool   g_delay2s = false;         // human vs fast AI: hold AI to >=2s/move
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

// raygui edit-mode flags
static bool g_editBoardFile = false;
static bool g_editWhiteType = false;
static bool g_editBlackType = false;
static bool g_editWhiteOpener = false;
static bool g_editBlackOpener = false;
static bool g_editWhiteEval = false;
static bool g_editBlackEval = false;

// Left overlay panel visibility (toggled by the Options button / Tab key).
static bool g_showPanel = true;

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

// Edit-mode flags for the typeable stepper styles (spinner / value box), kept as a
// small fixed set keyed by player+parameter so PlayerConfig stays unchanged.
// Index layout: [side][slot], side 0 = White, 1 = Black. Slot 0 = Depth,
// slots 1..MAX_EVAL_PARAMS = evaluator params, last slot = SmartRandom Forward.
static bool g_stepEdit[2][MAX_EVAL_PARAMS + 2] = { { false } };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void SetStatus(const char *msg) {
    std::strncpy(g_status, msg, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
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

static const char *PlayerName(int type) {
    switch (type) {
        case Human:         return "Human";
        case UniformRandom: return "Uniform Random";
        case TieredRandom:  return "Tiered Random";
        case SmartRandom:   return "Smart Random";
        case MiniMax:       return "MiniMax";
        default:            return "?";
    }
}

// The engine's w1 argument means depth for MiniMax, furthest-N for SmartRandom,
// and is unused for the other types.
static int SearchArg(const PlayerConfig &c) {
    if (c.type == MiniMax)     return c.depth;
    if (c.type == SmartRandom) return c.furthest;
    return 1;
}

// Advance to the next turn / state after a move has been applied.
static void AfterMove() {
    g_hasSel = false;
    g_winner = CheckWinner();
    if (g_winner != None) { g_state = AppState::GameOver; return; }

    g_turn = (g_turn == White) ? Black : White;
    int t = (g_turn == White) ? g_white.type : g_black.type;
    if (t == Human) {
        g_state = AppState::WaitingForHuman;
    } else {
        g_state = AppState::WaitingBeforeAI;
        g_aiTimer = 0.0;
    }
}

static void StartGame() {
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
    g_aiTimer = 0.0;
    g_editWhiteType = g_editBlackType = g_editBoardFile = false;

    int t = g_white.type;
    g_state = (t == Human) ? AppState::WaitingForHuman : AppState::WaitingBeforeAI;
    SetStatus("Game started. White to move.");
}

// ---------------------------------------------------------------------------
// Move handling
// ---------------------------------------------------------------------------
static void ApplyAIMove() {
    char prev[SIZE][SIZE];
    std::memcpy(prev, board, sizeof(prev));

    if (g_turn == White) {
        moveWhite(g_white.type, SearchArg(g_white), g_white.evaluator, g_white.evalParams, g_white.opener);
    } else {
        moveBlack(g_black.type, SearchArg(g_black), g_black.evaluator, g_black.evalParams, g_black.opener);
    }

    int x1, y1, x2;
    if (DiffMove(prev, g_turn, x1, y1, x2)) LogMove(g_turn, x1, y1, x2);
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
    bool wH = (g_white.type == Human), bH = (g_black.type == Human);
    int humans = (wH ? 1 : 0) + (bH ? 1 : 0);
    const PlayerConfig &ai = wH ? g_black : g_white;   // an AI side (if any)
    Matchup mu;
    mu.humans = humans;
    mu.aiVsAi = (humans == 0);
    mu.aiSlow = (ai.type == MiniMax && ai.depth > 5);  // slow enough to self-pace
    return mu;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static void Update() {
    // Tab toggles the options overlay.
    if (IsKeyPressed(KEY_TAB)) g_showPanel = !g_showPanel;

    // Board clicks (only meaningful while waiting for a human, not over a
    // dropdown/textbox in edit mode, and not over the open overlay).
    Vector2 m = GetMousePosition();
    if (g_state == AppState::WaitingForHuman &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !g_editBoardFile && !g_editWhiteType && !g_editBlackType &&
        !g_white.editDepth && !g_black.editDepth &&
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
        if (go) g_state = AppState::ComputingAI;
    }

    if (g_state == AppState::ComputingAI) {
        ApplyAIMove();
        g_stepRequested = false;
        AfterMove();
    }
}

// ---------------------------------------------------------------------------
// Layout (recomputed each frame from the live window size)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
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

            char who = board[x][by];
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

// One AI player's config block. Returns the y after the block. Defers drawing
// the type and opener dropdowns: stores their rectangles in *typeRect/*openerRect
// for a later top pass so their open lists render above the rows below them.
// `side` is 0 for White, 1 for Black (indexes the stepper edit-mode flags).
static float DrawPlayerConfig(const char *title, int side, PlayerConfig &c,
                              float x, float y, float w,
                              Rectangle *typeRect, Rectangle *openerRect,
                              Rectangle *evalRect) {
    GuiLabel(Rectangle{ x, y, w, 18 }, title);
    y += 20;

    *typeRect = Rectangle{ x, y, w, 26 };  // type dropdown drawn later (on top)
    y += 30;

    GuiLabel(Rectangle{ x, y, 52, 24 }, "Opener");
    *openerRect = Rectangle{ x + 58, y, w - 58, 24 };  // opener dropdown, drawn later
    if (c.opener < 0) c.opener = StandardOpener;
    y += 30;

    *evalRect = Rectangle{ 0, 0, 0, 0 };   // only shown for MiniMax (set below)

    const int FURTHEST_SLOT = MAX_EVAL_PARAMS + 1;
    if (c.type == SmartRandom) {
        y = StepperRow(side, FURTHEST_SLOT, STEP_SEGMENTS, x, y, w, "Forward", &c.furthest, 1, 16);
    } else if (c.type == MiniMax) {
        // Depth uses the typeable Number+bar design so it can exceed the bar's 25.
        y = StepperRow(side, 0, STEP_NUMBAR, x, y, w, "Depth", &c.depth, 1, 1000000, 25);

        // Evaluator dropdown (drawn later, on top) followed by that evaluator's
        // parameters, one row each from the registry. Switching evaluator reseeds
        // the values to that evaluator's defaults.
        GuiLabel(Rectangle{ x, y, 52, 24 }, "Eval");
        *evalRect = Rectangle{ x + 58, y, w - 58, 24 };
        y += 30;

        SeedEvalParams(c);
        const EvalDef &e = g_evaluators[c.evaluator];
        for (int i = 0; i < e.paramCount; i++) {
            // Cycle the stepper designs so adjacent rows look distinct; the Sliders
            // switcher (g_stepStyle) can still force one design across all rows.
            StepStyle st = (StepStyle)(i % STEP_STYLE_COUNT);
            y = StepperRow(side, 1 + i, st, x, y, w, e.params[i].name,
                           &c.evalParams[i], e.params[i].lo, e.params[i].hi);
        }
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

    const char *TYPES = "Human;Uniform Random;Tiered Random;Smart Random;MiniMax";

    // Slider-design switcher: "Per-row" shows each parameter in its own design,
    // any other choice forces that design on every row. GuiComboBox cycles in
    // place, so (unlike the type dropdowns) it needs no overlay/lock handling.
    GuiLabel(Rectangle{ x, y, 52, 22 }, "Sliders");
    GuiComboBox(Rectangle{ x + 58, y, w - 58, 22 },
                "Per-row;Bar+number;Segments;Number+bar;Handle;Ruler", &g_stepStyle);
    y += 28;

    bool anyDropdown = g_editWhiteType || g_editBlackType ||
                       g_editWhiteOpener || g_editBlackOpener ||
                       g_editWhiteEval || g_editBlackEval;
    if (anyDropdown) GuiLock();

    Rectangle whiteDrop, blackDrop, whiteOpenDrop, blackOpenDrop, whiteEvalDrop, blackEvalDrop;
    y = DrawPlayerConfig("WHITE player", 0, g_white, x, y, w, &whiteDrop, &whiteOpenDrop, &whiteEvalDrop);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 12;
    y = DrawPlayerConfig("BLACK player", 1, g_black, x, y, w, &blackDrop, &blackOpenDrop, &blackEvalDrop);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 14;

    // Board file
    GuiLabel(Rectangle{ x, y, 52, 26 }, "Board");
    if (GuiTextBox(Rectangle{ x + 58, y, w - 58, 26 }, g_boardFile, sizeof(g_boardFile), g_editBoardFile))
        g_editBoardFile = !g_editBoardFile;
    y += 32;

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

    // Draw the four dropdowns (player type + opener, per side) last so their open
    // lists render on top. The currently-open one (if any) is drawn after the rest
    // and after GuiUnlock, so only it is interactive while the others stay locked
    // underneath, preventing click-through from an open list. Opening any dropdown
    // closes the others so only one list is ever expanded.
    const char *OPENERS = "Standard;Offensive;Defensive";
    // Evaluator options string ("Classic;Experimental;...") built from the registry.
    static std::string EVALS;
    if (EVALS.empty())
        for (int i = 0; i < g_evalCount; i++) {
            if (i) EVALS += ";";
            EVALS += g_evaluators[i].name;
        }

    struct DropSpec { Rectangle r; const char *opts; int *val; bool *edit; };
    DropSpec ds[6] = {
        { whiteDrop,     TYPES,         &g_white.type,      &g_editWhiteType   },
        { blackDrop,     TYPES,         &g_black.type,      &g_editBlackType   },
        { whiteOpenDrop, OPENERS,       &g_white.opener,    &g_editWhiteOpener },
        { blackOpenDrop, OPENERS,       &g_black.opener,    &g_editBlackOpener },
    };
    int n = 4;
    // Eval dropdowns are present only for MiniMax sides (rect width > 0).
    if (whiteEvalDrop.width > 0) ds[n++] = { whiteEvalDrop, EVALS.c_str(), &g_white.evaluator, &g_editWhiteEval };
    if (blackEvalDrop.width > 0) ds[n++] = { blackEvalDrop, EVALS.c_str(), &g_black.evaluator, &g_editBlackEval };

    int openIdx = -1;
    for (int i = 0; i < n; i++) if (*ds[i].edit) { openIdx = i; break; }

    for (int i = 0; i < n; i++) {
        if (i == openIdx) continue;
        if (GuiDropdownBox(ds[i].r, ds[i].opts, ds[i].val, *ds[i].edit)) {
            for (int j = 0; j < n; j++) *ds[j].edit = false;
            *ds[i].edit = true;
        }
    }
    if (anyDropdown) GuiUnlock();
    if (openIdx >= 0) {
        if (GuiDropdownBox(ds[openIdx].r, ds[openIdx].opts, ds[openIdx].val, *ds[openIdx].edit))
            *ds[openIdx].edit = false;
    }
}

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

static void DrawPieceCounts() {
    if (g_boardPx <= 0) return;
    // In the reserved strip just to the right of the board, so nothing overlaps the
    // squares. Orientation: White starts on the low rows and moves up (its pieces
    // sit at the bottom of the screen); Black sits at the top.
    int bx = g_boardX + g_boardPx + 8;
    DrawCountBadge(bx, g_boardY,                  BLACK, g_blackCount);  // Black's side (top)
    DrawCountBadge(bx, g_boardY + g_boardPx - 26, WHITE, g_whiteCount);  // White's side (bottom)
}

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

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
static void UpdateDrawFrame() {
    ComputeLayout();
    Update();

    BeginDrawing();
    ClearBackground(COL_BG);

    DrawBoard();
    DrawPieceCounts();      // emblematic counts on the board (visible with panel hidden)
    if (g_showPanel) DrawPanel();
    DrawGameOverBanner();   // on top so the win banner stays readable

    // Top bar: Options/Hide toggle + title (above the overlay, always visible).
    if (GuiButton(Rectangle{ 8, 8, 96, 28 }, g_showPanel ? "Hide" : "Options"))
        g_showPanel = !g_showPanel;
    DrawText("Breakthrough", 116, 11, 22, COL_LABEL);
    DrawText(TextFormat("%s  vs  %s", PlayerName(g_white.type), PlayerName(g_black.type)),
             320, 15, 16, COL_LABEL);

    EndDrawing();
}

int main() {
    std::srand((unsigned)time(0));
    PRNT = 0;  // silence engine console output

    // Default matchup: Human (White) vs MiniMax (Black).
    g_white.type = Human;
    g_black.type = MiniMax;
    SeedEvalParams(g_white);   // load registry defaults into each side's evalParams
    SeedEvalParams(g_black);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(INIT_W, INIT_H, "Breakthrough");
    SetWindowMinSize(MIN_W, MIN_H);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    // Load the default board so the grid shows something before the game starts.
    if (!reloadBoard(g_boardFile)) {
        // Fall back to an empty board if the file is missing; the user can fix
        // the path in the Board text box and press Start.
        for (int x = 0; x < SIZE; x++)
            for (int yy = 0; yy < SIZE; yy++) board[x][yy] = EMPTY;
    }

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) UpdateDrawFrame();
#endif

    CloseWindow();
    return 0;
}
