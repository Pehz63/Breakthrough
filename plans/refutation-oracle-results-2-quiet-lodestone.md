# Refutation oracle, phases 1-4: the miner, and the transposition-table defect it found

Companion to `plans/refutation-oracle-plan-1-quiet-lodestone.md` (phases 1-4).
Phase 0's gate is `plans/refutation-oracle-results-1-quiet-lodestone.md`.

## Headline

**One book of 3657 positions wins 230 of 238 lines against the roster's 119
deterministic agents, both colours, on its own.** Mined 2026-08-29 on the
TT-fixed `@2` binary, oracle `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2`,
`--tries 8`, 2756 mining games plus 238 verification games.

The number to quote is 230, not the 238/238 the mining stage reports. Three
stages measure three different things and only the last is a memorization claim:

| Stage | Reports | What it establishes |
|---|---|---|
| mined | 238/238 (143 by the oracle line alone, 95 by repair) | Only that a win existed while our side was still SEARCHING at unbooked plies. Not a book result. |
| audit | 237/238 won, 7 left the book | The written book replayed on its own. The 7 that fell out were carried by the wearer's brain. |
| verify | 235-3-0 through the real `openerBook` | Includes those 7, so it overstates the book. |

Crossing coverage against the verified result splits the 238 lines as:

| Outcome | Lines |
|---|---|
| Fully covered by the book, verified win (**the memorization result**) | **230** |
| Fully covered, audit won, verify lost (`model=113`) | 1 |
| Left the book, won on the wearer's brain (NOT memorization) | 5 |
| Left the book, lost (`model=96` as White) | 1 |
| Left the book, audit won, verify lost (`model=111`) | 1 |

Both miner-versus-`openerBook` disagreements are the same two agents that fail
the determinism gate: `model=111` and `model=113`, the two cost-flagged
`time=150ms` cores running 2-3.3x over budget, `model=113` being the exact
subject-colour that missed this session's gate at half-move 3. A non-reproducible
opponent cannot be mined by construction, so these are not miner defects.
Excluding both, the result is **230 of 236, with 5 brain-assisted and 1 genuine
loss**. Wall-clock-budgeted targets overall went 26/28, and that figure is not
stable: an independent replay of the same book against the same roster returned
27/28, moving one row. Do not quote a `time=`-budgeted result to the unit. See
"The book21 audit reproduces, except on the timed cores" below.

This did not come first. Building the miner surfaced a defect in the
transposition table that invalidated the premise the whole approach rests on, and
fixing that took priority.

**The transposition table was handing each agent the other agent's search
results.** It is one process-wide table keyed by the position hash alone, and
`ttClear()` runs once per GAME rather than per move, so both players of a game
searched through it. A position hash records which position was searched, not who
searched it.

| | |
|---|---|
| Cross-evaluator read | White's agent stores a score its evaluator produced. Black's agent probes the same position, gets a key match, and returns White's evaluator's number as its own. |
| Cross-strength read | `ttProbe` accepts any entry whose stored depth is at least the prober's remaining depth, so a shallower agent reads a deeper agent's entries and plays above its own depth. |
| Reach | 169 of 217 active roster agents carry `tt`. |
| Elo consequence | **Not measured.** |

## How it surfaced

Mining is an unusually sharp probe for this, and that is not a coincidence. A
mined line is played back by a book that does no searching at all, so any
dependence of the opponent's replies on OUR side having searched shows up
immediately as the line failing to reproduce. An ordinary tournament never
notices, because both sides always search.

First full mine, 238 targets, d8/nb2m oracle: 238 of 238 lines mined, 226-12 on
the verification replay. That looked like a near-clean sweep with 12 stragglers.
It was not. A coverage audit of the written book found **132 of the 238 lines had
left the book entirely** and were being carried by the wearer's brain. The split
was exactly on the opponent's own flag:

| Opponent head | Lines that left the book | Lines that stayed |
|---|---|---|
| carries `,tt,` | **132** | 32 |
| no `tt` | **0** | 76 |

