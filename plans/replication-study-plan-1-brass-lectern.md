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
published technique on learning?** That needs a different design.

| | Budget-parity rebuild | Replication study |
|---|---|---|
| Unit compared | a whole regime (its code path, data, tuning) | one technique, switched on inside ONE shared pipeline, everything else identical |
| Everything else | differs between regimes by design | held identical, down to the binary and the seed list |
| Tuning | each regime tuned for its best shot | an equal tuning budget per technique, by protocol |
| Result | an Elo ordering, category titles | effect sizes with confidence intervals, and a replication verdict against each named published number |
| Seeds | 3 per regime | set by a power analysis from measured noise |
| Evaluation | the full-roster refit | a frozen reference panel, so every estimate is independent and on one fixed scale |
| Pre-registration | stopping rule only | hypotheses, primary endpoint, analysis, correction, equivalence margins |

What carries over from budget-parity unchanged: its Part 1 fixes (wall-clock
rungs and resume in `src/train_budget.cpp`, truthful provenance, `time=`
enforcement, training-compute instrumentation), the Round 4 study head, the
`retain` default (theory 70), and the TD-Leaf PV-walk fix (theory 72).
Budget-parity Part 2's pending lambda sweep folds into this study's Pass 2
calibration, since the same numbers serve both. Budget-parity itself continues
separately: it answers the leaderboard question and this study does not.

## Staging

- **Stage 1 (this plan): individual effects.** Each technique is measured
  alone, as one switch flipped on the baseline pipeline, against that baseline.
