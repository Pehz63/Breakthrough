# Results: bounded per-position jitter (tie-only random jitter)

Companion to `bounded-jitter-plan-1-buzzing-floyd.md`. Work done 2026-07-12.

## Summary of changes

- **Bounded per-position jitter** shipped as the second form of the Advanced
  evaluator's Noise param, selected by sign: `n > 0` = the existing PST noise
  (unchanged), `n < 0` = jitter of magnitude `-n`, `n = 0` = off. No ID format
  change, no re-identification of any rostered agent (sign-as-mode follows the
  `dil(rP,dN)` threshold precedent; `n < 0` was previously expressible in raw
  IDs but semantically broken and never used, so `adv@1` is kept).
- **Tie-only by construction (the developer's design):** the leaf returns
  `realEval * 256 + jitter` with `jitter = (raw mod (2*mag+1)) - mag` in
  [-mag, +mag], `raw` = the sum of a per-(color,square) hash over occupied
  squares (`noiseHashRaw`, a different salt than the PST stream). Two jitters
  differ by < 256 while any strict real-eval difference scales to >= 256, so
  the jitter can NEVER reverse a strict preference -- it reorders exact
  evaluation ties only. Win sentinels bypass the scaling.
- **Incremental:** `raw` lives in its own accumulator `g_noiseAcc`
  (`g_mlAcc` pattern; 2-3 hash adds per make/unmake under `g_noiseIncremental`,
  latched seed `g_noiseSeed`), with a scan fallback (`noiseRawScan`) shared by
  the full path and the benchmark level-1 branch. Zero-cost when off.
- Registry: Noise range widened to -99..99. Hill climber untouched (its signed
  mode reaches `n < 0` natively; the jitter is ~strength-neutral by
  construction so it is not a fitness lever).
- Tests: 694 assertions in 67 cases, all passing. New: jitter boundedness /
  determinism / seed- and move-sensitivity; the all-terms incremental walk run
  in both noise forms (adds a direct `g_noiseAcc == noiseRawScan` check at
  every make/unmake); and the **dominance (order-preservation) walk** (the
  developer's test design -- see below).

## The dominance walk (the missing verification the developer asked for)

At every node of a bounded game-tree walk, all sibling children (the exact
comparisons alpha-beta makes) are evaluated under a plain config and a noisy
config, counting reversals: pairs the plain eval orders strictly one way and
the noisy eval orders the other. Results, now enforced by the suite:

- **Jitter (n < 0), mixed real config, magnitudes 2 and 9, seeds 1-3:
  reversals == 0** in every combination, while tie-breaks > 0 (the term does
  reorder exact ties). This mechanically verifies tie-only behavior; since it
  is also provable from the x256 construction, any future reversal is an
  implementation bug.
- **PST noise at chip=4, n3 (the exact config that measured -800 Elo
  yesterday): reversals > 0** -- the old form demonstrably overturns real
  material preferences at material scale. The docs' original "tie-breaker"
  framing for those numbers is now pinned as wrong in code, not just prose.
- **PST noise at chip=80 (n1 and n3): sibling reversals == 0** -- real sibling
  diffs are multiples of 80 while sibling noise diffs are bounded by ~5
  differing piece-squares x n <= 15, which is why yesterday's chip=80 ablation
  config was accidentally tie-like among sibling comparisons.

Notable test-driven catch: at n=1 the PST reversal demonstration is flaky (a
sibling reversal needs five specific squares at exact extreme hash values,
~1/243 per eligible pair -- 20 seeds of the crafted walk produced none), which
is itself a small worked example of why "no reversals observed" is weak
evidence without the construction-level argument. The test uses n=3.

## Correctness gotchas discovered

- **32-bit wrap in the accumulator hooks**: `g_noiseAcc += a - b` with 32-bit
  unsigned hashes computes `a - b` at 32 bits (wrapping when b > a) BEFORE
  widening to the 64-bit accumulator, silently adding 2^32. Caught immediately
  by the incremental walk's `g_noiseAcc == noiseRawScan` check (1291 mismatches
  on its first run). Fix: separate `+=` and `-=` statements so each hash
  widens before the arithmetic. The `g_mlAcc` hooks never had this problem
  only because they are floating point.

## Speed

`train.exe speed --positions 24 --ms 120 --seed 42 --maxdepth 4 --reps 2
--warmup 1`, ms-budget table (us/move, nodes/move):

| agent | d4 us/move | d4 nodes/move | us/node |
|---|---|---|---|
| adv-chip g0 (t1,c4) | 201.8 | 4246 | 0.0475 |
| adv-chip+jitter (t20,c80,n-2) | 376.6 | 6982 | 0.0539 |

The jitter's arithmetic is nearly free (+13% us/node: the accumulator hooks
plus one mod per leaf). The real cost is search shape: **+64% nodes/move at
d4**, because breaking exact ties reduces alpha-beta cutoff efficiency (equal
values let the search cut earlier and skip re-raising bounds). Hypothesis for
the mechanism (labeled as such): distinct leaf values produce more strict
alpha improvements and fewer immediate equal-bound cutoffs. Whatever the
mechanism, the node count is the measured fact, and it means "tie-only" is
not "cost-free": a jittered agent pays roughly the node overhead even though
its move QUALITY provably cannot degrade except at ties.

