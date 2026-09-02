# Budget-parity rebuild, Part 1 prerequisites: results

Companion to `plans/budget-parity-plan-1-steady-meridian.md`. Covers Part 1
only: the eight cross-cutting prerequisites (P1 through P8). **No Part 2 regime
work was started, nothing was trained, and no ranking study was run.**

Executed 2026-09-01. All four link targets (`breakthrough.exe`, `tests.exe`,
`rank.exe`, `train.exe`) rebuilt and smoke-tested. The suite passes at **4471
assertions in 201 test cases**, up from a pre-change baseline of 4301 in 184
measured the same way at the start of the session.

---

## What changed, by prerequisite

Executed in the dependency order the developer set, which is not the plan's
numbering: P5, then P2, then P4 and P6 together, then P8, then P3 and P7, then
P1 last.

### P5. `games=` provenance written from actual spend

`src/ml_tdleaf.cpp` and `src/ml_gumbelzero.cpp` built `model->teacher` once
BEFORE the training loop from `cfg.games`. `tools/tdleaf_study.ps1` passes the
ladder's LAST rung as `--games`, so every rung of a laddered run carried the
same claim.

The provenance string is now split. `provRecipe` holds only the recipe (lambda,
lr, l2, depth, node budget, batch, opener plies, explore, seed) and carries no
count at all. A small `Saver` struct holds the recipe, the init tag, and a
reference to the live `TrainBudget`, and every checkpoint write goes through it,
so each rung stamps its own `,games=<n>,secs=<f>,nodes=<n>` at the moment it is
written. This had to land before P4 or the new wall-clock rungs would have
inherited the identical bug and every rung would have claimed the full budget.

The `tests/test_train_budget.cpp` case that pins this runs a real two-rung
`trainTDLeaf` and asserts rung 2 says `games=2,`, rung 6 says `games=6,`, and
the two provenance strings DIFFER. The last assertion is the one that would have
caught the original defect: both rungs were individually parseable before, they
were just identical.

### P2. `time=` budget enforcement

The largest behavioral change in this session, and the diagnosis in the plan was
incomplete in a way that matters.

The plan describes two problems: coarse 4096-node sampling, and no pre-iteration
check. There is a third, and it is the load-bearing one. `budgetTripped()` kept
**no memory** of having seen the deadline pass. A node that sampled the clock
and found it expired returned true, but the other 4095 nodes in its window never
asked and recursed to full depth regardless. The budget was not merely coarse,
it was very nearly inoperative: only 1 node in 4096 could ever end the search.

Three changes in `src/ai_minimax.cpp`:

1. **Sticky expiry.** `s_timeExpired` latches on the first observed deadline
   pass, and every later node in that search returns immediately without re-reading
   the clock. `seedTimeBudget()` clears it and also records `s_timeStart`.
2. **Sampling mask 4096 -> 256**, sized from measurement rather than guess (see
   "Measurements" below).
3. **`nextIterationFits(prevIterMs, lastIterMs)`**, a predictive pre-iteration
   check in both the `miniMaxWhite` and `miniMaxBlack` iterative-deepening
   loops. It projects the next depth's cost from the ratio of the search's own
   last two iteration times and declines to start a depth that cannot fit. It
   ABSTAINS (returns true) until two iterations have been timed, so it never
   guesses on the first pass.

The growth factor is derived per search from that search's own history. It is
never the plan's x3.41: that figure is Classic's own branching behaviour, and
move-ordering quality differs per evaluator, so it does not transfer. Node
budgets are deliberately excluded from the predictive check, because a node
budget is exact and needs no forecast. `BUDGET_SIMS` was appended (not inserted)
to the `BudgetKind` enum so no persisted budget-kind number is renumbered.

### P4 / P6. Wall-clock rungs, hard stop, resume, and compute instrumentation

New shared module `src/train_budget.h` / `src/train_budget.cpp`, so this is one
implementation rather than one per trainer. `TrainBudget` carries the ascending
cumulative second marks, the hard stop, the prior spend carried in by a resume,
and the live counters. Helpers: `tbParseMarks`, `tbBegin`, `tbElapsed`,
`tbNodes`, `tbUnits`, `tbShouldStop`, `tbTakeDueMark`, `tbMarkPath`, `tbStamp`,
`tbParsePrior`.

