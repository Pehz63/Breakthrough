# Reigning Champion (single source of truth)

This file declares the current champion (the standing dethrone target).
Update it in the same session as any certification refit. Other docs point
here for "who is the champion" instead of embedding Elo numbers, because
absolute Elo drifts as the pool grows (see `Docs/benchmarking.md`, "Elo scale
drift across fits"). When another doc must quote a number, it tags it with
the fit date.

## Current champion

- **ID:** `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1`
- **Certified:** 2026-07-18, full-roster anchored Bradley-Terry refit with
  every contender pair at >= 32 games. Elo 1145 +/- 13.
- **Criterion:** highest pooled Elo among target-class agents (the
  standing-loop "outrating it outright" criterion), full-roster instrument.
- **Reference class (excluded from the throne):**
  `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2` -- the d8/nb2m oracle, 1159
  +/- 13 in the same fit at 10x the node budget. Developer ruling 2026-07-16:
  deeper-at-same-budget and bigger-budget heads are reference, not targets.
- **OPEN SCRUTINY FLAG -- read before treating this as settled.** This
  champion is the classic chip counter (this project's very first agent
  family) riding a 134-entry hard-coded book of its own past wins
  (`models/book2.txt`, mined by `rank.exe bookgen`). It is not a generalized
  improvement to the evaluator or search. Whether this represents genuine
  transferable strength or a pool-specific/memorization effect has NOT been
  resolved -- see `plans/dethrone-champion-results-5-wiggly-mitten.md`'s "open
  scrutiny question" section for both readings and the developer decision
  points it raises (should a book-augmented agent be champion-eligible at
  all, or get a separate counter/book recognition track).
- **What the agent is:** `classic(t1,c4,w0,l0)` (material + turn only, zero
  structure weight -- the original champion's evaluator, unchanged) at the
  standard d6/tt/ord/nb200k head, wearing an opener that plays a stored reply
  from `models/book2.txt` whenever the current position matches (keyed by
  canonical position hash), falling back to its own normal search otherwise.
  The book was mined from the classic agent's OWN historical wins against
  `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` (the prior champion) --
  7 reproducible wins out of 32 stored games. See theory 33
  (`Docs/theories.md`) for the mechanism and the prior champion's own entry
  below for its recipe.
- **Nearest rivals (same fit):** s98+book1 (the OLD, wrong-direction oracle
  book) 1092 +/- 13; s98+qs 1092 +/- 13; s98 (plain) 1074 +/- 12. The top is
  non-transitive; the pooled-Elo criterion is what the throne uses.

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
| 2026-07-18 - | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1` | certified 1145 +/- 13 -- SEE THE OPEN SCRUTINY FLAG ABOVE | `plans/dethrone-champion-results-5-wiggly-mitten.md` |

## Defended challenges

| Date | Challenger | Result | Doc |
|---|---|---|---|
| 2026-07-17 | s98 + quiescence (`ab(d6,tt,ord,qs,nb200k)...learned(s98)`) | pooled tie (1073 vs 1074), champion won the pair 23-9 | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | chip counter + quiescence | 1002 +/- 14, not close | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | s98 + oracle refutation book (`.opener(book,1)`) | 1059 +/- 14 (below plain s98), champion won the pair 18-14 | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | chip counter + oracle refutation book | 967 +/- 13, went 7-25 vs the champion (worse than bookless) | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | 6-seed mirror-symmetrized weight ensemble of the champion's recipe (slot9) | 924 +/- 12, -155 vs the champion | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-17 | mirror-symmetrized champion weights (slot10) | 944 +/- 12, mirroring alone cost 135 Elo | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-18 | classic + self-mined book (`.opener(book,2)`) vs s98 | **DETHRONED s98**: 1145 +/- 13 vs 1074 +/- 12, head-to-head 25-7 | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
