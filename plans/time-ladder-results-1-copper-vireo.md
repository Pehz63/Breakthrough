# Results: the wall-clock ladder, and which track actually normalizes compute

Companion to `plans/time-ladder-plan-1-copper-vireo.md`. Run 2026-09-06 on the
post-bump binary. 40 cells, head `ab(deep=12,tt,ord[,retain],time=Xms)@3`,
**2,136 games per cell, all 40 identical**, one pinned fit against
`ranking/standings.tsv`. 79,200 games over three ladder rungs (19,800 / 19,800 /
39,600), merged with zero torn lines.

## How every contrast below was computed

Stated once so no table has to restate it, and because the earlier draft of this
document leaned on eyeballed per-cell differences.

- **Every contrast is a difference of two Elo estimates inside ONE fit.** It is
  comparable across cells of that fit and never across fits, so no number here is
  put beside a number from the node study's fit except as a difference.
- **Cells are combined by inverse-variance weighting**, weight `1/SE^2`, which is
  the minimum-variance unbiased combination under independence. Each cell's SE is
  the quadrature sum of the two agents' Fisher standard errors.
- **The pooled SE is a lower bound and the pooled z an upper bound on |z|.** Cells
  of one fit share a pinned pool, so they are not fully independent. Stated rather
  than ignored, because it is the one assumption the arithmetic cannot check.
- **Q and I^2 are reported with every pooled mean.** A large I^2 says the cores
  disagree and the pooled mean is averaging genuinely different per-core effects,
  which makes it a summary and not a prediction for any one core.
- The script is `analysis/pool_retain_effect.py`, run against
  `ranking/standings_pinned.tsv`.

## The headline

**The wall-clock track normalizes compute and the node track does not.** That was
the original question and it has a clean answer. Cross-core spread in realized
ms/move at one budget setting:

| head | ms range across cores | spread |
|---|---|---|
| `time=25ms`, plain | 9 - 14 | **1.6x** |
| `time=50ms`, plain | 20 - 21 | **1.1x** |
| `time=100ms`, plain | 38 - 47 | **1.2x** |
| `time=200ms`, plain | 78 - 104 | **1.3x** |
| `time=400ms`, plain | 147 - 199 | **1.4x** |
| `time=Xms`, `retain`, every rung | - | **1.1x** |
| `nodes=200k`, `deep=6` (the node track) | 11 - 400 | 36.4x |
| `nodes=300k`, `deep=12,rem=70` | 38 - 684 | 18.0x |

So ms IS comparable across cores on the time track. Calling the node track a
compute-normalization track is wrong, and `ranking/CHAMPION.md` should say what
it actually measures: strength per node, which isolates evaluator quality from
evaluator speed. Both questions are legitimate. Only one of them is about compute.

## Levels: Elo and realized ms

| core | condition | 25ms | 50ms | 100ms | 200ms | 400ms |
|---|---|---|---|---|---|---|
| chip counter | plain | 980+-9 (10ms) | 1003+-9 (20ms) | 1029+-9 (46ms) | 1037+-9 (88ms) | 1062+-9 (163ms) |
| chip counter | `retain` | 1012+-9 (21ms) | 1033+-9 (41ms) | 1043+-9 (83ms) | 1053+-9 (165ms) | 1065+-9 (328ms) |
| tdleaf_self lin `model=169` | plain | 1106+-9 (11ms) | 1197+-10 (21ms) | 1194+-10 (38ms) | 1326+-13 (78ms) | 1318+-13 (165ms) |
| tdleaf_self lin `model=169` | `retain` | 1182+-10 (21ms) | 1267+-12 (42ms) | 1299+-12 (83ms) | 1361+-14 (166ms) | 1356+-13 (327ms) |
| position_elo mlp `model=113` | plain | 601+-10 (9ms) | 694+-9 (20ms) | 794+-9 (47ms) | 819+-9 (92ms) | 896+-9 (147ms) |
| position_elo mlp `model=113` | `retain` | 726+-9 (23ms) | 784+-9 (45ms) | 827+-9 (87ms) | 897+-9 (172ms) | 938+-9 (343ms) |
| pool_games lin `model=97` | plain | 635+-9 (14ms) | 681+-9 (21ms) | 694+-9 (43ms) | 711+-9 (104ms) | 755+-9 (199ms) |
| pool_games lin `model=97` | `retain` | 678+-9 (22ms) | 678+-9 (44ms) | 699+-9 (88ms) | 744+-9 (177ms) | 753+-9 (345ms) |

