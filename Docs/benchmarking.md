# Benchmarking guide: measuring engine speed and strength

How to measure the runtime cost of engine code (evaluators, search features,
incremental accumulators) so the numbers are trustworthy, reproducible, and
comparable across sessions. Written alongside the chip-count speedup study
(`plans/chip-count-speedup-results-1-*.md`), the first worked application of
this workflow. Use it whenever a change claims to be faster, or when comparing
two implementations (for example the heuristic evaluator vs a learned one).

## Measuring strength (developer-set convention, 2026-07-12)

Agent strength comparisons run on the FULL main roster: `rank.exe gauntlet
--id <candidate>` with the default `ranking/roster.txt` (all active rated
agents, anchored scale, pool ratings held frozen). The point is diversity:
candidate variants never play each other, but they are compared through the
same broad frozen opponent set, so "does variant A still beat the same
opponents variant B beats" is answered on one scale. Do NOT draw strength
conclusions from the small `ranking/climb_roster.txt` pool -- it exists only
as the hill climber's cheap fitness function (few opponents, one family,
large SEs; the theory-19 identity artifact reached ~200 Elo there vs ~95 on
the main pool).

Rules of thumb:

- Vary ONE thing between compared candidate IDs (same head, same scale, same
  seed set) so the theory-19 identity artifact is shared as far as possible.
- Run at least two `--seed` replicates per config and read differences
  against the replicate spread, not the printed single-gauntlet SE.
- Comparing near-identical agents (re-labeled equivalents, tie-break-only
  variants) by gauntlet is unreliable at the ~100 Elo level; use `rank.exe
  pairgen` byte-level game comparison for behavioral identity, and a full
  pool refit (`rank.exe run`) for permanent ratings.
- Two different seeds: the gauntlet `--seed` varies the OPPONENTS' games. If
  the agent itself carries a random seed (e.g. the Advanced jitter's
  NoiseSeed), that is a SEPARATE axis, and a single value can be an outlier.
  Sweep the agent's own seed before concluding on a stochastic agent -- a
  bounded-jitter "cost" of -27/-79 Elo evaporated (mean within noise, 2 of 6
  seeds above baseline) once the NoiseSeed was swept.
- Full refits are the permanent record; gauntlets are the screening
  instrument.

### A new learned model: full run + training-seed replicas (2026-07-14)

A new learned model or agent is not done until its Elo is recorded. Measure it
with a FULL-roster refit (`rank.exe run`, not a lone gauntlet), rated at BOTH
standard heads (depth 4 and depth 6 at the nb200k budget) so it sits on the pool's
depth ladder next to the champion. A learned model's own TRAINING seed (weight init
plus data shuffle) adds 50-150 Elo of noise, theory 8 -- a separate axis from the
gauntlet's opponent seed above -- so train about 6 training-seed replicas of each
recipe and read a recipe's strength as the mean and spread over its seeds. Elo is
the primary metric of an agent. Offline proxies such as training loss, calibration,
or winrate-vs-random do not substitute for it. This mirrors the CLAUDE.md standing
instruction "A new model or agent is not done until its Elo is measured and it is
documented."

### Elo scale drift across fits (2026-07-17)

Absolute Elo values are NOT comparable between fits taken before and after the
pool grows. The Bradley-Terry fit adds 0.5 virtual games at 50% score per
PLAYED pair (`rankFitBT`, `src/ranking.cpp`), so when a large cohort joins,
every agent gains prior mass pulling it toward its opponents' mean and the
whole scale compresses toward the middle even if no real result changed.
Measured: the 72-ID residual/MLP study cohort compressed every top agent by
80-112 Elo between the 2026-07-12 and 2026-07-14 fits (oracle 1254 -> 1142,
then-champion family 1062-1135 -> 969-1029), on top of any real losses. Rules:
read ORDER and error bands within one fit, never absolute values across fits;
re-quote current numbers when citing strength in docs; and when the top of the
table must be resolved, boost the contender pairs' game counts (see
`ranking/roster_top.txt`, the reusable top-resolution roster) and refit rather
than trusting 8-games/pair separations.

