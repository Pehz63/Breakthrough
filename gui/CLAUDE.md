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

**Agent selection: portable core, native-only ID codec.** Both `build_gui.bat`
and `build_web.bat` link `src\agents.cpp`, `src\explorers.cpp`, and
`src\choosers.cpp` in addition to the engine sources every target needs (none of
the three touch `<windows.h>`), so **both platforms** can compose an `AgentSpec`
and drive either side through the same structured dropdown/slider agent editor
(`DrawAgentEditor`) -- any explorer/evaluator, budgets, search feature toggles, a
learned model slot, a policy brain, dilution, an opener. Only `build_gui.bat`
additionally links `src\ranking.cpp`, for the canonical-ID codec
(`rankAgentId`/`rankAgentFromId`, `src/ranking.h`) `rank.exe` validates
`ranking/roster.txt` with: a free-text ID box that round-trips through the
project's exact ID grammar, plus a persisted history of previously-used IDs.
`ranking.cpp` unconditionally `#include <windows.h>` (for `GetProcessTimes`),
which does not compile under Emscripten, so the web build cannot link it -- every
symbol/block in `main_gui.cpp` that needs it is guarded by `#if
!defined(PLATFORM_WEB)` (grep that string to find the exact boundary). Where the
native path needs a canonical id string for display/change-detection purposes
(e.g. `MakeDefaultBlackAgent`, the Apply handler), the web path substitutes
`agentDescribe()` (`src/agents.h`, portable, not re-parseable but suffient for a
label). Model-slot loading is unified behind one `GuiLoadAgentModels(spec, err)`
name: native reuses `rankLoadAgentModels` (`src/ranking.h`), web gets a small
portable reimplementation of just the slot-file naming convention (mirrors
`ranking.cpp`'s private `slotFile()`) plus a direct call to the otherwise-portable
`mlLoadSlot` (`src/ml_eval.h`, forward-declared locally rather than included, for
the same reason as `GUI_MODEL_SLOTS` below). `main_gui.cpp` cannot `#include
"ml_eval.h"` (for its `ML_SLOTS` constant, or for `mlLoadSlot`'s declaration) on
**either** platform, because that header pulls in `ml_model.h`'s `class Model`,
which collides with raylib's own `struct Model` (a 3D model asset) -- a type-name
collision `#undef` cannot fix, unlike `WHITE`/`BLACK`. `main_gui.cpp` instead
defines its own `GUI_MODEL_SLOTS` constant mirroring `ML_SLOTS`. See
`TESTING.md`'s gotchas list for both of these plus the `GuiToggleGroup`
per-item-width pitfall the agent editor's toggles hit.

## File details

| File | Purpose |
|---|---|
| `main_gui.cpp` | raylib + raygui front end. **Agent selection:** `PlayerConfig` is either Human (`isHuman`, click-driven) or a full `AgentSpec` (`spec`, the same composition `agentChooseMove` and the ranked agent pool use) plus its canonical id string (`id`, kept in sync via `rankAgentId`). AI moves dispatch through `agentChooseMove(spec, side)` (`src/agents.h`), not the old narrow `moveWhite`/`moveBlack(playerType, ...)` path, so a side is expressive enough for any roster agent: any explorer/evaluator combination, node/time budgets, search feature toggles (TT/move-order/quiescence/aspiration), a learned model slot, a policy brain, dilution, and an identity-level opener. **Opener wiring:** `AgentSpec::openerKind` is otherwise inert outside `src/ranking.cpp`'s `playOneGame`/`playoutCapture` (measured 2026-08-08: 62/170 active roster agents carry one), so `AiWorker` replicates that dispatch -- a shared half-move clock `g_halfMove` (0 at `StartGame()`, incremented once per applied move in `AfterMove()`, snapshotted into `g_aiHalfMove` at launch) feeds `g_openers[spec.openerKind].fn(side, g_aiHalfMove/2, g_aiHalfMove, spec.openerArg, spec.openerArg2, victor)` before falling back to `agentChooseMove`, mirroring `playOneGame`'s `h`/`h/2` exactly. The result (`g_aiPlayedByOpener`) gates the `pred` eval readout in `FinalizeAIMove`, since an opener-played move never populates `g_downEvalWhite/Black`. **Model loading:** applying an agent (Apply in the editor) calls `GuiLoadAgentModels(spec, err)` (one name on both platforms: native reuses `rankLoadAgentModels`, `src/ranking.h`, a single-spec wrapper around `rank.exe`'s internal `loadModelSlots`; web gets a small portable reimplementation of the same slot-file convention, see "Build notes" above) so a `LearnedValue`/`LearnedPolicy` agent's model slot is actually loaded, surfacing a failure in the status line instead of silently playing on stale/absent weights. **Panel:** `DrawPlayerBlock` is now just a Human/Agent `GuiToggleGroup`, a short elided `AgentSummary` (canonical ids run past 100 characters, far more than the 210px panel), and an "Edit Agent..." button (`OpenAgentEditor`) -- the old fixed player-type dropdown, its evaluator dropdown, and the three scripted Offensive/Defensive/Standard openers (`OpenerEnum`, unrelated to the roster's pluggable `rand`/`book` opener axis) are gone; every roster agent (`rand@1`, `smart(pieces=N)@1`, any `ab(deep=K,...)@1.<evalseg>[.dil][.opener]` form) is reachable only through the id/editor path now. **Agent editor popup (`DrawAgentEditor`, `AgentEditorState g_ed`):** a `GuiWindowBox` modal (one side open at a time, `g_ed.side`, the rest of the panel `GuiLock`ed underneath) editing a working copy `g_ed.working` that only commits to `g_white`/`g_black` on Apply. **Native only** (guarded by `#if !defined(PLATFORM_WEB)`, see "Build notes"), a `GuiTextBox` at the top holds the canonical id; losing edit focus (or clicking a "Recent" history entry) parses it via `rankAgentFromId` and adopts the spec on success, or shows the parser's error text on failure (`TryApplyIdText`). On web this block is compiled out entirely -- the structured fields below are the only way to build an agent, and Apply commits `g_ed.working` directly. Below the (native-only) id box, two fixed (non-scrolling, so their open `GuiDropdownBox` lists are never scissor-clipped) rows -- Brain/Opener, then Explorer+Evaluator (Search) or Chooser (Policy), option strings built once from the registries (`OpenerOptions`/`ExplorerOptions`/`ChooserOptions`/`EvalOptions`) -- and a `GuiScrollPanel` body of `StepperRow`/checkbox fields covering every remaining grammar field (depth, feature-toggle checkboxes, aspiration window, node/time budgets, depth cap, the selected evaluator's weights or the model slot + Risk, dilution probability/depth, opener args). A handful of fields need a scratch mirror rather than binding a widget directly to the `AgentSpec` field (node budget is `unsigned long long`, dilution probability is a 0..1 `double`, the opener dropdown needs a "None" offset `AgentSpec::openerKind` doesn't have) -- `SyncEditorScratchFromWorking`/`PushEditorScratchIntoWorking` keep those one-way-authoritative-while-open, same convention as the id textbox not fighting in-progress typing. Structured edits regenerate the id preview live (`rankAgentId`) every frame the textbox isn't focused (native only). **Agent history (native only):** `gui_agent_history.txt` (repo root, gitignored, plain text, one canonical id per line, newest first, capped at `AGENT_HISTORY_MAX`=30) is loaded once at startup (`LoadAgentHistory`, each line re-validated via `rankAgentFromId`, unparseable entries e.g. a deleted model file dropped silently) and appended to on every successful Apply (`RememberAgentId`, dedup + rewrite via `SaveAgentHistory`), shared between both sides' editors since an id isn't side-specific. Web has no id textbox to build a history of, so this whole section (state, `LoadAgentHistory`/`SaveAgentHistory`/`RememberAgentId`, the `main()` startup call) is compiled out there too. **Threaded AI (native):** the search runs on a background `std::thread` so the window keeps redrawing and stays responsive while it thinks, instead of freezing inside `agentChooseMove`. Because the search mutates the engine globals (`board`, the piece counts) via simulate/unsimulate while it runs, the renderer never reads those globals directly. It reads a snapshot -- the "view" (`g_viewBoard`/`g_viewWCount`/`g_viewBCount`) -- that only the main thread writes via `SyncView()`. `LaunchAIMove()` snapshots the pre-move board (`g_aiPrev` for the move diff), the mover's side + config (`g_aiSide`/`g_aiCfg`/`g_aiHalfMove`, so the still-interactive options panel can't race the worker), and the immediate eval (`hasEval` = the mover has a search brain, gating `immediateEvalForDisplay`), syncs the pre-move view, then starts the worker (`AiWorker` sets the `g_aiDone` atomic when done). The `ComputingAI` state polls `g_aiDone` each frame and calls `FinalizeAIMove()` (reads `g_aiCfg`, the frozen snapshot the worker actually used, not the live possibly-since-edited `g_white`/`g_black`; records the downstream eval when `hasDown` = a search brain, an AlphaBeta explorer, and not opener-played; logs the move by diffing `g_aiPrev`; advances the turn) once the worker joins. `AfterMove()` calls `SyncView()`, so both human and AI moves refresh the view. `JoinAiIfRunning()` blocks any main-thread action that reloads the globals (New Game via `StartGame()`, program exit) until the worker finishes. The **web build has no pthreads here**, so it keeps the synchronous path (worker runs inline in `LaunchAIMove`, window still stalls during the search); it does get the same `AgentSpec`-driven dispatch and structured agent editor as native (see "Build notes" above for exactly what web does and does not get). Resizable window; `ComputeLayout()` recomputes board geometry (`g_cell`/`g_boardX`/`g_boardY`/`g_boardPx`) each frame, reserving the panel width (`PANEL_W`, narrow) on the left while shown so the board sits **beside** it (not under it) and a `BADGE_STRIP` on the right for the piece-count badges; hiding the panel (`g_showPanel`) lets the board grow. Per-frame state machine (`Settings`/`WaitingForHuman`/`WaitingBeforeAI`/`ComputingAI`/`GameOver`), board rendering from the `board` global, mouse->grid click-to-move (via `tryMove*`/`playMove*`, ignored over the panel or while the agent editor is open), robust win detection by scanning goal rows + piece counts. **Auto-start:** `main()` sets White human / Black `MakeDefaultBlackAgent()` (the historical MiniMax-depth-8-Classic default, now spelled as an `AgentSpec`), loads the agent history (native only), then calls `StartGame()` before entering the main loop, so the game is immediately live on open. **Settings-changed notice:** `TakeSnapshot()` captures `isHuman` + canonical `id` per side plus the board file into `g_snap` (`SettingsSnapshot`) at each `StartGame()` call (a side's id is a faithful serialization of everything about it that affects play, so this is simpler than the old per-field comparison); `SnapMatches()` compares it every draw call, and `DrawPanel()` shows a "Settings changed." label above the "New Game" button whenever they diverge during a live game. `DrawPieceCounts`/`DrawCountBadge` draw emblematic count badges in the right strip (Black top, White bottom) so they stay visible with the panel hidden. Under each badge, `DrawEvalReadout` (gated by `g_showEval`) shows that side's board evaluation: `now` (immediate static eval) and, when `hasDown`, `pred` (the `g_downEval*` best-line value); `FormatEval` renders forced wins as `+WIN`/`-WIN`. Toggle the readouts with the panel "Show evaluations" checkbox or the **E** key. Numeric params use a modular `StepperRow()` with a `StepStyle` enum of distinct bar+number designs (`STEP_BAR_NUM`, `STEP_SEGMENTS`, `STEP_NUMBAR`, `STEP_HANDLE`, `STEP_RULER`), all stepping with a stacked "+" (up) above "-" (down) via `DrawStackedPM` and click/drag-to-set via `ScrubBar` (`DrawFillBar` draws track+fill); the `g_stepStyle` "Sliders" `GuiComboBox` switcher (still in the main panel; applies to every `StepperRow` drawn anywhere, including inside the agent editor) forces one design on all rows. Depth uses the typeable `STEP_NUMBAR` so it can exceed its bar's 25 cap; node budget similarly exceeds its bar cap. Pacing controls are matchup-driven (`ClassifyMatchup`, now reading `isHuman`/`spec.brain`/`spec.explorer`/`spec.depth`): AI vs AI gets slow-motion `|>` / fast-forward `>>` speed buttons (custom `DrawSpeedGlyph`, stepping `g_speedIndex` through `SPEED_NAME`/`SPEED_DELAY`) plus play/pause (`#131#`/`#132#`), step (`#134#`), and restart (`#211#`) raygui icon buttons; human vs a fast AI gets a `g_delay2s` "Min 2s per AI move" checkbox; human vs a slow (an AlphaBeta search past depth 5) AI or human vs human shows none. Toggle the panel with the Options/Hide button or Tab. Native/web main-loop shim at the bottom. |
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
