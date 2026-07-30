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
`tools/CLAUDE.md` explicitly first. Before designing, training, or evaluating
any ML model or training regime, read `Docs/model-training-playbook.md` in
full first -- the three-pass process, configuration-design rules, and
interactivity requirements are not optional and are not summarized here.

## Vocabulary

Use these terms exactly, in chat and in docs. This is the short list: each one
has already been misused in a way that produced a wrong statement. The full
glossary (67 terms, each with an example sentence) is `Docs/terminology.md`,
which is NOT force-loaded, so read it before writing a doc or a strength claim.

- **head** (search head) = the leading ID segment, the explorer and its search
  settings (`ab(d6,tt,ord,nb200k)`). An agent is a core + loadout on a head, so
  different heads are different players. Fix ONE head per comparison and say
  which. Give full canonical IDs, not abbreviations.
- **active / retired ("gone")** = an agent's roster state. A retired row is a
  superseded identity frozen at old game counts, so its Elo is history, never
  current strength. Read `ranking/standings.tsv` (active only, grouped by head)
  for standings, not `ranking/ratings.tsv` (the full historical fit).
- **core** = evaluator + search, the thing under study. **loadout** = the
  optional add-ons it wears (book, `qs`, `tt`, `ord`, `asp`, dilution), readable
  off the ID. An agent is **bare** (empty loadout) or **equipped**; a comparison
  is **loadout-matched** when both wear the same items. **lift** = the Elo one
  item adds to one core, and it does not transfer between cores. Never compare a
  bare core to an equipped one and call it an evaluator result.
- **turn weight** (`t`) = inert at fixed depth: it shifts every leaf by the same
  constant and reorders nothing unless leaves sit at mixed ply parity (`qs`, or
  `part`). Two IDs differing only in `t` are the same player. Compare cores on
  standings' `eff_evaluator` column, which elides it.
- **us/node** = per-leaf cost, the speed number that survives comparison across
  evaluators. us/move confounds leaf cost with how much the evaluator prunes.
- **seed-noise band** = 50-150 Elo between seed replicas of one recipe, so a
  single seed's result is never a conclusion. Check any margin against the error
  bars before calling it real.

## Standing Instructions

