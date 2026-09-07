# Plan: migrate both compute tracks, and make the evaluator core the roster unit

Written 2026-09-07, during rung 1 of the play and **before any rung's fit has
been read**. That ordering is the point of the document: the stopping rule below
is pre-registered, so deciding to stop is a comparison against a number written
in advance rather than a judgment made while looking at the answer.

## Why the tracks are being redefined

`ranking/CHAMPION.md` crosses 3 opener divisions with 2 compute tracks. Both
track definitions turned out to describe something other than what they claimed.

- **Neither budget was binding.** Both heads were `deep=6`, and at depth 6 the
  DEPTH CAP is reached before the budget is spent. The chip counter used 42k of
  its 200k node allowance, `pool_games lin model=97` 56k, and the 43 rostered
  `deep=6,time=150ms` agents realized a mean of 20.5 ms/move against a 150 ms
  flag. A budget that never binds normalizes nothing, so the two tracks were
  largely measuring the same depth-6 search twice.
- **The wall-clock track was not a wall-clock instrument.** Even where the clock
  did bind, a plain `time=` head realized 0.432 of its flag (sd 0.053). Theory
  70, `plans/time-ladder-results-1-copper-vireo.md`.
- **The node track was never a compute track.** Cross-core cost at one node
  budget spans 18x to 36x. Theory 69. It measures strength per node, which is a
  legitimate and different question.

## The new track heads

```
track node   ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3
track time   ab(deep=12,tt,ord,retain,time=25ms)@3
```

**Why this pair.** 25ms / 100k nodes = 0.25 us/node, and the three cheap cores
measure 0.23 (chip counter), 0.25 (`pool_games lin model=97`) and 0.26
(`tdleaf_self lin model=169`). They sit on the crossover line, so for most of
the roster the two tracks are the same operating point by construction, at a
mean effective depth of 5.4 to 5.9. An expensive evaluator still separates them
widely, which is why both exist: `position_elo mlp model=113` runs 4.15 us/node
and spends 374 ms/move on the node track against 23 ms/move on the time track.

**Why `deep=12`.** So the budget is what binds. Measured on the new heads,
maximum effective depth reached is 7.82, well clear of the cap.

**Why `retain` on both, and `rem=70` on only one.** `retain` raises realized
spend from 0.432 to 0.854 of the flag. On the node side it needs a gate to have
anything to bank, so `rem=70` is paired with it. The time side already has its
own gate in `nextIterationFits`, where `rem=` is inert.

## Why the roster now lists cores, not agents

One evaluator's six category identities differ only in the track head carrying
it and the division segment it wears, so five sixths of every roster line was
mechanical repetition. Two failures followed from that, both found 2026-09-07:

1. **The cells drifted.** They held 37 / 15 / 14 / 14 / 14 / 14 agents. The
   openless-node bucket is the oldest and collects any bare agent automatically,
   while the other five need a deliberate line. The 22 cores in openless-node
   and not openless-time were newer `tdleaf_self` and `position_elo` models
   added since Round 3, whose time twins nobody minted.
2. **Ablation heads held titles.** Membership was decided by whether the id
   mentioned a budget flag, so `qs`, `noOrd`, `margin=` and `deep=4` study rows
   (17 of them, plus 2 more on the time side) were in title races they exist to
   sit outside of.

`ranking/cores.txt` now names the evaluator once and `ranking/tracks.txt` holds
the cross. `rankCategoryOf` requires the head to EQUAL a configured track head,
which makes the one-head-per-search-family rule structural rather than a
convention someone has to remember. A different search family gets its own
`track` line once it can honour a budget. `gaz` cannot today, having only
`sims=` and no `nodes=`/`time=` grammar at all, so it holds no title in either
track and its checkpoints stay `off`. This preserves the intent of the
2026-08-23 eligibility revision, which dropped the exact-head rule so a future
GAZ agent would not be disqualified by head identity, while removing its side
effect of admitting AB ablations.

## The core set

All 37 cores that were bare on the standard node head, not Round 3's 14. A title
in one division should mean what a title in another means, and the 54-vs-14
imbalance broke that. 37 cores x 3 divisions x 2 tracks = **222 category
agents**.

## THE STOPPING RULE, pre-registered

