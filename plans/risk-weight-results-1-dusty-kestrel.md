# Results: risk-adjusted LearnedValue scoring (mu + k*sigma)

Companion to `risk-weight-plan-1-dusty-kestrel.md`. Work done 2026-08-06.

## Summary of changes

- **`mlValueScoreRisk(turnColor, slot, riskTenths)`** shipped in
  `src/ml_eval.cpp`: scores `mu + (riskTenths/10)*sigma` for a `DistModel`
  slot when `riskTenths != 0`, otherwise byte-identical to the existing
  mu-only `mlValueScore`.
- **`LearnedValue` evaluator** gained a second param, `Risk` (tenths of a
  sigma multiple, range -50..50, default 0), wired to the new function. A
  nonzero Risk forces the full-scan leaf path (the incremental accumulator
  never tracked sigma).
- **ID grammar**: `learned(...)` accepts an optional trailing `risk=<tenths>`,
  omitted at its default of 0 -- no existing agent's canonical id changed.
- Tests: 3109 assertions in 130 test cases, all passing, both before and
  after the mid-session tenths-of-sigma unit rescale (new tests: 2 in
  `test_ml.cpp`, 1 in `test_ranking.cpp`).
- Docs updated: `src/CLAUDE.md` (ai_eval.cpp/ml_eval.cpp/ranking.cpp entries),
  `ML.md` (search-integration paragraph + this study's results table),
  `Docs/terminology.md` (the dist-model entry's "search reads only mu" claim,
  now conditional), `Docs/theories.md` (theory 47).

## How to test

```powershell
.\tools\run_tests.ps1 -Build      # 3109 assertions, 130 cases
.\rank.exe check                  # real roster still canonical (170 agents)
.\rank.exe gauntlet --id "ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1,risk=5)@1" --games 4
```

The gauntlet command plays a `k=0.5` "optimistic" variant of the s76 core
against the full rated pool from scratch (writes only to the scratch
`ranking/gauntlet.jsonl`) -- a quick way to see the mechanism produce a
different, real result without touching any stored state.

## Pass 1: sanity (does the plumbing work)

Before any screening compute, the s76 core was gauntletted (4 games/opponent,
full ~170-agent pool, pre-rescale whole-integer units) at three settings:

| Risk | Elo (gauntlet, pool held fixed) |
|---|---|
| omitted (mu-only) | 907 +/- 15 |
| +2 ("optimistic") | 754 +/- 15 |
| -2 ("cautious") | 707 +/- 15 |

2,040 real games, zero errors. The knob demonstrably changes real play, not
just unit-test-level scores: the first game of the baseline run (vs
`ab(deep=1).classic(chip=100)`) ran 23 plies; the same opening matchup at
`risk=2` ran 43. This confirmed the mechanism was sound enough to spend
Pass-2 compute on, and also surfaced the first real signal -- both directions
already looked substantially worse than baseline at `|k|=2.0`, which directly
motivated rescaling to tenths so Pass 2 could probe smaller magnitudes rather
than only confirming the already-visible drop at coarser steps.

## Pass 2: screening grid

s76 core only, k in {-1.0, -0.5, -0.2, -0.1, 0.1, 0.2, 0.5, 1.0} (tenths
resolution), 8 games/pair, played into `ranking/matches_risk_study.jsonl`
(its own store, per the project's screening-cohort convention) against the
full active roster (170 agents) plus the 8 new agents against each other,
then rated with `rank.exe rate --pin ranking/standings.tsv` (pinned to the
real scale, canonical files untouched).

| k | -1.0 | -0.5 | -0.2 | -0.1 | **0 (baseline)** | 0.1 | 0.2 | 0.5 | 1.0 |
|---|---|---|---|---|---|---|---|---|---|
| Elo | 789 | 871 | 928 | 941 | **1007** | 910 | 865 | 818 | 779 |
| SE | 14 | 14 | 15 | 15 | 0 (pinned) | 15 | 14 | 14 | 14 |
| Games | 720 | 720 | 720 | 720 | 16 (pinned) | 720 | 720 | 720 | 720 |

(720 games/new-agent, not the naively-expected ~1400, because a large
fraction of the 170-agent pool is itself deterministic and
deterministic-vs-deterministic pairs are pinned at 2 games rather than 8,
per `pairGameTarget` -- see `src/CLAUDE.md`'s ranking.cpp entry. This is the
project's existing, deliberate independence mechanism, not a shortfall.)

**Every nonzero k tested rated below the mu-only baseline**, by 66 to 228
Elo, monotonically worse with `|k|` in both directions, comfortably outside
the +/-14-15 SE at every step. Negative k (cautious, mu-k*sigma) beat
positive k (optimistic, mu+k*sigma) of the same magnitude at every point
except the largest, where the two nearly converge (789 vs 779, within 1
combined SE). Even the smallest step tested, `|k|=0.1`, already cost 66-97
Elo.

**Verdict: theory 47 (`Docs/theories.md`) is refuted for this core.**
Blending sigma into the search-time leaf score, in either direction, made the
s76 linear position_elo model play measurably worse, and the damage was
already present at the finest resolution tested. Nothing here should be
promoted or certified (the certification gate requires beating the existing
baseline, which none of these did).

## Implementation gotchas

- **`evalBeginSearch` had to be told, not just the accessor.** The obvious
  first change (add a Risk param, dispatch through a new function) is
  insufficient on its own: `evalBeginSearch` unconditionally called
  `mlIncrementalBegin(params[0])` for any `LearnedValue` evaluator, which
  seeds the mean-only accumulator and makes `evalLeaf` take the fast
  `g_mlIncremental` branch *before* it ever looks at Risk. Without an
  explicit `mlIncrementalEnd()` in the `params[1] != 0` branch, a nonzero
  Risk would have been silently ignored during search (correct only in the
  non-incremental accessor, wrong in the actual game-playing path). Caught by
  writing the `evalBeginSearch`/`evalLeaf` integration test, not the
  standalone `mlValueScoreRisk` unit test.
- **`learned()`'s ID codec has no generic weight loop.** Every other
  evaluator (Classic/Experimental/Advanced) gets its weights encoded by a
  shared loop over `EvalDef.params`; `LearnedValue` is special-cased entirely
  (`ev->letters[0] == '\0'`) because the model slot needs a content hash, not
  a plain number. Adding Risk therefore meant extending that special-cased
  branch by hand on both the emit and parse sides, rather than the register-a-
  param-and-done path a normal evaluator weight would get.
- **Whole integers bottom out at 1.** The initial Risk design used plain
  integer k (range -5..5, matching every other evaluator weight's
  convention). The developer's question -- "why not smaller values if 2
  already looks too large" -- exposed that this design has no way to express
  anything between 0 and 1.0; there is no such thing as "a smaller whole
  number" once you're at the smallest nonzero one. Fixed by rescaling to
  tenths before any cohort was played, since nothing was committed yet.
- **First correction proposal was itself inconsistent.** The first
  tenths-based k-grid offered (`0.25, 0.5, 0.75, 1.0`) mixed in values that
  need hundredths (0.25 = 2.5 tenths, not representable as an integer tenths
  count). Caught by the developer before anything was built against it.

## Future Work

- **Does this generalize past s76?** The other 9 rostered `position_elo`
  cores (s77/78/79 original mlp, s110-115 the NNUE-shaped mlp family) were
  explicitly deferred to keep this first pass narrow. s76's sigma head is a
  plain linear model over the same 129 features as its mu head; an mlp sigma
  head might respond differently. Untested.
- **Is the small-k damage a measurement floor or real?** k=0.1 already cost
  66-97 Elo -- the grid never got small enough to see a magnitude where
  blending sigma stops hurting. A run at k=0.01-0.05 (needs a further units
  rescale, since 0.1 is currently the finest step) would tell whether the
  relationship keeps degrading smoothly toward k=0 or whether there's a
  genuinely flat/neutral region below 0.1 that this grid's resolution missed.
- **Mechanism is unprobed.** Theory 47 confirms the effect, not why: whether
  it's specifically because s76's sigma correlates with genuinely sharp
  positions where a deeper/more careful search would matter more than a
  static score nudge, or something else, was not investigated. Theory 35
  (`Docs/theories.md`) already found this dist family's sigma-vs-measured-
  volatility correlation weak (0.02-0.29 across configs); a noisy sigma
  signal being amplified into the leaf score is a plausible contributing
  mechanism but was not tested here.
- **The sampling variant (`todo.md`'s option (a)) remains open**: drawing a
  value from N(mu,sigma) at each leaf, a genuinely new stochasticity source
  distinct from this deterministic transform. Not attempted this session.

## Ideas This Inspired

- If a future study wants both a genuinely small-k probe AND the full
  10-model breadth, consider redesigning Risk's units as a percent-of-sigma
  (range e.g. -500..500 in units of 0.01) rather than iterating the
  tenths-then-hundredths pattern this session hit twice.
- The full determinism-based game-count structure of `pairGameTarget`
  (deterministic pairs pinned at 2 games, others at the full request) made
  this screening's realized game count meaningfully different from a naive
  games/pair x pairs estimate; a `rank.exe check` preview that reported the
  post-determinism-pinning game count (not just pair count) would have made
  the pre-run scale statement in this session more accurate.
- Given how cleanly negative k beat positive k at every magnitude but the
  extreme, it might be worth checking whether this is specific to being
  White-centric mu with a White-favorable board convention, or whether the
  same asymmetry shows up when the position is examined from Black's
  perspective -- i.e. is "cautious beats optimistic" a property of the risk
  transform, or an artifact of some other asymmetry in how mu is signed.

## Commit

```
Add LearnedValue Risk weight (mu + k*sigma), screen s76 core: refuted (theory 47)
```
