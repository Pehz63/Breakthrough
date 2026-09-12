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
.\tools\web_shot.ps1                                      # hidden, real time, build\web_shots\*.png
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

Headless Chrome (SwiftShader), screenshots taken through the DevTools protocol
after 20 s of wall-clock time (`tools/web_shot.ps1`), one browser per page:

| Page | What the screenshot shows |
|---|---|
| `?mode=black&level=hard&hints=1` | Hard's first move `f1e` (depth 6.0, 75k nodes), then Black's three arrows (+382 / +400 / +413) and the reply arrow |
| `?mode=black&level=easy&hints=1` | Easy's first move `c1d` (depth 6.0, 65k nodes) and Black's arrows |
| `?mode=watch&hints=0` | 18 moves each and the game still going. Watch Black (model 76) plays past its 8 opener moves |
| `?mode=watch&hints=1` | A finished game ("Black wins" in one run, "White wins" in another) with the eval bar |

Sizes: `index.wasm` 809 KB, `index.js` 178 KB, `index.data` 13 KB.

Check that the instrument measures the page: the screenshot's file time is the
capture time. An earlier `--timeout=30000 --screenshot` run saved all its shots
within about one second of launch, and a `--virtual-time-budget` run of Watch
sat on move 1, so neither shows what a user sees after waiting. Both are
recorded as gotchas in `TESTING.md`.

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
- **Agent-vs-agent pacing on the web ran many times too slow.** The delay
  before an agent move summed `GetFrameTime()`, which on the web covers only the
  frame's own work, not the browser's wait between frames. Under headless
  Chrome's virtual time the sum never reached the delay, so Watch stopped after
  White's first move (the first frame's time includes page startup). The timer
  is now a `GetTime()` difference. Native pacing is unchanged: the `aivai`
  capture played 72 moves in 900 frames, longest frame 1.0 ms, against 67 moves
  before the change (move counts vary with the random openers).

## Web hosting: GitHub Pages

The developer chose GitHub Pages through GitHub Actions (asked at the end of the
session, alongside keeping the difficulty, analysis, and style defaults as
built).

| Area | Change |
|---|---|
| `.github/workflows/web.yml` (new) | On pushes to `main` touching `gui/`, `src/`, `boards/`, or the build files, and on demand: install emsdk 6.0.9 and raylib 5.5 on `ubuntu-latest`, run `build_web.sh`, upload `build/web`, deploy with `actions/deploy-pages`. |
| `build_web.sh` (new) | The `build_web.bat` build for Linux and macOS, same sources and flags, `OUTDIR` overridable. |
| `gui/web_models/` (new) | Byte-exact copies of the five bundled model files, `* -text` in its `.gitattributes`. |
| `tools/web_preloads.ps1` | Reads the copies, mounts them at the `rankSlotFile` paths, checks every copy's hash against its preset id, `-Sync` refreshes them from `models/`. |
| `tools/web_shot.ps1` (new) | Real-time web screenshots through the DevTools protocol. |
| `gui/main_gui.cpp` | The pacing fix above. |

