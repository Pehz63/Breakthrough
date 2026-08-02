# Reigning Champions (single source of truth)

> **[RE-CERTIFIED 2026-08-01 after a scoring-population change]** The match
> store no longer includes games involving the TD-Leaf Pass-2 candidates that
> were screened and never promoted (457,611 rows, 71% of the store). Their files
> are on disk but their lines are out of `ranking/matches.index.txt`, by
> developer decision: a permanent ladder should not be dominated by games
> against transient candidates, and `play --cohort` had left some rostered
> agents with 60% of their games against that cohort. Screening cohorts now play
> into their own store so this cannot recur.
>
> **This changed the ranking**, because a Bradley-Terry fit is joint: a game
> against a retired agent is evidence about the ROSTERED agent that played it.
> Dropping those rows moved 153 of 170 rostered agents. The **openless** title
> changed hands as a result; the other four categories re-confirmed the same
> holders. Every number in the Summary below comes from the 2026-08-01 fit and
> is **not comparable to the 2026-07-29 numbers** that the rest of this file
> still quotes, since the scale is refit over a different game population.
> Analysis: `plans/store-sharding-results-1-tidy-albatross.md`.

> **[SPLIT 2026-07-28, EXPANDED 2026-07-29]** The single throne is now 5
> parallel category champions. Every number below comes from the 2026-07-29
> full-roster anchored refit (158 active agents), read from
> `ranking/standings.tsv` (active only, grouped by head), with all three
> defects in `Docs/benchmarking.md` applied: no retired rows, one search head
> per comparison, and distinct-game counts checked rather than stored-row
> counts. **All 5 declarations below are PROVISIONAL / SCREENING LEVEL**
> (11-games/pair-equivalent for every pair touching a category member, per
> `Docs/benchmarking.md`'s own rule that an 8-games/pair fill has inverted the
> top of the table three times in this project's history -- 11 is closer but
> still short of the 32-games/pair certification standard). Do not quote any
> of these as a settled result without re-reading the current
> `ranking/standings.tsv` first.

This file declares each category's current champion (the standing dethrone
target for that category). Update it in the same session as any certification
refit. Other docs point here for "who is the champion" instead of embedding Elo
numbers, because absolute Elo drifts as the pool grows (see
`Docs/benchmarking.md`, "Elo scale drift across fits"). When another doc must
quote a number, it tags it with the fit date.

## Why 5 categories

Until 2026-07-26 the project declared ONE champion: the highest pooled-Elo
target-class agent in the full-roster fit. That agent kept turning out to be a
book-wearing one (`todo.md`, 2026-07-18 - 2026-07-26), and theory 38 (`Docs/
theories.md`) showed a book's measured Elo lift is a memorized-line artifact --
it holds only while the opponent reproduces the replies it made when the book
was mined, and it collapses under opening diversification. `todo.md` flagged an
explicit open decision: keep one throne, or split into separate tracks? On
2026-07-28 the developer resolved it by splitting rather than picking a side,
then on 2026-07-29 asked for the roster to be grown further for Elo diversity
(round 2, see "Rounds" below).

## Category definitions and eligibility rule

Within the standard head `ab(d6,tt,ord,nb200k)@1`, excluding reference-class
agents (currently the d8/nb2m oracle, `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)
@2`, 1099 +/- 9 in this fit -- ten times the node budget, never a target), a
target-class agent's canonical ID places it in exactly one category by its
`.opener(...)` ID segment:

| Category | Rule |
|---|---|
| **openless** | no `.opener(...)` segment at all |
| **4-book** | `.opener(book,N)@1` where `models/bookN.txt` was mined at `--plies 4` |
| **8-book** | `.opener(book,N)@1` where `models/bookN.txt` was mined at `--plies 8` |
| **4-random** | `.opener(rand,4)@1` |
| **8-random** | `.opener(rand,8)@1` |

