# Web engine workers and gap-colored arrows - results 1 (quiet-lantern)

Date: 2026-10-06. No plan document: the work came from two developer requests
in one session. (1) "Could we make the web continue looking deeper?", with the
developer choosing "measure first, then a worker for moves too". (2) Color the
recommended-move arrows by how far each falls short of the best move, not by
rank.

## Summary of changes

- **Web engine workers.** The web page no longer runs engine work on its main
  thread. `gui/gui_engine.cpp` compiles a second time, with
  `-DGUI_ENGINE_WORKER`, into `build/web/engine_worker.js` (+ `.wasm`, `.data`).
  The page starts two copies: a move worker for agent moves and an analysis
  worker for the arrows and the eval bar. The public `eng*` API is unchanged, and
  `main_gui.cpp` does not know which path is running. Design detail is in
  `gui/CLAUDE.md`, "Web engine workers".
- **Inline fallback.** If a worker cannot start, reports an error, or has not
  reported ready 4 s after it was started, the page runs the old inline engine
  for the rest of the session. The move and analysis in flight are re-queued.
  `?engine=inline` forces the inline engine, so one build can compare the two.
- **Analysis restarts.** A worker reads commands only between root-move
  searches. If the analysis worker has not acknowledged a new analysis command
  within 150 ms, the page terminates it and starts a fresh one.
