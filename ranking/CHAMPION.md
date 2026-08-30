# Reigning Champions (single source of truth)

> **[RE-CERTIFIED 2026-08-30]** All 6 categories re-certified under the `@2`
> identities the 2026-08-28 banner below retired. Method: rather than boosting
> a per-category contender subset, the ENTIRE roster (218 active agents,
> 23,653 pairs) was filled on a rung ladder, `--games` 8 then 16 then 32, via
> `.\tools\run_rank.ps1 -Workers 10 --games N` with no `--roster` filter,
> snapshotting `ranking/standings.tsv`/`ratings.tsv` after each rung into
> `ranking/recert_snapshots/`. Rank order was stable from rung 8 through rung
> 32 for every category's top field (openless x node's top 5 held identical
> rank across all three rungs; the largest churn anywhere was a 3-way swap
> within a sub-1-SE cluster), so 32 was taken as converged and `rank.exe check
> --games 32` confirms 0 pairs pending. This satisfies CLAUDE.md rule 2 (every
> contender pair >= 32 games) for all 6 categories in one fit, including the 4
> that never had a `roster_top*.txt` file. **Asymmetry worth stating plainly:**
> `opener8`/`dil20` contenders draw from `rand()` (their `.opener(rand,...)`/
> `.dil(...)` segment), so their 32/pair is 32 genuine direct games against
> every other category member. `openless` contenders are mutually
> deterministic (no dilution, no opener), so `pairGameTarget` caps any two of
> them at exactly 2 direct games regardless of the fill target -- their order
> is resolved by the Bradley-Terry fit's transitive inference through the
> rest of the (now much better-connected) roster, the same mechanism the
> 2026-08-27 openless certification already relied on via the anchor alone,
> just now through a far larger shared-opponent pool. Several categories
> reordered materially under `@2` (the TT fix's whole point, see the banner
> below); each category section states plainly which held and which changed.
> `roster_top.txt`/`roster_top_time.txt` were bumped to `@2` IDs this session
> but NOT used as the boost mechanism this round (the full-roster fill
> superseded them) and now list a stale top-N -- rebuild them from current
> `ranking/standings.tsv` before next relying on them for a contender-only
> study.

> **[IDENTITIES RETIRED 2026-08-28]** Every category champion below is declared
> on the `ab(...)` search head, and the `ab` explorer's code version was bumped
> `@1` -> `@2` this day to fix the TT cross-agent contamination defect
> (`Docs/corrections.md`, `TT CROSS-AGENT CONTAMINATION`). Module versioning
> re-identifies every agent using that module, `tt` and non-`tt` alike, so all
> six declarations below now name a `gone` identity frozen with zero games
> under the live roster: `ranking/roster.txt`'s alpha-beta lines all read `@2`
> and have not played a single game yet. The Elo numbers stand as an honest
> record of what was measured under `@1`; they are not a description of who
> currently holds any title, since nobody has played under `@2`. Re-certifying
> each category (replaying games until the field clears 32/pair again under the
> new identities) is open in `todo.md`. Do not quote any ID below as an active
> agent without checking `ranking/standings.tsv` first.

> **[RESTRUCTURED 2026-08-24]** The 5-category system (one throne per opener)
> is replaced by a 6-category system: 3 divisions (openless / opener8 / a new
> 20%-full-random dilution division, dil20) crossed with 2 compute-
> normalization tracks (the existing `nodes=200k` node budget / a new
> `time=150ms` wall-clock budget). Read "Why 6 categories" and "Summary
> (2026-08-24 fit, 6-category system)" below for the current state; the two
> banners immediately following this one, and everything under "Superseded
> summaries" and "Deferred categories", are historical and describe the prior
> 5-category system. A genuine instrumentation defect was found while building
> this round -- two cost-flagged evaluator cores do not respect the
> `time=150ms` budget (2-3.3x overshoot); see the Summary section's defect
> note before trusting any `x time` category's close margins.

> **[RE-CERTIFIED 2026-08-01 after a scoring-population change]** The match
> store no longer includes games involving the TD-Leaf Pass-2 candidates that
> were screened and never promoted (457,611 rows, 71% of the store). Their files
> were removed from `ranking/matches.index.txt` on 2026-08-01 and DELETED on
> 2026-08-02, by developer decision: a permanent ladder should not be dominated by games
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
>
> Because those files were never committed and have now been deleted, the
> pre-drop fit **cannot be reproduced**. The 2026-07-29 declarations below are
> permanently historical: there is no way to re-run the fit that produced them.
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

## Why 6 categories

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

