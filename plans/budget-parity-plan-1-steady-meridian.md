# Budget-parity rebuild: every regime, both leaderboards, matched training compute

**Status: PLAN ONLY. Nothing in here has been run.** Written 2026-09-01 from a
research pass over the roster, match store, plans, and source. The companion
results doc (`budget-parity-results-1-steady-meridian.md`) does not exist yet and
should be created when the work is executed.

## Why this exists

The project declares two compute-normalization tracks, `nodes=200k` and
`time=150ms` (`ranking/CHAMPION.md`). The goal of this round is to give every
agent-production regime its best shot under each track, with training compute
equalized, so the two leaderboards compare methods rather than accidents of how
much each one happened to be tuned.

## The finding that motivates the whole plan

**Neither track currently constrains compute.** Measured by streaming
`ranking/matches.jsonl` (623,774 rows), matching exact canonical IDs, and
aggregating the per-side `wnod`, `wms`, `wed`, `wsn` telemetry over 70,000 to
92,000 searches per agent:

| Openless agent | nodes/move | of 200k | ms/move | of 150ms | eff depth |
|---|---|---|---|---|---|
| `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2` | 41,841 | 21% | 11.26 | n/a | 5.55 |
| `ab(deep=6,tt,ord,time=150ms)@2.classic(chip=100)@2` | 41,819 | n/a | 11.22 | **7.5%** | 5.55 |
| `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | 91,807 | 46% | 25.29 | n/a | 5.63 |
| `ab(deep=6,tt,ord,nodes=200k)@2.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1` | 80,396 | 40% | 22.07 | n/a | 5.61 |
| `ab(deep=6,tt,ord,time=150ms)@2.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1` | 68,702 | n/a | 18.01 | 12% | 5.62 |
| `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2` (reference class) | 485,343 | 24% of 2m | 131.37 | n/a | 7.28 |

Every agent is limited by `deep=6`, not by its budget. `deep=N` is a hard cap on
iterative deepening whenever a budget is set (`src/ai_minimax.cpp`, the
`for (int d = 1; d <= depth; d++)` loop). The same core costs 41,841 nodes/move on
the node head and 41,819 on the time head, so **the two tracks are currently the
same instrument measured twice**. The only genuinely budget-limited agent in the
roster is `ab(deep=6,noab,nodes=200k)@2.classic(chip=100)@2`, which has no
alpha-beta pruning and falls to eff depth 3.26. That is the positive control
proving the budget machinery works, and nothing else reaches it.

Two consequences shape the design.

**`tt` costs wall clock.** At `deep=6` with the chip counter,
`ab(deep=6,nodes=200k)@2` runs 133,222 nodes/move at 8.02 ms/move while
`ab(deep=6,tt,ord,nodes=200k)@2` runs 41,841 nodes/move at 11.26 ms/move. The
transposition table cuts nodes 3.2x and still loses on the clock, which is
`positionKey`'s per-node string build (`todo.md` carries the incremental-Zobrist
item). Once budgets bind, the node track should prefer `tt` and the time track
should prefer dropping it. That divergence is the entire reason to run two tracks,
and it does not exist today.

**Node growth per ply on the real `tt,ord` path is x3.41.** Anchored on two
measured cumulative-per-move points for `classic(chip=100)@2`: deep=6 gives 41,841
nodes and 11.26 ms, deep=8 (at a non-binding `nodes=2m`) gives 485,343 and 131.37.
The ratio over two plies is 11.60 on nodes and 11.67 on milliseconds, so x3.41 per
ply, consistent on both units. The bare unbudgeted ladder (`ab(deep=1..5)@2`, at
24 / 113 / 1,185 / 4,005 / 36,621 nodes/move) gives x5.6 instead, so the TT and
move ordering become MORE effective with depth. Use 3.41 for the target heads and
5.6 only for bare heads.

## Decisions taken (developer, 2026-09-01)

1. **Regimes in scope: 8.** Listed in Part 2. Three more are deferred to Part 3.
2. **The heuristic arm is Advanced only**, re-hill-climbed. Advanced's 16 weights
   are a strict superset of Experimental's 5, and `tools/hill_climb.ps1` already
   targets `adv(...)`.
3. **The `pool_games` tag splits into three regimes**: found-data pool replay,
   oracle-vs-champ, champdil-vs-champ. Justified by measurement below.
4. **Gumbel MCTS enters via calibrated `sims=`**, not by adding `nodes=`/`time=`
   grammar to `gaz()`.
5. **`deep=` becomes a non-binding ceiling** (deep=12), leaving the budget as the
   only constraint.
6. **Training compute is normalized on wall clock per seed**, on this machine.
7. **The rung ladder is cumulative wall clock: 2h, 4h, 8h**, with a checkpoint
   saved at each mark and every rung rated. This gives the training-scaling curve
   rather than a single point. Doubling past 8h only after the developer confirms,
   and only for regimes still improving at 8h.
8. **position_elo redesigns its labeling campaign to fit the budget** rather than
   repeating its original 15-hour one or treating labeling as free infrastructure.
9. **Contaminated historical training data is not reused.** Every regime
   regenerates its own data inside its budget.

## Why the `pool_games` split is a measurement result, not bookkeeping

`pool_games` is emitted for any `teacher=replay:<path>` (`src/ranking.cpp:430`),
so it does not distinguish found data from purpose-generated data. Measured spread
within the one tag, bare loadout, from `ranking/standings.tsv`:

| slot | actual data source | node Elo | time Elo |
|---|---|---|---|
| 96 | champdil-vs-champ pairgen, 4,000 games | 1129 | 1117 |
| 98 | oracle-vs-champ pairgen, 2,000 games | 1115 | 1119 |
| 99 | pure `rank.exe extract` found data, 189,538 positions | 934 | 935 |
| 94 | 56% extract + 44% pairgen, 171,721 positions | 864 | 855 |
| 95 | 100% pairgen from a depth-2 generator, 177,382 positions | 848 | 859 |

Genuine found-data replay sits 180 to 195 Elo below the pairgen arms. Treating the
tag as one regime would average across that gap and report a number describing
nothing.

## TT contamination: what is discarded and why

Rule (`Docs/corrections.md`, `TT CROSS-AGENT CONTAMINATION`): a stored game is
contaminated only if BOTH sides carry `tt`. Measured impact, 30.9% outcome
mismatch on replay of such games against a 13.5% baseline for everything else.

| Regime | Generators | Verdict |
|---|---|---|
| oracle-vs-champ | `ab(d8,tt,ord,nb2m)@1` vs `ab(d6,ord,nb200k)@1` | **clean**, one side only |
| champdil-vs-champ | `ab(d6,ord,nb200k)@1` vs itself, diluted | **clean**, neither side |
| Gumbel-Zero | `gaz(...)`, never carries `tt` | **clean** |
| Advanced hill climb | bare `ab(d4)@1` candidate vs `climb_roster.txt` | games clean, opponent RATINGS contaminated |
| Found-data pool replay | `rank.exe extract` over a `tt`-heavy store, 2026-07-06 | contaminated |
| position_elo | 11-rung ladder | 41.7% of games, **100% of labels** |
| tdleaf_self | self-play at `ab(d6,tt,ord,nb200k)` | ambiguous, see R7 |

Generators confirmed at `tools/train_vs_champion.ps1:62-63`.

**position_elo, decomposed exactly.** Ladder rungs 0 to 6 carry `tt`
(`ab(d6,tt,ord,nb200k)@1`, `ab(d8,tt,ord,nb2m)@1`), rungs 7 to 10 are `ab(d4)@1`
bare. The eval tier draws only from rungs 0 to 6, so all 296,800 eval games are
`tt` vs `tt`. The 189,720 salvaged v1 rows used the same rungs. In the train tier
only the d8 premium pairings are contaminated, 1.5 of 33.5 games per position.

| Slice | Games | Status |
|---|---|---|
| train, v2 bulk grid + style anchors | ~727,000 | clean |
| train, v2 d8 premium pairs | ~34,100 | contaminated |
| train, v1 salvaged rows | 189,720 | contaminated |
| eval tier | 296,800 | contaminated |
| **total** | **1,247,684** | **41.7% contaminated** |

Partitioning does not save it. `labelfit` converts raw outcomes into Elo labels
using each rung's rating from `data/labels/ratings_snapshot.tsv`, frozen
2026-07-19 over a contaminated store, and Bradley-Terry is joint, so **every label
is affected even where its own games were clean**. That is the cross-strength
read: a shallow agent reading a deeper agent's entries plays above its depth and
rates too high. Dropping every `tt` rung to be safe leaves only the d4-bare bulk
grid, roughly 636,100 games spanning rungs rated ~216 to ~714 on the old scale.
Large in rows, structurally gutted, because every strong anchor is gone and the
ladder's whole design was to anchor positions against strong play.

---

# Part 1: cross-cutting prerequisites

**None of the regime work in Part 2 can start until these land.** Ordered by
dependency.

## P1. Make `deep=` a non-binding ceiling

Set `deep=12` on both category heads so the budget is the only constraint. A fixed
value that binds correctly for one core fails for another: at `deep=8` the chip
counter finishes depth 8 in 131 ms of its 150 ms allowance and still stops on the
depth cap.

Safe to do. `src/ranking.cpp` lines 244 to 259 confirm `deep=`, `tt`, `ord`,
`margin=`, `qs`, `nodes=`, and `time=` are ID parameters and not module versions,
so raising the ceiling mints a new agent ID with zero games rather than triggering
another re-identification cascade.

Projected depth reached once the cap is lifted, using x3.41 per ply:

| core | node track (200k) | time track (150ms) |
|---|---|---|
| `classic(chip=100)@2` | completes d7 (~142k), cut in d8 | completes d8 (~131ms), cut in d9 |
| `learned(model=169,...,tdleaf_self,...)@1` | cut during d7 | completes d7 (~86ms), cut in d8 |

Classic gains exactly one ply on the TD-Leaf core on both tracks. **These are
projections from Classic's own branching factor.** A different evaluator changes
move-ordering quality and therefore its own factor, so each core's realized depth
is a Pass-1 measurement, never an assumption.

## P2. Fix `time=` budget enforcement (BLOCKING for the time track)

`budgetTripped()` checks the wall-clock deadline once per 4096 nodes
(`(nodes & 4095ULL) == 0`) and the iterative-deepening loop has no check before
starting a new depth iteration. Today that mis-caps only two slow cores
(`model=111`, `model=113`, measured 284 to 439 ms/move against 150 ms).

**Lifting the depth cap escalates this to every agent on the time track.** Under
`deep=12,time=150ms`, Classic completes depth 8 at 131 ms, sees 19 ms left, starts
depth 9, and runs roughly 448 ms before the coarse check catches it. That is about
580 ms against a 150 ms budget, a 3.8x overshoot on the cheapest core in the field.

`todo.md` scopes this as a one-line pre-iteration check. **That is not sufficient.**
A plain "is the deadline past" test does not fire at 131 ms. The fix needs:

- a predictive check that estimates the next iteration's cost from the measured
  per-ply growth factor and declines to start it when it cannot fit, and
- finer in-recursion granularity than 4096 nodes, sized against the measured
  `Clock::now()` overhead rather than guessed.

Validate with a two-setting control per `CLAUDE.md`'s instrument rule: run the same
core at two `time=` values and confirm realized ms/move tracks the flag.

## P3. Instrument node and leaf counters in `gaz` (BLOCKING for MCTS on the node track)

`src/ai_gumbel.cpp` never writes `g_lastNodes`, `g_lastLeafs`, `g_lastEffDepth`, or
`g_lastBudgetKind`. `src/ranking.cpp`'s `playOneGame` only credits `wnod`, `bnod`,
`wed`, `bed` when `ag.spec.brain==BRAIN_SEARCH && g_lastNodes>1` (line 2727), so
that gate never fires for a Gumbel move. Every `nodes_per_move` cell in every
round-6 export is literally 0. **Node cost is unmeasurable for `gaz` today**, so
the node track cannot be calibrated for it. Wall clock already works, since it is
timed around every move regardless of explorer.

Fix: thread `nodes` and `leafs` through `gumbelSimulate`, increment once per call
and at its four true-leaf return sites, and write the four globals once before
`gumbelSearch` returns. `ranking.cpp` needs no change. This is the counter half of
the design in
`plans/champion-category-restructure-plan-1-golden-painting-anchor.md` lines 176 to
185, not the budget-enforcement half.

## P4. Wall-clock rungs in every trainer

The 2h / 4h / 8h ladder needs a mechanism that does not exist. `--ckpt-at` and
`--ckpt-every` key on game count and epoch, and no trainer has `--resume` or a
wall-clock stop.

**Simplification worth taking: build checkpoint marks and a stop, skip resume.**
One 8h run per seed, writing checkpoints at the 2h and 4h marks, produces all three
rungs. Resume is only needed if the developer later approves doubling to 16h, so
defer it until that decision is actually made.

Add `--wall-ckpt-at "7200,14400,28800"` (seconds) and a hard wall-clock stop to
every regime entry point: `tdleaf`, `selfplay-supervised`, `dist-value`,
`gumbelzero`, and the hill climber. Checkpoint files must carry the ELAPSED
SECONDS in their provenance line, not the requested budget (see P5).

## P5. Fix the `games=` provenance bug (affects every compute claim)

`model->teacher` is written once BEFORE the training loop from `cfg.games`
(`src/ml_tdleaf.cpp:227-240`), and `tools/tdleaf_study.ps1:159` passes the ladder's
LAST rung as `--games`. Every checkpoint from one run therefore carries the same
`games=` value regardless of which rung it actually is.

slot169's header says `games=4000`. It actually trained on **1,500 games**
(`ranking/roster.txt:421` names it as REF's rung-1500 checkpoint). Confirmed twice,
also on slot131 (header `games=2000`, `todo.md` names it 1,000 games). **The
current openless x node champion's training cost is misstated 2.67x in the only
place a reader would look.**

Write provenance AFTER the loop, from what was actually spent. Without this the new
wall-clock rungs inherit the same bug and every rung claims 28800 seconds.

## P6. Training-compute instrumentation

`trainTDLeaf` logs only wins, losses, draws, trained-position count, and mean PV
depth (`src/ml_tdleaf.cpp:362-401`). No node accumulator, no wall clock. The
per-game rates that do exist in `plans/tdleaf-plan-1-amber-pangolin.md` disagree by
15x (0.041 s/game amortized over 500 games, 0.636 s/game in a 20-game process,
1.27 s/game under 12-way contention), so none supports a compute claim.

Add a node accumulator and a wall-clock timer to every trainer, printed in the final
summary and written into checkpoint provenance. Wall clock is the normalization
unit, but nodes are what make the number portable off this machine.

## P7. Refresh `ranking/climb_roster.txt`

Eight of its ten lines are `ab(...)@1` identities, all `gone` after the TT version
bump. A hill climb run today would silently fall back to two live opponents
(`rand@1` and `smart(pieces=6)@1`). Bump to `@2` and rebuild the pool against
current standings, now led by the TD-Leaf cohort rather than the classic-and-book
heavy pool the old climb saw.

## P8 (opportunity, not blocking). Fast tanh in the shared learned leaf tail

The TD-Leaf core costs 2.2x Classic in BOTH nodes and wall clock. It is not the
accumulator: `mlLeafScore` (`src/ml_eval.cpp:243`) does use the incremental scalar
`g_mlAcc` for a linear model. The cost is the shared leaf tail (lines 53 to 57),
`std::tanh(out) * scale` plus round and clamp, plus the
`g_mlSkipW*g_chipDiff + g_mlStmW*(...)` blend. One transcendental per leaf that
Classic's integer tail never pays.

On a wall-clock leaderboard, per-leaf cost is the currency, so this is the single
highest-leverage optimization available. It applies to `pool_games`,
`position_elo`, `tdleaf_self`, and `weight_merge` at once. **If taken it must land
BEFORE the comparison runs, never during**, or half the regimes get a speedup
mid-study.

---

# Part 2: per-regime change lists

Every regime gets 3 seeds minimum (`Docs/model-training-playbook.md`,
configuration rule 1) and the same 2h / 4h / 8h cumulative rung ladder (rule 2).

## R1. Classic chip counter (control, no training)

Its training budget is zero, which is the point of a control.

- New roster IDs at `deep=12` on both tracks. Depends on P1, and on P2 for the time
  track.
- Measure realized nodes/move, ms/move, and eff depth at both, and confirm the
  budget now ends the search: `g_lastBudgetKind` should report `BUDGET_NODE` or
  `BUDGET_TIME`, never `BUDGET_DEPTH`.
- Keep the `deep=6` twins rostered as continuity anchors so the new fit can be
  related to the existing one.
- Open question carried to Part 5: whether the time-track control should also drop
  `tt`, given the 8.02 versus 11.26 ms/move measurement. Recommend rostering both
  and letting the leaderboard answer it.

## R2. Advanced heuristic

- **P7 first.** The climb pool is dead.
- Run `tools/hill_climb.ps1` with `-Head "deep=12,tt,ord,nodes=200k"` and again
  with `-Head "deep=12,tt,ord,time=150ms"`. The `-Head` flag has never been used:
  every recorded climb ran at bare `ab(d4)@1` with no `tt`, no `ord`, and no
  budget, matching neither target head.
- **Climb under the real serving budget.** The promoted Run B vector
  (`adv(chip=77,support=-2,control=1,noiseseed=1,racewin=1)@1`) measures 126,211
  nodes/move at 40.59 ms/move and 0.322 us/node against Classic's 77,257 / 11.81 /
  0.153 on the same head. It pays twice: 2.1x per-node cost AND 1.63x the nodes,
  because a finer-grained evaluator produces fewer beta cutoffs. Compounded, 3.4x
  wall clock. A climb that does not run under the serving budget keeps selecting
  weights that are not worth their cost on the time track.
- Raise `-Games` above its default of 4. Recorded climbs carried SE of 60 to 85,
  which is how an unconfirmed garnish weight (`support=-2`, one sample) reached the
  promoted vector.
- Run both `nonneg` and `signed` modes fresh.
- **Noise caveat.** `Noise` does not draw from `rand()`, so
  `rankAgentIsDeterministic` (`src/ranking.cpp:2220`) still marks a noisy `adv`
  agent deterministic and `pairGameTarget` caps it at 2 games against another
  deterministic agent. Either leave Noise at 0 or fix the determinism predicate,
  otherwise the sample size collapses (`Docs/benchmarking.md`, defect 3).
- Budget mapping: the climb IS the training, so 2h / 4h / 8h buys iterations.
  Record iterations completed at each rung.

## R3. Found-data pool replay

- Re-run `rank.exe extract` against the post-fix store. Its own determinism-drift
  replay check drops irreproducible pre-fix rows automatically.
- `--min-elo` does not exist in `extract`'s signature (`src/ranking.cpp:4198-4256`)
  despite `todo.md` discussing it. Either implement it, or state plainly that
  theory 26 (low-Elo games are low-quality training data) remains untested and this
  arm is unfiltered.
- Re-sweep `lr`, `epochs`, and `l2`. They are script constants (`lr=0.05`,
  `epochs=6`, `l2=0`, `val-split=0`) never swept once the data source changed.
  `Docs/hyperparameter-log.md` lines 39 to 54 self-reports them as unswept.
- Budget mapping: extraction plus fit, both inside the wall clock.

## R4. oracle-vs-champ pairgen

- Data is clean, but was generated against a champion that no longer holds any
  title. Regenerate against **each track's current champion** per
  `ranking/CHAMPION.md`, which means two datasets, not one.
- The oracle generator `ab(deep=8,tt,ord,nodes=2m)@2.classic(chip=100)@2` costs
  131.37 ms/move against the champdil generator's 11.81. **Under a wall-clock
  budget this arm buys roughly 11x fewer games than R5 for the same spend.** That
  is not a flaw, it is the honest tradeoff the budget exists to expose, and it
  should be reported as a headline rather than buried.
- Never had a Pass 4. Three seeds at the final head, which no checkpoint in this
  family has ever had.

## R5. champdil-vs-champ pairgen

- Data is clean. Regenerate against each track's current champion.
- **Retrain on asymmetric-opener data (`--open-side a`).** The headline that
  champdil is the best training data is opener-inflated: its 62.5% score against the
  champion collapsed to 40% once the champion played its own opening instead of a
  forced random one (`plans/opener-bias-results-1-synchronous-stearns.md`, n=80).
  The oracle arm's edge survived the same test (58.8% to 66.2%). Comparing R4 and R5
  without this correction repeats a known artifact.
- Same lr, epochs, l2 re-sweep as R3, same three-seed requirement.

## R6. position_elo (position oracle)

- **Discard the existing label store entirely.** Not a partial salvage. 100% of
  labels are suspect and the salvageable game subset loses every strong rung.
- Rebuild the ladder with post-fix `@2` agents, then run
  `posgen` -> `label` -> `labelfit` -> `dist-value` inside the wall-clock budget.
- **Size the campaign from the measured saturation curve**, the one thing from the
  old campaign that survives: the linear head saturates between 425 and 1,707
  positions, while the MLP head was still climbing at the full 22,788 (Spearman
  0.65 to 0.74 in the last stretch). The linear head plausibly fits a 2h rung. The
  MLP head plausibly does not, and that is a finding rather than a failure.
- **Recommend dropping the sigma head for this round.** It is weakly supported
  (Pearson 0.12 to 0.29), its only proposed search-time use was refuted (Risk
  weight, theory 47, every nonzero k rated 66 to 228 Elo below the mu-only
  baseline), and it consumes training compute the mu head could use. Flagged for the
  developer rather than decided here, since it changes what the regime is.
- Wide-MLP variants (the `mu_shape=129-512-8-1` family, slots 110 to 115) run 2 to
  2.9x over the time budget. Exclude them from time-track claims, or fix P2 and
  re-measure. Slots 110, 112, 114, 115 share that architecture and cost but lack the
  `# cost flag` comment that 111 and 113 carry.