## Diversity checks (the idea's actual purpose)

`rank.exe pairgen`, 2 games each vs the classic champion, position streams
compared with agent-id fields masked:

- jitter seed 1 vs jitter seed 1 (repeat run): **byte-identical** (deterministic).
- jitter seed 1 vs plain (n0): **different games** (the jitter changes real play).
- jitter seed 1 vs jitter seed 2: **different games** (the seed is a diversity knob).

So one mix rostered under K noise seeds yields K deterministic, distinct
agents -- the "distribution of random-ish board states distinct from picking
uniformly among random legal moves" the original todo idea wanted.

## Strength retest on the proper instrument (theory 20)

Instrument per the developer's convention (now in `Docs/benchmarking.md`,
"Measuring strength"): main-roster gauntlets (61 active rated agents, frozen
pool), champion head `ab(d6,ord,nb200k)`, scale t20,c80, 4 games/opponent,
seeds 7 and 8, only `n`/`s` varying across configs. Three-way experiment:
deterministic first-found tie-breaking (baseline) vs persistent-bias PST noise
vs memoryless tie-only jitter.

| config | seed 7 | seed 8 | mean | vs baseline mean |
|---|---|---|---|---|
| baseline n0 | 1118 +/- 29 | 1148 +/- 30 | 1133 | -- |
| jitter n-1 | 1037 +/- 27 | 1175 +/- 31 | 1106 | -27 |
| jitter n-3 | 1037 +/- 27 | 1071 +/- 28 | 1054 | -79 |
| PST n1 | 905 +/- 25 | 869 +/- 25 | 887 | -246 |
| PST n3 | 898 +/- 25 | 913 +/- 25 | 906 | -227 |

Reading (against replicate spreads: baseline 30, jit-1 138, jit-3 34, pst-1
36, pst-3 15):

- **PST noise is confirmed harmful at chip=80 on the proper instrument:**
  ~-230 to -250 in all four runs, far outside every spread, dose-insensitive
  between n1 and n3. Note what this config IS: the dominance walk proves
  sibling material preferences cannot be reversed at this scale, so the PST
  form here acts through material-tie decisions (where its persistent
  square bias applies the same wrong-headed preference all game) and through
  its search-shape/budget cost (below). Yesterday's weak climb-pool numbers
  (-271/-309) replicated almost exactly.
- **The tie-only jitter is NOT free, but it is far cheaper:** means -27 (n-1)
  and -79 (n-3). The jit-1 pair straddles the baseline (one replicate at 1175
  beat both baseline runs; the other at 1037 sat well below), so at n-1 the
  data cannot distinguish "neutral" from "mildly costly"; n-3's pair is
  tighter and sits below both baseline runs. Given the proof that move
  quality cannot degrade except at exact ties, the candidate mechanisms are
  (a) random tie choices being genuinely worse than the plain agent's
  deterministic first-found choice, and (b) the measured search-shape cost:
  breaking ties reduces alpha-beta cutoffs (+64% nodes/move at d4), and under
  this head's 200k NODE BUDGET extra nodes buy less depth per move --
  an Elo cost fully compatible with tie-only move quality. The effective-
  depth probe below measures (b) directly.

