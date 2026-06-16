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

#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <cstdlib>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static const int CELL      = 70;
static const int BOARD_X   = 48;
static const int BOARD_Y   = 56;
static const int BOARD_PX  = SIZE * CELL;          // 560
static const int PANEL_X   = BOARD_X + BOARD_PX + 24;  // 632
static const int PANEL_W   = 372;
static const int WIN_W     = PANEL_X + PANEL_W + 16;   // 1020
static const int WIN_H     = 768;

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

// ---------------------------------------------------------------------------
// Application state
// ---------------------------------------------------------------------------
enum class AppState { Settings, WaitingForHuman, WaitingBeforeAI, ComputingAI, GameOver };

struct PlayerConfig {
    int  type   = TieredRandom;   // PlayerEnum value (Human..MiniMax map to 0..4)
    int  opener = StandardOpener; // 0..2
    // MiniMax weights
    int  depth = 8, turn = 1, chip = 4, wall = 0, col = 0;
    // SmartRandom furthest-N pieces
    int  furthest = 4;
    // raygui edit-mode flag for the depth spinner (typed entry)
    bool editDepth = false;
};

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
// delay per speed index (seconds); index 0 (Step) handled separately, 4 = instant
static const double SPEED_DELAY[5] = { 0.0, 1.0, 0.25, 0.0625, 0.0 };

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