- **Commit at natural checkpoints, without waiting to be asked.** After finishing a task or functional change (normally as the last step of the "After every functional change" workflow below, once the results doc and other updates are in place), create the commit yourself. Review `git status`/`git diff` first and never stage or commit anything that looks like a secret. **Never run `git push`** without explicit developer instruction given in that session; a prior commit approval does not carry forward to pushing.
- **Doc changes ride along with the next commit.** Include uncommitted edits to CLAUDE.md files, `todo.md`, `Docs/`, or `plans/` wholesale in whatever substantive commit happens next. Never cherry-pick hunks to exclude them, give them a dedicated commit, or describe them in the message (documents are self-evident). "Dedicated commit" requests mean the deliverable gets its own commit, not that entangled doc hunks get excluded.
- **Tests must pass before committing.** Run `.\tools\run_tests.ps1 -Build` (or confirm it already passed against the current code this session) before every commit. If any assertion fails, fix it or, if that isn't possible in scope, stop and tell the developer instead of committing. The one exception is a commit that touches no `src/`/`tests/`/build-affecting file (e.g. docs-only or `plans/`-only changes), where the test suite is a no-op anyway.
- **Give context first, then ask with the tool.** A real question must be posed through the AskUserQuestion tool, never as prose that ends the turn (that just stops and waits for a re-prompt instead of asking). When there is meaningful context to convey before the question, write it as a normal chat message and then invoke AskUserQuestion in the same turn, as the turn's final action. Do not stop after the context without asking, replace the widget with a prose question, or stuff the explanation into the widget's option descriptions.
- **Champion declaration and ranking-claim hygiene.** `ranking/CHAMPION.md` is the single source of truth for who each category champion (the standing dethrone target for its category) is. Since 2026-07-28 the throne is split into 5 parallel categories by opener loadout (openless / 4-book / 8-book / 4-random / 8-random, defined in `CHAMPION.md`'s summary table) rather than one single champion; docs reference that file instead of embedding champion Elo numbers, and a doc that must quote a number tags it with the fit date. Rules for any claim about the ranking: (1) each category champion is the highest pooled-Elo target-class agent within its category (an agent's `.opener(...)` ID segment decides category membership, see `CHAMPION.md`) in the one full-roster anchored refit, with reference-class agents (currently the d8/nb2m oracle) excluded and named in CHAMPION.md; (2) top-of-table claims require every contender pair at >= 32 games -- boost via `ranking/roster_top.txt` (keep its list current) and refit; never conclude from 8-games/pair fills, which have twice inverted at 32 games/pair; (3) never compare absolute Elo across fits (the Bradley-Terry prior compresses the scale as the pool grows, see `Docs/benchmarking.md`) -- compare order and error bands within one fit; (4) when the top may have changed (a new agent rates near the top, or a cohort joins the pool), re-certify and update `ranking/CHAMPION.md` and `todo.md`'s Agent Track goal paragraph in the same session; (5) **read standings from `ranking/standings.tsv`, not `ranking/ratings.tsv`.** `ratings.tsv` is the full historical fit and includes retired agents (`active` = `gone`: superseded `@N` code versions frozen at whatever game count they had when they left the roster). Quoting a retired row as current strength is wrong, and it has already produced a wrong conclusion (a `gone` classic row at 1081 vs its live identity at 990 -- a 91-Elo phantom gap). `standings.tsv` is written by the same fit with active agents only, grouped by search head; (6) **an agent is search + evaluator, so compare only within ONE search head.** `ab(d6,tt,ord,nb200k)` and `ab(d6,ord,nb200k)` are different agents; a table mixing heads is not an evaluator comparison. State the head explicitly in any standings table, and give full canonical IDs rather than abbreviations; (7) **count DISTINCT games, not stored rows.** `rand()` is consumed only by dilution and random-move agents, so a pair with neither has an inert seed and replays one game per colour however many rows the store holds. Before quoting any record or trusting any error bar, count distinct trajectories (colour + plies + result + both node totals) and report both numbers, as in "25-7 nominal, 8-4 on 12 distinct games". Median across the store is 0.438 distinct games per row, so printed `pm` is understated by roughly 1.5x and far more for deterministic pairs. Full explanation and the repair options: `Docs/benchmarking.md`, defect 3.
- **Validate the instrument before quoting the reading.** This applies to every measurement, not just Elo: win rates, us/node, speedups. Before reporting a number, confirm the harness actually produced what you are claiming to have measured, and state the check you ran. The three that have caught real errors here: (a) **independence** -- how many distinct results are in this sample, as opposed to rows? (b) **the knob does what you think** -- if a flag is supposed to introduce variation or change behavior, run it at two settings and show the output differs; (c) **residual state** -- name what carries across trials (transposition table, accumulators, process length, cross-game seeds) and report the batching used, preferring several short processes over one long one. Running these checks AFTER publishing is the specific failure this rule exists to prevent. It has happened twice: a retired-row Elo table (2026-07-25) and an opening-diversification column reported before confirming its seeds were live (2026-07-26). The checks are cheap. A number published before them is a guess with a decimal point.
- **A defect you discover applies to your own draft first.** When a measurement defect surfaces mid-task, re-check every number already in your own output against it before publishing, and say explicitly what survived and what did not. Flagging a defect in the project's past work while leaving it unapplied to the paragraph you are currently writing is not credible. Concrete instance: defect 3 was written up and 37 documents were flagged for it in the same session where the author's own `+124` book-lift quote went out without the correction applied.
- **These hygiene rules govern chat messages, not just committed files.** A table in a reply is a claim and gets the same treatment as one in a doc: full canonical IDs, the search head stated, the opponent named, and effective sample size where it is not obvious. A summary that is looser than the results doc it summarizes has invented a wrong claim rather than simplified a right one -- a chat table mixing two heads and two opponents (2026-07-26) reproduced the exact defect its own results doc was about.
- **A new model or agent is not done until its Elo is measured and it is documented, and it is not done in one pass.** The full procedure -- three required passes (sanity, broad sweep, optimize), minimum seed counts, consistent checkpoint rungs across a study, interactivity checkpoints with the developer, and the certification gate -- is `Docs/model-training-playbook.md`, read in full before this work starts (see the trigger above). Never conclude on, promote, or ship a new agent without its pooled Elo from a full-roster refit; offline proxies (loss, calibration, winrate-vs-random) do not substitute for it.
- **Compute is cheap; do not pre-shrink an experiment to save it.** Default to the thorough version: the full roster rather than a subset, every seed rather than the strongest one, 32 games/pair rather than 8. Do not drop an agent, a seed, or a condition from a study because it looks expensive, and do not silently substitute a cheaper design for the one that answers the question. If cost genuinely forces a choice, say so and let the developer decide rather than deciding alone. **The 2-hour / 1-day rule:** when a run passes 2 hours, measure its progress rate and project the finish time. If the projection exceeds one day, stop and tell the developer the projection, then let them choose between letting it run and reducing scope. Under a day, just let it run and report when it lands. (Developer instruction, 2026-07-26, after several experiments were needlessly narrowed on cost grounds.)
  - **Do not narrate estimates.** Set the work up and run it. Do not produce wall-clock or compute-cost tables, "this will take ~N hours" asides, or projection matrices. The 2-hour / 1-day rule above is the ONLY time a projection is wanted, and then it is one number with the progress measurement behind it, delivered because the developer has a scope decision to make. Anything finishing inside a day is not worth mentioning until it lands. (Developer instruction, 2026-07-29: "You talk so so much about wall clock and experiment time estimates. I don't care... if it's anything less than a day, then it's not worth fussing over at all. Don't waste your tokens making estimates all the time." Flagged as a cross-session habit, not a one-off.)
