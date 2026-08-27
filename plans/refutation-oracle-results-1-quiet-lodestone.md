# Refutation oracle, Phase 0 results: do deterministic agents actually replay?

Companion to `plans/refutation-oracle-plan-1-quiet-lodestone.md`. Covers Phase 0
only. Phases 1-4 (the `rank.exe refute` miner and the book itself) are not built.

## Verdict

**The gate passes.** All 119 active deterministic agents replay exactly against
a fixed deterministic probe, both colours, within a process and across separate
processes, with two named exceptions. Theory 14's premise 1 ("the deterministic
target is not reproducibly deterministic across runs") no longer holds on the
current code, so the repaired refutation-book variant is worth building.

| Measurement | Result |
|---|---|
| Subjects | 119 active deterministic agents (218 active roster, 99 stochastic) |
| Games per pass | 714 (119 x 2 colours x 3 replicas) |
| Full passes run | 4 (2 per build, 2 builds) |
| Within-process reproducible, final build | 238/238 subject-colours, both passes |
| Node-budgeted subset | 210/210 in every one of the 4 passes |
| Cross-process exact trace agreement | 238/238 rows, identical `traceset` hashes |
| Positive control (dilution agents) | 0/56 reproducible, 3 distinct traces from 3 replicas |

## What was measured, and how

`rank.exe determinism` (new). It replays each active deterministic roster agent
against a fixed deterministic probe (`ab(deep=2)@1.classic(chip=100)@2`) as both
White and Black, `--replicas` times, and compares the games exactly.

Three design points carry the result:

1. **Exact trajectories, not a proxy.** `playOneGame` gained an optional
   per-ply position-hash trace. "Same game" means the same move sequence.
2. **Interleaved replicas.** The loop is replica-major and agent-minor, so
   replica 2 of an agent is preceded by an entirely different sequence of games
   than replica 1. An agent-major loop would play each agent's replicas back to
   back and would not disturb the process state it is trying to detect.
3. **A different seed per replica.** A deterministic agent draws from `rand()`
   nowhere, so varying the seed cannot change its game. If it does, the agent is
   not what `rankAgentIsDeterministic` claims, which is itself worth catching.

Cross-process reproducibility is tested by running the command twice and
diffing the TSVs on the `traceset` column, a hash over the sorted distinct
traces.

## The two exceptions

`model=111` and `model=113`, both `ab(deep=6,tt,ord,time=150ms)@1` wearing
wide-MLP `position_elo` heads, are the only agents that ever varied. They are
exactly the two lines already carrying `# cost flag` in `ranking/roster.txt`
(lines 366 and 369) and already documented in `ranking/CHAMPION.md` as running
2-3.3x over their 150ms budget.

Across the four full passes, `model=113` as Black failed once, producing 2
distinct traces from 3 replicas with the first divergence at half-move 3. The
other three passes had it clean. **This is intermittent, not clean**, and the
honest reading is that the two cost-flagged wall-clock agents are not safely
mineable, not that they passed.

Every other `time=150ms` agent (12 of the 14) was exactly reproducible in all
four passes. The wall-clock budget is not inherently disqualifying here: the
search discards a budget-cut iteration rather than keeping a partial result, so
the move comes from the last completed iteration and stays stable as long as the
same number of iterations complete. That only breaks down when a single move is
expensive enough for the deadline to land in a variable place, which is what the
two cost-flagged cores do.

## A finding for distinct-game accounting

For `model=113`, the two cross-process passes produced **identical move
sequences with different node totals** (1857802 vs 1853028 as White, 2558889 vs
2564574 as Black). The `traceset` hashes matched, so the games were the same.

This matters to `Docs/benchmarking.md` defect 3, which counts distinct
trajectories by the tuple `(colour, plies, result, both node totals)`. For a
wall-clock-budgeted agent that tuple **overcounts**: it reports two identical
games as two distinct ones. The tuple is sound for node-budgeted agents, where
the node total is a function of the trajectory. Any distinct-game count over a
population containing `time=` agents is therefore an upper bound, and the
`x time` categories are the affected ones.

## Retrospective check on the existing store

Run before spending compute, and it produced one number worth keeping plus one
blocker.

Across `ranking/matches.jsonl`, `matches.roster.0001.jsonl`,
`matches.retired_other.0001.jsonl` and four post-fix screening stores (625,501
rows scanned), restricted to ordered pairs where BOTH agents are alpha-beta or
greedy, node-budgeted, undiluted, and wearing no random opener:

- **Pre-2026-08-03: 2,545 of 22,322 ordered pairs (11.4%) replayed identically**,
  at 0.796 distinct fingerprints per stored row. This is theory 14's premise-1
  failure measured at scale, and it is consistent with the 0.706 figure recorded
  when `ttClear()` was added to `playOneGame`.
- **Post-2026-08-03: no strictly deterministic ordered pair has been played more
  than once anywhere in the store.** The retrospective check cannot answer the
  post-fix question at all, which is why fresh games were required.

One methodological trap on the way: an early version of the scan classified
`gaz(...)` Gumbel MCTS heads as deterministic and reported 0/12768 reproducible
on the screening stores. Gumbel MCTS draws a Gumbel variate per legal move on
every root search, so those agents are stochastic by construction. The project's
own `rankAgentIsDeterministic` already excludes them. Use it rather than
re-deriving the rule from ID strings.

## Defect found and fixed: pairgen did not clear the transposition table

