# Results: auditing the two open engine defects, determinism and the time cap

Written 2026-09-09. No paired plan document: this was a direct instruction to
investigate two entries already written up in `todo.md`, in priority order.

Both turned out to be closed. That is the headline, and it is the reason the
method section below is longer than the findings: a null result is only worth
anything if the instrument that produced it was shown to be capable of a
positive one.

## 1. "A deterministic opponent does not reproduce its replies when our side stops searching"

### The claim as it stood

`todo.md` carried this as **DEFECT (indicated, not yet confirmed)**. The
evidence was entirely a chain of inference from a negative result: on a book
mine (slot 23), 12 lines left the book, 5 of them mined as WINS, and all 5
reported `ovr_ply = -1`, meaning no position existed where the replay stood on
a position the mine recorded and the book returned a different move. From that,
"the opponent replying differently is the only thing that can have changed".
The entry itself flagged the weakness: "The opponent replying differently has
never been watched directly."

This matters beyond the book. `rankSchedule`'s `pairGameTarget` caps a pair of
two deterministic agents at 2 games, floor and ceiling, because such a pair is
held to replay one game per colour. That cap is only sound if a reply is a
function of the position.

There was a plausible mechanism, and it is worth stating because it is the one
that turned out to be wrong. The transposition table is ONE process-wide array
of 2^20 slots (`src/transposition.cpp`), shared by both sides, indexed by
`key & TT_MASK`, and `ttStore` is always-replace except for a deeper
same-generation entry on the same exact key. So a searching opponent evicts
this agent's own entries. If the opponent stops searching, it stops evicting,
and the agent probes a different table.

### What was run

`tests/test_determinism.cpp` (new, now in the suite) plus a wider scratch
sweep. A reference game is played normally, then replayed under three
perturbations, compared per ply by `positionKey` hash:

| perturbation | what it models |
|---|---|
| White never searches, its recorded move applied directly | a book-wearing agent, in book for the whole game |
| White silent for its first 8 plies only, then searches | the `.opener(rand,moves=8)` shape, 77 roster rows |
| both sides search normally, but on a table another game already dirtied | whether table state can move a reply AT ALL, independent of who searched |

Swept over 5 boards x 11 configurations: chip counter, `tdleaf_self lin
model=169` and `position_elo mlp model=110`, with and without `tt`, with and
without `qs`, on `ab(deep=6,tt,ord,nodes=200k)@3`, on both new `deep=12` heads,
and on the budget-BOUND case where the node cap actually cuts the search
mid-iteration. That last one matters: a table-driven ordering change can only
alter the RESULT rather than the time taken when the search is truncated
partway, so a sweep that never binds its budget would prove nothing.

### The instrument was validated before the reading was quoted

Two positive controls, both permanent in the test file:

1. **The harness reproduces its own reference.** Two identical runs must agree
   ply for ply. They do.
2. **A stochastic opponent must diverge.** `.dil(prob=20)@1` diverges at
   **ply 1**.

Without control 2 every "no divergence" below would be unfalsifiable. This is
the check whose absence the `CLAUDE.md` rule about validating the instrument
exists to prevent, and the first version of this probe genuinely failed it: it
paired the subject against a White agent with no `tt`, which never writes the
table at all, so the experiment could not have detected the hypothesised
mechanism even if it were real.

### Result

**0 divergences in every node-budgeted and fixed-depth cell**, across all three
perturbations and all 11 configurations.

The only divergences were on `ab(deep=12,tt,ord,retain,time=25ms)@3`: 1 of 5
boards under a silent White (first divergent ply 15) and 1 of 5 under a dirty
table, and they moved between runs of the same binary. That is wall-clock
sensitivity, not a state leak, and it is not a defect: `rankAgentIsDeterministic`
returns false for `timeBudgetMs > 0`, so a `time=` agent is classified as
stochastic and the 2-game cap never applied to it.

Independently, the project's own gate agrees, over a far wider set than the
sweep. `rank.exe determinism --replicas 4` over the **whole deterministic
roster** (140 subjects of 426 active, 2 colours, 4 replicas, 1,120 games)
returns **280/280 subject-colours reproducible**. Restricted to the new node
head, `--only deep=12` returns 70/70. That set includes the fixed-depth agents
the scratch sweep did not cover, since every case there carried a `nodes=` or
`time=` budget.

### Why the shared table does not leak, and why that is fragile

Two properties, both load-bearing:

1. `setTTContext` (`src/ai_minimax.cpp:196`) salts the key with root side,
   evaluator and eval params. `ttProbe` requires `e.key == key` exactly, so a
   foreign entry is a MISS, not a false hit. Always-replace eviction therefore
   costs time, not correctness.
2. **The root move loop is unordered.** `orderMoves` runs only at interior
   nodes. So table-driven ordering cannot reorder the root candidates that a tie
   is broken among, and with the chip counter's large tie sets that is exactly
   where a reordering would have shown up.

