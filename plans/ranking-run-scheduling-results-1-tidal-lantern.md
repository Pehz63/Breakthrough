# Results: the rung ladder is the default for match scheduling

Companion to `plans/ranking-run-scheduling-plan-1-tidal-lantern.md`, which
proposed it. Implemented 2026-09-05. The plan's items 1 and 3 of the procedure
list, and code item 1, are done. Code items 2 (`--stop-when`) and 3
(`--opp-roster`) are deliberately not done, for reasons below.

## What changed

| file | change |
|---|---|
| `src/ranking.h`, `src/ranking.cpp` | `rankPlay` gained a `ladder` parameter, default true. It now loops rungs 2, 4, 8, ... `gamesPerPair`, re-scheduling against a store that carries this run's earlier rungs, and prints per-rung progress and projection |
| `tools/rank_main.cpp` | `--no-ladder` on `play` and `run`, plus the usage text |
| `tools/run_rank.ps1` | `-NoLadder`, `-PinEachRung <ratings.tsv>`. The sharded path drives the rungs itself, relaunching workers and merging between each. The merge now drops structurally torn lines with a warning |
| `tests/test_ranking.cpp` | new case, "a 2,4,8 ladder plays exactly the games of one pass at 8" |
| `CLAUDE.md` | standing instruction: ladder by default, a mid-ladder fit is a rung not a result, the stopping rule is written before the run |
| `Docs/ranking-workflow.md` | "Scheduling a study: the ladder is the default" replaces the earlier proposal paragraph |
| `tools/CLAUDE.md` | `run_rank.ps1` row updated |

## The invariant, and how it was checked

The whole design rests on one claim: **`--games N` is a target, not an
increment.** `rankSchedule` counts the games already in the store for each pair
and returns only the deficit, so rungs 2, 4, 8 play exactly what one pass at 8
would have played.

That was verified three ways, and the second and third are the ones that matter,
because a count can match while the games differ.

1. **Unit test.** `rankSchedule` walked as a ladder against a growing store
   versus called once at 8, on a 4-agent roster with two diluted members so some
   pairs are genuinely stochastic. Asserts equal size, equal sorted multiset of
   `(white, black, seed)`, all three rungs non-empty, rung 1 equal to
   `pairs x 2`, rung 2 smaller than rung 1, and nothing left to schedule after.
2. **End to end, serial.** `rank.exe play --games 8` versus `--games 8
   --no-ladder` on a scratch store, 4 agents, 42 games each. Identical multiset
   of `(w, b, seed)` and identical multiset of `(w, b, result, plies)`. The
   games are not merely equivalent, they are the same games.
3. **End to end, sharded.** `run_rank.ps1 -Workers 3 --games 8`, rungs of 12,
   10 and 20, totalling the same 42. Identical `(w, b, seed)` multiset to the
   single pass, and a maximum multiplicity of 1, so no shard replayed a
   sibling's game.

Check 3 is the one that could have failed. It is why the ladder is disabled
inside a sharded `rank.exe play`.

## Why sharding disables the in-process ladder

A shard writes to its own `<store>.<n>` and reads only the shared input store.
If shard 0 laddered in process, its rung-2 schedule would not contain shard 1's
rung-1 games, so the deficit it computes would include them, and the `p % k ==
shard` filter over a now-different pending list would hand it games shard 1 had
already played. The wrapper solves this by merging between rungs, so every worker
starts each rung seeing the whole store. `rank.exe play` prints
`ladder off: sharded run, the rungs are the wrapper's job` rather than silently
doing nothing.

## Rung 1 is the expensive rung

The plan quoted a rule of thumb from the developer, multiply rung-1 time by 8 to
project a `--games 16` run. That is wrong in a knowable direction and the code
now prints the exact number instead. Rung 1 touches every pair, including the
deterministic ones that `pairGameTarget` caps at 2 and which never appear in a
later rung. Measured on the current 228-agent active roster, 107 deterministic
and 121 stochastic:

| shape | rung-1 games | `--games 16` total | true multiplier |
|---|---|---|---|
| full round robin | 51,756 | 334,654 | 6.5x |
| one cohort cell vs the roster | 456 | 2,150 | 4.7x |

Per-rung increments for the full round robin: 51,756, then 40,414, then 80,828,
then 161,656.

## Sample output

```
rank: 4 active agents, 42 pending games (target 8/pair)
ladder: 2/pair -> 12 cumulative, 4/pair -> 22 cumulative, 8/pair -> 42 cumulative
  rung 1 is a full pass over every pair, so it is the most expensive single rung
  and its rate projects the rest.
== rung 1/3, --games 2: 12 game(s)
== rung 1/3 done: 12 game(s) in 0.0s (301.54 games/s), 12 cumulative
   30 game(s) left in rungs 4 8, about 0.0 min at 301.24 games/s so far
```

