# gui/ Reference

The raylib + raygui front end. Loaded when working on files in `gui/`. The
always-loaded overview and build commands live in the root `CLAUDE.md`, and
the full GUI verification playbook is in `TESTING.md`.

## Build notes

The graphical front end is an additive layer over the same engine (no `src/`
files change). Native build requires prebuilt raylib in `third_party/` (see
`INSTALL.md`): `.\build_gui.bat` -> `breakthrough_gui.exe`. Web build requires
emsdk + a raylib-for-web `libraylib.a` (see `INSTALL.md`), output to `docs/`:
`.\build_web.bat` (release) or `.\build_web.bat dev` (debug).

The GUI is built with `/MD` because the prebuilt raylib links the dynamic CRT.
`raylib.h` defines `WHITE`/`BLACK` as `Color` macros that collide with
`globals.h`'s board macros, so `main_gui.cpp` includes raylib/raygui first,
`#undef`s `WHITE`/`BLACK`, then includes `globals.h` and draws with explicit
`Color` literals. The GUI sets `PRNT=0` and never calls `getSettings()`,
`playerMove()`, or `printBoard()`.

## File details

| File | Purpose |
|---|---|
| `main_gui.cpp` | raylib + raygui front end. Resizable window; `ComputeLayout()` recomputes board geometry (`g_cell`/`g_boardX`/`g_boardY`/`g_boardPx`) each frame, reserving the panel width (`PANEL_W`, narrow) on the left while shown so the board sits **beside** it (not under it) and a `BADGE_STRIP` on the right for the piece-count badges; hiding the panel (`g_showPanel`) lets the board grow. Per-frame state machine (`Settings`/`WaitingForHuman`/`WaitingBeforeAI`/`ComputingAI`/`GameOver`), board rendering from the `board` global, mouse->grid click-to-move (via `tryMove*`/`playMove*`, ignored over the panel), AI turns via `moveWhite`/`moveBlack`, robust win detection by scanning goal rows + piece counts. **Auto-start:** `main()` calls `StartGame()` before entering the main loop, so the game is immediately live on open (no "Start Game" click needed). **Settings-changed notice:** `TakeSnapshot()` captures all gameplay settings into `g_snap` (`SettingsSnapshot` struct) at each `StartGame()` call; `SnapMatches()` compares `g_snap` to the current `g_white`/`g_black`/`g_boardFile` every draw call, and `DrawPanel()` shows a "Settings changed." label above the "New Game" button whenever they diverge during a live game. `DrawPieceCounts`/`DrawCountBadge` draw emblematic count badges in the right strip (Black top, White bottom) so they stay visible with the panel hidden. Under each badge, `DrawEvalReadout` (gated by `g_showEval`) shows that side's board evaluation: `now` (immediate static eval, captured before each move in `ApplyAIMove`/`HandleHumanClick`) and, for MiniMax sides, `pred` (the `g_downEval*` best-line value); `FormatEval` renders forced wins as `+WIN`/`-WIN`. Toggle the readouts with the panel "Show evaluations" checkbox or the **E** key. Player type, opener, and (for MiniMax) the evaluator are deferred `GuiDropdownBox`es (drawn last, open one on top, single-open; up to 6 specs, the two eval dropdowns added only for MiniMax sides). `PlayerConfig` carries `evaluator` + `evalParams[MAX_EVAL_PARAMS]`; `SeedEvalParams()` loads the registry defaults whenever the selected evaluator changes, and `DrawPlayerConfig` renders one `StepperRow` per parameter of the chosen evaluator (names/ranges from `g_evaluators`). The engine's `w1` arg (depth/furthest) is built by `SearchArg()`. Numeric params use a modular `StepperRow()` with a `StepStyle` enum of distinct bar+number designs (`STEP_BAR_NUM`, `STEP_SEGMENTS`, `STEP_NUMBAR`, `STEP_HANDLE`, `STEP_RULER`), all stepping with a stacked "+" (up) above "-" (down) via `DrawStackedPM` and click/drag-to-set via `ScrubBar` (`DrawFillBar` draws track+fill). Each row uses a distinct design, and the `g_stepStyle` "Sliders" `GuiComboBox` switcher forces one design on all rows. Depth uses the typeable `STEP_NUMBAR` so it can exceed the bar's 25 cap. Pacing controls are matchup-driven (`ClassifyMatchup`): AI vs AI gets slow-motion `|>` / fast-forward `>>` speed buttons (custom `DrawSpeedGlyph`, stepping `g_speedIndex` through `SPEED_NAME`/`SPEED_DELAY`) plus play/pause (`#131#`/`#132#`), step (`#134#`), and restart (`#211#`) raygui icon buttons; human vs a fast AI gets a `g_delay2s` "Min 2s per AI move" checkbox; human vs a slow (depth>5) AI or human vs human shows none. Toggle the panel with the Options/Hide button or Tab. Native/web main-loop shim at the bottom. |
| `raygui.h` | Vendored single-header raygui v4 widget library (`RAYGUI_IMPLEMENTATION` defined in `main_gui.cpp`). |
| `shell.html` | Emscripten HTML shell page for the web build. |

## Verification (GUI changes)

Always run the standard smoke test after any GUI change, the same way each time:

```powershell
.\tools\smoke_test_gui.ps1 -Build
```

This rebuilds `breakthrough_gui.exe`, launches it, waits for it to render, saves a
screenshot to `build\gui_smoke.png`, and closes it. Exit code `0` means it built
and stayed alive; non-zero means the build failed or it crashed on startup. Open
`build\gui_smoke.png` to confirm the board, pieces, and control panel render
correctly. Add `-KeepOpen` to interact with the window manually (e.g. to test
click-to-move or a new widget). For targeted widget screenshots use
`tools/gui_capture.ps1`. See `TESTING.md` for visual-inspection lessons,
matchup-gated UI capture, and MSVC/raygui gotchas.