Property 2 is a consequence of a design choice made for other reasons, and
nothing currently guards it. **A change that orders the root would invalidate
this whole result**, which is why the test exists rather than a note.

### What the original observation most likely was

The `Phase 0` reproducibility gate already on record found that only
`model=111` and `model=113` on the `time=150ms` head ever varied, and both
miner-versus-`openerBook` disagreements in the book21 run were those same two
cores. A `time=` opponent explains the residual with no new defect. The one
loose end: the 5 slot-23 lines with `ovr_ply = -1` were never checked for
whether their opponent was time-budgeted.

### Blast radius, now measured rather than feared

The worry was that stored games between an opener-wearing agent and a searcher
were affected, which would be the same class as theory 54 (it invalidated every
stored `tt`-vs-`tt` game and forced the `ab@1 -> @2` bump). The
`.opener(rand,moves=8)` shape was tested directly and shows 0 divergences. **No
stored game is invalidated and no code version bump is needed.**

## 2. "AB's `time=` budget does not correctly cap wall-clock cost"

### The claim as it stood

Found 2026-08-24: `model=111` and `model=113` ran **477-500 ms/move against a
`time=150ms` budget, 2-3.3x over**. Root cause given as `budgetTripped()`
checking the deadline only every 4096 nodes, with no check before the outer
iterative-deepening loop starts a new depth. Fix "identified but not
implemented".

### The fix is already in the tree

`nextIterationFits` (`src/ai_minimax.cpp:128`) is exactly that check: it
predicts the next iteration's cost from this search's own last two iterations
and declines one that cannot fit. It is called at lines 520 and 696, and it is
gated on `s_timeOn` alone, **not** on `retain`. So it covers every
time-budgeted search. Its own comment cites the depth-8-into-depth-9 case at
150 ms that this defect describes.

### Measured, 4 boards, 29 subject moves each, opponent `ab(deep=6,tt,ord,nodes=200k)@3.classic(chip=100)@2`

Mean ms per subject move:

| head | classic | lin169 | m110 | m113 |
|---|---|---|---|---|
| `ab(deep=6,tt,ord,time=150ms)@3` | 6.0 | 6.4 | 6.5 | 6.9 |
| `ab(deep=12,tt,ord,time=150ms)@3` | 68.2 | 67.7 | 64.3 | 65.6 |
| `ab(deep=12,tt,ord,retain,time=150ms)@3` | 115.2 | 141.0 | 115.8 | 116.1 |
| `ab(deep=12,tt,ord,time=25ms)@3` | 13.0 | 11.6 | 11.5 | 12.0 |
| `ab(deep=12,tt,ord,retain,time=25ms)@3` | 21.9 | 23.0 | 24.0 | 21.6 |

As a fraction of the flag:

| head | classic | lin169 | m110 | m113 |
|---|---|---|---|---|
| `ab(deep=6,tt,ord,time=150ms)@3` | 0.04x | 0.04x | 0.04x | 0.05x |
| `ab(deep=12,tt,ord,time=150ms)@3` | 0.45x | 0.45x | 0.43x | 0.44x |
| `ab(deep=12,tt,ord,retain,time=150ms)@3` | 0.77x | 0.94x | 0.77x | 0.77x |
| `ab(deep=12,tt,ord,time=25ms)@3` | 0.52x | 0.46x | 0.46x | 0.48x |
| `ab(deep=12,tt,ord,retain,time=25ms)@3` | 0.88x | 0.92x | 0.96x | 0.86x |

Worst SINGLE move, in ms:

| head | classic | lin169 | m110 | m113 |
|---|---|---|---|---|
| `ab(deep=6,tt,ord,time=150ms)@3` | 14.1 | 14.9 | 15.0 | 15.9 |
| `ab(deep=12,tt,ord,time=150ms)@3` | 150.0 | 150.0 | 150.1 | 150.0 |
| `ab(deep=12,tt,ord,retain,time=150ms)@3` | 296.9 | **553.7** | 312.7 | 337.5 |
| `ab(deep=12,tt,ord,time=25ms)@3` | 34.7 | 36.0 | 35.1 | 34.6 |
| `ab(deep=12,tt,ord,retain,time=25ms)@3` | **95.6** | 61.9 | 72.9 | 53.1 |

### Three things fall out of that grid

1. **The defect does not reproduce.** `model=110`/`model=113` on
   `ab(deep=12,tt,ord,time=150ms)@3` spend 64.3 and 65.6 ms/move where the
   2026-08-24 measurement recorded 477-500, and their worst single move is
   150.1 ms against a 150 ms flag. The mean never exceeds the flag on any bare
   head. Round 4's own 562,952 stored rows agree from live play: all 33 cores
   on `ab(deep=12,tt,ord,retain,time=25ms)@3` land between 16.2 and 18.6
   ms/move, a 1.15x cross-core spread.

2. **The old `deep=6` time head was measuring nothing.** 6.0 to 6.9 ms/move
   against a 150 ms flag, 0.04x. The depth cap binds so early that the clock
   never enters into it. This is the second, independent reason the Round 4
   migration moved to `deep=12`, and it is now measured on the head itself
   rather than inferred.

