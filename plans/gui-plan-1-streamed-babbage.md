# Plan: Add a Raylib + raygui GUI to Breakthrough (desktop first, web later)

## Context

Breakthrough is currently a console-only C++ app. The developer wants a graphical
front end (sprite board + widgets: dropdowns, radio, sliders, text boxes, a
scrolling move log, and mouse square-selection) that:

1. Is easy to work with,
2. Does **not** slow down the existing C++ AI, and
3. Can eventually be hosted on the web (GitHub Pages) for a resume site.

**Chosen approach (confirmed with developer):** Keep the existing C++ AI logic
completely intact and compile it to WebAssembly via **Emscripten**. Build the GUI
with **Raylib + raygui** (Raylib for sprites/mouse/game-loop, raygui for widgets).
Target a **native Windows .exe first**, then add the Emscripten web build from the
same source. The GUI replaces **all** console interaction (settings, board render,
human moves, move log); the existing console app stays buildable and untouched.

This works because the game logic is already cleanly separated from I/O. The only
console coupling is in `getSettings()`, `playerMove()`, and `printBoard()` /
`PRNT`-gated `cout` - none of which the GUI needs to call.

## Why this is the right fit

- **No AI rewrite, no perf loss.** WASM runs the existing minimax at near-native
  speed. Unity would force a C# rewrite and ship large/slow WebGL builds.
- **Same source -> desktop and web.** Raylib's frame-loop model maps directly onto
  both `while(!WindowShouldClose())` (native) and `emscripten_set_main_loop` (web).
- **Widgets included.** raygui is a single header providing exactly the controls
  requested.
- **Free static hosting.** Emscripten emits `index.html` + `.wasm` + `.js`, which
  GitHub Pages serves directly.

## Key integration facts (from codebase exploration)

- **Board state:** global `char board[SIZE][SIZE]` indexed `[col][row]`
  (`globals.h`/`globals.cpp`). White advances toward row `SIZE-1`, Black toward row
  `0` (per `tryMoveWhite`: source `moveY` -> dest `moveY+1`). The GUI renders by
  reading this array each frame.
