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
| `ai_eval.cpp` | `evaluateBoard` — board heuristic used by minimax and move dispatcher |
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

### Web build (GitHub Pages)

```powershell
.\build_web.bat            # release -> docs\index.html (+ .wasm/.js/.data)
python -m http.server -d docs
```

Commit `docs\` and enable GitHub Pages on the `/docs` folder to host it. See
[INSTALL.md](INSTALL.md) for details.

### Using the GUI

- The board is drawn on the left; the control panel is on the right.
- Pick a player type for **White** and **Black** from the dropdowns
  (Human / Uniform Random / Tiered Random / Smart Random / MiniMax), choose each
  side's **opener** (Std / Off / Def), and adjust **Smart Random** /
  **MiniMax** parameters with the controls that appear for those types. The
  default matchup is **Human (White) vs MiniMax (Black)**.
- **MiniMax depth:** the slider sets depth from 1 to 25; the number box beside it
  lets you type an exact depth (or one higher than 25, though large depths get
  very slow). The default depth is 8.
- Type a board file in the **Board** box (default `boards/board1.txt`) and press
  **Start Game** / **New Game**.
- **Human moves:** click one of your pieces to select it (legal destinations are
  highlighted), then click the destination square one row forward.
- **Pacing:** the **Speed** selector controls AI move pacing
  (Step / 0.25x / 1x / 4x / Instant). Use **Pause** and **Next move** to step
  through an AI-vs-AI game. The **Move Log** scrolls through the move history.

## Gameplay

The game runs interactively in the console. At startup you will be prompted to:

1. **Enter a board file** — e.g. `boards/board1.txt` through `boards/board5.txt`, or a puzzle like `boards/puzzle1.txt`
2. **Choose a player type** for White and Black:
   - `0` = Human
   - `1` = Uniform Random
   - `2` = Tiered Random
   - `3` = Smart Random
   - `4` = MiniMax (AI)
3. **Configure AI parameters** if applicable (search depth, weights, opener style)
4. **Choose number of games** to play and verbosity level (0 = silent, 1 = moves, 2 = full board)

### Human move format

Enter moves as `c1d` where:
- `c` = source column (letter)
- `1` = source row (number)
- `d` = destination column (letter)

Example: `d2e` moves the piece at column d, row 2 diagonally to column e.