3. **`retain`, not the deadline check, is what now exceeds a flag on a single
   move.** The purse banks the whole unspent remainder with no ceiling
   (`src/agents.cpp:164-171`). Worst observed: **553.7 ms on a 150 ms flag
   (3.7x)** and **95.6 ms on a 25 ms flag (3.8x)**. Bare heads by contrast top
   out at 1.00x (150 ms) and 1.39x (25 ms), the latter being the granularity of
   the every-4096-nodes deadline test. Spending several flags on one move is
   the documented per-GAME contract rather than a bug, and the means stay under
   the flag, but 3.8x is the number the open "cap the purse" decision needs.

### A correction to something said earlier in this session

Mid-investigation I attributed the fix to `retain` bringing the
`nextIterationFits` gate. That is wrong. The gate is on the time path
unconditionally, and the bare `time=` head is measured above holding its flag
without `retain`. There is no untested bare path to fix.

## Changes made

| file | change |
|---|---|
| `tests/test_determinism.cpp` | New. The regression test, four sections plus the stochastic positive control |
| `build_tests.bat` | Registers the new test file |
| `tests/CLAUDE.md` | Table row for the new test, including why the root-ordering property is load-bearing |
| `Docs/theories.md` | Theory 71 added, confirmed with its scope and its fragility stated |
| `src/ai_minimax.cpp` | Comments only. A block above `searchRootWhite` (and a pointer above `searchRootBlack`) recording that the unordered root scan is load-bearing for BOTH the partial-iteration slice and the determinism property, with the instruction to rerun the test if the root is ever ordered |
| `src/CLAUDE.md` | The `ai_minimax.cpp` row's "the root is not ordered" note extended with the same second consequence |
| `todo.md` | Both entries struck through with their measurements; the `retain` purse promoted to its own `[Now]` entry carrying the 3.8x number |

## How to test

```powershell
.\tools\run_tests.ps1 -Build            # the suite, including the new file
.\tests.exe "determinism*" -s           # just this test, with the per-section detail
.\rank.exe determinism --replicas 4 --only "deep=12"
```

Expected: the suite passes, `determinism*` reports its four reproduction
sections passing and the dilution control diverging, and `rank.exe determinism`
reports 70/70 subject-colours reproducible.

## Future Work

- **The 5 slot-23 lines were never traced.** The conclusion above says a
  `time=` opponent explains the book residual, and that is inference from the
  `Phase 0` gate and the book21 disagreements, not from those 5 lines. What
  would settle it: pull the 5 target ids out of the slot-23 run and check
  whether each carries `time=` in its head. One grep, and it either closes the
  entry or reopens it with a real case.
- **Nothing guards the unordered root.** Theory 71 holds because
  `orderMoves` runs only at interior nodes. Root ordering is an obvious future
  search improvement, and whoever makes it will not know it invalidates the
  2-game cap for deterministic pairs. A comment at the root loop pointing at
  the test would be cheap insurance, and is not written yet.
- **The purse cap decision has its number but not its Elo.** 3.8x on a single
  move is measured. What is NOT measured is whether capping the purse at, say,
  2x costs anything, and the honest control is `retain` capped versus `retain`
  uncapped on one core at one budget, not versus bare.
- **The timing grid is one opponent.** All ms/move figures above are against
  `ab(deep=6,tt,ord,nodes=200k)@3.classic(chip=100)@2` over 4 boards. Position
  difficulty drives iteration count, so a harder opponent could push the worst
  single move higher. The Round 4 live figures (16.2-18.6 ms across 33 cores
  and a full roster of opponents) are the cross-check, and they agree, but only
  for the 25 ms retain head.

## Ideas This Inspired

- A `--paranoid` run mode that clears the transposition table between every
  MOVE rather than every game would make the determinism property true by
  construction instead of by argument. It would be far slower, but as an
  occasional audit mode it turns theory 71 into a differential test: play a
  cohort both ways and diff the trajectories.
- The determinism probe is really a general "does this agent depend on hidden
  state" harness. Pointing it at the accumulators rather than the table (assert
  `g_evalPos` and `g_mlAcc` after unsimulate equal their pre-simulate values
  over a long random walk) would catch float drift in the incremental
  evaluators, which currently has no direct test at game length.
- The worst-single-move column is more informative than the mean for anything
  wall-clock, and nothing in the ranking harness records it. `RankMatchRow`
  carries `wms`/`bms` totals and `wmv`/`bmv` counts but no maximum. One more
  double per side would let a whole roster be audited for flag violations from
  the store, without a special probe.
- `nextIterationFits` predicts the next iteration from the growth of the last
  two. The node side uses a static `rem=N` percentage instead. Now that both
  exist, the obvious question is whether the node side would do better with the
  same measured-growth prediction, which would make `rem=` self-tuning per core
  instead of a hand-set knob that had to be swept to find its knee.
