# Budget parity, part 2: what `part` actually does, and a gate that fixes the waste

Companion to `budget-parity-plan-1-steady-meridian.md` and
`budget-parity-results-1-steady-meridian.md`. Part 1 established that a
budget-cut iteration is discarded and that this wastes about half the node
budget. This part answers what happens when `part` adopts that iteration
instead, corrects a framing error in part 1 about which head the live roster
actually uses, and adds a search flag that recovers the waste.

All measurements are on `boards/board1.txt` with a 6-ply seeded random opener.
Node counts are deterministic and load-independent, so the runs were
parallelised freely without affecting any number reported here.

## Correction to part 1: the live node track is not budget-bound

Part 1's census, its "54% of the budget is discarded" figure, and its
completed-depth distributions were all measured on `ab(deep=12,tt,ord,nodes=200k)@2`.
**That is not the head the roster uses.** 114 of the 222 active roster agents
sit on `ab(deep=6,tt,ord,nodes=200k)@2`, which caps the deepening loop at depth
6 as well as at 200,000 nodes.

Measured with the new `rank.exe nodeprofile`, 12 games per core, all 15 cores
from part 1's calibration set:

| core | plies | cap binds at `deep=6` | cap binds at `deep=7` |
|---|---|---|---|
| `learned(model=113,...,mlp)@1` | 731 | 14.8% | 77.4% |
| `learned(model=111,...,mlp)@1` | 744 | 13.6% | 77.4% |
| `learned(model=169,...)@1` | 814 | 6.8% | 71.0% |
| `learned(model=76,...)@1` | 825 | 5.0% | 79.3% |
| `learned(model=10,...)@1` | 803 | 4.4% | 72.8% |
| `learned(model=96,...)@1` | 789 | 4.2% | 71.2% |
| `learned(model=4,...)@1` | 669 | 1.6% | 65.3% |
| `learned(model=8,...)@1` | 683 | 1.6% | 74.1% |
| `learned(model=98,...)@1` | 772 | 1.4% | 65.7% |
| `learned(model=3,...)@1` | 774 | 1.2% | 75.6% |
| `learned(model=95,...)@1` | 714 | 0.7% | 64.9% |
| `learned(model=94,...)@1` | 765 | 0.7% | 66.4% |
| `learned(model=99,...)@1` | 659 | 0.6% | 58.8% |
| `learned(model=97,...)@1` | 902 | 0.3% | 37.4% |
| `classic(chip=100)@2` | 540 | 0.0% | 60.6% |

**Mean 3.8% of plies, range 0.0% to 14.8%.** On the head the roster actually
plays, the node cap is a safety net that almost never fires. The node track is
in practice a **fixed-depth-6 track**, and `nodes=200k` is the ceiling that stops
a pathological position, not the thing that decides how deep the search goes.

Three part-1 conclusions have to be re-scoped rather than repeated:

- The "54% of the budget goes into a discarded iteration" figure is a property
  of `deep=12,nodes=200k`, not of any rostered agent. On the roster head the
  discarded fraction is whatever the 3.8% of budget-bound plies contribute,
  which is small.
- The 200k-versus-250k question is close to moot for the current roster. Raising
  a ceiling that fires on 3.8% of plies changes 3.8% of plies. The earlier
  census that made 250k look consequential was measured at `deep=12`.
- `part` is inert on 96.2% of roster plies for the same reason: with no cut
  iteration there is nothing to adopt.

At `deep=7` the picture inverts: the cap binds on 37% to 79% of plies. So the
roster head sits just below the knee, and one more ply of depth cap would turn
the node budget from a safety net into the binding constraint.

## How `part` decides a move: mixed, and partly unsearched

The adoption test, `src/ai_minimax.cpp`:

```cpp
bool adopt = (moveX1 == -1) || (g_keepPartial && mx != -1 && a > alphaPrev);
if (adopt && mx != -1) { moveX1 = mx; moveY = my; moveX2 = mz; alpha = a; }
```

`alphaPrev` is the root score of the last iteration that FINISHED. `a` is what
`searchRootWhite` returned from the iteration the budget cut. So `part` compares
a depth-d value against a depth-(d-1) value, which is the "mixed evaluations"
half of the question. The other half is worse.

**`searchRootWhite` does not stop when the budget trips.** It walks every root
move:

```cpp
rootTotal++; if (!s_budgetHit) rootDeep++;
```

and `budgetTripped` short-circuits each subsequent recursion at its first node:

```cpp
bool budgetCut = budgetTripped(nodes);
if (level == depth || budgetCut) {
    if (budgetCut && level != depth) s_budgetHit = true;
    ...
    return evalLeaf(White, evaluator, evalParams);
}
```

Once `nodes >= g_nodeDeadline`, `budgetTripped` returns true on every later call
with no further node growth, so a root move examined after the trip is scored by
a static `evalLeaf` one ply in, with **no opponent reply searched at all**. That
score is systematically optimistic for the side to move, because the refutation
is exactly what was skipped. It can therefore win the `a > alphaPrev` test on
merit it does not have.

So `a` is a maximum over three tiers of root move:

1. examined before the trip: genuinely searched to depth d
2. examined at the trip: partially searched
3. examined after the trip: a static leaf value, not a search result

**Measured** with new telemetry (`g_lastPartAdopt`, which compares the index of
the root move that set alpha against `rootDeep`).
`ab(deep=12,tt,ord,part,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`,
279 budget-limited plies:

| outcome | plies | share |
|---|---|---|
| no adoption (the completed depth's move stands) | 173 | 62.0% |
| adopted a root move genuinely searched to depth d | 54 | 19.4% |
| **adopted a root move scored after the cut, never searched** | **52** | **18.6%** |

**Half of all `part` adoptions (52 of 106) rest on a score no search produced.**

### There is no root move ordering

The intuition behind "a partial iteration covers the important moves first"
requires the root to be ordered. It is not. `orderMoves` is called only inside
`maxAlphaBeta` and `minAlphaBeta`, at interior nodes. `searchRootWhite` walks a
fixed geometric board scan: y descending, x ascending, then capture-left,
capture-right, quiet-left, quiet-right, forward. A partial iteration therefore
covers an arbitrary geometric slice of the root moves, not a promising one.

## Re-answering part 1's questions with `part` enabled

**"Is 5.5 about a 6, and 6.3 about a 7, because of ordering?"** No, on both
counts. The ordering premise fails as above, and the agreement numbers say the
`part` agent is not an approximation of either neighbour.
`learned(model=169,...)@1`, `nodes=200k`, `deep=12`:

| pair | agreement | 95% CI | polls |
|---|---|---|---|
| `nodes=200k` (discards) vs `deep=6` | 92.2% | 90.0-93.9 | 743 |
| `nodes=200k,part` (adopts) vs `deep=6` | 70.1% | 67.7-72.4 | 1440 |
| `nodes=200k` vs `deep=7` | 56.4% | 52.7-60.1 | 693 |
| `nodes=200k,part` vs `deep=7` | 52.6% | 49.1-56.1 | 776 |
| `nodes=200k,part` vs `nodes=200k` | 74.4% | 72.0-76.7 | 1326 |

Turning `part` on moves the agent decisively AWAY from depth 6 (92.2% -> 70.1%)
without moving it toward depth 7 (56.4% -> 52.6%, intervals overlapping). It
produces a third player, not an interpolation between two.

**"Is 6.1 a bad depth because it explored only a tiny set of the next depth?"**
Without `part`, 6.1 is exactly a 6 (233 of 233 plies played the `deep=6` move,
part 1). With `part`, 6.1 is the WORST case rather than a harmless one: at cut
fraction 0.1, roughly 90% of the root moves in the adopted iteration are tier-3
static values.

**"Should the node count go to 250k?"** Reframed by the census above: on the
roster head this changes 3.8% of plies. The question only becomes live if the
depth cap moves to 7.

**The turn weight under mixed ply parity.** The concern is sound in principle,
since a cut iteration leaves leaves at mixed parity and the constant no longer
cancels. Measured, it does not bite:
`ab(deep=12,tt,ord,part,nodes=200k)@2.classic(turn=0,chip=100)@2` against the
same agent with `turn=1` agrees **100%, 426 of 426 polls**. Classic's turn
weight is 1 against a chip weight of 100, so `a > alphaPrev` would need two
candidates within 2 points to flip. The ID codec already gates the weight's
visibility on `turnLive = useQuiescence || keepPartial`, so the comparison is
unrepresentable in exactly the cases where it is meaningless. This is one core
at one weight ratio, and a learned evaluator's side-to-move term (`g_mlStmW`) is
learned rather than configured, so it cannot be A/B tested through the ID path.

## Does the disagreement track the cut fraction? Essentially no

The question was whether `part` diverges more at 6.7 (most root moves genuinely
searched) than at 6.2 (few). Because `part` and no-`part` search a byte-identical
tree and differ only in adoption, the cut fraction is a shared property of the
position, which makes this a clean paired comparison. Instrument check:
**708 of 708 plies reported identical effective depth and completed depth for
both agents.**

Point-biserial correlation between the cut fraction and disagreement, over
budget-limited plies:

| comparison | polls | r | z | significant at 95% |
|---|---|---|---|---|
| m169, `part` vs no-`part` | 1171 | +0.057 | +1.95 | no |
| m113 (MLP), `part` vs no-`part` | 1085 | -0.004 | -0.12 | no |
| `classic(chip=100)@2`, `part` vs no-`part` | 818 | -0.087 | -2.50 | yes, negative |
| m169, `part` vs `deep=6` | 1258 | +0.089 | +3.17 | yes, positive |

Every |r| is at or below 0.09, so under 1% of the variance, and **the sign is
not consistent across cores**. The bucketed tables are non-monotone. The honest
answer is that the cut fraction is not what decides whether `part` changes the
move.

The control that makes this readable: on plies where the budget never bound,
agreement is 100% (155/155 m169, 163/163 classic, 136/136 m113). `part` is
exactly inert with no cut iteration, which is what it should be.

## How much of a budget does one iteration cost?

Measured with the new `rank.exe nodeprofile`, which records
`g_nodesAtDepth[d]`, the cumulative node count at the instant iterative
deepening finished depth d. Plies where a mate score or a `nearWinCheck`
short-circuit ended the search early are excluded by REASON (the recorded
`BudgetKind`), not by a node threshold.

Share of everything spent to finish depth d that went into the depth-d
iteration alone, `(n_d - n_{d-1}) / n_d`, median over plies completing the whole
depth-1..9 ladder. All 15 cores, 12 games each, `ab(deep=9,tt,ord,nodes=999999k)@2`
so no budget ever binds:

| core | full-ladder plies | d5 | d6 | d7 | d8 | d9 |
|---|---|---|---|---|---|---|
| `classic(chip=100)@2` | 412 | 82.9% | 64.0% | 78.0% | 64.2% | 74.6% |
| `learned(model=3,...)@1` | 608 | 80.0% | 73.0% | 75.5% | 70.7% | 74.4% |
| `learned(model=4,...)@1` | 566 | 80.1% | 72.5% | 76.7% | 71.6% | 74.5% |
| `learned(model=8,...)@1` | 525 | 80.7% | 73.4% | 76.5% | 71.7% | 74.8% |
| `learned(model=10,...)@1` | 599 | 80.2% | 73.6% | 77.1% | 72.7% | 76.2% |
| `learned(model=76,...)@1` | 552 | 81.1% | 72.7% | 76.6% | 70.7% | 74.9% |
| `learned(model=94,...)@1` | 572 | 80.1% | 71.6% | 76.0% | 70.2% | 74.3% |
| `learned(model=95,...)@1` | 590 | 80.8% | 71.3% | 75.9% | 70.2% | 74.2% |
| `learned(model=96,...)@1` | 580 | 80.9% | 71.9% | 76.9% | 70.8% | 74.4% |
| `learned(model=97,...)@1` | 466 | 78.9% | 70.4% | 76.0% | 68.3% | 76.4% |
| `learned(model=98,...)@1` | 648 | 79.9% | 71.4% | 75.8% | 70.3% | 73.8% |
| `learned(model=99,...)@1` | 576 | 79.4% | 72.3% | 75.1% | 70.4% | 72.9% |
| `learned(model=111,...,mlp)@1` | 535 | 80.2% | 73.8% | 77.3% | 71.9% | 76.2% |
| `learned(model=113,...,mlp)@1` | 599 | 80.4% | 73.5% | 77.0% | 71.9% | 75.1% |
| `learned(model=169,...)@1` | 634 | 81.0% | 73.6% | 76.6% | 72.2% | 75.1% |

**The last completed iteration is 64% to 83% of everything spent to reach it.**
Excluding `classic`, the across-core spread at a fixed depth is 2.2 pp at d5,
3.4 pp at d6, 2.2 pp at d7, 4.4 pp at d8 and 3.5 pp at d9. The two MLP cores
fall inside the linear cores' range at d5, d8 and d9, and are the maximum at d6
(`model=111`, 73.8% against 73.6% for `model=10`) and at d7 (`model=111`, 77.3%
against 77.1%), by 0.2 pp in both cases. `classic` is the only core outside the
group, and only on the even plies, where its share is 64.0% and 64.2% against
70.2-73.8% for every other core.

The per-ply growth ratios alternate with parity, which is the ordinary alpha-beta
odd/even effect, and `classic`'s low even-ply shares are the same fact seen from
the other side (its `n6/n5` is 2.78x against 3.4-3.8x for every other core):

| core | n5/n4 | n6/n5 | n7/n6 | n8/n7 | n9/n8 |
|---|---|---|---|---|---|
| `classic(chip=100)@2` | 5.85x | 2.78x | 4.55x | 2.80x | 3.94x |
| `learned(model=3,...)@1` | 5.01x | 3.71x | 4.09x | 3.41x | 3.91x |
| `learned(model=4,...)@1` | 5.03x | 3.64x | 4.28x | 3.52x | 3.93x |
| `learned(model=8,...)@1` | 5.19x | 3.76x | 4.26x | 3.53x | 3.97x |
| `learned(model=10,...)@1` | 5.04x | 3.78x | 4.36x | 3.66x | 4.21x |
| `learned(model=76,...)@1` | 5.30x | 3.67x | 4.27x | 3.42x | 3.98x |
| `learned(model=94,...)@1` | 5.04x | 3.52x | 4.17x | 3.35x | 3.89x |
| `learned(model=95,...)@1` | 5.20x | 3.48x | 4.15x | 3.36x | 3.87x |
| `learned(model=96,...)@1` | 5.25x | 3.56x | 4.32x | 3.42x | 3.90x |
| `learned(model=97,...)@1` | 4.73x | 3.38x | 4.17x | 3.15x | 4.24x |
| `learned(model=98,...)@1` | 4.97x | 3.50x | 4.14x | 3.37x | 3.82x |
| `learned(model=99,...)@1` | 4.85x | 3.60x | 4.01x | 3.38x | 3.69x |
| `learned(model=111,...,mlp)@1` | 5.05x | 3.82x | 4.40x | 3.56x | 4.20x |
| `learned(model=113,...,mlp)@1` | 5.10x | 3.78x | 4.35x | 3.55x | 4.02x |
| `learned(model=169,...)@1` | 5.27x | 3.79x | 4.27x | 3.60x | 4.02x |

Against the 200,000-node cap, in absolute terms, median cumulative nodes:

| core | n6 | n6 as % of 200k | n7 | n7 as x of 200k |
|---|---|---|---|---|
| `classic(chip=100)@2` | 61,227 | 31% | 277,790 | 1.39x |
| `learned(model=99,...)@1` | 83,981 | 42% | 338,842 | 1.69x |
| `learned(model=98,...)@1` | 84,546 | 42% | 348,102 | 1.74x |
| `learned(model=94,...)@1` | 84,977 | 42% | 352,866 | 1.76x |
| `learned(model=95,...)@1` | 79,627 | 40% | 345,959 | 1.73x |
| `learned(model=97,...)@1` | 91,636 | 46% | 395,033 | 1.98x |
| `learned(model=4,...)@1` | 95,320 | 48% | 414,441 | 2.07x |
| `learned(model=3,...)@1` | 95,684 | 48% | 410,131 | 2.05x |
| `learned(model=96,...)@1` | 98,805 | 49% | 439,568 | 2.20x |
| `learned(model=8,...)@1` | 108,557 | 54% | 457,290 | 2.29x |
| `learned(model=76,...)@1` | 111,503 | 56% | 470,938 | 2.35x |
| `learned(model=10,...)@1` | 112,027 | 56% | 505,950 | 2.53x |
| `learned(model=169,...)@1` | 119,598 | 60% | 511,187 | 2.56x |
| `learned(model=113,...,mlp)@1` | 121,832 | 61% | 528,885 | 2.64x |
| `learned(model=111,...,mlp)@1` | 123,644 | 62% | 557,200 | 2.79x |

**Depth 7 needs 1.39x to 2.79x the whole 200k budget across all 15 cores. It can
never finish.** Every node a `deep=12,nodes=200k` agent spends past `n6` is spent
on an iteration that is structurally doomed. The n6 column says how much room is
left after depth 6 completes: 38% of the budget for `model=111` at one end, 69%
for `classic` at the other, and that leftover is exactly what the `rem=N` gate
below is designed to stop spending.

## Why the chip counter's even-ply iterations are cheap

Two cores stand out in the share table above: `classic(chip=100)@2` (a chip
counter) and `learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1` (a
pool-games linear model) have both the fewest full-ladder plies and the lowest
even-depth shares. They turn out to be two unrelated effects that the pooled
median makes look like one.

### The ply count is game length, not the filter

The exclusion is uniform. Every core lost exactly 108 plies of the 12 games
(9 per game, the endgame mate short-circuits), except the pool-games linear
`model=3` at 117. What differs is how long the games run:

| core | regime | median game plies | full-ladder plies |
|---|---|---|---|
| `classic(chip=100)@2` | chip counter | 48 | 412 |
| `learned(model=97,...)@1` | pool_games lin | 51 | 466 |
| the other 13 | various | 58 to 68 | 525 to 648 |

Nothing about the measurement is different for those two. They simply produce
fewer plies to measure.

### The even-depth deficit is uniform for one core and late-game for the other

Median depth-6 share split by game phase, full-ladder plies only:

| core | regime | ply 1-20 | ply 21-40 | ply 41-60 |
|---|---|---|---|---|
| `classic(chip=100)@2` | chip counter | **63.2%** | 65.0% | 65.1% |
| `learned(model=97,...)@1` | pool_games lin | 73.6% | 69.9% | 66.3% |
| the other 13 | various | 72.8-75.9% | 71.0-74.3% | 69.8-72.1% |

The chip counter is low from the first move and stays flat. It is not a phase
effect and not a crowding effect either: bucketed by root legal-move count it
reads 64.1% in the 23-30 bucket where every other core reads 71.3-74.0%. The
pool-games `model=97` is normal in the opening and drifts down, which is an
ordinary endgame effect of a different kind and much smaller.

### The mechanism is evaluator granularity, and it can be isolated

The alternation index below is the median growth ratio into an ODD depth divided
by the median ratio into an EVEN depth, averaged over the two of each in the
depth-5..9 ladder. 1.0 means no odd/even asymmetry.

At an odd depth the leaf is the position after OUR move, with no reply searched.
At an even depth the opponent has replied. That parity is the same for every
core, so it cannot on its own explain why one core differs. What differs is that
a chip counter's value changes only when material changes. Odd-depth leaves
spread out by whatever capture was just made. Even-depth leaves have mostly seen
the recapture and return to the same material total, so a large share of them
carry the IDENTICAL score. Equal scores still satisfy the `>= beta` cutoff test,
so a coarse evaluator produces cutoffs where a fine-grained one produces a search.

The test that isolates this: add a positional term to the SAME evaluator function
with a weight that cannot reorder any position differing in material. `classic`'s
positional term is `evalPosFull(p, 4)` -> `structOwner`, which scans a 7x7 grid of
cells contributing at most two pairs each at +/-`wallW` or +/-`colW`
(`src/ai_eval.cpp`). With either weight at 1 the term is bounded by 49, with both
at 1 by 98, and the turn term adds 1. All are strictly below the chip weight of
100, so a one-piece material difference always wins. These variants are the same
player on every position where material differs, and differ only in how ties are
broken.

All rows `ab(deep=9,tt,ord,nodes=999999k)@2` unless the variant says otherwise,
12 games each, so no budget ever binds:

| variant | full-ladder plies | median game plies | even ratio | odd ratio | alternation | d6 share |
|---|---|---|---|---|---|---|
| `classic(chip=100)@2` | 412 | 48 | 2.79x | 4.24x | **1.52** | 64.0% |
| `classic(chip=100,column=1)@2` | 511 | 55 | 3.25x | 4.21x | 1.30 | 70.0% |
| `classic(chip=100,wall=1)@2` | 524 | 59 | 3.16x | 4.19x | 1.32 | 68.9% |
| `classic(chip=100,wall=1,column=1)@2` | 502 | 55 | 3.30x | 3.95x | **1.20** | 70.7% |
| `classic(turn=1,chip=100)@2` under `qs` | 458 | 52 | 3.07x | 3.87x | 1.26 | 68.0% |
| `classic(chip=100)@2` with no `ord` | 392 | 45 | 2.81x | 4.78x | **1.70** | 62.3% |
| `learned(model=169,...)@1` (tdleaf_self lin) | 634 | 67 | 3.70x | 4.14x | 1.12 | 73.6% |
| same, under `qs` | 638 | 67 | 3.67x | 4.02x | 1.10 | 73.8% |
| same, with no `ord` | 624 | 66 | 4.18x | 4.49x | 1.07 | 76.1% |

Three readings, in decreasing order of how strongly the data supports them.

**1. A tie-only positional term removes most of the asymmetry.** Alternation
falls 1.52 -> 1.30 / 1.32 / 1.20 and the depth-6 share rises 64.0% -> 70.0% /
68.9% / 70.7%, which lands the chip counter inside the pack's 70.4-73.8% range.
The evaluator is otherwise unchanged and cannot have reordered a single
material-differing position, so the only thing the term did was give tied leaves
distinct values. This is the isolated test and it is what the mechanism predicts.

**2. Quiescence removes about half of it, on the chip counter only.** Under `qs`
alternation goes 1.52 -> 1.26 for the chip counter and 1.12 -> 1.10 for the
tdleaf_self linear core. Quiescence resolves the capture at the leaf, so the
odd/even material asymmetry is exactly what it removes, and it helps exactly the
core whose value is nothing but material. Caveat: quiescence nodes count against
the same node totals, so this row's ratios are not measuring quite the same tree
as the others.

**3. Move ordering is NOT the cause.** This refutes the ordering half of the
hypothesis that prompted the measurement. Turning `ord` off makes the chip
counter's asymmetry LARGER, 1.52 -> 1.70, and it does so entirely on the odd side
(odd 4.24x -> 4.78x, even 2.79x -> 2.81x, unchanged). If capture-first ordering
were what made even iterations cheap, removing it would have raised the even
ratio, and it did not move at all. On the tdleaf_self linear core removing `ord`
moves alternation the other way (1.12 -> 1.07) by making even depths much more
expensive (3.70x -> 4.18x). So ordering matters to both cores and in opposite
directions, and neither direction is the chip counter's even-depth cheapness.

**Game length moves with granularity too.** Adding the tie-breaking term
lengthens the chip counter's games from a median of 48 plies to 55-59, most of
the way to the pack's 58-68. Across the 15 cores alternation and median game
length correlate at r = -0.797 (t = -4.76, df = 13, and r = -0.673 excluding the
chip counter), so coarseness, short games and high alternation all travel
together. The `column=1` run is what separates cause from correlation: it changed
the evaluator's granularity and moved alternation two thirds of the way to the
pack while game length moved only 7 plies.

## The `rem=N` gate

New `ab()` head flag: **start another deepening iteration only if at least N% of
the node budget is still unspent.** `src/ai_minimax.cpp`:

```cpp
static inline bool nodeIterationWorthStarting(unsigned long long nodes) {
    if (g_iterMinRemain <= 0 || !g_nodeDeadline) return true;
    if (nodes >= g_nodeDeadline) return false;
    double remainPct = 100.0 * (double)(g_nodeDeadline - nodes) / (double)g_nodeDeadline;
    return remainPct >= (double)g_iterMinRemain;
}
```

It is the node counterpart to the existing wall-clock `nextIterationFits`, and
deliberately a static threshold rather than a prediction, so the agent's identity
carries the setting in its ID rather than depending on a runtime estimate.
Inert at 0 and inert with no node budget, so every pre-existing agent is
unchanged. Canonical spelling `ab(deep=12,tt,ord,rem=76,nodes=200k)@2`, emitted
after `margin=` and before `nodes=`, range 1-99.

Derivation of the threshold: finishing depth d+1 costs about 3.4x what has been
spent reaching depth d, so `remaining >= 3.4 x spent` means `remaining >= 0.77 x
budget`. Predicted knee near `rem=77`.

Measured, 8 games per direction against the same core at `nodes=200k` with no
gate:

| core | rem | move agreement | nodes/move gated | nodes/move ungated | node saving | plies losing a ply |
|---|---|---|---|---|---|---|
| classic | 40 | 99.8% | 162,071 | 167,531 | 3.3% | 0.0% |
| classic | 50 | 99.8% | 161,819 | 169,079 | 4.3% | 0.6% |
| classic | 60 | 99.4% | 144,684 | 169,784 | 14.8% | 1.7% |
| classic | 70 | 98.6% | 103,631 | 169,526 | 38.9% | 2.1% |
| classic | 76 | 98.4% | 77,755 | 172,012 | 54.8% | 6.6% |
| classic | 82 | 98.7% | 59,370 | 173,142 | 65.7% | 13.0% |
| classic | 88 | 96.9% | 37,158 | 161,261 | 77.0% | 40.8% |
| m169 | 40 | 99.8% | 164,277 | 178,900 | 8.2% | 0.2% |
| m169 | 50 | 99.6% | 152,566 | 178,771 | 14.7% | 0.6% |
| m169 | 60 | 99.4% | 135,817 | 178,531 | 23.9% | 1.1% |
| m169 | 70 | 99.2% | 116,452 | 179,316 | 35.1% | 2.7% |
| m169 | 76 | 96.3% | 101,518 | 181,956 | 44.2% | 7.7% |
| m169 | 82 | 85.0% | 75,402 | 181,769 | 58.5% | 27.0% |
| m169 | 88 | 69.9% | 43,342 | 179,875 | 75.9% | 57.5% |
| m113 (MLP) | 40 | 99.8% | 164,936 | 181,708 | 9.2% | 0.2% |
| m113 (MLP) | 50 | 99.5% | 158,305 | 183,152 | 13.6% | 0.8% |
| m113 (MLP) | 60 | 99.0% | 147,822 | 182,953 | 19.2% | 1.3% |
| m113 (MLP) | 70 | 98.3% | 122,374 | 179,639 | 31.9% | 3.1% |
| m113 (MLP) | 76 | 95.4% | 106,920 | 181,041 | 40.9% | 6.2% |
| m113 (MLP) | 82 | 81.5% | 71,824 | 182,979 | 60.7% | 28.9% |
| m113 (MLP) | 88 | 60.8% | 44,269 | 182,599 | 75.8% | 60.1% |

The MLP core tracks the two cheap cores closely, so the threshold behaves as a
property of the deepening ladder rather than of the evaluator, which is what the
64-83% share table above predicts.

The gate is **never deeper** than the ungated agent on a single ply of any of the
21 runs above, which is guaranteed by construction and is the check that it only
ever declines. The
cost is the "losing a ply" column: cases where the gate refused an iteration that
would in fact have completed. The knee is around `rem=70`: 35% to 39% of the
nodes saved for 2% to 3% of plies losing a ply. The predicted `rem=77` is too
aggressive, because it was derived from the MEDIAN growth ratio and the
distribution has a left tail where depth d+1 does fit from a spent fraction above
24%.

### Spending the saving

Within a fixed node budget the gate only makes an agent cheaper, which is not by
itself a strength gain. The value shows up when the freed budget is given back as
a bigger cap. `rem=70` at a raised budget, against plain `nodes=200k`:

| core | gated agent | nodes/move gated | nodes/move ungated | mean completed depth gated | ungated | deeper | shallower |
|---|---|---|---|---|---|---|---|
| m169 | `rem=70,nodes=300k` | 163,580 | 178,033 | 5.83 | 5.69 | 17.9% | 0.9% |
| m169 | `rem=70,nodes=400k` | 232,365 | 182,224 | 6.09 | 5.75 | 40.9% | 1.3% |
| m169 | `rem=70,nodes=550k` | 321,363 | 181,080 | 6.27 | 5.72 | 59.8% | 0.8% |
| classic | `rem=70,nodes=300k` | 193,890 | 168,907 | 6.08 | 5.73 | 41.5% | 1.6% |
| classic | `rem=70,nodes=400k` | 233,448 | 168,209 | 6.23 | 5.70 | 57.6% | 0.9% |
| classic | `rem=70,nodes=550k` | 259,947 | 172,855 | 6.28 | 5.65 | 71.3% | 1.7% |

**`rem=70,nodes=300k` on m169 dominates plain `nodes=200k` on both axes at
once**: 163,580 nodes/move against 178,033 (8% fewer) while searching deeper on
17.9% of plies and shallower on 0.9%. The mechanism is that a raised cap only
helps positions whose depth-7 tree happens to be small, and without the gate the
raised cap is spent on doomed depth-7 iterations everywhere else. The gate lets
the budget be raised without paying for the failures.

At `nodes=550k` classic buys +0.63 plies of mean completed depth for 50% more
nodes, which is a normal compute-for-depth trade rather than a free one.

**Depth and node counts are not strength.** The Elo measurement is the next
section, and it agrees on four of the five cores tested: `rem=70,nodes=300k` and
`rem=70,nodes=550k` gain +35/+42 (m169), +51/+41 (m349), +122/+163 (m113) and
+0/+10 (m97) over the roster baseline, while `classic` loses 49 to 66 Elo. The
depth-and-nodes result above measured `classic` and m169 only, and `classic` is
the core it turned out not to predict.

## Elo of search techniques across evaluators

Workflow A from `Docs/ranking-workflow.md`: a 55-agent cohort playing into the
main store against the active roster, 40,517 games at 8 games per pair, then a
pinned fit that reads the result on the frozen scale and cannot disturb any
champion. Working roster `ranking/q5/roster_rem.txt`, cohort list
`ranking/q5/cohort_all.txt`.

The grid holds the CORE fixed and varies the HEAD, which is the mirror image of
`CHAMPION.md` rule 6: a search-technique claim needs one core per row, exactly as
an evaluator claim needs one head per row. Every non-baseline cell has 1,228
games. `qs` and `part` make the turn weight live, so those two `classic` cells
carry `classic(turn=1,chip=100)@2`, which is the codec enforcing `turnLive`
rather than an inconsistency in the grid.

Cores, given by their short names in the tables below:

| short name | canonical core |
|---|---|
| m169 | `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` |
| m349 | `learned(model=349,5ee50d5c,tdleaf_self,lin,shape=129-1)@1` |
| classic | `classic(chip=100)@2` |
| m113 | `learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` |
| m97 | `learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1` |

### The instrument check that changed the answer

The first pinned fit reported `deep=12,tt,ord` as **+231** for `classic` and
**-236** for m169: a 467-Elo disagreement about what should be one effect. Both
were artifacts. Those two rows were already in the pin file at **160 games** each
(+/-37 and +/-28), and `--pin` freezes a listed agent's Elo, so the 1,228 games
this run added to each of them changed nothing. Dropping just those two rows from
a local copy of the pin file (`ranking/q5/pin_no_d12.tsv`, not tracked, since
`standings.tsv` itself is gitignored, regenerate by deleting those two id rows)
and refitting left the other 221 pinned rows byte-identical on Elo and gave both agents their 1,388-game
value: `classic` 1359 -> **1103**, m169 1015 -> **1212**. Registered as
`PINNED AT LOW GAME COUNT` in `Docs/corrections.md`. Every number below is from
the corrected fit.

### Absolute Elo, pinned fit, one core per column

`+/-0` marks a row pinned from `ranking/standings.tsv` rather than fit here.

| head | m169 | m349 | classic | m113 | m97 |
|---|---|---|---|---|---|
| `ab(deep=6,tt,ord,nodes=200k)@2` (baseline) | 1251 +/-0 | 1213 +/-0 | 1128 +/-0 | 974 +/-0 | 733 +/-0 |
| `ab(deep=6,ord,nodes=200k)@2` | 1222 +/-14 | 1181 +/-14 | 1140 +/-0 | 981 +/-11 | 729 +/-11 |
| `ab(deep=6,tt,nodes=200k)@2` | 1187 +/-14 | 1225 +/-14 | 1129 +/-0 | 1009 +/-12 | 713 +/-11 |
| `ab(deep=6,nodes=200k)@2` | 1078 +/-12 | 1122 +/-13 | 1016 +/-0 | 908 +/-11 | 684 +/-12 |
| `ab(deep=6,tt,ord,margin=100,nodes=200k)@2` | 1251 +/-15 | 1195 +/-14 | 1132 +/-0 | 996 +/-12 | 740 +/-11 |
| `ab(deep=6,tt,ord,qs,nodes=200k)@2` | 1267 +/-15 | 1174 +/-14 | 1072 +/-12 | 1036 +/-12 | 696 +/-11 |
| `ab(deep=12,tt,ord,nodes=200k)@2` | 1212 +/-13 | 1196 +/-14 | 1103 +/-11 | 1019 +/-12 | 726 +/-11 |
| `ab(deep=12,tt,ord,part,nodes=200k)@2` | 863 +/-11 | 894 +/-11 | 1075 +/-12 | 461 +/-14 | 593 +/-12 |
| `ab(deep=12,tt,ord,rem=70,nodes=200k)@2` | 1247 +/-15 | 1200 +/-14 | 1067 +/-12 | 1008 +/-12 | 716 +/-11 |
| `ab(deep=12,tt,ord,rem=70,nodes=300k)@2` | 1286 +/-16 | 1264 +/-15 | 1062 +/-12 | 1096 +/-12 | 733 +/-11 |
| `ab(deep=12,tt,ord,rem=70,nodes=550k)@2` | 1293 +/-16 | 1254 +/-15 | 1079 +/-12 | 1137 +/-13 | 743 +/-11 |

Delta against the `ab(deep=6,tt,ord,nodes=200k)@2` baseline, same core:

| head | m169 | m349 | classic | m113 | m97 |
|---|---|---|---|---|---|
| `ab(deep=6,ord,nodes=200k)@2` | -29 | -32 | +12 | +7 | -4 |
| `ab(deep=6,tt,nodes=200k)@2` | -64 | +12 | +1 | +35 | -20 |
| `ab(deep=6,nodes=200k)@2` | -173 | -91 | -112 | -66 | -49 |
| `ab(deep=6,tt,ord,margin=100,nodes=200k)@2` | +0 | -18 | +4 | +22 | +7 |
| `ab(deep=6,tt,ord,qs,nodes=200k)@2` | +16 | -39 | -56 | +62 | -37 |
| `ab(deep=12,tt,ord,nodes=200k)@2` | -39 | -17 | -25 | +45 | -7 |
| `ab(deep=12,tt,ord,part,nodes=200k)@2` | **-388** | **-319** | **-53** | **-513** | **-140** |
| `ab(deep=12,tt,ord,rem=70,nodes=200k)@2` | -4 | -13 | -61 | +34 | -17 |
| `ab(deep=12,tt,ord,rem=70,nodes=300k)@2` | +35 | +51 | -66 | +122 | +0 |
| `ab(deep=12,tt,ord,rem=70,nodes=550k)@2` | +42 | +41 | -49 | +163 | +10 |

Per-move process CPU cost of each cell, in ms/move:

| head | m169 | m349 | classic | m113 | m97 |
|---|---|---|---|---|---|
| `ab(deep=6,tt,ord,nodes=200k)@2` (baseline) | 25.4 | 26.5 | 11.3 | 399.0 | 14.6 |
| `ab(deep=6,ord,nodes=200k)@2` | 19.4 | 19.5 | 11.7 | 515.1 | 12.8 |
| `ab(deep=6,tt,nodes=200k)@2` | 31.2 | 31.0 | 11.4 | 561.2 | 23.0 |
| `ab(deep=6,nodes=200k)@2` | 12.3 | 12.3 | 7.9 | 649.4 | 11.3 |
| `ab(deep=6,tt,ord,margin=100,nodes=200k)@2` | 24.8 | 25.5 | 11.0 | 394.2 | 14.9 |
| `ab(deep=6,tt,ord,qs,nodes=200k)@2` | 29.2 | 29.8 | 12.9 | 472.1 | 16.2 |
| `ab(deep=12,tt,ord,nodes=200k)@2` | 48.6 | 48.1 | 41.6 | 764.8 | 45.9 |
| `ab(deep=12,tt,ord,part,nodes=200k)@2` | 48.4 | 48.3 | 40.1 | 768.3 | 44.9 |
| `ab(deep=12,tt,ord,rem=70,nodes=200k)@2` | 30.0 | 30.5 | 29.4 | 481.7 | 25.9 |
| `ab(deep=12,tt,ord,rem=70,nodes=300k)@2` | 44.3 | 44.3 | 37.4 | 684.1 | 39.4 |
| `ab(deep=12,tt,ord,rem=70,nodes=550k)@2` | 83.5 | 86.3 | 61.2 | 1374.2 | 72.0 |

### What the grid says

**`part` is the largest effect in the table, and it is negative on every core:**
-388, -319, -53, -513, -140. This settles the question the rest of this document
was built around. Adopting a budget-cut iteration's provisional best move is not
a way to recover the 54% of the budget that gets discarded, it is a way to play a
worse move, and the mechanism is already measured above: 18.6% of budget-limited
plies adopt a move whose score came from a static `evalLeaf` one ply in with no
reply searched, which is systematically optimistic for the side to move. The
cheapest core, `classic`, is hurt least (-53) and the most expensive, the MLP
m113, is hurt most (-513), which is consistent with cost per node deciding how
much of the root move list is still unsearched when the cap fires. That ordering
is a two-point pattern across five cores, not a tested mechanism. Theory 64 is
confirmed on strength, not only on mechanism.

**Neither `tt` nor `ord` alone reproduces having both, but having neither is
much worse than either.** Dropping both costs -49 to -173 on all five cores, the
only effect in the table that is negative everywhere and large everywhere. Which
of the two carries the loss is core-dependent and the signs cross: m169 wants
`ord` (`ord` only -29, `tt` only -64), m349 and m113 want `tt` (`tt` only +12 and
+35, `ord` only -32 and +7). The pair is not decomposable, and no single-item
result transfers between cores.

**`margin=100` (aspiration) is close to free and close to nothing:** +0, -18, +4,
+22, +7, with standard errors of 14-15. Only m349's -18 and m113's +22 are past
one standard error, and they point in opposite directions. It costs 2% of CPU.

**`qs` remains core-specific with the sign flipping**, which the earlier partial
fit already showed and the full fit confirms with tighter bars: +16 (m169), -39
(m349), -56 (classic), +62 (m113), -37 (m97). The MLP gains most and `classic`
loses most. Quiescence is a loadout item, and its lift does not transfer.

**`deep=12` at `nodes=200k` is a wash**, -39 to +45, once the low-game pinning
artifact is removed. That is worth stating plainly: raising the depth cap so the
node budget binds does not by itself change strength much, because the extra
iteration is discarded. It also nearly quadruples `classic`'s CPU (11.3 -> 41.6
ms/move) for -25 Elo, which is the cost of that discarded iteration made visible.

