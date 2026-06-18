# Plan: Resizable window, scaling board, and hideable left-side overlay panel

## Context

The Breakthrough GUI ([gui/main_gui.cpp](gui/main_gui.cpp)) currently uses a
fixed-size window and a fixed right-hand control panel. All geometry comes from
compile-time constants (`CELL`, `BOARD_X/Y`, `BOARD_PX`, `PANEL_X/W`, `WIN_W/H`),
so the window cannot be resized and the board never changes size.

The developer wants:
1. A **resizable window**.
2. The **board to grow/shrink with the window**.
3. The options panel moved to the **left** as an **overlay that draws over the
   board and can be hidden/shown**.

Decisions confirmed: scope is **desktop now, web later** (no `shell.html`/Emscripten
canvas work this round; the board still scales within whatever canvas size the web
build uses). The overlay is **visible on launch**, toggled by a button and the Tab
key. This is a single-file change to `gui/main_gui.cpp`; the engine and build are
untouched.

## Approach

### 1. Resizable window (`main()`)
- Call `SetConfigFlags(FLAG_WINDOW_RESIZABLE)` **before** `InitWindow`.
- After `InitWindow`, call `SetWindowMinSize(820, 620)` so the board + overlay stay
  usable. Keep the initial size around 1024x768 (rename `WIN_W/WIN_H` to
  `INIT_W/INIT_H` used only for the initial `InitWindow` call).

### 2. Per-frame board geometry (replaces the fixed constants)
- Delete the `static const` `CELL`, `BOARD_X`, `BOARD_Y`, `BOARD_PX`, `PANEL_X`,
  `PANEL_W`, `WIN_W`, `WIN_H`. Replace with file-scope runtime values updated each
  frame: `static int g_cell, g_boardX, g_boardY, g_boardPx;` and a panel-width
  constant `PANEL_W = 360`.
- Add `static void ComputeLayout()`:
  - `int W = GetScreenWidth(), H = GetScreenHeight();`
  - Reserve a top bar (`TOP = 44`) for the title + toggle button, and a label
    margin (`MARGIN = 28`) around the board for the a-h / 0-7 labels.
  - `g_cell = min((W - 2*MARGIN) / SIZE, (H - TOP - 2*MARGIN) / SIZE);` (clamp to a
    small minimum so it never goes non-positive).
  - `g_boardPx = g_cell * SIZE;`
  - Center the board in the area below the top bar:
    `g_boardX = (W - g_boardPx) / 2; g_boardY = TOP + (H - TOP - g_boardPx) / 2;`
- Call `ComputeLayout()` at the very top of `UpdateDrawFrame()`, before `Update()`,
  so both input handling and drawing use the same geometry that frame.
- Update `DrawBoard()`, `DrawPiece()` (radius from `g_cell`), `Update()`'s
  mouse->grid math, and `DrawGameOverBanner()` to read `g_cell`/`g_boardX`/
  `g_boardY`/`g_boardPx` instead of the old constants. Scale label font roughly
  with cell size (e.g. `clamp(g_cell/4, 12, 20)`); keep the existing
  `by = SIZE-1-sr` orientation.

### 3. Left-side hideable overlay panel
- Add state: `static bool g_showPanel = true;` and a panel rectangle the layout
  computes: `static Rectangle g_panelRect;` set in `ComputeLayout()` to
  `{ 0, TOP, PANEL_W, H - TOP }` (full height under the top bar, flush left).
- **Toggle:** in `UpdateDrawFrame()` draw a small always-visible `GuiButton` in the
  top-left of the top bar labeled `"Options"`/`"Hide"` that flips `g_showPanel`;
  also toggle on `IsKeyPressed(KEY_TAB)` inside `Update()`.
- **DrawPanel()** changes:
  - Only call it when `g_showPanel`.
  - Draw a semi-opaque background first so widgets read over the board:
    `DrawRectangleRec(g_panelRect, Color{20,22,28,235});` plus a right-edge line.
  - Lay widgets starting at `x = g_panelRect.x + 12`, `w = PANEL_W - 24`,
    `y = TOP + 12` (was `PANEL_X`/16). The move-log height becomes
    `H - logTop - 12` using live `GetScreenHeight()` instead of `WIN_H`.
  - Everything else (dropdowns drawn last with `GuiLock`, sliders, spinner,
    speed/pause/next, status, scroll-panel log) stays as-is, just re-anchored to the
    new `x`/`w`/dynamic height.

### 4. Gate board clicks under the overlay
- In `Update()`, when `g_showPanel` is true, ignore board clicks whose mouse point
  falls inside `g_panelRect`:
  add `&& !(g_showPanel && CheckCollisionPointRec(m, g_panelRect))` to the existing
  human-click guard (which already checks the edit-mode flags). Compute `m` before
  the guard. The toggle button lives in the top bar, outside the board, so it
  never conflicts with board clicks.

## Critical file
- [gui/main_gui.cpp](gui/main_gui.cpp) — all changes. No engine (`src/`) or build
  script changes. `gui/shell.html` is intentionally left unchanged this round.

## Verification
1. Build + smoke test: `./smoke_test_gui.ps1 -Build` (exit 0, screenshot to
   `build/gui_smoke.png`). Confirm the board renders centered with the overlay
   visible on the left.
2. Manual (`./smoke_test_gui.ps1 -KeepOpen` or `.\build_gui.bat; if ($?) { .\breakthrough_gui.exe }`):
   - Drag the window larger/smaller and confirm the board scales and stays square
     and centered; labels stay aligned.
   - Click **Options**/**Hide** and press **Tab**: the overlay shows/hides; when
     hidden the full board is visible.
   - With the overlay shown, click a widget over the board (e.g. a dropdown) and
     confirm it does NOT also select a board square underneath.
   - Start Human (White) vs MiniMax (Black), hide the panel, and play a move by
     clicking a piece then a destination to confirm click-to-move still maps to the
     right squares after scaling.
3. Console regression unaffected (no `src/` changes), but a quick
   `./smoke_test_gui.ps1 -Build` covers the GUI compile.

## Docs (per project workflow)
- Update [README.md](README.md) "Using the GUI": note the resizable window, the
  scaling board, and the **Options**/Tab toggle for the left overlay.
- Update [CLAUDE.md](CLAUDE.md) GUI notes: per-frame `ComputeLayout()` geometry and
  the overlay/toggle, replacing the description of a fixed right-hand panel.
