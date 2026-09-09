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
   FOREIGN entry is a MISS, not a false hit. This closes the cross-agent channel
   and nothing more: it says nothing about a searcher evicting its OWN earlier
   entries, which `ttProbe` will happily read because it does not check `e.gen`.
   That second channel is real, and it is ruled out by measurement rather than
   by this argument, in "Four explanations tested and dead" below.
2. **The root move loop is unordered.** `orderMoves` runs only at interior
   nodes. So table-driven ordering cannot reorder the root candidates that a tie
   is broken among, and with the chip counter's large tie sets that is exactly
   where a reordering would have shown up.

Property 2 is a consequence of a design choice made for other reasons, and
nothing currently guards it. **A change that orders the root would invalidate
this whole result**, which is why the test exists rather than a note.

### What the original observation most likely was

**This section was wrong when first written, and the paragraph it replaces is
kept below so a reader who meets it quoted elsewhere recognises it.** The claim
was: "The `Phase 0` reproducibility gate already on record found that only
`model=111` and `model=113` on the `time=150ms` head ever varied, and both
miner-versus-`openerBook` disagreements in the book21 run were those same two
cores. A `time=` opponent explains the residual with no new defect." That
inference was published without running the one grep this document's own Future
Work section said would settle it.

The grep was then run. It falsifies the claim. In `ranking/refute_book23.tsv`
all 210 rows carry `timed = 0`, and the 5 suspect lines' opponents are all
node-budgeted on the same head:

| colour | mined | audit | verify | plies | v_plies | oob_first | opponent |
|---|---|---|---|---|---|---|---|
| W | 1 | 0 | L | 56 | 67 | 16 | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1` |
| B | 1 | 1 | W | 65 | 71 | 9 | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=112,baa2951a,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` |
| B | 1 | 1 | W | 59 | 35 | 11 | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=387,607b64aa,tdleaf_self,lin,shape=30-1)@1` |
| B | 1 | 1 | W | 77 | 49 | 12 | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=459,642147d2,tdleaf_self,lin,shape=129-1)@1` |
| W | 1 | 1 | W | 54 | 54 | 8 | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=261,52cd70f8,tdleaf_self,mlp,shape=129-32-1)@1` |

Three further facts from the same file and from `ranking/refute_book23.log`:

- `blocked_shared = 0` on all 210 rows, and stage 3 reported 0 collisions, so
  the written book is consistent with every won line it stores.
- The oracle and the wearer share one brain,
  `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2` plus
  `.opener(book,book=23)@1` on the wearer. The wearer's fallback is therefore
  the same player that mined the line.
- All 5 `oob_key` values (`f8c848c4112ac741`, `ef5323c57304dc77`,
  `f1cdad46a82df389`, `343f5585cfb47bc5`, `41f5301bd33f47a7`) are absent from
  `models/book23.txt`. The audit game reached a position no won line ever
  visited, rather than one the book held and declined to serve.

The exact refute configuration was then added to the probe and rerun at the
current code version (`ab(deep=8,tt,ord,nodes=2m)@3.classic(chip=100)@2` as our
side, against `model=112` and `model=387` on their own head, plus oracle
self-play): **0 divergences on 5 boards under all four perturbations.**

So four candidate explanations are now dead: a `time=` opponent, a book
overwrite, a blocked shared prefix, and a divergence reproducible at 15 games.

### The phenomenon is deterministic, so nondeterminism was never the explanation

`rank.exe refute --slot 23 --verify-only` re-runs only the audit: it loads the
existing `models/book23.txt` and replays all 210 lines. Run on 2026-09-09 with
the current `@3` binary against the book the `@2` binary mined on 2026-08-29, it
reproduces the original run's progress trace exactly.

| audited | 2026-08-29, `@2` binary | 2026-09-09, `@3` binary |
|---|---|---|
| 40 / 210 | 40 won, 0 left the book | 40 won, 0 left the book |
| 80 / 210 | 77 won, 3 left the book | 77 won, 3 left the book |
| 120 / 210 | 117 won, 3 left the book | 117 won, 3 left the book |
| 160 / 210 | 154 won, 7 left the book | 154 won, 7 left the book |
| 200 / 210 | 192 won, 11 left the book | 192 won, 11 left the book |
| 210 / 210 | **202 won, 12 left the book** | **202 won, 12 left the book** |

Both runs then verify 202-8-0 over 210 games on 3505 book entries.

**A reproducible result is not evidence of nondeterminism.** The 12 lines that
leave the book leave it at the same lines, in the same order, on two different
binaries eleven days and six engine commits apart. Whatever they are, they are a
systematic difference between the mining condition and the audit condition, not a
failure of any agent to reproduce a reply. The entry that started this
investigation read them as the latter.

### Four explanations tested and dead

| explanation | test | result |
|---|---|---|
| the opponent was `time=`-budgeted | grep `timed` in `refute_book23.tsv` | dead: all 210 rows are `timed = 0` |
| a later winning line overwrote the entry | `ovr_ply`, `blocked_shared`, stage-3 collision count | dead: `ovr_ply = -1`, `blocked_shared = 0`, 0 collisions |
| the current binary is nondeterministic under a silent side | 1,836 targeted replay-games, below | dead: 0 divergences |
| the audit ran on a different code version than the mine | `--verify-only` rerun, table above | dead: byte-identical trace |

The third row is the powered version of the probe this document originally
quoted. Our side plays a reference game, then replays it while going silent, and
every ply is compared by `positionKey` hash. Both silent shapes were run, because
the first version of the probe had the window inverted: it went silent from ply N
ONWARD, which is the opposite of the book shape (silent for the opening, brain
resumes at N). Both give the same answer.

| our side, the one that goes silent | opponents | replay-games | diverged |
|---|---|---|---|
| `ab(deep=6,tt,ord,nodes=200k)@3.classic(chip=100)@2` | 4 non-`tt` roster agents | 72 | 0 |
| `ab(deep=6,tt,ord,nodes=200k)@3.classic(chip=100)@2` | 37 `tt` roster agents | 666 | 0 |
| `ab(deep=8,tt,ord,nodes=200k)@3.classic(chip=100)@2` | the 5 slot-23 opponents | 90 | 0 |
| `ab(deep=8,tt,ord,nodes=800k)@3.classic(chip=100)@2` | the 5 slot-23 opponents | 90 | 0 |
| `ab(deep=8,tt,ord,nodes=2m)@3.classic(chip=100)@2` | the 5 slot-23 opponents | 90 | 0 |
| `ab(deep=8,tt,ord,nodes=8m)@3.classic(chip=100)@2` | the 5 slot-23 opponents | 90 | 0 |
| the same six rows again, with the silent window corrected to the book shape | | 738 | 0 |
| **total** | | **1,836** | **0** |

The budget sweep is there because of a specific hypothesis: `ttProbe`
(`src/transposition.cpp:31-42`) returns on `e.key == key` alone and **does not
check `e.gen`**, so a search can read entries its own earlier searches stored in
the same game, both for cutoffs and, unconditionally, for the `fromSq`/`toSq`
that `orderMoves` uses. Whether those entries survive depends on what else stored
into their slots. Our side searching at `nodes=2m` wraps the 2^20-slot table
twice per move and wipes it; going silent leaves it whole. That is a real
asymmetry that `setTTContext` salting does NOT close, since salting stops a
foreign entry being read as ours and says nothing about our search evicting the
opponent's own entries. It is measured here across a 40x span of budgets and it
does not move a single reply.

**The instrument was validated in every arm.** A `.dil(prob=20)@1` opponent must
diverge, and did: at ply 1, 7, 3 and 5 in the four sweep arms and at ply 1 in
both cheap arms. Without that, "0 diverged" would be indistinguishable from a
comparison that cannot see a divergence.

### The correction this forces on the mechanism paragraph

The paragraph below headed "Why the shared table does not leak", and its copies
in `todo.md`, in `Docs/theories.md` theory 71 and in the comment above
`searchRootWhite`, argue that `setTTContext` salting makes a foreign entry a miss
rather than a false hit, and then conclude that the table cannot move a reply.
**The premise is right and the conclusion does not follow from it.** Salting says
nothing about eviction of a searcher's own entries, which is a second channel,
and interior-node ordering is table-driven whether or not the root is. The
unordered root is what bounds the effect at the root, and the 1,836 games above
are what make the claim measured rather than argued. Those texts have been
narrowed to say that.

### What is still open

Whose move actually differs. `ovr_ply = -1` was read as "our moves reproduced the
mined line, so the opponent must have changed", and that inference is not sound:
`ovr_ply` compares the written book against the mined path, so -1 is equally
consistent with "our move matched" and with "the comparison never ran at this
ply". No column reported where the audit game left the mined PATH, as distinct
from where the book fell silent.

Two columns were added to `rank.exe refute` for this, `off_ply` and `off_by`
(`src/ranking.cpp`): the first of our plies whose position differs from the mined
one at the same index, and whether our own move differed there (1) or ours
matched so the opponent replied differently (2). The console prints the split.
They need a full mine to fill, since `--verify-only` has no mined path to compare
against, and that mine is running to `models/book25.txt` and
`ranking/refute_book25.tsv`.

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
| `todo.md` | Both entries struck through with their measurements, the `retain` purse promoted to its own `[Now]` entry carrying the 3.8x number, and the determinism entry rewritten after the follow-up: the `time=` guess removed, the reproduction recorded, two new entries added |
| `src/ranking.cpp` | `off_ply` and `off_by` added to `rank.exe refute`: the first of our plies whose position differs from the mined one at the same index, and whether our move or the opponent's caused it. Report columns, header documentation, and a console split. Also corrects the `ovr_ply` header note, which said the column is blank under `--verify-only` when it prints -1 |
| `tools/CLAUDE.md` | Documents the new columns, corrects the same `ovr_ply` claim, and adds the "re-run the audit before calling it a determinism problem" note |
| `tools/migrate_ab_v3.py` | Skips `refute_*` alongside `det_*` and `recert_snapshots`, so a future bump does not renumber finished reports |
| `Docs/corrections.md` | New class `MIGRATED REPORT PROVENANCE`, for the 2026-09-06 migration rewriting seven finished refute reports |
| `ranking/refute_book2{1,2,3,4}*.tsv` | Point-of-citation header notes for that class, seven files |

## How to test

```powershell
.\tools\run_tests.ps1 -Build            # the suite, including the new file
.\tests.exe "determinism*" -s           # just this test, with the per-section detail
.\rank.exe determinism --replicas 4 --only "deep=12"
.\rank.exe refute --slot 23 --verify-only --roster ranking/roster_refute_snapshot.txt --out scratch.tsv
```

Expected: the suite passes, `determinism*` reports its four reproduction
sections passing and the dilution control diverging, and `rank.exe determinism`
reports 70/70 subject-colours reproducible. The `--verify-only` re-audit must
print 202 won and 12 left the book, matching 2026-08-29 exactly, and it does not
overwrite `models/book23.txt`. Its `off_ply` / `off_by` columns stay at -1,
because `--verify-only` has no mined path to compare against. To see them
filled, mine a fresh slot:

```powershell
.\rank.exe refute --slot 25 --skip-timed --roster ranking/roster_refute_snapshot.txt --out ranking/refute_book25.tsv
```

Its stage 1 should reproduce the slot-23 trace (20 / 33 / 46 / 54 / 73 won at 20
/ 40 / 60 / 80 / 100 mined), and the console should end with a line naming how
many lines left the mined path because our move differed and how many because
the opponent replied differently.

## Future Work

- **Whose move differs on the 12 book-leaving lines.** Closed as far as it can
  be without a mine: four explanations are dead and the phenomenon is
  reproducible rather than flaky. What is not known is whether our own move or
  the opponent's reply is the one that leaves the mined path. `off_ply` and
  `off_by` now report it, and the slot-25 mine that fills them is the run that
  answers it.
- **The `off_by` split does not say WHY.** Even with the column filled, "the
  opponent replied differently" and "our move differed" are each still one step
  short of a mechanism. The follow-up that would close it is a per-ply dump of
  the losing line, mining game against audit game side by side, for one target,
  which is a debug tool nothing currently prints.
- **`--verify-only` cannot fill the new columns**, because it has no mined path
  to compare against, so every cheap re-audit of an existing book leaves them at
  -1. That is the same shape as the `ovr_ply` trap this session tripped over: a
  column that prints -1 both when the comparison ran and found nothing and when
  it never ran. The header now says so. A distinct sentinel would say it better.
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