Two-setting control, same 8 targets, an oracle identical but for the one flag:

| Oracle | Book entries | Lines that left the book |
|---|---|---|
| `ab(deep=8,ord,nodes=2m)@1.classic(chip=100)@2` | 197 | **0 of 8** |
| `ab(deep=8,tt,ord,nodes=2m)@1.classic(chip=100)@2` | 146 | **8 of 8** |

Both runs reported 8-0-0. The record was not the signal. That is why the coverage
audit now runs on every invocation rather than only under `--verify-only`.

After the fix, the same command with the same `tt` oracle on the same 8 targets
goes to **0 of 8 lines leaving the book**, with the same 197 entries the tt-free
oracle produced.

The Phase 0 reproducibility gate was measured on the pre-fix binary, so it was
re-run on the fixed one before mining: **237/238 subject-colours reproducible over
3 replicas, node-budgeted 210/210**. The single miss is `model=113` as Black
diverging at half-move 3, which is the same agent, colour and divergence point
that missed one pass of four pre-fix. The fix did not regress reproducibility.

## The fix

`setTTContext` in `src/ai_minimax.cpp`, mixed into the key at both probe/store
sites. Each distinct searcher gets a disjoint region of the one table, which is
the same semantics as handing each player a private table. The context is:

- the evaluator index and the full `evalParams` array. That covers the learned
  evaluator's model slot too, since `agentChooseMove` wires the slot into
  `evalParams[0]`.
- `g_useQuiescence`, because a quiescence score at a given remaining depth
  extends captures past the horizon and a plain one does not, so the two are
  different numbers for the same position and depth.
- the ROOT side to move. In a game the White player always searches from a
  White-to-move root and the Black player from a Black-to-move root, so this one
  term separates the two players even when they are otherwise identical, and it
  costs nothing inside a single search where both parities still share a context.

Killer and history tables are not a second channel: `resetSearchHeuristics` runs
at the top of every root search.

## The regression test took three attempts, and the first two proved nothing

`tests/test_ai_integration.cpp`, "the transposition table never hands one searcher
another's score". Validated by reverting the fix and confirming it fails:
Black's node count 157 clean versus 135 contaminated, 30 key matches, 3 cutoffs.

The two failed attempts are worth recording, because both PASSED with the fix
reverted and would have shipped as decoration:

1. **Two searches from the same root never collide.** White's tree is "P then a
   white move", Black's is "P then a black move". The two never visit a common
   position, so the table was never even read. The collision needs an actual
   game: White searches from P and PLAYS, and Black then searches from the
   resulting P', which is a node inside White's tree stored at a greater
   remaining depth than Black's own.
2. **The position short-circuited the search.** The midgame position copied from
   the neighbouring test has White pieces on row 6, so `canWinWhite()` fires and
   `moveWhite` returns a winning move without entering the search at all.
   Instrumenting the table showed `stores=0`: nothing had happened. Moving both
   sides to midboard fixed it.

## Two existing tests were vacuous for the same reason, and are now fixed

Attempt 2's position is the same one two already-committed tests use, and
checking them turned the suspicion into a measurement.

`ttStore` is called at two sites in `minAlphaBeta`, both guarded by
`!isNearWin(best)` (`src/ai_minimax.cpp`). From a position with a White piece
already on row 6, effectively every line scores as a near-win, so the table is
never written. The search collapses further than that: instrumented at depth 6 on
that position, the whole search is **17 nodes, and the count is identical with and
without move ordering**. `canWinWhite()` answers at the root's children and there
is essentially no tree to explore.

Both of these therefore compared a 17-node no-op against a copy of itself:

| Test | Was asserting |
|---|---|
| `MiniMax - move ordering and TT preserve the search value` | that 4 identical no-op searches agree |
| `MiniMax - quiescence value is search-path invariant (tt/ord)` | the same, with quiescence on |

