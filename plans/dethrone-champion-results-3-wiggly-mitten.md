# Dethrone the Champion, Phase 2 (Offline Refutation Book) -- Results

Companion to `dethrone-champion-plan-1-wiggly-mitten.md` (phase 2). Sequel to
results-1 (phase 0, the s98 certification) and results-2 (phase 1,
quiescence). Session date 2026-07-17. Tests theory 14.

## Headline

**The offline refutation book does NOT dethrone s98, and the naive theory-14
premise is refuted.** A book of the d8/nb2m oracle's winning replies against
s98 (553 positions mined from 29 oracle wins) makes both a strong (s98) and a
weak (chip counter) live brain rate BELOW their bookless selves and does not
beat s98 head-to-head:

- **s98 + book: 1059 +/- 14 vs plain s98 1075 +/- 13** (-16), head-to-head
  **14-18** (a slight loss, essentially a mirror). The book is a wash-to-slight
  negative on the strong brain.
- **chip counter + book: 967 +/- 13 vs plain chip counter 983 +/- 12** (-16),
  and against s98 -- the very agent the book was mined to beat -- it went
  **7-25 (0.22), WORSE than the bookless chip counter's 0.28** (phase 0). It
  even loses to its own bookless self 13-19.

The mechanism is the interesting part and is itself a theory-14 result: the
book's stored moves only win with the oracle's deep follow-up behind them, and
**the target is not reproducibly deterministic across runs** -- persistent
cross-game search state (theory 19b) makes s98 deviate from the mined lines, so
the booked agent drops out of book into a position its own weaker brain must
finish. The one place the book "wins" (chip-counter+book beats the oracle
20-12) is an artifact of replaying the oracle's own positions back at it, not a
refutation of the target.

## What was built

- **`rank.exe bookgen`** (`rankBookGen`, `src/ranking.cpp`; CLI in
  `tools/rank_main.cpp`): replays every stored game between two agents, and from
  A's WINS keeps the move A played at each position in the first `--plies`
  half-moves, keyed by `positionKey(sideToMove,false).hash`. Recovers the move
  by diffing a pre-move board snapshot against the post-move board
  (`diffMoveFromSnap`). Writes `models/book<N>.txt` (a `#` provenance header +
  `<hash hex16> <sx> <sy> <dx>` lines); first-seen wins on key conflicts, and
  replay-drift games (result != stored) are counted but their A-won lines are
  still kept.
- **`book` opener** (`g_openers[]`, `src/ai_random.cpp`): plays the stored
  reply for the current position from `models/book<arg>.txt` at any ply the
  position matches, then hands off to the brain. Lazy per-process load,
  bounds+`tryMoveQuick*` guarded, `.opener(book,<arg>)@1` ID segment. The book
  file is NOT hashed into the ID (unlike `learned()`), so a book slot is
  immutable by convention -- a regenerated book gets a new slot.
- **Tests** (764 assertions / 81 cases pass): codec round-trip for
  `.opener(book,N)@1`, and a book-opener behavior test (plays the stored
  capture in book, defers out of book, defers on a missing file, ignores an
  out-of-bounds entry).

### How to test

- `.\tools\run_tests.ps1 -Build`
- `.\rank.exe bookgen --a "<oracle id>" --b "<s98 id>" --plies 60 --out models/book1.txt`
  prints "Kept 29 A-won replays ... 553 book entries".
- Roster `...learned(s98,5801570e)@1.opener(book,1)@1` validates via
  `rank.exe check` and plays.

## Measurement (full-roster refit, 2026-07-17, all contender pairs at 32 games)

| Elo | +/- | agent |
|---|---|---|
| 1146 | 14 | oracle d8/nb2m (reference) |
| 1078 | 14 | s98 + quiescence |
| **1075** | 13 | **s98 (champion, retained)** |
| 1061 | 13 | s96 |
| 1059 | 14 | s98 + book |
| 1019 | 12 | classic d6 ord |
| 1014 | 12 | adv(c77,d-2,b1) |
| 1000 | 12 | adv(c75,h3,r2) |
| 996 | 13 | classic + quiescence |
| 983 | 12 | classic d6 tt,ord (phase-0 dethroned incumbent) |
| 967 | 13 | classic + book |

Book-agent head-to-heads (32 games each):

| agent A | vs | result (A) | note |
|---|---|---|---|
| s98+book | plain s98 | 14-18 (0.44) | book barely changes the mirror |
| s98+book | oracle | 3-29 (0.09) | same as plain s98 (0-32); book does not help |
| s98+book | classic+book | 31-1 | |
| classic+book | oracle | **20-12 (0.62)** | the one "win": oracle's own moves replayed at it |
| classic+book | s98 | **7-25 (0.22)** | WORSE than bookless classic (0.28) vs the mined target |
| classic+book | plain classic tt,ord | 13-19 (0.41) | loses to its own bookless self |
| classic+book | s96 | 10-22 (0.31) | |