- **Non-interactive board load:** `reloadBoard("boards/board1.txt")`
  ([board_io.cpp:52](src/board_io.cpp#L52)) loads a file and recomputes
  `g_whiteCount`, `g_blackCount`, `g_chipDiff`, `g_whiteAtEnd`, `g_blackAtEnd`.
  No refactor needed.
- **Human move (bypass `cin`):** call `tryMoveWhite/Black(x1, y, x2, false)`
  ([moves.cpp:126](src/moves.cpp#L126)) to validate, then
  `playMoveWhite/Black(x1, y, x2)` ([moves.cpp:174](src/moves.cpp#L174)) to apply.
  Both return a victor code; both are I/O-free except `PRNT`-gated prints.
- **AI move (one call per turn):** `moveWhite(...)` / `moveBlack(...)`
  ([moves.cpp:6](src/moves.cpp#L6)) dispatch by player type and return a victor
  code. Safe to call directly for AI players. **Do NOT** call them for a Human
  player (they route to the `cin`-blocking `playerMove()`).
- **Silence console output:** set the global `PRNT = 0` so the gated `cout` calls
  in `playMove*`, `main`, etc. stay quiet.
- **Victor codes:** `None`, `White`, `Black`, `WhiteWin`, `BlackWin` (enums in
  `globals.h`). Loop-end condition in `main.cpp` is `victor >= WhiteWin ||
  victor <= BlackWin`.
- **RNG:** the random AIs use `rand()`; the GUI must `srand(time(0))` at startup
  (as `main.cpp` does).
- **Build today:** standard library only, no external deps (`CMakeLists.txt`).

## Architectural change: blocking loop -> per-frame state machine

The existing game loop blocks (on `cin`, and in a `while` loop). A GUI frame loop
must never block - mandatory for Emscripten in the browser. The GUI replaces
`main.cpp`'s loop with a state machine ticked once per frame.

**Explicit AI-turn states (do not move every frame).** A naive "if AI, move now"
tick would play one move per rendered frame (~60 moves/sec when searches are
cheap), making the board, log, and status unreadable, and would mask
turn-flip bugs as runaway repeated calls. Use distinct states plus a pacing delay:

```
enum class AppState {
    Settings,           // configuring; not yet started
    WaitingForHuman,    // human to move: handle clicks
    WaitingBeforeAI,    // AI to move: count down AI_MOVE_DELAY before searching
    ComputingAI,        // run moveWhite/Black once, apply, log, flip turn
    GameOver
};

// Pacing
double aiMoveDelaySeconds;     // set per match mode (below)
bool   paused;                 // Pause toggle
bool   stepRequested;          // "Next move" advances exactly one AI move when paused

// Tick:
//   Settings:        run only the widget panel + Start button.
//   WaitingForHuman: first click -> select source (highlight); second click -> dest;
//                    tryMove*(x1,y,x2,false); if valid playMove*(x1,y,x2);
//                    append notation to log; flip turn; route to next state by victor
//                    and by next player's type.
//   WaitingBeforeAI: accumulate GetFrameTime(); when elapsed >= aiMoveDelaySeconds
//                    (and not paused, or stepRequested) -> ComputingAI.
//   ComputingAI:     victor = moveWhite(...) / moveBlack(...);  // computes + plays
//                    append to log; flip turn; clear stepRequested;
//                    if victor decisive -> GameOver else route by next player type.
```

**Speed controls.**
- Human vs AI: `aiMoveDelaySeconds` ~ 0.15-0.30 s so moves are watchable.
- AI vs AI: a speed selector (Step / 0.25x / 1x / 4x / Instant) maps to delay
  values (e.g. Step = paused + Next-move only; Instant = 0 s). Add **Pause** and
  **Next move** buttons to the widget panel.

Note: a deep minimax search still runs synchronously within the `ComputingAI`
frame (a brief hitch). Acceptable for v1. If it becomes a problem on web, a later
pass can cap depth or move search off-thread; not in scope now.

## Files to create (additive only - no existing src file changes)

- `gui/main_gui.cpp` - the entire front end: window setup, state machine, board
  rendering, mouse->grid mapping, raygui widget panel, move log, and the
  native/web main-loop shim (`#ifdef PLATFORM_WEB` -> `emscripten_set_main_loop`,
  else `while(!WindowShouldClose())`).
- `gui/raygui.h` - vendored single-header raygui (define `RAYGUI_IMPLEMENTATION`
  in exactly one TU).
- `gui/shell.html` - minimal Emscripten shell page (page title, canvas, basic
  styling) for the web build.
- `resources/` - piece sprites (`white.png`, `black.png`) plus optional
  board/background art. (v1 fallback: draw filled circles if sprites are not yet
  added, so the build is runnable immediately.)
- Build scripts: `build_gui.bat` (native MSVC) and `build_web.bat` (Emscripten).

## GUI layout (single window)

- **Left:** 8x8 board. Cell size from a constant (e.g. 64 px). Render order matches
  `printBoard` orientation (row `SIZE-1` at top). Mouse->grid:
  `col = (mouseX - originX) / cell`, `row = SIZE-1 - (mouseY - originY) / cell`.
  Highlight hovered cell and the selected source square; show legal-destination
  hints by probing `tryMove*` for the selected piece.
- **Right panel (raygui):**
  - `GuiDropdownBox` x2 - White/Black player type (Human / UniformRandom /
    TieredRandom / SmartRandom / MiniMax).
  - `GuiToggleGroup` - opener selection (radio: Standard / Offensive / Defensive).
  - `GuiSliderBar` x N - MiniMax depth + turn/chip/wall/column weights (and
    SmartRandom "furthest N" piece count). Only show sliders relevant to the
    selected player type.
  - `GuiTextBox` - board file name (defaults to `boards/board1.txt`).
  - `GuiButton` - "Start / New Game" (calls `reloadBoard`, resets state machine).
  - Playback controls: **Pause** toggle, **Next move** button, and an AI-vs-AI
    speed selector (`GuiToggleGroup`: Step / 0.25x / 1x / 4x / Instant) that sets
    `aiMoveDelaySeconds`.
  - `GuiScrollPanel` - scrolling move log (accumulated move notation, e.g. `c1d`).
  - Status line: side to move, piece counts (`g_whiteCount`/`g_blackCount`),
    win banner in `GAME_OVER`.

Config selected in widgets is stored in a small local struct and passed straight
into `moveWhite/moveBlack` arguments - `getSettings()` is never called.

## Build setup

**Native (MSVC), v1 dev loop.** Install raylib (vcpkg `vcpkg install raylib`, or a
prebuilt MSVC release). Example command (paths adjusted to the raylib install):

```
cl gui\main_gui.cpp src\globals.cpp src\board_io.cpp src\settings.cpp ^
   src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp ^
   src\ai_minimax.cpp /I src /I gui /I <raylib>\include /EHsc /Fo"build\\" ^
   /Fe:breakthrough_gui.exe /link /LIBPATH:<raylib>\lib raylib.lib ^
   opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib
```

(`settings.cpp` is still linked so the console build is unaffected; the GUI simply
does not call `getSettings`.)

**Web (Emscripten), once native works.** Requires the emsdk and a web build of
raylib (`emcc`-compiled `libraylib.a`). **No Asyncify** - the per-frame state
machine + `emscripten_set_main_loop` already makes the app non-blocking, and
nothing calls `emscripten_sleep()` or synchronously waits on JS, so Asyncify would
only add WASM size and runtime overhead.

Common sources (shared by both builds), output to `docs/` so GitHub Pages serves
from the `main` branch `/docs` folder. `--preload-file boards` bundles the board
files into the WASM virtual FS so `reloadBoard` works in the browser.

Release build:
```
emcc gui\main_gui.cpp src\globals.cpp src\board_io.cpp src\settings.cpp ^
   src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp ^
   src\ai_minimax.cpp -I src -I gui -I <raylib-web>\include ^
   <raylib-web>\lib\libraylib.a -sUSE_GLFW=3 -O3 -flto -sASSERTIONS=0 ^
   --preload-file resources --preload-file boards ^
   --shell-file gui\shell.html -o docs\index.html -DPLATFORM_WEB
```

Development build (kept separate, for debugging in the browser):
```
emcc ... (same sources/includes/lib) -sUSE_GLFW=3 -O0 -gsource-map ^
   -sASSERTIONS=2 --preload-file resources --preload-file boards ^
   --shell-file gui\shell.html -o docs\index.html -DPLATFORM_WEB
```

## Documentation updates (per CLAUDE.md workflow)

- `README.md`: add GUI build/run sections (native + web), GitHub Pages hosting
  note, and updated controls (click-to-move).
- `CLAUDE.md`: add `gui/` to the file structure table, note the new GUI front end
  and the `PRNT=0` / direct-`playMove*` integration pattern, and the two new build
  commands.

## Verification

1. **Console regression:** existing `cl` build and `tests.exe` (63 assertions)
   still pass - confirms no game-logic file was broken.
2. **Native GUI smoke test:** launch `breakthrough_gui.exe`; board renders from
   `boards/board1.txt`; hovering highlights cells.
3. **Human play:** Human vs UniformRandom - click a piece, click a legal
   destination, piece moves; illegal clicks are rejected (no move, source stays
   selectable); move log scrolls.
4. **AI play + perf:** MiniMax (depth 3) vs MiniMax (depth 3) - game runs to
   completion without freezing the window beyond brief per-move think hitches;
   confirm AI strength/timing feels equivalent to console (no perf regression).
5. **Widgets:** changing dropdowns/sliders/radio then "New Game" applies the new
   config to the next game.
6. **Web build:** `emcc` build produces `docs/index.html`; serve locally
   (`python -m http.server` from `docs/`) and confirm board renders, clicks work,
   and an AI vs AI game completes in-browser. Then enable GitHub Pages on `/docs`.