Both are moved to the midboard position the new regression test uses, at depth 6,
and both gained two **anti-vacuity guards** asserting that each feature actually
changed the node count (`ordNodes != plainNodes`, `ttNodes != plainNodes`). That
is the part worth keeping: an invariance test that does not also prove the
invariant was *exercised* cannot fail, and neither of these could. Confirmed by
restoring the old position and rebuilding: the guard fails with `17 != 17`.

## What was built

`rank.exe refute` mines ONE position-keyed book intended to beat every
deterministic agent on the roster, both colours.

Both sides being deterministic makes each (book, opponent, colour) triple exactly
one game, so beating N opponents is winning 2N specific games rather than 2N
distributions, and finding each is depth-first search over our own moves with the
opponent as a fixed reply function. Not a minimax and not a sampled tournament.

Every trial replays from move 1. The opponent is a fixed function of the game
PATH rather than of the position, since the table is cleared per game and not per
ply, so an alternative move cannot be tried by un-playing the last one.

It is one book rather than 2N because `openerBook` keys on
`positionKey(sideToMove)` alone, with no ply and no path. Two lines needing
different moves from one position cannot both be expressed. The miner avoids that
by construction rather than repairing it afterwards: the book is shared from the
first game onward and every move our side plays is committed immediately, so a
later target inherits an earlier one's choice instead of deriving its own, which
would not agree anyway.

| Stage | What it does |
|---|---|
| 1 | Play every target once with the shared book, oracle as fallback. A win claims ownership of every position on its line. |
| 2 | Repair the losers. Walk each losing line backwards from the deepest of our moves, and at any position no won line owns, re-search with the already-tried moves filtered out of the search root (the same one-shot whitelist `cbook` uses) and replay. Positions a won line owns are left alone, so a real merge conflict surfaces as a `blocked_shared` row rather than quietly breaking a solved line. |
| 3 | Prune to the entries a winning line walks, write the book, audit coverage, then verify through `playOneGame` and the real `openerBook`. |

`--verify-only` runs stage 3 against an existing book without mining or rewriting
it. Exit code 2 unless the book wins every line, on its own, through `openerBook`.

Two self-checks are permanent, both because they caught something here: the miner
compares its own verdict against the `openerBook` replay and warns on
disagreement, and it detects a search that ignores the root whitelist and returns
an excluded move, which would otherwise make stage 2 silently retry the move that
already lost.

## Also fixed, from phase 0

`rankPairGen`'s game loop never called `ttClear()`, unlike `playOneGame` and the
extract replay path. `pairgen --games N` on a deterministic pair produced N
different games rather than N copies of one. No prior `pairgen` result is
re-measured here.

## Changes made

| File | Change |
|---|---|
| `src/ai_minimax.cpp` | `setTTContext` + the searcher context mixed into both TT keys |
| `src/ranking.cpp` | `rankRefute` (stages 1-3, coverage audit, `--verify-only`), `refPlayGame`, `rankPairGen` per-game `ttClear` |
| `src/ranking.h`, `tools/rank_main.cpp` | `refute` entry point, dispatch, usage |
| `tests/test_ai_integration.cpp` | TT searcher-context regression test |
| `Docs/corrections.md` | `TT CROSS-AGENT CONTAMINATION` defect class |
| `Docs/theories.md` | theory 54 |
| `src/CLAUDE.md`, `tools/CLAUDE.md`, `README.md`, `todo.md` | reference updates |

## How to test

```powershell
.\tests.exe "MiniMax - the transposition table never hands one searcher another's score"
.\rank.exe refute --slot 95 --only "ab(deep=4,tt,ord,nodes=200k)@1" --tries 4
```
The refute run should report `audit: 8/8 lines won by the written book on its own,
0 left the book`. Any non-zero "left the book" against a node-budgeted opponent
means the contamination is back.

To see the defect rather than its absence, revert the two `^ s_ttCtx` in
`src/ai_minimax.cpp` and re-run the test.

## The ownership check had a hole, at the write and not the branch