"4"/"8" are opener PLIES, not book slot numbers. Any other opener depth (the
existing 6/16/30/60-ply books, or a hypothetical `rand,6`) is ladder/study data,
not a member of any of the 5 categories -- it holds no title. This deliberately
retires the pre-split champion (`book11`, a 16-ply book) with no direct
successor; see "Pre-split history" below.

**One-head rule inside the categories.** A category's title is scoped to
`ab(d6,tt,ord,nb200k)@1` only. `ab(d6,ord,nb200k)@1.adv(t20,c77,...)@1` wears
its own 4-ply/8-ply books (`book,19`/`book,20`) for Elo-diversity purposes, but
that head has no `tt` -- a different agent -- so those rows are reported for
context (see the 4-book/8-book sections below) but are never category-eligible.

This is ONE roster (`ranking/roster.txt`), ONE match store
(`ranking/matches.jsonl`), ONE Bradley-Terry fit -- not a second incompatible-
scale pool. `ranking/roster_open.txt`/`matches_open.jsonl` remains a separate,
narrower instrument (fair, paired-opening, opener-bias-controlled comparison)
answering a different question than "which agent tops this category in the one
shared pool."

## Certification methodology (summary; details in Docs/benchmarking.md)

1. Full-roster anchored refit is the instrument; gauntlets only screen.
2. A top-of-category claim needs every category member's pairs at >= 32
   games/pair. Never conclude from an 8-11-games/pair fill alone -- three prior
   inversions in this project's history (see the Lineage table below).
3. Compare order and error bands within ONE fit; never absolute Elo across fits.
4. Read `ranking/standings.tsv` (active only, grouped by head), never
   `ranking/ratings.tsv` (includes retired `gone` rows).
5. Fix ONE search head (`ab(d6,tt,ord,nb200k)@1`) for every category.
6. The d8/nb2m oracle is reference class in every category, never a target.
7. Whenever the top may have changed, re-certify and update this file plus
   `todo.md`'s Agent Track goal paragraph in the same session.

## Rounds

- **Round 1 (2026-07-28):** 24 new agents (4 book-opener on `classic`/`s98`,
  20 random-opener on 10 cores), 116 -> 140 active agents, ~24,480 new games.
- **Round 2 (2026-07-29):** 18 more agents (4 more book-opener on `s3`/`adv`,
  14 more random-opener on the remaining cheap bare cores), 140 -> 158 active
  agents, ~21,384 new games. Every category's champion and runner-up from
  round 1 held its rank through round 2, but margins stayed thin (see below).

## Summary (2026-08-01 fit, after the scoring-population change)