Wired into `train.exe tdleaf` and `train.exe gumbelzero` via
`getWallLadder(...)` in `tools/train_main.cpp`, as `--wall-ckpt-at`,
`--wall-stop`, and `--resume`.

The plan proposed skipping resume ("build checkpoint marks and a stop, skip
resume"). **Resume was built**, per the developer's Part 5 answer 4: a regime
still improving at 8h runs to 16h alone, so it must continue one cumulative
ladder rather than restart. Consequences that fell out of that requirement:

- `tbBegin` skips marks the prior spend already passed, so a resumed 8h+8h run
  does not rewrite the 2h and 4h rungs.
- `--resume` HARD ERRORS on a checkpoint with no spend stamp rather than
  continuing from zero, because a run that cannot say what it inherited would
  mislabel every rung it then writes.
- `tbMarkPath` names a rung after its NOMINAL mark (`_t7200.txt`), not its
  actual elapsed time, so one filename finds the same rung across every seed and
  regime. The true elapsed time lives in the file's own provenance.
- `tbTakeDueMark` is a WHILE loop at the call site: one long unit can carry a run
  past several marks at once and every rung the ladder asked for must still be
  written.
- `tbFindNumber` requires a token boundary before the key, so `games=` cannot
  match inside `opengames=`. This is tested explicitly.

P6's node meter is `g_trainNodesTotal` (`src/globals.h`), fed by one add per
move in `agentChooseMove`: `g_lastNodes` is cleared before dispatch and added
after, so a move made without searching contributes 0 and **no search code path
knows the counter exists**. Wall clock is the normalization unit for the study,
but the node figure is what makes a spend claim portable off this machine.

### P8. Fast tanh in the shared learned leaf tail

Implemented per the developer's Part 5 answer 3 (IN, before any Part 2
comparison run). `mlSquashToEval` in `src/ml_eval.cpp` now uses a 1024-entry
linear-interpolated `|tanh|` table over [0, 8] plus a hand-written
round-half-away-from-zero.

**It is bit-exact, not an approximation.** When the scaled magnitude lands
within `TANH_EPS * |out_scale|` of a rounding boundary, the code recomputes with
`std::tanh` and rounds that instead. Exactness is a hard requirement, not a
nicety: a learned agent's canonical ID carries `learned(...)@1`, so a leaf
returning a different int would be a different player and would require a module
bump and a roster re-identification. `mlSquashToEvalFast` and
`mlSquashToEvalReference` are exported so `tests/test_ml.cpp` can assert
equality directly.

One correctness gotcha found by the tests: `lround` returns a 32-bit `long` on
MSVC, so the SHIPPED REFERENCE ITSELF is undefined past `LONG_MAX`, which is
where the first fast-vs-reference mismatch appeared (at `scale = 3e9f`).
Rounding and clamping now happen in `long long`, so the fast path is at least
well defined there, and the test's scale list stops short of `LONG_MAX` with a
comment saying why: beyond it there is no defined behaviour to be exact against.
Every value head this project trains uses `out_scale` 900.

### P3. `gaz` node and leaf counters

`src/ai_gumbel.cpp` never wrote `g_lastNodes`, `g_lastLeafs`, `g_lastEffDepth`,
or `g_lastBudgetKind`. `src/ranking.cpp`'s `playOneGame` credits a move's node
and effective-depth columns only when `g_lastNodes > 1`, so that gate never
fired for a Gumbel move and every `nodes_per_move` cell in every `gaz` export
reads 0.

A `GumbelCounters` struct is threaded through `gumbelSimulate`, counting one
node per call and, at its four TRUE-leaf sites (terminal, no legal moves, newly
expanded, ply-budget stand-pat), one leaf plus its depth. `meanLeafDepth()` is
therefore a mean over leaves, not a tree height. `publishGumbelTelemetry` writes
the four globals at all four `gumbelSearch` return sites, with the root counted
so a search running zero simulations still reports 1 node. `ranking.cpp` needed
no change, exactly as the plan predicted.

