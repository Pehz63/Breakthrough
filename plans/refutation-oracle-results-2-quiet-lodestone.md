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
loss**. Wall-clock-budgeted targets overall went 26/28.

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

## The ownership check has a hole, at the write and not the branch

Stage 3's own collision counter fired: **1 position of 3657 where two winning
lines recorded different moves.** A position-keyed book holds one move per
position, so the later line's move overwrote the earlier one's and the earlier
line is broken.

The hole is locatable. Stage 2 gates which position a repair may BRANCH at:

```cpp
if (ow != owners.end() && ow->second > 0) continue;   // shared prefix, not ours to change
```

but on a win it then commits the repair's ENTIRE path unconditionally:

```cpp
for (size_t q = 0; q < g.ourKeys.size(); q++) book[g.ourKeys[q]] = g.ourMoves[q];
```

So the check guards the branch point and not the write. A repair that branches at
a position it is allowed to change can still, further down its new line, cross a
position an earlier winner owns and overwrite the move there. The fix is to
validate the whole path against `owners` before committing and reject the repair
if it would overwrite an owned position with a different move, falling through to
the next candidate move rather than silently breaking a solved line.

**Untested hypothesis, flagged as such:** the 7 lines that left the book may all
be downstream of this single overwritten position, which would make the two
symptoms one defect. Their `oob_first` plies are 8, 8, 9, 11, 12, 13 and 16,
which is consistent with several lines crossing one position at different depths,
but the miner does not report the colliding position's key, so this is a guess
and not a measurement. Printing that key is the one-line change that would settle
it.

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
- **Close the ownership hole and re-mine.** The fix is described above: validate
  a repair's whole path against `owners` before committing it, rather than only
  its branch point. Until then 1 position of 3657 is overwritten and at least one
  winning line is broken by it. A re-mine would also settle whether the 7
  out-of-book lines are downstream of that one position.
- **Print the colliding position's key.** One line of output, and it converts the
  untested hypothesis above into a measurement.
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
