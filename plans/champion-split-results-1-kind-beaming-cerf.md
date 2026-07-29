# Results: splitting the single champion into 5 opener-based categories

Companion to `champion-split-plan-1-kind-beaming-cerf.md`. Round 1 executed
2026-07-28 (below); round 2 (a developer-requested roster expansion) executed
2026-07-29, appended at the end of this document.

## Summary of changes

- Mined 4 new book files: `models/book13.txt` (classic-own, 4-ply, 8 entries),
  `models/book14.txt` (classic-own, 8-ply, 18 entries), `models/book15.txt`
  (s98-own, 4-ply, 22 entries), `models/book16.txt` (s98-own, 8-ply, 60 entries).
  All 4 mined for free (`rank.exe bookgen`, no new games) by replaying the
  already-stored classic-vs-s98 match history that also produced book7-12.
- Added 24 new agent identities to `ranking/roster.txt` (4 book-opener, 20
  random-opener), taking the active roster from 116 to 140 agents. Purely
  additive -- 0 existing lines changed or removed.
- Played `.\tools\run_rank.ps1 -Workers 10 run --games 8 --paired-openings`:
  24,480 new games (matches the plan's estimate exactly), then a full-roster
  Bradley-Terry refit.
- Rewrote `ranking/CHAMPION.md`: one champion -> 5 parallel category champions
  (openless / 4-book / 8-book / 4-random / 8-random), with a shared methodology
  intro, a 5-row summary table, 5 per-category sections, and the pre-split
  history preserved under its own heading.
- Updated `todo.md` (struck the resolved "book eligibility" item, rewrote the
  Agent Track goal paragraph, added a "grow the pools further" follow-up),
  `tools/CLAUDE.md` (4 new Mined-books rows), and root `CLAUDE.md` (generalized
  the Champion declaration Standing Instruction to "each category champion",
  per the developer's confirmed minimal-wording choice).
- No `src/`/`tests/` changes, so `run_tests.ps1 -Build` was a no-op for this
  commit per the project's own exception rule.

## The 5 champions (2026-07-28 fit, screening level -- see caveat below)

| Category | Champion | Elo +/- SE | Games | Nearest rival gap / combined SE |
|---|---|---|---|---|
| openless | `learned(s76,ef183148,dist,lin,...)@1` | 1030 +/- 9 | 1992 | 10 / 12.7 -- statistically tied |
| 4-book | `learned(s98,...)@1.opener(book,15)@1` | 982 +/- 12 | 1112 | 31 / 17.0 |
| 8-book | `learned(s98,...)@1.opener(book,16)@1` | 1010 +/- 13 | 1112 | 41 / 17.7 |
| 4-random | `learned(s96,990e39e7,...)@1.opener(rand,4)@1` | 988 +/- 12 | 1112 | 18 / 17.0 -- weakly separated |
| 8-random | `learned(s76,ef183148,...)@1.opener(rand,8)@1` | 790 +/- 12 | 1112 | 22 / 17.0 -- weakly separated |

Full per-category rivals tables, "what the agent is" descriptions, and the
pre-split lineage/defended-challenges history: `ranking/CHAMPION.md`.

## Measurement caveat: these are screening-level, not certified

Every one of the 3,060 new pairs (all pairs touching one of the 24 new agents)
got exactly 8 games, matching this roster's existing per-pair convention but
falling short of the project's own 32-games/pair top-of-table certification
standard (`CLAUDE.md`'s ranking-claim hygiene rule 2). The developer explicitly
declined the usual boosting mechanism (`ranking/roster_top.txt`) for this pass,
preferring one unified roster over a second efficiency-boosting file (see the
plan's "Critically: this stays ONE roster" section). Consequence: of the 5
categories, only 8-book clears roughly 2 combined standard errors over its
runner-up; openless, 4-random, and 8-random are all within about 1 SE of their
runner-up, i.e., not distinguishable from a tie at this sample size. This
project's own history shows an 8-games/pair top-of-table read has inverted on
boosting three separate times, so none of these 5 declarations should be
treated as settled -- `ranking/CHAMPION.md`'s banner says this explicitly, and
any future document quoting these numbers should re-read `standings.tsv` first
rather than trusting this doc's numbers as current.

## Notable findings (same fit, so directly comparable within this doc)

- **Opener cost is sharply nonlinear between 4 and 8 plies.** On the `classic`
  control core, a 4-ply random opener was actually Elo-NEUTRAL to slightly
  positive (931 bare -> 946, +15), but an 8-ply one cost **-172** (759). The
  openless champion's own core (`s76`) showed the same shape: -60 at 4-ply,
  **-240** at 8-ply -- a 4x jump for doubling the random window, not a linear
  one. This is directionally consistent with the existing single-agent
  opener-sensitivity measurement on record (`.opener(rand,6)@1` costing the old
  champion ~-217 Elo, `Docs/terminology.md`), and gives that number a
  same-fit two-point curve for the first time.
- **The category leader flips between 4-random and 8-random.** `s96` tops
  4-random (988) but falls to 3rd at 8-random (760); `s76` tops 8-random (790,
  and openless) but sits 2nd at 4-random (970). No single core dominates both
  random-opener depths in this fit, though the gaps involved are all within
  ~1 combined SE, so this is a "worth re-checking with more games" observation,
  not a confirmed crossover.
- **The shallowest book tested (4-ply) is not distinguishable from no book at
  all, on the s98 core specifically.** Lift over s98's bare core: 4-ply -7,
  6-ply +65, 8-ply +21, 16-ply +73, 30-ply +14. The classic core's 4/8-ply
  lifts (+20, +38) are smaller than usual too. Both ladders remain
  non-monotonic in this fit, consistent with the project's existing "book depth
  has no explained mechanism" finding (theory 38) -- the new points don't
  resolve that open question, they just add two more irregular data points to
  it.
- **`book11`, the pre-split single champion, has no successor in its own
  weight class.** Under the exact 4-ply/8-ply taxonomy requested, a 16-ply book
  simply isn't a category member. It remains rostered as depth-ladder data
  (`tools/CLAUDE.md`) but the split retires it from any title, by design
  (developer-confirmed before implementation).

## Gotchas and implementation notes

- **Silent-missing-book hazard confirmed but avoided.** `bookForSlot()`
  (`src/ai_random.cpp`) loads `models/book<N>.txt` lazily and silently; a
  missing file makes the `book` opener permanently return false for that
  process with zero warning. Books 13-16 were mined and file-existence-checked
  before the roster edit landed, so this didn't bite, but it's worth flagging
  for any future book-slot addition: mine first, verify non-empty, roster
  second, always in that order.
- **Elo scale compressed as expected when the pool grew from 116 to 140
  agents.** Every pre-existing agent's Elo dropped between the pre-split
  (2026-07-26) and post-split (2026-07-28) fits purely from the larger BT prior
  mass -- e.g. `book11` 1122 -> 1062, `book10` 1110 -> 1054, bare `s76` 1077 ->
  1030, bare `classic` 965 -> 931. This is the documented "Elo scale drift
  across fits" effect (`Docs/benchmarking.md`), not a real strength change, and
  is exactly why `CHAMPION.md`'s openless number was re-quoted from the new fit
  rather than carried over from the pre-split one.
- **`run_rank.ps1 -Workers 10`** (not 8 as in the plan's example command) was
  used for the actual run, matching the machine's 12 logical processors with
  some headroom. The plan's ~24,480-game estimate held exactly (24,480 pending
  at `--games 8` per `rank.exe check` right after the roster edit).
- **No distinct-game-count audit was done per new pair.** `CLAUDE.md`'s hygiene
  rule 7 (count distinct games, not stored rows, for deterministic pairs)
  wasn't applied pair-by-pair here -- only the `.opener(rand,N)` pairs get
  guaranteed-independent trajectories from `--paired-openings`; the 4 new book
  pairs and any bare-vs-bare pairs among the 24 remain subject to the existing
  cross-game-TT-state caveat. This is flagged as a Future Work item below
  rather than resolved in this pass.

## Future Work

- **Boost the 4 non-openless categories to 32 games/pair before treating any
  of them as certified.** Directly tethered to the "screening-level, not
  certified" caveat above -- would settle whether 4-book, 4-random, and
  8-random's current leaders survive, since three of the four gaps sit inside
  ~1 combined SE today and this project's own history shows such gaps invert
  on boosting more often than not.
- **Grow the book categories beyond `classic`/`s98`.** `s3` and `adv(t20,c77,
  ...)` both already have an established own-book precedent (book6, book3) but
  were mined against a moving "champion" target rather than a fixed pair, so a
  stable `bookgen --b` convention needs picking before a same-fit 4-ply/8-ply
  rung can be added for them cleanly. Tethered to: whether the 4-book/8-book
  leaderboard is a `classic`-vs-`s98` two-horse race or something broader.
- **Investigate the 4-random/8-random leader flip (`s96` vs `s76`).** Tethered
  to the "category leader flips" finding above -- is this a real
  depth-dependent core property, or noise from the 8-games/pair sample? A
  boosted refit (previous item) would help distinguish it, but a dedicated
  head-to-head between `s96`+rand4/rand8 and `s76`+rand4/rand8 would isolate it
  faster.
- **Audit distinct-game counts for the 4 new book pairs and any deterministic
  bare-vs-bare pairs among the 24 new agents.** Tethered to the "no
  distinct-game-count audit was done" gotcha above -- `CLAUDE.md` rule 7 flags
  this as a prerequisite before quoting any of these pairs' records as strong
  evidence, and it hasn't been checked yet for this batch.

## Ideas This Inspired

- The 4-ply-vs-8-ply opener-cost cliff (neutral-to-positive at 4, sharply
  negative at 8) suggests there may be a specific ply range where a random
  opener crosses from "harmless variety" to "genuinely damaging" for a d6
  search. A finer sweep (5/6/7-ply) on one or two cores could locate that
  knee more precisely than the existing 4/6/8 spot checks.
- Since a book is inert once the position it's keyed on stops recurring
  (theory 38), and a random opener actively prevents recurrence, an agent
  wearing BOTH a random opener AND a book has an interesting failure mode
  worth checking directly: does the book ever fire at all once `.opener(rand,
  N)` has already randomized the first N plies? If the book's positions are
  all within the first N plies, this could be a fully wasted loadout item --
  cheap to check by instrumenting `openerBook`'s hit rate under that combined
  ID.
- The `s76`-tops-openless-and-8-random-but-not-4-random pattern raises a
  broader question worth a dedicated study: is there a class of evaluator
  (dist/mu-only linear heads specifically) that is unusually ROBUST to opening
  randomization compared to outcome-trained linear heads, rather than this
  being a one-core coincidence? Would need more dist-recipe cores at both rand
  depths to tell.

## Round 2 (2026-07-29): developer-requested roster growth

The developer asked, immediately after the round-1 Elos were reported, to add
more agents and combinations for Elo diversity and see if anything else needed
doing. Executed as a second round rather than folding into round 1 because it
was a genuinely separate request made after seeing round-1 results.

### What was added

- **4 more book files**, mined for free (no new games) by reusing the exact
  `--b` target that book3/book6 already established
  (`ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1`, the
  pre-split single champion identity, a fixed already-stored pair):
  `models/book17.txt` (s3-own, 4-ply, 10 entries), `models/book18.txt`
  (s3-own, 8-ply, 42 entries), `models/book19.txt` (adv-own, 4-ply, 2
  entries), `models/book20.txt` (adv-own, 8-ply, 4 entries). adv's books are
  off-head (`ab(d6,ord,nb200k)@1`, no `tt`) so they add roster diversity but
  can never be category-eligible.
- **18 new agent identities**: the 4 book-openers above (`s3`+book17/18,
  `adv`+book19/20) plus 14 more random-openers on the remaining cheap bare
  target-class cores not yet covered (`s4`/`s9`/`s10`/`s94`/`s95`/`s97`/`s99`,
  each at `.opener(rand,4)@1` and `.opener(rand,8)@1`). Deliberately excluded
  the wide dist-mlp architecture family (`s77`/`s78`/`s79`/`s110`/`s112`/
  `s114`/`s115`, 350-1670 ms/move) -- `s111`/`s113` from that same family
  already joined in round 1, and the rest would cost hours of roster play for
  rows theory 39 already predicts will rate low, the same cost-vs-benefit call
  `tools/CLAUDE.md` makes for excluding `book5`.
- Active roster: 140 -> 158 agents. Played
  `.\tools\run_rank.ps1 -Workers 10 run --games 8 --paired-openings`:
  ~21,384 new games (matched the pre-play `rank.exe check` estimate exactly),
  then a full-roster refit.

### Result: every category's champion held rank, but margins didn't improve

| Category | Champion (unchanged from round 1) | Round 1 Elo | Round 2 Elo | Round 1 gap/SE | Round 2 gap/SE |
|---|---|---|---|---|---|
| openless | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1` | 1030 +/- 9 | 1012 +/- 9 | 0.79 | 0.79 |
| 4-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,15)@1` | 982 +/- 12 | 972 +/- 12 | 1.83 | 1.90 |
| 8-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,16)@1` | 1010 +/- 13 | 997 +/- 12 | 2.32 | 2.30 |
| 4-random | `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,4)@1` | 988 +/- 12 | 968 +/- 12 | 1.06 | **0.80** |
| 8-random | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,8)@1` | 790 +/- 12 | 782 +/- 11 | 1.30 | **0.77** |

The whole scale compressed again (expected, more agents joining an anchored
BT fit, `Docs/benchmarking.md`'s "Elo scale drift across fits" -- e.g. bare
`s76` 1030 -> 1012, bare `classic` 931 -> 921), consistent with round 1's own
compression from the pre-split fit. More interestingly, **adding more agents
made two of the five categories LESS separated, not more**: 4-random's leader
gap shrank because a new core narrowed the field, and 8-random's shrank
because a new round-2 agent
(`ab(d6,tt,ord,nb200k)@1.learned(s10,fead67b7,value,lin,129-1,con100)@1.opener(rand,8)@1`,
770 +/- 11) landed almost exactly where the round-1 runner-up was. This is the
expected behavior of an unresolved close call, not a red flag -- more sampling
of nearby-strength agents makes a true near-tie look MORE like a tie, not
less. It reinforces the Future Work item below rather than changing it.

The `s3`-own-book finding from round 1's Elo pattern (own book underperforms
borrowed, per theory 38) reproduced independently on a NEW pair of rungs:
`ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1.opener(book,17)@1`
(4-ply) rates 928 +/- 11 against s3's own bare 987 +/- 8 (**-59 Elo**), and
`...opener(book,18)@1` (8-ply) rates 938 +/- 11 (**-49 Elo**) -- both new
rungs negative, the same direction as the existing 60-ply own-book result
(-33 Elo) already on record for this core. Theory 41 (the 4-ply-vs-8-ply
opener-cost cliff) also reproduced at the larger roster size: the `classic`
control's 8-ply random-opener lift stayed at roughly -168 Elo (was -172 in
round 1), still ~12 combined SE from zero.

### Updated Future Work (supersedes the round-1 list above)

- **Boost all 5 categories to 32 games/pair.** Now the single most load-bearing
  gap: round 2 was an opportunity to see whether more DATA (not more games per
  existing pair) would resolve the close calls, and it didn't -- openless,
  4-random, and 8-random remain within ~1 combined SE of a tie. Tracked in
  `todo.md`.
- **Give `s4`/`s9`/`s10`/`s94`/`s95`/`s97`/`s99` an own-book pair if the book
  categories are grown further.** Unlike `s3`/`adv`, these cores have no
  existing book precedent to reuse a `--b` target from, so a fresh source pair
  (or a deliberate choice to mine against a common fixed opponent) would need
  picking first.
- **`ranking/CHAMPION.md`'s "off-head, not category-eligible" adv rows are
  close enough to be worth watching**: `adv`+book20 (8-ply, 988 +/- 12) sits
  just 9 Elo below the actual 8-book champion (997 +/- 12) -- well inside a
  combined SE -- and clear above the 4-book champion (972 +/- 12), but it can
  never compete in either category because its head lacks `tt`. Whether to
  build a same-head (`ab(d6,tt,ord,nb200k)@1`) `adv` variant specifically to
  make it category-eligible is an open question for a future session.