## `retain` makes the flag mean what it says

Realized ms as a fraction of the flag:

| core | plain | `retain` |
|---|---|---|
| chip counter | 0.40 - 0.46 | 0.82 - 0.83 |
| tdleaf_self lin `model=169` | 0.38 - 0.46 | 0.82 - 0.85 |
| position_elo mlp `model=113` | 0.34 - 0.47 | 0.86 - 0.90 |
| pool_games lin `model=97` | 0.41 - 0.56 | 0.86 - 0.89 |

Pooled over all 20 cells, the fraction of the flag actually spent is **0.432
(sd 0.053) plain** and **0.854 (sd 0.028) with `retain`**. It nearly doubles the
realization and halves its spread, so the budget becomes both larger and more
predictable. Both halves matter: a track whose agents spend a consistent 0.43 of
the flag would at least be rescalable, and this one is not, because the fraction
also varies by core.

A plain `time=200ms` agent spends about 80 ms. That is not a rounding error, it
is more than half the budget, and it is `nextIterationFits` declining the last
iteration and then leaving the remainder on the floor. With `retain` the same
agent spends 165 ms. The flag becomes an honest description of the spend, and the
cross-core spread tightens to 1.1x at every rung.

## Pooled Elo effect of `retain`, at the same flag

Per-core differences are in the levels table above. Pooled across the four cores
at each rung:

| flag | pooled `retain` - plain | SE | z | Q (df=3) | I^2 |
|---|---|---|---|---|---|
| 25ms | **+67.3** | 6.5 | 10.29 | 30.1 | 90% |
| 50ms | **+44.6** | 6.6 | 6.71 | 30.7 | 90% |
| 100ms | **+33.2** | 6.6 | 5.00 | 28.3 | 89% |
| 200ms | **+41.4** | 6.9 | 6.03 | 12.8 | 77% |
| 400ms | **+17.6** | 6.8 | 2.58 | 8.6 | 65% |

Every rung is positive and every rung clears its own SE, the effect is largest at
the short rungs, and it declines as the budget grows. That is the same shape the
node track showed and it is the shape theory 70 states.

**The I^2 column is not decoration.** At 90% the cores are not measuring one
effect: at 25ms the per-core spread runs from +125 (`position_elo mlp model=113`)
to +32 (`chip counter`). The pooled +67.3 is a summary of four different numbers,
not a prediction for a fifth core.

## The honest caveat: CPU-matched, time-`retain` is a wash

This is the opposite of the node-track finding and must not be conflated with it.
On the node track `retain` was CPU-neutral against the default (0.85x-1.01x) and
bought +137 Elo. Here it roughly doubles the spend, so the fair comparison is
`retain` at flag X against plain at flag 2X, which land within a few ms of each
other:

| core | r@25 vs p@50 | r@50 vs p@100 | r@100 vs p@200 | r@200 vs p@400 |
|---|---|---|---|---|
| chip counter | +9+-13 | +4+-13 | +6+-13 | -9+-13 |
| tdleaf_self lin `model=169` | -15+-14 | +73+-16 | -27+-18 | +43+-19 |
| position_elo mlp `model=113` | +32+-13 | -10+-13 | +8+-13 | +1+-13 |
| pool_games lin `model=97` | -3+-13 | -16+-13 | -12+-13 | -11+-13 |

Eleven of sixteen cells are within 1.5 SE of zero and the signs alternate.
Pooled:

| pair | pooled | SE | z | Q (df=3) | I^2 |
|---|---|---|---|---|---|
| `retain`@25 vs plain@50 | +6.8 | 6.5 | 1.04 | 6.9 | 57% |
| `retain`@50 vs plain@100 | +7.2 | 6.6 | 1.09 | 23.0 | 87% |
| `retain`@100 vs plain@200 | -3.4 | 6.8 | -0.50 | 3.6 | 16% |
| `retain`@200 vs plain@400 | +0.0 | 6.9 | 0.00 | 6.3 | 53% |
| **all 16 cells** | **+2.8** | **3.3** | **0.84** | 41.6 | 64% |

