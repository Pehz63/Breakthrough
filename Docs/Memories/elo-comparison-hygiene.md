---
name: elo-comparison-hygiene
description: "Elo claims must use standings.tsv (active only, one head, loadout-matched); ratings.tsv retired rows caused a 91-Elo phantom gap"
metadata:
  node_type: memory
  type: project
  originSessionId: 9b2197c9-c8a4-4a00-98c2-319283f178f1
  modified: 2026-07-26T12:42:29.047Z
---

2026-07-25: three defects were found in how this project compares agent Elo. All three
produce plausible-looking numbers, so none is self-announcing.

1. **Retired rows.** `ranking/ratings.tsv` is the FULL historical fit and contains every
   agent ever rated, including retired ones (`active = gone`: superseded `@N` identities
   frozen at old game counts, often 196-504 games vs a then-smaller pool). Quoting one as
   current strength is wrong. Measured: a retired `classic(t1,c4,w0,l0)@1` row reads
   **1081** where its live `@2` identity reads **990** -- a 91-Elo phantom gap that
   inverted a conclusion (made a hill-climbed Advanced agent look worse than classic when
   at a matched head it is slightly better, 1036 vs 1009).
2. **Mixed search heads.** An agent is search + evaluator. `ab(d6,tt,ord,nb200k)` vs
   `ab(d6,ord,nb200k)` vs `ab(d8,tt,ord,nb2m)` are different agents; their Elos are not
   interchangeable even though rows look alike.
3. **Unmatched loadout.** The champion is a `classic` core wearing ONE loadout item
   (`.opener(book,2)`) worth **+124 Elo** on that core (990 bare -> 1114 equipped). Every
   learned/dist agent it was measured against is bare, so those comparisons read the
   loadout, not the evaluator.

**Fixes shipped same day:** `rank.exe rate` now also writes `ranking/standings.tsv`
(active agents only, grouped by search head, plus an `eff_evaluator` column that elides
the inert turn weight) -- READ THAT FILE, not `ratings.tsv`. `CLAUDE.md` gained
ranking-claim hygiene rules (5) and (6); `Docs/benchmarking.md` has the full explanation
under "Elo comparison hygiene" (7 rules, incl. checking margins against error bars: a
27-Elo gap at +/-11 and +/-10 is only ~1.8 SE, not a separation). 37 Elo-citing documents
carry an `[ELO HYGIENE UNVERIFIED]` banner pending re-evaluation, tracked in `todo.md`.

**Terminology adopted** (`Docs/terminology.md`): **core** (evaluator + search),
**loadout** (the optional add-ons: book, qs, tt, ord, asp, dilution -- readable off the
canonical ID), **bare/equipped**, **loadout-matched**, **lift** (Elo one item adds to one
core). Chosen over "vanilla/modded" (a loadout adds to a core, it does not modify it) and
over "basic/featured" ("feature" already means an ML input feature here).

**Book lift, corrected 2026-07-26.** In the fit, classic + its OWN book reads +124 and
classic + a foreign book reads -8, which was originally read as "books are not portable
across cores." That reading is REFUTED in both directions, see
[[book-opener-and-sample-size]]: at the fixed start a BORROWED book beat a self-mined one
(32-0 vs 16-16 for the hill-climbed `adv` core), and the whole effect collapses under
`pairgen --open-plies`. The +124 is real in the fit but is a fixed-start artifact, not
transferable strength, so it is also not a valid "loadout item worth +124 Elo" to match
against. See [[strength-benchmarking-instrument]].