// Build the w1..w5 argument pack the engine expects for a given player type.
static void BuildArgs(const PlayerConfig &c, int &w1, int &w2, int &w3, int &w4, int &w5) {
    if (c.type == MiniMax) {
        w1 = c.depth; w2 = c.turn; w3 = c.chip; w4 = c.wall; w5 = c.col;
    } else if (c.type == SmartRandom) {
        w1 = c.furthest; w2 = w3 = w4 = w5 = 1;
    } else {
        w1 = w2 = w3 = w4 = w5 = 1;
    }
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

    int w1, w2, w3, w4, w5;
    if (g_turn == White) {
        BuildArgs(g_white, w1, w2, w3, w4, w5);
        moveWhite(g_white.type, w1, w2, w3, w4, w5, g_white.opener);
    } else {
        BuildArgs(g_black, w1, w2, w3, w4, w5);
        moveBlack(g_black.type, w1, w2, w3, w4, w5, g_black.opener);
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

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static void Update() {
    // Board clicks (only meaningful while waiting for a human, and not over a
    // dropdown/textbox in edit mode).
    if (g_state == AppState::WaitingForHuman &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !g_editBoardFile && !g_editWhiteType && !g_editBlackType &&
        !g_white.editDepth && !g_black.editDepth) {
        Vector2 m = GetMousePosition();
        int sc = (int)((m.x - BOARD_X) / CELL);  // screen column
        int sr = (int)((m.y - BOARD_Y) / CELL);  // screen row (0 = top)
        if (m.x >= BOARD_X && m.y >= BOARD_Y && sc >= 0 && sc < SIZE && sr >= 0 && sr < SIZE) {
            int by = SIZE - 1 - sr;  // board row (y=SIZE-1 at top)
            HandleHumanClick(sc, by);
        }
    }

    // AI pacing.
    if (g_state == AppState::WaitingBeforeAI) {
        g_aiTimer += GetFrameTime();
        bool stepMode = (g_speedIndex == 0);
        bool go = false;
        if (g_stepRequested)                                          go = true;
        else if (!g_paused && !stepMode && g_aiTimer >= SPEED_DELAY[g_speedIndex]) go = true;
        if (go) g_state = AppState::ComputingAI;
    }

    if (g_state == AppState::ComputingAI) {
        ApplyAIMove();
        g_stepRequested = false;
        AfterMove();
    }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static void DrawPiece(int cx, int cy, char who) {
    float r = CELL * 0.36f;
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
            int px = BOARD_X + x * CELL;
            int py = BOARD_Y + sr * CELL;
            Color sq = ((x + by) % 2 == 0) ? COL_DARK : COL_LIGHT;
            DrawRectangle(px, py, CELL, CELL, sq);

            char who = board[x][by];
            if (who == WHITE || who == BLACK)
                DrawPiece(px + CELL / 2, py + CELL / 2, who);
        }
    }

    // Selection highlight + legal-destination hints.
    if (g_hasSel) {
        int selSr = SIZE - 1 - g_selY;
        DrawRectangleLinesEx(
            Rectangle{ (float)(BOARD_X + g_selX * CELL), (float)(BOARD_Y + selSr * CELL),
                       (float)CELL, (float)CELL }, 4, COL_SEL);
        int fwd = (g_turn == White) ? g_selY + 1 : g_selY - 1;
        for (int dx = -1; dx <= 1; dx++) {
            int nx = g_selX + dx;
            if (nx < 0 || nx >= SIZE || fwd < 0 || fwd >= SIZE) continue;
            bool ok = (g_turn == White) ? tryMoveWhite(g_selX, g_selY, nx, false)
                                        : tryMoveBlack(g_selX, g_selY, nx, false);
            if (ok) {
                int hsr = SIZE - 1 - fwd;
                DrawCircle(BOARD_X + nx * CELL + CELL / 2,
                           BOARD_Y + hsr * CELL + CELL / 2, CELL * 0.16f, COL_HINT);
            }
        }
    }

    // Hover highlight while a human is to move.
    if (g_state == AppState::WaitingForHuman) {
        Vector2 m = GetMousePosition();
        int sc = (int)((m.x - BOARD_X) / CELL);
        int sr = (int)((m.y - BOARD_Y) / CELL);
        if (m.x >= BOARD_X && m.y >= BOARD_Y && sc >= 0 && sc < SIZE && sr >= 0 && sr < SIZE)
            DrawRectangle(BOARD_X + sc * CELL, BOARD_Y + sr * CELL, CELL, CELL, COL_HOVER);
    }

    // Border + labels.
    DrawRectangleLinesEx(Rectangle{ (float)BOARD_X, (float)BOARD_Y,
                                      (float)BOARD_PX, (float)BOARD_PX }, 2, COL_LABEL);
    for (int x = 0; x < SIZE; x++) {
        const char *lbl = TextFormat("%c", 'a' + x);
        DrawText(lbl, BOARD_X + x * CELL + CELL / 2 - 4, BOARD_Y - 22, 18, COL_LABEL);
        DrawText(lbl, BOARD_X + x * CELL + CELL / 2 - 4, BOARD_Y + BOARD_PX + 4, 18, COL_LABEL);
    }
    for (int sr = 0; sr < SIZE; sr++) {
        int by = SIZE - 1 - sr;
        const char *lbl = TextFormat("%d", by);
        DrawText(lbl, BOARD_X - 20, BOARD_Y + sr * CELL + CELL / 2 - 8, 18, COL_LABEL);
        DrawText(lbl, BOARD_X + BOARD_PX + 6, BOARD_Y + sr * CELL + CELL / 2 - 8, 18, COL_LABEL);
    }
}

// One AI player's config block. Returns the y after the block. Defers drawing
// the type dropdown: stores its rectangle in *dropRect for a later top pass.
static float DrawPlayerConfig(const char *title, PlayerConfig &c, float x, float y,
                              float w, Rectangle *dropRect) {
    GuiLabel(Rectangle{ x, y, w, 18 }, title);
    y += 20;

    *dropRect = Rectangle{ x, y, w, 26 };  // type dropdown drawn later (on top)
    y += 30;

    GuiLabel(Rectangle{ x, y, 70, 22 }, "Opener");
    GuiToggleGroup(Rectangle{ x + 72, y, (w - 72) / 3.0f, 22 },
                   "Std;Off;Def", &c.opener);
    if (c.opener < 0) c.opener = StandardOpener;
    y += 28;

    if (c.type == SmartRandom) {
        float v = (float)c.furthest;
        GuiSliderBar(Rectangle{ x + 90, y, w - 130, 16 }, "Forward", TextFormat("%d", c.furthest), &v, 1, 16);
        c.furthest = (int)(v + 0.5f);
        y += 22;
    } else if (c.type == MiniMax) {
        // Depth: a slider for quick 1..25 selection plus a spinner to type an
        // exact value as high as desired. The slider only writes c.depth while
        // it is actually being dragged (we copy back only on change), so a value
        // typed above 25 in the spinner is preserved and not clamped to 25.
        float sv = (float)(c.depth < 1 ? 1 : (c.depth > 25 ? 25 : c.depth));
        float before = sv;
        GuiSliderBar(Rectangle{ x + 90, y, w - 90 - 92, 16 }, "Depth", "", &sv, 1, 25);
        if (sv != before) c.depth = (int)(sv + 0.5f);
        if (GuiSpinner(Rectangle{ x + w - 88, y - 1, 88, 18 }, NULL, &c.depth, 1, 1000000, c.editDepth))
            c.editDepth = !c.editDepth;
        y += 24;

        struct { const char *name; int *val; int lo, hi; } rows[] = {
            { "Turn",   &c.turn,  0, 10 },
            { "Chip",   &c.chip,  0, 10 },
            { "Wall",   &c.wall,  0, 10 },
            { "Column", &c.col,   0, 10 },
        };
        for (int i = 0; i < 4; i++) {
            float v = (float)*rows[i].val;
            GuiSliderBar(Rectangle{ x + 90, y, w - 130, 16 }, rows[i].name,
                         TextFormat("%d", *rows[i].val), &v, (float)rows[i].lo, (float)rows[i].hi);
            *rows[i].val = (int)(v + 0.5f);
            y += 22;
        }
    }
    return y + 6;
}

static void DrawPanel() {
    float x = (float)PANEL_X;
    float w = (float)PANEL_W;
    float y = 16;

    const char *TYPES = "Human;Uniform Random;Tiered Random;Smart Random;MiniMax";

    bool anyDropdown = g_editWhiteType || g_editBlackType;
    if (anyDropdown) GuiLock();

    Rectangle whiteDrop, blackDrop;
    y = DrawPlayerConfig("WHITE player", g_white, x, y, w, &whiteDrop);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 12;
    y = DrawPlayerConfig("BLACK player", g_black, x, y, w, &blackDrop);
    GuiLine(Rectangle{ x, y, w, 8 }, NULL); y += 14;

    // Board file
    GuiLabel(Rectangle{ x, y, 70, 26 }, "Board");
    if (GuiTextBox(Rectangle{ x + 72, y, w - 72, 26 }, g_boardFile, sizeof(g_boardFile), g_editBoardFile))
        g_editBoardFile = !g_editBoardFile;
    y += 32;

    // Start / New Game
    if (GuiButton(Rectangle{ x, y, w, 30 },
                  g_state == AppState::Settings ? "Start Game" : "New Game")) {
        StartGame();
    }
    y += 38;

    // Speed selector
    GuiLabel(Rectangle{ x, y, 60, 22 }, "Speed");
    GuiToggleGroup(Rectangle{ x + 62, y, (w - 62) / 5.0f, 22 },
                   "Step;0.25x;1x;4x;Inst", &g_speedIndex);
    if (g_speedIndex < 0) g_speedIndex = 2;
    y += 28;

    // Pause + Next move
    GuiToggle(Rectangle{ x, y, w / 2 - 4, 26 }, g_paused ? "Paused" : "Pause", &g_paused);
    if (GuiButton(Rectangle{ x + w / 2 + 4, y, w / 2 - 4, 26 }, "Next move"))
        g_stepRequested = true;
    y += 34;

    // Status line
    GuiLabel(Rectangle{ x, y, w, 20 },
             TextFormat("W:%d  B:%d   %s to move",
                        g_whiteCount, g_blackCount,
                        g_turn == White ? "White" : "Black"));
    y += 22;
    GuiLabel(Rectangle{ x, y, w, 20 }, g_status);
    y += 26;

    // Move log (scrolling).
    float logTop = y;
    float logH = (float)WIN_H - logTop - 16;
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

    if (anyDropdown) GuiUnlock();

    // Draw dropdowns last so their open lists render on top of everything.
    if (GuiDropdownBox(whiteDrop, TYPES, &g_white.type, g_editWhiteType)) {
        g_editWhiteType = !g_editWhiteType;
        g_editBlackType = false;
    }
    if (GuiDropdownBox(blackDrop, TYPES, &g_black.type, g_editBlackType)) {
        g_editBlackType = !g_editBlackType;
        g_editWhiteType = false;
    }
}

static void DrawGameOverBanner() {
    if (g_state != AppState::GameOver) return;
    const char *who = (g_winner == White) ? "WHITE WINS" : "BLACK WINS";
    int fs = 40;
    int tw = MeasureText(who, fs);
    int bx = BOARD_X + (BOARD_PX - tw) / 2 - 20;
    int by = BOARD_Y + BOARD_PX / 2 - 34;
    DrawRectangle(bx, by, tw + 40, 68, Color{ 0, 0, 0, 190 });
    DrawText(who, bx + 20, by + 14, fs, Color{ 255, 220, 90, 255 });
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------
static void UpdateDrawFrame() {
    Update();

    BeginDrawing();
    ClearBackground(COL_BG);
    DrawText("Breakthrough", BOARD_X, 12, 26, COL_LABEL);
    DrawText(TextFormat("%s  vs  %s", PlayerName(g_white.type), PlayerName(g_black.type)),
             BOARD_X + 200, 20, 16, COL_LABEL);
    DrawBoard();
    DrawGameOverBanner();
    DrawPanel();
    EndDrawing();
}

int main() {
    std::srand((unsigned)time(0));
    PRNT = 0;  // silence engine console output

    // Default matchup: Human (White) vs MiniMax (Black).
    g_white.type = Human;
    g_black.type = MiniMax;

    InitWindow(WIN_W, WIN_H, "Breakthrough");
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
