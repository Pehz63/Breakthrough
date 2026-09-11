# Replication study: published training techniques on Breakthrough, one controlled pipeline

**Status: PLAN ONLY. Nothing here has been run.** Written 2026-09-10. The
companion results doc is `plans/replication-study-results-1-brass-lectern.md`,
created when Pass 0 starts. The research goal this serves is recorded in
`Docs/Memories/breakthrough-replication-study-goal.md` and `CLAUDE.md`'s
"Research goal" section.

## What this study is for, and how it differs from the budget-parity rebuild

`plans/budget-parity-plan-1-steady-meridian.md` asks a leaderboard question:
given equal training compute, which REGIME produces the strongest agent? Each
regime gets its own best shot, tuned separately, with its own data source and
code path. The answer is an ordering of finished agents.

This study asks a science question: **what is the causal effect of each
published technique on learning, and do the effects add?** That needs a
different design.

| | Budget-parity rebuild | Replication study |
|---|---|---|
| Unit compared | a whole regime (its code path, data, tuning) | one technique, switched on or off inside ONE shared pipeline |
| Everything else | differs between regimes by design | held identical, down to the binary and the seed list |
| Tuning | each regime tuned for its best shot | an equal tuning budget per technique, by protocol |
| Result | an Elo ordering, category titles | effect sizes with confidence intervals, interaction estimates, replication verdicts against named published numbers |
| Seeds | 3 per regime | set by a power analysis from measured noise |
| Evaluation | the full-roster refit | a frozen reference panel, so every estimate is independent and on one fixed scale, plus certification of the winner |
| Pre-registration | stopping rule only | hypotheses, primary endpoint, analysis model, correction, equivalence margins |

What carries over from budget-parity unchanged: its Part 1 fixes (wall-clock
rungs and resume in `src/train_budget.cpp`, truthful provenance, `time=`
enforcement, training-compute instrumentation), the Round 4 study head, the
`retain` default (theory 70), and the TD-Leaf PV-walk fix (theory 72).
Budget-parity Part 2's pending lambda sweep folds into this study's Pass 2
calibration, since the same numbers serve both. Budget-parity itself continues
separately: it answers the leaderboard question and this study does not.

## Research questions

- **RQ1 (transfer).** Which published techniques improve self-play learning on
  Breakthrough, and by how much, at matched training compute?
- **RQ2 (efficiency kind).** Does each technique make learning faster (the same
  strength for less compute) or raise the strength it eventually reaches, or
  both? Sample efficiency (per game) and compute efficiency (per CPU second) are
  reported separately, because techniques like TreeStrap buy more updates per
  game at a higher cost per game.
- **RQ3 (additivity).** When two techniques are combined, is the gain the sum
  of their separate gains, less than the sum, or more?
- **RQ4 (direct replication).** Do Cohen-Solal 2026's Breakthrough results
  (the only published controlled comparison on this game) reproduce under an
  independent implementation and a stronger evaluation protocol?

## Published claims under test

Each claim gets a card in the results doc: source, original domain, original
protocol, original numbers, our adaptation, every deviation, and the verdict.
"Verified" means read from the paper's own text this session. Everything else
must be read from the source before the pre-registration is committed.