**Restructured 2026-08-24** from 5 opener-only categories to 3 divisions x 2
compute tracks. Two things motivated this, alongside the developer's own
request to simplify the divisions: (1) the 2026-08-23 reference-class
revision (below) opened the door to a different search algorithm (Gumbel
MCTS, `gaz(...)`) competing for a title once certified, but a title comparison
across search algorithms is only meaningful if both sides spend comparable
compute -- otherwise "wins the category" just measures "has more compute,"
the exact arms-race incentive the reference-class rule already excludes for
depth/node-budget scaling within one algorithm; (2) the book divisions needed
a fundamentally different mining methodology (self-maximizing book mining
within a division's own field, not a fixed-pair replay) that hadn't been
designed yet, so they were demoted rather than carried forward half-designed.
See "Deferred categories" below for what specifically was dropped and why
nothing about it was deleted.

## Category definitions and eligibility rule

Excluding reference-class agents (defined below), a target-class agent's
canonical ID places it in exactly one of 3 divisions by its `.opener(...)`/
`.dil(...)` segment, crossed with exactly one of 2 compute tracks by its
head's budget flag -- 3 x 2 = 6 categories:

| Division | Rule |
|---|---|
| **openless** | no `.opener(...)` segment and no `.dil(...)` segment |
| **opener8** | `.opener(rand,moves=8)@1` |
| **dil20** | `.dil(prob=20)@1`, no `.opener(...)` segment |

| Track | Rule |
|---|---|
| **node** | head carries `nodes=200k` |
| **time** | head carries `time=150ms` |

`dil(prob=20)` is a PERCENTAGE (20% of moves fully randomized), not a
fraction -- `dil(prob=0.2)` is a different, valid ID (~0.2% dilution) that
`src/ranking.cpp`'s `lenientPct` grammar accepts silently, a real footgun
rather than a typo the parser would catch. Any other opener/dilution/budget
combination (the retired 4-ply/8-ply book categories, `.opener(rand,moves=4)@1`,
any other `dil(prob=...)` value, any other `nodes=`/`time=` value) is
ladder/study data, not a member of any of the 6 categories -- it holds no
title. See "Deferred categories" below for what this retires and why nothing
was deleted.

> **[ELIGIBILITY RULE REVISED 2026-08-23]** Category eligibility previously
> required an exact search-head match ("one-head rule inside the categories,"
> scoped to `ab(deep=6,tt,ord,nodes=200k)@1` only). That requirement is
> dropped. It had inherited CLAUDE.md's evaluator-attribution hygiene rule
> (fix one search head to isolate an evaluator's contribution to an Elo gap,
> "Champion declaration and ranking-claim hygiene," rule 6) as though it were
> also a title-eligibility rule. It isn't: that rule governs what a written
> comparison is allowed to conclude, not who may hold a title, and the two
> were conflated when the category system was built (2026-07-28) and stayed
> conflated until this revision. CLAUDE.md rule 6 is unchanged and still
> applies in full to any claim that one evaluator beats another.

**Reference class (the only eligibility exclusion, defined by compute, not
head).** An agent is reference class, and excluded from all 5 categories
regardless of its Elo, if its distinguishing advantage over the target-class
field is spending more search compute on an already-established technique: a
deeper depth, a bigger node or simulation budget, or otherwise scaling up
resources on a search approach already represented in the roster, rather than
a different approach. Currently: the d8/nb2m oracle,
`ab(deep=8,tt,ord,nodes=2m)@1.classic(chip=100)@2`, 1099 +/- 9 in the
2026-08-01 fit, ten times the node budget of the standard head, never a
target. This is the project's guard against a pure compute arms race that
rewards spending more resources over finding a better technique, and it is
the only reason an agent is excluded from category eligibility. A different
search algorithm entirely (a different explorer, for example Gumbel MCTS,
`gaz(...)`) is not reference class merely for being different, and is not
excluded by head identity alone. It competes for the title on its own merits
once it clears the general certification bar (32 games/pair, full-roster
anchored refit) like any other target-class agent. If such an agent wins a
category, that is evidence for investing further in that approach, not a
result to exclude.

**Open question, not yet resolved:** what counts as comparable compute when
the techniques being compared use incompatible units (`ab`'s node budget
against `gaz`'s simulation count) is undecided. Until it is, a reference-class
judgment call on a novel search approach should be stated explicitly in the
declaration, not inferred silently from a head-string mismatch.

**Consequence for existing rows:** `ab(deep=6,ord,nodes=200k)@1.adv(chip=77,...)@1`
(the no-`tt` `adv` core wearing 4-ply/8-ply books) is no longer excluded by
head identity. It is not reference class either, since dropping `tt` is not
more compute, if anything less. Moot for now regardless, since the book
divisions it would have entered are demoted -- see "Deferred categories"
below.

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
5. Category eligibility has no search-head restriction (revised 2026-08-23).
   Reference-class exclusion (item 6) is the only eligibility filter beyond
   category membership by opener.
6. Reference-class agents are excluded in every category, never a target:
   currently the d8/nb2m oracle, and any future agent whose sole advantage
   over the field is more search compute (depth, node or simulation budget)
   on an already-established technique rather than a different one.
7. Whenever the top may have changed, re-certify and update this file plus
   `todo.md`'s Agent Track goal paragraph in the same session.

## Rounds

- **Round 1 (2026-07-28):** 24 new agents (4 book-opener on `classic`/`s98`,
  20 random-opener on 10 cores), 116 -> 140 active agents, ~24,480 new games.
- **Round 2 (2026-07-29):** 18 more agents (4 more book-opener on `s3`/`adv`,
  14 more random-opener on the remaining cheap bare cores), 140 -> 158 active
  agents, ~21,384 new games. Every category's champion and runner-up from
  round 1 held its rank through round 2, but margins stayed thin (see below).
- **Round 3 (2026-08-24):** the 3-division x 2-track restructure. 56 new
  agents (the same 14-core set as round 1/2's random-opener cohort -- 13
  learned cores + the bare classic control -- each newly wearing
  `.dil(prob=20)@1` at the existing `nodes=200k` head, plus bare/`.opener(rand,
  moves=8)@1`/`.dil(prob=20)@1` at a new `time=150ms` head), played into the
  same store via `.\tools\run_rank.ps1 -Workers 10 --games 8 --paired-openings`.
  `time=150ms` is a measured value: the existing `nodes=200k` head averages
  ~6.0 ms/move for the bare chip counter and ~18.6 ms/move for the then-openless
  champion core (`ranking/matches.roster.0001.jsonl`, `wms`/`wmv` fields),
  and 150ms gives roughly the same headroom over that average that
  `nodes=200k` gives over its ~62k avg node use -- reproduce by summing `wms`/
  `wmv` and `bms`/`bmv` per agent over the store. `nodes=200k` is unchanged
  (every existing category already used it). 4-book/8-book/4-random are
  demoted, not replayed or removed -- see "Deferred categories" below.

## Summary (2026-08-30 fit, 6-category system, re-certified under `@2`)

Fit population: full match store after the whole-roster rung-ladder fill
(8/16/32 games/pair, see the 2026-08-30 banner above), 218 active agents,
`rank.exe rate` re-run against the current `ranking/matches.jsonl` plus
indexed parts. Read `ranking/standings.tsv` yourself to reproduce any row.

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) |
|---|---|---|---|---|
| **openless x node** | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` **co-champion with model=602** | **1292 +/- 12** | 3404 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=602,68cbb27d,tdleaf_self,lin,shape=129-1)@1`, 1284 +/- 12 (gap 8 / SE 17.0 = 0.5 SE -- **statistically tied**); model=349 a clear 3rd at 1227 +/- 10 (gap from co-champions 57-65 / SE 15.6 = 3.7-4.2 SE) |
| **opener8 x node** | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | **927 +/- 5** | 6944 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 919 +/- 5 (gap 8 / SE 7.1 = 1.1 SE -- **statistically tied**, same pair, same champion, as the pre-fix fit) |
| **dil20 x node** | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1` **TITLE CHANGED** | **682 +/- 5** | 6944 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.dil(prob=20)@1`, 670 +/- 5 (gap 12 / SE 7.1 = 1.7 SE); classic close behind at 669 +/- 5. The pre-fix champion, model=98, is now 4th at 664 +/- 5 |
| **openless x time** | `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2` **3-way co-champion with model=98, model=96** | **1122 +/- 9** | 3404 | model=98, 1119 +/- 9 and model=96, 1117 +/- 9 (max gap among the three 5 / SE 12.7 = 0.4 SE -- **statistically tied**); model=10 a clear 4th at 1090 +/- 8 (gap from top 32 / SE 12.4 = 2.6 SE) |
| **opener8 x time** | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | **928 +/- 5** | 6944 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 924 +/- 5 (gap 4 / SE 7.1 = 0.6 SE -- **statistically tied**, same pair, same champion, as the pre-fix fit) |
| **dil20 x time** | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1` **RESOLVED** | **696 +/- 5** | 6944 | `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2.dil(prob=20)@1`, 675 +/- 5 (gap 21 / SE 7.1 = 3.0 SE). The pre-fix fit had this exact pair as an exact tie at 558/558 |

### Evidence level, stated honestly

**All 6 categories are certified at >= 32 games/pair** (2026-08-30, whole-roster
rung-ladder fill, `rank.exe check --games 32` confirms 0 of 23,653 pairs
pending; `CLAUDE.md` rule 2 satisfied for every category, not just a
contender subset). Two categories' resolution mechanism differs and is worth
restating plainly rather than implying uniform strength: **opener8 and dil20
contenders draw from `rand()`** (their `.opener(rand,...)`/`.dil(...)`
segment), so their 32/pair is 32 genuine direct games against every other
category member -- the strongest form of evidence this project produces.
**openless contenders are mutually deterministic**, so `pairGameTarget` caps
any two of them at exactly 2 direct games; their order above is the
Bradley-Terry fit's transitive inference through the wider roster (the same
mechanism the 2026-08-27 partial certification used via one anchor agent,
now run through a much larger connected pool). Both openless categories
resolved to a statistical tie at the top rather than a single champion --
report them as co-champions, not as one agent with two names for the same
rank, since the gap sits under 1 combined SE.

**dil20 x node's title changed under the TT fix**: the pre-fix champion
(model=98) drops to 4th; the pre-fix runner-up (model=76) is displaced from
2nd by model=10, which did not even chart in the pre-fix top field at this
loadout. **dil20 x time's exact pre-fix tie resolved**: model=10 now leads
classic by 3.0 combined SE, a real separation the pre-fix 8-games/pair
screening fit could not have detected. **opener8 x node and opener8 x time
held their pre-fix champion and their statistically-tied runner-up
unchanged** -- the TT fix moved absolute Elo (compare 777/791 pre-fix to
927/928 here, not meaningful across fits per rule 3) but not this pair's
order or its tie.

**Time-budget instrumentation defect, found while building the 2026-08-24
round, still present under `@2`.** Two cores (`model=111`, `model=113`,
both wide-MLP position-oracle heads, already flagged `# cost flag` in
`ranking/roster.txt`) do not respect the `time=150ms` budget: measured
(this fit) at 436-439 ms/move in openless x time, 284-293 ms/move in
opener8 x time, and 365-376 ms/move in dil20 x time (`cpu_ms_move` column,
`ranking/standings.tsv`) -- roughly 1.9-2.9x over budget, against every other
core in the field staying under it. Neither is top-2 in any `x time`
category here either, so this does not change any declaration above, but
their rows are still not evidence of a budget-respecting agent. Root
cause: `src/ai_minimax.cpp`'s `budgetTripped()` checks the wall-clock deadline
only once every 4096 nodes (`(nodes & 4095ULL) == 0`, a deliberate
`Clock::now()`-overhead tradeoff), and the outer iterative-deepening loop has
no check before starting a new depth iteration -- so an expensive-per-node
evaluator can start a whole iteration with no time left and run well past the
deadline before the coarse in-recursion check catches it. This is a genuine
engine gap, not a data artifact: it was invisible before this round because no
prior roster line ever combined `time=` with a slow evaluator. It does not
change any category's declared champion above (`model=111`/`113` are not
top-2 in any `x time` category), but their rows within the three `x time`
categories are not evidence of a budget-respecting agent and should be
excluded from any claim resting on compute parity within those categories
until the underlying check is fixed. Tracked in `todo.md`; not fixed this
session (design-only discussion, no `src/` changes made). Does not affect the
`x node` track at all -- `g_nodeDeadline` is checked on every node, with no
granularity gap.

