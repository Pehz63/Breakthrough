# CLAUDE.md - Breakthrough Project Reference

# Compact instructions

Claude Code reads this section on every compaction (auto and `/compact`). When
summarizing this conversation, preserve the following, since a generic summary
tends to drop them:

- Concrete measurement numbers produced this session: Elo, cpu/node before and
  after, percent speedup, win-rate deltas, and how each was measured.
- The exact agent IDs, model slot files, and roster lines under active work
  (for example `ab(d6,tt,ord,nb200k)@1.classic(...)@1`, `models/sweep/slotN.txt`).
- What was tried and its result, including dead ends, so they are not
  re-attempted.
- Any decision the developer made this session and its rationale.
- In-flight task state not yet written to `todo.md`.

**Self-healing rule.** If, after a compaction, you notice a dangling reference
(the summary cites a decision, file, number, or agent ID but the supporting
detail is gone, or you catch yourself about to re-derive something already
settled), the goal is to generalize, not just patch. Append to this section a
rule that preserves that whole category of thing on future compactions, so the
same class of loss cannot recur. Re-recording the single lost fact is secondary
and only when it is still recoverable (from `todo.md`, git, the ranking and
`runs/` artifacts, or memory). Honest limit: fully deleted content cannot always
be recovered, so the durable win is the category rule, and the trigger is the
dangling reference.

# How these instructions are organized

This root file is force-loaded into every session, so it holds only what every
session needs: rules, commands, and a map. Deep per-area reference lives in
per-directory CLAUDE.md files that Claude Code auto-loads only when working on
files in that area:

- `src/CLAUDE.md` - engine, evaluators, search, ML system, ranking internals,
  global-state table, architecture notes.
- `tools/CLAUDE.md` - trainer/ranker workflows, study scripts, CLI subcommands,
  artifact directories (`ranking/`, `runs/`, `data/`, `models/`, `agents/`).
- `gui/CLAUDE.md` - GUI internals, raygui/MSVC gotchas, smoke-test workflow.
- `tests/CLAUDE.md` - test file table.

Auto-loading triggers on reading or editing files in the directory, not on
executing them. Before running training or ranking studies that only invoke
scripts (`rank.exe` runs, sweeps, `train_vs_champion.ps1`, etc.), read
`tools/CLAUDE.md` explicitly first.

## Standing Instructions

