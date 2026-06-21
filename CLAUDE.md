# CLAUDE.md - Breakthrough Project Reference

## Standing Instructions

- **Never run `git commit` or `git push`.** The developer makes all commits manually.
- **Todo list:** Project tasks are tracked in `todo.md`. When a task is completed, cross it out using Markdown strikethrough (`~~like this~~`) rather than deleting it.
- **Writing style:** Avoid semicolons and em dashes. Use a comma or period instead, restructuring the sentence if needed. Avoid special Unicode characters like arrows or comparison signs. Use standard keyboard equivalents instead, such as `->` for a right-pointing arrow and `>=` for a greater-than-or-equal sign.
- **After every functional change:**
  1. Update `README.md` for any section affected by the change (build command, game rules, AI descriptions, etc.)
  2. Update this file (`CLAUDE.md`) to reflect new files, renamed functions, or changed behavior
  3. Tell the developer **how to test** the change and **what new behavior to expect**
  4. Suggest 2-3 **candidate commit messages** (message text only, not the full `git commit` command), then give a **top recommendation**
     - Before suggesting, check `git status` to see what is actually uncommitted. If this change will be bundled with other uncommitted work from the session, write the message to cover **all** of those changes together, not just the latest one
     - Use `Add` for files being committed for the first time, `Update` only if the file was already in a prior commit

---

## Project Overview

**Breakthrough** is an 8x8 abstract board game implemented in C++ with a console UI and multiple AI difficulty levels. White pieces start at rows 6-7 and move upward. Black pieces start at rows 0-1 and move downward. A player wins by advancing a piece to the opposite back row or capturing all opponent pieces.

- **Language:** C++ (C++11)
- **Compiler:** MSVC (`cl`), the primary build tool
- **Alternative build:** CMake (`CMakeLists.txt`), which is not the primary workflow
- **Entry point:** `.\breakthrough.exe` from project root

---

## Build & Run

**`cl` is not on the default PATH.** Every `cl` build must first load the MSVC
environment via `vcvars64.bat`. From PowerShell, wrap the build in
`cmd /c '"<vcvars64.bat>" && cl ...'` as shown below (path matches README; adjust
the Visual Studio edition/version if yours differs). The bare `cl ...` lines that
follow are the compile command itself, after the environment is loaded.

### Build
```
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe'
```

### Tests
```
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl tests\test_main.cpp tests\test_move_validation.cpp tests\test_win_detection.cpp tests\test_eval.cpp tests\test_ai_integration.cpp tests\test_game_outcomes.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /I tests /EHsc /Fo"build\\" /Fe:tests.exe'
```

### Run
```
.\breakthrough.exe
```
When prompted for a board file, enter e.g. `boards\board1.txt`.

### GUI (raylib + raygui)
The graphical front end is an additive layer over the same engine (no `src/` files
change). Native build requires prebuilt raylib in `third_party/` (see `INSTALL.md`):
```
.\build_gui.bat
.\breakthrough_gui.exe
```
Web build requires emsdk + a raylib-for-web `libraylib.a` (see `INSTALL.md`), output
to `docs/`:
```
.\build_web.bat          # release;  .\build_web.bat dev for a debug build
```
Notes for working on the GUI: it is built with `/MD` because the prebuilt raylib
links the dynamic CRT. `raylib.h` defines `WHITE`/`BLACK` as `Color` macros that
collide with `globals.h`'s board macros, so `main_gui.cpp` includes raylib/raygui
first, `#undef`s `WHITE`/`BLACK`, then includes `globals.h` and draws with explicit
`Color` literals. The GUI sets `PRNT=0` and never calls `getSettings()`,
`playerMove()`, or `printBoard()`.

---

## File Structure