## Superseded summaries

Kept for lineage. **Do not quote these numbers as current.**

### 2026-08-24 fit, 6-category system, before the TT-contamination fix

Superseded 2026-08-30 by the whole-roster re-certification above after the
`ab` explorer's `@1` -> `@2` TT-contamination fix retired every alpha-beta
agent's game history (see the 2026-08-28/2026-08-30 banners at the top of
this file). Fit population: full match store after round 3's 56-agent
addition, 218 active agents.

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) |
|---|---|---|---|---|
| **openless x node** | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | **1036 +/- 10** | 1975 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1`, 995 +/- 8 (gap 41 / SE 12.8 = 3.2 SE) |
| **opener8 x node** | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | **777 +/- 9** | 1840 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 765 +/- 9 (gap 12 / SE 12.7 -- statistically tied) |
| **dil20 x node** | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` | **553 +/- 10** | 1736 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=111,78ef6974,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1.dil(prob=20)@1`, 543 +/- 10 (gap 10 / SE 14.1 -- statistically tied) |
| **openless x time** | `ab(deep=6,tt,ord,time=150ms)@1.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1` | **982 +/- 14** | 1052 | `ab(deep=6,tt,ord,time=150ms)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1`, 939 +/- 14 (gap 43 / SE 19.8 = 2.2 SE) |
| **opener8 x time** | `ab(deep=6,tt,ord,time=150ms)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | **791 +/- 9** | 1736 | `ab(deep=6,tt,ord,time=150ms)@1.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 788 +/- 9 (gap 3 / SE 12.7 -- statistically tied) |
| **dil20 x time** | `ab(deep=6,tt,ord,time=150ms)@1.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1` | **558 +/- 10** | 1736 | `ab(deep=6,tt,ord,time=150ms)@1.classic(chip=100)@2.dil(prob=20)@1`, exact tie at 558 +/- 10 |

**openless x node and openless x time were certified at 32 games/pair**
(2026-08-27, `ranking/roster_top.txt` / `ranking/roster_top_time.txt`, a
top-N contender pool at their head). The other four categories sat at
8-11 games/pair, screening level only.

### 5-category system, pre-2026-08-24

The category system itself changed on 2026-08-24 (see "Why 6 categories"
above); openless/8-random above already supersede this table's own rows for
those two divisions specifically. 4-book/8-book/4-random are carried forward
verbatim into "Deferred categories" below rather than re-quoted here.

### 2026-08-01 fit, after the scoring-population change

Fit population: the `roster` and `retired_other` store parts (191,823 games
before the openless boost run, 192,639 after), 170 active agents.

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) | vs 2026-07-29 |
|---|---|---|---|---|---|
| openless | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | 1044 +/- 11 | 1603 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1`, 1007 +/- 8 (gap 37 / SE 13.6 = 2.7 SE) | CHANGED (was s76) |
| 4-book | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=15)@1` | 968 +/- 11 | 1392 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=13)@1`, 947 +/- 11 (gap 21 / SE 15.6 = 1.3 SE) | held |
| 8-book | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=16)@1` | 989 +/- 11 | 1392 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=14)@1`, 964 +/- 11 (gap 25 / SE 15.6 = 1.6 SE) | held |
| 4-random | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1` | 965 +/- 11 | 1392 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=4)@1`, 949 +/- 11 (gap 16 / SE 15.6 -- statistically tied) | held |
| 8-random | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | 781 +/- 10 | 1392 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 773 +/- 10 (gap 8 / SE 14.1 -- statistically tied) | held |

