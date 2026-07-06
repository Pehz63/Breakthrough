# Make wall/column eval truly incremental, bump version, re-rank

## Context

In the minimax hot path the positional eval is kept in the accumulator `g_evalPos`
instead of rescanning the board at each leaf. Today that accumulator is only
*partially* incremental:

- **Chip diff** (`g_chipDiff`) is a true O(1) scalar delta: `+/-1` at the capture
  site, reversed on unmake ([moves.cpp:279-281](../src/moves.cpp#L279-L281)).
- **Forward** is already a true 2-square delta
  ([ai_eval.cpp:120-121](../src/ai_eval.cpp#L120-L121)) -- only the source and
  destination squares are recomputed.
- **Wall + column** are NOT: `evalPosLocal` re-sums `structOwner` over a ~3x3
  bounding box, and `simulateMove/unsimulateMove` call it once *before* and once
  *after* each mutation and subtract ([ai_eval.cpp:110-118](../src/ai_eval.cpp#L110-L118),
  [moves.cpp:277-288](../src/moves.cpp#L277-L288)). That is ~18 `pairContrib` calls
  x2 per make and again per unmake.

The goal: make wall/column follow the chip-diff model -- a tight local delta over
only the adjacencies that actually changed -- then bump the evaluator's code
version so the affected agents get fresh identities, replay the ranking, and
report whether they run faster. The change must produce **identical eval values**
(pure performance refactor), so game outcomes and Elo are unchanged; only
cpu-per-node moves.

## Part 1 -- Make wall/column a true neighbor-local delta

**File: [src/ai_eval.cpp](../src/ai_eval.cpp)** -- rewrite only the structure branch of
`evalPosLocal` (lines 110-118). Keep the existing before/after subtraction
framework in `moves.cpp` untouched (it is what the equivalence test validates).

Replace the bounding-box loop with a sum of `pairContrib` over the 4 orthogonal
neighbors of each of the two changed squares `(sx,sy)` and `(dx,dy)`. Add a small
bounds-guarded helper, e.g.:

```cpp
// Sum of the (up to 4) orthogonal same-color pairs touching square (x,y).
static inline int neighborStruct(int x, int y, int wallW, int colW) {
    int s = 0;
    if (x+1 < SIZE) s += pairContrib(x, y, x+1, y, wallW, colW);
    if (x-1 >= 0)   s += pairContrib(x, y, x-1, y, wallW, colW);
    if (y+1 < SIZE) s += pairContrib(x, y, x, y+1, wallW, colW);
    if (y-1 >= 0)   s += pairContrib(x, y, x, y-1, wallW, colW);
    return s;
}
```

Then the structure branch becomes
`s += neighborStruct(sx, sy, wallW, colW) + neighborStruct(dx, dy, wallW, colW);`
(forward branch unchanged).

**Why this is correct under the before/after diff** (the equivalence test asserts
it, but the reasoning):
- Diagonal moves: source and dest are not orthogonally adjacent, so the two
  neighbor sums share no pair -> every changed pair counted once.
- Straight moves: source and dest *are* orthogonally adjacent, so the
  source-dest pair is counted twice; but straight moves only land on an empty
  square (dest empty before, source empty after), so that pair is 0 in both the
  before and after passes -> the double count contributes 0. Harmless.
- Every other pair (each square with its other neighbors) is counted once in
  before and once in after; the subtraction yields exactly the move's delta.

Cost drops from ~18 `pairContrib`/pass (9 owners x2) to 8/pass (2 squares x4),
roughly halving the structure work in make/unmake. Leave `evalPosFull`,
`structOwner`, `evalLeaf`, and the `moves.cpp` call sites unchanged.

## Part 2 -- Bump the evaluator code version

**File: [src/ranking.cpp:163-166](../src/ranking.cpp#L163-L166)** (`g_rkEvals`). Both
`classic` and `exp` share the changed structure code, so bump both from `1` to `2`:

```cpp
{ "Classic",      "classic", "tcwl",  2 },   // was 1
{ "Experimental", "exp",     "tcwlf", 2 },   // was 1
{ "LearnedValue", "learned", "",      1 },   // unchanged
```

Every classic/exp agent's canonical ID becomes `...@2`; `rank.exe check` prints
the exact fix for each stale roster line. Old `@1` identities and their games stay
in the store forever (retired) and remain in the report as the before baseline.

## Part 3 -- Update the roster (full control run)

**File: [ranking/roster.txt](../ranking/roster.txt)** -- rewrite every `.classic(...)@1`
and `.exp(...)@1` segment to `@2` (the pure-random heads `rand/tiered/smart` carry
no eval segment and stay `@1`). Keep **all** agents `on` (full control run): the
8 non-zero-weight agents (lines 25, 35, 36-39, 90-91: wall `w`, column `l`, or
forward `f`) exercise the changed path, while the ~30 `w0,l0` agents are a control
whose cpu/node should stay flat, sharpening the speedup claim. `rank.exe check`
must pass with zero canonical errors before running.

## Part 4 -- Replay the ranking (serial, elevated game count)

Serial gives clean per-move timing (`par=1`); an elevated `--games` averages the
small structure-code speedup above scheduler/CPU noise:

```
.\tools\run_rank.ps1 -Build check          # rebuild rank.exe, confirm roster is canonical
.\tools\run_rank.ps1 run --games 12        # serial play (all @2 pairs are fresh) then rate
```

Runtime caveat: because every classic/exp agent is a new `@2` identity, nearly the
whole ~40-agent matrix is replayed fresh -> on the order of thousands of games.
This is a long serial run (likely hours). If wall-clock becomes a problem, the
play phase can be sharded with `-Workers K` (cpu times stay contention-honest;
per-move ms gets noisier) and then rated once. Dial `--games` down (e.g. 8) to
shorten it. Output lands in `ranking/ratings.tsv`, `ranking/games.tsv`, and
`ranking/report.md`.

## Part 5 -- Report the speed result

From `ranking/report.md` (and `games.tsv`), for each of the 8 affected agents
compare **new `@2` vs retired `@1`** using **cpu per node** = (cpu/move) /
(nodes/move), which is robust to the two runs facing different opponent pools
(node counts per move are identical for identical positions, but positions
differ; cpu/node normalizes that). Confirm the control (`w0,l0`) agents show
essentially unchanged cpu/node, and report the affected agents' delta. State
plainly whether they got faster and by how much, and if the delta is within noise,
say so.

## Files to modify

- [src/ai_eval.cpp](../src/ai_eval.cpp) -- `evalPosLocal` structure branch + new
  `neighborStruct` helper (the only engine change).
- [src/ranking.cpp](../src/ranking.cpp) -- bump `classic`/`exp` versions in `g_rkEvals`.
- [ranking/roster.txt](../ranking/roster.txt) -- `@1` -> `@2` on all classic/exp lines.
- Generated by the run: `ranking/ratings.tsv`, `ranking/games.tsv`,
  `ranking/report.md`, `ranking/matches.jsonl` (appended).
- Docs (per CLAUDE.md standing instructions): update `README.md` and `CLAUDE.md`
  where they describe the incremental eval (note wall/column now a true
  neighbor-local delta, not a bounding-box rescan) and the eval `@N` version.

## Verification

1. Build the test suite: `.\tools\run_tests.ps1 -Build`. The make/unmake
   equivalence walk in [tests/test_eval.cpp](../tests/test_eval.cpp) asserts
   `g_evalPos` equals a full `evalPosFull` recompute at every node -- this is the
   correctness gate proving the new delta is exact. All existing assertions must
   still pass (ranking tests will also exercise the bumped `@2` codec round-trip;
   update any test that pins the old `@1` string if present).
2. Build the engine with the CLAUDE.md `cl` command; confirm no new warnings.
3. `.\tools\run_rank.ps1 -Build check` -- roster canonical, no errors.
4. Run a short `run --games 2` first as a smoke test (fast), confirm it plays and
   rates, then launch the full `--games 12` run.
5. Read `ranking/report.md`: verify Elo is unchanged vs the retired `@1` agents
   (identical eval -> identical games), and read off the cpu/node before/after for
   the speed answer.

---

*Results of executing this plan are recorded in the companion document
[incremental-wall-column-eval-results-1-golden-forest.md](incremental-wall-column-eval-results-1-golden-forest.md).*