| ID | Technique | Source | Domain | Published result | Replication type | Verified |
|---|---|---|---|---|---|---|
| C1 | TreeStrap(alpha-beta) vs TreeStrap(minimax) vs RootStrap(alpha-beta) vs TD-Leaf | Veness, Silver, Blair, Uther, NIPS 2009 | chess, linear evaluator, 1812 features, self-play from random weights | Best Elo (95% CI): TreeStrap(alpha-beta) 2157 +/- 31, TreeStrap(minimax) 1807 +/- 32, RootStrap(alpha-beta) 1362 +/- 59, TD-Leaf 1068 +/- 36, untrained 250 +/- 63. One training run per method. Games at fixed time control, update time charged against thinking time. Step size tuned per method | conceptual (new game) | yes, Table 2 |
| C2 | TD-Leaf(lambda) vs TD(lambda) vs TD-directed(lambda) | Baxter, Tridgell, Weaver 1999 (arXiv cs/9901001) | chess (KnightCap), backgammon | KnightCap 1650 -> 2100 in 308 games on FICS. Veness 2009 notes those weights were initialised from expert values and that random-init TD-Leaf stalled at weak amateur level | conceptual | abstract only |
| C3 | tree learning vs root learning vs terminal learning | Cohen-Solal, JMLR 27 (2026), Table 2 | 11 games incl. Breakthrough, conv net | Breakthrough, iterative-deepening alpha-beta: tree 79.2%, root 50.6%, terminal 14.3% (MCTS: 78.1 / 45.5 / 22.3) | **direct** (same game) | yes |
| C4 | additive depth reward (win fast, lose slow) vs plain win/loss | Cohen-Solal 2026, Table 4, Section 6.2.2 | same | Breakthrough: additive depth 69.5%, win/loss 39.0% (multiplicative depth 40.4, mobility 43.9, presence 48.5). Terminal value l = P - p + 1 for a win, -l for a loss, P = maximum actions in a game, p = actions played | **direct** | yes |
| C5 | ordinal action distribution vs epsilon-greedy | Cohen-Solal 2026, Section 7, Remark 16 | many games | "performs better on average and on the majority of games... the gain is however quite slight". No numbers shown in the paper | conceptual, **exploratory only** (no published effect size to test) | yes |
| C6 | symmetry data augmentation | AlphaGo / AlphaGo Zero line (Silver et al.) | Go | to be read before pre-registration | conceptual | no |
| C7 | TD(lambda < 1) vs Monte Carlo (lambda = 1) targets | Sutton 1988, Tesauro (TD-Gammon) | backgammon | to be read before pre-registration. In-project prior: lambda = 1 scored below the untrained init (`Docs/hyperparameter-log.md`) | conceptual | no |

Protocol notes that matter for C3 and C4. Cohen-Solal trained each variant 48
hours per repetition, repeated 32 times (48 for Table 4), and scored a variant
by an all-play-all among the final evaluation functions of every variant and
repetition, with every match played at **minimax depth 1**. So their
percentages measure static-evaluation quality inside a pool of six variants,
not playing strength against outside opponents. This study reproduces their
statistic as a secondary instrument (see "Evaluation") and adds a
serving-depth, fixed-panel rating on top.

## Design

### The shared pipeline (every run, every cell)

One trainer, `train.exe tdleaf`, extended with switches (Pass 0). A technique
is a switch value, never a different code path or binary. Held fixed across the
whole study and recorded in every checkpoint's provenance:

| Controlled variable | Value |
|---|---|
| game, start | `boards/board1.txt` |
| model | linear v2 piece-square, 129 features + bias (`Docs/model-training-playbook.md`, architecture guidance) |
| initialization | scratch, small random weights drawn from the run seed (Veness and Cohen-Solal both learn from scratch) |
| generator search | the study head below, exactly (playbook, generator depth rule) |
| opening diversity | `--open-plies` at one calibrated value |
| exploration schedule | one calibrated schedule (unless the exploration factor is on) |
| update schedule | strictly online, `--batch 1` |
| lambda (TD levels only) | one calibrated value |
| l2 | 0.0 |
| learning rate | tuned per technique level with an equal budget (see "Hyperparameter fairness") |
| binary | one commit, hash stamped into provenance, no rebuild mid-study |
| machine, threading | this machine, one thread per training process, a fixed number of concurrent processes |

### Factors (Block A, the confirmatory factorial)

| Factor | Levels | Reference level | Claims |
|---|---|---|---|
| F1 backup target | TD-Root, TD-Leaf, RootStrap(alpha-beta), TreeStrap(alpha-beta) | TD-Leaf | C1, C2, C3 |
| F2 terminal reward | win/loss, additive depth | win/loss | C4 |
| F3 symmetry augmentation | off, left-right mirror | off | C6 |
| F4 exploration distribution | epsilon-greedy, ordinal | epsilon-greedy | C5 |

Full factorial: 4 x 2 x 2 x 2 = **32 cells**. TD-Leaf is the reference level
because it is the regime this project already runs and validates, and C1 and
C2 both report against it. The reference level only changes which contrasts
print by default. It does not change any estimate.

