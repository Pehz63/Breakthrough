# Corrections index

The enumerable list of **defect classes** found in this project's own past
writing. One entry per class. Analogous to a Wikipedia tracking category or the
RFC index: the affected documents carry a banner, and this file is what makes
them countable in one place instead of only discoverable by stumbling into them.

**Read this before quoting a number out of any `plans/` document.** A document
carrying a banner is not worthless, it is *unverified in a specific way*, and
each entry below says exactly which way and what to do about it.

## How the convention works

Two mechanisms, deliberately separate because they answer different questions.

**1. Document banner: "is this document sound?"** A blockquote at the very top
of the affected document:

```
> **[<DEFECT NAME> - flagged <YYYY-MM-DD>]** <what was wrong, what it means for
> this document's numbers, and what a reader should do instead.>
```

`<DEFECT NAME>` is SHORT, UPPERCASE, and reused verbatim across every affected
document so `grep -rn "<DEFECT NAME>"` finds the whole set. Register the name in
this file. Never invent a second name for a class that already has one.

**2. Point-of-citation note: "is this claim safe to quote?"** The banner marks
where a claim was *analysed*. It does not travel to where the claim is *quoted*,
and quoting sites are where the damage happens (see the `SELF-PLAY CONVERGENCE
UNSUPPORTED` entry, which is a case of exactly that). So at each place that
states the conclusion - a `CLAUDE.md` table row, a script header, a `todo.md`
summary - add a short inline note naming the defect class and pointing here:

```
(see <DEFECT NAME>, Docs/corrections.md)
```

This is the documentation equivalent of a deprecation warning firing at the use
site rather than the definition. Keep the original claim text visible in both
cases: the wrong claim is the reason the mark exists, and a later reader needs to
recognise it if they meet it quoted somewhere that was missed.

---

## `ELO HYGIENE UNVERIFIED` - flagged 2026-07-25

**38 documents.** Elo comparisons written before the ranking-claim hygiene rules
existed. Three defects, detailed in `Docs/benchmarking.md`:

1. Numbers read from `ranking/ratings.tsv`, which mixes RETIRED agents
   (`active = gone`, superseded `@N` identities frozen at old game counts) with
   live ones. Read `ranking/standings.tsv` instead. This alone produced a
   91-Elo phantom gap.
2. Agents compared across different SEARCH HEADS, which are different agents, so
   the comparison is not an evaluator result.
3. Stored ROWS counted instead of DISTINCT games. `rand()` is consumed only by
   dilution and random-move agents, so a pair with neither replays one game per
   colour however many rows exist. Median 0.438 distinct games per row in the
   fixed-start store, so printed `pm` is understated by roughly 1.5x.

**What to do:** treat any Elo number in a banner-carrying document as
provisional. Re-derive from the current `ranking/standings.tsv` before quoting.
Affected documents are found with `grep -rl "ELO HYGIENE UNVERIFIED"`.

## `SELF-PLAY CONVERGENCE UNSUPPORTED` - flagged 2026-07-29

**The claim:** "single-teacher self-play converges/plateaus at 500 games",
sourced from `models/sweep/scaling.csv` and `tools/train_scaling.ps1`.

**Why it does not hold.** The entire self-play arm is four rows:

| games | seed 1001 | seed 2002 | mean |
|---|---|---|---|
| 250 | 536 | 442 | 489 |
| 500 | 541 | 469 | 505 |

`train_scaling.ps1` stopped because the +16 mean gain fell under its
`-ConvergeElo 20` rule. But the seed spread WITHIN a size is 94 Elo (250) and 72
Elo (500), so the threshold is smaller than the noise it is thresholding and
fires at the first rung by construction. Each point is a 4-games/pair gauntlet at
+/- 29-31, understated ~1.5x by defect 3 above. **No size above 500 was ever
run**, so there is no evidence about the curve's shape past 500 in either
direction. The ladder stopped; it did not converge. Filed as theory 45.

**It also does not transfer to online regimes at all**, which is a separate point
from the resolution problem. It measures a FIXED teacher generating a FIXED
distribution, which is precisely why saturation is expected there. An online
bootstrapped learner (TD-Leaf) changes its own generator every game, so the
distribution keeps moving and the saturation argument has no purchase.

**Why this entry exists at all.** The correct reading was ALREADY recorded, in
`plans/training-sweep-results-1-luminous-snail.md` item 3 on 2026-07-24, which
said the stop "triggered on noise, not on convergence" and warned "do not trust
'self-play plateaus at 500'". It was contradicted anyway on 2026-07-29 while
planning the TD-Leaf study, because none of the places the result gets quoted
from carried the caveat. That is the failure this file and the point-of-citation
note exist to prevent, and it is why the two mechanisms are separate.