**`rem=70` at the same `nodes=200k` is also a wash but for a third less
compute.** Against `deep=12,tt,ord` on the same core, `rem=70` reads +35 (m169),
+4 (m349), -36 (classic), -11 (m113), -10 (m97) while cutting CPU from 48.6 to
30.0, 48.1 to 30.5, 41.6 to 29.4, 764.8 to 481.7, and 45.9 to 25.9 ms/move. The
gate declines only iterations that cannot finish, so declining them is close to
free in Elo and saves 29-38% of the wall clock. That is theory 65's central
prediction, now measured on strength rather than only on depth and node counts.

**Spending the saving is where the gain is, on four of five cores.** Raising the
budget under the gate to `nodes=300k` and `nodes=550k` buys +35/+42 (m169),
+51/+41 (m349), +122/+163 (m113) and +0/+10 (m97) over the roster baseline. m113
is the largest positive number anywhere in this grid at **+163 Elo**, on the core
that the node track exists to subsidize. m169 at `rem=70,nodes=550k` reaches
1293 +/-16 against the roster baseline's 1251, the highest cell in the table.

**`classic` is the exception and goes the other way**: -61, -66, -49 at the three
gated budgets, against -25 for ungated `deep=12`. It is the only core where the
gate costs real Elo, and it is also the core with by far the most budget left
over after depth 6 (median n6 = 61,227, 31% of a 200k cap, against 60% to 62%
for m169 and the two MLP cores). One hypothesis is that `classic`'s doomed
depth-7 iteration is cheap enough to seed the transposition table usefully for
the NEXT move, so declining it throws away a real benefit the expensive cores
never get. That is a hypothesis, not a measured claim. The test is `rem=70`
against ungated `deep=12` with `noTT` on `classic`, where the hypothesis predicts
the gap should close.