## Pick the right metric first

| Metric | What it answers | When to use |
|---|---|---|
| us/move | How long does one move selection take end to end? | Comparing agents as players (includes search shape differences) |
| us/node (or cpu/node) | How expensive is one search node? | Comparing leaf/eval implementations where the search is held identical |
| total CPU per game | What does a whole game cost? | Ranking-system efficiency stats (`eff` column, pareto table) |

The key invariant for eval comparisons: **hold the search identical and verify
it**. If two variants visit the same nodes and pick the same moves, any us/move
difference is purely per-node cost, and us/move and us/node deltas agree. If
node counts differ, you are comparing search shapes, not implementations, and a
per-node claim is invalid. `speedBench`'s eval-level ladder enforces this with
an equivalence self-check (same end board + same node count across variants)
that prints PASS/FAIL before the numbers.

## The harness contract

Every timing comparison should satisfy all of these. `train.exe speed`'s
eval-level ladder implements them and is the template to copy.

1. **Fixed, seeded workload.** Build the position set once from a printed
   `--seed`, so a run is reproducible bit for bit. Never time on freshly
   random positions per variant.
2. **Fixed repetition count, not a wall-clock budget.** A "run until N ms
   elapsed" loop under-samples slow variants (fewer reps in the same budget)
   and gives variants different amounts of work. Fixed reps x positions gives
   every variant the identical workload.
3. **Warmup passes, discarded.** The first pass through a workload pays cold
   caches, page faults, and branch-predictor training. Run at least one full
   untimed pass first.
4. **Interleave variants.** Time variant A, B, C back to back on the same
   position, not all-A then all-B. CPU frequency scaling and thermal drift
   then hit all variants equally instead of penalizing whichever ran last.
5. **Monotonic clock.** `std::chrono::steady_clock` (QueryPerformanceCounter
   on MSVC, ~100 ns resolution). `high_resolution_clock` is not guaranteed
   monotonic on every toolchain.
6. **Report dispersion, not just a mean.** Keep per-rep samples and print
   mean, median, and min. A mean alone hides outliers (a background process
   stealing the core for one rep). Median is the robust central estimate; min
   approximates the noise-free cost.
7. **Functional equivalence check before trusting timings.** If the variants
   are supposed to compute the same thing, assert it in the harness (same
   chosen move, same node count) and refuse to compare on failure.
8. **Same build for all variants.** Compare code paths selected at runtime
   inside ONE binary wherever possible. Comparing across two builds adds
   compiler-version, flag, and layout noise. If two builds are unavoidable,
   record the exact compiler version and flags for both.

## Confounds checklist

Check each of these before believing a number:

- **Weight short-circuits.** `evalPosFull` and `evalPosLocal` skip the whole
  structure scan when wall and column weights are 0 (and the forward loop when
  forward is 0). A comparison meant to exercise the structure path must use
  nonzero weights, or it silently measures nothing. The same class of bug
  applies to any code with early-outs: confirm the timed path actually runs.
- **Different harnesses.** A number produced by harness X is not comparable to
  a number from harness Y (different sampling, workload, and accounting). The
  original wall/column incremental speedup (33-39% cpu/node) came from diffing
  two builds over whole ranking games; the chip-count study re-measured it
  inside `speedBench` specifically because cross-harness comparison is not
  trustworthy. When you need to compare against an old number, re-measure the
  old configuration in the current harness.
- **Whole-game vs fixed-position sampling.** Whole games weight the opening
  and endgame by how long the game lasts and let the variants diverge onto
  different positions. Fixed mid-game positions are controlled but less
  representative. Know which one your question needs.
- **Paying for unused state.** A reconstructed "old version" must not maintain
  accumulators it never reads (that overstates its cost), and must not skip
  work the old version really did. In `speedBench`'s ladder, levels 1-2 leave
  `g_evalIncremental` false so make/unmake does not maintain `g_evalPos`.