**What to do:** do not cite a self-play game-count ceiling. Before reusing
`train_scaling.ps1`, either raise `-Seeds` until the seed band sits below
`-ConvergeElo`, or drop the early stop and rate a fixed ladder end to end, which
is what `tools/tdleaf_study.ps1` does via `train.exe --ckpt-at`.

**Affected:** `plans/training-sweep-results-1-luminous-snail.md` (source),
`tools/CLAUDE.md`, `tools/train_scaling.ps1`, `todo.md` (citation sites).

---

## `TT CROSS-AGENT CONTAMINATION` - flagged 2026-08-27

**Scope: every stored game played between two agents that both carry `tt`, up to
2026-08-27.** 169 of the 217 active roster agents carry it. This is a code defect
rather than a writing defect, so unlike the other entries here it is not a class of
sloppy claim, it is a class of number that the engine itself produced wrongly.

The transposition table is ONE process-wide table keyed by the position hash alone
(`src/transposition.cpp`), and `ttClear()` runs once per GAME, not per move. Both
players of a game therefore searched through the same table, and a position hash
records WHICH position was searched, not WHO searched it. Two wrong reads follow,
both reachable in ordinary ranked play:

1. **Cross-evaluator.** White's agent stores a score its evaluator produced. Black's
   agent probes the same position, gets a key match, and returns White's evaluator's
   number as its own. That is not a cache hit, it is a different value function's
   answer.
2. **Cross-strength.** `ttProbe` accepts any entry whose stored depth is at least the
   prober's remaining depth, so a shallower agent reads a deeper agent's entries and
   plays above its own depth.

**How it was found.** Mining refutation lines (`rank.exe refute`), because a book
plays its line back without searching at all, so any dependence of the opponent's
replies on OUR side having searched shows up immediately as the line failing to
reproduce. 132 of 238 mined lines stopped reproducing, and the split was exactly on
the opponent's own flag: all 132 had a `,tt,` opponent, and 0 of the 77 non-`tt`
opponents were affected. A two-setting control on 8 targets with an oracle identical
but for the flag gave 0 of 8 lines leaving the book tt-free versus 8 of 8 with `tt`,
and both runs still reported a winning record, because the wearer's brain silently
covered for the book once it fell out.

**Fixed** the same day by mixing a searcher context (evaluator index, the full
`evalParams` array, `g_useQuiescence`, and the root side to move) into the TT key,
which gives each player a disjoint region of the one table. `tests/test_ai_integration.cpp`
carries a regression test, validated to fail with the fix reverted.

**What this does and does not license.** It does NOT mean past Elo numbers are
wrong by a known amount. What it does mean: a stored `tt`-vs-`tt` game is not
reproducible by the current binary, so anything derived by REPLAYING those games
(`rank.exe extract` training data, `bookgen` books, `cbookdump` cluster books) was
derived under the defect, and any claim that rests on a `tt` agent's exact node
count or effective depth is measuring a quantity that included the opponent's work.
Re-measure rather than re-quote.

**Impact measured 2026-08-28**, as a reproducibility proxy (not yet a direct Elo
refit): replaying a 3000-game sample of stored `tt`-vs-`tt` games under the fixed
binary gave a 30.9% outcome-mismatch rate (705 of 2284 replayable games), against a
13.5% baseline (24 of 178) on a same-size sample of non-`tt`-vs-`tt` games (unaffected
by this defect, since contamination requires both sides to carry `tt`) replayed the
same way. The baseline is nonzero because ANY code change since a stored game was
recorded, not only this fix, can make its replay diverge, so the two rates are not
directly subtractable into a clean "TT-caused" fraction, but the gap (z approx 6.4)
is far too large to be that baseline noise alone. This confirms the defect changed a
substantial share of `tt`-vs-`tt` outcomes, not merely a rare edge case.