The C7 contrast (TD vs Monte Carlo) sits inside the calibration sweep of lambda
rather than in the factorial, since lambda is continuous and lambda = 1 is one
end of it.

**Why a full factorial rather than "baseline, each alone, then combinations".**
That one-at-a-time layout is contained in the factorial: the all-reference cell
is the baseline, the cells with one factor switched are "each alone", and the
rest are the combinations. What the factorial adds is that every run informs
every effect. In a balanced two-level factorial with N runs, each main effect
and each two-way interaction is a difference of two means over N/2 runs each,
so its standard error is 2 sigma / sqrt(N). A one-at-a-time design spends the
same runs estimating each effect from only two cells. The factorial also
estimates interactions, which the one-at-a-time layout cannot do at all.

### Block B: techniques that change the generator or need a teacher

Run after Block A, each against a control arm drawn from Block A's pipeline at
the identical compute budget. They cannot be factors in Block A without
changing the search or the data source for every cell.

| ID | Technique | Source | Why it is separate |
|---|---|---|---|
| B1 | Descent as the data generator | Cohen-Solal 2026, Table 3 (Breakthrough: Descent 86.5%, unbounded best-first minimax 72.5%, alpha-beta tree learning 47.6%) | a new search algorithm, not a switch |
| B2 | Gumbel AlphaZero | Danihelka et al. 2022, existing `gumbelzero` | a different search family, no node budget grammar (`ranking/tracks.txt`) |
| B3 | supervised learning from strong-agent games | Lorentz and Zosa 2017 (expert-move CNNs), existing `pool_games` path | needs a teacher, whose compute is charged to the budget |
| B4 | search-score distillation | NNUE training practice | needs a deep-search teacher, charged to the budget |
| B5 | experience replay | Lin 1992, Mnih et al. 2015 | promotable to a Block A factor if Pass 2's power analysis leaves room |

### Block C: deferred

Architecture scaling (Jones 2021, Hex), KataGo-style auxiliary targets (needs
multi-head models), potential-based reward shaping, implicit minimax backups
(Lanctot et al. 2014, needs MCTS with an evaluator). Recorded so they are not
lost.

## Outcome measures

The developer's question: converged Elo, time to reach some Elo, or Elo after
the same training time? **All three come off one learning curve, so every run
is rated at every rung, and the pre-registration names which one is primary.**

- **Primary: Elo at the final rung, T_max, at matched training compute.** It is
  defined for every run, it answers the practitioner's question ("I have this
  much compute, which technique?"), and it needs no assumption about
  convergence.
- **S1, area under the learning curve** on a log2 compute axis, the mean Elo
  over rungs. Robust to curves that peak and decline.
- **S2, compute multiplier.** The compute a cell needs to reach the reference
  cell's T_max Elo, log-interpolated between rungs, divided by T_max. A value of
  0.25 means the technique reaches baseline strength on a quarter of the
  compute. Censored as "> 1" when the cell never gets there. This is the
  time-to-threshold measure, stated in a unit that does not depend on this
  machine.
- **S3, peak Elo, corrected for selection.** Veness reports "best performance",
  which is the maximum over checkpoints and is biased upward by the noise of
  the checkpoint that wins. Correction: split each agent's panel games by game
  index parity, pick the peak rung on one half, report its Elo from the other.
- **S4, plateau Elo, only where a plateau is shown.** Reported only for runs
  whose last two rungs differ by less than one combined SE. Otherwise the run
  is reported as not converged.
- **Sample efficiency.** Elo at matched GAME counts, secondary, since TreeStrap
  and augmentation change the cost of a game.

Why converged Elo is not primary: self-play value learning in this project has
an interior optimum more often than not. TD-Leaf peaked at 1,000 to 1,500 games
and declined (theory 46), and 93 of 101 Gumbel-Zero arms were non-monotonic
(theory 50). A convergence point inside a finite budget cannot be verified, so
a primary endpoint built on it would rest on an assumption the data contradicts.

## Interaction model: do techniques add?