### P7. `ranking/climb_roster.txt` rebuilt

Eight of its ten lines were `ab(...)@1` identities, dead since the TT version
bump, so a hill climb silently fell back to two live opponents. The file is now
11 lines built around two stated properties:

1. **Every member draws from `rand()`** (dilution, the `rand` opener, or
   SmartRandom). A deterministic-vs-deterministic pair replays one game per
   colour however many are requested, which turns the climber's fitness into a
   step function. The two deterministic mid-ladder rungs the old pool carried
   (`ab(deep=3)`, `ab(deep=4)`) are gone for this reason.
2. **The ceiling was raised.** The old pool topped out at Elo 832. `gauntlet`'s
   1-D MLE is unidentified when a candidate beats every opponent, and
   `hill_climb.ps1 -Head "deep=6,tt,ord,nodes=200k"` reaches agents rating 1124.
   The new ceiling rung is
   `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1`
   at Elo 1093 and 17.5 ms/move, the strongest cheap stochastic agent available.
   Rungs now span 0 to 1093 with no gap wider than about 200.

Validated with `rank.exe check --roster ranking/climb_roster.txt`: 11 agents,
all canonical, all rated. Elo figures are from the 2026-09-01 full-roster fit
and are ordering guidance only, never comparable against another fit.

### P1. `deep=12` roster IDs

Five lines appended to `ranking/roster.txt`, taking the roster from 218 to 223
active agents. `rank.exe check` passes, and the new lines have zero games.

```
on  ab(deep=12,tt,ord,nodes=200k)@2.classic(chip=100)@2
on  ab(deep=12,tt,ord,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1
on  ab(deep=12,tt,ord,time=150ms)@2.classic(chip=100)@2
on  ab(deep=12,tt,ord,time=150ms)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1
on  ab(deep=6,tt,ord,time=150ms)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1
```

Two cores on both tracks: `classic(chip=100)@2` (the cheapest core in the field,
and the openless x time champion) and `model=169` (the openless x node
champion). The `deep=6` twins stay rostered and active as the continuity anchor
that makes the depth-cap delta measurable per core. The fifth line exists
because the time track had no `model=169` core at all, so without it the delta
would be measurable for classic on both tracks but for `model=169` on only one.
The time track carries `tt`, per the developer's Part 5 answer 2.

`deep=` is an ID parameter, not a module version, so these mint new agents and
re-identify nothing.

---

## Measurements

Every number below names the instrument that produced it. Nothing here is a
projection.

### P2 two-setting control (the validation the developer asked for)

One chip-counting core at four settings, scratch roster, serial play, 12 games,
`ab(deep=12,tt,ord,time=Xms)@2.classic(chip=100)@2` plus a node-budget anchor.
**Post-fix binary:**

| agent | moves | ms/move | nodes/move | eff depth | us/node |
|---|---|---|---|---|---|
| `ab(deep=12,tt,ord,time=50ms)@2.classic(chip=100)@2` | 160 | 20.18 | 122,778 | 6.29 | 0.164 |
| `ab(deep=12,tt,ord,time=150ms)@2.classic(chip=100)@2` | 153 | 58.52 | 300,252 | 7.07 | 0.195 |
| `ab(deep=12,tt,ord,time=450ms)@2.classic(chip=100)@2` | 160 | 180.89 | 993,910 | 7.89 | 0.182 |
| `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2` (anchor) | 167 | 6.30 | 32,751 | 5.59 | 0.192 |

Realized ms/move ratios are **2.90x and 3.09x against a flag ratio of 3.0x**, so
the flag now controls the spend. Effective depth rises across these three points
(6.29, 7.07, 7.89) and stays far below the deep=12 cap, so the budget binds
rather than the depth cap.

**Pre-fix binary, same roster, same settings** (stopped after 9 of 12 games,
because pre-fix `deep=12,time=Xms` is unbounded by construction and the run was
going to take hours, so the figures are per-move means over the games completed):

