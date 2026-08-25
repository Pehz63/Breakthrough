# Restructure CHAMPION.md to a 3-division x 2-track (6-category) system

## Context

`ranking/CHAMPION.md` currently splits the champion title into 5 categories
purely by opener (openless / 4-book / 8-book / 4-random / 8-random), all on
one fixed search head. Earlier this session the "one-head" eligibility rule
was revised (2026-08-23): category eligibility no longer requires an exact
search-head match, only a "reference class" compute-arms-race exclusion. That
revision was motivated by wanting a different search algorithm (GAZ, Gumbel
MCTS) to be able to compete for a title on its own merits once it's strong
enough, rather than being disqualified by head identity alone.

The developer wants to act on that opening by (a) simplifying the opener-based
divisions from 5 to 3 (openless / `.opener(rand,moves=8)@1` / a new 20%
full-random dilution division, `dil(prob=20)@1` -- book divisions deferred to
their own future design pass, since self-maximizing book mining needs a
genuinely new system) and (b) crossing those 3 divisions with 2
compute-normalization tracks (a node-visit budget and a wall-clock time
budget), so both `ab(...)` and, eventually, `gaz(...)` agents can compete on
comparable compute. That's 3 x 2 = 6 categories.

**This pass is AB-only** (developer decision): the 6-category restructure
ships using only `ab(...)` agents, since it needs zero `src/` changes. The
work required to let a `gaz(...)` agent actually enter one of these categories
(a node/time-budget concept GAZ doesn't have today) is a separable piece of
work, deferred to its own future pass and scoped in the appendix below so the
research already done isn't lost -- it's also independently blocked on GAZ
clearing Pass 3 certification (`todo.md`), which hasn't started.

**Confirmed with the developer:**
- AB-only now; GAZ instrumentation is a later, separate pass.
- The random-opener division is `moves=8` (`.opener(rand,moves=8)@1`), not 6
  -- this is exactly today's existing "8-random" category, unchanged.
- Dilution division uses `dil(prob=20)@1` -- the correct grammar is a
  PERCENTAGE integer, not a fraction (`dil(prob=0.2)@1` would silently parse
  as ~0.2% dilution, a real footgun caught while researching this plan;
  confirmed against `ranking.cpp`'s `lenientPct` and existing roster usage
  like `dil(prob=25)@1`).
- Shared time-budget value: `time=150ms`, based on a real measurement from
  the match store (not guessed): the existing node-budgeted standard head
  averages ~6.0 ms/move for the bare chip counter and ~18.6 ms/move for the
  current openless champion core (`ranking/matches.roster.0001.jsonl`, `wms`/
  `wmv` fields). 150ms gives roughly the same headroom over the champion
  core's average that `nodes=200k` already gives over its ~62k avg node use.
- Node-budget value: unchanged, `nodes=200k` (same number every existing
  category already uses -- no new decision, preserves continuity).

## What ships in this pass

Two of the six categories already exist and need no new games:
- **openless x node** = today's openless category, unchanged.
- **opener8 x node** = today's "8-random" category, unchanged (just kept
  instead of retired alongside 4-random).

Four are new and need new roster lines + new games:
- **dil20 x node**: `ab(deep=6,tt,ord,nodes=200k)@1.<core>@N.dil(prob=20)@1`
- **openless x time**: `ab(deep=6,tt,ord,time=150ms)@1.<core>@N`
- **opener8 x time**: `ab(deep=6,tt,ord,time=150ms)@1.<core>@N.opener(rand,moves=8)@1`
- **dil20 x time**: `ab(deep=6,tt,ord,time=150ms)@1.<core>@N.dil(prob=20)@1`

