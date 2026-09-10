# GUI overhaul: results

Companion to `plans/gui-overhaul-plan-1-copper-kestrel.md`. Session of
2026-09-10.

## What changed

| Area | Change |
|---|---|
| `gui/gui_engine.h/.cpp` (new) | Engine service thread that owns every engine global. Move jobs mirror `playOneGame`'s opener-then-`agentChooseMove` dispatch. Analysis jobs score every root move exactly at each depth (one search per root move), with a TT context salt. Abort by forcing `g_nodeDeadline`. Web: the same API run inline in bounded slices. Pure `GuiPos` rule helpers for the UI thread. |
| `gui/gui_library.h/.cpp` (new) | Standings, champions, presets, favorites, recent ids, a background model-catalog scan (1725 slot files found on this machine), board discovery, settings file. |
| `gui/presets.txt` (new) | Easy / Medium / Hard / Watch agents (table in the plan). |
| `gui/main_gui.cpp` (rewritten) | Play / Analysis / View tabs, recommended-move arrows with scores and a reply arrow, eval bar, agent library, agent editor, model picker, favorites, piece and board themes, flip, drag moves, undo, simple mode, dark raygui palette, `--capture` mode, web URL options. |
| `gui/shell.html` | Full-window canvas and a loading line. |
| `build_gui.bat`, `build_web.bat` | Link the new files. Web: `em++`, `ranking.cpp`, `-fwasm-exceptions`, memory and stack flags, preloads, output `build\web\`, auto-activates `third_party\emsdk`. |
| `tools/smoke_test_gui.ps1` | Hidden capture by default. `-Visible` refuses while a Steam or Epic game runs. |
| `tools/gui_shot.ps1`, `tools/web_preloads.ps1` (new) | Scenario screenshots, and the web build's model-file list. |
| `src/ml_model.cpp` | `loadModel` strips a trailing `\r` from each line. |
| Docs | README GUI + web sections, INSTALL web setup, TESTING GUI section and gotchas, root, `gui/`, and `tools/` CLAUDE.md, todo.md, `Docs/Memories/gui-work-etiquette.md`. |

## How to test

```powershell
.\build_gui.bat; if ($?) { .\breakthrough_gui.exe }      # interactive
.\tools\smoke_test_gui.ps1 -Build                         # hidden, build\gui_smoke.png
.\tools\gui_shot.ps1 -All                                 # hidden, build\gui_shots\*.png
.\build_web.bat; python -m http.server -d build\web       # http://localhost:8000/?mode=black&level=hard
```

What to expect: White is Human and Black the Medium preset on first launch. With
analysis on (A), green / amber / orange arrows show the best three moves for the
side to move with white-centric scores, a dashed red arrow shows the reply to the
best one, and the bar left of the board fills with White's share. Moving a piece
or changing any analysis setting restarts the analysis. The window keeps drawing
and responding while an agent thinks.

## Measurements

### UI responsiveness (native, hidden capture, this machine)

`main_gui.cpp` records each frame's update + draw time (before the frame-rate
wait) and prints the longest in capture mode. The first 5 frames (startup) are
excluded. One process per row, default settings, analysis on (evaluator
LearnedValue model 169, depth limit 30, 60 s, TT + ordering).

| Scenario | Frames | Moves played | Longest frame |
|---|---|---|---|
| `aivai`: Watch White vs Watch Black at 4x | 900 | 67 | 0.9 ms |
| `analysis,hard` with `c1c` played: Hard (model 169) moves while analysis runs | 600 | 2 | 1.0 ms |
| `nopanel`: human to move, analysis running | 600 | 0 | 0.7 ms |

Check that the instrument measures a stall: the number is the UI thread's own
work per frame, which is where a synchronous `agentChooseMove` would land (a
Hard move takes about 25 ms per `ranking/standings.tsv`'s `cpu_ms` column for
that agent). The 67 moves in the first row show the agent moves did complete
during the measured frames. Hidden-window rendering may be cheaper than a
visible window, so these are UI-thread work times, not display latency.

### Analysis depth (native, one observation)

From the `analysis` scenario screenshot, the start position after `c1c f6f`,
White to move, LearnedValue model 169, TT + ordering: depth 7 complete and depth 8
at 22/23 root moves after 4.7 s and 8.2M nodes. One run, not a benchmark.

### Web

Headless Chrome (SwiftShader, real time): `?mode=black&level=hard&hints=1`
showed Hard's first move `f1e` (depth 6.0, 75k nodes) and Black's analysis
arrows. `?mode=watch&hints=0` (virtual time) showed the opener move `h1g`. Sizes:
`index.wasm` 809 KB, `index.js` 178 KB, `index.data` 13 KB.

## Implementation notes and differences from the plan

- **Web model loading failed until `loadModel` stripped `\r`.** Model files are
  CRLF in this Windows checkout. MSVC text streams drop the `\r`, Emscripten's do
  not, so `type=linear\r` matched no model type and every learned preset failed
  to load. The id's hash check still passed (it hashes bytes), which is why the
  error surfaced only at model load. Fixed in `src/ml_model.cpp` (tests: 4973
  assertions in 222 test cases pass). The files are bundled byte-identical
  rather than converted, because the canonical id carries the file hash.
- **Web link needs `em++`.** `emcc` left the C++ runtime unresolved.
- **Headless Chrome's virtual time froze the analysis slice loop.** Under
  `--virtual-time-budget` the clock does not advance inside a task, so the
  `while (elapsed < 10 ms)` slice never ended. `engTick` now also caps a slice at
  64 steps. Real browsers were not affected, but the cap makes the loop
  terminate under any clock.
- **Model picker froze the frame rate.** Re-truncating ~1700 rows every frame
  with a linear `MeasureText` per character made a capture take minutes. Rows are
  now cached by filter and width, and `FitText` binary-searches.
- **Mixed light raygui widgets on the dark panel** made the move log and editor
  labels unreadable (light text on raygui's light list background).
  `ApplyDarkStyle` sets one palette for every control.
- Web starts with hints off, and reads `?mode=&level=&hints=` from the URL. Not
  in the plan, added so a link can open a matchup and for testing.
- The eval bar maps learned scores linearly over +/-900 (the model files'
  `out_scale`) and heuristic scores through tanh at 2.5 chips.
- Dropped from the plan: nothing. Deferred: an agent-vs-agent ladder runner.

## Gotchas for later sessions

- Only `gui_engine.cpp` may include `ml_eval.h` (raylib's `struct Model`).
- Use a fresh browser profile or a cache-busting query after each web rebuild:
  a reused headless profile served the previous `index.wasm`.
- `Start-Process -PassThru` returns an empty `ExitCode` unless `$p.Handle` is
  read before the process exits.
- A preset whose model file changes on disk no longer matches its id's hash and
  fails to load with the parser's error. Re-pin the preset id after a retrain
  of that slot.

## Commit

`Overhaul the GUI: agent library, analysis arrows, a non-blocking engine thread, and a web page`
(see `git log` for the full message).

## Future Work

- **Visible-window frame times.** The responsiveness table is from hidden
  capture. Measuring the same scenarios in a visible window would confirm the
  UI-thread number is what a user sees. Settles: whether display latency matches
  the 1 ms work time.
- **Web frame stalls.** A web move job runs inside one frame. The Hard agent's
  wasm move time was not measured, so the length of that stall is unknown.
  A `performance.now()` log around `runMoveJob` in the web build would settle it,
  and decide whether web moves need slicing too.
- **Analysis TT fidelity.** Analysis and the agents share one TT's capacity. The
  salt keeps their entries apart, but analysis stores can evict an agent's
  entries mid-game, which could change an agent's move versus `rank.exe`'s
  games. A test that plays one game with analysis on and one with it off and
  compares move lists would show whether this happens at the default TT size.
- **Preset strength for humans.** Easy / Medium / Hard were chosen from the
  pool's Elo, which measures agent vs agent. Whether Easy is beatable and Hard
  unbeatable for a person is untested.

## Ideas This Inspired

- A "coach" mode: after each human move, show the analysis's score for the move
  played next to the best move's score.
- Show the analysis's principal variation as a sequence of faded arrows.
- A GUI tab that runs a small `rank.exe gauntlet` for the agent on a side card and
  shows its Elo when done.
- Save and load positions (a board file) from the GUI, and a position editor for
  puzzles.