Book stats: 553 entries, 29 kept A-won replays of 32 stored oracle-vs-s98
games, 2 move conflicts dropped, 3 replays drifted from the stored result
(theory 19b in action, see below).

## Interpretation (why the book fails, and what it means for theory 14)

Theory 14 assumed: "the champion is deterministic, so its lines can be mined and
its best replies stored; a book + d6 agent then beats it with less live
compute." Two load-bearing parts of that premise are false here:

1. **The target is not reproducibly deterministic across runs.** s98 is a
   deterministic function of board + its own TT/killer/history state, but that
   state PERSISTS ACROSS GAMES within a shard process (theory 19b). The mined
   games carried one cross-game state history; a fresh measurement run carries
   another, so s98's replies diverge from the book's lines within a few plies.
   The bookgen run itself saw this directly: 3 of 32 stored games did not even
   reproduce their own recorded result on replay. Once s98 deviates, the booked
   agent is out of book, holding an oracle-shaped middlegame with a non-oracle
   brain.
2. **The stored moves' value is not brain-portable.** The oracle's winning
   moves win BECAUSE the oracle's d8/nb2m search stands behind them. Played by a
   d6 chip counter or by s98's d6 learned brain and then handed off mid-line,
   the same moves lead into positions the weaker brain misplays. Evidence: the
   chip-counter+book, fed the exact moves that beat s98, does WORSE against s98
   (0.22) than the bookless chip counter (0.28).

The lone positive (chip-counter+book beats the oracle 20-12) is not a
counterexample: those book positions ARE the oracle's own past positions, so
replaying its moves keeps parity against it specifically. It is a curiosity, not
a dethrone of the target.

**Verdict on theory 14:** refuted in its naive form (book of a stronger agent's
moves, hand off to a cheap brain). It could still hold under two fixes, now the
concrete open version: (a) a `--reset-state` mode that clears cross-game search
state so the target is reproducible (the todo item under theory 19), making the
mined lines actually recur; and (b) a book that stays in book to the WIN (a
full winning line, or a full response tree covering the target's deviations)
rather than handing off to a weak brain mid-line. Without both, an offline book
does not convert a deep agent's edge into a cheap agent's win.

## Deviations from the plan

- The plan expected either a dethrone or a labeled "gotcha" counter. The actual
  outcome is neither: the book does not even reliably beat the target
  head-to-head, for the reproducibility reason above. That is a stronger and
  more useful negative than "it works but only as a gimmick."
- The plan's success test "(a) oracle-verified winning lines exist" is
  confirmed (29 mined), but "(b) the book agent's pool Elo does not drop" FAILS:
  both book agents dropped ~16 Elo. The book is mildly harmful, not neutral.
- No `--reset-state` work was done (it is a pre-existing todo item and out of
  this phase's scope); it is now the tethered prerequisite for any theory-14
  retry.

## Roster state

Both book agents left `on` as documented style probes (their non-transitive
profiles -- chip-counter+book beating the oracle while losing to s98 -- are
distinctive). `models/book1.txt` is committed as the immutable book for slot 1.
Retire later per the curation policy if redundant.

## Future Work

- **`--reset-state` gauntlet mode** (tethered to Interpretation 1): clear
  TT/killer/history between games so a deterministic pair replays identically.
  This is the prerequisite to even TEST whether a book can reproduce mined
  lines; without it, theory 14 cannot be evaluated cleanly. Also separates
  theory 19's two artifact mechanisms.
- **In-book-to-the-win / response-tree book** (tethered to Interpretation 2):
  extend bookgen to store a full winning continuation (or branch on the target's
  legal replies via the existing `--branch-tries` machinery in pairgen) so the
  agent never hands a won position to a weak brain. Only then is "less live
  compute beats the champion" actually on trial.
- **Why chip-counter+book beats the oracle 20-12** (tethered to the lone
  positive): is it purely "replay its own positions," or does the book encode
  something transferable about anti-oracle play? A book mined from a DIFFERENT
  agent's wins over the oracle would separate these.

## Ideas This Inspired

- The non-reproducibility of a "deterministic" agent across runs (theory 19b) is
  now blocking TWO efforts (this book, and clean det-vs-det head-to-heads). A
  `--reset-state` flag is worth prioritizing as shared infrastructure, not just
  a book prerequisite.
- Books as a DIVERSITY source rather than a strength source: the book agents
  have genuinely distinct, non-transitive profiles cheaply. A pool of
  book-perturbed agents could stress-test the champion's robustness the way the
  dilution ladder and counter-agents are meant to.
- A book keyed by the mirror-folded position key (`positionKey(.,true)`) would
  cover both wings of the symmetric opening from one mined line, roughly halving
  the entries needed and improving hit rate against a deviating target.

## Commit

One commit: bookgen + book opener (src/, tools/), tests, `models/book1.txt`,
roster + roster_top additions, new match data + refit artifacts (ranking/),
this results doc, theory 14 update (refuted, with mechanism), CHAMPION.md
defended-challenges row, todo.md strike-out, and the README/CLAUDE.md reference
updates.