Each new block uses the same established core set already rostered for the
existing 4-random/8-random cohort (`ranking/roster.txt:260-326`: ~13 evaluator
cores + the bare classic control), matching that section's own "vary one axis
at a time" convention. No `src/` or `tests/` changes are needed anywhere in
this pass -- `ab(...)` already fully supports `time=`/`nodes=`/`dil(prob=...)`
(all pre-existing, tested grammar and runtime behavior); this is a pure
roster + docs change. Per this project's own testing rule, a commit touching
no `src/`/`tests/`/build-affecting file makes the test suite a no-op --
`.\tools\run_tests.ps1 -Build` does not need to be run before committing this
pass, though running it once is a reasonable sanity check.

4-book/8-book/4-random are demoted, not deleted, following this project's own
existing precedent (`CHAMPION.md`'s `book11` retirement): they keep their
existing sections, lineage tables, and historical numbers verbatim in a new
"Deferred categories" appendix, and simply stop being read into the active
title table. No roster lines are touched or removed.

## Steps

1. **`ranking/roster.txt`**: add the 4 new blocks above (dil20-node,
   openless-time, opener8-time, dil20-time), each as a `# ---...---` banner +
   1-2 sentences of rationale + `on <id> # <label>` lines, mirroring the
   existing champion-split cohort's format (`roster.txt:243-326`). Also fix
   the stale round-6 GAZ cohort banner (`roster.txt:382-393`), which currently
   says GAZ "is not eligible for a CHAMPION.md title regardless of Elo" per
   the one-head rule -- that's now factually wrong since the 2026-08-23
   revision; reword it to say eligibility is no longer head-restricted, but
   these checkpoints stay `off` pending GAZ's own budget instrumentation (a
   future pass, see appendix) and Pass 3 certification (`todo.md`), not the
   old one-head rule.
