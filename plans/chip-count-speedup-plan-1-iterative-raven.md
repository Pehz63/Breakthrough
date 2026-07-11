# Plan: Retrospective v1 baseline -- measure the chip-count (v1->v2) speedup

## Context

Theory 16 (`Docs/theories.md:344`) claims the heuristic evaluator's terms can be
computed incrementally (updating only the two squares a move changes) for
byte-identical results at lower cpu/node. Two of its three instances are fully
measured and documented:

- **v2->v3** (wall/column/forward structure made incremental via cached
  `g_evalPos`): measured at a **33-39% cpu/node reduction**, recorded in
  `plans/incremental-wall-column-eval-results-1-golden-forest.md` and Theory 16,
  guarded by `test_eval.cpp`'s equivalence walk.
- The learned-model instance (`g_mlAcc`): measured in
  `plans/incremental-ml-eval-results-1-luminous-snail.md`.

The ONE unmeasured gap is the ORIGINAL instance: the chip-count migration in
commit `3af970d` (2026-06-04), the **v1->v2** step, which replaced a full-board
`chipDiff()` rescan with the incremental `g_chipDiff`/`g_whiteCount`/
`g_blackCount` counters. It shipped before the developer's documentation habit,
so it has no measured speedup number and no equivalence check.

This session does two things:

1. **Measure v1->v2** (the unmeasured chip-count speedup) for the first time.
2. **Re-measure v2->v3** under the SAME harness. The existing 33-39% figure was
   produced a DIFFERENT way (us/**node** diffed across two builds via the ranking
   system over seeded whole games, per
   `plans/incremental-wall-column-eval-results-1-golden-forest.md`), not the
   isolated `speedBench` harness. A single-harness re-measurement is a
   cross-check: if it reproduces ~33-39% that validates both methods, if it
   diverges the new same-harness numbers become canonical and the discrepancy
   itself is a finding worth understanding (build flags, timing granularity,
   whole-game vs fixed-position sampling). All three levels get canonical numbers
   from one run.

Note also the existing `train.exe speed` benchmark is confounded for this purpose:
when wall and column weights are 0, `evalPosFull` / `evalPosLocal` short-circuit
the entire structure scan (`ai_eval.cpp:76,79,137`). The v1/v2 comparison must run
at NONZERO structure weights so the leaf actually does the full-board work whose
chip term we are isolating.

### Retrospective nature -- how it changes the workflow

This is not the usual theory -> feature -> test loop. The feature (incremental
chip count) already exists, is the winner, and is already documented as CONFIRMED
in Theory 16. We are reconstructing ONE prior version of the code (v1, the
full-board chip rescan) that no longer exists in the tree, purely to measure how
much the `3af970d` optimization bought, retroactively filling Theory 16's empty
"measured speedup" cell for the chip-count instance. Consequences:

- The deliverable is a NUMBER (the v1->v2 chip-count speedup), not a code change
  to keep. v2 is not the winner either -- v3 is -- but v3's advantage over v2 is
  already measured, so v3 is out of scope here.
- v1 is a reconstruction, so its correctness check is behavioral: it must produce
  IDENTICAL move choices (and identical nodes/move) to v2 on the benchmark
  positions -- same search, only leaf cost differs. This is the retrospective
  stand-in for the equivalence test `3af970d` never had.
- The v1 code is scaffolding for a measurement, not a feature. Per the user: once
  results are collected it need not be left lying around, but keep it if it is
  cheap to keep. Plan keeps it behind an inert default-off benchmark toggle so it
  costs nothing at runtime and can be deleted later in one commit.
- Documentation target is Theory 16's existing chip-count cell plus a results
  doc, NOT a new theory. The theory is already confirmed; we back-fill its
  evidence.

## Version definitions (heuristic Classic/Experimental)

Only v1 needs new code. v2 and v3 already exist as the current non-incremental and
incremental leaf paths respectively. All three run the SAME alpha-beta search and
MUST pick identical moves (identical nodes/move) at matched depth.

- **v1** (NEW, the reconstruction) -- pre-`3af970d`. Every leaf: full-board chip
  rescan via a reintroduced `chipDiff()` + full `evalPosFull` structure scan.
- **v2** (already reachable) -- incremental `g_chipDiff` + full `evalPosFull`
  scan. EXACTLY the current non-incremental heuristic leaf path (`evalLeaf` ->
  `evaluateBoard` -> `evalClassic`, `ai_eval.cpp:176,187`), reached when
  `g_evalIncremental` is false. The `3af970d` chip optimization, without the later
  structure cache.
- **v3** -- incremental `g_chipDiff` + cached `g_evalPos`. The current shipping
  default. Re-measured here against v2 as a cross-check on the existing 33-39%.

The v1->v2 delta is the chip-count speedup; the v2->v3 delta is the structure
speedup (cross-check). All at nonzero wall/column weights so the structure scan is
real.

## Implementation

### 1. Reintroduce the full-board chip rescan (`src/moves.cpp` / `src/moves.h`)

Recovered verbatim from commit `3af970d^`:
```cpp
int chipDiff() {                 // full-board rescan (v1 baseline only)
    int count = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) count++;
            else if (board[x][y] == BLACK) count--;
        }
    return count;
}
```
Declare in `moves.h`. Used only by the benchmark leaf at v1. (The per-row
`chipDiff(int)` overload is not needed and is not restored.)

### 2. Benchmark-only eval-level selector (`src/globals.h`, `src/globals.cpp`, `src/ai_eval.cpp`)

Add a global `int g_evalLevel = 3;` (default 3 == current shipping behavior, so
console/GUI/tests/tournaments are byte-for-byte unchanged). Meaning: 1 = full
chip rescan + full structure, 2 = incremental chip + full structure, 3 =
incremental chip + cached structure. Consulted in exactly two places:

- `evalBeginSearch` (`ai_eval.cpp:149`): seed `g_evalPos` and set
  `g_evalIncremental = true` only when `g_evalLevel == 3`. For levels 1-2 leave
  `g_evalIncremental = false`, so make/unmake in `moves.cpp` does NOT maintain the
  `g_evalPos` accumulator -- v1/v2 must not pay to maintain state they ignore, or
  they are not honest baselines. Learned-model `mlIncrementalBegin` untouched.
- `evalLeaf` (`ai_eval.cpp:174`): keep the v3 cached fast path exactly as is
  (guarded by `g_evalLevel==3` / `g_evalIncremental`). On the non-incremental
  branch (levels 1-2), keep the same `nearWinCheck` shortcut, turn term, and
  `evalPosFull` scan, but choose the chip term by level: `g_chipDiff` at level 2,
  `chipDiff()` (full rescan) at level 1. Keeping `nearWinCheck` and `evalPosFull`
  identical across all levels guarantees they differ ONLY in the two intended
  axes, so any move-choice or nodes/move difference is a reconstruction bug.

`g_evalLevel` is set only by the benchmark and restored to 3 after, inert in every
shipping path. One int, two small branches, deletable later in one commit.

### 3. Extend `speedBench` (`src/ml_train.cpp:1320`) with rigor upgrades

The three-level comparison needs nonzero structure weights and a fair harness. The
current benchmark's zero-weight `chip`/`chip+forward` presets skip structure
entirely (the confound), and its timing loop has gaps a rigorous run should close.
Add a dedicated level-ladder block AND tighten the harness:

- **Nonzero-weight preset:** Classic `{turn=1, chip=4, wall=2, col=2}` (plus the
  Experimental `{..., forward=2}` variant), so `evalPosFull` does real per-leaf
  work.
- **Level ladder:** for each depth `d = 1..maxDepth`, time the SAME `AlphaBeta`
  agent three times with `g_evalLevel` = 1, 2, 3 (save/restore around each rep,
  like the existing `restoreState`), emitting `chip-v1-full`, `chip-v2-incr`,
  `chip-v3-cached` rows. Reuse `g_lastNodes` reporting.
- **Harness rigor upgrades** (the reason not to trust the old numbers blindly):
  - Switch timing to `std::chrono::steady_clock` (monotonic; `high_resolution_`
    can be non-monotonic on some MSVC configs) and report both mean us/move AND a
    dispersion stat (min + median, or stddev) so noise is visible, not hidden in a
    single mean.
  - Fixed iteration count per (agent, position) rather than a wall-clock budget,
    so every level does identical work and slow levels are not under-sampled by
    the `el >= msPerAgent` break. Add warmup reps (discarded) before timing to
    settle caches/branch-predictor.
  - Interleave levels per position (v1,v2,v3 back-to-back on the same board)
    rather than all-v1-then-all-v2, so thermal/frequency drift hits all levels
    equally instead of biasing one.
  - Pin the position set with a fixed `--seed` and print it, so a run is
    reproducible.
- **Self-check:** for every benchmark position assert `agentChooseMove` returns
  the same victor code and same `g_lastNodes` at v1, v2, v3 (retrospective
  stand-in for the equivalence test `3af970d` lacked). Print one-line PASS/FAIL
  and abort the reported numbers on FAIL.
- Keep the existing four `Var` presets and learned-model rows so the zero-weight
  vs nonzero-weight contrast stays visible.

No dispatch change needed in `tools/train_main.cpp` (the `speed` subcommand exists
at `train_main.cpp:197`); note the level ladder + new flags (`--reps`, `--warmup`)
in the help text.

### 4. Reusable performance-measurement guide (`Docs/benchmarking.md`, NEW)

A standalone guide so this timing workflow is replicable and reusable for future
speed work (e.g. heuristic vs learned evaluator runtime). Contents:

- **When to benchmark and what to measure:** us/node vs us/move vs total CPU/game,
  and why nodes/move must be held identical to attribute a delta to leaf cost
  alone (the "same search, different leaf" invariant used here).
- **The harness contract:** fixed seeded position set, fixed rep count, warmup,
  interleaved variants, monotonic clock, report mean + dispersion, assert
  functional equivalence (same move / same nodes) before trusting timings.
- **Confounds checklist:** weight short-circuits (the wall/col=0 case),
  build-flag/optimization parity across the versions compared, whole-game vs
  fixed-position sampling, thermal/frequency drift, cold caches, and comparing
  numbers produced by two DIFFERENT harnesses (the exact reason the old v2->v3
  us/node number needed re-measurement here).
- **Tools:** what `speedBench` provides today; for finer microbenchmarks point to
  Google Benchmark (`benchmark::DoNotOptimize`/`ClobberMemory`, auto rep-count,
  quantiles) and nanobench (single-header, easy to vendor like `catch.hpp`) as
  options if isolated per-call timing is ever needed, with the tradeoff (they time
  a call in isolation, not a realistic search, so they answer a different
  question). Note the process-level `GetProcessTimes` CPU accounting already in
  `ranking.cpp` for honest CPU under parallel contention.
- **How to add a new comparison:** worked example of adding a heuristic-vs-learned
  row to `speedBench` (both are already `AgentSpec`s, so it is one `Bench` entry
  each at matched depth), and how to read the result.
- **Provenance:** link the chip-count results doc as the first worked application.

## Files touched

- `src/moves.cpp`, `src/moves.h` -- reintroduce `chipDiff()` (full-board rescan).
- `src/globals.h`, `src/globals.cpp` -- `g_evalLevel` (default 3).
- `src/ai_eval.cpp` -- `evalBeginSearch` + `evalLeaf` consult `g_evalLevel`.
- `src/ml_train.cpp` -- `speedBench` level ladder + harness rigor upgrades +
  self-check.
- `tools/train_main.cpp` -- help text (`--reps`, `--warmup`, level ladder note).
- `Docs/benchmarking.md` -- NEW reusable guide.

## Verification

1. Build the trainer: `.\build_train.bat` (or `.\tools\run_train.ps1 -Build docs`
   to confirm it links).
2. Build + run the test suite: `.\tools\run_tests.ps1 -Build`. All existing
   assertions must still pass -- `g_evalLevel` defaults to 3 so nothing shipping
   changes, and `test_eval.cpp`'s existing equivalence walk still guards level 3.
3. Run the benchmark:
   `.\tools\run_train.ps1 speed --positions 24 --reps 200 --warmup 20 --maxdepth 6 --seed 1`.
   Confirm: (a) the v1==v2==v3 move-choice + nodes self-check prints PASS;
   (b) at matched depth `chip-v1-full` >= `chip-v2-incr` >= `chip-v3-cached` in
   us/move with nodes/move IDENTICAL across all three (if nodes differ, a
   reconstruction is wrong); (c) the v2->v3 delta lands near the previously
   documented 33-39% -- if not, record the same-harness number as canonical and
   note the discrepancy.
4. Sanity: run a normal MiniMax vs MiniMax game via `.\breakthrough.exe`
   (depth 3, PRNT=1) to confirm the default (level 3) engine path is unchanged.

## Documentation (per CLAUDE.md "After every functional change")

- **Results doc** `plans/chip-count-speedup-results-1-<suffix>.md` (paired with a
  copied plan): the v1/v2/v3 us/move + dispersion + nodes/move table per depth;
  the v1->v2 chip speedup (the new number) and the v2->v3 structure speedup (the
  cross-check) with an explicit comparison to the old 33-39% and any explanation
  for a discrepancy; methodology caveats (benchmark reconstructs a prior version
  that no longer ships; timing over whole move selections, not isolated eval
  calls; positions are random-teacher mid-game boards; measured at one
  nonzero-weight preset); a Future Work section (higher depth / other weight mixes
  / TT-ordered path); an Ideas-This-Inspired section.
- **Theory 16 update** (`Docs/theories.md:344,361`): fill the empty speedup cell
  for the ORIGINAL chip-count (`3af970d`) instance -- add the measured v1->v2
  speedup, cite the new results doc under "Tested in", note this retroactively
  supplies the measurement + move/nodes equivalence check `3af970d` shipped
  without. For the v2->v3 wall/column entry, record the same-harness re-measured
  number alongside the original 33-39% (reconciled or flagged), so the theory now
  carries both and states which harness produced which.
- **`Docs/benchmarking.md`** (new): the reusable guide (section 4 above).
- **CLAUDE.md**: update the `train.exe speed` description (it now also runs the
  heuristic v1/v2/v3 level ladder + self-check with `--reps`/`--warmup`); note
  `g_evalLevel` in the globals table as a benchmark-only default-3 toggle; add
  `Docs/benchmarking.md` to the root file table; reintroduce `chipDiff()` in the
  `moves.cpp` row if warranted.
- **Commit** at the end per the updated CLAUDE.md Standing Instructions: once the
  results doc, Theory 16 update, `Docs/benchmarking.md`, and CLAUDE.md edits are in
  place and `.\tools\run_tests.ps1 -Build` passes, review `git status`/`git diff`
  (no secrets), then create the commit myself. The entangled doc/plan edits ride
  along wholesale in that same commit, not described in the message. Do NOT
  `git push` (no session instruction to). Use `Add` for new files
  (`Docs/benchmarking.md`, the results/plan docs), `Update` for existing ones.