The developer's example: A gives +100, B gives +150. Additive predicts +250 for
both. The measured quantity is the interaction

```
I_AB = E[AB] - E[A] - E[B] + E[baseline]
```

so if both together give +200, I_AB = -50 (sub-additive). The analysis fits,
on the primary endpoint,

```
Elo_run = mu + sum_i beta_i x_i + sum_{i<j} beta_ij x_i x_j + e_run
```

with effect-coded factors, reports every beta_ij with its 95% CI, and tests
the main-effects-only model against the two-way model. Three-way terms are
estimated and reported as exploratory.

**The scale additivity is judged on changes the answer, so two scales are
reported, with Elo primary.**

1. **Elo** is log-odds of winning against the panel. Additivity there means
   the two techniques multiply the odds independently.
2. **Log compute.** If A is worth a compute multiplier of 0.5 and B one of
   0.33, independence predicts 0.17 for both. This is the scale on which "two
   speedups" should compose.

Expect some sub-additivity in Elo from a ceiling alone: a 130-parameter linear
evaluator has a representational ceiling (theory 10), and gains shrink near it
whatever the mechanism. The compute scale separates that from a genuine
interaction. So does the learning-curve decomposition from RQ2: a curve model
with a per-cell shift along log compute (speed) and a per-cell asymptote
(ceiling) says which kind each technique is. Two speed techniques should
compose on the compute scale. Two ceiling techniques compete for the same
headroom.

**Out-of-sample additivity check.** Fit the main-effects-only model on the
reference cell and the single-switch cells alone, predict every combination
cell, and report predicted against measured, cell by cell.

## Power and sample size

Every run's endpoint carries two noise sources: seed-to-seed training variance
sigma_seed and rating error sigma_meas, so sigma^2 = sigma_seed^2 +
sigma_meas^2. Both are measured in Pass 2. The seed-noise band (50 to 150 Elo
between replicas, theory 8) is a range, not a standard deviation, so it only
brackets the guess below.

Standard error of a two-level main effect or two-way interaction, balanced
factorial, N runs total: 2 sigma / sqrt(N).

| sigma | N = 32 | N = 64 | N = 96 | N = 128 |
|---|---|---|---|---|
| 50 | 17.7 | 12.5 | 10.2 | 8.8 |
| 75 | 26.5 | 18.8 | 15.3 | 13.3 |
| 100 | 35.4 | 25.0 | 20.4 | 17.7 |

F1 has four levels, so a pairwise contrast between two of them uses N/4 runs
per level: SE = sigma sqrt(8 / N). At sigma = 75 and N = 96 that is 21.7.

Minimum detectable effect at 80% power, two-sided alpha 0.05, is about 2.8 SE.
At sigma = 75 and 3 seeds per cell (N = 96), that is 43 Elo for a two-level
effect or interaction and 61 Elo for an F1 pairwise contrast. Veness's C1 gaps
are several hundred Elo, so the F1 ordering is well inside reach. Interactions
smaller than about 40 Elo would need more seeds.

Rating error per agent against a panel, G games at mean p(1-p) = 0.2:
SE = 173.7 / sqrt(0.2 G), which is 19.4 at G = 400 and 12.3 at G = 1,000. The
rule is to choose G so that sigma_meas <= sigma_seed / 2, which keeps rating
error under 20% of the total variance. Spending more on games past that point
buys less than spending it on seeds.

**Multiple comparisons.** The confirmatory family is fixed in the
pre-registration: the C1 ordering, C3, C4, the C7 lambda contrast, and the
global additivity test. Holm correction across that family. Everything else is
labelled exploratory.

**"Did not transfer" needs an equivalence test, not a failed significance
test.** A technique is reported as having no effect only if its 90% CI lies
inside a pre-registered margin (two one-sided tests, margin proposed at +/- 30
Elo). A CI that is wide and straddles zero is reported as inconclusive.

## Evaluation instrument

**Study head: `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3`**, the Round 4
node-track head. Three reasons:

- A node budget measures strength per node, which isolates evaluator quality,
  and evaluator quality is what a training technique changes.
