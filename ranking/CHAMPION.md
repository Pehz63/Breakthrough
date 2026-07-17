# Reigning Champion (single source of truth)

This file declares the current champion (the standing dethrone target).
Update it in the same session as any certification refit. Other docs point
here for "who is the champion" instead of embedding Elo numbers, because
absolute Elo drifts as the pool grows (see `Docs/benchmarking.md`, "Elo scale
drift across fits"). When another doc must quote a number, it tags it with
the fit date.

## Current champion

- **ID:** `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1`
- **Certified:** 2026-07-17, full-roster anchored Bradley-Terry refit with
  every contender pair at >= 32 games. Elo 1074 +/- 14 in the phase-1 fit,
  1075 +/- 13 in the phase-2 fit.
- **Criterion:** highest pooled Elo among target-class agents (the
  standing-loop "outrating it outright" criterion), full-roster instrument.
- **Reference class (excluded from the throne):**
  `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2` -- the d8/nb2m oracle, 1187
  +/- 21 in the same fit at 10x the node budget. Developer ruling 2026-07-16:
  deeper-at-same-budget and bigger-budget heads are reference, not targets.
- **Nearest rivals (same fit):** s98+qs 1073 +/- 15 (pooled tie, lost the
  direct pair 9-23); s96 1061 +/- 14 (wins its head-to-head with s98 20-12
  but rates below on the pool); adv(c75,h3,r2,g1) 1023; ord-classic 1022.
  The top is non-transitive; the pooled-Elo criterion is what the throne
  uses.
- **What the agent is:** a linear v2 piece-square value model (129 sparse
  binary inputs: 64 White + 64 Black occupancy + side-to-move; logistic
  outcome training; tanh*900 output) trained on d8/nb2m-oracle vs
  old-champion pairgen games, searched by alpha-beta depth 6 with
  transposition table, move ordering, and a 200k node budget. Full recipe:
  `plans/vs-champion-training-results-1-cozy-forest.md`.

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
| until 2026-07-17 | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` (chip counter; earlier as its @1 identity) | dethroned at 976 +/- 13 vs s98's 1064 +/- 14, head-to-head 9-23 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-17 - | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` | certified 1064 +/- 14 (dethrone fit), re-confirmed 1074 +/- 14 (phase 1 fit) | `plans/dethrone-champion-results-1-wiggly-mitten.md` |

## Defended challenges

| Date | Challenger | Result | Doc |
|---|---|---|---|
| 2026-07-17 | s98 + quiescence (`ab(d6,tt,ord,qs,nb200k)...learned(s98)`) | pooled tie (1073 vs 1074), champion won the pair 23-9 | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | chip counter + quiescence | 1002 +/- 14, not close | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | s98 + oracle refutation book (`.opener(book,1)`) | 1059 +/- 14 (below plain s98), champion won the pair 18-14 | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | chip counter + oracle refutation book | 967 +/- 13, went 7-25 vs the champion (worse than bookless) | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