| agent | moves | ms/move | nodes/move | eff depth | overshoot |
|---|---|---|---|---|---|
| `time=50ms` | 102 | 193.78 | 1,121,454 | 6.81 | 3.9x |
| `time=150ms` | 167 | 900.30 | 6,723,360 | 7.72 | 6.0x |
| `time=450ms` | 138 | 3217.37 | 15,305,284 | 8.45 | 7.1x |
| node anchor | 87 | 6.06 | 32,501 | 5.57 | n/a |

The node anchor is unchanged pre to post (6.06 vs 6.30 ms/move, 32,501 vs
32,751 nodes/move), which is the negative control: the fix does not touch the
node path.

**Utilization caveat, worth the developer's attention before Part 2 runs.**
Post-fix the search finishes at 40.4%, 39.0%, and 40.2% of its allowance at 50,
150, and 450 ms. That follows from the "never start an iteration you cannot
finish" rule at a growth factor near 3.4: the declined iteration would have cost
more than the remaining time, so roughly 60% of the allowance is structurally
unspendable. It is correct behaviour for a hard budget, but it means the time
track's effective compute is about 0.4x its nominal figure, which is a
compute-parity decision (accept it, or spend the remainder on a partial
iteration via `keepPartial`) rather than a bug.

### Clock-sampling cost, which sized the 256-node mask

`steady_clock::now()` measured at **22.13 ns and 21.89 ns per call** over 20M
calls in two runs, against a **0.183 ns per iteration** empty-loop baseline
measured the same way. At the ~270 ns/node this engine runs, sampling every 256
nodes costs 0.086 ns/node, under 0.04% of node cost, and bounds the timing
granularity at about 69 us, under 0.05% of a 150 ms budget. 4096 would have been
16x cheaper and 16x coarser. 256 buys precision that is free at this node cost.

### P8 cost ceiling, measured BEFORE implementing

`std::tanh` costs **10.7 ns/call** and the whole leaf tail costs **17.3
ns/call**, so `lround` plus the clamp accounts for the remaining 6.6 ns
(`lround` is a libm call on MSVC).

**This contradicts the plan's premise for P8.** The plan calls it "the largest
single wall-clock win available to four regimes at once", but the plan's own
table puts the Classic core at 269 ns/node and the TD-Leaf learned core at 275
ns/node. Those are essentially equal, so the learned tail is not where the
learned core's cost sits and cannot explain a 2.2x gap. P8 was implemented
anyway because the developer answered IN and because it must land before the
study or not at all, but **its per-node effect should be measured in Pass 1
rather than assumed from the plan's framing**, and it should not be quoted as
the reason a learned core is or is not fast.

### Identity impact of the P2 fix

`rank.exe determinism --replicas 2` run on both binaries, diffing the TSVs.

- **Node track** (16 `nodes=200k` agents, 32 subject-colours): the two TSVs are
  **byte-identical**, node counts included. The node path is provably untouched.
- **Time track** (14 openless `deep=6,tt,ord,time=150ms` agents, 28
  subject-colours): 24 of 28 tracesets identical, **4 changed** -- `model=111`
  both colours, `model=113` White, `model=96` Black. Several other rows differ
  only in `nodes_self`, which is already documented as not a trajectory
  fingerprint for `time=` agents.

The plan predicted this would affect `model=111` and `model=113`. **`model=96`
is a third, un-flagged core** whose play also changed: it carries no `# cost
flag` roster comment, unlike the other two.

### Stored-game reproducibility

Same design as the 2026-08-28 `TT CROSS-AGENT CONTAMINATION` measurement.
Replaying a random sample of stored rows under the fixed binary via `rank.exe
extract`:

| population | replayable | mismatches | rate |
|---|---|---|---|
| rows with >= 1 side on a `time=150ms` head | 182 | 13 | **7.14%** (Wilson 95% CI 4.2% to 11.8%) |
| rows with neither side on a `time=` head | 240 | 0 | **0.00%** |