### Root
| File | Purpose |
|---|---|
| `README.md` | User-facing docs: build, run, game rules, move notation |
| `CLAUDE.md` | Claude reference and workflow instructions (this file) |
| `CMakeLists.txt` | Alternative CMake build (not primary) |
| `minimax_params.txt` | Saved MiniMax weights, loaded automatically when MiniMax player is selected |
| `.gitignore` | Excludes `.exe`, `.obj`, `build/`, and `third_party/` |
| `INSTALL.md` | Setup notes: VS C++ workload, raylib download, emsdk for the web build |
| `build_gui.bat` | Native GUI build (MSVC + raylib, `/MD`) -> `breakthrough_gui.exe` |
| `build_web.bat` | Emscripten/WASM GUI build -> `docs/index.html` (release or `dev`) |
| `tools/smoke_test_gui.ps1` | Standard GUI smoke test: build/launch/screenshot/close, exits non-zero on crash (run from project root) |
| `TESTING.md` | Verification playbook: console/engine test steps, the GUI smoke-test workflow, visual-inspection lessons, matchup-gated UI capture, and MSVC/raygui gotchas |
| `tools/gui_capture.ps1` | Targeted screenshot helper: finds the `GLFW30` window by process id and crops its client area for inspecting individual widgets (complements `smoke_test_gui.ps1`) |

### `src/`
| File | Purpose |
|---|---|
| `globals.h` | Master header: macros (`EMPTY='.'`, `WHITE='W'`, `BLACK='B'`, `SIZE=8`), enums, `extern` globals, and all function prototypes. Included by every `.cpp`. |
| `globals.cpp` | Definitions of all global variables (`board`, `g_whiteCount`, `PRNT`, etc.). Separated from `main.cpp` so the test executable can link them without pulling in `main()`. |
| `main.cpp` | Top-level game loop: seeds RNG, loads board, calls `getSettings()`, dispatches turns, tracks per-player time, and accumulates scores over multi-game sets. Holds per-side `wEval`/`bEval` (evaluator index) and `wParams`/`bParams` (`int[MAX_EVAL_PARAMS]`). It also hosts parameter test-sweep mode (`testingParam` 1 = depth, 2..1+paramCount = the tested side's evaluator weights). |
| `board_io.cpp` / `board_io.h` | Board file I/O and display: `getBoard()`, `reloadBoard()`, `printBoard()`, `loadMinimaxParams()` (reads `<side>_eval`, `<side>_depth`, `<side>_opener`, and each evaluator param via `<side>_<key>`, falling back to legacy `<side>_<key>_weight` names) |
| `settings.cpp` / `settings.h` | Interactive CLI configuration. `getSettings()` prompts for player types and, for MiniMax, the evaluator and its weights (looped from the registry by name via `getEvaluatorSettings()`); also game count and verbosity. `printVictor()` displays winner and timing. |
| `moves.cpp` / `moves.h` | All move logic: dispatch (`moveWhite`/`moveBlack`), human input parsing, full validation (`tryMoveWhite/Black`), fast AI validation (`tryMoveQuickWhite/Black`), execution (`playMoveWhite/Black`), reversible simulation (`simulateMove`/`unsimulateMove`). `simulateMove`/`unsimulateMove` also maintain the incremental positional accumulator `g_evalPos` (via `evalPosLocal`) when `g_evalIncremental` is set. |
| `board_analysis.cpp` / `board_analysis.h` | Chip counting, row-level chip difference, one-step win detection: `findWinWhite/Black()`, `canWinWhite/Black()` |
| `ai_eval.cpp` / `ai_eval.h` | Pluggable board evaluators. A registry `g_evaluators[]` (with `g_evalCount`) lists each `EvalDef` (name, parameter list of `EvalParamDef{name,key,def,lo,hi}`, scoring `fn`). Two ship: `evalClassic` (original heuristic: near-end win detection at rows `SIZE-2`/`1`, turn bonus, wall/column structure bonuses, chip-diff base) and `evalExperimental` (Classic plus an "Advance" weight, identical to Classic when that weight is 0). `evaluateBoard(turnColor, evaluator, params)` dispatches by index; a convenience overload `evaluateBoard(turnColor, turn, chip, wall, col)` calls Classic. `MAX_EVAL_PARAMS` is defined in `globals.h` so param-array callers need only that header. To add an evaluator: append one `EvalDef` and write its `fn` — both UIs pick it up automatically. End-row win detection is handled before calling this. **Incremental eval:** for the minimax hot path, the positional part (structure + advance) is cached in `g_evalPos` instead of rescanned per leaf. `evalBeginSearch`/`evalEndSearch` (RAII-guarded in minimax) seed/tear it down; `evalPosFull` does the full scan; `evalPosLocal` computes a move's local delta (an owner bounding-box for structure + the two changed squares' advance) applied by `simulateMove`/`unsimulateMove`; `evalLeaf` combines `g_evalPos` with the already-incremental `g_chipDiff` and turn term. An `EvalDef.incremental` flag opts in (requires the standard `p[0]=turn,p[1]=chip,p[2]=wall,p[3]=column,p[4]=advance` layout); non-incremental evaluators fall back to a full `evaluateBoard` at the leaf. `nearWinCheck` is shared so the fast and full paths can't diverge. |
| `ai_random.cpp` / `ai_random.h` | Three random AI strategies (`pureRandomMove`, `tieredRandomMove`, `smartRandomMove`) and opening-sequence logic (`playOpenerWhite/Black`) |
| `ai_minimax.cpp` / `ai_minimax.h` | Alpha-beta minimax: `miniMaxWhite/Black()` top-level search, `maxAlphaBeta`/`minAlphaBeta` recursive pruning, capture-first move ordering, win-decay for fastest wins |