Read this table, not the 2026-07-29 one below it. Fit population: the `roster`
and `retired_other` store parts (191,823 games before the openless boost run,
192,639 after), 170 active agents.

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) | vs 2026-07-29 |
|---|---|---|---|---|---|
| **openless** | `ab(d6,tt,ord,nb200k)@1.learned(s169,4975683c,tdleaf_self,lin,129-1,con100)@1` | **1044 +/- 11** | 1603 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,position_elo,lin,129-1,sig129-1,con100)@1`, 1007 +/- 8 (gap 37 / SE 13.6 = 2.7 SE) | **CHANGED** (was s76) |
| 4-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,pool_games,lin,129-1,con100)@1.opener(book,15)@1` | 968 +/- 11 | 1392 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,13)@1`, 947 +/- 11 (gap 21 / SE 15.6 = 1.3 SE) | held |
| 8-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,pool_games,lin,129-1,con100)@1.opener(book,16)@1` | 989 +/- 11 | 1392 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,14)@1`, 964 +/- 11 (gap 25 / SE 15.6 = 1.6 SE) | held |
| 4-random | `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,pool_games,lin,129-1,con100)@1.opener(rand,4)@1` | 965 +/- 11 | 1392 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,position_elo,lin,129-1,sig129-1,con100)@1.opener(rand,4)@1`, 949 +/- 11 (gap 16 / SE 15.6 -- **statistically tied**) | held |
| 8-random | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,position_elo,lin,129-1,sig129-1,con100)@1.opener(rand,8)@1` | 781 +/- 10 | 1392 | `ab(d6,tt,ord,nb200k)@1.learned(s10,fead67b7,weight_merge,lin,129-1,con100)@1.opener(rand,8)@1`, 773 +/- 10 (gap 8 / SE 14.1 -- **statistically tied**) | held |

### Evidence level, stated honestly

**openless is the only category boosted to rule-2 fill.** Because dropping the
cohort games left the new leader at a median 8.0 games/pair -- exactly the fill
this project has been inverted by three times -- `ranking/roster_top.txt` was
rewritten to the openless top 9 plus the bare chip counter and played out: 816
games, and `rank.exe check --roster ranking/roster_top.txt --games 32` now
reports **0 pending at 32 across all 55 contender pairs**.

Two caveats on that, both measured rather than assumed:

- **32 stored rows/pair is ~22.6 DISTINCT games/pair.** The boost games came
  back at 0.706 distinct trajectories per row (816 rows, 576 distinct). These
  contenders carry no dilution and no opener, so nothing consumes `rand()`; the
  variation comes from the `tt` head's cross-game state differing across the 12
  shard processes. Printed SEs are therefore understated by about
  `sqrt(1/0.706)` = 1.19x. Applying that, the openless gap is 37 Elo against a
  combined SE of ~16.1, so **2.3 SE, not 2.7**. Still a separation, less
  comfortable than the raw table suggests.
- **The other four categories are unboosted** and sit at 1392 games with pairs
  well under 32. They are re-confirmed only in the sense that the same agent
  still leads; 4-random and 8-random remain statistically tied at the top and
  should not be quoted as settled.

**A standing suspicion about the openless result.** `s169` took the title while
*losing* 36% of its games, because it is itself a TD-Leaf agent and the removed
cohort was its own family. The boost run was specifically designed to test
whether that survived proper fill, and it did (1050 +/- 12 at 8 games/pair ->
1044 +/- 11 at 32). But the mechanism by which removing cohort games helps a
cohort member is not understood, and the bare chip counter
`ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` swinging from openless rank 2 to
rank 35 under the same change is unexplained. Treat this title as the one most
likely to move again.

## Superseded summary (2026-07-29 fit, round 2, screening level)

Kept for lineage. **Do not quote these numbers as current** -- they come from a
fit over a different game population (see the re-certification banner at the top
of this file).

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) |
|---|---|---|---|---|
| openless | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1` | 1012 +/- 9 | 2136 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1`, 1002 +/- 9 (gap 10 / SE 12.7 -- **statistically tied**) |
| 4-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,15)@1` | 972 +/- 12 | 1256 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,13)@1`, 941 +/- 11 (gap 31 / SE 16.3) |
| 8-book | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,16)@1` | 997 +/- 12 | 1256 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,14)@1`, 958 +/- 12 (gap 39 / SE 17.0) |
| 4-random | `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,4)@1` | 968 +/- 12 | 1256 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,4)@1`, 955 +/- 11 (gap 13 / SE 16.3 -- **statistically tied**) |
| 8-random | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,8)@1` | 782 +/- 11 | 1256 | `ab(d6,tt,ord,nb200k)@1.learned(s10,fead67b7,value,lin,129-1,con100)@1.opener(rand,8)@1`, 770 +/- 11 (gap 12 / SE 15.6 -- **statistically tied**) |

Only 4-book and 8-book clear ~2 combined SE; the other three are inside 1 SE.
Growing the roster (round 2) did not resolve the close calls -- if anything
4-random and 8-random got closer, since more cores joined and one (`s10`)
landed a near-tie with the round-1 8-random leader. None of these should be
read as settled -- they are the honest current answer at screening depth, per
this file's own rule 2 above.

---

## Category: openless

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1`
- **Elo:** 1012 +/- 9 (2136 games), 2026-07-29 full-roster fit.
- **What it is:** a DIST model (position-oracle recipe: two trained heads, mu +
  log-sigma, fit by probit BCE against per-position Elo-gap labels) whose mu
  head has NO hidden layer -- structurally a linear value model, so search
  reads it exactly like the `value`-recipe linear models, but its training data
  and loss come from the position-oracle pipeline rather than outcome labels.
  No loadout item at all: this is the core's bare identity.