- **Global engine state.** The engine's board/eval state is global, so never
  time variants on multiple threads in one process. Processes are the sharding
  unit (see `run_tournament.ps1` / `run_rank.ps1`).
- **Parallel contention.** Wall time lies when other processes share the CPU.
  For long parallel runs use process-CPU accounting (`GetProcessTimes` deltas,
  already implemented in `ranking.cpp` per side per game) instead of wall
  clock, or run serially.

## Tools in this repo

- **`train.exe speed`** (`speedBench`, `src/ml_train.cpp`): the standing
  harness. Fixed seeded mid-game positions, per-move timing of whole
  `agentChooseMove` calls, us/move + nodes/move. Two sections:
  - the historical ms-budget table (learned model vs heuristic AB variants,
    including the zero-weight vs nonzero-weight presets), and
  - the eval-level ladder (`--reps`, `--warmup`): the rigorous fixed-rep,
    interleaved, self-checked comparison across `g_evalLevel` 1/2/3.
- **`g_evalLevel`** (`globals.h`): benchmark-only selector reconstructing the
  heuristic leaf's generations (1 = full chip rescan + full structure scan,
  2 = incremental chip + full structure scan, 3 = shipping incremental).
  Default 3; only `speedBench` changes it.
- **Ranking telemetry** (`rank.exe`): per-game per-side wall ms, process-CPU
  ms, node totals in `ranking/matches.jsonl`; `report.md` derives cpu/move and
  `eff`. Right tool for "is agent X cheaper per Elo", wrong tool for isolating
  a single code path.
- **Tournament telemetry** (`train.exe tournament-*`): streaming eff-depth /
  nodes / branching stats per agent.

## External libraries, if finer measurement is ever needed

The harness above times whole move selections (thousands of node visits per
sample), which averages out per-call noise and is the right granularity for
engine questions. If you ever need to time a single function in isolation
(nanosecond scale), use a microbenchmark library rather than a hand loop:

- **Google Benchmark**: auto-chooses rep counts, computes quantiles, and
  provides `benchmark::DoNotOptimize` / `ClobberMemory` to stop the optimizer
  deleting the measured code. Heavier dependency (CMake library).
- **nanobench** (martinus/nanobench): single header, vendorable next to
  `tests/catch.hpp`, same optimizer barriers, good default statistics.

Caveat: a microbenchmark times the function on a hot cache with perfect branch
prediction, which is NOT the cost inside a real search tree. Use them to
compare two implementations of the same small function, not to predict engine
throughput. For engine-level claims, `speedBench`'s realistic-search timing is
the more honest instrument.

## How to add a new comparison (worked example)

To compare the heuristic evaluator against a learned evaluator at equal depth:

1. Both are already expressible as `AgentSpec`s. In `speedBench`, add one
   `Bench` row per side, e.g. `agentMakeSearch("h", abIdx, classic, d, 0)` with
   nonzero weights vs `agentMakeSearch("lv", abIdx, learnedValueIndex(), d, 2)`
   (slot 2 = `models/pst_value.txt`).
2. Decide whether the searches are supposed to be identical. Different
   evaluators pick different moves, so the equivalence check does NOT apply;
   you are comparing us/move at matched depth, and should report nodes/move
   alongside so a node-count difference is visible in the result.
3. Follow the harness contract: fixed reps, warmup, interleave the two agents
   per position, report mean/median/min.
4. Record the run line (seed, reps, positions, depths), the build command, and
   the numbers in a results doc per the standard workflow.

## Reporting results

State in the results doc: the exact command line (with seed), the machine
context if unusual, the metric (us/move vs us/node), the dispersion, whether
the equivalence check applied and passed, and every confound from the
checklist you ruled out or accepted. A speedup percentage without its harness
description is the thing this guide exists to prevent.