### `gui/`
| File | Purpose |
|---|---|
| `main_gui.cpp` | raylib + raygui front end. Resizable window; `ComputeLayout()` recomputes board geometry (`g_cell`/`g_boardX`/`g_boardY`/`g_boardPx`) each frame, reserving the panel width (`PANEL_W`, narrow) on the left while shown so the board sits **beside** it (not under it) and a `BADGE_STRIP` on the right for the piece-count badges; hiding the panel (`g_showPanel`) lets the board grow. Per-frame state machine (`Settings`/`WaitingForHuman`/`WaitingBeforeAI`/`ComputingAI`/`GameOver`), board rendering from the `board` global, mouse->grid click-to-move (via `tryMove*`/`playMove*`, ignored over the panel), AI turns via `moveWhite`/`moveBlack`, robust win detection by scanning goal rows + piece counts. `DrawPieceCounts`/`DrawCountBadge` draw emblematic count badges in the right strip (Black top, White bottom) so they stay visible with the panel hidden. Player type, opener, and (for MiniMax) the evaluator are deferred `GuiDropdownBox`es (drawn last, open one on top, single-open; up to 6 specs, the two eval dropdowns added only for MiniMax sides). `PlayerConfig` carries `evaluator` + `evalParams[MAX_EVAL_PARAMS]`; `SeedEvalParams()` loads the registry defaults whenever the selected evaluator changes, and `DrawPlayerConfig` renders one `StepperRow` per parameter of the chosen evaluator (names/ranges from `g_evaluators`). The engine's `w1` arg (depth/furthest) is built by `SearchArg()`. Numeric params use a modular `StepperRow()` with a `StepStyle` enum of distinct bar+number designs (`STEP_BAR_NUM`, `STEP_SEGMENTS`, `STEP_NUMBAR`, `STEP_HANDLE`, `STEP_RULER`), all stepping with a stacked "+" (up) above "-" (down) via `DrawStackedPM` and click/drag-to-set via `ScrubBar` (`DrawFillBar` draws track+fill). Each row uses a distinct design, and the `g_stepStyle` "Sliders" `GuiComboBox` switcher forces one design on all rows. Depth uses the typeable `STEP_NUMBAR` so it can exceed the bar's 25 cap. Pacing controls are matchup-driven (`ClassifyMatchup`): AI vs AI gets slow-motion `|>` / fast-forward `>>` speed buttons (custom `DrawSpeedGlyph`, stepping `g_speedIndex` through `SPEED_NAME`/`SPEED_DELAY`) plus play/pause (`#131#`/`#132#`), step (`#134#`), and restart (`#211#`) raygui icon buttons; human vs a fast AI gets a `g_delay2s` "Min 2s per AI move" checkbox; human vs a slow (depth>5) AI or human vs human shows none. Toggle the panel with the Options/Hide button or Tab. Native/web main-loop shim at the bottom. |
| `raygui.h` | Vendored single-header raygui v4 widget library (`RAYGUI_IMPLEMENTATION` defined in `main_gui.cpp`). |
| `shell.html` | Emscripten HTML shell page for the web build. |