- **Gap-colored arrows** (native and web). `ArrowColor` in `main_gui.cpp`
  colors each arrow, score pill, and Analysis-tab line from the gap to the best
  line, measured as a share of the eval bar for the side to move. It runs green
  at no gap, through yellow, to red at `ARROW_RED_GAP` = 0.25 of the bar (450
  points on the learned evaluators' +/-900 scale). Rank still shows in arrow
  width.
- **Measurement hooks** (web build). `window.__anaLog` gets one record per
  completed analysis depth: wall and compute ms, nodes, top-3 scores, and why
  the run stopped. `window.__anaStarts` gets the moves behind each analysed
  position, and `window.__moveLog` one record per agent move job. URL options:
  `&moves=` sets up a position, `&anacap=<ms>` sets the inline cap, and
  `&engine=inline` skips the workers.
- **`tools/web_ana_bench.ps1`.** This script opens pages one at a time in hidden
  headless Chrome and saves those logs, the frame count, the longest frame gap,
  frames over 50 ms, and the number of worker starts to
  `build\web_bench\<name>.json`. `-Gpu` drops SwiftShader. `-Fps` caps
  `requestAnimationFrame`, default 60.
- **Build scripts.** `build_web.bat` and `build_web.sh` build the worker after
  the page.

## How to test

```powershell
.\build_web.bat
.\tools\web_ana_bench.ps1 -Gpu -Seconds 70 -Pages "wk=mode=white&hints=1","inl=mode=white&hints=1&engine=inline"
python -m http.server -d build\web    # then http://127.0.0.1:8000/?mode=white&hints=1
```

Expected behavior:
- With hints on and the human to move, the Lines readout keeps deepening past
  depth 8. It stops at the Analysis time limit (60 s), depth 30, or a proven
  result.
- Agent moves never freeze the page.
- `inl.json` stops at depth 7-8 with `stop: 3` (the 120 ms cap).
- Arrows: when the top moves score close together, all of them are green. When
  one move stands out, the others shade toward orange or red.
- Native: `.\tools\gui_shot.ps1 -Scenario analysis -Moves "<moves>"` shows the
  same colors.

Commit message: see the commit that adds this file.

## Measurements

### Setup and instrument checks

- Machine: the 12-logical-core Windows dev machine. Browser: headless Chrome.
  The web build is `-O3 -flto`.
- Five fixed positions, each with White to move and the human playing White.
  p0 is the start position. p10, p20, p30, and p40 are positions 10, 20, 30, and
  40 plies into one recorded Watch game:
  - p10 = `d2e,d7c,c1d,c7d,d2c,d6d,e2d,g7f,e3f,d5e`
  - p20 = p10 + `b2a,b8c,g2f,g8g,f2g,h7g,f3e,e7d,g3g,c7b`
  - p30 = p20 + `f1g,c6b,g4g,f6g,f4g,f7e,a3b,b5a,b4b,d8c`
  - p40 = p30 + `a1b,c7c,b5c,b7c,c2b,a4b,a2b,c6b,d3c,b5c`
- Analysis evaluator: the default, the Hard preset's model slot 169 (LearnedValue).
  TT and move ordering on, no quiescence. Limits: depth 30, 60 s.
- One 70 s page load per position and engine. Each load is a fresh browser
  profile, so the transposition table starts empty. These are single runs with
  no replicates. Timing noise between runs of one condition was not measured.
- **Search parity (instrument check).** For every position, the worker and the
  inline engine produced identical node counts and identical top-3 scores at
  every depth both completed (depths 1-7 or 1-8). Hard's first move from the
  start position was `f2e` at 75236 nodes, depth 6, on both paths. The worker
  therefore runs the same search. Only timing differs.
- **Knob check.** `-Fps 60` measured 59.99 frames/s. Without it the same page
  rendered 298 frames/s. `anacap=0` removed the cap: the page went to depth 9-11
  instead of stopping at 7.
- **Restart path.** Pressing A to turn analysis off during a depth-11 search
  restarted the analysis worker (worker starts went from 2 to 3). There were no
  frame gaps over 50 ms. Turning it back on ran depths 1-9 on the fresh worker
  in 10 s.
- **Fallback path.** I renamed `engine_worker.js` away. The Watch page then
  played 51 moves with analysis on the inline engine, and no record carried a
  worker tag.

### Deepest completed depth, GPU rendering, 60 fps (the realistic setting)

| position | inline (before) | workers (after) |
|---|---|---|
| p0 | d8, stopped by the 120 ms cap at 2.0 s | d11 at 56.7 s, then the 60 s limit |
| p10 | d8, cap, 2.0 s | d10 at 23.3 s |
| p20 | d7, cap, 1.4 s | d9 at 22.5 s |
| p30 | d8, cap, 4.2 s | d9 at 19.1 s |
| p40 | d8, cap, 1.7 s | d10 at 21.3 s |

### Wall seconds to complete each depth, same runs

| depth | p0 inline | p0 workers | p10 inline | p10 workers | p20 inline | p20 workers | p30 inline | p30 workers | p40 inline | p40 workers |
|---|---|---|---|---|---|---|---|---|---|---|
| 6 | 0.23 | 0.10 | 0.19 | 0.12 | 0.30 | 0.21 | 0.31 | 0.23 | 0.19 | 0.12 |
| 7 | 0.61 | 0.32 | 0.66 | 0.43 | 1.36 | 0.93 | 1.15 | 0.97 | 0.61 | 0.49 |
| 8 | 2.01 | 1.29 | 1.98 | 1.50 | - | 4.22 | 4.24 | 3.73 | 1.74 | 1.42 |
| 9 | - | 5.19 | - | 6.06 | - | 22.50 | - | 19.14 | - | 6.43 |
| 10 | - | 15.95 | - | 23.34 | - | - | - | - | - | 21.32 |
| 11 | - | 56.73 | - | - | - | - | - | - | - | - |

Nodes per depth (identical on both paths): p0 d8 = 5,605,942, p10 d8 =
9,128,858, p20 d7 = 4,128,840, p30 d8 = 15,134,861, p40 d8 = 6,510,860.

### Search speed, nodes per compute ms, same runs

| position | inline d6 / d7 / d8 | workers d6 / d7 / d8 |
|---|---|---|
| p0 | 2078 / 2808 / 3603 | 3047 / 3545 / 4391 |
| p10 | 4869 / 4069 / 6245 | 4913 / 4066 / 6135 |
| p20 | 5219 / 4380 / - | 4806 / 4488 / 5112 |
| p30 | 4115 / 4120 / 4170 | 3568 / 3434 / 4075 |
| p40 | 3873 / 3557 / 5309 | 4153 / 2883 / 4630 |

### Watch mode, GPU rendering, 60 fps, 180 s page load (one game each)

Watch is `watch_white` vs `watch_black` from `gui/presets.txt`:
`ab(deep=6,tt,ord,nodes=200k)@3.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`
against
`ab(deep=6,tt,ord,nodes=200k)@3.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1`.
Each position lasts about 280 ms before the next move. The two runs are two
different games, because the openings are random.

| | inline | workers |
|---|---|---|
| positions analysed | 66 | 75 |
| deepest depth per position (count) | d1:3 d2:1 d4:1 d5:13 d6:47 d7:1 | d1:3 d2:1 d4:1 d5:1 d6:51 d7:16 d8:2 |
| agent move job ms, median / p90 / max | 15 / 44 / 52 | 15 / 42 / 112 |
| longest frame gap | 67 ms | 25 ms |
| frames over 50 ms | 10 | 0 |

The d1-d4 rows are positions near the end of each game, where a win is
proven early.

### Earlier cuts of the same comparison, and why the 60 fps one is the headline

| setting | inline deepest (p0..p40) | workers deepest (p0..p40) | worker nodes/ms vs inline |
|---|---|---|---|
| SwiftShader, uncapped frame rate | 7, 7, 7, 7, 7 | 10, 9, 8, 9, 9 | about 2.5x lower |
| GPU, uncapped (about 300 fps) | 8, 8, 8, 8, 8 | 10, 10, 9, 9, 10 | 10-25% lower |
| GPU, 60 fps | 8, 8, 7, 8, 8 | 11, 10, 9, 9, 10 | about the same |

Headless Chrome has no display to sync to, so it renders as fast as it can.
With SwiftShader the rendering itself runs on the CPU. In the worker build the
main thread is free to render continuously, and that rendering competed with
the analysis worker for cores. In the inline build the analysis blocked
rendering instead. Neither condition matches a real browser, which renders at
the display rate on the GPU. The uncapped Watch runs showed the same artifact:
moves were slower in the worker build, and depths ran shallower than inline.

The inline engine's cap depth depends on the frame rate. Its 120 ms rule
measures wall time across frames, so slower rendering stops it sooner:
depth 7 under SwiftShader, 8 on the GPU.

## Implementation notes and gotchas

- **No SharedArrayBuffer.** Real threads (pthreads) need COOP/COEP headers,
  which GitHub Pages cannot send. Workers that only exchange messages need no
  headers. The cost is that a running search cannot be aborted from the page,
  hence the 150 ms restart rule for analysis. Move jobs are short (median
  15 ms, max 112 ms measured), so the move worker is never restarted, and a
  cancelled move's result is dropped by request id.
- **Two workers, not one.** Each worker has its own transposition table, so
  analysis never evicts an agent's entries. A new agent move never waits
  behind an analysis step.
- **Wire format.** Messages are raw bytes of trivially copyable structs
  (`GuiPos`, `AgentSpec`, `EngAnalysisConfig`, `EngLine`). Both ends are
  `gui_engine.cpp` built by the same compiler.
- **Worker startup can be starved.** The default `web_shot.ps1` run loads three
  pages at once in software-rendered Chrome. There, workers took 13 s to over 2
  minutes to report ready. Before the 4 s startup deadline existed, the Watch
  and Easy pages sat on "thinking" for the whole run, because their first move
  was queued on a worker that had not loaded. With the deadline, all three pages
  switch to the inline engine and play. A single page's workers are ready in
  100-400 ms.
- **Bash heredocs with backslashes.** Patching `build_web.bat` through a Bash
  heredoc turned `src\agents.cpp`, `src\board_io.cpp`, and
  `src\transposition.cpp` into control characters (`\a`, `\b`, `\t`). The file
  was restored from git and re-patched from a script file. A Git Bash `sed -i`
  edit also converted `main_gui.cpp` to LF. Both were caught by
  `git ls-files --eol` and a control-character scan before the build.

## Future Work

- **Real browsers and phones.** Every number here comes from headless Chrome on
  one 12-thread desktop. The claim "the workers reach depth 9-11 in a minute,
  where the inline engine stopped at 7-8" holds there only. Run
  `web_ana_bench.ps1`'s positions in a headed desktop Chrome, in Firefox, and on
  a phone (`todo.md` already has a phone item). A phone's slower cores may also
  approach the 4 s startup deadline, so record worker ready time there.
- **High-refresh displays.** The headline numbers assume a 60 Hz display. The
  developer's own monitors run at 144 Hz (4K) and 300 Hz (1080p), so 60 Hz is
  not the case that matters most for this project. The closest measured
  condition is "GPU, uncapped (about 300 fps)" above, where the workers ran
  10-25% fewer nodes per ms than inline and still reached depth 9-10. That is a
  headless proxy: it renders at headless Chrome's default window size, not at
  4K, and it is not a real windowed browser. Run `web_ana_bench.ps1 -Gpu
  -Fps 144` and `-Fps 300` for a headless check, then load the page in headed
  Chrome on each monitor and compare the worker's nodes per ms and deepest
  depth against the 60 fps rows.
- **Run-to-run noise.** Each position-engine cell is one 70 s run. The
  nodes-per-ms table varies by up to 25% between depths within one run, which
  bounds how small a speed difference these runs can show. The depth gains
  (2-3 plies) are far outside that. A claim of "about the same speed" would need
  3-5 replicate loads per cell.
- **The 150 ms restart threshold is untuned.** It never fired in the 60 fps
  Watch runs. It fired once, on purpose, in the A-key test. In human play,
  every human move during a deep search triggers a restart. Measure the restart
  latency, from spawn to the first new snapshot, in a real browser to decide
  whether a warm spare analysis worker is worth its memory.
- **`ARROW_RED_GAP` = 0.25 is a judgment call.** Over 119 Watch positions at
  depth 5-7, the gap from best to second had a median of 50 points and a p90 of
  368. The gap from best to third had a median of 123 and a p90 of 617. So at
  the median the top three run green to yellow-green, and about one position in
  ten shows red. Whether that matches what a player finds useful is the
  developer's call after playing with it.

## Ideas This Inspired

- A pool of analysis workers, each scoring the next unscored root move, the web
  form of `todo.md`'s "split the root moves across worker processes" item. It
  needs a measurement of what losing the shared TT costs.
- Agent "pondering" on the web: with the move worker idle during the human's
  turn, it could search the expected reply in advance.
- The dashed reply arrow is red, and red now also means "much worse than the
  best move". A neutral color (blue or violet) for the reply would remove the
  overlap.
- `web_shot.ps1` could take `-Fps` and `-Gpu` like the bench, so its default
  run also exercises the workers.