- The plan-versus-production `lr` disagreement (plan said 0.02, production used
  0.01) is settled in favour of production, per the hyperparameter sweep.

## R7. TD-Leaf(lambda)

- **P5 first**, or every wall-clock rung inherits the same provenance bug.
- Add `time=150ms` roster lines. This needs **no retraining**: every other learned
  regime already serves the same slot file at both heads, so it is a roster line
  plus a replay.
- Note the budget mismatch honestly. TD-Leaf trains at `nb200000` and would then be
  served under a clock. This was never considered, not because it was examined and
  dismissed, but because the time category postdates the study by three weeks. Under
  this plan, train a matched arm at `--time-budget-ms 150` so each track has a regime
  member trained under its own budget.
- **Decide the self-play contamination question rather than assuming.** Both sides
  carry `tt`, but they are the same agent with the same evaluator and params, so a
  cross-read returns the same value function at the same depth. That is closer to
  sharing a cache with yourself than to contamination. Cheapest resolution: train one
  short arm with `tt` disabled and compare the resulting model's Elo to a matched
  `tt` arm. Under this plan every regime retrains anyway, so this only affects
  whether the OLD checkpoints stay rostered as reference.
- **The wall-clock ladder must save all rungs, because this regime is known to
  decline past its peak.** Pass 1 (ladder 100/250/500/1000/2000) found a peak at
  1,000 games. Pass 2 (ladder 250/500/1000/1500/2500/4000) relocated it to **1,500**
  at Elo 1030, declining to 955 by 4,000. Pass 1 never sampled 1,500. An 8h rung may
  sit past the peak, which is informative only if the 2h and 4h rungs are rated too.