**openless was the only category boosted to rule-2 fill this round**: 32
stored rows/pair measured at ~22.6 distinct games/pair (0.706 distinct
trajectories/row, since these bookless/openerless contenders consume no
`rand()`), so the printed SEs understate by ~1.19x -- the true openless gap
was ~2.3 combined SE, not 2.7. The other four categories sat at 1392 games,
well under 32/pair, unboosted. `s169` took the openless title while losing 36%
of its games (it is itself a TD-Leaf agent and the removed cohort was its own
family); the boost run confirmed the result survived proper fill (1050 +/- 12
at 8 games/pair -> 1044 +/- 11 at 32), but why removing cohort games helps a
cohort member, and why the bare chip counter swung from rank 2 to rank 35
under the same change, were never explained.

### 2026-07-29 fit, round 2, screening level

| Category | Champion (loadout on its core) | Elo +/- SE | Games | Nearest rival (gap / combined SE) |
|---|---|---|---|---|
| openless | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1` | 1012 +/- 9 | 2136 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=6,eac8ab99,pool_games,lin,shape=129-1)@1`, 1002 +/- 9 (gap 10 / SE 12.7 -- statistically tied) |
| 4-book | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=15)@1` | 972 +/- 12 | 1256 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=13)@1`, 941 +/- 11 (gap 31 / SE 16.3) |
| 8-book | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=16)@1` | 997 +/- 12 | 1256 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=14)@1`, 958 +/- 12 (gap 39 / SE 17.0) |
| 4-random | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1` | 968 +/- 12 | 1256 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=4)@1`, 955 +/- 11 (gap 13 / SE 16.3 -- statistically tied) |
| 8-random | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | 782 +/- 11 | 1256 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1`, 770 +/- 11 (gap 12 / SE 15.6 -- statistically tied) |

Only 4-book and 8-book cleared ~2 combined SE; the other three were inside 1
SE. Growing the roster (round 2) did not resolve the close calls -- 4-random
and 8-random got closer, not further apart, as more cores joined.

---

## Category: openless x node

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`
  **co-champion with model=602** (statistically tied, gap 0.5 combined SE).