Why a snapshot folder: two things stop a Linux runner from bundling `models/`.
Slots 10 and 76 (Easy, Medium, Watch) are not tracked in git. And a canonical
id's hash is over the file's working-tree bytes, which are CRLF on this machine
(`core.autocrlf=true`) while git stores LF, so a Linux checkout hashes
differently. Measured with the engine's hash: `slot169.txt` is `4975683c` as
CRLF (the id's hash) and `0916f6d4` as LF, and the other two differ the same way.
`web_preloads.ps1` rejected an LF copy of slot 169 with exactly that `0916f6d4`,
and passed the CRLF copies. `git ls-files --eol` shows the copies stored
`i/crlf` with `attr/-text`.

Checks run: `build_web.bat` and `build_web.sh` (under Git Bash with the emsdk on
PATH, `OUTDIR=build/web_sh`) both built, with identical `index.wasm` and
`index.data` sizes. The screenshots in the Web table above are from this build.
The workflow itself has not run on GitHub: that needs a push of `main` and the
Pages source set to "GitHub Actions" (`INSTALL.md` section 3d).

The push is blocked. A `git push origin main` at the end of the session failed,
and the cause is in the unpushed history, not the GUI commits:
`ranking/matches.jsonl` is 39.1 MB on `origin/main` and 581.9 MB at HEAD, 9 of
the 51 unpushed commits change it, and 8 of those versions are 240 to 582 MB.
GitHub rejects any file over 100 MB in any pushed commit, so `rank.exe seal` at
HEAD alone does not unblock it. The developer chose to wait and leave the fix to
the ranking side (`todo.md`).

Found along the way, left for the owners of the ranking code (both in
`todo.md`): roster-cited slots 10 and 76 are untracked despite `.gitignore`'s
rule that anything the roster cites lives in git, and the working-tree-bytes
hash makes every learned id platform-dependent.

## Phone layout

After the page went live, the developer's playtest found that on a phone half
the board was off screen unless the page was zoomed all the way out.

Cause: `main()` called `SetWindowMinSize(900, 640)` on every platform. raylib's
web resize callback sizes the canvas to `window.innerWidth x innerHeight` and
clamps it to that minimum, so on a 390 px wide phone the canvas stayed 900 px
wide. The layout also always put a 300 px panel and a 136 px badge strip beside
the board.

| Area | Change |
|---|---|
| `gui/main_gui.cpp` | No minimum window size on the web. `ComputeLayout` sizes the board for the side layout and for a stacked layout (board across the width, a badge row above and below it, the panel underneath) and takes the stacked one in simple mode when its cell is more than 1.15x the side layout's. `DrawSimplePanelCompact`: mode, level (or Watch's speed, pause, step), and New / Undo / Hints / Flip as three rows of buttons, then the status, rules, level note, and move list where they fit. It is also used in the side layout when the panel is shorter than 430 px (a landscape phone). The game-over banner shrinks to fit a small board. The board hover highlight is off on screens without hover. Capture scenarios `watch` and `hints`. |
| `gui/shell.html` | `touch-action: none`, no long-press selection or tap flash on the canvas. |
| `tools/web_shot.ps1` | One DevTools session per page for the whole run, `-Device WxH` phone emulation with touch, `-Steps` touch taps and drags. |
| `tools/gui_shot.ps1` | `-All` adds simple mode at 390x760, 360x640, and 844x390. |

Which layout each size gets (simple mode, hints off):