- Run `--batch`. It is the one axis both passes skipped entirely.
- **Correct `Docs/hyperparameter-log.md`.** Its tdleaf table cites only `results-1`
  and marks lambda, lr, l2, explore, open-plies, model-type, and feature-version as
  untested or n=1. `results-2` swept all of them, including a 154-Elo model-type
  effect, the largest single axis in the study. Two `todo.md` items are also
  mismarked: the MLP arm is done (5 MLP checkpoints are rostered) and the game-count
  bracket is partially done.

## R8. Gumbel-Zero MCTS

- **P3 first.** Without node counters the node track cannot be calibrated at all.
- Calibrate `sims=` per architecture against each budget. Measured `cpu_ms_move` at
  rung 4000: mlp32 at sims=300 gives 3.04 ms (3-seed mean), mlp32 at sims=500 gives
  5.38, mlp64 at sims=300 gives 5.22, mlp64-32 at sims=500 gives 22.24,
  conv(16,16,16)+fc32+policy-mlp at sims=300 gives ~79, conv(32,32) at sims=300
  gives ~146.
- **The calibrated value spans two orders of magnitude across architectures.**
  Fitting the two mlp32 points gives 0.0117 ms/sim, so 150 ms needs roughly
  **sims = 12,900**, a 25x extrapolation past anything measured, while conv(32,32)
  at sims=300 is already at budget. Treat 12,900 as a starting hypothesis to be
  measured, never a value to roster directly.