- Every Block A model is linear v2 with identical per-node cost, so the node
  and time tracks sit at the same operating point by construction (0.25
  us/node, `ranking/tracks.txt`). Each agent's `cpu_ms_move` is still recorded
  to confirm it.
- Node-budget agents are deterministic (theory 71). `time=` agents are not
  (theories 59 and 71), which would put machine load into every game.

**Frozen reference panel.** A fixed set of opponents spanning the whole range
the learning curves cross, from untrained scratch models and weak fixed agents
(`rand@1`, `greedy@1`, shallow `ab` heads) through the Classic chip counter to
the current top Round 4 cores. Panel ratings are pinned from the converged
Round 4 full-roster fit. Each study agent plays only the panel, into a study
store separate from `ranking/matches.jsonl`, and is rated with
`rank.exe rate --pin`. Consequences: every study agent's Elo is an independent
estimate on one fixed scale, study agents never shift each other, and the
pool-compression hazard of comparing across fits does not arise. Panel size and
spread are fixed in Pass 2 so that low rungs are resolved as well as high ones.

**Game independence.** Deterministic agents against deterministic opponents
replay one game per colour (`Docs/benchmarking.md`, defect 3). Games therefore
start from paired random openings, each opening played twice with colours
swapped (`rank.exe play --paired-openings`). If the same opening sequence is
drawn for every study agent against a given panel member, differences between
study agents also benefit from common random numbers. Pass 1 verifies both
properties by reading back stored games. The distinct-trajectory count is
reported next to every rating.

**Secondary instruments.**

1. **Cohen-Solal's statistic (RQ4).** An all-play-all among the final
   checkpoints of the cells matching their contrasts, every agent at
   `ab(deep=1)@3` (their matches are depth 1), scored as they score it. Their
   percentages come from their own six-variant pools, so the comparison is the
   ordering and the direction of each gap, not the absolute percentage.
2. **Transitivity check.** A head-to-head matrix among the best final agent of
   each F1 level, compared against panel Elo. Techniques that learn different
   styles can order differently head to head than against a panel.
3. **Certification.** The best Block A cell and the reference cell enter the
   unpinned full-roster refit under `ranking/CHAMPION.md`'s rules. This is the
   only step that can make a title claim, and the paper's strength claim for
   the best recipe comes from it.

## Training compute accounting

- **Primary unit: process CPU seconds**, one thread per process, measured with
  `GetProcessTimes`. `src/train_budget.cpp` measures wall clock
  (`steady_clock`) today, and budget-parity P6 recorded per-game rates
  differing 15x under contention, so wall clock alone cannot carry a compute
  claim. Wall seconds, nodes searched, games, positions trained and gradient
  updates are all logged beside it.
- **Update cost is charged to the budget**, as Veness did. TreeStrap's extra
  updates are part of its price.
- **Rungs are geometric in compute**, a doubling ladder from T_0 to T_max,
  sized in Pass 2 from where curves flatten (playbook, Pass 2).
- **Reproducibility.** A run that stops on a clock produces a checkpoint that
  depends on machine speed. The recommended alternative, open for the
  developer: Pass 2 measures each cell's CPU seconds per game, and Pass 3
  expresses every rung as a game count equal to the compute target divided by
  that cost. Every checkpoint is then a pure function of (commit, config,
  seed), realized CPU seconds are still logged, and the analysis uses realized
  compute on the x axis. This is the method theory 48's compute-matched Gumbel
  sweep used.

## Hyperparameter fairness

Unequal tuning is the standard confound in method comparisons (Henderson et
al. 2018). Veness tuned the step size per method, and so does this study, under
a fixed protocol:

- For every F1 level, and for F2 and F3 (they change target magnitude and the
  number of updates), the learning rate gets the **same random-search budget**:
  the same number of draws over the same log range, the same seeds, the other
  factors at their reference levels (Bergstra and Bengio 2012).
- The tuned rate is fixed for Pass 3. Nothing else is tuned per cell.
- Sensitivity is reported: each technique's effect at its own tuned rate and at
  the reference level's rate. A technique whose gain disappears at a shared
  rate is reported that way.