### Effective-depth probe (mechanism (b) measured, not guessed)

One quick main-roster gauntlet per config (`--games 1`, seed 7), reading the
candidate's per-move effective-depth telemetry out of the scratch
`ranking/gauntlet.jsonl` (~1350 searches each):

| config | mean eff depth | vs baseline |
|---|---|---|
| baseline n0 | 5.526 | -- |
| jitter n-1 | 5.472 | -0.054 |
| jitter n-3 | 5.513 | -0.013 |
| PST n1 | 5.441 | -0.085 |

Under this head (`d6,ord,nb200k`) the budget-consumption effect is real but
TINY: at the E2 depth-Elo slope even the worst case (-0.085 ply) is worth
maybe 10-15 Elo. So mechanism (b) is measured and rejected as the main cost
driver for both forms. (The +64% nodes/move from the speed table was measured
at d4 with no move ordering and no budget; with `ord` and the budget the
search-shape effect largely washes out. Search-shape numbers do not transfer
across heads.)

### Conclusions

- **PST form: refuted, now with the right evidence.** Its ~-240 at chip=80
  cannot be blamed on depth loss (-0.085 ply) and cannot involve sibling
  material flips (dominance walk). What remains is its tie DECISIONS: a
  persistent per-square bias applies the same arbitrary preference at every
  tie all game, a compounding systematic error. This mechanism statement is
  consistent with all measurements but the tie-decision pathway itself was
  not isolated -- labeled a supported hypothesis, not a finding.
- **Jitter form: tie-only as proven, cheap but not provably free.** Mean -27
  (n-1) / -79 (n-3) against replicate spreads of 30-138, with one jitter
  replicate beating both baseline runs. Bounds: somewhere between ~0 and ~80
  Elo at this head, only ~10 of which is measured depth loss. Whether the
  remainder is real (random tie choices worse than the plain agent's
  deterministic first-found choices, filed as theory 22) or replicate noise
  needs more seeds.
- **As a diversity knob, the jitter does exactly what the todo idea wanted:**
  deterministic per seed, distinct across seeds, distinct from plain, provably
  sane at every non-tied decision, at a bounded and much smaller cost than the
  PST form.

## Future Work

- **Resolve the jitter's residual cost (theory 22)**: the -27/-79 means vs
  spreads of 30-138 need 4-6 seed replicates per config to separate "random
  ties cost ~30-80 Elo" from "noise". A sharper design: a third tie-break
  variant that, at ties, picks the SAME move the plain agent would (first-
  found) but still perturbs non-chosen orderings -- isolating tie-choice
  quality from every other effect.
- **Jitter node-overhead across heads**: +64% nodes/move at bare d4 vs a wash
  at d6/ord/nb200k (eff depth -0.013..-0.054) shows the search-shape effect is
  head-dependent; map it against `ord`/`tt`/budget combinations before using
  jittered agents in unbudgeted contexts.
- **Climber integration**: pin the jitter out of the climbed simplex (it is
  not a strength weight); optionally give the climber a `-Jitter` pass-through
  like `-RaceWin`.
- **Noise-seed ensembling** (from yesterday's Ideas): roster one strong mix
  under several jitter seeds as a cheap behavioral-diversity family; the
  diversity checks above confirm the mechanism works.

## Ideas This Inspired

- The dominance walk generalizes into a standing harness check: ANY eval
  change claiming "this only affects X" can often be phrased as an order-
  preservation invariant over sibling sets and pinned in code.
- Ties helping alpha-beta pruning suggests the reverse experiment: a
  tie-COLLAPSING transform (coarsen eval resolution, e.g. divide by k) might
  BUY search speed at bounded decision cost, the mirror image of the jitter.
- A per-move (rather than per-position) jitter seeded by the game ply would
  diversify openings specifically while leaving endgame play deterministic.