- **Train at the calibrated sims.** Every existing checkpoint was trained at its
  serving sims of 300 to 500, so serving mlp32 at ~12,900 would put the policy prior
  far outside its training distribution, and nothing measures what happens there.
  Under the wall-clock budget this resolves itself: train fresh at the calibrated
  sims inside the budget rather than reusing a checkpoint trained elsewhere.
- Average-matching is more defensible here than for alpha-beta.
  `gumbelHalvingRounds` fixes total simulations regardless of position
  (`ai_gumbel.cpp` 300 to 301) except for an immediate root win and a forced single
  move, so per-move cost is near-constant rather than varying with pruning. That is
  a code-structure argument, and a realized-variance measurement should confirm it.
- No `gaz` checkpoint has ever had an unpinned full-roster refit. All five roster
  lines are `off` with zero games in the main store, so every existing number is
  screening-scale and not comparable to `ranking/standings.tsv`.

---

# Part 3: deferred regimes (appendix, per developer instruction)

Recorded so they are not lost, deliberately not planned in detail.

**`weight_merge` (ensemble and mirror).** `train.exe ensemble --models --mirror`.
Theory 30 refuted it for playing strength: mirroring the champion's own weights cost
135 Elo, and the 6-seed mirror ensemble 144 against the seed mean. **Yet slot10,
which is `ensemble(k=1,mirror=1):models/sweep/slot98.txt` and therefore a pure
mirror of the oracle model with no ensembling at all, currently tops both dil20
categories (682 node, 696 time) and is second in both opener8 categories.** That
contradiction is unexplained and is the interesting thing here. Theory 32 proposes
the resolving experiment (five mirror modes: off, average, flip, left-onto-both,
right-onto-both). Better treated as a post-processing step applied to every regime's
winner than as a regime of its own.

