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
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\datastore.cpp src\transposition.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe'
```
The `ml_features.cpp` / `ml_model.cpp` / `ml_eval.cpp` trio is required by the engine
(ai_eval.cpp's `LearnedValue` evaluator calls `mlValueScore`), so every build target
(`breakthrough.exe`, GUI, web, tests) links them. As of the search-budget work,
`ai_minimax.cpp`'s opt-in transposition-table path also pulls in `src\datastore.cpp`
(for `positionKey`) and `src\transposition.cpp`, so every target links those two as well
(they are inert unless an agent sets `useTT`).

### Trainer (`train.exe`)
The modular ML toolchain is a separate binary (does not touch `breakthrough.exe`):
```
.\tools\run_train.ps1 -Build selfplay-supervised --games 250 --epochs 6
.\tools\run_train.ps1 imitate --out models/lin_policy.txt --games 150
.\tools\run_train.ps1 tournament --games 10        # single-process, default depth ladder
.\tools\run_train.ps1 docs
```
**Note:** the linear value model overfits past ~6-8 epochs on outcome labels (loss climbs
back toward 0.69); keep `--epochs` small (~6). **Parallel depth-laddered tournament**
(process-sharded across all CPUs, then rated):
```
.\tools\run_tournament.ps1 -Workers 12 -Depths "2,4,6,8,10" -Games 10 -NodeBudget 200000
```
Under the hood that runs `train.exe tournament-play --shard i --of K ...` (each shard writes
`data/tourney.jsonl.<i>`) then `train.exe tournament-rate ...` (merges, fits Elo, prints the
`Elo | ms/move | max ms | games | agent` table, writes `agents/champion*.txt`). Threads are
not used because the engine's board/eval state is global; processes each get their own copy.
Add `-Only "name1,name2,..."` to restrict the roster to those agent names (include their
depths in `-Depths` so the names exist); a subset run leaves `agents/library.txt` +
`champion*.txt` untouched. Every run is archived under `runs/<id>/` (`config.json`,
`elo.tsv`, `notes.md`, `results.jsonl`), logged in `runs/index.jsonl`, and folded into the
agent registry (`agents/registry.{jsonl,md}`, a union with a `spec_hash` that flags retrains
/ changes); `-Note "..."` records a pre-run note and `train.exe run-note --run <id> --note
"..."` attaches one later. Raw build: `.\build_train.bat` (mirrors `build_tests.bat`). See
`ML.md` for the full system and the "how to add more" workflow.

### Ranker (`rank.exe`)
The persistent agent Elo-ranking system is a third binary, independent of both
`breakthrough.exe` and `train.exe` (it links the engine sources plus `src\ranking.cpp`,
NOT `src\ml_train.cpp` or `src\settings.cpp`):
```
.\tools\run_rank.ps1 -Build check           # validate ranking/roster.txt, print model hashes
.\tools\run_rank.ps1 run --games 8          # serial play (live per-game progress) then rate
.\tools\run_rank.ps1 -Workers 8 --games 8   # process-sharded play, merge, rate
.\rank.exe history --agent "ab(d4"          # per-opponent record for one agent
.\rank.exe gauntlet --id "<id>" --games 4   # rate one candidate vs the frozen pool, O(N) games
```
Agents are identified by a canonical ID string, e.g.
`ab(d6,tt,ord,nb200k)@1.classic(t2,c10,w3,l2)@1` (grammar in `src\ranking.h`), which is
the permanent key of the append-only match store `ranking/matches.jsonl` (committed,
never regenerated). Every module segment carries its code version as `@N`, a constant in
the codec tables in `src\ranking.cpp`: bump one constant when that module's code changes
behavior and only the agents using it get new identities (a stale `@N` in the roster
fails the canonical check and prints the fix). The roster `ranking/roster.txt` is
hand-edited (`anchor|on|off <id>` lines, exactly one anchor). The scheduler plays only
each active pair's missing games (color-balanced, per-game srand seeds derived from the
pair + game ordinal, so shard splits and re-runs reproduce identical games), which makes
adding one agent O(N) games. Each game records per-side wall ms, process-CPU ms
(GetProcessTimes deltas, honest under parallel contention), node totals, effective
depth, plies, and end piece counts. Ratings are an anchored Bradley-Terry MM refit
(anchor = Elo 0, 0.5 virtual games prior per played pair, Fisher standard errors);
`rate` writes `ranking/ratings.tsv` + `ranking/games.tsv` (per-game export) +
`ranking/report.md` (W-L split by color, avg plies, end-piece margin, cpu/move,
`eff` = Elo / log2(1 + cpu_us/move), and an Elo-vs-CPU pareto-frontier table). Learned
agents embed an 8-hex model-file content hash in the ID and roster load hard-errors on
a mismatch (a retrain is a new identity). Raw build: `.\build_rank.bat`.

### Tests

Preferred one-liner (handles VS environment automatically, mirrors `build_gui.bat`):
```powershell
.\tools\run_tests.ps1 -Build
```
This rebuilds `tests.exe` via `build_tests.bat` (using `vswhere` to locate VS), then
runs it and reports pass/fail. Omit `-Build` to re-run the last build without recompiling.

Raw build command (equivalent, for reference):
```
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl tests\test_main.cpp tests\test_move_validation.cpp tests\test_win_detection.cpp tests\test_eval.cpp tests\test_ai_integration.cpp tests\test_game_outcomes.cpp tests\test_ml.cpp tests\test_ranking.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\explorers.cpp src\choosers.cpp src\agents.cpp src\datastore.cpp src\transposition.cpp src\ml_train.cpp src\ranking.cpp /I src /I tests /EHsc /Fo"build\\" /Fe:tests.exe'
```

### Run
```
.\breakthrough.exe
```
When prompted for a board file, enter e.g. `boards\board1.txt`.

**Scripting the console for tests:** pipe answers through the **Bash tool**, not
PowerShell. PowerShell's pipe encoding/BOM corrupts the first `cin` read, which
sends the program into the `getBoard()` filename loop and an infinite "Invalid
file..." spew. The right way (answer `1` to use the default board):
```
printf '1\n2\n0\n4\n0\n3\n0\n0\n1\n0\n0\n0\n1\n0\n1\n1\n' | ./breakthrough.exe 2>&1 | grep -E "eval:|has won"
```
See the "Driving the console non-interactively" section of [TESTING.md](TESTING.md).

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
| `.gitignore` | Excludes `.exe`, `.obj`, `build/`, `third_party/`, and ML generated artifacts (`data/*.jsonl`, `models/*_ckpt*.txt`) |
| `INSTALL.md` | Setup notes: VS C++ workload, raylib download, emsdk for the web build |
| `build_gui.bat` | Native GUI build (MSVC + raylib, `/MD`) -> `breakthrough_gui.exe` |
| `build_web.bat` | Emscripten/WASM GUI build -> `docs/index.html` (release or `dev`) |
| `tools/smoke_test_gui.ps1` | Standard GUI smoke test: build/launch/screenshot/close, exits non-zero on crash (run from project root) |
| `tools/run_tests.ps1` | Build and run the Catch2 test suite in one step: `.\tools\run_tests.ps1 -Build`. Use the `/run-tests` skill to invoke this correctly from Claude sessions. |
| `build_tests.bat` | MSVC batch build for `tests.exe` (called by `run_tests.ps1`). Uses `vswhere` to locate VS automatically. |
| `build_train.bat` | MSVC batch build for the ML trainer `train.exe` (mirrors `build_tests.bat`). |
| `tools/run_train.ps1` | Build (`-Build`) and run `train.exe`, passing args through. |
| `tools/run_tournament.ps1` | Mint a UTC `RunId`, write `runs/<id>/` config via `run-config`, launch K `tournament-play` shards in parallel (one process each, own output file), then merge + `tournament-rate --run <id>`. Params include `-Only`, `-Note`, `-RunId`. |
| `build_rank.bat` | MSVC batch build for the persistent Elo ranker `rank.exe` (engine sources + `src\ranking.cpp`, excludes `ml_train.cpp`/`settings.cpp`). |
| `tools/run_rank.ps1` | Build (`-Build`) and run `rank.exe`, passing args through. `-Workers K` shards `play` across K processes (per-shard `<store>.<s>` files appended to the store only after every worker exits cleanly), then rates once. Uses `PositionalBinding=$false` so `--key value` passthrough args are not captured by named params. |
| `ranking/` | The persistent Elo-ranking state: `roster.txt` (hand-edited `anchor|on|off <id>` lines), `matches.jsonl` (append-only ID-keyed match history, committed, the never-recomputed asset), and generated `ratings.tsv` + `games.tsv` + `report.md`. Shard temps `matches.jsonl.*` and `gauntlet.jsonl` scratch are gitignored. |
| `ML.md` | Machine-learning system overview + "how to add more" workflow; the registry tables at the bottom are auto-generated by `train.exe docs`. |
| `requirements.txt` | Optional Python deps (duckdb/pandas; torch/wandb optional) for `analysis/` + `train_py/`. |
| `analysis/analyze.py`, `analysis/README.md` | DuckDB queries over `data/*.jsonl` (top agents, fairest positions, average eval per position). |
| `train_py/export_format.py` | Writes a model in the C++ `type=` format (Python -> engine weight export contract). |
| `data/`, `models/`, `agents/` | ML outputs: append-only JSONL datastore, model checkpoints + `manifest.{json,md}` + `registries.json`, the Elo-rated `agents/library.txt` (full-roster snapshot), and the agent registry `agents/registry.{jsonl,md}` (union of every agent ever rated, with a `spec_hash`). |
| `runs/` | Per-run archive (one timestamped dir per tournament): `config.json` (exact config + pre-run note), `elo.tsv` (that run's ranked table), `notes.md` (pre-run + `run-note`-appended notes), `results.jsonl` (gitignored copy). `runs/index.jsonl` is the master log, one summary line per run. |
| `TESTING.md` | Verification playbook: console/engine test steps, the GUI smoke-test workflow, visual-inspection lessons, matchup-gated UI capture, and MSVC/raygui gotchas |
| `tools/gui_capture.ps1` | Targeted screenshot helper: finds the `GLFW30` window by process id and crops its client area for inspecting individual widgets (complements `smoke_test_gui.ps1`) |
| `.claude/skills/run-tests.md` | Claude skill: instructs future sessions to use `run_tests.ps1 -Build` and never to call `cl` directly without loading `vcvars64.bat` first |

### `src/`
| File | Purpose |
|---|---|
| `globals.h` | Master header: macros (`EMPTY='.'`, `WHITE='W'`, `BLACK='B'`, `SIZE=8`), enums, `extern` globals, and all function prototypes. Included by every `.cpp`. |
| `globals.cpp` | Definitions of all global variables (`board`, `g_whiteCount`, `PRNT`, etc.). Separated from `main.cpp` so the test executable can link them without pulling in `main()`. |
| `main.cpp` | Top-level game loop: seeds RNG, loads board, calls `getSettings()`, dispatches turns, tracks per-player time, and accumulates scores over multi-game sets. Holds per-side `wEval`/`bEval` (evaluator index) and `wParams`/`bParams` (`int[MAX_EVAL_PARAMS]`). It also hosts parameter test-sweep mode (`testingParam` 1 = depth, 2..1+paramCount = the tested side's evaluator weights). When `SHOW_EVAL` is set, after each move it prints that side's eval via `printEvalLine()` (`now=` immediate static eval, plus `pred=` predicted best-line eval for MiniMax sides); `formatEval()` renders forced wins as `+WIN`/`-WIN`. |
| `board_io.cpp` / `board_io.h` | Board file I/O and display: `getBoard()`, `reloadBoard()`, `printBoard()`, `loadMinimaxParams()` (reads `<side>_eval`, `<side>_depth`, `<side>_opener`, and each evaluator param via `<side>_<key>`, falling back to legacy `<side>_<key>_weight` names) |
| `settings.cpp` / `settings.h` | Interactive CLI configuration. `getSettings()` prompts for player types and, for MiniMax, the evaluator and its weights (looped from the registry by name via `getEvaluatorSettings()`); also game count, verbosity, and `SHOW_EVAL` (the "Show board evaluations?" prompt). `printVictor()` displays winner and timing. |
| `moves.cpp` / `moves.h` | All move logic: dispatch (`moveWhite`/`moveBlack`), human input parsing, full validation (`tryMoveWhite/Black`), fast AI validation (`tryMoveQuickWhite/Black`), execution (`playMoveWhite/Black`), reversible simulation (`simulateMove`/`unsimulateMove`). `simulateMove`/`unsimulateMove` also maintain the incremental positional accumulator `g_evalPos` (via `evalPosLocal`) when `g_evalIncremental` is set. |
| `board_analysis.cpp` / `board_analysis.h` | Chip counting, row-level chip difference, one-step win detection: `findWinWhite/Black()`, `canWinWhite/Black()` |
| `ai_eval.cpp` / `ai_eval.h` | Pluggable board evaluators. A registry `g_evaluators[]` (with `g_evalCount`) lists each `EvalDef` (name, parameter list of `EvalParamDef{name,key,def,lo,hi}`, scoring `fn`). Two ship: `evalClassic` (original heuristic: near-end win detection at rows `SIZE-2`/`1`, turn bonus, wall/column structure bonuses, chip-diff base) and `evalExperimental` (Classic plus a "Forward" weight (rewards pieces further toward the goal), identical to Classic when that weight is 0). `evaluateBoard(turnColor, evaluator, params)` dispatches by index; a convenience overload `evaluateBoard(turnColor, turn, chip, wall, col)` calls Classic. `MAX_EVAL_PARAMS` is defined in `globals.h` so param-array callers need only that header. To add an evaluator: append one `EvalDef` and write its `fn` — both UIs pick it up automatically. End-row win detection is handled before calling this. **Incremental eval:** for the minimax hot path, the positional part (structure + forward) is cached in `g_evalPos` instead of rescanned per leaf. `evalBeginSearch`/`evalEndSearch` (RAII-guarded in minimax) seed/tear it down; `evalPosFull` does the full scan; `evalPosLocal` computes a move's local delta (an owner bounding-box for structure + the two changed squares' forward score) applied by `simulateMove`/`unsimulateMove`; `evalLeaf` combines `g_evalPos` with the already-incremental `g_chipDiff` and turn term. An `EvalDef.incremental` flag opts in (requires the standard `p[0]=turn,p[1]=chip,p[2]=wall,p[3]=column,p[4]=forward` layout); non-incremental evaluators fall back to a full `evaluateBoard` at the leaf. `nearWinCheck` is shared so the fast and full paths can't diverge. `immediateEvalForDisplay(isMiniMax, evaluator, params)` returns a white-centric static eval of the current board for the UIs (a MiniMax side uses its own evaluator/weights; others fall back to Classic registry defaults). A third evaluator `LearnedValue` (param `p[0]` = model slot) delegates to `mlValueScore` (see `ml_eval.cpp`); `nearWinCheck` is now declared in `ai_eval.h` so the learned evaluator and 1-ply explorers reuse it. |
| `ai_random.cpp` / `ai_random.h` | Three random AI strategies (`pureRandomMove`, `tieredRandomMove`, `smartRandomMove`) and opening-sequence logic (`playOpenerWhite/Black`) |
| `ai_minimax.cpp` / `ai_minimax.h` | Alpha-beta minimax: `miniMaxWhite/Black()` top-level search, `maxAlphaBeta`/`minAlphaBeta` recursive pruning, capture-first move ordering, win-decay for fastest wins. After the root move loop each side stores its best-line score (root `alpha`/`beta`) into `g_downEvalWhite`/`g_downEvalBlack` for the "predicted downstream" display (free: the value was already computed). **Budgets & telemetry:** with a per-move node budget (`g_nodeBudget`) or wall-clock budget (`g_timeBudgetMs`) set, the search runs iterative deepening (`searchRootWhite/Black` per depth) sharing one pool; `budgetTripped()` flags a cut leaf. Each search records `g_lastEffDepth` (fractional: completed depth + fraction of the cut iteration's root moves), `g_lastBudgetKind` (node/time/depth) and `g_lastNodes`/`g_lastLeafs`. A cut iteration is discarded unless `g_keepPartial`. **Opt-in, ablatable efficiency features** (default off, so console/GUI/tests are unchanged): `g_useAlphaBeta` (off = full minimax baseline), `g_aspirationWindow` (root window seeded from the prior iteration), and `g_useTT`/`g_useMoveOrder`, which route to `maxAlphaBetaOrdered`/`minAlphaBetaOrdered` (transposition probe/store keyed by `positionKey` + best-move/killer/history ordering). `test_ai_integration.cpp` asserts these features preserve the exact search value. |

#### Modular ML system (linked into every target; see `ML.md`)
The ML system is four pluggable axes plus an agent composition. An **Agent** is a **Move Chooser** that is either a **Search** (**Explorer** + **Evaluator**) or a **Policy** (a direct chooser, heuristic or a learned move-rater). Each axis is a registry of `*Def` structs (name + metadata + fn pointer), so adding one is a single table entry + a function body, and the console/GUI/tournaments/docs all pick it up.

| File | Purpose |
|---|---|
| `ml_features.cpp` / `ml_features.h` | Board reading for ML. `generateMoves(side, Move*)` (capture-first, shared by explorers/policies/trainer). Value features (`mlExtractValueFeatures`, `MLV_FEATURES=30`, white-centric, versioned) for value models. Move features (`mlExtractMoveFeatures`, `MLM_FEATURES=9`, side-relative) for policy models. Names tables drive the auto-doc. |
| `ml_model.cpp` / `ml_model.h` | `Model` base (`forward`, `outputScale`, `save`, `typeName`, `head`/`featureVersion`/`featureCount`, plus a `teacher` provenance string written to the file as `teacher=` and read back by `loadModel`) + `LinearModel` (value or policy head, `sgdLogisticStep`). `makeModel`/`loadModel` factory dispatch on a file's `type=` line. `g_modelTypes[]` registry (linear implemented; mlp/nnue/transformer stubbed). |
| `ml_eval.cpp` / `ml_eval.h` | Model **slots** `g_mlModels[ML_SLOTS=8]` (White/Black can use different models in one process). `mlValueScore(turnColor, slot)` (shared `nearWinCheck` shortcut, then `tanh*out_scale` clamped inside the `+/-WIN` sentinels) backs the `LearnedValue` evaluator. `mlRateMoves(side, slot, ...)` scores moves for the `LearnedPolicy` chooser. |
| `explorers.cpp` / `explorers.h` | Move-tree explorer registry `g_explorers[]`: `Greedy` (1-ply argmax over an evaluator) and `AlphaBeta` (wraps `miniMax*`). `ExplorerDef{name, desc, fn(side, evaluator, params, budget)}` returns the victor code. |
| `choosers.cpp` / `choosers.h` | Direct move-chooser registry `g_choosers[]`: wraps the random family + `LearnedPolicy` (argmax of `mlRateMoves`). `ChooserDef{name, desc, fn(side, modelSlot, param)}`. |
| `agents.cpp` / `agents.h` | `AgentSpec` (brain = Search or Policy; explorer/evaluator/chooser indices, modelSlot, evalParams, depth, dilution `randomMoveProb`/`depthCap`; per-agent `nodeBudget`/`timeBudgetMs` and feature toggles `useAlphaBeta`/`useTT`/`useMoveOrder`/`keepPartial`/`aspirationWindow`, defaulted by `seedAgentDefaults` to the historical search). `agentChooseMove(spec, side)` composes the axes, save/restores the budget+feature globals around the call (so one tournament can mix settings), and is reused by tournaments/data-gen. `agentDescribe` appends the budgets + enabled flags. `agentMakeSearch`/`agentMakePolicy`/`learnedValueIndex`. |
| `datastore.cpp` / `datastore.h` | Append-only JSONL (`dsAppendLine`) + canonical position key `positionKey(sideToMove, mirrorFold)` (packed encoding + 64-bit FNV-1a, optional left-right mirror fold) used as the join key across the `data/*.jsonl` streams. Its hash also keys the transposition table. |
| `transposition.cpp` / `transposition.h` | Opt-in transposition table (gated by `g_useTT`): fixed-size always-replace array keyed by `positionKey(...).hash`. `ttProbe` (cutoff on a deep-enough EXACT/LOWER/UPPER entry, plus the stored best move for ordering), `ttStore` (skips near-win sentinels and budget-cut scores), `ttNewSearch` (per-move generation bump), `ttBytes` (analytic per-feature memory). Linked into every target but inert unless `useTT`. |
| `ml_train.cpp` / `ml_train.h` | Trainer (used by `train.exe` and `test_ml.cpp`): self-play game runner with position capture, `trainSupervisedValue` (outcome-labeled linear value model) and `trainImitationPolicy` (behavioral cloning of the move-rater), both with a SELECTABLE teacher/generator (`--gen-eval`/`--teacher-eval` = Classic or Experimental, optional `--gen-params`/`--teacher-params` weights) whose spec is recorded as the model's provenance, `writeManifest` (models/manifest.{json,md}), `exportDocs` (regenerates the `<!-- AUTODOC -->` region of `ML.md` + `models/registries.json`), and the `g_regimes[]` registry. Emits positions/labels/evaluations to `data/*.jsonl`. **Tournament:** `buildTournamentRoster(depths,hasValue,hasPolicy)` (deterministic: heuristics + SmartRandom-N variants + LearnedPolicy, plus Greedy and AlphaBeta over a table of evaluator presets x depths); `tournamentPlay(...,shard,ofK,nodeBudget,out,only,timeBudgetMs,budgets,ablate)` plays its `gameIndex%ofK==shard` slice with per-move `std::chrono` timing, appending `{a,b,sa}` + `{timing,...}` rows (the timing rows now also carry streaming search-telemetry accumulators: count/sum/sumsq/min/max of eff-depth, nodes, branching, plus node/time/depth budget-kind counts) and a process-level `{resource,peak_ws_mb,cpu_s}` row (`emitResourceLine`, psapi); `tournamentRate(depths,in,only,runId,note,budgets,ablate)` fits Elo (`fitElo`), prints `Elo|ms/move|max ms|games|agent`, then a search-telemetry table (eff-depth mean+-std [min..max], nodes/move, branching, budget-kind %) and a resource summary, and (full run only, `only` empty) writes `agents/champion.txt` + `agents/champion_params.txt` (a `minimax_params.txt` block). Opt-in `--budgets "a,b,c"` adds an unbounded-depth budget ladder (Classic `bal` + Experimental `ebal` per budget); `--ablate` adds a feature-toggle family for both evaluators (`AblC-*`/`AblE-*`); `--forward-study` adds Experimental agents differing only in the forward weight (`Fwd0/1/2/4/8`, `Fwd0` = Classic-equivalent control) to rank forward by Elo. The default AB ladder crosses every preset (Classic chip/wall/col/bal/chip2/chip5 + Experimental fwd/bal/push) x depths. All three opt-in flags must be passed to BOTH play and rate so rosters match. `turnSwing(board,games,depth,seed,chipW,wallW,colW,advW)` calibrates the turn-advantage weight (1-ply white-centric eval swing between sides, turn term zeroed). Uses the Experimental evaluator (== Classic when forward is 0) so `--chip/--wall/--col/--forward` all vary; the tempo value scales with the structure and (most cleanly) the forward weight, with a floor of `2*fwdW`. `runTournament` is the single-process convenience wrapper. **Allowlist:** `filterRosterByName(roster, only)` reduces the roster to named agents (empty = full; warns on unresolved names), applied identically in play and rate. **Run archive:** `makeRunId` (UTC stamp), `writeRunConfig` (`runs/<id>/config.json` + `notes.md` header), `runNote` (append a later note), `agentSpecHash` (structural fields + learned model content), plus internal `writeRunElo`/`appendRegistry`/`writeRegistryRollup`/`appendRunIndex` that build `runs/<id>/{elo.tsv,results.jsonl}`, `runs/index.jsonl`, and `agents/registry.{jsonl,md}`. |
| `tools/train_main.cpp` | `train.exe` CLI: subcommands `selfplay-supervised`, `imitate`, `tournament`, `tournament-play`, `tournament-rate`, `turn-swing`, `speed`, `run-config`, `run-note`, `docs`, all `--key value` (incl. `--only`, `--run`, `--note`, `--node-budget`, `--time-budget-ms`, `--budgets`, `--ablate`, `--forward-study`, `--gen-eval`/`--gen-params`, `--teacher-eval`/`--teacher-params`, and `turn-swing`'s `--chip/--wall/--col/--forward`). |
| `ranking.cpp` / `ranking.h` | Persistent agent Elo ranking (`rank.exe`), independent of `ml_train.cpp`. **ID codec:** `rankAgentId`/`rankAgentFromId` round-trip an `AgentSpec` to/from a canonical ID string (grammar documented in `ranking.h`; head = `rand`/`tiered`/`smart(N)`/`policy`/`greedy`/`ab(dK,flags)`, then `classic`/`exp` with letter-keyed weights or `learned`/`linpol` with a model-content hash, optional `dil(rP)`). Each module segment carries an `@N` code version sourced from the codec tables (`g_rkChoosers`/`g_rkExplorers`/`g_rkEvals`/`RK_DIL_VERSION`); bumping a constant re-identifies only that module's agents, and a stale `@N` fails the canonical check with the fix printed. `linpol` alone carries no `@N` (its hash is its identity). The eval-letter table `g_rkEvals` must gain a row when an evaluator is added (a test enforces this). **Roster:** `rankLoadRoster` parses `anchor|on|off <id>` lines (exactly one anchor, no dupes, canonical IDs only). **Store:** `rankFormatMatchRow`/`rankParseMatchRow` for `ranking/matches.jsonl` rows (`{t,w,b,r,plies,wms,bms,wcpu,bcpu,wmv,bmv,wnod,bnod,wpc,bpc,wed,bed,wsn,bsn,seed,board,par,ts,run}`; cpu = per-side GetProcessTimes deltas, pc = end piece counts, ed/sn = effective-depth sums/counts; fields absent in old rows parse as -1 sentinels). **Scheduler:** `rankSchedule` computes color-balanced pending games per pair minus stored games, per-game seeds from FNV over `whiteId|blackId|pairOrdinal|runSeed`. **Fit:** `rankFitBT` (Bradley-Terry MM, anchor pinned at Elo 0, 0.5 virtual-game prior per played pair, union-find marks components disconnected from the anchor provisional, Fisher SE) and `rankFitSingle` (gauntlet 1-D MLE by bisection). **Subcommands:** `rankCheck`, `rankPlay` (live per-game progress lines), `rankRate` (writes `ranking/ratings.tsv`, `ranking/games.tsv`, and `report.md` with color-split W-L, avg plies, end-piece margin, cpu/move, `eff` = Elo/log2(1+cpu_us/move), an Elo-vs-CPU pareto table, head-to-head matrix, and per-agent history), `rankHistory`, `rankGauntlet` (candidate vs frozen pool, scratch `ranking/gauntlet.jsonl` unless `--keep`). |
| `tools/rank_main.cpp` | `rank.exe` CLI: subcommands `check`, `play`, `rate`, `run`, `history`, `gauntlet`, all `--key value` (`--roster`, `--in`, `--out`, `--board`, `--games`, `--seed`, `--shard`/`--of`, `--agent`, `--last`, `--id`, `--keep`). |

### `gui/`
| File | Purpose |
|---|---|
| `main_gui.cpp` | raylib + raygui front end. Resizable window; `ComputeLayout()` recomputes board geometry (`g_cell`/`g_boardX`/`g_boardY`/`g_boardPx`) each frame, reserving the panel width (`PANEL_W`, narrow) on the left while shown so the board sits **beside** it (not under it) and a `BADGE_STRIP` on the right for the piece-count badges; hiding the panel (`g_showPanel`) lets the board grow. Per-frame state machine (`Settings`/`WaitingForHuman`/`WaitingBeforeAI`/`ComputingAI`/`GameOver`), board rendering from the `board` global, mouse->grid click-to-move (via `tryMove*`/`playMove*`, ignored over the panel), AI turns via `moveWhite`/`moveBlack`, robust win detection by scanning goal rows + piece counts. **Auto-start:** `main()` calls `StartGame()` before entering the main loop, so the game is immediately live on open (no "Start Game" click needed). **Settings-changed notice:** `TakeSnapshot()` captures all gameplay settings into `g_snap` (`SettingsSnapshot` struct) at each `StartGame()` call; `SnapMatches()` compares `g_snap` to the current `g_white`/`g_black`/`g_boardFile` every draw call, and `DrawPanel()` shows a "Settings changed." label above the "New Game" button whenever they diverge during a live game. `DrawPieceCounts`/`DrawCountBadge` draw emblematic count badges in the right strip (Black top, White bottom) so they stay visible with the panel hidden. Under each badge, `DrawEvalReadout` (gated by `g_showEval`) shows that side's board evaluation: `now` (immediate static eval, captured before each move in `ApplyAIMove`/`HandleHumanClick`) and, for MiniMax sides, `pred` (the `g_downEval*` best-line value); `FormatEval` renders forced wins as `+WIN`/`-WIN`. Toggle the readouts with the panel "Show evaluations" checkbox or the **E** key. Player type, opener, and (for MiniMax) the evaluator are deferred `GuiDropdownBox`es (drawn last, open one on top, single-open; up to 6 specs, the two eval dropdowns added only for MiniMax sides). `PlayerConfig` carries `evaluator` + `evalParams[MAX_EVAL_PARAMS]`; `SeedEvalParams()` loads the registry defaults whenever the selected evaluator changes, and `DrawPlayerConfig` renders one `StepperRow` per parameter of the chosen evaluator (names/ranges from `g_evaluators`). The engine's `w1` arg (depth/furthest) is built by `SearchArg()`. Numeric params use a modular `StepperRow()` with a `StepStyle` enum of distinct bar+number designs (`STEP_BAR_NUM`, `STEP_SEGMENTS`, `STEP_NUMBAR`, `STEP_HANDLE`, `STEP_RULER`), all stepping with a stacked "+" (up) above "-" (down) via `DrawStackedPM` and click/drag-to-set via `ScrubBar` (`DrawFillBar` draws track+fill). Each row uses a distinct design, and the `g_stepStyle` "Sliders" `GuiComboBox` switcher forces one design on all rows. Depth uses the typeable `STEP_NUMBAR` so it can exceed the bar's 25 cap. Pacing controls are matchup-driven (`ClassifyMatchup`): AI vs AI gets slow-motion `|>` / fast-forward `>>` speed buttons (custom `DrawSpeedGlyph`, stepping `g_speedIndex` through `SPEED_NAME`/`SPEED_DELAY`) plus play/pause (`#131#`/`#132#`), step (`#134#`), and restart (`#211#`) raygui icon buttons; human vs a fast AI gets a `g_delay2s` "Min 2s per AI move" checkbox; human vs a slow (depth>5) AI or human vs human shows none. Toggle the panel with the Options/Hide button or Tab. Native/web main-loop shim at the bottom. |
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
| `test_ml.cpp` | ML unit tests: feature determinism, `LinearModel` forward/save/load + SGD loss drop, `mlValueScore` bounds, Greedy winning move, LearnedPolicy legal move, Elo monotonicity |
| `test_ranking.cpp` | Ranking unit tests: ID canonical round trips + rejection battery (incl. stale/missing `@N` module versions), learned-model hash checks, codec completeness vs `g_evaluators`, roster parsing, match-row round trip (new cpu/pc/ed fields + old-row sentinels) + board filter, scheduler color balance / incremental top-up / determinism, BT fit accuracy + anchoring + disconnected components, gauntlet 1-D fit |

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
| `g_evalPos` | `int` | Running positional eval (structure + forward) of the board, maintained incrementally by `simulateMove`/`unsimulateMove` during a search |
| `g_evalIncremental` | `bool` | True while an incremental minimax search is active (gates the `g_evalPos` updates in make/unmake) |
| `g_activeParams` / `g_activeParamCount` | `const int*` / `int` | The active evaluator's weight array and its length, used by `evalPosLocal` |
| `g_downEvalWhite` / `g_downEvalBlack` | `int` | Last MiniMax best-line ("predicted downstream") eval per side, white-centric. Set from the root `alpha`/`beta`; read by the UIs for the `pred` readout. |
| `SHOW_EVAL` | `int` | Console toggle for the per-move eval print: `1`=show, `0`=hide (set by the "Show board evaluations?" prompt). |
| `g_nodeBudget` / `g_timeBudgetMs` | `unsigned long long` / `double` | Per-move search budgets (0 = off). Node and wall-clock caps; whichever trips first ends the search via iterative deepening. |
| `g_useAlphaBeta` / `g_useTT` / `g_useMoveOrder` / `g_keepPartial` / `g_aspirationWindow` | `bool` / `int` | Per-search feature toggles, set (saved/restored) by `agentChooseMove` from the `AgentSpec` fields. Defaults reproduce the historical search. |
| `g_lastEffDepth` / `g_lastBudgetKind` / `g_lastNodes` / `g_lastLeafs` | `double` / `int` / `ull` | Per-move search telemetry. `g_lastEffDepth` is fractional (completed depth + cut-iteration fraction); `g_lastBudgetKind` is `BUDGET_NONE/DEPTH/NODE/TIME`. Read by the tournament for its distribution stats. |
| `g_nodeBudget` | `unsigned long long` | Per-move search node cap (0 = unlimited, the default; console/GUI/tests unchanged). Set by the tournament so depths up to 10 stay bounded. |
| `g_nodeDeadline` | `unsigned long long` | Per-search cutoff = `nodes + g_nodeBudget`, seeded in `miniMax*`; `max/minAlphaBeta` treat a node as a leaf once `nodes >= g_nodeDeadline`. |
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
- **Incremental evaluation:** a move only changes 2 squares, so instead of rescanning all 64 at each leaf, the positional score (structure + forward) is kept in `g_evalPos` and updated by each make/unmake via a small local delta (`evalPosLocal`). The leaf (`evalLeaf`) then just adds the already-incremental `g_chipDiff` and the turn term. Seeded/torn down per search by `evalBeginSearch`/`evalEndSearch`. Guarded by an equivalence test (`test_eval.cpp`) that walks the move tree and asserts `g_evalPos` always equals a full `evalPosFull` recompute.
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
2. `.\tools\run_tests.ps1 -Build` passes all 390 assertions (use the `/run-tests` skill)
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
