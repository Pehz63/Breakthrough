# Book Opener Audit -- Results

Session 2026-07-26. Not driven by a prior plan document, so there is no paired
plan file: the work started from a developer question ("how does the book work,
does it depend on the agent or the evaluator, what does it work best against, did
it improve Elo, do the docs follow comparison hygiene") and expanded into an audit
plus four new mined books.

## What the book actually is

`rank.exe bookgen` (`rankBookGen`, `src/ranking.cpp`) replays every stored game
between `--a` (the line owner) and `--b` (the target), and for each game that A
WON records the move A played at each of its first `--plies` half-moves. The
output `models/book<N>.txt` is a flat map, one line per position:

```
<positionKey(sideToMove, false).hash as 16 hex digits> <sx> <sy> <dx>
```

The `book` opener (`openerBook`, `src/ai_random.cpp`, registered in `g_openers[]`
and selected by the ID segment `.opener(book,<N>)@1`) hashes the live position,
looks it up, validates the stored move is still legal, plays it, and returns
true. On any miss it returns false and the agent's own brain moves.

Three consequences follow from that design, and they answer the question directly:

- **The lookup does not depend on the agent or the evaluator at all.** The key is
  the canonical position hash plus side to move, the same key the transposition
  table uses. Any agent wearing `.opener(book,N)` gets the stored move in any
  position that matches, regardless of its core.
- **The VALUE depends on the wearer**, because the book is only as good as what
  follows it. A mined move wins because the miner's own search stood behind it at
  every later ply (theory 14's brain-portability failure).
- **The value depends on the opponent even more strongly**, and this is the part
  that was not previously written down. The book has no response tree. It stores
  one move per position. It keeps firing past move 1 only while the opponent
  reproduces the replies it made in the mined games. The first deviation ends the
  book for the rest of the game.

So the book is a memorized line, not an opening theory. It is worth something
exactly when the game is replayed from a fixed start against an opponent that
repeats itself, and that condition is precisely what this project's rating
instrument guarantees. `rank.exe play` / `run` has no opening-diversification
flag, so **every rated game in `ranking/matches.jsonl` began from
`boards/board1.txt`.**

## Defect 3: nominal games are not distinct games

This surfaced while checking whether the book's win records were real, and it is
the most consequential finding of the session. It is not book-specific.

`rankSchedule` gives every game a self-contained seed via `gameSeed`, but `rand()`
is only ever consumed by dilution and by random-move agents. **A pair with no
`dil(...)` segment and no `rand` opener consumes no randomness at all**, so the
seed is inert and every game with the same colour assignment is byte-identical.
`playOneGame` also never calls `ttClear()` (unlike the pairgen replay path and the
posgen ladder, which both do), so for a `tt` head the only residual variation is
which games happened to run earlier in the same process. Theory 19 mechanism b is
acting as the instrument's entire source of sample diversity.

Measured over `ranking/games.tsv`, counting distinct game trajectories per pair
(colour assignment + ply count + result + both node totals, which node counts make
effectively unique):

| Statistic | Value |
|---|---|
| Pairs with >= 16 stored games | 190 |
| Median distinct trajectories / games played | **0.438** |
| Worst observed | 32 games -> **2** distinct trajectories |
| Head of every worst case | `ab(d6,ord,nb200k)` (no TT, therefore no cross-game state) |
| Mean ratio, no book involved | 0.574 |
| Mean ratio, one side wears a book | 0.529 |

The book hypothesis for the collapse was wrong: books do not cause it, the medians
are identical at 0.438 either way. The cause is determinism.

**Null control.** Two identical deterministic agents at the standard start,
`classic` wearing a nonexistent book slot (so the opener can never fire) versus
bare `classic`, 64 games: **A won 32-0 as White and 1-31 as Black.** The result is
decided by colour, not skill. 64 stored rows carry the information of 2 games.

**Consequence for every error bar in the project.** The Bradley-Terry fit treats
each stored row as an independent trial, so `pm` in `ratings.tsv` and
`standings.tsv` is understated by roughly `sqrt(1/0.438)`, about 1.5x, for a
typical pair, and far more for the deterministic no-TT pairs. A margin that reads
as 2 standard errors may be under 1.

## The champion's certification does not reproduce

The champion is `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1`.
Its record against its own bookless self at the same head reads 29-3 (91%) in the
store. Decomposed by run, and re-measured fresh on 2026-07-26 with identical
agents, board, and code:

| Source | Record | Rate |
|---|---|---|
| Store, run `20260718T175325Z` | 5-3 of 8 | 62.5% |
| Store, run `20260718T175433Z` | 24-0 of 24 | 100% |
| Fresh, 8 separate 8-game processes | 5-3 in every one of the 8 | 62.5% |
| Fresh, one 64-game process | 34-30 | 53% |

All eight fresh shards returned exactly 5-3, which independently confirms seed
inertness. The spread from 53% to 100% comes entirely from cross-game TT state.
The certification rests on the 24-0 run, the top of that range.

Two further corrections to the certification's supporting numbers:

- 25-7 (78%) over s98 is **8-4 (67%) on 12 distinct games**.
- 27-5 (84%) over the d8 oracle is **12-2 (86%) on 14 distinct games**.

What survives is the contrast that motivated theory 33 in the first place: bare
classic scores 28% against s98 over a genuine 32 distinct games, and the
self-booked version scores 67% over 12. Something real happens at the fixed start.
Its size and its reproducibility are what fail.

## Did the book improve Elo, and do the docs follow hygiene?

Read from `ranking/standings.tsv`, all rows active, one head
(`ab(d6,tt,ord,nb200k)@1`), loadout-matched bare-versus-equipped:

| Core | Bare | + book1 (oracle-mined, foreign) | + book2 (classic self-mined) |
|---|---|---|---|
| `classic(t1,c4,w0,l0)@2` | 990 +/- 10 | 982 +/- 11 (**-8**) | 1114 +/- 12 (**+124**) |
| `learned(s98,5801570e)@1` | 1043 +/- 11 | 1066 +/- 12 (**+23**) | not rated |

So yes for one core and one book (+124), and no for everything else. Both foreign-
book numbers are inside their error bars.

**The docs did not follow hygiene, in three specific places.** All are now
corrected in `Docs/theories.md`.

1. **Theory 33 quoted the wrong search head.** "32-0 against its own bookless
   self" is not its bookless self: that record belongs to
   `ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`, a different head with no TT. At
   the shared head the store reads 29-3. This is defect 2, committed inside the
   entry that certified the reigning champion.
2. **Theory 14 claimed the foreign book made agents WORSE than bookless.** Not
   supported. 7-25 nominal is 4-10 (29%) on 14 distinct games, against bare
   classic's 9-23 (28%) on a genuine 32. The Elo deltas are inside their error
   bars, and in the current fit one has flipped sign: s98+book1 now reads 1066
   +/- 12 against bare s98's 1043 +/- 11 (+23, about 1.4 combined SE) where the
   original fit read -16. The theory is still refuted, because it predicted a book
   would let a d6 agent BEAT the champion and no version of the numbers shows a
   gain, but the supported claim is a null, not a harm.
3. **No document counted distinct games.** Every book record in the project was
   quoted as its nominal row count.

Theory 33's status is downgraded from "Confirmed as a major result" to
"Unresolved". `ranking/CHAMPION.md` carries a CERTIFICATION UNDER CHALLENGE note.
The throne is not vacated, because no challenger has been certified against it
under a corrected instrument.

## What the book works best against

Per-opponent records for the booked agents, restricted to opponents with >= 16
games against all five agents (the boosted contender set, 480 games each), with
nominal rates. These are the raw store numbers and inherit defect 3, so read them
as structure rather than as magnitudes.

| Aggregate over the 15-opponent contender set | Record | Rate |
|---|---|---|
| classic bare | 272-208 | 56.7% |
| classic + book1 (foreign) | 272-208 | **56.7%** (identical to bare) |
| classic + book2 (own) | 350-130 | **72.9%** |
| s98 bare | 294-186 | 61.2% |
| s98 + book1 | 295-185 | **61.5%** (identical to bare) |

The foreign book is worth exactly nothing in aggregate for either core, to the
row. The own book is worth +16pp. Within that aggregate the own book's gains are
concentrated on the classic family and the oracle (32-0 against no-TT classic,
27-5 against the d8 oracle, 91-97% against the aspiration variants) and it loses
badly to quiescence-equipped opponents (12-20 against `qs` classic, 2-30 against
`qs` s98). The `qs` collapse looked like a headline finding until the distinct-game
collapse was applied: 2-30 is 2-12 (14%) on 14 distinct games against bare
classic's 3-9 (25%) on 12, which is not a separation. Reported here as a caution
about the nominal numbers rather than as a result.

`s98 + book1` deserves a note. Book1's line owner is the d8 oracle and its target
is s98, so s98 wearing book1 is wearing the book mined to BEAT it. It also shows
full trajectory diversity (32 of 32 distinct against every opponent) where the
other booked agents collapse, which is consistent with the book almost never
firing for it: to reach the oracle's to-move positions, s98 would need its own
opponent to play as s98 did. Zero lift and full diversity are the same
observation.

## Four new books, and own versus borrowed

Mined with `rank.exe bookgen`, zero code changes. Each core's own book comes from
its own wins against the target named:

| Slot | Line owner | Target | Entries | Kept replays | Replay drift |
|---|---|---|---|---|---|
| `models/book3.txt` | `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1` | champion | 24 | 16 of 32 | 0 |
| `models/book4.txt` | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` | `classic(t1,c4,w0,l0)@2` | 519 | 25 of 32 | 12 |
| `models/book5.txt` | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1` (dist MLP) | champion | 145 | 6 of 8 | 3 |
| `models/book6.txt` | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1` | champion | 162 | 7 of 8 | 1 |

**Book size measures the mining pair's diversity, not its skill.** Book3 is tiny
(24 entries, 0 drift) because its no-TT head is fully deterministic: its 16 wins
collapse to about 7 distinct games that share one opening prefix. Book4 is large
(519 entries, 12 drifted) because the `tt` head's cross-game state made its 25
wins genuinely varied. This is defect 3 reappearing as a property of the mined
artifact.

### The measurement

Each core played its own book's target under two conditions, via `rank.exe
pairgen`. `open0` is the fixed standard start, exactly what the rating instrument
does. `open8` is `--open-plies 8 --open-side both`, eight uniform-random opening
half-moves by both sides, which makes `rand()` live and every seed a genuinely
different game. Rates are for the first-named agent.

**`adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1` at head
`ab(d6,ord,nb200k)@1`, versus the champion.** Own book = book3.

| Loadout | Fixed start (32 nominal) | Diversified (64 real) | Change |
|---|---|---|---|
| bare | 16-16 (50%) | 32-32 (50%) | +0pp |
| + own (book3) | 16-16 (50%) | 36-28 (56%) | +6pp |
| + borrowed (book2) | **32-0 (100%)** | 37-27 (58%) | **-42pp** |

**`learned(s98,5801570e)@1` at head `ab(d6,tt,ord,nb200k)@1`, versus bare
`classic(t1,c4,w0,l0)@2`.** Own book = book4.

| Loadout | Fixed start (32 nominal) | Diversified (64 real) | Change |
|---|---|---|---|
| bare | 22-10 (69%) | **42-22 (66%)** | -3pp |
| + own (book4) | **32-0 (100%)** | 38-26 (59%) | **-41pp** |
| + borrowed (book2) | 29-3 (91%) | 37-27 (58%) | **-33pp** |

**`learned(s3,68364898)@1` at head `ab(d6,tt,ord,nb200k)@1`, versus the
champion.** Own book = book6.

| Loadout | Fixed start (32 nominal) | Diversified (64 real) | Change |
|---|---|---|---|
| bare | 22-10 (69%) | **34-30 (53%)** | -16pp |
| + own (book6) | **26-6 (81%)** | 31-33 (48%) | **-33pp** |
| + borrowed (book2) | 19-13 (59%) | 28-36 (44%) | -16pp |

**`learned(s111,78ef6974)@1` (dist MLP) at head `ab(d6,tt,ord,nb200k)@1`, versus
the champion.** Own book = book5. Only 16 games per cell, 370 ms/move.

| Loadout | Fixed start (16 nominal) | Diversified (16 real) | Change |
|---|---|---|---|
| bare | 10-6 (62%) | 4-12 (25%) | -38pp |
| + own (book5) | **16-0 (100%)** | 8-8 (50%) | **-50pp** |
| + borrowed (book2) | 6-10 (38%) | 4-12 (25%) | -12pp |

### Reading the table

**Own-book lift at the fixed start is large for three of four cores** (+31pp for
s98, +38pp for s111, +12pp for s3) and **exactly zero for the hill-climbed `adv`
core**. The zero is the informative case: that core sits at the no-TT
`ab(d6,ord,nb200k)` head, so it is fully deterministic with no cross-game state,
and a book of its own wins can only ever replay what its own brain already
chooses. A self-mined book is a no-op for a reproducible agent by construction.
Three of the four cells reaching exactly 100% (32-0, 32-0, 16-0) is itself a
signature of the degeneracy rather than of strength.

**Every one of those gains is gone under diversified openings.** The own-book
column changes by -41pp (s98), -50pp (s111), and -33pp (s3). Measured against
each core's own diversified bare baseline, the own book is worth -7pp for s98,
-5pp for s3, +6pp for `adv`, and +25pp for s111 at n=16. The three well-powered
cells are nulls or slightly negative. Bare s98 is the strongest s98 loadout once
openings vary.

**Own versus borrowed is not a clean story either.** At the fixed start the own
book beats the borrowed one for s98 (100% vs 91%), s3 (81% vs 59%), and s111
(100% vs 38%), which is the direction theory 33 predicts, but for the `adv` core
it loses by 50 points (50% vs **100%**). One 50pp reversal in four cases means
"books are core-specific" is not a rule. Under diversification the own/borrowed
gap is within noise everywhere.

### A separate observation about the learned cores

Both learned cores and the dist core lose substantial ground against the champion
when the opening is randomized, beyond anything the book explains: s111 goes from
62% to 25% bare, s3 from 69% to 53% bare. The champion's own book also dies under
diversification, so this comparison is close to learned-core-versus-bare-classic
in both columns, and the classic chip counter is the one that gains.

Flagged as a **hypothesis, not a finding**: these models were trained on positions
drawn from standard-start games, so uniformly random openings are out of
distribution for them while a material counter is opening-agnostic by
construction. Nothing here tests that mechanism. It would be tested by training a
value model on diversified-opening self-play and re-running the same comparison.

## Conclusions

## Conclusions

1. **A book's measured lift is a fixed-start artifact.** Every large book effect
   in this project evaporates when the opening is varied. This is theory 38.
2. **"Books are core-specific" is refuted in both directions.** At the fixed
   start the BORROWED champion book beat the hill-climbed agent's own book
   (32-0 versus 16-16). The self-mined-book story from theory 33 is not the
   mechanism.
3. **A self-mined book is a near no-op for a deterministic agent by
   construction.** In book it replays what its own brain would have chosen
   anyway. It can only add value where cross-game state would otherwise have made
   the agent deviate from its own best line, which is exactly the explanation
   theory 33 offered and exactly the kind of variance the instrument should not be
   rewarding.
4. **The instrument, not the book, is the thing to fix.** A rating ladder that
   plays every game from one position, with a seed that does nothing, cannot
   distinguish "found a good line once" from "is stronger".

## Caveats on these numbers

- `open8` results are genuine independent games. `open0` results are not: for a
  deterministic pair the effective sample is 2, one game per colour, and the
  apparent spread across a 32-game batch is TT pollution ordering. Both are
  reported so the contrast is visible, but only `open8` carries statistics.
- The `open8` comparisons are 64 games (16 for the s111 dist core, whose leaf
  costs 370 ms/move). A 6pp difference at n=64 is well under 1 SE. Where a book
  reads a few points above or below bare under `open8`, that is a null, not a
  small effect. The load-bearing claim is the collapse from the fixed-start
  number, which is 30-50pp and far outside noise.
- No new Elo was fitted. These are direct pair records at one head with matched
  loadouts, which is the right instrument for a lift and is immune to cross-fit
  scale drift, but it is not a pooled rating. The four new books are deliberately
  NOT in `ranking/roster.txt`: adding deterministic booked identities would add
  stored rows without adding distinct games.
- `--open-plies 8` is one arbitrary diversification depth. The champion-versus-
  classic probe was also run at 4 (35-29, 55%) and behaves monotonically, but no
  sweep over K was done.
- The 8-game fills that produced books 5 and 6 are below this project's own
  32-games-per-pair threshold for any conclusion, which is why the s3 and s111
  bare-versus-champion results below are flagged for boosting rather than acted
  on.

## Future Work

- **Settle the champion question properly.** Tethered to: the CERTIFICATION UNDER
  CHALLENGE note. Implement option 1 from the `todo.md` diversity item (`--open-
  plies` on `rank.exe play`/`run`), tag the diversified games as a separate board
  or store so they never mix with the fixed-start rows, and refit. Prediction from
  theory 38: the champion drops to roughly bare classic, and the throne passes to
  a learned core. This is the test that would confirm or refute conclusion 1 at
  the pooled level rather than pairwise.
- **Boost s3 and s111 against the champion to 32 games/pair.** Tethered to: the
  8-0 and 7-1 fills. Either they are top-of-table contenders that the current fit
  is under-rating, or they invert at 32 games as two previous preliminary reads
  already have. Cheap for s3, expensive for s111.
- **Measure how deep the book actually stays in book.** Tethered to: the "memorized
  line, no response tree" claim, which is currently an inference from the code plus
  the collapse, not a direct measurement. Instrument `openerBook` with a per-game
  in-book ply counter and report its distribution by opponent. Would directly
  confirm that the champion's book dies on the opponent's first deviation.
- **Separate the two mechanisms in theory 33's explanation.** Tethered to: the
  claim that a self-mined book adds value only by suppressing cross-game-state
  variance. Adding `ttClear()` per game and re-running the champion versus bare
  classic would test it: if the book's fixed-start advantage vanishes when the
  bookless agent is made reproducible, the book was only ever compensating for TT
  noise.
- **Re-audit the remaining flagged documents for defect 3.** Tethered to: the
  `[Now]` re-evaluation item, whose banner text currently names only defects 1 and
  2. Every record quoted anywhere in `plans/` needs a distinct-game count.

## Ideas This Inspired

- A book with a **response tree** rather than one move per position. `pairgen`'s
  `--branch-tries` machinery already explores opponent deviations, so it could mine
  a shallow tree and the book would survive a deviation instead of dying on it.
  That would be an actual opening book rather than a replayed line.
- **Colour-split reporting as a standard column.** The null control's 32-0 / 1-31
  is invisible in an aggregate 33-31. Any pair whose colour split is that extreme
  is a pair with no measurable skill difference, and `report.md` could flag it
  automatically.
- **An opening-diversity axis in the roster.** Rather than one global
  `--open-plies`, agents could be rated at several diversification depths, turning
  "strength at the standard start" and "strength across openings" into two
  reported numbers instead of a hidden methodological choice.
- **Use the fixed-start degeneracy deliberately.** A deterministic pair playing one
  game per colour is a perfect regression test: any change to search or eval that
  alters a stored trajectory is detectable exactly, with no statistics. That is a
  cheap engine-level equivalence harness hiding inside the ranking store.
- If the throne survives on fixed-start play, the community-competition framing
  needs an explicit rule about memorized lines, because the current criterion
  rewards them and a human entrant would reasonably call that scripted.
