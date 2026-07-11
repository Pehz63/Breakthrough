# Results: retrospective chip-count speedup measurement (implemented 2026-07-10)

Companion to the plan
[chip-count-speedup-plan-1-iterative-raven.md](chip-count-speedup-plan-1-iterative-raven.md).
Permanent record of what was done, what was measured, and how.

## Status: shipped and verified

The heuristic eval-level ladder is in `train.exe speed`, the equivalence
self-check passes, all 574 test assertions pass, and the engine's shipping
paths are byte-for-byte unchanged (`g_evalLevel` defaults to 3).

## Purpose (retrospective measurement, not a feature)

Commit `3af970d` (2026-06-04) replaced the full-board `chipDiff()` rescan in the
minimax leaf with the incremental `g_chipDiff` counter, before this project's
documentation and equivalence-test habits existed. Its speedup was never
measured. This session reconstructed that older leaf as a benchmark-selectable
level and measured it, and re-measured the later structure-caching step under
the same harness as a cross-check on the previously recorded 33-39% figure.

Level definitions (`g_evalLevel`, heuristic evaluators only):

- **v1** = full-board `chipDiff()` rescan + full `evalPosFull` scan per leaf
  (the pre-`3af970d` leaf)
- **v2** = incremental `g_chipDiff` + full `evalPosFull` scan per leaf
  (`3af970d` applied, structure caching not yet)
- **v3** = incremental `g_chipDiff` + cached `g_evalPos` (the shipping engine)

## What changed

- **Engine (benchmark scaffolding only).** `g_evalLevel` global
  ([globals.h](../src/globals.h), default 3). `evalBeginSearch` forces
  `g_evalIncremental` off below level 3 (so v1/v2 do not pay to maintain the
  accumulator they ignore); `evalLeaf`'s non-incremental branch uses
  `chipDiff()` instead of `g_chipDiff` at level 1
  ([ai_eval.cpp](../src/ai_eval.cpp)). The full-board `chipDiff()` needed no
  reconstruction: it still exists in
  [board_analysis.cpp](../src/board_analysis.cpp) (the plan assumed it was
  deleted; only its leaf call site was).
- **Harness (`speedBench`, [ml_train.cpp](../src/ml_train.cpp)).** New
  eval-level ladder section: fixed `--reps` x positions timed reps per level
  (default 8 x 24), `--warmup` discarded passes, levels interleaved per
  position, `steady_clock` timing, mean/median/min us/move + nodes/move, and
  an equivalence self-check (same end board + same node count across levels on
  every position) that prints PASS/FAIL. The pre-existing ms-budget section is
  unchanged apart from the monotonic clock.
- **Docs.** New [Docs/benchmarking.md](../Docs/benchmarking.md) (reusable
  measurement guide). CLAUDE.md files and Theory 16 updated.

## How it was measured

```
.\train.exe speed --positions 24 --ms 150 --maxdepth 6 --reps 8 --warmup 1 --seed 1
```

24 seeded mid-game positions (TieredRandom playouts from `boards/board1.txt`,
White to move), whole `agentChooseMove` calls timed, 192 samples per level per
depth. Levels interleaved per position; one warmup pass doubling as the
equivalence check. Node counts were identical across all three levels at every
depth (the searches are provably the same; only leaf cost differs), so the
us/move deltas are pure per-node eval cost. Self-check: **PASS**.

## Headline result 1: the chip-count optimization (v1 -> v2)

At nonzero structure weights (w2,l2), where the leaf also pays a full structure
scan, replacing the chip rescan with the counter cuts mean us/move by a stable
**-14 to -16% at depth >= 3** (larger at d1-d2 where root overhead is thin):

| agent | d3 | d4 | d5 | d6 |
|---|---|---|---|---|
| classic(t1,c4,w2,l2) | -16.6% | -16.1% | -16.2% | -15.8% |
| exp(t1,c4,w2,l2,f2) | -14.7% | -13.7% | -14.2% | -13.6% |