- **Elo:** 1292 +/- 12 (3404 games), 2026-08-30 fit, whole-roster
  certification at >= 32 games/pair (mutually deterministic against every
  other openless contender, so resolved by transitive inference through the
  rest of the roster rather than direct head-to-head fill -- see the
  2026-08-30 banner and Summary section above).
- **What it is:** a TD-Leaf self-play linear value model. No loadout item at
  all: this is the core's bare identity, at the `nodes=200k` node-budget track.
- **Division/track:** this is the same `openless` division this file has
  tracked since 2026-07-28, now also labeled by the `node` compute track it
  was always implicitly running at (`nodes=200k`, unchanged) -- carried
  forward, not a new category needing new games.
- **Nearest rivals (this fit, same head, node track):**

  | Elo | Agent |
  |---|---|
  | 1284 +/- 12 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=602,68cbb27d,tdleaf_self,lin,shape=129-1)@1` (gap 8, combined SE 17.0 = 0.5 SE -- co-champion) |
  | 1227 +/- 10 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=349,5ee50d5c,tdleaf_self,lin,shape=129-1)@1` (clear 3rd: gap 65, combined SE 15.6 = 4.2 SE) |
  | 1181 +/- 10 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=603,2da121ee,tdleaf_self,lin,shape=129-1)@1` |
  | 1147 +/- 9 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=351,42d88d8e,tdleaf_self,lin,shape=129-1)@1` |
  | 1124 +/- 9 | `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2` (control, 9th) |

- **Lineage:** founding declaration 2026-07-28 (1030 +/- 9), 2026-07-29 round 2
  (1012 +/- 9), 2026-08-01 re-certification after the scoring-population
  change (1044 +/- 11, s169 took the title), 2026-08-24 restructure (1031 +/-
  10, s169 held, now also labeled `x node`), 2026-08-27 32-games/pair
  certification of the top-12 field (1036 +/- 10, s169 held), 2026-08-28 `@1`
  -> `@2` TT-fix retirement (game history reset to zero), 2026-08-30
  whole-roster re-certification (1292 +/- 12, s169 now a statistically-tied
  co-champion with s602 rather than a clear leader -- s76, the prior clear
  runner-up at 3.2 SE, fell to 16th at 1032 +/- 8).
- **Defended challenges:** none yet.

## Category: opener8 x node

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1`
- **Elo:** 927 +/- 5 (6944 games), 2026-08-30 fit, whole-roster certification
  at >= 32 direct games/pair against every other contender (this agent draws
  from `rand()` via its opener, so unlike the openless categories this fill is
  genuine direct head-to-head evidence, not transitive inference).
- **What it is:** the openless-champion lineage's own core (s76) wearing
  `.opener(rand,moves=8)@1` (uniform-random for its own first 8 plies), then
  handing off to its real search. This is the same category this file called
  "8-random" through 2026-08-01; the division rule is unchanged, only the name
  and its explicit `node`-track label are new as of the 2026-08-24
  restructure.
- **Nearest rivals (this fit, same head, node track):**

  | Elo | Agent |
  |---|---|
  | 919 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1` -- statistically tied (gap 8, combined SE 7.1 = 1.1 SE) |
  | 892 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1` (clear 3rd: gap 35, combined SE 7.1 = 4.9 SE) |
  | 880 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2.opener(rand,moves=8)@1` (control) |
  | 879 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=111,78ef6974,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1.opener(rand,moves=8)@1` |
  | 879 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=8,6f1a4264,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1` |

- **Lineage:** founding declaration 2026-07-28 as "8-random" (790 +/- 12),
  2026-07-29 round 2 (782 +/- 11), 2026-08-24 restructure (777 +/- 9, same
  agent, renamed `opener8 x node`), 2026-08-28 `@1` -> `@2` TT-fix retirement,
  2026-08-30 whole-roster re-certification (927 +/- 5, s76 held the title and
  its statistically-tied margin over s10 unchanged in kind, only in absolute
  Elo which is not comparable across fits per rule 3).
- **Defended challenges:** none yet.