### What this does not settle

The gains are measured at a fixed node budget against a roster whose agents are
almost all `deep=6,nodes=200k`, so `rem=70,nodes=550k` is spending more compute
than its opponents (83.5 against 25.4 ms/move for m169). Whether the gate helps
at EQUAL wall clock is a different question and needs the calibrated-`nodes=`
track from the compute-parity work, not this grid. What this grid does establish
is that under the node track's own definition, which charges nothing for
evaluator cost, `rem=70` plus a raised cap beats the current head on four of five
cores, and that `part` should never be used.

## The `retain` carry-over flag

The `rem=N` gate leaves part of the cap unspent on most moves, and that remainder
is forfeited. `retain` banks it instead and adds it to the SAME side's cap on its
next move. The budget then becomes a per-GAME allowance rather than a per-move
one: total spend over a game stays bounded by (cap x plies), while an individual
move may spend several caps' worth.

New binary `ab()` head flag, canonical position after `rem=` and before `nodes=`:

```
ab(deep=12,tt,ord,rem=70,retain,nodes=200k)@2.<core>
```

`src/agents.cpp`, in `agentChooseMove`:

```cpp
const int carrySlot = (side == White) ? 0 : 1;
const bool retaining = a.retainBudget && a.nodeBudget != 0;
unsigned long long effBudget = a.nodeBudget;
if (retaining) effBudget = a.nodeBudget + g_nodeCarry[carrySlot];
if (a.nodeBudget)        g_nodeBudget = effBudget;
...
if (retaining)
    g_nodeCarry[carrySlot] = (g_lastNodes < effBudget) ? (effBudget - g_lastNodes) : 0ULL;
```