- **Todo list:** Project tasks are tracked in `todo.md`. When a task is completed, cross it out using Markdown strikethrough (`~~like this~~`) rather than deleting it.
- **Writing style:** Avoid semicolons and em dashes. Use a comma or period instead, restructuring the sentence if needed. Avoid special Unicode characters like arrows or comparison signs. Use standard keyboard equivalents instead, such as `->` for a right-pointing arrow and `>=` for a greater-than-or-equal sign.
  - **Voice:** Write documentation as a factual guide to what things do, not as marketing. Avoid persuasive or self-congratulatory adjectives (for example "powerful", "seamless", "robust", "just faster"), do not restate the same point twice, and break long run-on sentences into shorter ones. State what a feature does and how to use it, and let the facts speak.
  - **Report the data, do not characterize it.** Give the numbers and stop. Do not add a summarizing adjective or trend word that the data has not been tested for -- "behaves monotonically" over three non-monotonic points, "converged to", "independently rediscovered", "catastrophic". Each of these has already produced a wrong statement here. If a characterization is worth making, it is worth computing first and reporting as its own measured claim. When a number is the developer's to interpret, list it plainly, including the unflattering and the inconvenient ones, rather than pre-digesting it.
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
- **Correcting a document this project already wrote: use the existing banner convention, and register the defect class in `Docs/corrections.md`.** This project has had a working convention since 2026-07-25, instantiated on 38 documents: a dated blockquote banner at the TOP of the affected document, naming a reusable defect class. Follow it. Do NOT invent a parallel vocabulary (a `[HINDSIGHT]` tag was invented on 2026-07-29 for exactly this job by someone editing a file whose first six lines demonstrate the real convention; it was removed). Format:
  ```
  > **[<DEFECT NAME> - flagged <YYYY-MM-DD>]** <what was wrong, what it means for
  > this document's numbers, what to do instead, and a pointer to Docs/corrections.md.>
  ```
  `<DEFECT NAME>` is short, uppercase, and reused verbatim everywhere, so `grep -rn "<DEFECT NAME>"` enumerates the affected set. Register every name in `Docs/corrections.md`, which is the index (the analogue of a Wikipedia tracking category or the RFC index) -- a class that is not registered there is not discoverable, which was the whole failure. Existing classes: `ELO HYGIENE UNVERIFIED`, `SELF-PLAY CONVERGENCE UNSUPPORTED`. Keep the original claim text visible rather than deleting it: the wrong claim is why the mark exists, and later readers must recognise it if they meet it quoted somewhere that was missed. Say both (a) what the old text actually established versus what it was read as establishing, and (b) what does and does not transfer to the new situation, because "too weak to conclude" and "does not apply here at all" are different corrections and a reader needs both.
  - **Also mark every place the claim can be QUOTED FROM, with a separate short note.** This is the load-bearing half, and no document-level convention covers it -- the closest established analogue is a code deprecation warning firing at the USE site rather than the definition. At each quoting site (a `CLAUDE.md` table row, a script header, a `todo.md` summary) add `(see <DEFECT NAME>, Docs/corrections.md)`. Keep it distinct from the banner: the banner answers "is this document sound?", the note answers "is this claim safe to quote?". Concrete instance: `plans/training-sweep-results-1-luminous-snail.md` item 3 correctly recorded in 2026-07-24 that the self-play convergence stop "triggered on noise, not on convergence" and warned "do not trust 'self-play plateaus at 500'". On 2026-07-29 that exact claim was asserted as measured fact anyway, because the places a reader actually reaches for it carried the headline with no caveat. A correction that lives only in a results doc does not travel. (Developer instruction, 2026-07-29.)
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
| `Docs/works-cited.md` | External research referenced in this project's docs and decisions (currently: Bergstra & Bengio 2012 on random search) |
| `Docs/axioms.md` | Breakthrough truths in four tiers: rules, project choices, proofs, empirical claims |
| `Docs/Memories/` | Git-tracked mirror of project auto-memories (see the memory Standing Instruction) |
| `Docs/benchmarking.md` | Guide to measuring engine speed: metric choice, harness contract, confounds checklist, tools |
| `Docs/corrections.md` | **Index of defect classes found in this project's own past writing** (`ELO HYGIENE UNVERIFIED`, `SELF-PLAY CONVERGENCE UNSUPPORTED`), and the banner + point-of-citation convention. Read before quoting a number out of any `plans/` doc |
| `Docs/ranking-workflow.md` | **Step-by-step: how to add agents to the roster and read a ranking.** Workflow A (add + screen on a frozen scale via `play --cohort` + `rate --pin`, cannot disturb anything) vs Workflow B (the unpinned refit, the only thing that can dethrone a champion). Read before any ranking run |
| `Docs/model-training-playbook.md` | **Read in full before designing, training, or evaluating any ML model.** The three-pass pipeline (sanity -> broad sweep -> optimize), configuration-design rules (minimum seeds, consistent rungs, present the grid before running it), interactivity checkpoints with the developer, extension points, and the certification gate |
| `Docs/hyperparameter-log.md` | Cross-study reference of hyperparameter values actually tried, by training regime. Consult before setting a Pass-2 sweep's ranges; growing document, not force-read (see `todo.md` for the backfill task) |
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
| `ml_model.cpp/.h` | Model base + Linear/MLP/Residual/Dist models, probit distribution math, save/load factory, model-type registry |
| `ml_eval.cpp/.h` | 128 model slots, `mlValueScore`, incremental v2 path, move rating |
| `explorers.cpp/.h` / `choosers.cpp/.h` | Explorer registry (Greedy, AlphaBeta) and direct-chooser registry |
| `agents.cpp/.h` | `AgentSpec` composition (brain + dilution + budgets + toggles), `agentChooseMove` |
| `datastore.cpp/.h` | Append-only JSONL + canonical `positionKey` (also keys the TT) |
| `transposition.cpp/.h` | Opt-in transposition table (inert unless `useTT`) |
| `ml_train.cpp/.h` | Trainer: self-play, supervised value + imitation policy, tournament play/rate, run archive |
| `ranking.cpp/.h` | Persistent Elo ranking: ID codec, roster, match store, scheduler, BT fit, subcommands (incl. the posgen/label/labelfit position-oracle pipeline) |

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