## Category: dil20 x node

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1`
  **-- TITLE CHANGED from the pre-fix champion, model=98 (now 4th).**
- **Elo:** 682 +/- 5 (6944 games), 2026-08-30 fit, whole-roster certification
  at >= 32 direct games/pair against every other contender (this agent draws
  from `rand()` via its dilution, so this fill is genuine direct head-to-head
  evidence, not transitive inference).
- **What it is:** a weight-merge-trained linear value model wearing
  `.dil(prob=20)@1`: 20% of moves fully randomized throughout the game, not
  just an opening phase. `dil(prob=20)` is a PERCENTAGE, not a fraction --
  `dil(prob=0.2)` silently parses as ~0.2% dilution instead (`src/
  ranking.cpp`'s `lenientPct`), a footgun avoided here deliberately.
- **Nearest rivals (this fit, same head, node track):**

  | Elo | Agent |
  |---|---|
  | 670 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.dil(prob=20)@1` (gap 12, combined SE 7.1 = 1.7 SE) |
  | 669 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2.dil(prob=20)@1` (control; gap from champion 13, combined SE 7.1 = 1.8 SE) |
  | 664 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` (the pre-fix champion, now 4th) |
  | 663 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` |
  | 657 +/- 5 | `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=3,68364898,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` |

- **Lineage:** founding declaration 2026-08-24 (round 3, s98 champion, 553 +/-
  10), 2026-08-28 `@1` -> `@2` TT-fix retirement, 2026-08-30 whole-roster
  re-certification: **title changed to s10** (682 +/- 5), which did not chart
  in the pre-fix top field at this loadout at all; s98 dropped to 4th.
- **Defended challenges:** none yet.

## Category: openless x time

- **ID:** `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2`
  **3-way co-champion with model=98, model=96** (statistically tied, max gap
  0.4 combined SE).
- **Elo:** 1122 +/- 9 (3404 games), 2026-08-30 fit, whole-roster certification
  at >= 32 games/pair (mutually deterministic against every other openless
  contender, so resolved by transitive inference through the rest of the
  roster rather than direct head-to-head fill).
- **What it is:** the bare chip counter -- no loadout, no learned evaluator --
  at the `time=150ms` wall-clock budget track. `time=150ms` was chosen from a
  measurement (not guessed): the `nodes=200k` head averages ~6.0 ms/move for
  the bare chip counter and ~18.6 ms/move for the then-openless champion core,
  and 150ms gives roughly the same headroom over that average that
  `nodes=200k` gives over its own ~62k avg node use (see "Rounds" above for
  the reproduction command).
- **Nearest rivals (this fit, same head, time track):**

  | Elo | Agent |
  |---|---|
  | 1119 +/- 9 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1` (gap 3, combined SE 12.7 = 0.2 SE -- co-champion) |
  | 1117 +/- 9 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1` (gap 5, combined SE 12.7 = 0.4 SE -- co-champion; this agent held the pre-fix title outright) |
  | 1090 +/- 8 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1` (clear 4th: gap from top 32, combined SE 12.4 = 2.6 SE) |
  | 1068 +/- 8 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=3,68364898,pool_games,lin,shape=129-1)@1` |
  | 1042 +/- 8 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1` |

- **Time-budget defect applies to this category's field:** `model=111`/`113`
  do not respect the `time=150ms` budget, measured 436-439 ms/move here --
  see the Summary section's defect note. Well below the champion field, no
  effect on this declaration.
- **Lineage:** founding declaration 2026-08-24 (round 3, screening level, 977
  +/- 14). 2026-08-27 32-games/pair certification of the full field (982 +/-
  14, s96 held outright). 2026-08-28 `@1` -> `@2` TT-fix retirement.
  2026-08-30 whole-roster re-certification (1122 +/- 9): the bare chip
  counter, s98, and s96 are now a 3-way statistical tie at the top -- s96 no
  longer holds the title outright, but remains a co-champion.
- **Defended challenges:** none yet.

## Category: opener8 x time

- **ID:** `ab(deep=6,tt,ord,time=150ms)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1`
- **Elo:** 928 +/- 5 (6944 games), 2026-08-30 fit, whole-roster certification
  at >= 32 direct games/pair against every other contender (this agent draws
  from `rand()` via its opener, so this fill is genuine direct head-to-head
  evidence, not transitive inference).
- **What it is:** the openless x node lineage's core (s76) wearing
  `.opener(rand,moves=8)@1`, at the `time=150ms` track instead of
  `nodes=200k`.
