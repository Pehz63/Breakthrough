# Reigning Champion (single source of truth)

> **[ELO HYGIENE RE-VERIFIED 2026-07-26]** Every number in this document comes
> from the 2026-07-26 full-roster anchored refit (116 active agents), read from
> `ranking/standings.tsv` (active only, grouped by head), with all three defects
> in `Docs/benchmarking.md` applied: no retired rows, one search head per
> comparison, and distinct-game counts checked rather than stored-row counts.
> Rows in the Lineage and Defended-challenges tables below are historical and
> stay tagged to the fit that produced them, per the never-compare-across-fits
> rule.

This file declares the current champion (the standing dethrone target).
Update it in the same session as any certification refit. Other docs point
here for "who is the champion" instead of embedding Elo numbers, because
absolute Elo drifts as the pool grows (see `Docs/benchmarking.md`, "Elo scale
drift across fits"). When another doc must quote a number, it tags it with
the fit date.

## Current champion

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(book,11)@1`
- **Certified:** 2026-07-26, full-roster anchored Bradley-Terry refit (116 active
  agents) with every contender pair at >= 32 games via `ranking/roster_top.txt`.
  Elo 1122 +/- 10.
- **Statistically tied with the runner-up.** The same core wearing the 6-ply
  rung of the same ladder, `...learned(s98,5801570e)@1.opener(book,10)@1`, is
  1110 +/- 10, a 12-Elo gap at 0.8 combined SE. The pooled-Elo criterion names
  book11, but these two are not separated. The dethroning IS separated: both beat
  the previous champion's 1075 +/- 9 by 3.5 and 2.6 combined SE.
- **Read the OPEN SCRUTINY FLAG below before treating this as strength.** The new
  champion is a book agent too, so it inherits the previous champion's unresolved
  question in full, and theory 38 predicts this lift does not survive opening
  diversification.
- **Criterion:** highest pooled Elo among target-class agents (the
  standing-loop "outrating it outright" criterion), full-roster instrument.
- **Reference class (excluded from the throne):**
  `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2` -- the d8/nb2m oracle, 1146
  +/- 10 in the 2026-07-26 fit at 10x the node budget, plus its two `dil(...,d6)`
  variants at 1113 and 1112. Developer ruling 2026-07-16: deeper-at-same-budget
  and bigger-budget heads are reference, not targets.
- **How the previous champion fell (2026-07-26).** The book audit
  (`plans/book-opener-audit-results-1-vivid-lantern.md`, theory 38) found that the
  2026-07-18 certification rested on a record that does not reproduce: against its
  own bookless self the store read 29-3, decomposing into one run at 5-3 of 8 and
  one at 24-0 of 24, while fresh re-runs gave exactly 5-3 in each of eight 8-game
  processes and 34-30 (53%) in one 64-game process. That pair is deterministic, so
  the 53%-100% spread is cross-game TT state alone (`Docs/benchmarking.md`, defect
  3). Rather than re-run the old instrument, 13 new book agents were added to the
  roster and the whole cohort was boosted to 32 games/pair. The previous champion
  now rates 1075 +/- 9, sixth among target-class agents. Note the 8-games/pair fill
  had it FIRST at 1090 with the new leader at 1067: the top inverted on boosting,
  the third time that has happened here, which is why methodology note 2 exists.
- **OPEN SCRUTINY FLAG -- read before treating this as settled.** The throne has
  now changed hands twice between BOOK agents, and the open question is unchanged
  and unresolved: a book is a memorized line, not a generalized improvement to the
  evaluator or the search. This champion is the s98 linear value model riding a
  134-entry hard-coded book of its own past wins against one specific opponent.
  Theory 38 measured the mechanism directly: a book is keyed on position hash with
  no response tree, so it keeps firing only while the opponent repeats the replies
  it made when mined, and every large book lift in this project collapses under
  `pairgen --open-plies` diversification. The prediction, untested for THIS agent,
  is that book11's +82 goes to roughly zero once the opening varies.
  The strongest BOOKLESS target-class agent in this fit is
  `ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148)@1` at 1077 +/- 9, which is the
  number to use if a bookless throne is wanted. Developer decision still
  outstanding (raised 2026-07-18, `plans/dethrone-champion-results-5-wiggly-
  mitten.md`): should book-augmented agents be champion-eligible at all, or get a
  separate recognition track? The case for splitting the track is now stronger,
  since six of the top eight target-class agents wear a book.
- **What the agent is:** `learned(s98,5801570e)` (the v2 sparse piece-square
  linear value model, the 2026-07-17 champion's evaluator, unchanged) at the
  standard d6/tt/ord/nb200k head, wearing an opener that plays a stored reply from
  `models/book11.txt` whenever the current position matches, falling back to its
  own normal search otherwise. `models/book11.txt` is 134 entries mined by
  `rank.exe bookgen` from s98's OWN 25 winning replays (of 32 stored games)
  against `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`, capped at **16 half-
  moves**. It is the 16-ply rung of a 6/16/30/60-ply depth ladder
  (`models/book10,11,12,4.txt`), the first time book depth has been varied.
- **Nearest rivals (same fit, same head, all at 32 games/pair):**

  | Elo | Agent (loadout on its core) |
  |---|---|
  | 1110 +/- 10 | `...learned(s98,5801570e)@1.opener(book,10)@1` (6-ply rung, statistically tied) |
  | 1095 +/- 10 | `...learned(s98,5801570e)@1.opener(book,4)@1` (60-ply rung) |
  | 1092 +/- 10 | `...learned(s98,5801570e)@1.opener(book,2)@1` (classic's BORROWED book) |
  | 1077 +/- 9 | `...learned(s76,ef183148)@1` (bare, strongest bookless target-class agent) |
  | 1075 +/- 9 | `...classic(t1,c4,w0,l0)@2.opener(book,2)@1` (the deposed champion) |
  | 1040 +/- 9 | `...learned(s98,5801570e)@1` (this champion's own bare core, so the book is worth +82) |

- **The depth ladder is not monotonic and is not explained.** On the s98 core the
  rungs read 6-ply +70, 16-ply +82, 30-ply +9, 60-ply +55 over bare. The 30-ply
  rung sits 73 Elo below its 16-ply neighbour at +/- 10 each, which is far outside
  the error bars, and no mechanism has been tested. On the classic core the same
  ladder reads +54, +45, +71, +110, a different shape entirely. Do not describe
  book depth as having a trend until this is investigated.

## Certification methodology (summary; details in Docs/benchmarking.md)

1. Full-roster anchored refit is the instrument; gauntlets only screen.
2. A top-of-table claim needs every contender pair at >= 32 games. Boost
   with `ranking/roster_top.txt` (keep its contender list current), then
   refit. Never conclude from 8-games/pair fills: two preliminary reads have
   already inverted at 32 games/pair (phase 0 and phase 1 of the dethrone
   plan).
3. Compare order and error bands within ONE fit; never absolute Elo across
   fits.
4. Whenever the top may have changed (a new agent rates near the top, or a
   cohort of new IDs joins the pool), re-certify and update this file plus
   `todo.md`'s Agent Track goal paragraph in the same session.

## Lineage

| Reign | Champion | Certification | Doc |
|---|---|---|---|
| until 2026-07-17 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` (chip counter, bookless; earlier as its @1 identity) | dethroned at 976 +/- 13 vs s98's 1064 +/- 14, head-to-head 9-23 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-17 - 2026-07-18 | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` | certified 1064 +/- 14 (dethrone fit), re-confirmed 1074 +/- 14 (phase 1 fit), dethroned at 1074 +/- 12 vs the self-mined book's 1145 +/- 13, head-to-head 7-25 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-18 - 2026-07-26 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1` | certified 1145 +/- 13 in its own fit; dethroned 2026-07-26 at 1075 +/- 9 vs book11's 1122 +/- 10 (3.5 combined SE) | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
| 2026-07-26 - | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1.opener(book,11)@1` | certified 1122 +/- 10, 116-agent full-roster refit, contenders at 32 games/pair -- statistically TIED with book10 (1110 +/- 10) | `plans/book-opener-audit-results-1-vivid-lantern.md` |

## Defended challenges

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