Fisher exact two-sided p = 1.4e-05. The control's zero rate matters: there is no
measurable general replay drift today, so the 7.14% is attributable to this fix
rather than sitting on an unknown floor. (The 2026-08-28 measurement's 13.5%
control was against an older store with more intervening code changes.) 29.5% of
the affected population involves `model=111`, `model=113`, or `model=96`,
consistent with the effect concentrating in the cores slow enough that depth 6
never fit inside 150 ms.

Store scope: 274,260 of 815,597 loaded rows (33.6%) involve at least one
`time=150ms` agent, all on the single head `ab(deep=6,tt,ord,time=150ms)@2`.

---

## The open decision: is `ab` bumped `@2` to `@3`?

**It was not bumped, and this is recorded as a pending developer decision rather
than a settled one.** It is the opposite call from the 2026-08-27 TT fix, which
did bump, so the reasoning matters.

The argument for bumping is that P2 changes what a `time=` head computes, and
7.14% of stored games involving one no longer replay. The argument against is
collateral. Module versions have no finer grain than per-module, so bumping `ab`
re-identifies all 211 active `ab(...)` roster lines and orphans **814,817 of
815,597 stored games (99.9%)** in order to correct reproducibility on the 33.6%
that carry a `time=` head, while the node track is provably byte-identical. The
TT bump's collateral was proportionate because that defect reached every `tt`
agent. This one does not.

Not bumping is also the reversible direction: a bump can be applied later, a
completed cascade is far harder to walk back. Registered as
`TIME BUDGET NOT ENFORCED` in `Docs/corrections.md` with the full tradeoff, so
it stays enumerable if revisited, and carried in `todo.md` as an open decision.

## A second finding that changes Part 2's design: theory 59

While validating the fix, `rank.exe determinism --replicas 3 --only
"time=150ms"` was run on the FIXED binary, in-process. It **exits 2**: 31 of 34
subject-colours reproduce, 3 do not.

Which 3 is the whole point:

- both colours of
  `ab(deep=12,tt,ord,time=150ms)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`
  (first divergence at half-moves 24 and 13)
- White of
  `ab(deep=6,tt,ord,time=150ms)@2.learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1`
  (half-move 34)

Every reproducing agent is a `deep=6` core fast enough to finish depth 6 inside
150 ms, where the DEPTH cap stops the search and the clock is never consulted at
the decision point. Non-reproducibility appears exactly where the budget BINDS.

**P1's entire purpose is to make the budget bind on every core.** So raising the
cap to `deep=12` converts the time track from mostly-deterministic to genuinely
stochastic, and `rankAgentIsDeterministic` does not know this: it derives
determinism from whether an agent draws `rand()`, a `time=` agent draws none, so
`pairGameTarget` pins any two of them at exactly 2 games as both floor AND
ceiling. Those 2 games are samples of a noisy process, not replays of one game.

This is the opposite error to `Docs/benchmarking.md`'s defect 3. Defect 3 is too
few DISTINCT games per stored row. This is too few ROWS scheduled for a pair
that is not actually deterministic. Both understate the error bar.

Not fixed here: changing the classifier raises the time track's game targets and
re-opens the 3 time-track category certifications, so it is a deliberate
scheduling decision, not a bug fix to slip into a prerequisites commit. Filed as
theory 59 and as a `todo.md` item.

---

## Divergences from the plan

| Plan said | What was done | Why |
|---|---|---|
| P2 is coarse sampling plus no pre-iteration check | Three fixes, the sticky expiry flag being the load-bearing one | `budgetTripped()` had no memory, so 4095 of every 4096 nodes recursed past the deadline. Sampling granularity was the secondary issue |
| P2 mis-caps `model=111` and `model=113` | Three cores changed: `model=111`, `model=113`, and `model=96` | Measured by determinism traceset diff. `model=96` carries no cost flag in the roster |
| "Build checkpoint marks and a stop, skip resume" | Resume built | Developer Part 5 answer 4: a 16h rung runs for one regime alone, which requires continuing a cumulative ladder |
| P8 is "the largest single wall-clock win available" | Implemented, but the premise is not supported | The tail costs 17.3 ns/call and the plan's own table has Classic at 269 ns/node vs learned at 275, essentially equal. Measure P8's effect in Pass 1, do not assume it |
| Use x3.41 per-ply growth | Growth derived per search from its own last two iterations | x3.41 is Classic's alone. Move-ordering quality differs per evaluator, so it does not transfer |
| P1 rosters the `deep=12` heads | Also rosters one new `deep=6` time-track twin | The time track had no `model=169` core, so the depth-cap delta would have been measurable on only one track for that core |