`rankPairGen`'s game loop called `playOneGameCapture` with no `ttClear()`,
unlike `playOneGame` (fixed 2026-08-03) and the `extract` replay path (which
carries a comment explaining exactly why it clears). So `pairgen --games N` on a
deterministic pair produced N *different* games rather than N copies of one, and
each game's play depended on every game the process had run before it.

This is the tool used for training-data generation, for theory 38's
book-collapse measurements, and for the `--branch-tries` line miner. Fixed by
adding the per-game clear.

**Not re-measured.** No prior `pairgen` result is re-run here, so the numbers in
`plans/book-opener-audit-results-1-vivid-lantern.md` and the cluster-book
results docs stand as published and are not corrected by this session. What
changes is that a *future* `pairgen` run on a deterministic pair now behaves as
its documentation says.

## A bug in this session's own instrument, and how it surfaced

The first version of the probe pinned `srand(12345u)` before every game,
reasoning that deterministic agents draw no randomness so the seed is inert.
The positive control then reported **56/56 dilution agents reproducible**, which
is impossible: dilution draws from `rand()` on every move. Pinning the seed had
made even a stochastic agent replay, so the probe was measuring nothing and
would have reported a clean sweep regardless of the truth.

With the seed varied per replica, the same control reads 0/56 reproducible with
first divergence at half-move 8-9. The main sweep's numbers above all come from
the fixed build.

This is the concrete argument for `--include-stochastic` existing at all: a
probe that only ever prints "reproducible" is indistinguishable from a probe
that cannot detect anything. The flag exists to be run before the sweep is
believed.

## Changes made

| File | Change |
|---|---|
| `src/ranking.cpp` | `playOneGame` gained an optional per-ply position-hash trace, `rankPairGen` now clears the TT per game, new `rankDeterminism` |
| `src/ranking.h` | Declared `rankDeterminism` |
| `tools/rank_main.cpp` | `determinism` subcommand + usage |
| `ranking/det_run1.tsv`, `det_run2.tsv` | The two full passes, for diffing |
| `ranking/det_control.tsv` | The positive control |

## How to test

```powershell
.\rank.exe determinism --only "greedy@1" --replicas 3 --out ranking/det_smoke.tsv
```
Expect 4/4 reproducible in a few seconds.

```powershell
.\rank.exe determinism --only "dil(prob=20)" --include-stochastic --replicas 3 --out ranking/det_control.tsv
```
Expect **0/56** reproducible. If this prints anything else, the probe is broken
and no sweep from it should be believed.

```powershell
.\rank.exe determinism --replicas 3 --out ranking/det_a.tsv
.\rank.exe determinism --replicas 3 --out ranking/det_b.tsv
```
Expect 238/238 in each, and a clean diff on the `traceset` column. Exit code is
2 when any subject-colour did not repeat, so it is usable as a gate in a script.

Run the two passes with the machine otherwise idle. The `time=150ms` agents'
play depends on wall-clock, so a concurrent build or sweep changes what is being
measured for those 28 subject-colours.

## Future Work

- **The 32-games/pair certification standard is mostly buying replays.** With
  119 of 218 active agents deterministic, a deterministic pair contributes one
  game per colour no matter how many are scheduled. `Docs/benchmarking.md`
  defect 3 records this, and Phase 0 now confirms the underlying premise
  directly rather than inferring it from fingerprint counts. The open question
  is what the scheduler should do about it: skipping known-replay pairs would
  free most of the compute a boost round spends, but it changes what a
  "games/pair" number means in `CHAMPION.md` and would need the error bars
  recomputed on distinct games. Not attempted here.
- **`model=113`'s intermittent failure is unexplained.** It failed once in four
  passes, at half-move 3, which is early enough that the search should be far
  from any budget edge. Worth a targeted run at higher `--replicas` before
  concluding it is only about the time budget. This bears directly on whether
  the two cost-flagged agents can be included in the refutation-book target set.
- **The probe uses one fixed opponent.** Reproducibility against
  `ab(deep=2)@1.classic(chip=100)@2` does not strictly establish reproducibility
  against the deep book lines the miner will actually walk, which reach very
  different positions. A second pass with a strong probe would test whether
  anything depends on position character. This qualifies the Phase 0 pass as
  "reproducible in the positions this probe visits", which is the assumption
  Phase 2 will lean on at every node.
- **Cross-machine reproducibility is untested.** Everything here is one machine
  and one binary. A book mined here is only guaranteed to refute the same
  binary. This matters if the ranking is ever meant to run elsewhere.

## Ideas This Inspired

- **A determinism column in the roster check.** `rank.exe check` already
  validates the roster, and it could print how many pairs are known-replay, which
  would make the cost of a boost round visible before it is spent.
- **Use the trace as a cheap game-identity key in the match store.** Storing an
  8-byte trace hash per row would make distinct-game counting exact instead of
  inferred from `(plies, result, nodes)`, and would have made this whole
  retrospective check a one-line query. It would also fix the `time=`
  overcounting described above.
- **A "reproducibility" axis for the ladder.** The split between agents that
  replay and agents that do not is currently implicit in the ID string. It is
  arguably a more useful grouping for study design than the openless/opener8/
  dil20 division, since it determines whether games are samples or replays.
- **Invert the refutation book into a training signal.** If a one-player search
  finds a winning line against an agent, the positions along that line are
  precisely where that agent's evaluator is wrong. That is a targeted training
  set, and a more direct use of the mining compute than the book itself.