**It converges rather than exploding.** Banking raises the next cap, which raises
the gate's ABSOLUTE threshold (`rem=70` means "spend nothing past 30% of the
cap", and 30% of a bigger cap is more nodes), which eventually lets a full
iteration through, which drains the purse. Traced by hand on the chip counter at
`nodes=200k` from the opening position, where depth 6 costs 119,217 nodes: the
gate declines depth 7 on moves 1 through 3 while the purse grows 80,783 ->
161,566 -> 242,349, then move 4 has a 442,349-node cap whose 30% threshold
(132,705) clears the depth-6 cost, starts depth 7, and spends everything. The
cycle then repeats. The test `retain - unspent budget carries to the same side's
next move` asserts both halves of that cycle occur, since a test that only saw
one would pass on a no-op.

**Instrument check**, `rank.exe nodeprofile`, `rem=70,nodes=200k`, 4 games each:

| agent | plies | mean nodes/move | max nodes/move | mean completed depth | plies at depth >= 7 |
|---|---|---|---|---|---|
| chip counter | 152 | 108,032 | 200,164 | 5.56 | 19.7% |
| chip counter, `retain` | 163 | 165,058 | **429,035** | 5.75 | **36.8%** |
| tdleaf_self lin `model=169` | 234 | 116,712 | 200,097 | 5.59 | 16.2% |
| same, `retain` | 236 | 174,098 | **501,604** | 5.81 | **23.7%** |