## How to test

```powershell
.\tools\run_tests.ps1 -Build          # 4471 assertions in 201 test cases
.\rank.exe check                      # 223 active agents, all canonical
.\rank.exe check --roster ranking/climb_roster.txt    # 11 agents
```

The P2 two-setting control, reproducible from a scratch roster holding the same
core at two `time=` values:

```powershell
.\rank.exe play  --roster <scratch roster> --in <scratch store> --games 12
.\rank.exe rate  --roster <scratch roster> --in <scratch store>
```

Then read `ms/move` off the resulting standings and confirm the ratio tracks the
flag ratio.

Theory 59's check, which SHOULD exit 2:

```powershell
.\rank.exe determinism --replicas 3 --only "time=150ms"
```

New trainer behaviour to expect:

```powershell
.\train.exe tdleaf --out models/sweep/tdl --wall-ckpt-at "7200,14400,28800" --seed 1001
.\train.exe tdleaf --out models/sweep/tdl2 --resume models/sweep/tdl.txt --wall-stop 57600 --seed 1001
```

The first writes `tdl_t7200.txt`, `tdl_t14400.txt`, `tdl_t28800.txt` and stops
at 28800 s. Each carries its own `games=`/`secs=`/`nodes=`. The second continues
the same cumulative ladder to 57600 s and refuses to start if the resumed file
carries no spend stamp. A `--wall-stop` run makes `--games` optional: pass
neither and wall clock alone governs the length.

## Commit

```
Fix time-budget enforcement, add wall-clock training rungs, and roster the deep=12 heads

Part 1 prerequisites for the budget-parity rebuild (P1-P8).
```

---

## Future Work

Each entry is tethered to a specific conclusion it could confirm or refute.

1. **The 40% utilization figure has one measurement behind it, on one core.**
   The post-fix control shows 40.4 / 39.0 / 40.2% of allowance spent at three
   budgets, all on `classic(chip=100)@2`. The mechanism (declining an iteration
   costing more than the remainder, at growth ~3.4) predicts the fraction should
   move with the core's growth factor, so a core with lower growth should spend
   a larger share. Untested. It matters because if utilization varies by core,
   the time track is not compute-normalized across cores even after the fix,
   which is the exact property the whole rebuild exists to establish. Test: the
   same three-budget control on `model=169` and on one MLP core, reporting
   realized-over-nominal per core.

2. **`keepPartial` is the untested alternative to leaving 60% unspent.** A
   partial iteration's result is currently discarded unless `g_keepPartial`.
   Spending the remainder on a partial deepening would raise utilization toward
   100%, at the cost of mixed leaf parity (which makes the turn weight live, see
   `Docs/terminology.md`). Whether that is a strength gain or loss at fixed wall
   clock is unmeasured, and it decides whether the 40% figure above is a problem
   or just an accounting fact.

3. **P8's per-node effect is unmeasured on a real agent.** The tail cost was
   measured in isolation (17.3 ns/call) and the fast path proven bit-exact, but
   no before/after us/node was taken on a learned agent in ranked play. Without
   it, "P8 helps the learned regimes" is unsupported, and the plan's framing of
   P8 as the largest available win is actively contradicted by the 269 vs 275
   ns/node figures. Test: `train.exe speed` or a fixed-roster `rank.exe play` on
   one learned core, baseline binary vs current, reporting us/node.