- **Nearest rivals (this fit, same head, time track):**

  | Elo | Agent |
  |---|---|
  | 924 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1` -- statistically tied (gap 4, combined SE 7.1 = 0.6 SE) |
  | 895 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1` (clear 3rd: gap 33, combined SE 7.1 = 4.7 SE) |
  | 875 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1` |
  | 874 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2.opener(rand,moves=8)@1` (control) |
  | 874 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=8,6f1a4264,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1` |

- **Time-budget defect applies to this category's field:** `model=111`/`113`
  rate at 284-293 ms/move here, roughly 2x over budget -- see the Summary
  section's defect note. Well below champion and runner-up, no effect on this
  declaration.
- **Lineage:** founding declaration 2026-08-24 (round 3), 2026-08-28 `@1` ->
  `@2` TT-fix retirement, 2026-08-30 whole-roster re-certification (928 +/- 5,
  s76 held the title and its statistically-tied margin over s10 unchanged in
  kind).
- **Defended challenges:** none yet.

## Category: dil20 x time

- **ID:** `ab(deep=6,tt,ord,time=150ms)@2.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1`
  **-- RESOLVED from the pre-fix exact tie.**
- **Elo:** 696 +/- 5 (6944 games), 2026-08-30 fit, whole-roster certification
  at >= 32 direct games/pair against every other contender (this agent draws
  from `rand()` via its dilution, so this fill is genuine direct head-to-head
  evidence, not transitive inference).
- **What it is:** a weight-merge-trained linear value model wearing
  `.dil(prob=20)@1`, at the `time=150ms` track.
- **Nearest rivals (this fit, same head, time track):**

  | Elo | Agent |
  |---|---|
  | 675 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2.dil(prob=20)@1` (control; gap 21, combined SE 7.1 = 3.0 SE -- resolved, was an exact tie pre-fix) |
  | 674 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` |
  | 673 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.dil(prob=20)@1` |
  | 669 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` |
  | 653 +/- 5 | `ab(deep=6,tt,ord,time=150ms)@2.learned(model=3,68364898,pool_games,lin,shape=129-1)@1.dil(prob=20)@1` |

- **Time-budget defect applies to this category's field:** `model=111`/`113`
  rate at 365-376 ms/move here, roughly 2.5x over budget -- see the Summary
  section's defect note. Well below champion and runner-up, no effect on this
  declaration.
- **Lineage:** founding declaration 2026-08-24 (round 3, s10/classic exact
  tie at 558/558), 2026-08-28 `@1` -> `@2` TT-fix retirement, 2026-08-30
  whole-roster re-certification: the tie resolved, s10 leading classic by 3.0
  combined SE (696 vs 675).
- **Defended challenges:** none yet.

---

## Deferred categories

4-book, 8-book, and 4-random are not part of the active 6-category system as
of the 2026-08-24 restructure ("Why 6 categories" above): book divisions need
a self-maximizing mining methodology that hasn't been designed yet, and the
4-ply opener division was folded away when the active random-opener division
moved to 8-ply. Nothing here was deleted or replayed -- these are the same
declarations this file carried before the restructure, verbatim, kept as
ladder/study data. `ranking/roster.txt`'s rows for these agents are untouched
and still `on`.

### Category (deferred): 4-book

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=15)@1`
- **Elo:** 972 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** the pre-split single champion's evaluator (`learned(model=98,...)`,
  v2 sparse piece-square linear value model, outcome-trained) wearing a 4-ply
  self-mined book, `models/book15.txt` (22 entries), mined 2026-07-28 via
  `rank.exe bookgen --plies 4` from s98's own 25-of-32 kept winning replays vs
  `classic(chip=100)@2` -- the exact same already-stored match history that
  produced its 6/16/30-ply siblings (book10/11/12), so mining needed no new
  games. Lift over s98's bare core in this SAME fit: 972 - 973 = **-1 Elo**,
  noise -- the shallowest book tested on this core is not distinguishable from
  no book at all.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 941 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=13)@1` (4-ply, classic-own; lift over classic's bare 921 = +20) |
  | 928 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=3,68364898,pool_games,lin,shape=129-1)@1.opener(book,book=17)@1` (4-ply, s3-own, added round 2; lift over s3's bare 987 = **-59**, consistent with the existing on-record finding that s3's OWN book costs it Elo while a borrowed one doesn't, theory 38) |

- **Previously excluded by head, eligibility revised 2026-08-23 (see the
  category eligibility rule above):**
  `ab(deep=6,ord,nodes=200k)@1.adv(chip=77,support=-2,control=1,noiseseed=1,racewin=1)@1.opener(book,book=19)@1`
  (4-ply, adv-own, added round 2), 952 +/- 11 in the 2026-07-29 fit. No longer
  excluded by head identity alone, and not reference class (dropping `tt` is
  not more compute). Below this category's 972 champion in that same fit, so
  the title does not change, but this needs re-checking against the current
  fit rather than assumed.
- **Lineage:** founding declaration 2026-07-28 (982 +/- 12), re-quoted
  2026-07-29 after round 2 (972 +/- 12, same agent, same rank). Deferred
  2026-08-24 (division demoted, not superseded by a new champion).
- **Defended challenges:** none yet.

### Category (deferred): 8-book

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=16)@1`
- **Elo:** 997 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** same core as the 4-book champion, wearing an 8-ply self-mined
  book, `models/book16.txt` (60 entries), mined 2026-07-28 via `rank.exe bookgen
  --plies 8` from the same already-stored s98-vs-classic match history as
  book15 (no new games). Lift over s98's bare core (973): **+24 Elo**.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 958 +/- 12 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=14)@1` (8-ply, classic-own; lift over classic's bare 921 = +37) |
  | 938 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=3,68364898,pool_games,lin,shape=129-1)@1.opener(book,book=18)@1` (8-ply, s3-own, added round 2; lift over s3's bare 987 = **-49**, same direction as its 4-ply rung) |

- **Previously excluded by head, eligibility revised 2026-08-23 (see the
  category eligibility rule above):**
  `ab(deep=6,ord,nodes=200k)@1.adv(chip=77,support=-2,control=1,noiseseed=1,racewin=1)@1.opener(book,book=20)@1`
  (8-ply, adv-own, added round 2), 988 +/- 12 in the 2026-07-29 fit. No longer
  excluded by head identity alone, and not reference class (dropping `tt` is
  not more compute). This reads ABOVE that fit's 8-book champion (997), so
  under the revised rule this category's title was unsettled pending a direct
  re-check against the current standings; moot now that the division is
  deferred rather than re-checked.
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
  2026-07-29 after round 2 (997 +/- 12, same agent, same rank). Deferred
  2026-08-24.
- **Defended challenges:** none yet.

### Category (deferred): 4-random

- **ID:** `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1`
- **Elo:** 968 +/- 12 (1256 games), 2026-07-29 full-roster fit.
- **What it is:** a v2 sparse piece-square linear value model (outcome-trained,
  one of the s94-s99 multi-seed replicate family) wearing `.opener(rand,moves=4)@1`:
  uniform-random for its own first 4 plies, then hands off to its real search.
  New identity in this pool as of round 1 (same ID text has separate history in
  `ranking/roster_open.txt`'s incompatible-scale pool; this is independent
  history in the merged pool). Lift over its own bare core (971, this fit):
  **-3 Elo**, noise.
- **Nearest rivals (this fit, same head, all 1256 games):**

  | Elo | Agent |
  |---|---|
  | 955 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=4)@1` -- statistically tied (gap 13, combined SE 16.3); note s76 topped openless AND 8-random but not 4-random |
  | 943 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=6,eac8ab99,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1` |
  | 934 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1` |
  | 932 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(rand,moves=4)@1` (control; lift over bare classic 921 = +11) |
  | 910 +/- 11 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=9,e5b3b014,value,lin,129-1,con100)@1.opener(rand,moves=4)@1` (added round 2) -- **RETIRED 2026-07-30**: models/sweep/slot9.txt was accidentally overwritten (gitignored, unrecoverable); this rating is historical only, the identity can never play again |

