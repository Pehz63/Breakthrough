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

## Fixed-depth calibration, and the move-agreement experiment

This section covers work done after the Part 1 commit, in response to the
developer's objection that the post-fix `time=` agent uses only ~40% of its
allowance and that unreproducible stored games are not acceptable. The
developer proposed a loose compute constraint (a fixed depth close to the
budget) instead of a hard one. What follows is the measurement that was run to
evaluate that proposal, plus a third option the measurement surfaced.

### Full 15-core fixed-depth calibration

Every core currently on the roster's time track was run at a ladder of fixed
depths with `tt,ord` and no budget, on `boards/board1.txt`, and its cpu ms per
move recorded from the match store's `wcpu`/`wmv` fields. The depth closest to
250 ms in log space was taken as that core's pick.

| core | pick | cpu ms/move at pick |
|---|---|---|
| `classic(chip=100)@2` | `deep=9` | 198.9 |
| `learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1` | `deep=8` | 100.3 |
| `learned(model=99,59815079,pool_games,lin,shape=129-1)@1` | `deep=8` | 155.9 |
| `learned(model=94,784bb5fe,pool_games,lin,shape=129-1)@1` | `deep=8` | 166.0 |
| `learned(model=8,6f1a4264,pool_games,lin,shape=129-1)@1` | `deep=8` | 173.5 |
| `learned(model=3,68364898,pool_games,lin,shape=129-1)@1` | `deep=8` | 204.2 |
| `learned(model=98,5801570e,pool_games,lin,shape=129-1)@1` | `deep=8` | 218.7 |
| `learned(model=111,78ef6974,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` | `deep=6` | 220.1 |
| `learned(model=4,eb105733,pool_games,lin,shape=129-1)@1` | `deep=8` | 220.7 |
| `learned(model=95,07792e69,pool_games,lin,shape=129-1)@1` | `deep=8` | 223.7 |
| `learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1` | `deep=8` | 228.5 |
| `learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` | `deep=6` | 231.6 |
| `learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1` | `deep=8` | 243.8 |
| `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | `deep=8` | 282.2 |
| `learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1` | `deep=8` | 284.2 |

**Wall-clock spread at the picks: 2.83x** (100.3 to 284.2 ms/move). Measured
per-ply cost growth runs 3.39x (classic, the low outlier) to 4.39x across the
learned cores, so the best a closest-integer-depth rule can do is roughly
`sqrt(4.4) = 2.1x` worst case. The observed 2.83x is near that floor and is not
fixable by choosing depths more carefully. This is the price of the fixed-depth
mechanism, not a defect in how the depths were picked.

`model=97` is the core that pulls the range: `deep=8` costs it 100.3 ms, and
`deep=9` was not measured. Projected from its own d7 -> d8 growth it lands near
390 ms, which is farther from 250 in log space than 100.3 is, so `deep=8`
remains the pick. The projection is not a measurement and `deep=9` should be
run before any fixed-depth roster is frozen.

### Tree size at fixed depth varies 2.86x across identically shaped models

At `deep=8`, over the 12 linear `shape=129-1` cores:

| core | nodes/move at d8 | cpu ms/move at d8 |
|---|---|---|
| `model=97` | 453,296 | 100.3 |
| `model=99` | 698,433 | 155.9 |
| `model=94` | 738,334 | 166.0 |
| `model=8` | 796,470 | 173.5 |
| `model=3` | 852,984 | 204.2 |
| `model=98` | 943,457 | 218.7 |
| `model=95` | 954,505 | 223.7 |
| `model=4` | 978,417 | 220.7 |
| `model=96` | 1,038,100 | 228.5 |
| `model=76` | 1,138,558 | 243.8 |
| `model=10` | 1,213,572 | 284.2 |
| `model=169` | 1,296,527 | 282.2 |

Same architecture, same parameter count, same search flags, same depth, and a
2.86x range in the size of the tree searched. The difference is how well each
trained evaluator's scores order moves for the alpha-beta cutoffs. It is a
property of the weights, not of the shape, so it cannot be predicted from the
model definition and has to be measured per trained model.

### us/node is set by architecture class, not by parameter count

Measured at each core's calibration pick:

| class | us/node | n cores |
|---|---|---|
| `classic(chip=100)@2` | 0.148 | 1 |
| linear learned, `shape=129-1` | 0.214 to 0.239 | 12 |
| MLP learned, `mu_shape=129-512-8-1` | 2.677, 2.682 | 2 |

Twelve independently trained linear models sit inside a 12% band and the two
MLP models are 0.2% apart, so per-node cost is predictable from the
architecture before the model is trained. The mechanism is the incremental
accumulator: a linear model's leaf is `g_mlAcc + skipW*g_chipDiff + stmW`,
a handful of flops no matter how many inputs it has, because the 129-input dot
product is maintained in make/unmake. The MLP maintains its 512-wide first
hidden layer the same way but still pays the `512 -> 8 -> 1` tail at every
leaf, about 4,100 multiply-adds. Leaf cost therefore tracks the tail after the
first hidden layer, not the parameter count.

### us/node is NOT stable across depth within one core

| core | d4 | d5 | d6 | d7 | d8 | d9 |
|---|---|---|---|---|---|---|
| `classic(chip=100)@2` | | 0.1408 | 0.2207 | 0.1432 | 0.2237 | 0.1481 |
| `learned(model=169,...,lin,shape=129-1)@1` | 0.2145 | 0.1841 | 0.2037 | 0.1655 | 0.2177 | |
| `learned(model=96,...,lin,shape=129-1)@1` | 0.1804 | 0.1791 | 0.1929 | 0.1648 | 0.2201 | |
| `learned(model=113,...,mlp,...)@1` | 2.8987 | 2.8753 | 2.6774 | | | |

Odd depths come in cheaper per node than the even depths on either side of
them, on every core measured. The pattern is not an opponent-mix artifact:
splitting `model=10`'s ladder by opponent gives d6 0.2039-0.2457, d7
0.1642-0.1880, d8 0.2310-0.2475, so the depth term dominates the opponent term.
**The mechanism is not established here.** The practical consequence is what
matters: a node budget derived from us/node measured at one depth will miss
when the agent under that budget settles at a different depth.

That is exactly what happened. Budgets derived from the d8 (even) us/node came
in at a mean of 202.7 ms/move against a 250 ms target, a 19% undershoot. See
the next subsection.

### Per-core calibrated node budgets, measured

The determinism problem in P2 was the *time* budget, not the idea of a budget.
A node budget is already deterministic, which is what the live `nodes=200k`
track is. So the time track can instead be redefined as a per-core node budget
calibrated to 250 ms, which keeps determinism and has a continuous knob.

First pass, budgets derived from d8 us/node, 240 games over 16 agents
(`cal2/r_verify.txt` -> `cal2/m_verify.jsonl`):

| core | budget | realized cpu ms/move | realized eff depth |
|---|---|---|---|
| `learned(model=10,...)@1` | `nodes=1067k` | 179.7 | 6.99 |
| `learned(model=3,...)@1` | `nodes=1044k` | 182.1 | 7.25 |
| `learned(model=98,...)@1` | `nodes=1079k` | 186.6 | 7.21 |
| `learned(model=95,...)@1` | `nodes=1067k` | 186.9 | 7.11 |
| `learned(model=4,...)@1` | `nodes=1108k` | 187.4 | 7.17 |
| `learned(model=169,...)@1` | `nodes=1148k` | 187.5 | 6.94 |
| `learned(model=97,...)@1` | `nodes=1130k` | 187.6 | 7.31 |
| `learned(model=94,...)@1` | `nodes=1112k` | 187.9 | 7.16 |
| `learned(model=96,...)@1` | `nodes=1136k` | 189.9 | 7.27 |
| `learned(model=99,...)@1` | `nodes=1120k` | 195.4 | 7.36 |
| `learned(model=8,...)@1` | `nodes=1148k` | 198.4 | 7.32 |
| `learned(model=76,...)@1` | `nodes=1168k` | 204.9 | 7.22 |
| `learned(model=113,...,mlp)@1` | `nodes=93k` | 239.8 | 5.29 |
| `learned(model=111,...,mlp)@1` | `nodes=93k` | 245.8 | 5.36 |
| `classic(chip=100)@2` | `nodes=1689k` | 280.3 | 7.80 |

**Realized spread 1.56x** (179.7 to 280.3), mean 202.7 against the 250 ms
target. Compare the fixed-depth mechanism's 2.83x. Unlike fixed depth, the
level error and the residual spread are both correctable, because the knob is
continuous: rescaling each budget by `250 / realized` gives the second-pass
roster in `cal2/r_verify2.txt`.

Caveat on all of these numbers: cpu ms comes from `GetProcessTimes`, and the
runs shared the machine with other work in this session. Absolute levels are
therefore soft. The spread across cores, which is the number the decision turns
on, is a ratio measured under one set of conditions and is far more robust than
the levels.

### The rescale round: 1.19x

Second pass, every budget rescaled by `250 / realized` from the first pass, 240
games over 16 agents (`cal2/r_verify2.txt` -> `cal2/m_verify2.jsonl`):

| core | budget | realized cpu ms/move | realized eff depth |
|---|---|---|---|
| `learned(model=113,...,mlp)@1` | `nodes=97k` | 240.0 | 5.36 |
| `learned(model=111,...,mlp)@1` | `nodes=95k` | 240.0 | 5.39 |
| `classic(chip=100)@2` | `nodes=1506k` | 245.5 | 7.62 |
| `learned(model=3,...)@1` | `nodes=1433k` | 250.5 | 7.37 |
| `learned(model=97,...)@1` | `nodes=1506k` | 252.2 | 7.57 |
| `learned(model=98,...)@1` | `nodes=1446k` | 252.8 | 7.43 |
| `learned(model=10,...)@1` | `nodes=1485k` | 254.4 | 7.24 |
| `learned(model=4,...)@1` | `nodes=1478k` | 257.2 | 7.47 |
| `learned(model=99,...)@1` | `nodes=1433k` | 257.2 | 7.43 |
| `learned(model=8,...)@1` | `nodes=1446k` | 258.0 | 7.42 |
| `learned(model=96,...)@1` | `nodes=1496k` | 261.1 | 7.45 |
| `learned(model=169,...)@1` | `nodes=1531k` | 261.6 | 7.23 |
| `learned(model=94,...)@1` | `nodes=1479k` | 263.1 | 7.53 |
| `learned(model=95,...)@1` | `nodes=1427k` | 264.8 | 7.40 |
| `learned(model=76,...)@1` | `nodes=1425k` | 284.4 | 7.37 |

**Spread 1.19x** (240.0 to 284.4), mean 256.2 against the 250 ms target, from
1.56x and mean 202.7 on the first pass. One rescale pass closed both the level
error and most of the spread. `model=76` is the remaining outlier at 284.4,
because its us/node rose from 0.2141 to 0.2264 between passes rather than
holding.

Ranking of the three mechanisms on wall-clock parity, all measured on the same
15 cores:

| mechanism | deterministic | wall-clock spread |
|---|---|---|
| `time=150ms`, budget enforced | no | 1.00x by construction |
| per-core calibrated `nodes=`, after one rescale | yes | **1.19x** |
| per-core calibrated `nodes=`, first pass | yes | 1.56x |
| fixed depth `cal=250ms` | yes | 2.83x, not reducible |

### What a node budget actually normalizes

All 15 cores were run at `nodes=200k`, the live node track's setting, over 240
games:

| core | cpu ms/move | eff depth | nodes/move |
|---|---|---|---|
| `classic(chip=100)@2` | 28.7 | 6.32 | 176,487 |
| `learned(model=97,...)@1` | 32.0 | 6.23 | 177,187 |
| `learned(model=10,...)@1` | 33.0 | 5.92 | 177,216 |
| `learned(model=169,...)@1` | 33.3 | 5.95 | 178,419 |
| `learned(model=76,...)@1` | 33.5 | 5.95 | 179,379 |
| `learned(model=96,...)@1` | 33.5 | 6.11 | 179,598 |
| `learned(model=8,...)@1` | 33.6 | 6.08 | 178,166 |
| `learned(model=95,...)@1` | 33.7 | 6.17 | 178,309 |
| `learned(model=3,...)@1` | 33.8 | 6.14 | 178,045 |
| `learned(model=4,...)@1` | 33.8 | 6.05 | 178,808 |
| `learned(model=94,...)@1` | 33.8 | 6.07 | 177,659 |
| `learned(model=98,...)@1` | 34.0 | 6.16 | 178,935 |
| `learned(model=99,...)@1` | 34.1 | 6.21 | 178,169 |
| `learned(model=113,...,mlp)@1` | 515.8 | 5.88 | 180,902 |
| `learned(model=111,...,mlp)@1` | 527.4 | 5.89 | 179,671 |

**Effective depth spans 0.43 plies (5.88 to 6.32) while wall clock spans
18.4x (28.7 to 527.4 ms/move).** That is the cleanest statement of what the two
tracks are:

- **A node budget normalizes search DEPTH.** Every core reaches about the same
  depth, and an expensive evaluator is charged nothing for being expensive. The
  question it answers is "whose evaluator is better, given the same amount of
  search".
- **A time budget normalizes WALL CLOCK.** Every core gets the same second, and
  an expensive evaluator pays for itself in plies. The question it answers is
  "who plays best per second".

So `nodes=200k` is not arbitrary in effect even though the number was picked
that way: it pins the whole roster at about 6.1 plies. The principled
restatement of the node track's definition is "search depth fixed at ~6.1
plies", and the node count is just how that is spelled.

### Choosing the node track's setting

There is no single node count "comparable to 250 ms", because at 250 ms the
cores do not land at a common depth: the 13 linear and classic cores reach 7.23
to 7.62 plies while the two MLP cores reach 5.36 to 5.39. Equalizing wall clock
is precisely what makes depth unequal. What the node count controls is the
depth the node track probes.

Measured node-count to depth mapping, from the three full-roster runs:

| node budget | eff depth, classic + linear | eff depth, MLP | cpu ms/move, linear | cpu ms/move, MLP |
|---|---|---|---|---|
| `nodes=200k` | 5.92-6.32 | 5.88-5.89 | 28.7-34.1 | 515.8-527.4 |
| `nodes=~1100k` | 6.94-7.36 | 5.29-5.36 (at 93k) | 179.7-204.9 | 239.8-245.8 (at 93k) |
| `nodes=~1450k` | 7.23-7.62 | 5.36-5.39 (at ~96k) | 245.5-284.4 | 240.0 (at ~96k) |

Raising the node track toward `nodes=1450k` would put it at about 7.4 plies,
the same depth the time track's fast cores reach, and would cost those cores
about 250 ms/move, the same as the time track. **That would make the two tracks
nearly redundant for 13 of the 15 cores**, differing only for the MLP class.
The measured agreement backs this up from the other direction: at `nodes=200k`
against `deep=8`, `model=169` picks a different move 44% of the time, so the
two tracks are currently asking genuinely different questions.

Lowering it makes the node track shallower than 6 plies and saves almost
nothing, since the 13 fast cores already cost only 28.7 to 34.1 ms/move there.
The only core `nodes=200k` is expensive for is the MLP at ~520 ms/move, and
that expense is the node track working as intended: it is the subsidy that lets
an expensive evaluator be judged on evaluator quality rather than on speed.

### Move agreement: `rank.exe agree`

A new subcommand was written because nothing existing could answer the
developer's question. `determinism` and `pairgen` compare whole games, so after
the first divergence the two agents stand in different positions and their
later moves are no longer comparable. `agree` keeps them in lockstep: it
snapshots the position, runs the driver, records where the driver left the
board, rewinds to the snapshot, runs the other agent from the identical
position, compares, then restores the driver's move and continues. Each pair is
run in both directions (each agent drives a set of games while the other is
polled), because the driver's own trajectory decides which positions get judged.

Two confounds are handled in the implementation rather than in the write-up.

1. **Transposition-table leakage.** The TT searcher context keys on evaluator,
   eval params, quiescence and root side, and not on the budget, so two agents
   differing only in `deep=` versus `nodes=` share one table. Polling the
   second agent right after the first searched the same root would let it read
   the first agent's stored result back out, which would measure cache reuse
   and report it as agreement. The TT is therefore wiped before both searches.
   The cost is that this is cold-TT play, where a rostered game carries each
   agent's table across its own moves.
2. **Forced plies.** A position with `<= 1` legal move agrees trivially. Those
   are counted separately and excluded from the reported rate. In practice
   every run reported 0 forced plies, so this changed nothing, but the count is
   printed so a future run cannot hide behind it.

Instrument validation, run before any number below was read:

- **Self-agreement is exactly 1.0.** An agent polled against itself must repeat
  its own move at every ply. Anything less would mean the snapshot/rewind leaks
  state (board counters, the incremental eval accumulator `g_evalPos`, the ML
  accumulator `g_mlAcc`, or a stale TT entry) and every agreement number would
  be measuring that leak. This is asserted as a unit test in
  `tests/test_ranking.cpp`, not just checked once by hand.
- **Anti-vacuity.** The same test asserts that `deep=2` versus `deep=5` on one
  core comes back strictly below 1.0. It measured 51.1%, so the metric is not
  pinned at agreement by construction.
- **Direction symmetry.** Every pair below agreed to within 2.6 points between
  its two directions, so the answer does not depend on whose trajectory is
  being walked.

### Agreement results

Six games per direction, `--open-plies 6`, seed 4242, `boards/board1.txt`.
Random opening plies are mandatory because both agents are deterministic and
would otherwise replay one trajectory. Polls are non-forced plies pooled over
both directions.

**Compute-matched** (fixed depth against the rescaled node budget, both
targeting ~250 ms/move):

| core | fixed depth | node budget | agreement | 95% CI | polls |
|---|---|---|---|---|---|
| `classic(chip=100)@2` | `deep=9` | `nodes=1506k` | 96.9% | 95.2-98.0% | 581 |
| `learned(model=169,...,lin)@1` | `deep=8` | `nodes=1531k` | 70.9% | 67.7-74.0% | 805 |
| `learned(model=113,...,mlp)@1` | `deep=6` | `nodes=97k` | 61.7% | 58.1-65.1% | 749 |

**Track-realistic** (the proposed fixed-depth time-track agent against the live
`nodes=200k` node-track agent, which is what the two champion tracks would
actually hold):

| core | fixed depth | node budget | agreement | 95% CI | polls |
|---|---|---|---|---|---|
| `classic(chip=100)@2` | `deep=9` | `nodes=200k` | 94.1% | 91.7-95.9% | 511 |
| `learned(model=113,...,mlp)@1` | `deep=6` | `nodes=200k` | 88.1% | 85.6-90.2% | 748 |
| `learned(model=169,...,lin)@1` | `deep=8` | `nodes=200k` | 55.6% | 52.0-59.1% | 745 |

An earlier compute-matched pass used the first-generation budgets, which
undershot on the learned cores. It read classic 98.0%, `model=169` 62.0%,
`model=113` 62.7%. Those numbers are superseded by the table above because the
`model=169` comparison was mismatched by about 1.5x in wall clock. They are
recorded here so the correction is visible rather than quietly dropped.

### Why classic is the outlier, and why its 97% does not generalize

`classic(chip=100)@2` resolves to turn weight 1, chip weight 100, wall 0,
column 0. The turn weight is inert at fixed depth. `evalPosFull` returns 0 when
wall and column are both 0. So the evaluator's score for any non-terminal
position is exactly `g_chipDiff * 100`, a pure material count, plus the
`nearWinCheck` shortcut. Every quiet move in a position with the same material
scores identically, and the tie is broken by move-generation order, which both
budgets share.

That is a fact about the evaluator read off the registry (`src/ai_eval.cpp`,
`g_evaluators[0]`) and the codec's weight-subset rule, not an inference from
the agreement numbers. The inference, which is a hypothesis and is labelled as
one, is that this is *why* classic's move choice is nearly depth-insensitive.
The supporting observation, taken from the compute-matched runs and split by
what effective depth the polled agent actually reached:

| core | polled reached the SAME eff depth | polled reached a DIFFERENT eff depth |
|---|---|---|
| `classic(chip=100)@2` | 0 of 68 disagreed (0.0%) | 18 of 513 disagreed (3.5%) |
| `learned(model=169,...,lin)@1` | 3 of 62 disagreed (4.8%) | 231 of 743 disagreed (31.1%) |
| `learned(model=113,...,mlp)@1` | 6 of 51 disagreed (11.8%) | 281 of 698 disagreed (40.3%) |

Two things fall out. First, when both agents reach the same effective depth
they agree 88% to 100% on every core, so the disagreement is driven by the
depth actually reached and not by the budget rule as such. Second, the two
rules produce different depths by construction: fixed depth completes depth `d`
everywhere, while a node budget spends more of its allowance on hard positions
and less on easy ones and lands at a fractional effective depth that varies
position to position. On classic that variation almost never changes the move.
On the learned cores it changes the move 31% to 40% of the time.

The consequence for the decision is what matters. Classic's 97% must not be
quoted as "the budget rule barely matters". On the learned cores, which are
what the ML work is about, the two rules pick different moves 29% to 38% of the
time at matched compute.

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

9. **`model=97` at `deep=9` was never measured.** It is the core that sets the
   low end of the fixed-depth mechanism's 2.83x wall-clock spread, at 100.3 ms
   for `deep=8`. Its `deep=9` cost is projected near 390 ms from its own d7 ->
   d8 growth, which would keep `deep=8` as the log-space pick but change the
   spread figure to 390/166 = 2.35x if `deep=9` were forced. If a fixed-depth
   roster is ever frozen, run the d9 rung first. The projection must not be
   quoted as a measurement.

10. **The us/node depth-parity effect has no established mechanism.** Odd search
    depths measured cheaper per node than the even depths on either side, on
    every one of the 15 cores. This directly caused the 19% level miss in the
    first node-budget calibration pass, so the effect is load-bearing, not a
    curiosity. Candidate explanations not yet tested: the ratio of evaluated
    leaves to interior nodes changing with parity, the `nearWinCheck` shortcut
    firing at different rates at leaf parity, or a `GetProcessTimes`
    quantization interaction. The test that would settle it is instrumenting
    `g_lastLeafs` alongside `g_lastNodes` in the match store and recomputing
    the cost per LEAF rather than per node across the same ladder. If cost per
    leaf is flat across depth, the leaf-fraction explanation is confirmed and a
    leaf budget would calibrate more cleanly than a node budget.

11. **Move agreement was measured on 3 cores, not 15.** `classic` came in at
    96.9% and both learned cores at 61.7% to 70.9%, so the split is currently
    "one coarse hand-written evaluator versus two learned ones". Whether the
    97% is specific to a pure-material evaluator or generalizes to any
    low-resolution one is untested. The cheapest test that would separate them
    is running `agree` on `advanced(...)` or on `classic` with `wall` and
    `column` weights made non-zero: if a hand-written evaluator with a
    fine-grained positional term drops toward 70%, resolution is the variable
    and the "pure material" reading is right.

12. **Agreement is measured with a cold transposition table on every search.**
    The wipe is required to stop the polled agent reading the driver's entries
    out of the shared searcher context, but it means neither agent gets the
    cross-move table reuse it has in a rostered game. Warm-TT agreement could
    be higher (both agents converge on the same cached lines) or lower (the
    node-budget agent's savings buy it extra depth). Settling it needs two
    private tables keyed per agent, which the current single global table
    cannot express.

13. **No Elo consequence of any of this has been measured.** Every number in
    the calibration and agreement sections is compute accounting and move
    choice. Whether a fixed-depth or calibrated-node time track reorders the
    standings, and by how much, needs a full-roster unpinned refit with the two
    tracks read separately. In particular the observation that `nodes=200k`
    hands the MLP cores 12.3x the wall clock of the linear cores predicts that
    the MLP is rated higher on the node track than on any wall-clock-normalized
    track, and that prediction is untested.

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

- A **leaf budget** instead of a node budget. If the depth-parity effect in
  us/node turns out to be a leaf-fraction effect, then cost per LEAF is the
  stable quantity and `leaves=` would calibrate to a wall clock in one pass
  instead of needing a rescale round.
- **Per-agent transposition tables.** The single global table keyed on a
  searcher context that excludes the budget is what forced the TT wipe in
  `agree`. Keying the table per agent would let two budget variants be polled
  from one position without either wiping or leaking, and would also remove a
  cross-agent coupling nobody has ever measured the size of.
- **Agreement as a cheap roster-pruning tool.** Two agents agreeing on 97% of
  moves cannot differ by much Elo, so `agree` could screen a candidate cohort
  before spending games on it. Calibrating the agreement-to-Elo relationship on
  pairs whose Elo gap is already known would turn a 12-game run into a filter
  for a 500-game one.
- **Report effective-depth variance, not just the mean.** The whole difference
  between the two budget rules showed up as "did the polled agent reach the
  same effective depth", and a node budget's depth varies position to position
  by construction. A per-agent standard deviation of `g_lastEffDepth` in the
  match store would make that visible without a bespoke experiment.
