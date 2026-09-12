# gui/ Reference

The raylib + raygui front end. Loaded when working on files in `gui/`. The
always-loaded overview and build commands live in the root `CLAUDE.md`, and
the full GUI verification playbook is in `TESTING.md`.

## Build notes

The GUI is an additive layer over the engine. Native: `.\build_gui.bat` ->
`breakthrough_gui.exe` (needs the prebuilt raylib in `third_party/`, see
`INSTALL.md`). Web: `.\build_web.bat` -> `build\web\index.html` (+ `.js`,
`.wasm`, `.data`), `.\build_web.bat dev` for a debug build. It needs the emsdk
and a raylib-for-web `libraylib.a` (`INSTALL.md` section 3), and activates
`third_party\emsdk` itself when `emcc` is not on PATH. `build_web.sh` is the
same build for Linux and macOS (it honors an `OUTDIR` environment variable), and
`.github/workflows/web.yml` runs it on an Ubuntu runner and deploys the page to
GitHub Pages (`INSTALL.md` section 3d). Keep the two scripts' source lists and
flags identical. A workflow run's job log needs admin rights to read through
the API, so a failed build posts the last 60 lines of `build_web.sh`'s output as
an error annotation, which anyone can read:
`curl https://api.github.com/repos/Pehz63/Breakthrough/actions/runs?per_page=5`
for the run, `.../actions/runs/<run>/jobs` for the job id, then
`.../check-runs/<job id>/annotations`.

Both builds link the same set: `gui\main_gui.cpp gui\gui_engine.cpp
gui\gui_library.cpp`, the engine link set, and `src\agents.cpp
src\explorers.cpp src\choosers.cpp src\ranking.cpp src\ai_gumbel.cpp`.
`ranking.cpp` compiles under Emscripten, so the canonical-ID codec
(`rankAgentId` / `rankAgentFromId` / `rankReportId` / `rankSlotFile`,
`src/ranking.h`) is available on both platforms and there are no
`PLATFORM_WEB` feature forks beyond the ones listed below.

Web build specifics (`build_web.bat`):
- `em++`, not `emcc`, or the link fails on undefined C++ runtime symbols.
- `-fwasm-exceptions`, because a few engine parsers use `try { std::stoi } catch (...)`.
- `-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB -sSTACK_SIZE=8MB`. The
  default Emscripten stack is far too small for the recursive search.
- Preloads `boards/`, `gui/presets.txt`, `models/lin_value.txt`,
  `models/lin_policy.txt`, and every model file a preset names
  (`tools\web_preloads.ps1` prints the `--preload-file` list, mirroring
  `rankSlotFile`). A preset's id carries its model file's hash, so the file must
  be bundled byte-identical at the same relative path.
- The bundled model files come from `gui/web_models/`, not `models/`. The
  canonical id's hash (`rankFileHash8`) is over the file's bytes as they sit in
  the Windows working tree, which are CRLF, while git stores them LF, so a Linux
  checkout of `models/` hashes differently. Some preset slots (10, 76) are not
  tracked in git at all. `gui/web_models/.gitattributes` (`* -text`) keeps the
  copies byte-exact on every platform. `web_preloads.ps1` checks every copy's
  hash against its preset id, and `-Sync` refreshes the copies from `models/`.
- Model files are CRLF in a Windows checkout and the web build's text streams
  do not strip `\r`, which is why `loadModel` (`src/ml_model.cpp`) strips it.
- Timers use `GetTime()` differences, never a sum of `GetFrameTime()`. On the
  web, raylib's `GetFrameTime()` covers only the frame's own work, not the
  browser's wait between frames, so a summed delay runs many times too long.