**From-scratch `selfplay-supervised`.** The original regime: self-play games labeled
by outcome, no teacher, no bootstrap. No representative on the current roster, so it
would need training from zero. It is the natural floor that `tdleaf_self` and
`position_elo` should have to clear, and its absence is why "bootstrapping helps"
has never been measured against a same-budget non-bootstrap control.
`Docs/corrections.md`'s `SELF-PLAY CONVERGENCE UNSUPPORTED` applies to its old
convergence stop, which fired on noise.

**`imitate` (policy-only).** Behavioral cloning of a teacher's move choice into a
`LearnedPolicy` move-rater, played with no lookahead. Its single roster line
(`ranking/roster.txt:89`) is commented out, so nothing live uses it. As a standalone
chooser it would sit at the bottom of both leaderboards. The more promising use is
as a move-ORDERING policy inside alpha-beta, where `todo.md` records that the PST
prunes 3x worse than Classic. That is a different experiment than this round runs.

---

# Part 4: comparison protocol

1. **Prerequisites P1 through P7 land and are validated** before any regime runs.
   P8 either lands first or is deferred entirely, never mid-study.
2. **Pass 1 (sanity) per regime.** Confirm the trainer stops on wall clock, writes
   all three rungs, and records elapsed seconds truthfully. Confirm each rung's model
   loads through the real search path. Confirm the budget, not the depth cap, ends
   the search at serving time.