## How to test

```
.\tools\run_tests.ps1 -Build          # 4780 assertions, 211 cases, all passing
rank.exe play --roster <small roster> --in <scratch> --out <scratch> --games 8
rank.exe play --roster <small roster> --in <scratch2> --out <scratch2> --games 8 --no-ladder
```
The two stores should hold the same rows. Expect the laddered run to print three
rung banners and two projection lines, and the flat run to print neither.

For the sharded path, `tools/run_rank.ps1 -Workers 3 -NoRate -Store <scratch>
play --roster <small roster> --games 8` should print three rung sections and
merge 12, 10 and 20 rows.

## What was deliberately not built

- **`--stop-when` (plan code item 2).** The developer chose advisory over
  automatic: every rung prints, the ladder always runs to the requested
  `--games`. Automatic early stopping is optional stopping, and building it into
  the tool would make the unsound thing the convenient one. The stopping decision
  stays human and stays governed by an SE target written before the run.
- **`--opp-roster` / Elo-window opponent selection (plan code item 3).** Still
  unbuilt. It is orthogonal to the ladder (the window decides which pairs exist,
  the ladder decides how many games each gets) and it needs the anchor
  connectivity question settled first. Still recorded as a gap.

## Correctness gotchas found

- **The torn-line merge.** The wrapper's merge was `Get-Content $sf |
  Add-Content`, which is what put 3 torn rows into `ranking/matches.jsonl` on
  2026-09-04. A ladder merges once per rung rather than once per run, which would
  have multiplied the exposure, so the merge now checks each line begins with `{`
  and ends with `}`, drops the rest, and warns with a count.
- **`rate` must not receive play-only flags.** `-PinEachRung` reuses the caller's
  pass-through options, which include `--games` and `--cohort`. It filters down to
  `--roster` and `--board`.
- **The wrapper has to strip a caller's `--games`** before appending the rung's
  own, or the last one on the command line wins and every rung plays the target.

## Future Work

- **Validate the stopping rule against a completed study, offline.** Unchanged
  from the plan and now cheaper to do, since the `retain` store is finished.
  Subset its rows to 2, 4, 8 and 16 games per pair and find the rung at which
  each of the five slope contrasts first reached its final sign and magnitude. If
  a contrast that later moved would have satisfied the rule at an early rung, the
  rule's SE multiplier is too loose. This tests the one part of the design that
  the equivalence proof says nothing about.
- **Measure whether a rung's rate actually predicts the next rung's.** The
  projection assumes games/sec is stable across rungs, and it should not be
  exactly: rung 1 is dominated by deterministic pairs and later rungs skew toward
  stochastic ones, whose games can differ in length. The projection line is
  printed with that caveat in the code comment but the size of the error is
  unmeasured. One instrumented full-roster run would settle it.
- **The per-rung fixed cost is unmeasured on a real roster.** Each rung relaunches
  every worker, and a `rank.exe` worker loads model slots at about 650 MB
  resident. On the scratch roster this was invisible. On a 228-agent roster with
  12 workers and 4 rungs it is 48 process starts, and if that turns out to be
  minutes it argues for fewer, wider rungs.

## Ideas This Inspired

- **The same ladder belongs on `posgen`/`label`.** Position-oracle labeling
  currently spends a fixed playout budget per position. The `p(1-p)` argument that
  showed 26-48% of a cohort cell's games were near-worthless applies directly:
  label every position cheaply, then spend the later rungs only on positions whose
  label sits near 0.5. That is a bigger win than the ranking ladder, because there
  the extra games are wasted on pairs that are settled, whereas here they would be
  redirected rather than skipped.
- **Successive halving for `hill_climb.ps1` and Pass 3 of the training
  playbook.** Uniform doubling spends the same on a candidate that is already
  clearly last. Racing (drop the worst half at each rung, double the survivors)
  is the right shape when the question is "which of these" rather than "how big
  is this effect".
- **`rate --include-shards`.** Reading `<store>.<n>` read-only would make a
  mid-run fit a one-liner. Wanted less now that the wrapper offers
  `-PinEachRung`, but still the right answer for looking at a run someone else
  launched.
- **A rung count in the match row.** Rows do not record which rung produced them,
  so the offline replay in Future Work has to reconstruct rungs from per-pair
  ordering. One small integer per row would make "refit this study as it stood at
  rung 2" exact and trivial.