MSVC and raylib gotchas: the GUI is built with `/MD` (the prebuilt raylib links
the dynamic CRT). `raylib.h` defines `WHITE`/`BLACK` as `Color` macros that
collide with `globals.h`'s board macros, so `main_gui.cpp` includes raylib/raygui
first, `#undef`s them, then includes `globals.h` and draws with explicit
`Color` literals. `ml_model.h`'s `class Model` collides with raylib's
`struct Model`, a type name `#undef` cannot fix, so `gui_engine.cpp` (which
includes no raylib header) is the only GUI file that includes `ml_eval.h`. The
GUI sets `PRNT=0` and never calls `getSettings()`, `playerMove()`, or
`printBoard()`.

## Threading contract (why the window never freezes)

The engine keeps its state in process globals (board, piece counts, incremental
accumulators, the transposition table, killers, model slots), and a search
mutates them for its whole duration. So exactly one thread touches them: the
engine service thread in `gui_engine.cpp`. The UI thread keeps the authoritative
position itself as a `GuiPos`, validates and applies human moves with the pure
`guiIsLegal` / `guiApplyMove` helpers, and hands the engine jobs that carry a
copy of the position:

- **Move job** (`engRequestMove`): loads the model slot if needed, sets the
  board, takes the static eval, then mirrors `playOneGame` in
  `src/ranking.cpp`: the agent's identity-level opener first
  (`g_openers[k].fn(side, halfMove/2, halfMove, ...)`), else
  `agentChooseMove`, then clears the root filter. The move is recovered by
  diffing the board. `engNewGame` clears the TT and the retain purses, as
  `playOneGame` does per game.