3. **Pass 2 (calibration).** Measure each core's realized nodes/move, ms/move, and
   per-ply growth factor at `deep=12` under both budgets. The x3.41 figure is
   Classic's and must not be assumed for anyone else. This is also where each `gaz`
   architecture's `sims=` is fixed.
4. **Pass 3 (the run).** 8 regimes x 3 seeds, one 8h run per seed, checkpoints at 2h
   and 4h. Identical rung list for every regime, no exceptions
   (`Docs/model-training-playbook.md`, configuration rule 2).
5. **Screening between rungs** uses Workflow A (`play --cohort` plus
   `rate --pin ranking/standings.tsv`), which cannot disturb the existing scale.
6. **Certification** uses Workflow B, an unpinned full-roster refit at >= 32
   games/pair. Nothing is promoted or called a winner on a pinned fit.
7. **Report both tracks separately, never pooled.** State the head on every table,
   give full canonical IDs, and report distinct-game counts alongside stored-row
   counts (`Docs/benchmarking.md`, defect 3), since openless contenders are mutually
   deterministic and capped at 2 direct games per pair.
8. **Report training compute as a column** next to Elo on every row: wall clock
   spent, nodes consumed, games generated. Wall clock is equalized by construction,
   so the other two columns are what make the result portable to another machine.