- **Needed no new games for this declaration in either round** -- it was
  already the strongest bare bookless target-class agent in the pre-split
  2026-07-26 fit (1077 +/- 9 there; the number here is re-quoted from each
  post-split fit per rule 3, never compare absolute Elo across fits).
- **Nearest rivals (this fit, same head):**

  | Elo | Agent |
  |---|---|
  | 1002 +/- 9 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1` -- statistically tied (gap 10, combined SE 12.7) |
  | 987 +/- 8 | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1` |
  | 973 +/- 8 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1` (the pre-split single champion's bare core) |
  | 971 +/- 8 | `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1` (the 4-random champion's bare core) |
  | 921 +/- 8 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` (the chip-counter control) |

- **Lineage:** founding declaration 2026-07-28 (1030 +/- 9), re-quoted 2026-07-29
  after round 2 (1012 +/- 9, same agent, same rank -- the split held).
- **Defended challenges:** none yet.

## Category: 4-book

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,15)@1`
- **Elo:** 972 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** the pre-split single champion's evaluator (`learned(s98,...)`,
  v2 sparse piece-square linear value model, outcome-trained) wearing a 4-ply
  self-mined book, `models/book15.txt` (22 entries), mined 2026-07-28 via
  `rank.exe bookgen --plies 4` from s98's own 25-of-32 kept winning replays vs
  `classic(t1,c4,w0,l0)@2` -- the exact same already-stored match history that
  produced its 6/16/30-ply siblings (book10/11/12), so mining needed no new
  games. Lift over s98's bare core in this SAME fit: 972 - 973 = **-1 Elo**,
  noise -- the shallowest book tested on this core is not distinguishable from
  no book at all.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 941 +/- 11 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,13)@1` (4-ply, classic-own; lift over classic's bare 921 = +20) |
  | 928 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1.opener(book,17)@1` (4-ply, s3-own, added round 2; lift over s3's bare 987 = **-59**, consistent with the existing on-record finding that s3's OWN book costs it Elo while a borrowed one doesn't, theory 38) |

- **Off-head, not category-eligible (context only):**
  `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(book,19)@1`
  (4-ply, adv-own, added round 2), 952 +/- 11 -- different search head
  (`ab(d6,ord,nb200k)@1`, no `tt`), so it can never hold this title.
- **Lineage:** founding declaration 2026-07-28 (982 +/- 12), re-quoted
  2026-07-29 after round 2 (972 +/- 12, same agent, same rank).
- **Defended challenges:** none yet.

## Category: 8-book

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,16)@1`
- **Elo:** 997 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** same core as the 4-book champion, wearing an 8-ply self-mined
  book, `models/book16.txt` (60 entries), mined 2026-07-28 via `rank.exe bookgen
  --plies 8` from the same already-stored s98-vs-classic match history as
  book15 (no new games). Lift over s98's bare core (973): **+24 Elo**.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 958 +/- 12 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,14)@1` (8-ply, classic-own; lift over classic's bare 921 = +37) |
  | 938 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1.opener(book,18)@1` (8-ply, s3-own, added round 2; lift over s3's bare 987 = **-49**, same direction as its 4-ply rung) |

- **Off-head, not category-eligible (context only):**
  `ab(d6,ord,nb200k)@1.adv(t20,c77,w0,l0,f0,d-2,e0,m0,h0,b1,o0,r0,x0,n0,s1,g1)@1.opener(book,20)@1`
  (8-ply, adv-own, added round 2), 988 +/- 12 -- would rate ABOVE this
  category's champion if it were eligible, but it is a different search head.
- **Depth-ladder context (same fit, not a new claim -- consistent with the
  project's existing "book depth is not monotonic" finding, theory 38):** on
  the `s98` core, lift by depth reads 4ply -1 / 6ply / 8ply +24 / 16ply / 30ply
  (6/16/30-ply lifts not re-derived this session, see the pre-split history's
  numbers, which used a different fit and aren't directly comparable per rule
  3); on the `classic` core, 4ply +20 / 8ply +37; on the `s3` core (new this
  session), 4ply -59 / 8ply -49 -- s3 is the first core where BOTH new rungs
  are negative, strengthening rather than resolving the "own book is not
  reliably better than bare" finding.
- **Lineage:** founding declaration 2026-07-28 (1010 +/- 13), re-quoted
  2026-07-29 after round 2 (997 +/- 12, same agent, same rank).
- **Defended challenges:** none yet.

## Category: 4-random

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,4)@1`
- **Elo:** 968 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** a v2 sparse piece-square linear value model (outcome-trained,
  one of the s94-s99 multi-seed replicate family) wearing `.opener(rand,4)@1`:
  uniform-random for its own first 4 plies, then hands off to its real search.
  New identity in this pool as of round 1 (same ID text has separate history in
  `ranking/roster_open.txt`'s incompatible-scale pool; this is independent
  history in the merged pool). Lift over its own bare core (971, this fit):
  **-3 Elo**, noise.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 955 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,4)@1` -- statistically tied (gap 13, combined SE 16.3); note s76 is the openless AND 8-random champion but does not top 4-random |
  | 943 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1.opener(rand,4)@1` |
  | 934 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(rand,4)@1` |
  | 932 +/- 11 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1` (control; lift over bare classic 921 = +11) |
  | 910 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s9,e5b3b014,value,lin,129-1,con100)@1.opener(rand,4)@1` (added round 2) -- **RETIRED 2026-07-30**: models/sweep/slot9.txt was accidentally overwritten (gitignored, unrecoverable); this rating is historical only, the identity can never play again |

- **Round 2 did not change the leader**, but tightened the gap to the runner-up
  (18 / SE 17.0 in round 1 -> 13 / SE 16.3 now) -- the opposite of what more
  data should do if the true gap were real; consistent with treating this
  category as unresolved (Future Work: boost to 32 games/pair).
- **Lineage:** founding declaration 2026-07-28 (988 +/- 12), re-quoted
  2026-07-29 after round 2 (968 +/- 12, same agent, same rank).
- **Defended challenges:** none yet.

## Category: 8-random

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,8)@1`
- **Elo:** 782 +/- 11 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** the openless champion's same core wearing `.opener(rand,8)@1`
  (uniform-random for its own first 8 plies). Lift over its own bare core
  (1012, this fit): **-230 Elo** -- an 8-ply random opener costs this core
  roughly 8x what a 4-ply one does (4-random lift for the same core was only
  955 - 1012 = -57). The classic control shows the same sharp step: +11 at
  4-ply (932 vs bare 921) but **-168** at 8-ply (753 vs bare 921, combined SE
  ~13.6, ~12 combined SE -- highly significant, holds up in round 2 exactly as
  in round 1). Filed as theory 41 (`Docs/theories.md`), now confirmed twice at
  two different roster sizes.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 770 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s10,fead67b7,value,lin,129-1,con100)@1.opener(rand,8)@1` -- statistically tied (gap 12, combined SE 15.6), added round 2 and immediately near-tied the round-1 leader |
  | 766 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1.opener(rand,8)@1` |
  | 758 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s8,6f1a4264,value,lin,129-1,con100)@1.opener(rand,8)@1` |
  | 758 +/- 11 | `ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,8)@1` (the 4-random champion, but falls to 5th here) |
  | 753 +/- 11 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,8)@1` (control) |