Stage 3's own collision counter fired on the book21 mine: **1 position of 3657
where two winning lines recorded different moves.** A position-keyed book holds
one move per position, so the later line's move overwrote the earlier one's and
the earlier line was broken.

The hole was locatable. Stage 2 gated which position a repair may BRANCH at:

```cpp
if (ow != owners.end() && ow->second > 0) continue;   // shared prefix, not ours to change
```

but on a win it then committed the repair's ENTIRE path unconditionally:

```cpp
for (size_t q = 0; q < g.ourKeys.size(); q++) book[g.ourKeys[q]] = g.ourMoves[q];
```

The check guarded the branch point and not the write. A repair that branches at a
position it is allowed to change can still, further down its new continuation,
cross a position an earlier winner owns and overwrite the move there.

Fixed by validating the whole path before committing. On a win, every position on
the new line is checked against `owners`, and if any owned position would receive
a move different from the one already in the book, the win is refused and the
search falls through to the next candidate move at that node:

```cpp
if (conflict) {
    t.rejected++;
    cout << "    reject: won, but our ply " << badQ << " would overwrite "
         << refHexKey(g.ourKeys[badQ]) << ", owned by "
         << owners[g.ourKeys[badQ]] << " won line(s)\n" << flush;
    continue;                          // fall through to the next candidate move
}
```

Stage 1 never needed this. It writes insert-only (`if (book.find(k) == book.end())`),
so a stage-1 line always agrees with whatever the book already holds. Only stage 2
overwrites.

The check is deliberately conservative in one respect worth stating. `owners` is
incremented and never decremented, so when stage 2 replaces a target's path the
positions on its abandoned path stay marked as owned. That can refuse a repair
that would in fact have been safe. It cannot admit an unsafe one, which is the
direction that matters, and the summary now reports how many wins were refused so
the cost is visible rather than silent.

### Two diagnostics, because the collision count alone could not be acted on