# Part 5: open questions for the developer

Answered by the developer on 2026-09-01:

1. **Does the sigma head survive in position_elo?** Dropping it buys mu-head compute
   under a tight budget. It is weakly supported and its only proposed use was
   refuted, but removing it changes what the regime is.
   **Answer: DROP.**
2. **Should the time-track control drop `tt`?** The measurement says `tt` costs wall
   clock (8.02 versus 11.26 ms/move). Rostering both answers it empirically and costs
   one extra agent.
   **Answer: KEEP `tt` ONLY** (roster only the `tt` variant for the time= track).
3. **Is P8 (fast tanh) in or out?** It is the largest single wall-clock win available
   to four regimes at once, and it must land before the study or not at all.
   **Answer: IN.** Implement before any Part 2 comparison runs, never mid-study.
4. **What happens if a regime is still improving at 8h?** The stated rule is to
   confirm before doubling. Worth deciding in advance whether a 16h rung runs for
   that regime alone (breaking rung parity) or for all of them (preserving it at 2x
   the cost).
   **Answer: 16h for that regime alone.** Rung parity breaks for the affected
   regime; the others stop at 8h. Resume support is therefore in scope for P4,
   not just checkpoint marks and a hard stop.
5. **Do the old contaminated-era checkpoints stay rostered** as frozen historical
   reference, or come off entirely? They cannot be compared to the new cohort, but
   removing them changes the fit population again.
   **Answer: KEEP fully active,** not marked frozen or retired. They are valid
   agents, likely weaker, but comparable like any other roster entry rather than
   given retired-agent semantics.