### `tests/`
| File | Purpose |
|---|---|
| `catch.hpp` | Catch2 v2 single-header test framework (no external dependency) |
| `helpers.h` | `setupBoard()`, `clearBoard()`, `runGame()` -- shared test utilities |
| `test_main.cpp` | Catch2 entry point (defines `CATCH_CONFIG_MAIN`) |
| `test_move_validation.cpp` | Unit tests for `tryMoveWhite/Black` and `tryMoveQuickWhite/Black` |
| `test_win_detection.cpp` | Unit tests for `canWinWhite/Black` and `findWinWhite/Black` |
| `test_eval.cpp` | Unit tests for `evaluateBoard`, the Classic/Experimental equivalence check, and the incremental-eval walk asserting `g_evalPos` matches `evalPosFull` over make/unmake |
| `test_ai_integration.cpp` | MiniMax forced-win scenarios on hand-crafted positions |
| `test_game_outcomes.cpp` | Full-game outcome tests using puzzle boards (Black/White MiniMax vs TieredRandom) |

### `boards/`
| Files | Purpose |
|---|---|
| `board1.txt` - `board5.txt` | Standard starting configurations |
| `puzzle1.txt` - `puzzle13.txt` | Mid-game tactical positions for testing AI |

---

## Key Global State

All globals are declared `extern` in `globals.h` and defined in `globals.cpp`.

| Variable | Type | Meaning |
|---|---|---|
| `board[SIZE][SIZE]` | `char[8][8]` | Board grid indexed `[col][row]`. Values are `'W'`, `'B'`, `'.'`. |
| `g_whiteCount` | `int` | Live white piece count, updated on every capture |
| `g_blackCount` | `int` | Live black piece count, updated on every capture |
| `g_chipDiff` | `int` | `g_whiteCount - g_blackCount`, used directly in `evaluateBoard()` |
| `g_whiteAtEnd` | `int` | Count of White pieces currently on row `SIZE-1`. Updated by `simulateMove`/`unsimulateMove`. |
| `g_blackAtEnd` | `int` | Count of Black pieces currently on row `0`. Updated by `simulateMove`/`unsimulateMove`. |
| `nodesWhite` / `nodesBlack` | `int` | Minimax nodes visited this turn, printed for perf analysis |
| `g_evalPos` | `int` | Running positional eval (structure + advance) of the board, maintained incrementally by `simulateMove`/`unsimulateMove` during a search |
| `g_evalIncremental` | `bool` | True while an incremental minimax search is active (gates the `g_evalPos` updates in make/unmake) |
| `g_activeParams` / `g_activeParamCount` | `const int*` / `int` | The active evaluator's weight array and its length, used by `evalPosLocal` |
| `PRNT` | `int` | Verbosity: `0`=silent, `1`=moves only, `2`=full board states |

---

## AI Player Types (`PlayerEnum`)

| Value | Name | Description |
|---|---|---|
| 0 | Human | Console input. Move format `c1d` (source col, source row, dest col). |
| 1 | UniformRandom | All legal moves equally likely |
| 2 | TieredRandom | Prioritizes winning moves first, then captures, then normal moves |
| 3 | SmartRandom | Like TieredRandom but restricts candidates to the furthest N pieces (`p1` parameter) |
| 4 | MiniMax | Alpha-beta search with configurable depth, a selectable evaluator and its weights, and an opener |

### Opening Strategies (`OpenerEnum`)
| Value | Name | Description |
|---|---|---|
| 0 | StandardOpener | No opening sequence, plays normal moves immediately |
| 1 | OffensiveOpener | Edge pieces attack diagonally, center pieces push forward |
| 2 | DefensiveOpener | Corner-focused, protects corner pieces first |

Openers are disabled automatically once the opponent advances into the player's half of the board.

---

## Architecture Notes