**At matched wall clock, time-`retain` neither helps nor hurts.** Its raw per-rung
gains (+32 to +125 at the short rungs) are bought by spending the roughly 2x CPU
that plain leaves unspent. The overall +2.8 +-3.3 does not exclude a real effect
of up to about 9 Elo in either direction, so this is "no effect detected at this
sample size", not "effect proven absent".

**The practical consequence, which is easy to get backwards:** replacing a plain
flag with a `retain` flag at equal compute means roughly HALVING the flag. Plain
`time=400ms` realizes 147-199 ms across the four cores and `retain` `time=200ms`
realizes 165-177 ms, and the two rate the same (+0.0 +-6.9).

That is still a reason to want it in a track definition, but a different reason
than on the node side. It is not free strength. It makes `time=X` mean X, which
is a property an instrument should have.

## The knee: not in range for half the cores

The criterion was the cheapest flag past which more time stops buying strength.
Taking "stops buying" as a doubling that gains less than its own SE:

| core | condition | total Elo, 25 -> 400ms | knee |
|---|---|---|---|
| chip counter | plain | +82 | 100ms |
| chip counter | `retain` | +53 | **50ms** |
| tdleaf_self lin `model=169` | plain | +212 | 50ms (but +132 at 100->200 after) |
| tdleaf_self lin `model=169` | `retain` | +174 | **200ms** |
| position_elo mlp `model=113` | plain | +295 | **none in range** |
| position_elo mlp `model=113` | `retain` | +212 | **none in range** |
| pool_games lin `model=97` | plain | +120 | none in range |
| pool_games lin `model=97` | `retain` | +75 | 25ms (but +45 at 100->200 after) |

**`position_elo mlp model=113` is still climbing hard at the top rung**, gaining
+77 (plain) and +41 (`retain`) in the last doubling. This was predicted before
the run from its 252-1431 ms/move cost in the node grid, and it happened, so it
is reported as no knee rather than fitted into one.

The two plain rows marked with a parenthetical are why the rule needs the
"and it stays flat" clause the ladder plan wrote down: a single quiet doubling
followed by a large one is noise, not a plateau. Under `retain`, which halves the
noise by making the spend consistent, only the genuine plateaus survive.

**What can be said:** with `retain` on, the chip counter is done by 50ms and
`tdleaf_self lin model=169` by 200ms. The two weaker cores are not done at 400ms.
A track bound has to serve every core in it, so nothing in 25-400ms is defensible
as "the" bound yet.

**The best candidate in range is `time=200ms` with `retain`.** Realized spend is
165-177 ms across all four cores (1.1x spread), two of the four have plateaued,
and it is the cheapest rung where that is true. It is a candidate, not a
conclusion, because the other two cores have not plateaued.

## Efficiency, for contrast

Elo per 100 ms of realized CPU falls monotonically with the budget on every core
and both conditions, from about 9,700 at 25ms to about 800 at 400ms on
`tdleaf_self lin model=169`. That is the expected shape and is listed only to
make the point that "most Elo per ms" and "reasonable bound" are different
questions: the first is always answered by the smallest budget on the ladder.

## How to test

```
python tools/make_time_ladder_roster.py --abver 3
rank.exe check --roster ranking/q7/roster_timeladder.txt
tools/run_rank.ps1 -Workers 10 -NoRate -PinEachRung ranking/standings.tsv \
    play --roster ranking/q7/roster_timeladder.txt \
         --cohort ranking/q7/cohort_timeladder.txt --games 8
rank.exe rate --roster ranking/q7/roster_timeladder.txt --pin ranking/standings.tsv
```
Expect three ladder rungs, a pinned fit after each, and 2,136 games per cell.

## The track this was meant to inform is not currently a wall-clock track either

Measured while writing up the decision below, on the same pinned fit
(`ranking/standings_pinned.tsv`), over the 45 rostered `time=150ms` agents at 320
games each:

| head | rows | mean realized ms/move | range |
|---|---|---|---|
| `ab(deep=6,tt,ord,time=150ms)@3` | 43 | **20.5** | 7.5 - 75.9 |
| `ab(deep=12,tt,ord,time=150ms)@3` | 2 | **61.5** | 57.2 - 65.7 |

