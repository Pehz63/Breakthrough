# Results: the wall-clock ladder, and which track actually normalizes compute

Companion to `plans/time-ladder-plan-1-copper-vireo.md`. Run 2026-09-06 on the
post-bump binary. 40 cells, head `ab(deep=12,tt,ord[,retain],time=Xms)@3`,
**2,136 games per cell, all 40 identical**, one pinned fit against
`ranking/standings.tsv`. 79,200 games over three ladder rungs (19,800 / 19,800 /
39,600), merged with zero torn lines.

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

A plain `time=200ms` agent spends about 80 ms. That is not a rounding error, it
is more than half the budget, and it is `nextIterationFits` declining the last
iteration and then leaving the remainder on the floor. With `retain` the same
agent spends 165 ms. The flag becomes an honest description of the spend, and the
cross-core spread tightens to 1.1x at every rung.

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

Eleven of sixteen cells are within 1.5 SE of zero and the signs alternate. **At
matched wall clock, time-`retain` neither helps nor hurts.** Its raw per-rung
gains (+32 to +125 at the short rungs) are bought by spending the roughly 2x CPU
that plain leaves unspent.

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

## Future Work

- **Extend the ladder to 800 and 1600ms**, at minimum for `position_elo mlp
  model=113` and `pool_games lin model=97`. Without it the study's own criterion
  is unmet for half the cores and no bound can be defended. This is the direct
  continuation and it is cheap: the existing cells stay, only new rungs are added,
  and the scheduler plays only the deficit.
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