- **Analysis job** (`engStartAnalysis`): iterative deepening where each step
  scores ONE root move exactly (apply it, run `miniMaxWhite/Black(d-1)` from the
  opponent's side, read `g_downEval*`), so every root move gets a real score at
  each depth, not just the principal variation. The reply arrow is the diff of
  the board after that search. Analysis TT entries are salted
  (`params[MAX_EVAL_PARAMS-1] = 0x6A11`, a slot no evaluator reads) so they never
  collide with an agent's entries, though they share the table's capacity.
  Stops at the depth limit, the time limit, or a proven result.
- **Priority and abort:** a move request pre-empts analysis. The UI aborts a
  running step by setting `g_nodeDeadline = 1` (under the service lock, and only
  while that step is still the one running), which makes `budgetTripped()` turn
  every node into a leaf. TT stores are skipped once the budget trips, so an
  aborted step does not pollute the table. The aborted step is discarded and
  retried after the move.
- MSVC's `rand()` state is per thread, so the engine thread seeds its own.
- **Web** (no pthreads): the same API, but `engTick()` runs the work inline: a
  move job completes within one frame, and analysis runs in slices of about
  10 ms per frame, declining to start a depth whose predicted step exceeds
  120 ms.

## File details

| File | Purpose |
|---|---|
| `main_gui.cpp` | The front end. Sections are marked `// ===` (grep it). **State:** `PlayerConfig` per side (Human, or an `AgentSpec` + canonical id + optional label), `g_pos` / `g_history` / `g_moves` (position, undo stack, move log), `AppState` (`WaitingForHuman`, `WaitingBeforeAI`, `ComputingAI`, `GameOver`, `Stopped`). **Flow:** `StartGame`, `ApplyMove` (shared by human and agent moves), `Undo` (back to the last human turn, one move and a pause in agent vs agent), `LaunchAIMove` -> `engRequestMove`, polled with `engTakeMoveResult` and applied by `FinalizeAIMove`. **Analysis:** `UpdateAnalysis` restarts the engine's analysis whenever the position or the settings hash (`AnalysisHash`) changes and stops it when unwanted. `DrawArrows` draws the top N lines (green / amber / orange / gray by rank, width by rank), white-centric score pills, and a dashed red reply to the best line. `DrawEvalBar` sits left of the board (learned evaluators map linearly over +/-900, heuristics through tanh at 2.5 chips). **Panel:** Play / Analysis / View tabs (`DrawPlayTab`, `DrawAnalysisTab`, `DrawViewTab`), or simple mode (`DrawSimplePanel`). **Modals:** agent library (`DrawLibrary`), agent editor (`DrawAgentEditor`, canonical id box + structured fields, both views of one `AgentSpec`), model picker (`DrawModelPicker`), save favorite. Open dropdowns go through `DeferDropdown` / `FlushDropdowns` so their lists draw on top and lock the controls under them. **Widgets:** `StepperRow` with five bar+number designs (`StepStyle`), forced globally by the View tab's Sliders switch. **Style:** `ApplyDarkStyle` sets raygui's DEFAULT palette to match the app's dark panels. **Persistence:** `SaveAllSettings` / `LoadStartupSettings` via `gui_settings.txt` (players as canonical ids). **Capture mode:** `--capture <png> [--frames N] [--size WxH] [--scenario a,b] [--moves m1,m2]` renders into a hidden window's offscreen texture and saves the last frame (`ParseArgs`, `ApplyScenario`, `PlayMovesText`), reading and writing none of the user's files. **Web:** simple mode only, hints off at start, URL options `?mode=white|black|watch&level=easy|medium|hard&hints=0|1` (`ApplyWebUrlOptions`). |
| `gui_engine.h/.cpp` | The engine service described above, plus the pure `GuiPos` rule helpers (`guiIsLegal`, `guiLegalMoves`, `guiApplyMove`, `guiWinner`, `guiCountPieces`, `guiMoveText`, `guiLoadBoardFile`). Includes no raylib header. |
| `gui_library.h/.cpp` | Where agent ids come from, all plain text files read on the UI thread: `ranking/standings.tsv` (header-driven columns, active roster by head), `ranking/CHAMPION.md`'s Summary table, `gui/presets.txt`, `gui_favorites.txt`, `gui_agent_history.txt` (30 most recent). Also the model catalog (a background scan of every slot file via `rankSlotFile`, header lines only, each slot tagged with its best standings Elo), board file discovery, and `gui_settings.txt` key=value settings. |
| `presets.txt` | Curated agents: `role | name | canonical id | description`. Roles `easy` / `medium` / `hard` are simple mode's difficulties and `watch_white` / `watch_black` its Watch matchup. Every learned model a preset names is bundled into the web build. |
| `raygui.h` | Vendored single-header raygui 4.0 (`RAYGUI_IMPLEMENTATION` in `main_gui.cpp`). |
| `shell.html` | Emscripten page shell: a full-window canvas (the app is resizable, so raylib sizes the canvas to the browser window) and a loading line. |
| `web_models/` | Byte-exact copies of the preset model files the web build bundles, at their `models/...` relative paths. `.gitattributes` turns off line-ending conversion. Refresh with `tools\web_preloads.ps1 -Sync`. |

Local, gitignored files the native GUI writes next to the exe:
`gui_settings.txt`, `gui_favorites.txt`, `gui_agent_history.txt`.

## Verification (GUI changes)

```powershell
.\tools\smoke_test_gui.ps1 -Build     # build, run hidden, screenshot to build\gui_smoke.png
.\tools\gui_shot.ps1 -All             # every scenario -> build\gui_shots\*.png
.\tools\gui_shot.ps1 -Scenario analysis -Moves "c1c,f6f"
```

Both run the exe in `--capture` mode: the window is created hidden, nothing takes
focus, and nothing is shown on screen, so they are safe while the developer is
in a full-screen game. `smoke_test_gui.ps1 -Visible` (a real window and a
full-screen grab) refuses to run while a Steam or Epic game is running. Open the
PNGs and read them: an exit code of 0 does not catch invisible text or overlap.
See `TESTING.md` for the visual-inspection lessons and raygui gotchas.

Web: `.\build_web.bat`, then `.\tools\web_shot.ps1` (real-time screenshots of
Watch, Hard, and Easy in hidden headless Chrome, into `build\web_shots\`). For a
live look, `python -m http.server -d build\web` and open
`http://127.0.0.1:8000/?mode=watch&hints=1`.
