# Ranking-run scheduling: sequential doubling instead of one fixed `--games`

Written 2026-09-05, from what the `retain` cohort run (`ranking/q6/roster_retain.txt`,
40 cells, about 52,000 games) showed about how this project currently schedules a
study. The run itself is not the subject. The way it was launched is.

## The problem this fixes

Every cohort study here is launched the same way: pick `--games N` once, launch,
wait for all of it, then fit. That has three costs, all of which were paid on
2026-09-05.

1. **The answer is unavailable until the last cell lands.** The `retain` study's
   headline result was legible at roughly half the games, but nothing in the
   workflow surfaces that, so the default is to wait.
2. **`N` is guessed before any data exists.** Too small and the fit inverts later
   (`ranking/CHAMPION.md` rule 2 records 8-games/pair orderings inverting at 32
   games/pair twice). Too large and compute goes into pairings that were settled
   hundreds of games ago.
3. **Cells finish at wildly different game counts, and a partial cell is a
   hazard, not just a weak row.** In the run above, four cells sat at 106-1,183
   games while the other 36 sat at about 1,340. A 106-game row carries a `pm`
   near 39, and `PINNED AT LOW GAME COUNT` (`Docs/corrections.md`) is exactly the
   failure of quoting such a row as if it were settled.

## The proposal

Run the same study as a ladder of rungs, fitting between each:

```
rank.exe play --roster <r> --cohort <c> --games 2     # then rate
rank.exe play --roster <r> --cohort <c> --games 4     # then rate
rank.exe play --roster <r> --cohort <c> --games 8     # then rate
rank.exe play --roster <r> --cohort <c> --games 16    # then rate
```

### Why this is nearly free

`--games N` is a **target**, not an increment. The scheduler counts games already
in the store for each pair and plays only the deficit, so the `--games 4` rung
plays the two games the `--games 2` rung did not, and the ladder 2, 4, 8, 16 plays
exactly the same set of games as a single `--games 16` launch. The only added cost
is process startup per rung plus one `rate` per rung, and `rate` is about 20
seconds on the current store. There is no compute argument against doing this.

The corollary matters just as much: **a study never has to commit to its final `N`
up front.** Launch at 2, look, and let the data decide whether 16 is needed.

### What the ladder does not change

`pairGameTarget` (`src/ranking.cpp`) returns 2 whenever BOTH agents are
deterministic, and `--games N` otherwise. Most of the standing roster is
deterministic, so those pairs are saturated at rung 1 and every later rung buys
nothing there. Doubling only moves the stochastic pairs, which are the ones
carrying most of the information anyway (`Docs/ranking-workflow.md`, cost model).
Expect the marginal cost of each rung to be well under a literal doubling, and do
not describe a rung as "twice the games".

### The stopping rule

**State it on a within-fit contrast, never on absolute Elo.** Absolute Elo is not
comparable across fits: the Bradley-Terry prior compresses the scale as the pool
grows (`Docs/benchmarking.md`), so "1,220 last rung, 1,233 this rung" is not a
measurement of movement. What is stable across fits is a difference measured
inside one fit between two rows of that fit.

For a study of the shape "does treatment X help", the contrast is X minus control
at the same core and the same budget. For a study of the shape "which cell is
best", it is the gap between the top cell and each challenger.

Stop the ladder when, for every contrast the study is meant to answer:

- the contrast moved by less than its own SE between the last two rungs, **and**
- the contrast's SE is small enough to answer the question that was asked, which
  has to be written down before launching (for example "resolve a 30-Elo effect"
  implies SE <= 15), **and**
- no contested pair is below 32 games, per `CHAMPION.md` rule 2, if the study will
  be used to move a category title.

Two of the three are not enough. A contrast can sit still for one rung purely by
chance, and a tight SE on a pair that never played is not a measurement.

### Worked example, from the run this document came out of

Fitting the `retain` study's in-flight store at roughly half its games already
separated the cores cleanly on the quantity the study exists to measure, the slope
of Elo on log2(node budget), in Elo per doubling, over the three rungs that were
fully populated at that point:

| core | plain | retain | difference |
|---|---|---|---|
| chip counter | +5.7 +-9.7 | +4.4 +-10.1 | -1.3 +-14.0 (z=-0.10) |
| tdleaf_self lin `model=169` | +120.9 +-10.8 | +30.0 +-11.8 | -90.9 +-16.0 (z=-5.67) |
| tdleaf_self lin `model=349` | +103.4 +-10.5 | +12.2 +-11.5 | -91.2 +-15.5 (z=-5.87) |
| position_elo mlp `model=113` | +53.9 +-9.7 | +44.1 +-10.1 | -9.8 +-14.0 (z=-0.70) |
| pool_games lin `model=97` | +10.3 +-9.7 | +9.2 +-9.7 | -1.1 +-13.7 (z=-0.08) |

These are preliminary numbers from a mid-run snapshot of the store, not the
study's result, and they are reproduced here only to show what a mid-ladder fit
looks like. The point is that two of the five contrasts are past z=5 and three are
under z=1, and no further rung was going to change which group a core falls in. A
ladder would have surfaced that at rung 2 or 3 and let the developer decide whether
the remaining rungs were worth running.

## Procedure changes (no code needed)

These are usable today.

1. **Write the contrast and its target SE into the plan before launching.** A
   study whose stopping rule is not written down cannot be stopped early without
   it looking like a decision made to save time.
2. **Ladder 2, 4, 8, 16 by default**, fitting between rungs, rather than one
   `--games 8` or `--games 16` launch.
