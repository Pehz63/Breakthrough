# Breakthrough

A C++ implementation of the Breakthrough board game by Zeph Johnson, with a
console interface and a graphical interface (raylib + raygui) that also compiles
to the web via WebAssembly.

## Prerequisites

- Visual Studio Community with the **Desktop development with C++** workload installed
- For the graphical interface: **raylib** (and, for the web build, the Emscripten
  SDK). See [INSTALL.md](INSTALL.md) for exact download/setup steps.

## Compiling

Run from the project root in any VS Code terminal (regular PowerShell works):

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe'
```

This produces `breakthrough.exe` in the project root. Intermediate `.obj` files go into `build/`.

## Compile and Run

To recompile and immediately run the result:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe' ; if ($?) { .\breakthrough.exe }
```

## Testing

Build and run the unit and integration test suite (uses [Catch2 v2](https://github.com/catchorg/Catch2/tree/v2.x), header already included in `tests/`):

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl tests\test_main.cpp tests\test_move_validation.cpp tests\test_win_detection.cpp tests\test_eval.cpp tests\test_ai_integration.cpp tests\test_game_outcomes.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /I tests /EHsc /Fo"build\\" /Fe:tests.exe'
```

Then run from the project root (required so puzzle board paths resolve correctly):

```powershell
.\tests.exe
```

For the GUI, the smoke test (`.\tools\smoke_test_gui.ps1 -Build`) and the targeted
screenshot helper (`.\tools\gui_capture.ps1`) are the main tools. See
[TESTING.md](TESTING.md) for the full verification playbook, including what to look
for visually and how to capture matchup-gated controls.

## Source files

| File | Contents |
|---|---|
| `globals.h` | Shared macros, enums, extern globals, all forward declarations |
| `globals.cpp` | Global variable definitions (board, counters, etc.) |
| `main.cpp` | `main()` game loop |
| `board_io.cpp` | `getBoard`, `reloadBoard`, `printBoard`, `loadMinimaxParams` |
| `settings.cpp` | `getSettings`, `printVictor` |
| `board_analysis.cpp` | `countChips`, `chipDiff`, `findWin*`, `canWin*` |
| `moves.cpp` | Move validation, execution, simulation, player input, move routing |
| `ai_eval.cpp` | Evaluator registry (`g_evaluators`) + each evaluator's scoring function, and `evaluateBoard` — the board heuristic used by minimax and the move dispatcher |
| `ai_random.cpp` | `playOpener*`, `pureRandom*`, `tieredRandom*`, `smartRandom*` |
| `ai_minimax.cpp` | `miniMax*`, `maxAlphaBeta`, `minAlphaBeta` |
| `gui/main_gui.cpp` | raylib + raygui front end: window, per-frame state machine, board rendering, click-to-move, widget panel, move log |
| `gui/raygui.h` | Vendored single-header raygui widget library |
| `gui/shell.html` | Emscripten HTML shell for the web build |

## Running

Run from the project root (required so board file paths resolve correctly):

```powershell
.\breakthrough.exe
```

## Graphical interface (GUI)

The GUI is an additive front end built with [raylib](https://www.raylib.com/) and
[raygui](https://github.com/raysan5/raygui). It reuses the exact same C++ game
logic and AI as the console app, so AI strength and speed are unchanged. The same
source builds both a native Windows window and a WebAssembly version for the web.

First-time setup (download raylib, plus Emscripten for the web build) is described
in [INSTALL.md](INSTALL.md).

### Native desktop build

```powershell
.\build_gui.bat
.\breakthrough_gui.exe
```

`build_gui.bat` finds Visual Studio automatically, so it runs from a plain
terminal. It produces `breakthrough_gui.exe` in the project root.

To rebuild and run in one line (only runs if the build succeeds):

```powershell
.\build_gui.bat; if ($?) { .\breakthrough_gui.exe }
```

If a previous window is still open it will lock the executable and block the
rebuild; close it first, or kill it inline:

```powershell
Get-Process breakthrough_gui -EA 0 | Stop-Process -Force; .\build_gui.bat; if ($?) { .\breakthrough_gui.exe }
```

### Web build (GitHub Pages)

```powershell
.\build_web.bat            # release -> docs\index.html (+ .wasm/.js/.data)
python -m http.server -d docs
```

Commit `docs\` and enable GitHub Pages on the `/docs` folder to host it. See
[INSTALL.md](INSTALL.md) for details.

### Using the GUI

- The window is **resizable** and the board **scales to fill it** (kept square and
  centered).
- The options live in a narrow panel on the **left**. The board sits **beside** it
  (it is not covered). Toggle the panel with the **Options** / **Hide** button in
  the top-left, or the **Tab** key. Hiding it lets the board grow to fill the
  window.
- Pick a player type for **White** and **Black** from the dropdowns
  (Human / Uniform Random / Tiered Random / Smart Random / MiniMax), choose each
  side's **opener** from a dropdown (Standard / Offensive / Defensive), and adjust
  **Smart Random** / **MiniMax** parameters with the controls that appear for those
  types. The default matchup is **Human (White) vs MiniMax (Black)**.
- **Evaluator (MiniMax):** an **Eval** dropdown picks which board-state evaluator
  the MiniMax search uses (e.g. Classic / Experimental). Each evaluator defines its
  own set of weights, and the parameter sliders below the dropdown change to match
  the selected evaluator. Switching evaluator resets that side's weights to the new
  evaluator's defaults.
- **Slider designs:** the numeric parameters use prototype controls that each show
  both a bar and the number and step with a "+" (up) above a "-" (down) button.
  Each row demonstrates a different design (Bar+number, Segments, Number+bar,
  Handle, Ruler) so you can compare them, and you can also click or drag a bar to
  set its value directly. The **Sliders** switcher at the top of the panel forces
  one design across all rows ("Per-row" restores the mixed view).
- **MiniMax depth:** the Depth control (the Number+bar design) lets you type an
  exact depth, step it with +/-, or drag its bar (which tops out at 25, though the
  typed/stepped value can go higher). Large depths get very slow. The default depth
  is 8.
- Type a board file in the **Board** box (default `boards/board1.txt`) and press
  **Start Game** / **New Game**.
- **Piece counts** are shown on the board itself as small badges (a piece icon plus
  the count) on each side, so they stay visible even with the options panel hidden.
- **Board-state evaluation** is shown under each side's count badge. `now` is the
  immediate static evaluation of the position that side faced; for a **MiniMax**
  side a second line `pred` shows the AI's predicted best-line ("downstream")
  evaluation. Numbers are white-centric: a positive value favors White, and a
  forced win shows as `+WIN` / `-WIN`. Turn off the readouts with the **Show
  evaluations** checkbox or the **E** key (useful for a hint-free PvP / PvC game).
- **Human moves:** click one of your pieces to select it (legal destinations are
  highlighted), then click the destination square one row forward.
- **Pacing** adapts to the matchup:
  - **Human vs a strong AI** (MiniMax depth > 5): no pacing controls, the AI's own
    search sets the pace.
  - **Human vs a fast AI** (shallow MiniMax or a random AI): a **Min 2s per AI
    move** checkbox so the AI does not snap back instantly.
  - **AI vs AI:** the full set, slow-motion (`|>`) / fast-forward (`>>`) buttons
    that step the speed presets (Step / 0.25x / 1x / 4x / Instant) shown between
    them, plus icon buttons for **play/pause**, **step**, and **restart**.
- The **Move Log** scrolls through the move history.

## Gameplay

The game runs interactively in the console. At startup you will be prompted to:

1. **Enter a board file** — e.g. `boards/board1.txt` through `boards/board5.txt`, or a puzzle like `boards/puzzle1.txt`
2. **Choose a player type** for White and Black:
   - `0` = Human
   - `1` = Uniform Random
   - `2` = Tiered Random
   - `3` = Smart Random
   - `4` = MiniMax (AI)
3. **Configure AI parameters** if applicable. For MiniMax this includes the search
   depth, the **evaluator** to use (Classic / Experimental / ...), that evaluator's
   weights (prompted one at a time by name), and the opener style.
4. **Choose number of games** to play and verbosity level (0 = silent, 1 = moves, 2 = full board)
5. **Show board evaluations?** (0 = no, 1 = yes). When enabled, after each move a
   line prints that side's `now` (immediate static eval) and, for a MiniMax side,
   its `pred` (predicted best-line eval). Values are white-centric (positive favors
   White); forced wins print as `+WIN` / `-WIN`.

### Board-state evaluators

The MiniMax AI scores leaf positions with a selectable **evaluator**. Each evaluator
is one entry in the `g_evaluators` registry in `src/ai_eval.cpp`: a name, a list of
parameters (display name, save-file key, default, and min/max), and a scoring
function. To add or change an evaluator you edit that one table entry and its
function body. The new evaluator and its parameters then appear automatically in
both the console prompts and the GUI's Eval dropdown / sliders, and can be saved per
side in `minimax_params.txt` via `<side>_eval` plus each weight's `<side>_<key>`.

For speed, the MiniMax search evaluates positions **incrementally**: since a move
changes only two squares, the positional score is updated by a small per-move delta
instead of rescanning the whole board at every leaf. This is internal and does not
change the scores, so AI play is identical — just faster when positional (wall /
column / advance) weights are enabled at higher depths.

### Human move format

Enter moves as `c1d` where:
- `c` = source column (letter)
- `1` = source row (number)
- `d` = destination column (letter)

Example: `d2e` moves the piece at column d, row 2 diagonally to column e.
