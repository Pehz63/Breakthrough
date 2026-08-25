# Results: restructure CHAMPION.md to a 3-division x 2-track (6-category) system

Companion to `plans/champion-category-restructure-plan-1-golden-painting-anchor.md`.

## Summary of changes

- **`ranking/roster.txt`**: added 56 agents (round 3, 2026-08-24) -- the
  established 14-core set (13 learned cores + the bare classic control) each
  newly wearing `.dil(prob=20)@1` at the existing `nodes=200k` head, plus
  bare / `.opener(rand,moves=8)@1` / `.dil(prob=20)@1` at a new `time=150ms`
  head. Also fixed the stale round-6 GAZ cohort banner, which still cited the
  pre-revision one-head rule.
- **`ranking/CHAMPION.md`**: replaced the 5-category (opener-only) system with
  a 3-division x 2-track, 6-category system. Added a 2026-08-24 top banner,
  rewrote "Why 6 categories", replaced the category-definitions table,
  replaced the eligibility rule's exact-head language (already revised
  2026-08-23 earlier this session) with the compute-track cross, wrote a new
  Summary table from a fresh full-roster fit, moved the two prior summary
  tables under "Superseded summaries," relabeled the openless/8-random
  detail sections to `openless x node` / `opener8 x node` (same lineage,
  same games, just now also carrying the track label), added four new
  detail sections for the new categories, and moved 4-book/8-book/4-random
  into a new "Deferred categories" appendix, verbatim.
- **`CLAUDE.md`** (root): updated the category-count sentence in the
  "Champion declaration and ranking-claim hygiene" rule, and added the
  clarification that CLAUDE.md's one-head evaluator-attribution rule and
  `CHAMPION.md`'s title-eligibility rule are separate (2026-08-23 work,
  confirmed still accurate against the final restructure).
- **`todo.md`**: updated the Agent Track goal paragraph for the 6-category
  system, and added two `[Next]` items under Move-Tree Explorers/GAZ: the
  already-scoped-but-unbuilt GAZ budget instrumentation design, and a new
  entry for the AB time-budget granularity defect found this round (below).
- **`Docs/ranking-workflow.md`**: noted that `roster_screening_pool.txt`
  still describes the pre-restructure taxonomy and hasn't been rebuilt.
- **`plans/`**: this doc plus the plan copy.

No `src/` or `tests/` changes were made. Per this project's own testing rule,
a commit touching no `src/`/`tests/`/build-affecting file makes
`.\tools\run_tests.ps1 -Build` a no-op for this commit; it was not run.

## How to test

- `.\rank.exe check` -- confirms the roster parses with no errors: 218 active
  agents, 23653 pairs, 0 pending at `--games 8` (all round-3 games already
  landed).
- `.\rank.exe rate` -- regenerates `ranking/standings.tsv` from the current
  match store; every number quoted in `CHAMPION.md`'s Summary table and the
  six category detail sections can be reproduced by grepping that file for
  the quoted IDs.
- Read `ranking/CHAMPION.md` end to end and confirm no section still asserts
  a 5-category framing as current (the two "Superseded summaries" and the
  "Deferred categories" appendix are explicitly historical, everything else
  should read as 6-category).

## Results: the 6 category champions (2026-08-24 fit)

Fit population: 218 active agents, full match store (round 1-3 games plus
everything before them). Reproduce with `rank.exe rate`, read
`ranking/standings.tsv`.

| Category | Champion | Elo +/- SE | Games | Runner-up gap / combined SE |
|---|---|---|---|---|
| openless x node | s169 (tdleaf_self) | 1031 +/- 10 | 1967 | 40 / 12.8 = 3.1 SE |
| opener8 x node | s76 (position_elo) | 777 +/- 9 | 1840 | 12 / 12.7 -- tied |
| dil20 x node | s98 (pool_games) | 553 +/- 10 | 1736 | 10 / 14.1 -- tied |
| openless x time | s96 (pool_games) | 977 +/- 14 | 1028 | 43 / 19.8 = 2.2 SE |
| opener8 x time | s76 (position_elo) | 791 +/- 9 | 1736 | 3 / 12.7 -- tied |
| dil20 x time | s10 (weight_merge) | 558 +/- 10 | 1736 | 0 -- exact tie with the classic control |

Full canonical IDs and nearest-rival tables are in `ranking/CHAMPION.md`; not
re-quoted here per this project's own hygiene rule against embedding numbers
in more than one place without a fit-date tag. **Only openless x node and
opener8 x node approach anything like a settled result** (3.1 SE and a
long-standing lineage, respectively); the other four are screening-level,
8 games/pair nominal, well short of the 32-games/pair certification standard.

## Implementation differences from the plan

The plan anticipated a straightforward roster-add-and-play pass with zero
`src/` changes, which held. What the plan did not anticipate: **the
`time=150ms` budget itself turned out to be broken for two of the 14 cores**,
discovered only because this was the first roster line ever combining `time=`
with an expensive-per-node evaluator. This became the dominant topic of the
session's second half.