3. **Fit between rungs against a pin file, not an unpinned refit.** Workflow A
   (`Docs/ranking-workflow.md`) is the screening tool and cannot disturb the
   standing scale. An unpinned refit per rung would change the scale under the
   ladder and break the cross-rung comparison the stopping rule depends on.
4. **Check the pin file's own game counts before every rung's fit.** Two 160-game
   rows in `ranking/standings.tsv` produced a 231-Elo phantom in this project on
   2026-09-04 because `--pin` froze them (`PINNED AT LOW GAME COUNT`). This is
   already step 7 of Workflow A. On a ladder it fires once per rung rather than
   once per study, so it matters more.
5. **Report a rung's numbers as a rung, with its game counts inline.** Any table
   from a mid-ladder fit carries the rung number and per-cell games in the table
   itself, so it cannot later be quoted as the final grid.
6. **Never let a cell trail the others by more than one rung.** If a cell is
   behind because its games are slow, either let the ladder wait for it or bench
   it, but do not fit a grid where one row is at rung 1 and the rest are at rung 4.

## Implementation items (code)

Ordered by value per unit of work.

### 1. `rank.exe play --ladder 2,4,8,16`

Run the rungs in one invocation, fitting and printing a designated contrast set
between them, so the ladder is one command rather than a shell loop someone has to
babysit. Should print, per rung: games played this rung, cumulative games per cell,
and each contrast with its SE and its movement since the previous rung.

### 2. `--stop-when` on that ladder

A machine-checkable form of the stopping rule: take the contrast list and an SE
target, and stop the ladder when every contrast satisfies the three conditions
above. Must print which condition ended it, and must refuse to stop while any
contested pair is under 32 games.

### 3. `--opp-roster` or an Elo-window opponent selection

Already recorded as the "Known gap" in `Docs/ranking-workflow.md`. A cohort agent
currently plays every active roster agent, and measurement on this store put 26%
to 48% of a cell's games against opponents it beats or loses to more than nine
times in ten, which contribute almost nothing to a Bradley-Terry fit. Restricting
the pool to a window around the cohort's expected rating would cut a screening run
by roughly a third at no cost to the error bars that matter.

The tradeoff to settle first is anchor connectivity: the fit needs the cohort
connected to `rand@1`, so the window has to retain some spread rather than only
near-equal opponents. The natural design is a window plus a fixed small quota of
anchor games, and the check is that a windowed fit reproduces a full fit's ordering
on a study already run both ways.

This composes with the ladder rather than competing with it. The ladder decides how
many games a pair gets, the window decides which pairs exist at all.

### 4. Incremental fit between rungs

`rate` at about 20 seconds is not currently a bottleneck, and it will not become
one until the ladder is automatic and the store is several times its current size.
Listed so it is not mistaken for an oversight.

## What this does not solve

- **Deterministic pairs still store copies.** The ladder cannot add information to
  a pair that replays one trajectory per colour. That is defect 3
  (`Docs/benchmarking.md`), and the fix is diversified openings or dilution, not
  scheduling.
- **A rung is not a free look at a moving target.** If cells are still being added
  to the cohort between rungs, the pool is changing and cross-rung contrast
  movement mixes "more games" with "different pool". Freeze the cohort before
  starting a ladder.
- **The stopping rule can be gamed by choosing a lax SE target.** Writing the
  target before launching is what prevents that, which is why item 1 of the
  procedure is first.

## Future Work

- **Validate the ladder against a completed study.** Replay the `retain` store at
  2, 4, 8 and 16 games per pair by subsetting the stored rows, and check at which
  rung each of the five slope contrasts first reached its final sign and magnitude.
  This is the direct test of whether the stopping rule would have stopped in the
  right place, and it costs no new games. If the rule would have stopped a rung
  early on any contrast that later moved, the rule is wrong and the SE multiplier
  needs raising.
- **Measure the real marginal cost of each rung.** The `pairGameTarget` saturation
  makes rung cost sublinear, but by how much is unmeasured on this roster. Counting
  deterministic-vs-deterministic pairs in `ranking/roster.txt` gives the number
  directly and would let a plan state a ladder's cost honestly.
- **Test whether a windowed pool changes any existing conclusion.** Item 3 is only
  safe if a windowed fit reproduces a full fit's ordering. The `retain` store is
  now large enough to run that comparison offline, with no new games.

## Ideas This Inspired

- **Per-pair adaptive games rather than per-study.** The ladder doubles everything
  uniformly. The information argument says the games should go where `p(1-p)` is
  largest, which is a per-pair schedule: more games to pairs near 50%, early stop
  on pairs past 90%. That is a bandit allocation, and it would be strictly better
  than uniform doubling if the bookkeeping is manageable.
- **A contrast as a first-class object.** Several rules here ("state contrasts
  before launching", "track contrast movement across rungs", "never compare
  absolute Elo across fits") all want the same thing: a named, stored contrast the
  ranking tool knows about. A `ranking/contrasts/<study>.txt` listing pairs of
  agent IDs, fitted and printed with SEs by `rate`, would make the correct
  comparison the easy one.
- **Mid-run fits without a snapshot store.** The preview fit that produced the
  table above needed a hand-built index and a validated copy of the in-flight
  shards. A `rate --include-shards` flag that reads `<store>.<n>` read-only would
  make a mid-run look a one-liner, and would remove the temptation to blind-append
  a shard file a live process is still writing.
- **Bench-on-lag as a roster state.** "Never let a cell trail by more than one
  rung" is a rule a person has to enforce. A roster flag meaning "include only at
  rung parity" would enforce it in the fit instead.