**Consequence: the `ab` explorer's code version was bumped 1 -> 2** the same day
(`src/ranking.cpp`'s `g_rkExplorers` table), per the module-versioning scheme
documented at that table. This re-identifies every alpha-beta agent, `tt` and
non-`tt` alike (the versioning scheme has no finer grain than per-module), so
`ranking/roster.txt`'s 216 active `ab(...)` lines now read `@2` and carry zero
games; their entire pre-fix history sits under the frozen `@1` identity, `gone` in
a refit. See `ranking/CHAMPION.md` for what this means for the category champions
and `todo.md` for the re-certification task.

---

## `TIME BUDGET NOT ENFORCED` - flagged 2026-09-01

**Scope: every stored game played by an agent whose search head carries a
`time=` budget, up to 2026-09-01.** 274,260 of the 815,597 loaded store rows
(33.6%) involve at least one, all of them on the single head
`ab(deep=6,tt,ord,time=150ms)@2`. Like `TT CROSS-AGENT CONTAMINATION`, this is a
code defect rather than a writing defect: a class of number the engine produced
wrongly, not a class of sloppy claim.

`budgetTripped()` (`src/ai_minimax.cpp`) sampled the wall clock once per 4096
nodes and kept no memory of the answer. A node that found the deadline already
past returned true, but the other 4095 nodes in that window never asked, so they
recursed to full depth regardless. The iterative-deepening loop had no check at
all before starting a new depth iteration, so a search that had 19 ms left would
begin a depth costing hundreds of milliseconds. `time=` therefore did not bound
anything, it was a soft hint the search overshot by a factor that grew with the
core's per-node cost.

**Measured pre-fix overshoot**, one chip-counting core
(`ab(deep=12,tt,ord,time=Xms)@2.classic(chip=100)@2`) at three settings, serial
play on a scratch roster, realized ms/move against the flag:

| flag | pre-fix ms/move | post-fix ms/move |
|---|---|---|
| `time=50ms` | 193.78 (3.9x over) | 20.18 |
| `time=150ms` | 900.30 (6.0x over) | 58.52 |
| `time=450ms` | 3217.37 (7.1x over) | 180.89 |

Post-fix the realized ratios are 2.90x and 3.09x against a flag ratio of 3.0x,
so the flag now controls the spend. The pre-fix run was stopped after 9 of 12
games because it was unbounded by construction, so the numbers above are its
per-move means over the games it completed.

**What this does and does not license.** It does NOT mean the time track's Elo
numbers are wrong by a known amount, and it does NOT touch the node track: a
`rank.exe determinism --replicas 2` run over 16 `nodes=200k` agents (32
subject-colours) produced byte-identical TSVs before and after the fix, node
counts included. What it does mean:

1. **A `time=` agent's stored per-move cost is not what its ID claims.** Any
   claim resting on the time track's realized ms/move, nodes/move, or effective
   depth is measuring a search that ran past its budget. Re-measure.
2. **The time track was not a wall-clock instrument.** At `deep=6` almost every
   core finished depth 6 before the budget mattered, so the head behaved as a
   fixed-depth head for all but the slowest evaluators, which is why the node
   and time tracks recorded near-identical node counts for the same core
   (41,841 versus 41,819 nodes/move). Do not describe pre-fix time-track results
   as a compute-normalized comparison.
3. **Some stored `time=` games no longer replay.** Measured 2026-09-01 as a
   reproducibility proxy, same design as the `TT CROSS-AGENT CONTAMINATION`
   measurement: replaying a random sample of stored rows under the fixed binary
   via `rank.exe extract` gave 13 of 182 replayable games mismatching (7.14%,
   Wilson 95% CI 4.2% to 11.8%) for rows involving a `time=150ms` agent, against
   0 of 240 (0.0%) for rows involving none (Fisher two-sided p = 1.4e-05). The
   control's zero rate means there is no measurable general replay drift today,
   so the 7.14% is attributable to this fix rather than to a floor. 29.5% of the
   affected population involves `model=111`, `model=113`, or `model=96`, the
   three cores a determinism probe independently showed changing play, which is
   consistent with the effect being concentrated in the cores slow enough that
   depth 6 never fit inside 150 ms.

**The `ab` explorer's code version was NOT bumped.** This is the opposite call
from `TT CROSS-AGENT CONTAMINATION` and the reasoning is the collateral, not the
severity. Module versions have no finer grain than per-module, so bumping `ab`
to `@3` would re-identify all 211 active `ab(...)` roster lines and orphan
814,817 of 815,597 stored games (99.9%) to correct reproducibility on the 33.6%
that involve a `time=` head, while the node track is provably untouched. The TT
bump's collateral was proportionate because that defect reached every `tt`
agent. This one does not. Recorded as a pending developer decision rather than a
settled one: bumping later remains possible, and this entry is what makes the
tradeoff enumerable if it is revisited.

**Fixed** 2026-09-01 by making the expiry sticky (once any node sees the
deadline pass, every later node in that search returns immediately without
re-reading the clock), tightening the sampling mask from 4096 to 256 nodes, and
adding a pre-iteration predictive check that declines to start a depth whose
projected cost exceeds the remaining time. The projection uses the search's OWN
last two iteration times, never an assumed per-ply growth factor, because
branching depends on the evaluator's move-ordering quality and does not transfer
between cores. Node budgets are deliberately excluded from the predictive check:
a node budget is exact and needs no forecast.

**Affected:** `ranking/CHAMPION.md` (openless x time, opener8 x time, dil20 x
time rows), `plans/budget-parity-plan-1-steady-meridian.md` (source), `todo.md`
(citation sites).

## `POSGEN POOL NOT REPRODUCIBLE` - flagged 2026-09-02

**Scope: every position pool built by `rank.exe posgen` from a store containing
`time=` agents, which is every pool built to date.** Like `TIME BUDGET NOT
ENFORCED`, of which this is a downstream consequence, it is a code defect rather
than a writing defect. The affected claim is the word "deterministic" in
`ML.md`'s pipeline description, and any statement that a pool can be regenerated
from its seed.

`posgen` replays a seeded sample of stored games and compares each replay's
outcome to the stored one, dropping the game when they differ
(`mismatchSkipped`, `src/ranking.cpp`). The guard is correct and makes the tool
SAFE: a game that no longer reproduces never contributes positions. It does not
make the tool DETERMINISTIC, because a `time=` agent's replay depends on machine
load, so *which* games get dropped changes between runs. The pool is a function
of the sample minus the drops, so the pool changes too.

**Measured, `ranking/matches.jsonl` at 623,774 rows of which 274,260 (44%)
involve a `time=` agent.** Same seed (123), same targets, same board, twelve
identical invocations:

| condition | determinism mismatches | pool vs the idle baseline |
|---|---|---|
| idle, run 1 | 2 | (baseline) |
| idle, run 2 | 2 | identical |
| under 14-way CPU load, run 1 | 6 | DIFFERS |
| under 14-way CPU load, run 2 | 1 | DIFFERS |
| under 14-way CPU load, run 3 | 2 | identical |

The two idle runs also disagreed on the near-win skip counter (44 versus 43)
without the difference reaching the kept positions, so the drift is present even
unloaded and the idle match is luck rather than a guarantee.

**What this does and does not invalidate.** It does NOT make existing labels
wrong. Every position in a pool is a real position from a game that replayed
correctly, and the `label` and `labelfit` stages downstream are unaffected: they
consume whatever pool they are given. What it invalidates is REGENERABILITY. A
pool cannot be rebuilt from its seed, so any result that says "rerun posgen with
seed N to reproduce this" is not actionable, and two pools built from the same
seed on different machines or under different load are not the same pool and
must not be treated as one.

**What to do instead.** Treat a pool file as the artifact of record and archive
it, rather than treating the seed as sufficient provenance. When a pool must be
reproducible, build it from a store filtered to agents with no live wall-clock
budget.

**Self-limiting.** Once the `time=150ms` roster lines are retired in favour of
compute-normalized agents that carry no live clock, every newly stored row
becomes replayable and pools built from the post-migration store are
reproducible. The defect is bounded by the rows already stored.

**Test fixed 2026-09-03, the underlying `posgen` defect above is unchanged and
still open.** The test `rank posgen - deduped, stratified, deterministic pools`
(`tests/test_ranking.cpp`) asserted byte-identical reproduction across two runs
over the live store, which is asserting a property the tool does not have, so
it failed intermittently under CPU load (reproduced twice in two tries against
a 12-way concurrent `rank.exe play` load job on the pre-fix test; the fixed
test then ran 10/10 clean under the same load). Split into two tests: `rank
posgen - deduped and stratified pools` keeps the original per-row invariant
checks (dedup, ply/hash-tier stratification, undecided-position check) against
the live store, and `rank posgen - byte-identical pools on a time=-free
fixture store` moves the determinism claim onto a fixture store built via
`rankLoadMatches` + `rankFormatMatchRow`, filtered to rows where neither agent
id contains `time=` -- exactly the subset this entry already identified as
replayable, so posgen is genuinely deterministic on it. The fixture is a fresh
store path with no `.index.txt` and no sibling numbered shards, so
`rankStoreParts` loads it as the single file it is. `posgen` itself was not
touched: it is still not reproducible against a store containing `time=`
agents, which remains a deliberate, pending developer decision (see `todo.md`).

---