## Correctness gotchas discovered and how resolved

- **`dil(prob=N)` is a percentage, not a fraction.** Caught during planning,
  before any games were played -- `dil(prob=0.2)@1` would have silently
  parsed as ~0.2% dilution instead of 20%, since `src/ranking.cpp`'s
  `lenientPct` grammar accepts it without complaint. All roster lines and
  docs use `dil(prob=20)@1` throughout.
- **AB's `time=` budget does not cap wall-clock cost for expensive-per-node
  evaluators.** `model=111`/`model=113` (both wide-MLP position-oracle
  heads, `mu_shape=129-512-8-1`, already flagged `# cost flag` in the roster
  from an earlier NNUE-shaped-head study) measured at 477-500 ms/move in
  openless x time, 309-320 ms/move in opener8 x time, and 413-438 ms/move in
  dil20 x time -- against a 150ms budget, roughly 2-3.3x over. All 12 other
  cores in the same cohort stayed under budget. Root cause, confirmed by
  reading `src/ai_minimax.cpp`: `budgetTripped()` checks the wall-clock
  deadline only once every 4096 nodes (`(nodes & 4095ULL) == 0`, a
  deliberate `Clock::now()`-overhead tradeoff, not an oversight -- the
  comment on the line above says so), and the outer iterative-deepening loop
  in `miniMaxWhite` has no check before starting the next depth iteration.
  An expensive evaluator can therefore start an entire new iteration with
  the deadline already passed and run well past it before the coarse
  in-recursion check fires. **Not fixed this session** -- this was a design
  discussion with the developer (see `todo.md`'s new `[Next]` entry for the
  scoped fix: a pre-iteration check mirroring the round-boundary check
  already designed for GAZ's Sequential Halving). Does not change any
  category's declared champion, since the two affected cores are not top-2
  anywhere. Does not affect the `x node` track at all, since `g_nodeDeadline`
  is checked on every node with no granularity gap.
- **A related, now-resolved false alarm:** mid-session, the bare
  openless-x-time row for `model=111` appeared to have zero rows in
  `ratings.tsv`/`standings.tsv` despite 1028 matches.jsonl rows recorded for
  it. This was not a second bug -- `rank.exe rate` simply hadn't been re-run
  since those games landed. A fresh `rank.exe rate` (run while writing this
  doc) resolved it; the row is present and correctly rated (661 +/- 12).

## Future Work

- **Fix the AB time-budget granularity defect** (`src/ai_minimax.cpp`,
  `budgetTripped()` / `miniMaxWhite`'s iterative-deepening loop). Would
  confirm or refute whether `model=111`/`113` are actually competitive once
  correctly budgeted -- their node-track sibling (dil20 x node, unaffected by
  this defect) already rates them at 543 and 503 respectively, close to the
  553 champion, so a correct time-budget enforcement might move them
  meaningfully within the `x time` categories rather than leaving them stuck
  near the bottom on an artifact of running unbudgeted.
- **Boost the four new categories to 32 games/pair** before treating any of
  their declared champions as more than a screening-level signal, per this
  project's own certification standard (`CLAUDE.md` rule 2). Currently all
  four sit at 8 games/pair nominal.
- **Design and implement GAZ budget instrumentation** (scoped in the plan's
  appendix, not built) before any `gaz(...)` agent can enter a category.
  Independent of the AB fix above, but the two share a common fix shape
  (check the budget before committing to the next coarse unit of work,
  whether a depth iteration or a Sequential Halving round) worth keeping
  consistent when both are eventually built.
- **`ranking/roster_screening_pool.txt` still describes the pre-restructure
  taxonomy** and has not been rebuilt against the 6-category system
  (`Docs/ranking-workflow.md` now notes this). Rebuilding it would let
  gauntlet-based screening reflect the current categories.

## Ideas This Inspired

- A cost-weighted node/sim budget (an expensive leaf counts as N cheap-leaf
  equivalents) would fold the `x node` and `x time` tracks closer together
  and sidestep the whole `Clock::now()`-overhead-vs-granularity tradeoff, at
  the cost of needing a calibrated per-evaluator weight.
- The project already has evidence (the NNUE-shaped-head study) that wide
  MLP architectures like `model=111`/`113` are prediction-neutral, not
  actually smarter, just slower -- a narrower retrain of the same recipe
  might recapture close to all of its budget-fit cost for free. Worth a
  dedicated small study once the time-budget bug itself is fixed, so the
  comparison isn't confounded by the enforcement defect.
- GAZ's `sims` is already trained matched-to-serving per existing project
  practice, and theory 48's interior optimum (100-400 sims productive) means
  a budget-driven downward adjustment to `sims` may cost near-zero Elo if it
  stays inside that range -- worth checking directly once GAZ has any
  budget concept to test against.