Individual moves exceed the nominal cap by 2.1x to 2.5x, which is the point, and
the mean stays under it, which is the conservation property holding.

**Correctness contracts, and why each one is there.**

- *Per side.* The purse is `g_nodeCarry[2]`, indexed 0 = White, 1 = Black. One
  shared purse would let one agent in a game spend the other's savings.
- *Per game.* `retainResetCarry()` is called beside every per-game `ttClear()` in
  `src/ranking.cpp` (play, pairgen capture, posgen replay, label playouts,
  refPlayGame) and once per game in `nodeProfileGame`, where the existing
  `ttClear` is per PLY and would have wiped the purse on every move. A carry that
  survived into the next game would make play depend on which games a worker
  happened to run first, the same defect the per-game `ttClear` exists to prevent.
- *The `agree` probe cannot spend the driver's savings.* `agreeStep` runs two
  agents on the same side at the same ply, so the polled agent's purse is parked
  in `s_agreeOthCarry` between plies and swapped in around its search.
- *Inert without a node budget.* There is no cap to leave a remainder of, so a
  fixed-depth agent with `retain` spends the identical node count every move.
- *Inert for every existing agent.* Old and new `rank.exe` produce byte-identical
  node profiles over 181 plies on `ab(deep=12,tt,ord,rem=70,nodes=200k)@2` with
  the tdleaf_self linear core.