- **Commit at natural checkpoints, without waiting to be asked.** After finishing a task or functional change (normally as the last step of the "After every functional change" workflow below, once the results doc and other updates are in place), create the commit yourself. Review `git status`/`git diff` first and never stage or commit anything that looks like a secret. **Never run `git push`** without explicit developer instruction given in that session; a prior commit approval does not carry forward to pushing.
- **Doc changes ride along with the next commit.** Include uncommitted edits to CLAUDE.md files, `todo.md`, `Docs/`, or `plans/` wholesale in whatever substantive commit happens next. Never cherry-pick hunks to exclude them, give them a dedicated commit, or describe them in the message (documents are self-evident). "Dedicated commit" requests mean the deliverable gets its own commit, not that entangled doc hunks get excluded.
- **Tests must pass before committing.** Run `.\tools\run_tests.ps1 -Build` (or confirm it already passed against the current code this session) before every commit. If any assertion fails, fix it or, if that isn't possible in scope, stop and tell the developer instead of committing. The one exception is a commit that touches no `src/`/`tests/`/build-affecting file (e.g. docs-only or `plans/`-only changes), where the test suite is a no-op anyway.
- **Give context first, then ask with the tool.** A real question must be posed through the AskUserQuestion tool, never as prose that ends the turn (that just stops and waits for a re-prompt instead of asking). When there is meaningful context to convey before the question, write it as a normal chat message and then invoke AskUserQuestion in the same turn, as the turn's final action. Do not stop after the context without asking, replace the widget with a prose question, or stuff the explanation into the widget's option descriptions.
- **A new model or agent is not done until its Elo is measured and it is documented.** (1) **Measure strength with a full-roster Elo run:** `rank.exe run` (or the sweep's rating path) against the whole active roster, an anchored Bradley-Terry refit at the standard heads (depth 4 and depth 6 at the nb200k budget). Train about 6 seed replicas of each recipe so a comparison clears the training-seed-noise band (50-150 Elo, theory 8). Elo is the primary metric of an agent. Offline proxies such as training loss, calibration, or winrate-vs-random do not substitute for it. Never conclude on, promote, or ship a new agent without its pooled Elo. (2) **Document it as one complete picture** (its results doc plus `ML.md`): inputs (feature set + count), architecture (layers, widths, activations, value or policy head, output), the search wrapper that turns the model into an agent (explorer + depth + budget), how it was trained (regime + data source + labels), the hyperparameters actually run versus what the code supports, and its Elo. A reader must never have to reconstruct what an agent is from scattered pieces.
- **Todo list:** Project tasks are tracked in `todo.md`. When a task is completed, cross it out using Markdown strikethrough (`~~like this~~`) rather than deleting it.
- **Writing style:** Avoid semicolons and em dashes. Use a comma or period instead, restructuring the sentence if needed. Avoid special Unicode characters like arrows or comparison signs. Use standard keyboard equivalents instead, such as `->` for a right-pointing arrow and `>=` for a greater-than-or-equal sign.
  - **Voice:** Write documentation as a factual guide to what things do, not as marketing. Avoid persuasive or self-congratulatory adjectives (for example "powerful", "seamless", "robust", "just faster"), do not restate the same point twice, and break long run-on sentences into shorter ones. State what a feature does and how to use it, and let the facts speak.
  - **Abbreviations:** establish the full term on first use before abbreviating, or use the full term throughout. Do not sprinkle bare abbreviations (acronyms, sweep-group letters, internal shorthand) that force the reader to hunt for what they mean.
  - **Self-contained writing:** documentation and standing instructions must read cleanly in a fresh session. Do not leave artifacts of the conversation that produced them (contrasts like "now uses X instead of Y", "changed from 2 to 6", asides that only parse if you were present). State the current fact and its durable rationale.
- **Memory mirroring + bootstrap.** The auto-memory store is per-machine. When saving an auto-memory entry about this project, mirror a short copy into the git-tracked `Docs/Memories/`. On a machine whose local store is missing entries that `Docs/Memories/` has, rebuild the local store from it before starting work. Always-on rules never go in memories, they belong in these Standing Instructions.
- **After every functional change:**
  1. Update `README.md` for any section affected by the change (build command, game rules, AI descriptions, etc.)
  2. Update this file, or the relevant per-directory `CLAUDE.md`, to reflect new files, renamed functions, or changed behavior
  3. Tell the developer **how to test** the change and **what new behavior to expect**
  4. **Archive the plan, and write a companion results document.** If the work was driven by a plan (for example a session plan under `~/.claude/plans/`), copy that plan into the repo `plans/` folder under a cleaner, descriptive name matching the existing style (`<topic>-plan-<N>-<suffix>.md`, keeping the original trailing random-word suffix). Then create a **separate companion results document** next to it named `<topic>-results-<N>-<suffix>.md` (the same name with `plan` replaced by `results`). Keep the two files separate: the plan captures intent, the results doc captures outcome. The results doc is a permanent record of the same substance you would give in the end-of-session rundown, not just a chat log. Here is a **non-exhaustive** list of what to include in this document:
     - The end-of-session rundown: a summary of all the changes made, how to test them, and the commit message(s) used
     - Results of implemented optimizations as concrete numbers: percent speedup, cpu/node before and after, Elo change, win-rate deltas, and how they were measured
     - Implementation details and any differences between the planned document and the final implementation (what changed, what was harder than expected, what was dropped or added, and why)
     - Correctness gotchas discovered and how they were resolved, plus any measurement or methodology caveats that qualify the numbers
     - A **"Future Work"** section: holes or limitations in the experiments run, each entry tethered to the specific experiment/conclusion it could confirm or refute (what the hole is, why it matters to that conclusion, what test would settle it) -- not a generic todo dump
     - An **"Ideas This Inspired"** section: a lightweight, untethered list of new ideas the work brought to mind, whether or not they relate to this session's conclusions. Lower bar than Future Work -- a reminder to think of new ideas while reflecting on the work, not required to justify itself against a specific finding
     - (A) This list is not exhaustive. Include other outcome-worthy content even when it is not listed here.
     - (B) When you notice a recurring category worth capturing that is not yet listed, help grow this list, but do not edit `CLAUDE.md` unprompted. Confirm with the developer via the multiple-choice prompt that it is a meaningful addition first.
  5. **Update the theory log.** If the results doc confirms, refutes, or opens a new testable theory, add or update its entry in `Docs/theories.md` (status, origin plan, tested-in link, and a citation key in Notes if it draws on external research).
  6. **Commit the change.** First confirm `.\tools\run_tests.ps1 -Build` passes (or already passed this session against the current code) -- do not commit on a red or unverified suite; stop and flag it to the developer instead. Then check `git status` to see what is actually uncommitted; if this change will be bundled with other uncommitted work from the session, write the message to cover **all** of those changes together, not just the latest one. Use `Add` for files being committed for the first time, `Update` only if the file was already in a prior commit. Create the commit directly (no need to ask first or wait for approval) using the standard heredoc `git commit -m` form. Do not `git push` unless the developer explicitly asks in that session.
- **Grow this file with lessons learned.** Beyond the routine factual updates in step 2 above, use each session as a moment to reflect on the project's purpose and how to best support the developer, then propose durable lessons, new workflow steps, or new instruction categories to record here so a fresh session starts better informed. This applies to every part of `CLAUDE.md`, not only the results-section list above. Do not add such discretionary changes unprompted. When you identify a meaningful addition, confirm it with the developer using the multiple-choice prompt before writing it.

---

## Project Overview

**Breakthrough** is an 8x8 abstract board game implemented in C++ with a console UI and multiple AI difficulty levels. White pieces start on rows 0-1 (the bottom of the printed board) and advance toward row 7. Black pieces start on rows 6-7 and advance toward row 0. A player wins by advancing a piece to the opposite back row or capturing all opponent pieces.

- **Language:** C++ (C++11)
- **Compiler:** MSVC (`cl`), the primary build tool
- **Alternative build:** CMake (`CMakeLists.txt`), which is not the primary workflow
- **Entry point:** `.\breakthrough.exe` from project root

---

## Build & Run

**`cl` is not on the default PATH.** Every `cl` build must first load the MSVC
environment via `vcvars64.bat`. From PowerShell, wrap the build in
`cmd /c '"<vcvars64.bat>" && cl ...'` as shown below (path matches README; adjust
the Visual Studio edition/version if yours differs).

### Console engine (`breakthrough.exe`)
```
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\datastore.cpp src\transposition.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe'
```
The `ml_*`, `datastore`, and `transposition` files are required by every build
target (see `src/CLAUDE.md`, "Engine link set"). Run with `.\breakthrough.exe`
and enter e.g. `boards\board1.txt` at the board prompt.

**Scripting the console:** pipe answers through the **Bash tool**, not
PowerShell (its pipe encoding/BOM corrupts the first `cin` read and the program
loops on the board-file prompt). Example and details: the "Driving the console
non-interactively" section of [TESTING.md](TESTING.md).

### Tests
```powershell
.\tools\run_tests.ps1 -Build     # build via build_tests.bat + run (use the /run-tests skill)
```
Omit `-Build` to re-run the last build. The raw `cl` command compiles
`tests\test_*.cpp` plus all engine sources including `ml_train.cpp` and
`ranking.cpp` with `/I src /I tests /Fe:tests.exe` (same vcvars wrapper as above).

### Trainer, ranker, tournaments
```powershell
.\tools\run_train.ps1 -Build selfplay-supervised --games 250 --epochs 6
.\tools\run_train.ps1 imitate --out models/lin_policy.txt --games 150
.\tools\run_tournament.ps1 -Workers 12 -Depths "2,4,6,8,10" -Games 10 -NodeBudget 200000
.\tools\run_rank.ps1 -Build check           # validate ranking/roster.txt
.\tools\run_rank.ps1 -Workers 8 --games 8   # process-sharded play, merge, rate
.\rank.exe gauntlet --id "<id>" --games 4   # one candidate vs the frozen pool
.\rank.exe pairgen --a "<id>" --b "<id>" --games 200 --dil-apply a --out data/pg.jsonl
.\tools\hill_climb.ps1 -Iters 40 -Games 4   # hill-climb Experimental eval weights
```
`train.exe` and `rank.exe` are separate binaries from `breakthrough.exe`. All
workflows, study scripts, subcommand flags, and artifact directories:
`tools/CLAUDE.md`. System design: `ML.md`.

### GUI
```
.\build_gui.bat          # native (needs third_party/ raylib, see INSTALL.md)
.\build_web.bat          # web via emsdk -> docs/;  "dev" arg for a debug build
```
Build gotchas (`/MD`, `WHITE`/`BLACK` macro collision) and internals:
`gui/CLAUDE.md`.

---

## File Map

One line per file. Deep detail lives in the named per-directory CLAUDE.md.

### Root
| File | Purpose |
|---|---|
| `README.md` | User-facing docs: build, run, game rules, move notation |
| `CLAUDE.md` | This file: rules, commands, map (per-directory CLAUDE.mds hold the detail) |
| `Docs/theories.md` | Running log of testable theories, each with status, origin, and tested-in links |
| `Docs/terminology.md` | Glossary of project and domain terms with definitions and example sentences |
| `Docs/axioms.md` | Breakthrough truths in four tiers: rules, project choices, proofs, empirical claims |
| `Docs/Memories/` | Git-tracked mirror of project auto-memories (see the memory Standing Instruction) |
| `Docs/benchmarking.md` | Guide to measuring engine speed: metric choice, harness contract, confounds checklist, tools |
| `TESTING.md` | Verification playbook: console driving, GUI smoke test, visual-inspection lessons |
| `INSTALL.md` | Setup: VS C++ workload, raylib download, emsdk for the web build |
| `ML.md` | ML system overview + "how to add more" (registry tables auto-generated by `train.exe docs`) |
| `CMakeLists.txt` | Alternative CMake build (not primary) |
| `minimax_params.txt` | Saved MiniMax weights, auto-loaded when a MiniMax player is selected |
| `.gitignore` | Excludes exes, `build/`, `third_party/`, and generated ML artifacts |
| `build_gui.bat` / `build_web.bat` | GUI builds: native raylib exe / Emscripten WASM to `docs/` |
| `build_tests.bat` / `build_train.bat` / `build_rank.bat` | MSVC batch builds for `tests.exe` / `train.exe` / `rank.exe` |
| `tools/*.ps1` | Run/build wrappers, smoke test, study scripts: see `tools/CLAUDE.md` |
| `ranking/`, `runs/`, `data/`, `models/`, `agents/` | Persistent Elo state and ML artifacts: see `tools/CLAUDE.md` |
| `analysis/`, `train_py/`, `requirements.txt` | Optional Python layer: DuckDB queries, model export contract |
| `plans/` | Archived session plans + companion results docs |
| `.claude/skills/run-tests.md` | Skill: always run tests via `run_tests.ps1 -Build`, never bare `cl` |

### `src/` (details: `src/CLAUDE.md`)
| File | Purpose |
|---|---|
| `globals.h` / `globals.cpp` | Master header (macros, enums, externs, prototypes) and global definitions |
| `main.cpp` | Console game loop, per-side evaluator/params, test-sweep mode, eval display |
| `board_io.cpp/.h` | Board file I/O, `printBoard()`, `loadMinimaxParams()` |
| `settings.cpp/.h` | Interactive CLI configuration (`getSettings()`), winner display |
| `moves.cpp/.h` | Move dispatch, validation (full + fast), execution, simulate/unsimulate with incremental accumulators |
| `board_analysis.cpp/.h` | Chip counting and one-step win detection (`canWin*`, `findWin*`) |
| `ai_eval.cpp/.h` | Evaluator registry (Classic, Experimental, LearnedValue) + incremental leaf eval |
| `ai_random.cpp/.h` | Random-move choosers, scripted openers, pluggable opener registry `g_openers[]` |
| `ai_minimax.cpp/.h` | Alpha-beta search, iterative-deepening budgets, telemetry, opt-in TT/ordering/aspiration |
| `ml_features.cpp/.h` | Move generation + ML feature extractors (v1 dense, v2 sparse piece-square) |
| `ml_model.cpp/.h` | Model base + LinearModel, save/load factory, model-type registry |
| `ml_eval.cpp/.h` | 128 model slots, `mlValueScore`, incremental v2 path, move rating |
| `explorers.cpp/.h` / `choosers.cpp/.h` | Explorer registry (Greedy, AlphaBeta) and direct-chooser registry |
| `agents.cpp/.h` | `AgentSpec` composition (brain + dilution + budgets + toggles), `agentChooseMove` |
| `datastore.cpp/.h` | Append-only JSONL + canonical `positionKey` (also keys the TT) |
| `transposition.cpp/.h` | Opt-in transposition table (inert unless `useTT`) |
| `ml_train.cpp/.h` | Trainer: self-play, supervised value + imitation policy, tournament play/rate, run archive |
| `ranking.cpp/.h` | Persistent Elo ranking: ID codec, roster, match store, scheduler, BT fit, subcommands |

### `tools/` C++ CLIs (details: `tools/CLAUDE.md`)
| File | Purpose |
|---|---|
| `train_main.cpp` | `train.exe` CLI subcommand dispatch |
| `rank_main.cpp` | `rank.exe` CLI subcommand dispatch |

### `gui/` (details: `gui/CLAUDE.md`)
| File | Purpose |
|---|---|
| `main_gui.cpp` | raylib + raygui front end: layout, state machine, widgets, pacing controls |
| `raygui.h` | Vendored single-header raygui v4 |
| `shell.html` | Emscripten HTML shell for the web build |

### `tests/` (details: `tests/CLAUDE.md`)
| File | Purpose |
|---|---|
| `catch.hpp`, `helpers.h`, `test_main.cpp` | Catch2 framework, shared utilities, entry point |
| `test_*.cpp` | Move validation, win detection, eval (+ incremental equivalence), AI integration, game outcomes, ML, ranking |

### `boards/`
| Files | Purpose |
|---|---|
| `board1.txt` - `board5.txt` | Standard starting configurations |
| `puzzle1.txt` - `puzzle13.txt` | Mid-game tactical positions for testing AI |

---

## Verification Checklist

See [TESTING.md](TESTING.md) for the full playbook. The condensed checklist
after any change:

1. Build succeeds with the `cl` command above (no errors or warnings introduced)
2. `.\tools\run_tests.ps1 -Build` passes all assertions (use the `/run-tests` skill)
3. `.\breakthrough.exe` launches, loads `boards\board1.txt`, and displays the board
4. Run a quick Human vs. UniformRandom game (a few moves) to confirm basic flow
5. **For AI changes:** run MiniMax (depth 3) vs. MiniMax (depth 3) with `PRNT=1`. Confirm `nodesWhite`/`nodesBlack` stats print and the game completes.
6. **For eval/weight changes:** compare win rates over a 10-game batch before and after
7. **For GUI changes:** `.\tools\smoke_test_gui.ps1 -Build` (details: `gui/CLAUDE.md`)