A count says a merge conflict happened. It does not say which lines it broke, and
the guess that followed from it ("the 7 lines that left the book are probably all
downstream of this one position") could not be checked against anything.

**`ovr_ply` / `ovr_key`** measure it directly. During the coverage audit the
replay stands on the same positions the mine recorded for that line, so an
overwrite is observable: the book hands back a move DIFFERENT from the mined one
at a position the line is replaying correctly. The audit now carries the mined
path in and reports the first ply where that happens, with the position's hash.
This is the difference between "these plies are consistent with one shared
position" and "this line was overwritten at this position."

**`oob_key`** records the position hash where the book first fell silent, in the
same 16-digit form the book file uses, so the position can be grepped straight
out of `models/book<N>.txt`.

Both are `-1` / `0` when nothing happened, and `ovr_*` is blank under
`--verify-only`, which loads a book with no mined path to compare against.

### What the distinct oob keys do and do not establish

Re-running the book21 audit with `oob_key` populated gives seven DISTINCT keys
for the seven out-of-book lines. That is not evidence against the shared-collision
explanation, and it is worth being explicit about why, because the naive reading
is the opposite. If several lines cross an overwritten position K, each plays the
same wrong move from K, but each faces a different opponent, so each receives a
different reply and reaches a different successor position. Distinct oob keys are
what the hypothesis predicts. The keys are recorded for future debugging, not as
a test.

The test is `ovr_key` on a fresh mine, which is why the fix is validated by
re-mining rather than by re-auditing the old book.

## The A/B: the fix is behaviour-neutral, and the hypothesis it was built on is refuted

The first attempt at validating the fix compared the new mine (slot 22) against
book21 and looked like a regression: 86 of 95 lines repaired against 95 of 95, 16
lines out of book against 7. That comparison is void, and the reason matters more
than the fix.

**Stage 1 is identical code in both binaries, and it diverged.** book21 ended
stage 1 with 3532 book entries, book22 with 3530, first differing between targets
181 and 200. Nothing in the fix can cause that: the check lives in stage 2, and on
that run it never fired at all (0 rejects, 0 collisions). The miner is simply not
reproducible run to run, and stage 2 is path dependent, so two entries of
difference at the end of stage 1 change which positions are owned and cascade into
nine extra concessions.

Re-running with `--skip-timed`, which drops the 14 wall-clock-budgeted agents and
leaves 210 targets, settles both questions at once. Slot 23 ran on the fixed
binary, slot 24 on a pre-fix binary built from `src/ranking.cpp` at commit
`3d945c2` (sources kept in `build/pre/`), same frozen snapshot, same oracle, two
separate processes:

| | slot 23 (fixed) | slot 24 (pre-fix) |
|---|---|---|
| stage 1 won | 130/210 | 130/210 |
| stage 3 entries kept | 3505 of 3899 | 3505 of 3899 |
| mined won | 203/210 | 203/210 |
| audit won | 202/210 | 202/210 |
| audit left the book | 12 | 12 |
| verify (W-L-D) | 202-8-0 | 202-8-0 |
| mining games | 1736 | 1736 |
| collisions | 0 | 0 |
| wins refused by the check | 0 | n/a |

`models/book23.txt` and `models/book24.txt` are byte-identical apart from the slot
number in their header comment, and the two reports agree on all 210 rows across
every column both versions have.

Both are kept because the pre-fix book is the evidence, but note the consequence:
an agent wearing `book=23` and one wearing `book=24` are the SAME player under
different IDs, exactly like two evaluators differing only in turn weight. Only
slot 23 should ever be rostered.

Three conclusions, and the third is the one that matters:

1. **The miner is exactly reproducible once the wall-clock-budgeted agents are
   excluded.** Two processes, two different binaries, identical output. With them
   included, two runs differ. That localises the irreproducibility to the `time=`
   opponents, consistent with the independent replay of book21, where the only row
   that moved was a `time=150ms` core.
2. **The fix is behaviour-neutral on this workload.** It never fired, so it cost
   nothing and proved nothing. It closes a hole that is reachable in principle,
   and the collision on book21 shows the hole is not theoretical, but that
   collision needed the timed agents' irreproducibility to occur.
3. **The ownership hole does not explain lines leaving the book.** This was the
   hypothesis the whole exercise was built to test, and it is refuted: 12 lines
   left the book with 0 collisions and 0 overwrites on any won line.

### What the 12 out-of-book lines actually are

Seven are conceded lines (`mined=0`), which is expected: a line that was never
solved has no book coverage to leave. The remaining five were mined as wins and
still left the book:

| colour | oob ply | unserved | opponent |
|---|---|---|---|
| W | 16 | 18 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1` |
| B | 9 | 26 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=112,baa2951a,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` |
| B | 11 | 6 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=387,607b64aa,tdleaf_self,lin,shape=30-1)@1` |
| B | 12 | 12 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=459,642147d2,tdleaf_self,lin,shape=129-1)@1` |
| W | 8 | 2 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=261,52cd70f8,tdleaf_self,mlp,shape=129-32-1)@1` |

All five report `ovr_ply = -1`, and that is informative rather than merely
negative. The overwrite check fires when the replay stands on a position the mine
recorded for that line and the book returns a different move. Getting `-1` means
no such position exists: up to the point the book fell silent, OUR moves reproduced
the mined line exactly. The book was not overwritten and our side did not deviate.

What is left is that the line reached a position the book has never seen, and
since our moves matched, the position can only have changed because THE OPPONENT
replied differently in the audit than it did during mining. The opponent is
deterministic and its own inputs are unchanged. The one thing that differs between
the two runs is that during mining our side SEARCHES at unbooked plies and during
the audit it does not.

That is the same shape as the transposition-table contamination fixed earlier in
this campaign: an opponent whose replies depend on our side having searched. It is
a deduction from `ovr_ply = -1` and not a direct measurement, so it is recorded as
the leading hypothesis with the test named in Future Work, not as a result.

### The ovr check needed a gate, found by using it

Its first run reported "4 line(s) were overwritten". All four were `mined=0`. A
conceded target's stored `keys`/`moves` are its last FAILED attempt, which was
never written to the book, so every difference against the book is expected and
means nothing. Gated to `t.status == 1`. The three surviving reports on slot 23,
before the gate landed, are the same false positive and should be read as zero.

Worth stating as a general point: a diagnostic added to explain a defect is itself
untested code, and the first thing it reported here was an artifact. It was caught
only because the rows it flagged were checked against another column rather than
believed.

## The book21 audit reproduces, except on the timed cores

Replaying `models/book21.txt` a second time through `--verify-only`, against the
same frozen roster snapshot, is an independence check on every number quoted from
the first run.

| Measure | Mine run (2026-08-29) | Independent replay | Agrees |
|---|---|---|---|
| audit won | 237/238 | 237/238 | yes |
| audit left the book | 7 | 7 | yes |
| verify (W-L-D) | 235-3-0 | 236-2-0 | **no** |
| `time=` targets won | 26/28 | 27/28 | **no** |

Exactly one row of 238 changed: `ab(deep=6,tt,ord,time=150ms)@2.learned(model=111,
78ef6974,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` as Black,
which flipped `L` to `W` and moved its own `oob_first` from ply 13 to ply 16. It is
one of the two wall-clock-budgeted cores already known not to replay.

The 236 non-timed rows reproduced exactly, including all seven out-of-book plies
and the identity of every line in that set. So the memorization result is stable
under replay and the instability is confined to the cores that cannot be mined by
construction. It also means the verified total for a timed line is a coin flip
between runs and should never be quoted to the unit.

## The book transfers: coverage is fallback-independent, so a cheap wearer memorizes the same lines

Every number in the headline above was measured with the MINING ORACLE as the
fallback brain, `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2`. That agent is
reference class under `ranking/CHAMPION.md` because of its brain, ten times the
standard node budget, so the headline as stated describes an agent that can never
hold a title. The question that decides whether the book is worth anything to the
ladder is whether it still covers the same lines when a CHEAP brain wears it.

The mechanism predicts it should. What a book agent falls back to is the wearer's
own brain, and the brain is consulted only where the book is silent. On a line the
book covers end to end the brain is never called at all, so the game should not
depend on which brain is sitting behind the book.

Tested by replaying `models/book21.txt` through `--verify-only` with three
different brains, same frozen roster snapshot, same 238 targets. Both the audit
fallback (`--oracle`) and the verification agent (`--wearer`) were set to the brain
under test:

| Wearer | node-budgeted fully covered | covered and won | time= coverage differs |
|---|---|---|---|
| `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2` (as mined) | 205/210 | 205 | baseline |
| `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2` | 205/210 | 205 | 3 rows |
| `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=459,642147d2,tdleaf_self,lin,shape=129-1)@1` | 205/210 | 205 | 3 rows |

**Across the 210 node-budgeted targets, coverage differs on 0 rows between all
three brains.** Two different evaluator families and a 10x node-budget difference
produce identical coverage. The book carries the same 205 lines on a standard
`deep=6,nodes=200k` head that it carries on the d8/nb2m oracle.

Every coverage difference is on a `time=150ms` target, all three of them, and
those are the cores already measured as non-replaying. The two `deep=6` wearers
differ from each other on exactly one row, also a timed one. This is the third
independent measurement pointing at the same 28 targets and is consistent with
theory 57.

Verification results DO differ across brains, on 4 node-budgeted rows, and all
four are lines that left the book. That is the expected division of labour: the
book decides the covered lines, the brain decides the rest.

### What this means for certification

The number to quote for a TARGET-CLASS book agent is **205 of 210 node-budgeted
lines won by the book alone**, on `ab(deep=6,tt,ord,nodes=200k)@2` with either
evaluator tested. Not 230 of 238, which is the reference-class d8/nb2m wearer's
figure and includes the 28 timed targets that do not replay.

This is the result the campaign was aimed at: the lines are carried with LESS live
compute than the oracle that found them, by construction, since on a covered line
the wearer does not search at all.

It does not make the agent certifiable as a champion, for two reasons that are
independent of the above and are not fixed by a cheaper wearer:

1. **No category admits it.** `CHAMPION.md`'s divisions are openless (no
   `.opener(...)` and no `.dil(...)` segment), opener8 (`.opener(rand,moves=8)@1`)
   and dil20 (`.dil(prob=20)@1`). An agent carrying `.opener(book,book=21)@1`
   matches none of the three, and the file says so directly: any other opener
   combination, the retired 4-ply and 8-ply book categories named explicitly, is
   ladder and study data holding no title. The book divisions were deferred
   pending "a self-maximizing mining methodology that hasn't been designed yet,
   not a fixed-pair replay". This miner IS a fixed-pair replay against a frozen
   roster, so by the deferral's own wording it does not yet clear that bar.
   Reviving those categories is a design decision, not a consequence of this
   result.
2. **The book was mined against the pool it would be rated against.** All 119
   deterministic agents in the fit are agents the miner had access to. A pooled
   Elo from that fit measures memorization of this specific pool. The agent is
   also maximally intransitive: near-perfect against those 119, no coverage at all
   against the 52 dilution and 66 random-opener agents, which never reproduce a
   line. One Bradley-Terry strength parameter averages two populations that behave
   nothing alike, and describes neither.

The measurement that WOULD be a strength claim is a held-out mine: mine against a
subset of the roster, rate against all of it, and report the held-out agents
separately. Nothing run so far distinguishes "this book generalises" from "this
book memorised 205 specific games".

## Running a long mine while another session works the same tree

Two mines were lost to collisions with a concurrent session working in the same
working tree and the same binaries, so the isolation below is not optional
hygiene, it is what makes a multi-hour mine survivable.

| Hazard | What happened | Mitigation |
|---|---|---|
| The other session rebuilds `rank.exe` | A running exe is locked on Windows, so their link fails until something clears the lock. The mine died at 40/238 with no book. | Run the mine from a private copy, `rank_refute.exe`. Neither side can block or kill the other. |
| `ranking/roster.txt` is rewritten mid-run | The `ab` explorer was bumped to `@2` and all 217 roster lines rewritten 13 seconds after a mine started, leaving its output naming `@1` identities that no longer exist. | Snapshot the roster and pass `--roster ranking/roster_refute_snapshot.txt`. |
| Shared output paths | Not hit, but `refute` writes `models/book<N>.txt`, and book slots are immutable under their number. | Confirm the slot is free before starting. `refute` already refuses to overwrite without `--force`. |

The general rule: a long-running job in a shared tree should depend on a private
binary and a private copy of every input it reads, so that the only shared thing
left is CPU.

## A module version bump stales every hardcoded ID

The `ab` bump to `@2` re-canonicalised every ID wearing the alpha-beta explorer,
and a hardcoded `"@1"` then fails the canonical check and refuses to start. There
were 129 such literals in the tree (80 in `tests/test_ranking.cpp`, 41 in
`tools/`, 8 in `src/`). Two of the `src/` ones were live defaults in commands
added by this work, `refute`'s default oracle and `determinism`'s default probe,
and both are now composed from the module registries through new
`rkExplorerVersion` / `rkEvalVersion` helpers rather than written out. Anything
that needs to NAME a specific agent in code should compose it the same way.

## Open, and deliberately not decided here

**The roster and the stored history.** 169 of 217 active agents carry `tt`, so
every stored game between two of them was played under the defect and the current
binary will not reproduce it. Whether that warrants a code-version bump on the
`ab` explorer segment is a developer decision with a large consequence: bumping
re-identifies every alpha-beta agent, including the `tt`-free ones whose play did
not change, and retires the roster's Elo history. Whether the affected games get
re-played, discarded, or kept with a banner is the same decision's other half.

## Future Work

- **The Elo consequence of the fix is unmeasured.** The cheap version is to
  replay a slice of the store with the current binary and count how many stored
  `tt`-vs-`tt` games no longer reproduce their stored result. That number is the
  input to the roster decision above, and it is a few minutes of compute.
- **A DEFECT is indicated here, and it is not a theory.** Filed as a defect in
  `todo.md`, not in the theory log, because a deterministic agent that plays
  differently across two runs of the same position is broken whatever the cause.
  Only the mechanism is an open question, and that is what theory 56 now holds.
  The distinction is worth keeping: an entry in the theory log reads as a claim
  that might settle either way, which is the wrong frame for a bug and makes
  fixing it look optional.
  The indication: all five won-but-out-of-book lines report `ovr_ply = -1`, so
  our moves reproduced the mined line exactly up to the point the book fell
  silent and the book was not overwritten. The line still reached a position the
  book had never seen, which leaves the opponent's reply as the only thing that
  can have changed. Every step of that is inference from a negative result, and
  the opponent replying differently has never been watched directly, so the
  defect is INDICATED and not confirmed.
  Confirm it in one run before doing any mechanism work: play the same
  deterministic opponent from the same position twice, once with our side
  searching and once with our side replaying book moves, and diff its chosen move
  per ply. Then bisect what state that ply reads. Scope the blast radius in the
  same pass: in ordinary ranked play both sides search, so the asymmetry does not
  arise, but it arises for every book-wearing agent while in book and plausibly
  every random-opener agent during its opener, which is 77 `.opener(...)` rows in
  `ranking/roster.txt`.
- **Whether the ownership check ever fires is unknown.** It is correct by
  construction and behaviour-neutral on every workload run so far, which means it
  is also untested in the only way that counts. A targeted test would construct
  two winning lines that genuinely want different moves at one shared position
  and assert the second is refused rather than committed. Without it the check is
  a guard nothing has ever tripped.
- **The irreproducibility is localised to the `time=` agents but not explained.**
  Excluding them makes two separate processes on two different binaries produce
  byte-identical books. Including them makes two runs differ by two book entries.
  That is consistent with wall-clock budgets alone, but it has not been separated
  from any other property those 14 agents share.
- **The 5 brain-assisted wins are not memorization and should not be counted as
  such.** They left the book and the wearer's search finished the game. Whether
  they are memorizable at all is unknown: they may need a deeper oracle, more
  `--tries`, or they may be genuinely unreachable through a single position-keyed
  book because of the merge-conflict limit.
- **`model=111` and `model=113` cannot be mined by construction.** Both are
  cost-flagged wall-clock cores that do not replay, so no book can hold a line
  against them. Either exclude them from the target set explicitly or give the
  miner a reproducibility pre-check, rather than letting them surface as
  miner-versus-`openerBook` disagreements that look like tool defects.
- **`bookgen` is only partly repaired by this.** Its books are mined from stored
  games in which the line owner DID search, and are then worn by an agent that
  does not. The fix stops the opponent reading our entries, but the owner's own
  entries still shaped the moves that got mined, so this remains a second
  candidate explanation for theory 38's book collapse alongside position novelty.
- **Stage 2's ordering is unstudied.** Targets are repaired in roster order, and
  the shared prefix is therefore fixed by whichever targets happened to be mined
  first. Hardest-first would plausibly concede fewer lines. Untested.

## Ideas This Inspired

- **A "would this replay?" column on the store.** The fix makes a stored game's
  reproducibility a property worth recording rather than rediscovering. An 8-byte
  per-ply trace hash per row would make it a query.
- **Mining as a general instrument for hidden state.** The reason this defect
  surfaced here and nowhere else is that playback removes one side's search
  entirely. Any shared mutable state that a search touches is detectable the same
  way, so a "mine a line, then replay it without searching" harness is a
  general-purpose test for search-state leaks, not just a book-building tool.
- **Give each agent a real private table.** The context XOR is the minimal fix and
  keeps one allocation, but it also means two agents contend for the same slots
  and evict each other. Separate tables would remove the contention and make the
  per-agent table size a tunable, which is itself a strength axis worth rating.