- **Round 2 did not change the leader**, but tightened the gap to the runner-up
  (18 / SE 17.0 in round 1 -> 13 / SE 16.3 now) -- the opposite of what more
  data should do if the true gap were real; this category was never resolved
  before being deferred.
- **Lineage:** founding declaration 2026-07-28 (988 +/- 12), re-quoted
  2026-07-29 after round 2 (968 +/- 12, same agent, same rank). Deferred
  2026-08-24 (division folded into `opener8`).
- **Defended challenges:** none yet.

---

## Pre-split history (single-champion era, until 2026-07-28)

Kept as a permanent record. These tables describe the ONE-champion regime and
are not re-quoted or extended after the split. The final single champion,
`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=11)@1` (a 16-ply
book, certified 2026-07-26 at 1122 +/- 10), does not fall into either new book
category under the split's exact taxonomy (4-ply / 8-ply only) -- it retires
with no title in any of the 5 tracks, staying in `ranking/roster.txt` as
depth-ladder data (see `tools/CLAUDE.md`'s "Mined books" table).

### Lineage

| Reign | Champion | Certification | Doc |
|---|---|---|---|
| until 2026-07-17 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2` (chip counter, bookless; earlier as its @1 identity) | dethroned at 976 +/- 13 vs s98's 1064 +/- 14, head-to-head 9-23 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-17 - 2026-07-18 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1` | certified 1064 +/- 14 (dethrone fit), re-confirmed 1074 +/- 14 (phase 1 fit), dethroned at 1074 +/- 12 vs the self-mined book's 1145 +/- 13, head-to-head 7-25 | `plans/dethrone-champion-results-1-wiggly-mitten.md` |
| 2026-07-18 - 2026-07-26 | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=2)@1` | certified 1145 +/- 13 in its own fit; dethroned 2026-07-26 at 1075 +/- 9 vs book11's 1122 +/- 10 (3.5 combined SE) | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
| 2026-07-26 - 2026-07-28 | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=11)@1` | certified 1122 +/- 10, 116-agent full-roster refit, contenders at 32 games/pair -- statistically TIED with book10 (1110 +/- 10) | `plans/book-opener-audit-results-1-vivid-lantern.md` |
| 2026-07-28 - | *(split into 5 category champions, see above)* | -- | `plans/champion-split-plan-1-kind-beaming-cerf.md` / `champion-split-results-1-kind-beaming-cerf.md` |

### Defended challenges

| Date | Challenger | Result | Doc |
|---|---|---|---|
| 2026-07-17 | s98 + quiescence (`ab(deep=6,tt,ord,qs,nodes=200k)...learned(model=98)`) | pooled tie (1073 vs 1074), champion won the pair 23-9 | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | chip counter + quiescence | 1002 +/- 14, not close | `plans/dethrone-champion-results-2-wiggly-mitten.md` |
| 2026-07-17 | s98 + oracle refutation book (`.opener(book,book=1)`) | 1059 +/- 14 (below plain s98), champion won the pair 18-14 | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | chip counter + oracle refutation book | 967 +/- 13, went 7-25 vs the champion; the original "worse than bookless" reading is withdrawn (7-25 is 4-10 on 14 distinct games vs bookless 9-23 on a genuine 32) | `plans/dethrone-champion-results-3-wiggly-mitten.md` |
| 2026-07-17 | 6-seed mirror-symmetrized weight ensemble of the champion's recipe (slot9) | 924 +/- 12, -155 vs the champion | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-17 | mirror-symmetrized champion weights (slot10) | 944 +/- 12, mirroring alone cost 135 Elo | `plans/dethrone-champion-results-4-wiggly-mitten.md` |
| 2026-07-18 | classic + self-mined book (`.opener(book,book=2)`) vs s98 | **DETHRONED s98**: 1145 +/- 13 vs 1074 +/- 12, head-to-head 25-7 | `plans/dethrone-champion-results-5-wiggly-mitten.md` |
| 2026-07-26 | s98 + its OWN 16-ply self-mined book (`.opener(book,book=11)`) | **DETHRONED classic+book2**: 1122 +/- 10 vs 1075 +/- 9 | `plans/book-opener-audit-results-1-vivid-lantern.md` |
| 2026-07-26 | s98 + own book at 6 / 30 / 60 ply (`book=10` / `book=12` / `book=4`) | 1110 / 1049 / 1095. The 6-ply rung ties book11. The 30-ply rung is an unexplained 73-Elo dip | same |
| 2026-07-26 | classic + own book at 6 / 16 / 30 ply (`book=7` / `book=8` / `book=9`) | 1019 / 1010 / 1036 vs bare 965 and 60-ply 1075, so lift grows with depth on this core | same |
| 2026-07-26 | s98 + classic's BORROWED book2 | 1092 +/- 10, within 0.2 SE of its own 60-ply book (1095) | same |
| 2026-07-26 | `learned(model=3,68364898)` + its own book6 | 1030 vs 1063 bare: its OWN book COSTS 33 Elo, while classic's borrowed book2 is neutral (1064) | same |
| 2026-07-26 | `adv(chip=77,...)` + its own book3 (no-TT head) | 911 vs 1018 bare: its OWN book costs **107 Elo**, while borrowed book2 is +15 | same |
