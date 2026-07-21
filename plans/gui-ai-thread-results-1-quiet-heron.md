# GUI AI Threading - Results

Companion results doc for a small, non-plan-driven fix: the GUI froze while an
AI was thinking. This records the change, the before/after measurement, and the
correctness reasoning. There was no separate plan file (the work came straight
from a `todo.md` item), so this doc stands alone.

## Problem

`ApplyAIMove()` called `moveWhite()` / `moveBlack()` synchronously inside the
per-frame `Update()`. The search can take seconds (deep MiniMax), and during it
the render thread was blocked inside the engine, so the whole window stopped
pumping messages: no redraw, resize, panel interaction, or button clicks until
the move returned. That is the "GUI hangs when processing an AI" report.

## Change

Move the AI search off the render thread on the native build. `gui/main_gui.cpp`
only:

- **Background worker.** `LaunchAIMove()` starts `AiWorker()` on a `std::thread`;
  `AiWorker` runs the same `moveWhite`/`moveBlack` and sets an atomic `g_aiDone`
  when finished. The `ComputingAI` state polls `g_aiDone` each frame and calls
  `FinalizeAIMove()` once the worker joins. The window keeps redrawing the whole
  time.
- **Render view snapshot.** The search mutates the engine globals (`board`, the
  piece counts) via simulate/unsimulate while it runs, so the renderer must not
  read them mid-search. Draw code now reads a snapshot -- `g_viewBoard` /
  `g_viewWCount` / `g_viewBCount` -- written only by the main thread via
  `SyncView()`. The pre-move position is synced into the view before the worker
  launches (it stays on screen during the search) and `AfterMove()` re-syncs
  after the move lands, so both human and AI moves refresh the view.
- **Config snapshot.** The options panel stays interactive during the search, so
  `LaunchAIMove()` copies the mover's side + `PlayerConfig` into `g_aiSide` /
  `g_aiCfg` and the worker reads only those. A user dragging a stepper mid-search
  cannot race the worker's reads of the evaluator weights / depth.
- **Teardown guard.** `JoinAiIfRunning()` blocks before any main-thread action
  that reloads the globals -- `StartGame()` (New Game) and program exit -- so the
  board is never reloaded out from under a running search.
- **Web build unchanged.** This Emscripten build has no pthreads, so the
  `#if !defined(PLATFORM_WEB)` path keeps the synchronous behavior (worker runs
  inline in `LaunchAIMove`, window still stalls during a deep search). Threaded
  responsiveness is a native-only win here.

## Measurement

The window's message pump was probed with `SendMessageTimeout(hwnd, WM_NULL, ...,
SMTO_ABORTIFHUNG, 120ms)` every ~160ms for 12s while an AI-vs-AI MiniMax depth-12
game ran (each search takes multiple seconds, so a blocked pump is easy to catch).
A returned failure = the pump was stalled past 120ms = frozen.

| Build | Responsive probes | Frozen probes |
|---|---|---|
| Threaded (this change) | 209 | **0** |
| Synchronous control (old behavior, forced on via a temp `GUI_SYNC_CONTROL` define) | 0 | **190** |

The control run confirms the probe actually detects the freeze: the old
synchronous path is frozen for the entire 12s (one depth-12 search never returns
inside a single probe window), while the threaded build never stalls. Note: the
naive `Process.Responding` property is useless here -- it only flips after a ~5s
hang, so sub-5s searches read as "responsive" under it either way. That is why
the `SendMessageTimeout` probe was used instead.

The `GUI_SYNC_CONTROL` define and the temporary AI-vs-AI depth-12 default in
`main()` were scaffolding for this measurement only and were removed afterward;
the shipped default matchup is back to Human (White) vs MiniMax (Black).

## How to test

- `.\tools\smoke_test_gui.ps1 -Build` -- builds `breakthrough_gui.exe`, launches,
  screenshots, closes. Exit 0 = built and stayed alive.
- Manually: open the GUI (default Human vs MiniMax), make a move, and while the AI
  thinks, drag the window / toggle the Options panel. It stays responsive, the
  board shows the pre-move position, then updates to the AI's reply.
- For a stress view, set both sides to MiniMax at a high depth and confirm the
  window still resizes and the speed / pause / step buttons respond during each
  search.

## Correctness gotchas

- **Data race on `board` and counts.** The worker mutates these during search, so
  the fix routes all rendering through the main-thread-only view snapshot. Only
  two draw reads needed redirecting (`DrawBoard`'s piece read, `DrawPieceCounts`'s
  badge counts); the hint / hover reads of `board` are gated to `WaitingForHuman`,
  when no worker is live and the view equals `board`.
- **Data race on config.** The panel is live during `ComputingAI`; snapshotting
  the config into `g_aiCfg` at launch closes it.
- **Reload during search.** New Game / exit must join first (`JoinAiIfRunning`),
  or `reloadBoard` would race the search and crash.
- **`immediateEvalForDisplay` timing.** It reads the board, so it is called on the
  main thread in `LaunchAIMove` before the worker starts, not after.

## Measurement caveats

- The responsiveness numbers are from one machine (Windows 10, MSVC) and one probe
  cadence. They demonstrate presence/absence of freezing, not a latency
  distribution.
- The probe measured AI-vs-AI at depth 12 specifically to guarantee multi-second
  searches. The human-vs-AI default at depth 8 has shorter searches; the fix
  applies identically but a freeze there would have been briefer and harder to
  show.

## Test-suite status

The Catch2 suite (`tests.exe`) is not affected by this change -- it does not
compile `gui/main_gui.cpp`. At the time of this work the suite was 97/98 with one
pre-existing failure at `tests/test_ranking.cpp:1133` (the `rank posgen`
globally-unique-enc assertion), reproducible against the committed, unmodified
`ranking/matches.jsonl` and independent of this GUI work. Flagged to the developer
separately.

## Future Work

- **Web responsiveness.** The web build still stalls on a deep search because
  there are no pthreads in the current Emscripten setup. Confirming/refuting "the
  web GUI can be made responsive" would need either a pthreads-enabled build
  (`-pthread` + SharedArrayBuffer + cross-origin isolation headers) with the same
  worker path, or a cooperative search that yields to the main loop. Neither was
  attempted here.
- **Cancel an in-flight search.** New Game currently blocks until the worker
  finishes (`JoinAiIfRunning`). For a very deep search that join can hang the UI
  action for seconds -- the exact freeze this change removed, just relocated to
  the New Game / close path. A cooperative cancel flag checked inside the search
  would settle whether that residual stall matters in practice.
- **AI-vs-AI throughput.** With the search threaded, the render loop and the
  search now overlap. Whether that changed effective moves-per-second at a fixed
  speed setting was not measured.

## Ideas This Inspired

- A tiny on-screen "thinking..." indicator or a spinner while `ComputingAI` is
  active, now that the window can actually animate during the search.
- Surface live search telemetry (current depth, nodes) from the worker to the
  panel during the search, since the UI is no longer frozen and could poll it.
- A per-side move-time readout, cheap to add now that move timing spans real
  wall-clock frames rather than a single blocked call.
