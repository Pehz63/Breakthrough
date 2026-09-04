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
depth-1..9 ladder:

| core | full-ladder plies | d5 | d6 | d7 | d8 | d9 |
|---|---|---|---|---|---|---|
| `classic(chip=100)@2` | 412 | 82.9% | 64.0% | 78.0% | 64.2% | 74.6% |
| `learned(model=10,...)@1` | 557 | 80.2% | 73.8% | 77.1% | 72.8% | 76.6% |
| `learned(model=169,...)@1` | 525 | 81.2% | 73.8% | 76.7% | 72.5% | 75.6% |
| `learned(model=113,...,mlp)@1` | 44 | - | 73.2% | 74.8% | 73.9% | - |

**The last completed iteration is 64% to 83% of everything spent to reach it.**
The per-ply growth ratios alternate with parity, which is the ordinary alpha-beta
odd/even effect:

| core | n5/n4 | n6/n5 | n7/n6 | n8/n7 | n9/n8 |
|---|---|---|---|---|---|
| `classic(chip=100)@2` | 5.85x | 2.78x | 4.55x | 2.80x | 3.94x |
| `learned(model=10,...)@1` | 5.04x | 3.78x | 4.37x | 3.71x | 4.28x |
| `learned(model=169,...)@1` | 5.31x | 3.83x | 4.33x | 3.66x | 4.09x |

Against the 200,000-node cap, in absolute terms:

| core | median n6 | median n7 |
|---|---|---|
| `classic(chip=100)@2` | 61,227 | 277,790 |
| `learned(model=113,...,mlp)@1` | 94,042 | 386,691 |
| `learned(model=10,...)@1` | 113,031 | 509,197 |
| `learned(model=169,...)@1` | 124,607 | 536,561 |

Depth 7 needs 1.4x to 2.7x the whole 200k budget. It can never finish. Every
node a `deep=12,nodes=200k` agent spends past `n6` is spent on an iteration that
is structurally doomed.

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
| m113 | 40 | 99.8% | 164,936 | 181,708 | 9.2% | 0.2% |
| m113 | 50 | 99.5% | 158,305 | 183,152 | 13.6% | 0.8% |

The gate is **never deeper** than the ungated agent on any ply of any run, which
is guaranteed by construction and is the check that it only ever declines. The
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

**Elo is not measured here.** Depth and node counts are not strength. The Elo
question is the study described in the next section.

## Elo of search techniques across evaluators (in flight)

Workflow A from `Docs/ranking-workflow.md`: a 40-agent cohort playing into the
main store against the 222-agent active roster, then a pinned fit
(`rate --pin ranking/standings.tsv`) that reads the result on the frozen scale
and cannot disturb any champion. Working roster `ranking/q5/roster.txt`, cohort
list `ranking/q5/cohort.txt`, 8 games per pair.

The grid holds the CORE fixed and varies the HEAD, which is the mirror image of
`CHAMPION.md` rule 6: a search-technique claim needs one core per row, exactly as
an evaluator claim needs one head per row.

Cores, with their current Elo on `ab(deep=6,tt,ord,nodes=200k)@2` for reference:

| core | Elo on the baseline head |
|---|---|
| `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | 1292 |
| `learned(model=349,5ee50d5c,tdleaf_self,lin,shape=129-1)@1` | 1227 |
| `classic(chip=100)@2` | 1124 |
| `learned(model=113,e3cc8b4e,position_elo,mlp,...)@1` | 983 |
| `learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1` | 742 |

Heads: `ab(deep=6,nodes=200k)@2` (bare), `+ord`, `+tt`, `+tt,ord` (the roster
baseline), `+tt,ord,margin=100`, `+tt,ord,qs`, `ab(deep=12,tt,ord,nodes=200k)@2`
(budget actually binding), and `ab(deep=12,tt,ord,part,nodes=200k)@2`.

`qs` and `part` make the turn weight live, so those two classic cells carry
`classic(turn=1,chip=100)@2`, which is the codec enforcing `turnLive` rather than
an inconsistency in the grid.

**The `rem=` heads are not in this cohort**, because the flag was implemented
after the run started. A second cohort pass is the natural follow-up and is the
only way to turn the depth and node numbers above into an Elo claim.

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

- **The `rem=` Elo measurement.** Everything above says the gate reaches equal or
  greater depth for equal or fewer nodes. Nothing above says it is stronger. A
  second cohort pass with `rem=70` heads at `nodes=200k`, `300k` and `550k`,
  pinned against the same frozen scale, is what would settle it. Specifically it
  would test whether the 2-3% of plies that lose a ply cost more than the deeper
  search on the other plies buys.
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
- **Only 4 of 15 cores have a complete depth-1..9 ladder** at the time of
  writing, and the MLP core has only 44 full-ladder plies. The share and growth
  tables should be re-read when the remaining runs land.
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