`ranking/CHAMPION.md`'s wall-clock track is defined at `time=150ms`, and almost
all of its agents sit on a `deep=6` head. At depth 6 the DEPTH CAP binds long
before the clock does, so those agents spend 20 ms of a 150 ms allowance and the
budget is close to decorative. The two `deep=12` rows are the ones where the clock
actually binds, and there plain leaves about 60% of it unspent, consistent with
the 0.432 realization measured across this whole study.

This is a second, independent reason the track needs re-specifying, and it is not
the same problem as `retain` solves. `retain` recycles budget the clock gate
declined to spend. Nothing recycles budget a depth cap never asked for. Both have
to change together, and both mint new agent identities, so it is a
re-certification of the track rather than an edit to it. Not done here.

## Decision: the plain condition is retired

Developer instruction, 2026-09-06, after reading the results above: the study
carries `retain` only from here. The reasoning, stated so a later reader can
disagree with it on the record:

- On the **node** track `retain` is free strength. `rem=70,retain` costs 0.85x to
  1.01x the `rem=0` default's ms/move and pools to +52.5 +-7.2 at 100k.
- On the **time** track it is not free strength, but it is what makes the flag
  describe the spend. An instrument whose setting predicts 0.43 of its own reading,
  with the fraction varying by subject, is not measuring what its axis is labelled.
- Carrying plain doubles every cell of every future ladder to keep re-measuring a
  condition already characterised at 2,136 games per cell.

What this changes concretely:

- `tools/make_time_ladder_roster.py` generates `retain` cells only, and its rungs
  extend to 800 and 1600ms.
- The 20 plain cells of the first pass are listed `off` in
  `ranking/q7/roster_timeladder.txt`, not deleted. Their 42,720 games stay on
  record and can be re-rated, but no fit places them beside the `retain` cells as
  if the comparison were still open.
- `ranking/CHAMPION.md`'s wall-clock track needs its head re-specified on a
  `retain` head, which re-certifies every category in that track. That is a
  separate deliberate act and is NOT done by this document.

## Future Work

- **Extend the ladder to 800 and 1600ms.** LAUNCHED 2026-09-06 as a retain-only
  pass, 28 cells. Without those rungs the study's own criterion is unmet for
  `position_elo mlp model=113` and `pool_games lin model=97` and no bound can be
  defended. The existing `retain` cells stay and the scheduler plays only the
  deficit, so the added cost is the 8 new cells plus the pairs that reach them.
- **Re-run the CPU-matched contrast at more rungs.** The r@X vs p@2X comparison is
  16 cells and eleven are within noise, which is consistent with "no effect" but
  also with an effect smaller than 13 Elo. More rungs would tighten it, and it
  matters because it is the difference between "`retain` belongs in the track
  definition for honesty" and "`retain` belongs in it for strength".
- **Check whether the time and node tracks order the cores differently.** The node
  track has `tdleaf_self lin model=169` about 200 Elo above the chip counter; the
  time track at 400ms has 1318 vs 1062, a similar gap. If the orderings never
  diverge, the two tracks are measuring the same thing at different cost and one
  of them is redundant. If they do diverge, both titles are meaningful. This is
  one query against the existing data and was offered as a criterion before the
  run but not chosen.
- **`nextIterationFits` uses this search's own last-two-iteration growth.** At the
  short rungs a search may complete only two or three iterations, so the estimate
  is built from very little. Measuring its prediction error against the iteration
  that actually followed would say whether the 0.4 underspend is a necessary
  consequence of a conservative predictor or a fixable one, which is the
  alternative to `retain` for the same problem.

## Ideas This Inspired

- **A `cal=` style calibrated time head.** The 0.4 underspend is systematic and
  per-core stable (0.38-0.56 across every core and rung). A head that multiplies
  its internal deadline by the reciprocal would hit the flag without needing a
  purse at all, and unlike `retain` it would not let one move borrow from another.
- **Report realized ms in `CHAMPION.md` next to every time-track title.** The
  whole confusion in this thread came from a flag being read as a spend. A title
  row that carries both makes the gap unmissable.
- **The spread table is the track's own health check.** Cross-core realized-ms
  spread at a fixed budget is one number that says whether a track is normalizing
  what it claims. Worth computing automatically in `rank.exe rate` per head, since
  it caught a 36x discrepancy that had gone unnoticed across many studies.