- **Stage 2 (later, designed from Stage 1's results): combinations.** Which
  techniques to combine, whether their gains add, and the recipe for a strong
  agent. Not designed here, by developer decision. The section at the end only
  records what Stage 1 must collect so Stage 2 is possible.

## Research questions (Stage 1)

- **RQ1 (transfer).** Which published techniques improve self-play learning on
  Breakthrough, and by how much, at matched training compute?
- **RQ2 (efficiency kind).** Does each technique make learning faster (the same
  strength for less compute), raise the strength it eventually reaches, or
  both? Sample efficiency (per game) and compute efficiency (per CPU second) are
  reported separately, because some techniques buy more updates per game at a
  higher cost per game.
- **RQ3 (direct replication).** Do Cohen-Solal 2026's Breakthrough results
  (the only published controlled comparison on this game) reproduce under an
  independent implementation and a stronger evaluation protocol?

## Published claims under test

"Verified" means read from the paper's own text on 2026-09-10. Everything else
must be read from the source before the pre-registration is committed.

| ID | Claim | Source | Domain | Published result | Replication type | Verified |
|---|---|---|---|---|---|---|
| C1 | TreeStrap(alpha-beta) > RootStrap(alpha-beta) > TD-Leaf | Veness, Silver, Blair, Uther, NIPS 2009 | chess, linear evaluator of 1812 features, self-play from random weights | Best Elo (95% CI): TreeStrap(alpha-beta) 2157 +/- 31, TreeStrap(minimax) 1807 +/- 32, RootStrap(alpha-beta) 1362 +/- 59, TD-Leaf 1068 +/- 36, untrained 250 +/- 63. One training run per method | conceptual (new game) | yes, Table 2 |
| C2 | TD-Leaf(lambda) > TD-directed(lambda) | Baxter, Tridgell, Weaver 1999 (arXiv cs/9901001), Section 5 | chess (KnightCap, linear evaluator), learning in games against human opponents on FICS, not self-play | Material weights at standard values, every other weight 0, lambda 0.7. TD-Leaf: FICS blitz rating 1650 +/- 50 -> 2110 +/- 50 in 308 games. TD-directed: "a 200 point rating rise over 300 games", "slower than TDLeaf". One run each. Backgammon (Section 6): from weights already trained by TD(lambda), neither variant changed strength significantly in 50,000 games | conceptual, and the chess comparison is against humans, so no self-play comparison exists | yes, full text |
| C3 | tree learning > root learning > terminal learning | Cohen-Solal, JMLR 27 (2026), Table 2 | 11 games incl. Breakthrough, conv net | Breakthrough, iterative-deepening alpha-beta: tree 79.2%, root 50.6%, terminal 14.3% | **direct** (same game) | yes |
| C4 | additive depth reward > plain win/loss | Cohen-Solal 2026, Table 4, Section 6.2.2 | same | Breakthrough: additive depth 69.5%, win/loss 39.0% | **direct** | yes |
| C5 | ordinal action distribution > epsilon-greedy | Cohen-Solal 2026, Section 7, Remark 16 | many games | "performs better on average and on the majority of games... the gain is however quite slight". No numbers published | conceptual, **exploratory only** | yes |
| C6 | symmetry augmentation helps | Silver et al. 2018 (AlphaZero, Science) on AlphaGo Zero | Go, 8 board symmetries | No ablation isolates augmentation. AlphaGo Zero augmented every position with its 8 symmetries and averaged evaluation over a random symmetry. AlphaZero dropped both, among other changes, and "defeated AlphaGo Zero, winning 61% of games", which the paper reads as a general approach recovering "the performance of an algorithm that exploited board symmetries to generate eight times as much data". So the source gives a practice, not a measured effect | conceptual, **no measured effect in the source**. Breakthrough has 1 symmetry, not 8 | yes, full text |
| C7 | TD(lambda < 1) > Monte Carlo (lambda = 1) | Sutton 1988 (Machine Learning 3), Tesauro 1992 (Machine Learning 8) | Sutton: a 5-state random walk prediction task, linear. Tesauro: backgammon self-play, neural net | Sutton: lambda = 1 (Widrow-Hoff) gave the worst error in both experiments, lowest at lambda = 0 under repeated presentation (Fig. 3) and near 0.3 after one presentation (Fig. 5). Tesauro: lambda "appeared to have almost no effect on the maximum obtainable performance, although there was a speed advantage to using large values", and in the full-game experiment "a few experiments" suggested larger lambda would decrease performance and smaller would not. Neither tested lambda = 1 in a game. In-project prior: lambda = 1 scored below the untrained init (`Docs/hyperparameter-log.md`) | conceptual, and the only game evidence is Tesauro's informal remark | yes, full text |

**Cohen-Solal's protocol, which bounds what C3 and C4 can be compared on.**
Each variant trained 48 hours per repetition, 32 repetitions (48 for Table 4).
A variant's score is its win percentage in an all-play-all among the final
evaluation functions of every variant and repetition, every match played at
**minimax depth 1**. So their percentages measure static-evaluation quality
inside a pool of five or six variants, not strength against outside
opponents. Read as Elo against the pool average, their Breakthrough gaps are
roughly 200 Elo for both C3 (tree vs root) and C4 (depth vs win/loss). That
conversion is rough and is used only to size the power analysis below.

## The baseline pipeline

One trainer, `train.exe tdleaf`, extended with switches in Pass 0. A technique
is a switch value, never a different code path or binary. The baseline is the
pipeline with every switch at its reference value:

| Controlled variable | Baseline value | Why |
|---|---|---|
| backup target | TD-Leaf(lambda), gradient at the PV leaf toward the lambda-return | the regime this project already runs and validates (theory 72), and the method C1 and C2 both report against |
| lambda | one value calibrated in Pass 2 over [0, 0.7] | `Docs/hyperparameter-log.md`: only 0.0, 0.7, 1.0 ever tried |
| terminal value | win/loss, z = 1 or 0 (0 draws in 1,132,483 stored games, so z = 0.5 never occurs) | the plain reward every claim compares against |
| loss | cross-entropy between sigmoid(model output) and the target probability | the trainer's existing loss, kept for every arm so arms differ only in which positions and which targets |
| move selection | uniform-random opening plies (`--open-plies`, one calibrated value), then the search's best move | the project's existing diversity mechanism. Veness also played greedily after an opening book |
| update timing | after each game, over that game's positions | the trainer's existing schedule. Cohen-Solal also learns after each game |
| augmentation | none | |
| model | linear v2 piece-square, 129 features + bias | playbook architecture default, and close in kind to Veness's linear evaluator |
| initialization | scratch, small random weights from the run seed | developer decision, matches Veness and Cohen-Solal |
| generator search | the study head below, exactly | playbook generator-depth rule |
| l2 | 0.0 | |
| learning rate | tuned per arm with an equal budget | see "Hyperparameter fairness" |
| binary | one commit, hash stamped into provenance, no rebuild mid-study | |
| machine, threading | this machine, one thread per training process, a fixed number of concurrent processes | |

## The Stage 1 arms

Every arm differs from the baseline in exactly one switch. The contrast for
each arm is arm minus baseline, and two arm-to-arm contrasts are also
pre-registered because a published claim is stated that way (C1's ordering and
C3's tree-vs-root).

| Arm | Switch | Claim |
|---|---|---|
| B0 | baseline | reference for every contrast |
| A1 | TD-directed(lambda) | C2 |
| A2 | RootStrap(alpha-beta) | C1, C3 (root learning) |
| A3 | TreeStrap(alpha-beta) | C1, C3 (tree learning) |
| A4 | lambda = 1 (Monte Carlo target on PV leaves) | C7 |
| A5 | additive depth terminal reward | C4 |
| A6 | left-right mirror augmentation | C6 |
| A7 | epsilon-greedy exploration | C5's control |
| A8 | ordinal exploration | C5 |

Pre-registered contrasts: A1..A8 each against B0, plus A3 against A2 (C3's
tree-vs-root, and the top of C1's ordering) and A8 against A7 (C5 as stated).

### A1. TD-directed(lambda) (Baxter et al. 1999)

- **What changes.** The lambda-return is computed over the ROOT positions'
  static evaluations instead of the PV leaves'. The search still chooses every
  move. Baxter et al. call this TD-directed(lambda). It is the comparison their
  paper makes against TD-Leaf.
- **Implementation.** `tdLeafGradients` is unchanged. Only the vector `p` and
  the features the gradient is applied to switch from `leaf(s_t)` to `s_t`.
- **Closed-form check.** At lambda = 1 both arms reduce to Monte Carlo on their
  own positions, so A1 at lambda = 1 is Monte Carlo on root positions.
- **Prediction (C2).** A1 below B0.
- **Deviations.** Baxter's chess comparison learned in games against human
  opponents on FICS, starting from standard material values with every other
  weight 0. This study learns by self-play from scratch (developer decision).
  Baxter reports both choices as handicaps: a start from all weights at a
  pawn's value gained 280 points in over 1,000 games against 460 in 308, and
  600 self-play games lost 11 to 89 to the FICS-trained weights. Their
  self-play run trained TD-Leaf only, so no self-play TD-Leaf vs TD-directed
  result exists to replicate, and A1 against B0 is new evidence rather than a
  replication of a measured gap.

### A2. RootStrap(alpha-beta) (Veness et al. 2009)

- **What changes.** After each search, the root's static evaluation is moved
  toward the root's search value. No lambda, no game outcome except where the
  search itself proves a result.
- **Target mapping.** The search returns an integer score, `lround(tanh(out) *
  900)` for a linear model plus whatever `mlLeafScore`'s tail adds. The target
  output is the inverse of that tail, `out* = atanh(score / 900)` after
  undoing any blend, clamped inside (-1, 1). A proven win or loss maps to
  target probability 1 or 0. Pass 1 verifies the round trip on random
  positions: static eval, depth-0 search score, inverse, equal to the model
  output within the 1/900 rounding step.
- **Prediction (C1).** A2 above B0.
- **Deviations.** Veness used squared error on the evaluation. This study keeps
  cross-entropy for every arm so the loss is a controlled variable. Veness
  updated after every search, this study after every game (controlled
  variable). Veness used a simplified training-time search. This study uses the
  serving head, per the playbook.

### A3. TreeStrap(alpha-beta) (Veness et al. 2009, closest to Cohen-Solal's tree learning)

- **What changes.** After each search, EVERY searched node with remaining depth
  at least d_min is moved toward its alpha-beta bound, with Veness's one-sided
  loss: if the static eval is above the node's upper bound it is pushed down to
  it, if below the lower bound it is pushed up, otherwise no update. An exact
  entry is both bounds.
- **Implementation, Veness's own procedure.** Veness computes the update by
  walking the transposition table from the root (`DeltaFromTransTbl`, his
  Algorithm 2): probe a position, apply its bound, recurse into each successor
  that has an entry of sufficient depth. That needs no change to the search, so
  the rated path stays untouched, exactly as `ml_tdleaf.h` requires. Our table
  already stores score, bound flag and remaining depth per entry
  (`src/transposition.h`), and keys are salted, so the walk XORs in
  `ttSearchContext()` (theory 72's lesson). Features are extracted as the walk
  steps through positions.
- **Two engine facts to handle in Pass 0.** The table is always-replace, so
  some of the tree is overwritten before the walk reads it. The walk logs
  coverage (nodes updated per search), the counterpart of TD-Leaf's mean PV
  depth. The table also persists across moves of a game, while Veness cleared
  it before each training search. Clearing it would change the generator away
  from the serving head, so instead each entry gets a search-generation stamp
  that only the walk reads, and the walk uses only the current search's
  entries.
- **Hyperparameter.** d_min, Veness used 1. Calibrated in Pass 2 over a small
  set, since it trades updates per search against their quality.
- **Closed-form check.** With the walk limited to the root, A3 equals A2.
- **Prediction.** A3 above A2 above B0 (C1), and A3 above A2 (C3).
- **Deviations.** Same loss, update-timing and search-mode deviations as A2.
  Cohen-Solal's tree learning differs from TreeStrap: it learns after each game
  on the stored partial tree with non-terminal leaves removed, with minimax
  values from its own searches. The A3 vs A2 contrast is the closest this
  engine gets to their tree-vs-root comparison, and the results doc states that
  mapping plainly rather than calling it identical.

### A4. Monte Carlo target (lambda = 1)

- **What changes.** lambda = 1, which by the unit-tested closed form is
  outcome-supervised training on PV leaves.
- **Prediction (C7).** A4 below B0. The in-project prior agrees: lambda = 1
  scored below the untrained init on the old d6 head.
- **Note.** B0's lambda is calibrated in Pass 2, so A4 is a pre-registered
  contrast against whichever value the calibration picks, stated before Pass 3.

### A5. Additive depth reward (Cohen-Solal 2026, Section 6.2.2)

- **What changes.** The terminal value rewards short wins and long losses.
  Cohen-Solal's form: l = P - p + 1 for a first-player win and -l for a loss,
  with P the maximum number of actions in a game and p the actions played.
  Mapped into this model's win-probability target as z = 0.5 + 0.5 l / P for a
  White win and 0.5 - 0.5 l / P for a Black win.
- **P.** The proven maximum game length if `Docs/axioms.md` can supply one,
  otherwise the engine's 400-ply cap, fixed in Pass 1.
- **Check.** With l held at P for every terminal, the targets equal win/loss
  exactly.
- **Prediction (C4).** A5 above B0.
- **Deviations.** Cohen-Solal's evaluation is unbounded, this model's target is
  a probability, so the mapping is bounded. They measured it with Descent and
  tree learning, here it rides on the TD-Leaf baseline. Whether it behaves
  differently on TreeStrap is a Stage 2 question.
- **Interaction with the search.** The search already prefers faster wins
  (win-decay in `ai_minimax.cpp`). A5 is the training-side counterpart, so the
  results doc reports mean game length per arm, since Cohen-Solal's stated
  mechanism is shorter games.

### A6. Left-right mirror augmentation

- **What changes.** Every trained position is also trained as its left-right
  mirror, with the same target. Breakthrough's rules are left-right symmetric.
  The v2 feature map already has a mirror partner function, `mlv2MirrorIndex`
  (used by `train.exe ensemble --mirror`).
- **Check.** A mirror-symmetric position yields the identical feature vector.
  Updates per game double, and both counts are logged.
- **Prediction (C6).** A6 above B0 at matched games. The source reports the
  practice, not a measured gain (C6 row), so A6 is labelled a test of common
  practice rather than a replication of a number. At matched compute it
  could go either way, since each game costs more to learn from.
- **Relevant in-project result.** Mirroring the champion's trained WEIGHTS
  after the fact cost 135 Elo (theory 30). Augmentation during training is a
  different operation, and the results doc says so rather than treating one as
  evidence about the other.

### A7 and A8. Exploration: epsilon-greedy and ordinal (Cohen-Solal 2026, Section 7)

- **A7 epsilon-greedy.** With probability epsilon a uniformly random move,
  otherwise the best. Implemented today as `--explore`. Positions after a
  random move are currently skipped for training (no valid PV), which is kept.
- **A8 ordinal.** Moves are ranked by search value. The probability of the i-th
  best of n moves is P(c_i) = (e + (1 - e) / (n - i)) (1 - sum_{j<i} P(c_j)),
  so e = 1 always plays the best move and smaller e spreads probability down
  the ranking. Cohen-Solal anneals e = t / T over training. Needs the root's
  per-move search values, which the root loop computes but does not keep
  (`ai_minimax.cpp`, root loop): only the best move's value is exact, the rest
  are alpha-beta bounds. Pass 0 decides between a training-only full-window
  root search for exploratory plies and ranking by bound. Whichever is chosen
  is a recorded deviation.
- **Training on a non-best move.** Under TD-Leaf, the PV below a second-best
  move is still that child's own principal variation, so those positions stay
  trainable in A8. Recorded as a design choice.
- **Hyperparameters.** epsilon for A7 and the e schedule for A8 each get the
  same tuning budget.
- **Prediction (C5).** A8 above A7. Exploratory only, since no published
  effect size exists. Both are also compared to B0, which does not explore past
  the opening.

## Outcome measures

Every run is rated at every rung, so all of these come off one learning curve,
and the pre-registration names which one is primary.

- **Primary: Elo at the final rung T_max, at matched training compute.** It is
  defined for every run, answers the practitioner's question ("I have this much
  compute, which technique?"), and needs no assumption about convergence.
- **S1, area under the learning curve** on a log2 compute axis, the mean Elo
  over rungs. Robust to curves that peak and decline.
- **S2, compute multiplier.** The compute an arm needs to reach B0's T_max Elo,
  log-interpolated between rungs, divided by T_max. A value of 0.25 means the
  technique reaches baseline strength on a quarter of the compute. Censored as
  "> 1" when the arm never gets there. This is the time-to-threshold measure,
  in a unit that does not depend on this machine.
- **S3, peak Elo, corrected for selection.** Veness reports the best checkpoint,
  which is biased upward by the noise of whichever checkpoint wins. Correction:
  split each agent's panel games by game index parity, pick the peak rung on
  one half, report its Elo from the other.
- **S4, plateau Elo, only where a plateau is shown.** Reported only for runs
  whose last two rungs differ by less than one combined SE. Otherwise the run is
  reported as not converged.
- **Sample efficiency.** Elo at matched GAME counts, secondary, since A3 and A6
  change the cost of a game.
- **RQ2 decomposition.** A curve model with a per-arm shift along log compute
  (speed) and a per-arm asymptote (ceiling). It says which kind of gain each
  technique gives, which Stage 2 needs: two speed gains should compose on the
  compute scale, while two ceiling gains compete for the same headroom.

Why converged Elo is not primary: self-play value learning in this project has
an interior optimum more often than not. TD-Leaf peaked at 1,000 to 1,500 games
and declined (theory 46), and 93 of 101 Gumbel-Zero arms were non-monotonic
(theory 50). A convergence point inside a finite budget cannot be verified.

## Power and sample size

Each run's endpoint carries seed-to-seed training variance sigma_seed and
rating error sigma_meas, so sigma^2 = sigma_seed^2 + sigma_meas^2. Both are
measured in Pass 2. The seed-noise band (50 to 150 Elo between replicas, theory
8) is a range, not a standard deviation, so it only brackets the values below.

For an arm with n_a seeds against a baseline with n_0 seeds, the contrast's
standard error is sigma sqrt(1/n_a + 1/n_0). Every contrast shares the baseline,
so the baseline gets more seeds than any arm. The standard allocation for
comparing k arms to one control is n_0 = n_a sqrt(k), about 2.8 n_a for k = 8.

| seeds (arm / baseline) | sigma = 50 | sigma = 75 | sigma = 100 |
|---|---|---|---|
| 5 / 5 | 31.6 | 47.4 | 63.2 |
| 5 / 12 | 26.6 | 39.9 | 53.2 |
| 8 / 20 | 20.9 | 31.4 | 41.8 |
| 10 / 24 | 18.8 | 28.2 | 37.6 |

Minimum detectable effect at 80% power is about 3.5 SE once the 8 comparisons
against the baseline are corrected with Dunnett's procedure (the standard test
for many arms against one control). At sigma = 75 and 8 / 20 seeds that is
about 110 Elo. The published gaps are larger than that for C1 (hundreds of
Elo) and roughly 200 Elo for C3 and C4, so those claims are within reach. C5's
"quite slight" gain is not, and the plan says so in advance: A8 against A7 will
most likely be inconclusive, and it is labelled exploratory for that reason.

Rating error per agent against a panel, G games at mean p(1-p) = 0.2:
SE = 173.7 / sqrt(0.2 G), which is 19.4 at G = 400 and 12.3 at G = 1,000.
Choose G so that sigma_meas <= sigma_seed / 2, which keeps rating error under
20% of the total variance. Past that point, compute spent on seeds buys more
than compute spent on games.

**Multiple comparisons.** The confirmatory family, fixed in the
pre-registration: A1..A7 against B0 (Dunnett), A3 against A2, and the C1
ordering. A8 and anything not listed are exploratory.

**"Did not transfer" needs an equivalence test, not a failed significance
test.** A technique is reported as having no effect only if its 90% CI lies
inside a pre-registered margin (two one-sided tests, margin proposed at +/- 30
Elo). A wide CI straddling zero is reported as inconclusive.

## Evaluation instrument

**Study head: `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3`**, the Round 4
node-track head.

- A node budget measures strength per node, which isolates evaluator quality,
  and evaluator quality is what a training technique changes.
- Every arm's model is linear v2 with identical per-node cost, so the node and
  time tracks sit at the same operating point (0.25 us/node,
  `ranking/tracks.txt`). Each agent's `cpu_ms_move` is still recorded to
  confirm it.
- Node-budget agents are deterministic (theory 71). `time=` agents are not
  (theories 59 and 71), which would put machine load into every game.

**Frozen reference panel.** A fixed set of opponents spanning the whole range
the learning curves cross: untrained scratch models and weak fixed agents
(`rand@1`, `greedy@1`, shallow `ab` heads) through the Classic chip counter to
the top Round 4 cores. Panel ratings are pinned from the converged Round 4
full-roster fit. Each study agent plays only the panel, into a study store
separate from `ranking/matches.jsonl`, and is rated with `rank.exe rate --pin`.
So every study agent's Elo is an independent estimate on one fixed scale, study
agents never shift each other, and the hazard of comparing across fits does
not arise. Panel size and spread are fixed in Pass 2 so low rungs are resolved
as well as high ones.

**Game independence.** Deterministic agents against deterministic opponents
replay one game per colour (`Docs/benchmarking.md`, defect 3). Games therefore
start from paired random openings, each opening played twice with colours
swapped (`rank.exe play --paired-openings`). If the same opening sequence is
drawn for every study agent against a given panel member, the contrasts between
arms also benefit from common random numbers (every arm faces identical
openings, so opening luck cancels in the difference). Pass 1 verifies both
properties by reading back stored games. The distinct-trajectory count is
reported next to every rating.

**Secondary instruments.**

1. **Cohen-Solal's statistic (RQ3).** An all-play-all among the final
   checkpoints of B0, A2, A3 and A5, every agent at `ab(deep=1)@3` (their
   matches are depth 1), scored as they score it. Their percentages come from
   their own pools, so the comparison is the ordering and the direction of
   each gap, not the absolute percentage.
2. **Transitivity check.** A head-to-head matrix among the best final agent of
   each arm, compared against panel Elo. Techniques that learn different styles
   can order differently head to head than against a panel.

## Training compute accounting

- **Primary unit: process CPU seconds**, one thread per process, measured with
  `GetProcessTimes`. `src/train_budget.cpp` measures wall clock
  (`steady_clock`) today, and budget-parity P6 recorded per-game rates
  differing 15x under contention, so wall clock alone cannot carry a compute
  claim. Wall seconds, nodes searched, games, positions trained and gradient
  updates are logged beside it.
- **Update cost is charged to the budget**, as Veness did. TreeStrap's extra
  updates and augmentation's doubled updates are part of their price.
- **Rungs are game counts calibrated to compute (developer decision).** Pass 2
  measures each arm's CPU seconds per game. Pass 3 expresses each rung as a
  game count equal to the compute target divided by that cost, on a doubling
  ladder from T_0 to T_max sized from where Pass 2's curves flatten. Every
  checkpoint is then a pure function of (commit, config, seed). Realized CPU
  seconds are logged and are the analysis x axis. This is the method theory
  48's compute-matched Gumbel sweep used.

## Hyperparameter fairness

Unequal tuning is the standard confound in method comparisons (Henderson et
al. 2018). Veness tuned the step size per method, and so does this study,
under a fixed protocol:

- Every arm's learning rate gets the **same random-search budget**: the same
  number of draws over a log range of the same width, with the same seeds
  (Bergstra and Bengio 2012). The width is shared and the position is each
  arm's own, set by one probe applied identically to every arm (developer
  decision, 2026-09-11). Pass 1 showed why a single shared range cannot work:
  TreeStrap sums its update over about 9,000 table entries per search and
  diverges at a rate 100 to 1,000 times below the baseline's
  (`plans/replication-study-results-1-brass-lectern.md`, Pass 1 section 6).
  **The probe** (`tools/replication_lr_probe.ps1`): every arm at every
  half-decade learning rate from 1e-8 to 1, one calibration seed, 50 games,
  checkpoints every 10 games. D, the divergence point, is the lowest rate
  whose weights exceed max |w| 5 at any checkpoint. L, the floor, is the lowest
  rate whose weights move a mean of 0.01 from the initialization. The tuning
  range is [D / 10^2.5, D / 10^0.5], 2 decades below D, with L reported beside
  it so a range reaching below the floor is visible.
- Arm-specific hyperparameters (lambda for B0 and A1, d_min for A3, epsilon for
  A7, the e schedule for A8) get the same budget again, jointly with the
  learning rate.
- Tuned values are fixed for Pass 3. Nothing else is tuned per arm.
- Sensitivity is reported: each arm's effect at its own tuned learning rate and
  at the rate sitting at the same position relative to its own D as the
  baseline's tuned rate sits relative to the baseline's D. A technique whose
  gain disappears there is reported that way.

## Pass 0: what has to be built

Each item lands with its test before any Pass 1 run.

| Item | What | Verification |
|---|---|---|
| I1 | `--backup td-leaf\|td-directed\|rootstrap\|treestrap` in `trainTDLeaf` (A1, A2, A3) | the closed forms above. The TreeStrap walk never runs inside the search: with the walk on, the search returns the identical move, score and node count on a fixed position set |
| I2 | search-generation stamp on table entries, read only by the TreeStrap walk | a stale entry from the previous move is never updated. The rated search's behavior is byte-identical with the stamp present |
| I3 | score-to-target inverse mapping (A2, A3) | round trip within the 1/900 step on random positions |
| I4 | `--terminal winloss\|depth` (A5) | l held at P reproduces win/loss exactly |
| I5 | `--augment mirror` (A6) | mirror-symmetric positions yield identical features, update counts double |
| I6 | `--explore-dist eps\|ordinal` and the root move-value source (A7, A8) | the empirical move-rank distribution over many draws matches the closed form |
| I7 | CPU-seconds accounting in `train_budget`, written into provenance | two settings of a busy-loop load give the same CPU seconds and different wall seconds |
| I8 | panel file, study store, analysis export | paired openings verified from stored games, study store disjoint from the main store |
| I9 | `analysis/replication_stage1.py`: contrasts against baseline with Dunnett correction, bootstrap CIs over seeds, compute multiplier, AULC, split-half peak, two one-sided tests, speed-vs-ceiling curve fit | run on synthetic data with known effects, which it must recover |

## Passes

The playbook's four passes, mapped onto Stage 1.

**Pass 1, sanity.** Every switch at two settings, output differs as claimed.
Each arm trains, stops at its rung, writes truthful provenance, and loads
through the real search path. Mean PV depth is well above 1 on every TD-Leaf
arm (theory 72's guard), and TreeStrap coverage is reported. Same seed twice
gives a byte-identical model. Panel games are independent. Report to the
developer before Pass 2.

**Pass 2, calibration.** Four outputs, all required before the grid is shown:

1. Curve shape: B0 and every arm at 1 seed on a generous ladder, to place
   T_max past where curves flatten.
2. Noise: 5 seeds of B0 and of one contrasting arm at the candidate T_max,
   giving sigma_seed. Panel fits give sigma_meas. The power analysis then fixes
   seeds per arm, seeds for the baseline, and G.
3. Tuning: the equal-budget search per arm, including lambda.
4. Cost: CPU seconds per game per arm, which sets the game-count rungs.

Then the grid goes to the developer: arms, seeds, rungs, agent count, total
games implied, and one projection from measured throughput if it exceeds a
day.

**Pass 3, the confirmatory run.** Exactly the pre-registered grid. This
departs on purpose from the playbook's "1 seed per draw" rule for Pass 3: that
rule is for exploratory random search analysed by a fitted model. This pass is
a designed confirmatory comparison whose replication comes from seeds. Report
the shape of the result to the developer before Pass 4.

**Pass 4, validation.** Every arm claimed as a positive effect is trained
again on fresh seeds (at least 5) never used for any Pass 2 or Pass 3 choice,
together with fresh baseline seeds, with the ladder extended to confirm any
plateau. A claimed effect must hold on those seeds, which controls the
winner's curse of picking the best-looking results. Certification in the
full-roster refit waits for Stage 2's strong-agent recipe.

## Pre-registration

Before any Pass 3 game, commit
`plans/replication-study-prereg-1-brass-lectern.md` containing: the directional
hypotheses from the claim table, the primary endpoint, the contrasts and their
correction, alpha, the equivalence margin, the rung list and T_max, seeds and
G, exclusion rules (a crashed run is rerun on the same seed and reported), and
the code commit hash. Its commit date and hash are the evidence that the
analysis was fixed before the data existed. Every later deviation is listed in
the results doc with its reason.

## Data collected

| Level | Recorded |
|---|---|
| training run | full config, seed, commit hash, CPU seconds, wall seconds, nodes searched, games, positions trained, gradient updates, mean game length, mean PV depth (TD-Leaf arms), TreeStrap coverage, mean absolute TD error or bound violation, weight norm, per-rung checkpoint files |
| rated agent | canonical ID, rung, compute at the rung, panel Elo and SE, games and distinct trajectories, `cpu_ms_move`, realized nodes per move |
| game | both IDs, opening sequence and its pair index, colour, result, plies, per-side nodes and ms |
| study | panel membership and pinned ratings, the Round 4 fit they came from, the pre-registration hash |

## What Stage 1 reports

- Levels first: Elo per arm per rung with per-seed points, and learning curves
  with 95% bootstrap intervals over seeds.
- Then each contrast with its CI, compute multipliers, equivalence verdicts,
  and the speed-vs-ceiling classification per arm.
- Each claim side by side with the published numbers. C1 is compared on
  ordering and on the ratio of gains over the untrained agent, never on
  absolute Elo, since the scales belong to different pools and games. From
  Veness Table 2 the gains over untrained are TreeStrap(alpha-beta) 1907,
  RootStrap(alpha-beta) 1112, TD-Leaf 818, so TreeStrap gains 2.33 times what
  TD-Leaf gains.
- Reporting follows Agarwal et al. 2021: interval estimates over runs rather
  than point estimates, and the distribution of per-seed results rather than a
  mean alone.
- Artifact release: code at the study commit, every config and seed, every
  checkpoint, the study match store, the analysis scripts, and a compute
  statement (hardware, total CPU hours for training and for evaluation).

## Threats to validity

- **One game.** Transfer to other games is not claimed.
- **Linear evaluator.** Cohen-Solal used conv nets, Veness a linear model of
  1812 features. A technique that needs capacity may underperform here for a
  reason unrelated to Breakthrough.
- **Implementation fidelity.** Each arm is a reimplementation from the paper's
  text. Every deviation is listed in its arm's section and carried into the
  results doc.
- **Panel dependence.** Elo against a fixed panel can differ from head-to-head
  strength. The transitivity check measures how much.
- **Single machine.** CPU seconds tie compute to this hardware. Nodes and games
  are logged so the result can be restated elsewhere.
- **Tuning.** An equal budget is fair by protocol, not optimal for every
  technique. The sensitivity table shows how much it matters.

## Stage 2, deferred: what Stage 1 must leave behind

Stage 2 designs combinations after Stage 1's results are in. For it to be
possible, Stage 1 keeps: every arm's tuned hyperparameters, its speed-vs-ceiling
classification, and its effect size with CI. The additivity question Stage 2
will ask is measured by the interaction I_AB = E[AB] - E[A] - E[B] + E[B0], on
both the Elo scale and the log-compute scale, since a ceiling alone makes Elo
gains look sub-additive near the top.

## Dependencies

1. Round 4 converges and `ranking/CHAMPION.md` is rewritten. Panel ratings are
   pinned from that fit. Pass 0 to Pass 2 can proceed before it.
2. Budget-parity Part 1 fixes (landed) and theory 72's PV fix (landed).
3. A contiguous model-slot range claimed and recorded in `src/CLAUDE.md` before
   the first checkpoint is published.
4. Every claim read from its source before the pre-registration is committed.
   C2, C6 and C7 were read on 2026-09-10 (table above).

## Decisions taken (developer, 2026-09-10)

1. **Compute matching: calibrated game counts.** Every checkpoint reproducible
   from (commit, config, seed). Realized CPU seconds are the analysis x axis.
2. **Scratch initialization only.** C2's KnightCap result started from
   standard material values and learned against humans, both recorded as A1's
   deviations.
3. **Baseline backup target: TD-Leaf.**
4. **Stage 1 measures individual effects only.** Combinations, and the question
   of how to assemble a strong agent from them, are designed after Stage 1's
   results.