- Lambda is calibrated once for the TD levels, over [0, 0.7] plus the
  lambda = 1 end for C7. RootStrap and TreeStrap have no lambda.

## Pass 0: what has to be built

Each item lands with its test before any Pass 1 run.

| Item | What | Verification |
|---|---|---|
| I1 | `--backup td-root\|td-leaf\|rootstrap\|treestrap` in `trainTDLeaf`. TD-Root takes the gradient at the root's static eval. RootStrap moves the root's static eval toward the root search value. TreeStrap(alpha-beta) moves every searched node at depth >= d_min toward its search bound, only when the static eval violates the bound (Veness, Section 4) | closed forms: TreeStrap restricted to the root equals RootStrap, TD-Root at lambda = 1 equals Monte Carlo on root positions. The TreeStrap node collector must not touch the rated search path: with collection on, the search returns the identical move, score and node count on a fixed position set, and rated us/node is unchanged (the reason `ml_tdleaf.h` gives for never modifying the hot recursion) |
| I2 | `--terminal winloss\|depth`. Cohen-Solal's l = P - p + 1 mapped into the model's win-probability target, z = 0.5 + 0.5 l / P for a White win and the mirror for a loss | with l held at P for every terminal position, the depth targets equal the win/loss targets exactly. P is fixed in Pass 1 as the proven maximum game length if `Docs/axioms.md` can supply one, otherwise the engine cap. The bounded mapping is a recorded deviation, since Cohen-Solal's output is unbounded |
| I3 | `--augment mirror`. Each trained position is also trained as its left-right mirror, reusing `mlv2MirrorIndex` from `trainEnsemble` | a mirror-symmetric position yields the identical feature vector, updates and positions trained double, both logged |
| I4 | `--explore-dist eps\|ordinal`, Cohen-Solal's ordinal distribution (their Algorithm 14, parameter range from the paper) | the empirical move-rank distribution over many draws matches the closed form |
| I5 | CPU-seconds accounting in `train_budget`, written into provenance | two settings of a busy-loop load give the same CPU seconds and different wall seconds |
| I6 | panel file, study store, analysis export | paired openings verified from stored games, study store disjoint from the main store |
| I7 | `analysis/replication_factorial.py`: factorial fit, bootstrap CIs over runs, compute multiplier, AULC, split-half peak, two one-sided tests, predicted-vs-measured additivity table | run on synthetic data with known effects, which it must recover |

## Passes

The playbook's four passes, mapped onto this study.

