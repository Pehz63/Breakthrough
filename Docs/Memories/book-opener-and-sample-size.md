---
name: book-opener-and-sample-size
description: "Book lift is a fixed-start artifact. Deterministic pairs give 2 distinct games per 32 stored rows, so all Elo error bars are understated"
metadata: 
  node_type: memory
  type: project
  originSessionId: 09785e12-8958-42d5-9dca-af23f144cc15
  modified: 2026-07-26T12:45:05.387Z
---

2026-07-26 book-opener audit. Two findings, the second larger than the first.

**The `book` opener is a memorized line, not opening theory.** `models/book<N>.txt` maps
`positionKey(sideToMove).hash -> one move`, mined by `rank.exe bookgen` from games the
line owner WON. Lookup is opponent-blind and core-blind (any agent wearing
`.opener(book,N)` gets the stored move), but the VALUE depends on both: the wearer must be
able to follow the line up, and the book has no response tree, so it keeps firing past
move 1 only while the opponent repeats the replies it made when mined. First deviation
ends it. Four cores were given self-mined books (`models/book3..6.txt`) and measured
against their mining target. Own-book lift at the fixed start: +31pp (s98), +38pp (s111
dist), +12pp (s3), and **exactly 0pp** for the no-TT hill-climbed `adv(t20,c77,...)` core.
Under `pairgen --open-plies 8` every gain vanishes (-41, -50, -33pp), leaving -7pp, -5pp,
+6pp vs each core's own diversified bare baseline. Bare s98 is the strongest s98 loadout
once openings vary. Two corollaries. (a) a self-mined book is a NO-OP for a fully
deterministic agent by construction, since in-book it replays what its own brain would
have chosen anyway, hence the `adv` 0pp and three cells landing on exactly 100%. (b) "books
are core-specific" is not a rule -- own beat borrowed for 3 of 4 cores at the fixed start,
but for `adv` the BORROWED champion book won by 50pp (100% vs 50%). This supersedes the
"+124 own vs -8 foreign" framing in [[elo-comparison-hygiene]]: that +124 is real in the
fit but is fixed-start-only, not transferable strength.

**Defect 3, instrument-wide: nominal stored games are not distinct games.** `gameSeed`
seeds every game, but `rand()` is only consumed by dilution and random-move agents, so a
pair with no `dil(...)` and no `rand` opener consumes NO randomness and the seed is inert:
every game with the same colour assignment is byte-identical. `playOneGame` never calls
`ttClear()`, so for a `tt` head the only variation is which games ran earlier in the same
process (theory 19b). Median distinct-trajectory ratio **0.438** across 190 pairs with
>= 16 games. Worst cases are 32 games -> **2** distinct games (all `ab(d6,ord,nb200k)`, no
TT). Null control of two identical deterministic agents: **32-0 as White, 1-31 as Black**
over 64 games -- colour decides, not skill. So `pm` in `ratings.tsv`/`standings.tsv` is
understated ~1.5x typically and far more for deterministic pairs. ALWAYS count distinct
trajectories (colour + plies + result + both node totals) before quoting a record.

**The champion's certification does not reproduce.** Its 29-3 vs its own bookless self
splits into one run at 5-3 of 8 and one at 24-0 of 24. Fresh re-runs gave exactly 5-3 in
each of 8 separate 8-game processes and 34-30 (53%) in one 64-game process. 53%-100% from
TT state alone. Theory 33 downgraded Confirmed -> Unresolved, and its "32-0 vs its own bookless
self" was read off the WRONG HEAD (`ab(d6,ord,nb200k)`, no TT). Theory 14's "foreign book
made agents WORSE" is unsupported, it is a null. `ranking/CHAMPION.md` carries a
CERTIFICATION UNDER CHALLENGE note. Fix before re-certifying: add `--open-plies` to
`rank.exe play`/`run` (plumbing already in `playoutCapture`, only the CLI is missing), and
tag diversified games separately so they never mix with fixed-start rows.

**Pooled full-roster refit, 2026-07-26 (the instrument that counts).** The pairwise
records above answer "beats the mined opponent", NOT "is stronger". 13 book agents were
rostered, played to 8 games/pair over 116 agents, boosted to 32/pair, refit.
**THE THRONE CHANGED:** `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(book,11)@1`
1122 +/- 10 dethroned `...classic(t1,c4,w0,l0)@2.opener(book,2)@1` (now 1075 +/- 9) by 3.5
SE, tied with its own 6-ply rung book10 (1110 +/- 10). Three lessons: (a) the 8-games/pair
fill INVERTED on boosting (old champ 1090 vs book11 1067, then 1075 vs 1122), third time
in this project, never conclude from 8-game fills. (b) a pairwise sweep can be
ANTI-predictive -- `adv(t20,c77,...)+book2` went 32-0 vs the champion pairwise but rates
1033 +/- 9 vs its own bare 1018 +/- 9, and that core's OWN book costs it 107 Elo. (c) book DEPTH (varied for the first time, 6/16/30/60 ply) has NO consistent direction:
classic +54/+45/+71/+110, s98 +70/+82/+9/+55, with an unexplained 73-Elo dip at the s98
30-ply rung. Book entry count does not predict Elo. Six of the top eight target-class
agents now wear a book, so "are book agents champion-eligible" is a live developer call.

Full write-up: `plans/book-opener-audit-results-1-vivid-lantern.md`.