2. **Play the new games**: `.\tools\run_rank.ps1 -Workers N` (or the
   project's standard sharded-play invocation), following the existing
   "Rounds" precedent in `CHAMPION.md` for how a cohort addition is played
   into the shared store, then `rank.exe rate`.
3. **Rewrite `ranking/CHAMPION.md`**: replace the 5-category system with the
   6-category table (3 divisions x 2 tracks), using the real post-play
   numbers from step 2. Update "Why N categories" to add the compute-track
   motivation alongside the existing opener-split motivation (keep that
   history, don't replace it). Add a short "shared budget values" subsection
   stating `nodes=200k` / `time=150ms` and reproducing the measurement
   command, so it's reproducible later rather than just asserted. Update the
   certification methodology's eligibility rule to state the compute-track
   cross explicitly. Add the "Deferred categories" appendix for
   4-book/8-book/4-random per the demotion note above.
4. **`CLAUDE.md`** (root): update the one sentence in the "Champion
   declaration and ranking-claim hygiene" rule that names the 5 categories by
   name, to the new 6-category description. No other part of that rule
   changes.
5. **`todo.md`**: update the Agent Track goal paragraph's category count/
   names, and add a line in the Move-Tree Explorers/GAZ section noting the
   budget-instrumentation work is scoped (appendix below) but not started,
   tracked as its own future item alongside the existing Pass 3 line.
6. **`Docs/ranking-workflow.md`**: line ~73 describes `roster_screening_pool`
   as spanning "all 5 opener categories" -- update to the current category
   set, or note the pool itself hasn't been rebuilt against the new taxonomy
   yet if that's true.
7. **Companion results doc**: archive this plan and a paired results doc in
   `plans/` per the project's standing workflow, with the real Elo numbers,
   distinct-game-count caveats, and what (if anything) differed from this
   plan during execution.
8. **Commit.**

### Critical files
- `ranking/roster.txt`
- `ranking/CHAMPION.md`
- `CLAUDE.md` (root)
- `todo.md`
- `Docs/ranking-workflow.md`
- `plans/` (new plan + results doc)

## Verification

- `rank.exe check` after the roster edit: confirms no parse errors on the new
  lines and the expected active-agent count increase.
- After play + rate, `ranking/standings.tsv` contains all new IDs at both new
  heads (`time=150ms` alongside `nodes=200k`) with plausible game counts.
- Spot-check that `time=150ms` games actually differ in behavior from
  `nodes=200k` games for at least one slow core (e.g. a wide-MLP dist model)
  by comparing `g_lastBudgetKind`/effective depth in a couple of stored rows,
  confirming the time cap is actually binding somewhere, not just present in
  the ID.
- Each of the 6 categories' declared champion in the rewritten `CHAMPION.md`
  is the verified highest-Elo `standings.tsv` row in its bucket, per this
  project's own certification discipline.

---

## Appendix: GAZ budget instrumentation design (deferred, future pass)

Not implemented in this pass. Captured here so the research isn't lost when
this becomes relevant (closer to GAZ's Pass 3 certification).

**The hard problem:** Gumbel MCTS (`src/ai_gumbel.cpp`) has no node-visit or
wall-clock budget concept today -- only a simulation-count budget (`sims=N`,
reusing the same int field `ab()` uses for depth). Its Sequential Halving
round schedule (`gumbelHalvingRounds`) is computed up front from that sim
budget, not checked incrementally, so a budget can't simply decrement a
per-node counter the way AB's `g_nodeDeadline` does.

**Resolved design:** cut only at round boundaries, never mid-round. Round 0
(`ai_gumbel.cpp:304`'s loop, `r=0`) always completes unconditionally,
guaranteeing progress under any budget, mirroring how AB's iterative
deepening always completes depth 1. Starting at `r=1`, check the budget
before starting that round; if exhausted, stop. This preserves Sequential
Halving's exact fairness invariant for every round that does run (every
survivor gets an identical sim share that round) -- a mid-round interrupt
would instead systematically starve later-indexed candidates in the
deterministic iteration order, a real bias, not just a coarser measurement.
`chosen = cand[0]` (`ai_gumbel.cpp:343`) already picks correctly from
whichever round the loop last completed with no special-casing needed.
Overshoot is bounded to roughly one round's worth of sims (verified against
`gumbelHalvingRounds`' allocation), consistent with this project's own
"Pass-1 discipline, correctness first" note already in `ai_gumbel.h`.

**Counter design:** thread `unsigned long long& nodes, & leafs` through
`gumbelSimulate` (mirroring `ai_minimax.cpp`'s `nodes++`/`leafs++`
convention exactly, not a hidden global), increment `nodes` once per call,
`leafs` at each of its four true leaf-event returns. `gumbelSearch` declares
local counters, counts the root's own initial evaluation, passes them through
every simulation call, and writes `g_lastNodes`/`g_lastLeafs`/
`g_lastEffDepth` (rounds completed, not a ply-depth analog)/`g_lastBudgetKind`
once at the end -- which makes `ranking.cpp`'s existing telemetry gate
(`g_lastNodes>1 && brain==BRAIN_SEARCH`, `ranking.cpp:2592-2594`) start
recording GAZ match-row telemetry for free, no caller changes needed.

**Grammar:** add `nodes=`/`time=` flags to `gaz()`'s flag loop in
`parseAgentId` (`ranking.cpp:957-986`) and `rankAgentId`'s `gaz` branch
(`:589-600`), reusing `ab()`'s own `nb`/`tbMs`/`fmtBudget`/`lenientBudget`
machinery verbatim (those locals are already declared once, shared across the
`ab`/`gaz` branch split, so this is a pure addition with no restructuring).
Existing `gaz(...)` ids are unaffected (no `@N` version bump needed), since
`nodes=`/`time=` default to off exactly like `ab()`'s do.

**Test coverage needed then:** `tests/test_gumbel.cpp` (budget-cut behavior,
`BUDGET_NODE`/`BUDGET_TIME` reporting, round-0-always-completes, per-agent
`agentChooseMove` save/restore) and `tests/test_ranking.cpp` (grammar
round-trip, canonical ordering, duplicate/bad-value rejection for the new
flags), following each file's existing test patterns for `gaz()` and Gumbel
search respectively.