- **Lineage:** founding declaration 2026-07-28 (790 +/- 12), re-quoted
  2026-07-29 after round 2 (782 +/- 11, same agent, same rank, but the gap to
  2nd place shrank from 22 to 12 as more cores joined -- see Future Work).
- **Defended challenges:** none yet.

---

## Pre-split history (single-champion era, until 2026-07-28)

Kept as a permanent record. These tables describe the ONE-champion regime and
are not re-quoted or extended after the split. The final single champion,
`ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(book,11)@1` (a 16-ply
book, certified 2026-07-26 at 1122 +/- 10), does not fall into either new book
category under the split's exact taxonomy (4-ply / 8-ply only) -- it retires
with no title in any of the 5 tracks, staying in `ranking/roster.txt` as
depth-ladder data (see `tools/CLAUDE.md`'s "Mined books" table).

### Lineage

| Reign | Champion | Certification | Doc |
|---|---|---|---|
| until 2026-07-17 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` (chip counter, bookless; earlier as its @1 identity) | dethroned at 976 +/- 13 vs s98's 1064 +/- 14, head-to-head 9-23 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-17 - 2026-07-18 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` | certified 1064 +/- 14 (dethrone fit), re-confirmed 1074 +/- 14 (phase 1 fit), dethroned at 1074 +/- 12 vs the self-mined book's 1145 +/- 13, head-to-head 7-25 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-18 - 2026-07-26 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1` | certified 1145 +/- 13 in its own fit; dethroned 2026-07-26 at 1075 +/- 9 vs book11's 1122 +/- 10 (3.5 combined SE) | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
| 2026-07-26 - 2026-07-28 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(book,11)@1` | certified 1122 +/- 10, 116-agent full-roster refit, contenders at 32 games/pair -- statistically TIED with book10 (1110 +/- 10) | `plans/book-opener-audit-results-1-vivid-lantern.md` |
| 2026-07-28 - | *(split into 5 category champions, see above)* | -- | `plans/champion-split-plan-1-kind-beaming-cerf.md` / `champion-split-results-1-kind-beaming-cerf.md` |

### Defended challenges

| Date | Challenger | Result | Doc |
|---|---|---|---|
| 2026-07-17 | s98 + quiescence (`ab(d6,tt,ord,qs,nb200k)...learned(s98)`) | pooled tie (1073 vs 1074), champion won the pair 23-9 | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | chip counter + quiescence | 1002 +/- 14, not close | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | s98 + oracle refutation book (`.opener(book,1)`) | 1059 +/- 14 (below plain s98), champion won the pair 18-14 | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | chip counter + oracle refutation book | 967 +/- 13, went 7-25 vs the champion; the original "worse than bookless" reading is withdrawn (7-25 is 4-10 on 14 distinct games vs bookless 9-23 on a genuine 32) | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | 6-seed mirror-symmetrized weight ensemble of the champion's recipe (slot9) | 924 +/- 12, -155 vs the champion | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-17 | mirror-symmetrized champion weights (slot10) | 944 +/- 12, mirroring alone cost 135 Elo | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-18 | classic + self-mined book (`.opener(book,2)`) vs s98 | **DETHRONED s98**: 1145 +/- 13 vs 1074 +/- 12, head-to-head 25-7 | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
| 2026-07-26 | s98 + its OWN 16-ply self-mined book (`.opener(book,11)`) | **DETHRONED classic+book2**: 1122 +/- 10 vs 1075 +/- 9 | `plans/book-opener-audit-results-1-vivid-lantern.md` |
| 2026-07-26 | s98 + own book at 6 / 30 / 60 ply (`book,10` / `book,12` / `book,4`) | 1110 / 1049 / 1095. The 6-ply rung ties book11. The 30-ply rung is an unexplained 73-Elo dip | same |
| 2026-07-26 | classic + own book at 6 / 16 / 30 ply (`book,7` / `book,8` / `book,9`) | 1019 / 1010 / 1036 vs bare 965 and 60-ply 1075, so lift grows with depth on this core | same |
| 2026-07-26 | s98 + classic's BORROWED book2 | 1092 +/- 10, within 0.2 SE of its own 60-ply book (1095) | same |
| 2026-07-26 | `learned(s3,68364898)` + its own book6 | 1030 vs 1063 bare: its OWN book COSTS 33 Elo, while classic's borrowed book2 is neutral (1064) | same |
| 2026-07-26 | `adv(t20,c77,...)` + its own book3 (no-TT head) | 911 vs 1018 bare: its OWN book costs **107 Elo**, while borrowed book2 is +15 | same |