| Viewport (CSS px) | Layout | Board cell |
|---|---|---|
| 390x760 (phone, upright) | stacked | 44 px |
| 360x640 (small phone, upright) | stacked | 40 px |
| 844x390 (phone, landscape) | side, compact panel | 35 px |
| 1264x765 (the headless desktop window's viewport) | side, full panel, unchanged | 82 px |

The cells are computed from `ComputeLayout`'s formulas. The 390x760 value is
also confirmed by the tap coordinates below landing on the intended squares.

Checks run, all in headless Chrome on the local build through `web_shot.ps1`
(`-Device`, device scale 3, touch emulation), plus hidden native captures at
the same sizes:
- 390x760, Play White vs Medium: the canvas came up 390x760
  (`innerWidth x innerHeight` and `canvas.width x height` read back through the
  protocol). Tapping `d1` then `d2` played `d1d`, and Medium replied. Dragging
  `c1` to `c2` played `c1c`, and Medium replied. Tapping "Hard" switched the
  level and started a new game. After the change to `g_noHover`, no tint stayed
  on the last square touched.
- 360x640, Watch with hints: the game played to "White wins" with the eval bar
  and both readouts in the badge rows.
- 360x640, Play Black vs Hard with hints: Hard's first move at depth 6.0 with
  75k nodes, Black's arrows on the flipped board.
- 844x390: the side layout with the compact panel and a move list.
- Desktop (no `-Device`, a 1264x765 viewport): the same page as before the
  change.

Not checked: a real phone. Headless touch emulation sends the same DOM touch
events raylib listens for, but iOS Safari's toolbars, rotation, and text
rendering were not seen.

## Gotchas for later sessions

- Only `gui_engine.cpp` may include `ml_eval.h` (raylib's `struct Model`).
- Use a fresh browser profile or a cache-busting query after each web rebuild:
  a reused headless profile served the previous `index.wasm`.
- `Start-Process -PassThru` returns an empty `ExitCode` unless `$p.Handle` is
  read before the process exits.
- A preset whose model file changes on disk no longer matches its id's hash and
  fails to load with the parser's error. Re-pin the preset id after a retrain
  of that slot, then run `tools\web_preloads.ps1 -Sync`.
- The engine's FNV-1a starts from `1469598103934665603`, not the standard
  offset basis `14695981039346656037` (one digit shorter). Any tool that
  recomputes an id hash must use the engine's value.
- PowerShell 5.1 `Start-Process -ArgumentList` joins arguments with spaces and
  does not quote them. The project path contains a space, so path arguments
  need explicit quotes.
- In PowerShell 5.1, a hex literal like `0xffffffff` is an `Int32` (-1). Use
  decimal `[long]` constants for 32-bit masks.
- raylib's `GetFrameTime()` on the web is the frame's work time, not the
  interval between frames. Time anything with `GetTime()` differences.
- A web `libraylib.a` needs `utils.c` (it defines `TraceLog`) and not `rglfw.c`
  (the desktop GLFW backend), the object list of raylib's own PLATFORM_WEB
  makefile. The first Pages run (run 34677223491, 2026-09-12) failed at link with
  `undefined symbol: TraceLog` because the workflow copied INSTALL.md's command,
  which listed `rglfw.c` instead. The local library had been built with the right
  list, so no local build showed it. Reproduced by building the library both ways
  and linking a clean clone of `main` against each: the old list gave the same 20
  `TraceLog` errors, the corrected list linked.
- A job log needs admin rights to download through the API. The workflow posts a
  failed build's last 60 lines as a public error annotation for that reason.
- raylib's `SetWindowMinSize` also clamps the web canvas, which is what pushed
  the board off a phone's screen.
- DevTools emulation overrides (`Emulation.setDeviceMetricsOverride`, touch)
  last only as long as the websocket session that set them. A tool that opens a
  new socket per request loses them, so `web_shot.ps1` keeps one session per
  page open for the whole run.
- In PowerShell 5.1, `Invoke-RestMethod ... | Where-Object` passes a JSON array
  through as one object. Assign the result to a variable first, then pipe it.

## Commit

- `Overhaul the GUI: agent library, analysis arrows, a non-blocking engine thread, and a web page`
- `Publish the web page with GitHub Pages, and fix agent pacing on the web`
- `Fit the web page to phones: a stacked layout, no minimum canvas size, touch checks`

See `git log` for the full messages.

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
- **The Pages workflow on GitHub.** It was checked piecewise here (the Linux
  script built under Git Bash, the snapshot hashes pass, the page works from
  the snapshot), never as a whole on an Ubuntu runner. The first run
  (2026-09-12) passed the emsdk install and the raylib build, then failed at
  link on the missing `utils.c` (see the gotchas). The run after the fix
  (34703014162, commit `85d8e23`) built and uploaded the page, then the deploy
  step failed with "Failed to create deployment (status: 404) ... Ensure GitHub
  Pages has been enabled", because the repository's Pages source was not yet set
  to GitHub Actions. After the developer set it, a rerun of that run (attempt 2)
  deployed. The live page at https://pehz63.github.io/Breakthrough/ serves all
  four files (`index.wasm` as `application/wasm`), and real-time headless
  screenshots showed Watch playing a game to the end and Hard opening `f1e` at
  depth 6.0 with 75k nodes, the same as the local build.
- **Web pacing in a real browser.** The pacing fix was checked in headless
  Chrome at real time. A look at Watch at each speed in a desktop browser would
  confirm the delays match the native app's.
- **The phone layout on a real phone.** The stacked layout and touch input were
  checked only under headless Chrome's device emulation. iOS Safari's changing
  `innerHeight` as its toolbars show and hide, rotation, and whether a quick tap
  ever lands inside one frame were not seen. Opening the live page on an iPhone
  and an Android phone, playing a few moves by tap and by drag, and rotating
  once would settle it.
- **Text sharpness on high-density screens.** The canvas renders at CSS-pixel
  resolution, so a phone with 3 device pixels per CSS pixel upscales it. It
  reads fine in the emulated screenshots. Rendering at device resolution would
  need a scale on every draw and the mouse, and raylib's default font is a
  small bitmap that stays blocky when drawn large, so a TTF font would have to
  come with it. Worth doing only if the page looks soft on a real phone.
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