At the champion's actual weights (w0,l0), where `evalPosFull` short-circuits
and the leaf is nearly all chip term, the same optimization is worth far more.
This is the historically relevant configuration for `3af970d` (the champion
plays `classic(t1,c4,w0,l0)`):

| agent | d1 | d2 | d3 | d4 | d5 | d6 |
|---|---|---|---|---|---|---|
| classic(t1,c4,w0,l0) | -61.8% | -49.3% | -57.5% | -46.7% | -55.4% | -45.4% |

So the retrospective answer for `3af970d`: **roughly a 2x speedup (-45 to -62%
us/move) in the shipping zero-structure configuration, and -14 to -16% when a
full structure scan shares the leaf**. The odd/even depth alternation (odd
depths gain more) is consistent and unexplained; likely a leaf-parity effect on
which side's moves dominate the last ply.

## Surprise finding: v3 is SLOWER than v2 at zero structure weights

The w0,l0 row was planned as a sanity row (v2->v3 expected ~0 because the
structure scan short-circuits). Instead v3 came out **+18 to +35% slower** than
v2 (e.g. d6: 6,219 -> 7,321 mean us/move). Cause: with all positional weights
zero, both `evalPosFull` (v2's per-leaf call) and `evalPosLocal` early-out and
return 0, but v2 pays that early-out once per LEAF while v3 pays TWO
`evalPosLocal` calls per make/unmake on EVERY node (before + after, in both
simulate and unsimulate), maintaining an accumulator that is always zero. The
incremental structure machinery is pure overhead when there is no structure
term to maintain.

Actionable implication (not implemented this session, recorded in `todo.md`):
gate the incremental path on the active weights, e.g. `evalBeginSearch` leaves
`g_evalIncremental` false when wall == column == forward == 0. Eval values are
identical by construction (the existing equivalence test and this harness both
verify), and the champion-config engine would get the ~20-25% back. This also
retroactively qualifies the "incremental is strictly better" intuition: it is
strictly better only when the cached term is nonzero.

## Headline result 2: the structure-caching re-measure (v2 -> v3)

Same harness, same searches: caching the structure+forward scan in `g_evalPos`
cuts mean us/move by **-62 to -66% at depth >= 2**:

| agent | d3 | d4 | d5 | d6 |
|---|---|---|---|---|
| classic(t1,c4,w2,l2) | -63.2% | -64.8% | -61.9% | -63.5% |
| exp(t1,c4,w2,l2,f2) | -65.0% | -66.4% | -64.1% | -65.5% |

Combined v1 -> v3 at classic d6: 731,598 -> 224,758 mean us/move, **-69.3%**.

### Reconciling with the old 33-39% figure

No contradiction: the two numbers measure different steps. The recorded 33-39%
cpu/node ([incremental-wall-column-eval-results-1-golden-forest.md](incremental-wall-column-eval-results-1-golden-forest.md))
was the pt.2 refinement only, replacing `evalPosLocal`'s ~3x3 bounding-box
rescan with the true neighbor-local `neighborStruct` delta, i.e. an
already-incremental baseline made cheaper. This session's v2 baseline is older:
a FULL `evalPosFull` scan at every leaf with no accumulator at all. So the
-62 to -66% spans both pt.1 (full scan -> cached accumulator with bounding-box
delta) and pt.2 (bounding-box -> neighbor-local), and the old number nests
inside it consistently: if pt.2 alone was ~-39%, pt.1 alone was ~-40%
(0.60 x 0.61 = 0.37, matching the measured ~63% total reduction). Residual
differences in weights (t2,c10,w3,l2 then vs t1,c4,w2,l2 now), metric (cpu/node
over whole ranking games vs us/move over fixed positions), and harness account
for the rest. Both old and new numbers stand, now with their baselines stated.

## Raw data

Full output table (mean/median/min per level per depth, both runs):
`build/speed_ladder.txt` and `build/speed_ladder2.txt` (gitignored build
artifacts; regenerate with the command above, same seed reproduces the same
positions).

Reference points from the ms-budget section of the same run (zero-weight vs
nonzero-weight presets, quantifying the short-circuit confound): at d6,
`chip` (w0,l0) runs 7,519 us/move vs `chip+structures` (w2,l2) 169,282 us/move
on the always-incremental v3 path -- different searches though (nodes/move
172,608 vs 1,933,774), since eval values change move ordering and cutoffs, so
those two rows are NOT a per-node comparison, unlike the ladder.

## Correctness / methodology notes

- The equivalence self-check (same end board + node count across levels, every
  position, every depth) is the retrospective stand-in for the equivalence test
  `3af970d` never had. It passed on the first run.
- v1 is a faithful reconstruction of the leaf, but make/unmake still maintains
  `g_chipDiff`/`g_whiteCount`/`g_blackCount` (a few increments the pre-`3af970d`
  code did not do). v1 ignores `g_chipDiff` at the leaf but pays those
  increments, so the measured v1->v2 gain slightly UNDERSTATES the historical
  gain. The bias is a handful of add/compare ops per make/unmake against a
  64-square scan per leaf, negligible at these magnitudes.
- Mean > median at higher depths (right-skewed samples; a few positions have
  much larger trees). Percentages quoted are on means; median-based percentages
  agree within ~2 points.
- Measured at one position set (seed 1), one machine, MSVC 19.51 x64 `/EHsc`
  default optimization. us/move over whole move selections, not isolated eval
  calls.

## Future Work

- **The v1->v2 number at the original 2026-06 weights.** The champion-weights
  (w0,l0) row added in run 2 covers today's shipping configuration, but the
  actual default weights at `3af970d`'s commit date may have differed. Checking
  out that commit's defaults and rerunning the w-row would pin the historical
  number exactly.
- **TT/ordered search path.** The ladder runs the plain alpha-beta path. The
  strongest rostered agents use `tt,ord,nb200k`; leaf cost is a different share
  there (TT cutoffs skip leaves). Rerunning the ladder with those toggles would
  test whether the percentages transfer to the modern search.
- **Pt.1 vs pt.2 decomposition.** The reconciliation above infers pt.1 was
  ~-40% algebraically. A g_evalLevel 2.5 (cached accumulator with the old
  bounding-box delta) would measure it directly and close the theory 16
  evidence chain completely.
- **The odd/even depth alternation in the w0,l0 chip numbers** (-62/-49/-58/
  -47/-55/-45 for d1..d6). Consistent across both runs; a leaf-parity or
  branching-factor-parity effect is suspected but unverified. A run with
  depths 7-8 would show whether the alternation persists.
- **Confirm the zero-weight v3 overhead fix.** After implementing the
  weight-gated incremental path (see the surprise finding), rerun the ladder:
  v2->v3 at w0,l0 should go from +18..+35% to ~0%, and the champion's
  cpu/move in the ranking pool should drop measurably.

## Ideas This Inspired

- The eval-level selector pattern (reconstruct an older code generation behind
  a benchmark-only runtime toggle, verify same-search equivalence, then time)
  generalizes to any future "how much did that optimization buy" question, and
  is now documented in `Docs/benchmarking.md`.
- `speedBench`'s ms-budget section could migrate to the fixed-reps/interleaved
  harness too, making the learned-model rows dispersion-aware.
- A CI-style "performance regression" check: run the d3 ladder row in the test
  suite with a loose threshold, so an accidental de-incrementalization (e.g. a
  future evaluator forgetting the `incremental` flag) is caught by a number,
  not just by Elo drift.

## Candidate commit messages

1. `Add eval-level speed ladder + benchmarking guide; measure the 3af970d chip-count speedup retrospectively (-45..-62% at champion weights)`
2. `Measure chip-count (v1->v2) and structure-cache (v2->v3) speedups under one harness; add g_evalLevel ladder, Docs/benchmarking.md`
3. `Add retrospective eval-speed measurement: g_evalLevel ladder in train.exe speed, benchmarking guide, theory 16 numbers`

Top recommendation: (1), it names the deliverable (the retroactive number) and
the two additions (ladder + guide).