`--games N` is a target, not an increment, so rungs 2, 4, 8, 16, 32 play exactly
the games of one pass at 32. The ladder does not stop on its own and no rung is
a result. Fitting after every rung and stopping when the answer looks good is
optional stopping, which inflates false positives in a way error bars do not
show, so the criterion is fixed here in advance.

**Floor.** No title claim below **32 games/pair**, per `CLAUDE.md`'s
ranking-claim hygiene rule 2. Conclusions have twice inverted between 8 and 32
games/pair on this project. Rungs 2, 4, 8 and 16 are progress reads only, quoted
with the rung number and per-cell game counts inline, and never as the answer.

**Convergence test**, evaluated between CONSECUTIVE rungs over the 222 category
agents, reading `ranking/cores.tsv`:

1. **Order is stable.** Spearman rho over the 37 cores' mean-Elo ordering is
   `>= 0.99`, and no core moves more than 3 rank positions.
2. **Champions are stable.** All 6 cell champions are the same agent as at the
   previous rung, and each champion's Elo moved by less than 1 combined SE.
3. **Error bars are behaving.** Median `pm` fell by a factor in `[1.30, 1.55]`,
   which brackets the `sqrt(2)` a doubling of INDEPENDENT samples predicts. A
   fall much shallower than that means the extra games are replays rather than
   new information, which is the hazard `Docs/benchmarking.md` defect 3 exists
   for, and it is a reason to investigate rather than to keep doubling.

**Deterministic pairs do not scale, and criterion 3 has to allow for it.**
`pairGameTarget` caps a pair of two deterministic agents at 2 games, floor and
ceiling, because such a pair replays one game per colour and further rows would
be copies. So those pairs contribute a fixed 2 games at every rung while every
other pair doubles. Counted on this roster: 144 of 450 active agents are
deterministic, giving 10,296 of 101,025 pairs frozen at 2 games. Their share of
all stored games therefore falls 10.2% -> 5.4% -> 2.8% -> 1.4% -> 0.7% across
rungs 2, 4, 8, 16, 32. The effect on criterion 3 is real but concentrated at the
early rungs, and it is negligible by the 16 -> 32 doubling that the floor makes
the decisive one. A `pm` ratio below the band at 2 -> 4 or 4 -> 8 is expected
and is NOT the replay defect. Only a shallow ratio at 16 -> 32 is.

**Stop when 1, 2 and 3 all hold at a rung of at least 32 games/pair. Otherwise
double again.** If 1 and 2 hold but 3 fails, stop doubling and report the
independence problem instead, since more games of the same replay do not fix it.

## What is NOT settled by this pass

- **No title moves until Round 4 is rated.** `ranking/CHAMPION.md` still
  describes the `deep=6` tracks and is rewritten only at the end, with the
  `TIME BUDGET NOT ENFORCED` banner retired at the same time and the reason for
  its retirement stated in the Round 4 entry rather than the banner silently
  vanishing.
- **The legacy agents stay on.** The ~127 agents on the superseded `deep=6`
  heads keep their games, their ratings, and their roster lines. They simply no
  longer hold a category. Benching them would remove their games from a joint
  fit and move every surviving rating, which is a separate methodology change.
- **GAZ budget instrumentation** remains designed and unbuilt. The appendix of
  `plans/champion-category-restructure-plan-1-golden-painting-anchor.md` scopes
  it.

## Verification

- `rank.exe check` prints the cross, the per-core count, and the six cell
  counts, which must all be equal. An unequal cell means a hand-written roster
  line is holding a category and names itself in the output.
- The expansion refuses to collide with an explicit roster line, so the drift
  this replaces cannot come back silently.
- `tests/test_ranking.cpp` covers the tracks grammar, the expansion arithmetic,
  the duplicate rejection, and the exact-head membership rule including the
  ablation-head case that motivated it.

## What would make this migration wrong

- If `deep=12` turns out to bind after all at these budgets on some core, that
  core's cell is measuring a depth cap again and the head needs raising. Watch
  the effective-depth column: anything at 12 is a failure of this design.
- If the two tracks never order the cores differently once both budgets bind,
  they are measuring one thing at two costs and one of them is redundant. That
  is a real possible outcome of fixing the budgets, and it should be reported
  rather than assumed away. `ranking/cores.tsv` puts the two columns side by
  side specifically so it can be checked.