**Pass 1, sanity.** Every switch at two settings, output differs as claimed.
Each cell trains, stops at its rung, writes truthful provenance, and loads
through the real search path. Mean PV depth is well above 1 on every TD-Leaf
cell (theory 72's guard). Same seed twice gives a byte-identical model. Panel
games are independent. Report to the developer before Pass 2.

**Pass 2, calibration.** Four outputs, all required before the grid is shown:

1. Curve shape: the reference cell, each single-switch cell and the all-on cell
   at 1 seed, on a generous ladder, to place T_max past where curves flatten.
2. Noise: 5 seeds of the reference cell and of one contrasting cell at the
   candidate T_max, giving sigma_seed. Panel fits give sigma_meas. The power
   analysis above then fixes seeds per cell and G.
3. Tuning: the equal-budget learning-rate search per level, and lambda.
4. Cost: CPU seconds per game per cell, which sets the game-count rungs.

Then the grid goes to the developer: cells, seeds, rungs, agent count, total
games implied, and one projection from measured throughput if it exceeds a
day.

**Pass 3, the confirmatory factorial.** Run exactly the pre-registered grid.
This deliberately departs from the playbook's "1 seed per draw" rule for Pass
3: that rule exists for exploratory random search analysed by a fitted model.
This pass is a designed confirmatory experiment, and its replication comes from
seeds within cells. Report the shape of the result to the developer before
Pass 4.

**Pass 4, validation.** Two parts:

1. **Winner's-curse control.** The best cell and the reference cell are trained
   again on fresh seeds (at least 5 each) never used for selection, with the
   ladder extended to confirm the plateau. The paper quotes these numbers for
   the best recipe, not the Pass 3 numbers that selected it.
2. **Certification** of the best recipe in the unpinned full-roster refit.

Block B then runs as its own cycle, each technique against a Block A control
arm at identical compute.

## Pre-registration

Before any Pass 3 game, commit
`plans/replication-study-prereg-1-brass-lectern.md` containing: the directional
hypotheses from the claim cards, the primary endpoint, the analysis model, alpha
and the Holm family, the equivalence margin, the rung list and T_max, seeds and
G, exclusion rules (a crashed run is rerun on the same seed and reported), and
the code commit hash. Its commit date and hash are the evidence that the
analysis was fixed before the data existed. Every later deviation is listed in
the results doc with its reason.

## Data collected

| Level | Recorded |
|---|---|
| training run | full config, seed, commit hash, CPU seconds, wall seconds, nodes searched, games, positions trained, gradient updates, mean PV depth (TD-Leaf), mean absolute TD error, weight norm, per-rung checkpoint files |
| rated agent | canonical ID, rung, compute at the rung, panel Elo and SE, games and distinct trajectories, `cpu_ms_move`, realized nodes per move |
| game | both IDs, opening sequence and its pair index, colour, result, plies, per-side nodes and ms |
| study | panel membership and pinned ratings, the Round 4 fit they came from, the pre-registration hash |

## What the paper reports

- Levels first: Elo per cell per rung with per-seed points, and learning curves
  with 95% bootstrap intervals over seeds.
- Then main effects, interactions with CIs, compute multipliers, equivalence
  verdicts, and the predicted-against-measured additivity table.
- Each claim card side by side with the published numbers. C1 is compared on
  ordering and on the ratio of gains over the untrained agent, never on
  absolute Elo, since the scales belong to different pools and games. From
  Veness Table 2, the gains over untrained are TreeStrap(alpha-beta) 1907,
  TreeStrap(minimax) 1557, RootStrap(alpha-beta) 1112, TD-Leaf 818, a
  TreeStrap-to-TD-Leaf ratio of 2.33.
- A reporting style that follows Agarwal et al. 2021: interval estimates over
  runs rather than point estimates, and the distribution of per-seed results
  rather than a mean alone.
- Artifact release: code at the study commit, every config and seed, every
  checkpoint, the study match store, the analysis scripts, and a compute
  statement (hardware, total CPU hours for training and for evaluation).

## Threats to validity

- **One game.** Transfer to other games is not claimed.
- **Linear evaluator.** Cohen-Solal used conv nets, Veness a 1812-feature
  linear model. A technique that needs capacity (tree learning on a deep net)
  may underperform here for a reason unrelated to Breakthrough.
- **Implementation fidelity.** Each adaptation is a reimplementation from the
  paper's text. Every deviation is listed on its card.
- **Panel dependence.** Elo against a fixed panel can differ from head-to-head
  strength. The transitivity check measures how much.
- **Single machine.** CPU seconds tie compute to this hardware. Nodes and games
  are logged so the result can be restated elsewhere.
- **Tuning.** An equal budget is fair by protocol, not optimal for every
  technique. The sensitivity table shows how much it matters.

## Dependencies

1. Round 4 converges and `ranking/CHAMPION.md` is rewritten. Panel ratings are
   pinned from that fit. Pass 0 to Pass 2 can proceed before it.
2. Budget-parity Part 1 fixes (landed) and theory 72's PV fix (landed).
3. A contiguous model-slot range claimed and recorded in `src/CLAUDE.md` before
   the first checkpoint is published.

## Decisions taken (developer, 2026-09-10)

1. **Compute matching: calibrated game counts.** Pass 2 measures each cell's
   CPU seconds per game, Pass 3's rungs are game counts, every checkpoint is
   reproducible from (commit, config, seed), and realized CPU seconds are the
   analysis x axis.
2. **Scratch initialization only.** No champion-initialized arm. C2's
   KnightCap result started from expert weights, which is recorded on its card
   as a deviation.
3. **F1 reference level: TD-Leaf.**

## Open questions for the developer

1. Is F4 (ordinal exploration) in the confirmatory factorial, given its source
   publishes no effect size, or moved to exploratory Block B?