4. **The 7.14% replay-divergence figure rests on 182 replayable games.** The CI
   is 4.2% to 11.8%, wide enough that it cannot distinguish "a handful of slow
   cores" from "a third of the time track". It is also a mixed population: 68 of
   250 sampled rows had unparseable or stale IDs and were dropped, and that drop
   is not random with respect to agent age. It matters because this number is
   the evidence base for NOT bumping `ab` to `@3`. Test: a larger sample
   stratified by core, reporting a per-core divergence rate rather than a pooled
   one.

5. **Theory 59 was measured on 34 subject-colours against one probe.**
   `determinism` uses a single fixed probe opponent, so the 3 non-reproducing
   cases are 3 positions' worth of evidence, not a rate over the position space.
   Since the conclusion (the `deep=12` time track is stochastic, so its pairs
   need real game targets) drives a scheduling change affecting 3 category
   certifications, it deserves a second probe and more replicas before the
   classifier is changed.

6. **No `gaz` agent's node count has been read back through the ranking
   pipeline.** P3's counters are unit-tested against the `g_lastNodes > 1` gate,
   but no actual `gaz` game has been played and rated since, so "node cost is
   now measurable for `gaz`" is verified at the unit level and not end to end.
   Test: play a handful of games for one rostered `gaz` agent and confirm
   `nodes_move` in `ranking/ratings.tsv` is non-zero.

7. **`--wall-ckpt-at` has only been exercised at sub-second marks.** The
   integration tests use 0.35 s and 0.7 s so the suite stays fast. Nothing has
   run an actual multi-hour ladder, so clock drift, the resume path against a
   large real checkpoint, and the interaction with `tools/tdleaf_study.ps1`'s
   own CSV resume logic are all untested at the scale Part 2 will use. This is
   the single most likely place Part 2 stumbles on its first day.

8. **`selfplay-supervised`, `dist-value`, and the hill climber did not get wall
   rungs.** P4 names five entry points, and two got them (`tdleaf`, `gumbelzero`).
   The other three are epoch- or iteration-structured rather than
   game-structured and need their own unit decision (what does `tbUnits` count
   for an epoch-based trainer?). Part 2 cannot compare those three regimes at
   matched wall clock until this is done, so it blocks a subset of the study
   rather than all of it.

## Ideas This Inspired

Lower bar than Future Work: these are not tethered to a specific conclusion.

- **A budget-utilization column in `report.md`.** Realized ms/move over nominal
  budget, per agent. It would have made the pre-fix time track's 3.9x-to-7.1x
  overshoot visible on every rating run for months instead of waiting for a
  targeted audit, and it now makes the 40% under-spend visible too. Cheap: both
  numbers are already stored per row.

- **A determinism CLASS on the agent rather than a derived boolean.**
  `rankAgentIsDeterministic` answers yes/no from whether `rand()` is drawn, and
  theory 59 shows that is the wrong question. A three-way class (deterministic /
  seed-stochastic / load-stochastic) would let `pairGameTarget` schedule
  correctly for each, and load-stochastic agents could get a distinct-trajectory
  audit automatically.

- **Make the predictive check's abstention visible.** `nextIterationFits`
  silently returns true until two iterations are timed, which means the first
  two depths of every search are unprotected. A counter for "searches that
  overshot while abstaining" would show whether that window ever matters, and it
  is one increment.

- **A "budget binds?" assertion in the test suite.** A test that plays one move
  at `deep=12,time=50ms` and asserts `g_lastBudgetKind == BUDGET_TIME` would
  have failed loudly against the pre-fix binary, and would catch a future
  regression that reintroduces depth-capping. Nothing in the suite currently
  asserts WHICH budget ended a search.

- **Store the growth factor per move.** The search now computes a per-iteration
  growth ratio and throws it away. Recording it alongside eff-depth would give a
  measured branching factor per evaluator, which is exactly the number the plan
  had to guess at (x3.41) and the number every depth projection needs.

- **A cheap pre-flight for any new roster head.** Before rostering a head, play
  one game and report realized-over-nominal budget and effective depth. Both
  defects this session touched (the depth cap silently binding, the time budget
  not binding) would have shown up in one line of output.