### Elo: does `retain` make the node budget matter less?

**Yes on the two cores where the node budget mattered, and by raising the low end
rather than by lowering the high end.** On the three cores whose Elo barely moved
with budget in the first place there was nothing to flatten.

40 cells, one pinned fit (`rank.exe rate --roster ranking/q6/roster_retain.txt
--pin ranking/q5/pin_no_d12.tsv`), 1,328 to 1,358 games per cell, all 40 cells
within 30 games of each other. The head is
`ab(deep=12,tt,ord,rem=70[,retain],nodes=<B>)@2` throughout, so every comparison
below is within one search head and one core, differing only in `nodes=` and the
presence of `retain`. Absolute Elo is from this fit only and must not be compared
to any other fit. Distinct-trajectory check: 0.95 distinct games per stored row on
a sampled cell, so the printed `pm` is close to honest here (these cells face a
mostly deterministic roster at 2 games per pair, which is the floor, not padding).

**Elo per cell, +-SE:**

| core | 100k plain | 100k `retain` | 200k plain | 200k `retain` | 300k plain | 300k `retain` | 400k plain | 400k `retain` |
|---|---|---|---|---|---|---|---|---|
| `classic(chip=100)@2` (chip counter) | 1066+-11 | 1080+-11 | 1072+-11 | 1059+-11 | 1075+-11 | 1094+-12 | 1071+-11 | 1075+-11 |
| `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | 1078+-11 | 1220+-13 | 1222+-13 | 1233+-13 | 1261+-14 | 1272+-14 | 1273+-14 | 1295+-15 |
| `learned(model=349,5ee50d5c,tdleaf_self,lin,shape=129-1)@1` | 1081+-11 | 1201+-13 | 1199+-13 | 1239+-14 | 1242+-13 | 1216+-13 | 1227+-13 | 1265+-14 |
| `learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` | 995+-11 | 1026+-11 | 1004+-11 | 1048+-11 | 1089+-11 | 1102+-12 | 1054+-11 | 1071+-11 |
| `learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1` | 713+-11 | 714+-11 | 714+-11 | 716+-11 | 731+-11 | 730+-11 | 744+-11 | 756+-11 |

**The hypothesis stated as a number.** Weighted least squares of Elo on
log2(node budget) across the four rungs, in Elo per doubling. The flattening
claim is the `difference` column, which is a contrast inside one fit:

| core | plain | `retain` | difference | z |
|---|---|---|---|---|
| chip counter | +3.3+-7.3 | +1.7+-7.4 | -1.6+-10.4 | -0.15 |
| tdleaf_self lin `model=169` | +103.3+-8.1 | +36.7+-9.1 | **-66.6+-12.2** | **-5.45** |
| tdleaf_self lin `model=349` | +81.3+-7.9 | +24.0+-8.9 | **-57.3+-11.9** | **-4.83** |
| position_elo mlp `model=113` | +40.0+-7.3 | +29.5+-7.4 | -10.5+-10.4 | -1.01 |
| pool_games lin `model=97` | +15.2+-7.3 | +18.9+-7.3 | +3.7+-10.4 | +0.36 |

Same thing as a spread, the plain range across the four rungs versus the `retain`
range, in Elo:

| core | plain spread | `retain` spread |
|---|---|---|
| chip counter | 9 | 35 |
| tdleaf_self lin `model=169` | 195 | 75 |
| tdleaf_self lin `model=349` | 161 | 64 |
| position_elo mlp `model=113` | 94 | 76 |
| pool_games lin `model=97` | 31 | 42 |

**`retain` is not flattening by capping the top.** Cell by cell, `retain` minus
plain at matched core and budget:

| core | 100k | 200k | 300k | 400k |
|---|---|---|---|---|
| chip counter | +14+-16 | -13+-16 | +19+-16 | +4+-16 |
| tdleaf_self lin `model=169` | **+142+-17** | +11+-18 | +11+-20 | +22+-21 |
| tdleaf_self lin `model=349` | **+120+-17** | +40+-19 | -26+-18 | +38+-19 |
| position_elo mlp `model=113` | +31+-16 | +44+-16 | +13+-16 | +17+-16 |
| pool_games lin `model=97` | +1+-16 | +2+-16 | -1+-16 | +12+-16 |

Only two entries in the grid are negative by more than their SE, and 18 of 20 are
zero or positive. The flattening comes from a +142 and a +120 at the bottom rung,
not from a loss at the top.

**How much budget `retain` buys back.** On both tdleaf_self linear cores,
`retain` at 100k nodes is statistically indistinguishable from plain at 200k:

| core | `retain`@100k minus plain@200k | `retain`@100k minus plain@400k |
|---|---|---|
| chip counter | +8+-16 | +9+-16 |
| tdleaf_self lin `model=169` | -2+-18 | -53+-19 |
| tdleaf_self lin `model=349` | +2+-18 | -26+-18 |
| position_elo mlp `model=113` | +22+-16 | -28+-16 |
| pool_games lin `model=97` | +0+-16 | -30+-16 |

So on those two cores `retain` is worth exactly one doubling of the node budget at
the low end, and less than two.

**Why the effect is core-dependent.** `rem=70` declines an iteration unless 30% of
the cap is unspent, and how often that fires depends on the ratio of one
iteration's cost to the cap. The 15-core table earlier in this document put the
chip counter's depth-6 cost at 31% of a 200k budget and the position_elo mlp
`model=113`'s at 62%. A core that finishes its last affordable iteration with
little left over has little to bank, and a core whose Elo does not improve with
depth has nothing to spend it on even when it does bank. The chip counter is the
second case: its plain slope is +3.3+-7.3 Elo per doubling, indistinguishable from
zero across a 4x budget range, so `retain` measurably changes its search (mean
nodes/move 108,032 -> 165,058, plies at depth >= 7 rising 19.7% -> 36.8%) and does
not change its strength. That is a consistent pair of facts, not a contradiction,
and it is the cleanest evidence in this document that the chip counter is
depth-saturated at these budgets.

**A `PINNED AT LOW GAME COUNT` check that paid off.** A mid-run preview fit taken
at about half the games had the four 400k `retain` cells at 106 to 1,183 games,
and those cells were flagged as unquotable rather than reported. They moved by up
to 59 Elo on the way to their final values:

| cell | preliminary (games) | final (1,328 games) | moved |
|---|---|---|---|
| tdleaf_self lin `model=169` @400k `retain` | 1261 (207) | 1295 | +34 |
| tdleaf_self lin `model=349` @400k `retain` | 1270 (106) | 1265 | -5 |
| position_elo mlp `model=113` @400k `retain` | 1077 (1,183) | 1071 | -6 |
| pool_games lin `model=97` @400k `retain` | 815 (106) | 756 | -59 |

The 106-game `pool_games` cell would have been read as a +71 `retain` gain. The
value is +12+-16. The 1,183-game cell moved 6 Elo, which is the same defect at a
sample size where it no longer bites.

## Implementation notes

Six layers had to be threaded for `rem=`, mirroring how `margin=` flows:
`globals.h`/`globals.cpp` (the `g_iterMinRemain` global), `agents.h` (the
`iterMinRemain` field), `agents.cpp` (default in `seedAgentDefaults`, save and
restore around the search, the debug flag string), `ranking.cpp` (the `LBL_REM`
label table, the parse branch, the assignment, the canonical emitter), and
`ai_minimax.cpp` (the helper and both colour loops).

`seedAgentDefaults` is the single place every `AgentSpec` gets its defaults, and
both factories call it, so the flag is inert for every agent that does not name
it. Verified directly rather than by inspection: the old and new `rank.exe`
binaries run on the same agent produce identical node counts and an identical
338/366 agreement figure.

New telemetry, all pure observation with no effect on search:

- `g_nodesAtDepth[d]` (`MAX_PROFILE_DEPTH` 64): cumulative nodes when depth d
  finished, 0 if it never did, cleared at the top of every search.
- `g_lastPartAdopt`: 0 nothing adopted, 1 adopted a searched root move, 2
  adopted a post-cut static one. Built on a new file-static `s_rootBestIdx`,
  the root-scan index of the move that last raised alpha (or lowered beta).
- `rank.exe nodeprofile --id <id> --games N --out <tsv>`: one agent self-plays
  both colours and every non-forced ply emits its whole depth ladder plus the
  `BudgetKind` that ended the search.

### Gotchas found

- **`agree`'s TSV columns swap meaning between directions.** `drv_*` is agent A
  in direction 1 and agent B in direction 2. An analysis that reads `drv_*`
  unconditionally is silently averaging two different agents. This produced a
  wrong first pass at the cut-fraction table here, caught because the
  "budget never bound" bucket came out implausibly large (32% of plies against an
  expected 9%).
- **`agree` seeds its opener from BOTH agent IDs**
  (`srand(gameSeed(drv.id, oth.id, g, runSeed))`), so changing the polled agent
  changes the trajectory. A depth ladder built by varying the polled agent is
  therefore not a paired comparison. This is why `nodeprofile` records the whole
  ladder from one search instead.
- **`asp` is spelled `margin=N`** in the ID codec, not `asp`.
- `board` is `char[SIZE][SIZE]`, not `int`. A test that snapshotted it into an
  `int` array overran and compared garbage.

## Future Work

- **Why the gate costs `classic` Elo.** `classic` is the one core where
  `rem=70` loses (-49 to -66 against the roster baseline, against -25 for ungated
  `deep=12`), and it is also the core with the most budget left over after depth
  6 (31% of a 200k cap against 60-62% elsewhere). The hypothesis in the grid
  section is that its cheap doomed depth-7 iteration seeds the transposition
  table for the next move, so the gate throws away a real benefit. The test is a
  `noTT` pair, `ab(deep=12,noTT,ord,nodes=200k)@2` against
  `ab(deep=12,noTT,ord,rem=70,nodes=200k)@2` on `classic`: if the TT is the
  mechanism, the gap should close.
- **`rem=` at equal wall clock, not equal node budget.** The +122/+163 on m113
  and +35/+42 on m169 are measured against a roster running at 25-27 ms/move
  while the `nodes=550k` gated agents run at 83-86. The node track charges
  nothing for evaluator cost by design, so this is a valid claim within that
  track, but it is not a claim that the gate is free. The calibrated-`nodes=`
  wall-clock track from the compute-parity work is where that question belongs.
- **A `rem=` sweep at the winning budget.** Only `rem=70` was rated, and only at
  200k, 300k and 550k. The 21-row agreement sweep says the knee is between
  `rem=60` and `rem=76`, so `rem=60` and `rem=76` at `nodes=550k` would say
  whether 70 is the peak or just the first value tried.
- **`part` combined with `rem=`.** The gate removes most cut iterations, so it
  removes most of `part`'s opportunities to adopt an unsearched score. The two
  interact and neither has been measured with the other on.
- **Fixing the tier-3 scores rather than gating them.** `searchRootWhite` could
  stop at the trip instead of walking the remaining root moves with a static
  eval. That would make `part` compare only genuinely searched values, and is a
  smaller change than the gate. It was not tried, so the 18.6% figure above
  should not be read as "`part` is unfixable", only as "`part` as currently
  written adopts unsearched scores half the time".
- **The parity alternation in iteration cost** (growth 2.78x then 4.55x for
  classic) means one static `rem=` threshold is wrong for one of the two
  parities. A parity-aware threshold, or the predictive form that
  `nextIterationFits` already uses for wall clock, would fit both. Not tested.
- **The ladder measures cost, not the cost under a real budget.** All 15 cores
  now have a complete depth-1..9 ladder (412 to 648 full-ladder plies each), but
  it was run at `nodes=999999k` so no budget ever binds. Whether the same
  per-iteration shares hold when a cap is actually cutting searches short, and
  the TT is being filled by aborted iterations, is not tested.
- **The depth cap, not the node cap, is what the roster is normalising on.**
  Whether `deep=6` is the right cap has never been measured as such, because it
  was always discussed as a node-budget question. `deep=7,nodes=200k` is a
  genuinely different agent (the cap binds 37-79% of the time) and nobody has
  rated one.

## Ideas This Inspired

- **Report `g_nodesAtDepth` into the match store.** Every rated game already
  carries node totals. Carrying the depth profile would make "how budget-bound is
  this agent" answerable from the store instead of needing a bespoke run.
- **A `leaves=` budget.** Iteration cost is dominated by the leaf layer, so a
  leaf budget might make the parity alternation disappear and give one threshold
  that works at every depth.
- **Order the root.** The root is the one place ordering would most improve
  alpha-beta pruning, and it is the one place the search does not do it. This is
  independent of everything above and might be worth more than any budget rule.
- **Use agreement as a pre-filter for Elo runs.** Two agents agreeing at 99.8%
  cannot differ by much Elo, so the `rem=40` and `rem=50` rows above arguably do
  not need games at all. Calibrating agreement against known Elo gaps would turn
  a cheap run into a filter for an expensive one.
- **A "wasted nodes" column in the standings.** Fraction of each agent's nodes
  spent on iterations it discarded, straight from the profile. It would have made
  the `deep=6` versus `deep=12` framing error visible immediately.