- **Simulate/unsimulate pattern:** `simulateMoveWhite/Black` and `unsimulateMoveWhite/Black` apply and reverse moves in-place so minimax avoids copying the board. They also maintain incremental state across make/unmake: piece counts, `g_chipDiff`, `g_whiteAtEnd`/`g_blackAtEnd`, and (during an incremental search) the positional accumulator `g_evalPos`.
- **Incremental evaluation:** a move only changes 2 squares, so instead of rescanning all 64 at each leaf, the positional score (structure + advance) is kept in `g_evalPos` and updated by each make/unmake via a small local delta (`evalPosLocal`). The leaf (`evalLeaf`) then just adds the already-incremental `g_chipDiff` and the turn term. Seeded/torn down per search by `evalBeginSearch`/`evalEndSearch`. Guarded by an equivalence test (`test_eval.cpp`) that walks the move tree and asserts `g_evalPos` always equals a full `evalPosFull` recompute.
- **Two validation tiers:** `tryMoveWhite/Black` gives full validation with user-readable error messages for human input. `tryMoveQuickWhite/Black` skips bounds checks for AI inner loop performance.
- **Capture-first move ordering:** In minimax, moves for each piece are tried in strict priority order: actual captures (diagonal to an enemy piece), then empty diagonal advances, then the forward move. This ordering maximizes alpha-beta cutoffs. End-row win detection (`canWinWhite`/`canWinBlack`) uses `g_whiteAtEnd`/`g_blackAtEnd` for O(1) checks instead of scanning a row.
- **Win decay:** When minimax finds a forced win, the score decreases by 1 per level, incentivizing the fastest possible victory path.
- **Evaluator registry / parameter threading:** Instead of fixed positional weights, the chosen evaluator (`int evaluator`) and its parameter array (`const int* evalParams`) are threaded together through `moveWhite/Black` -> `miniMaxWhite/Black` -> `maxAlphaBeta`/`minAlphaBeta` -> `evaluateBoard`. Adding/renaming a parameter or evaluator is a one-place edit in `g_evaluators` (`src/ai_eval.cpp`); both the console (`getEvaluatorSettings`) and GUI (`DrawPlayerConfig`) generate their controls from that table.
- **`minimax_params.txt`:** Key-value config file (`key=value`, `#` comments) for persisting preferred AI settings between sessions. Per side: `<side>_eval`, `<side>_depth`, `<side>_opener`, and one key per evaluator weight (`<side>_<key>`, with legacy `<side>_<key>_weight` still honored).
- **Section banners:** The longer source files (`moves.cpp`, `ai_random.cpp`, `settings.cpp`, `gui/main_gui.cpp`) carry `// === LABEL ===` banner comments before each logical section, so `grep "// ==="` over a file returns its outline and `grep "// === FAST VALIDATION"` jumps to a region. Labels mirror the per-file descriptions in this document. `main_gui.cpp` also lists the sections in its header comment. Keep banners in sync when adding or moving a section. Vendored headers (`tests/catch.hpp`, `gui/raygui.h`, `third_party/...`) are excluded.

---

## Verification Checklist

See [TESTING.md](TESTING.md) for the full verification playbook (visual-inspection
lessons, the `tools/gui_capture.ps1` screenshot helper, matchup-gated UI capture,
and MSVC/raygui gotchas). The condensed checklist:

Use this after any change to confirm nothing is broken:

1. Build succeeds with the `cl` command above (no errors or warnings introduced)
2. `.\tests.exe` passes all 81 assertions (run from project root)
3. `.\breakthrough.exe` launches and shows the settings prompts
4. Enter `boards\board1.txt` when asked for a board file. Confirm the board displays correctly.
5. Run a quick Human vs. UniformRandom game (a few moves) to confirm basic flow
6. **For AI changes:** run MiniMax (depth 3) vs. MiniMax (depth 3) with `PRNT=1`. Confirm `nodesWhite`/`nodesBlack` stats print and the game completes.
7. **For eval/weight changes:** compare win rates over a 10-game batch before and after

### For GUI changes (`gui/`)

Always run the standard smoke test after any GUI change, the same way each time:

```powershell
.\tools\smoke_test_gui.ps1 -Build
```

This rebuilds `breakthrough_gui.exe`, launches it, waits for it to render, saves a
screenshot to `build\gui_smoke.png`, and closes it. Exit code `0` means it built
and stayed alive; non-zero means the build failed or it crashed on startup. Open
`build\gui_smoke.png` to confirm the board, pieces, and control panel render
correctly. Add `-KeepOpen` to interact with the window manually (e.g. to test
click-to-move or a new widget).
