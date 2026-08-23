# Peak-eff_elo_per_log2cpu predictor: decision tree / feature importance

Source: `plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv`, 92 draws (one row per `block`, collapsing each draw's checkpoint ladder to its peak `eff_elo_per_log2cpu`).
Purpose: identify, per swept config axis, which values tend to produce the highest eff_elo_per_log2cpu, to bias the next round's sweep ranges rather than searching from scratch. This is descriptive/exploratory, not a certification -- see Caveats.

## Method

Three models fit on the same 11 swept axes -> peak eff_elo_per_log2cpu, compared by 5-fold cross-validated R2 (`sklearn.model_selection.KFold`, shuffle, seed 0):

| Model | CV R2 (mean +/- std) | Why fit it |
|---|---|---|
| Decision tree (depth=3) | 0.684 +/- 0.079 | Human-readable rules; captures interactions and non-monotonic effects without assuming a functional form |
| Random forest (500 trees) | 0.720 +/- 0.049 | Averages many trees for a more stable importance ranking than any single tree's splits |
| Linear (standardized) | 0.654 +/- 0.069 | Baseline: if this is much worse than the tree/RF, the axes' effects are not additive/monotonic |

Tree depth chosen by CV R2 over depths 1=0.526, 2=0.644, 3=0.684, 4=0.675 (min_samples_leaf=8 throughout, to keep leaves large enough to average out per-draw seed noise).

## Decision tree rules

Fit on all 92 draws (the CV split above is for model comparison only; this tree is refit on the full dataset for interpretation):

```
|--- modeltype_mlp <= 0.5
|   |--- sims <= 450
|   |   |--- batch <= 80
|   |   |   |--- value: [47.7]
|   |   |--- batch >  80
|   |   |   |--- value: [43.2]
|   |--- sims >  450
|   |   |--- value: [41.0]
|--- modeltype_mlp >  0.5
|   |--- l2 <= 0.00015
|   |   |--- value: [72.3]
|   |--- l2 >  0.00015
|   |   |--- lr <= 0.02
|   |   |   |--- value: [61.3]
|   |   |--- lr >  0.02
|   |   |   |--- value: [53.1]
```

Rendered diagram: `plans/gumbel-mcts-arch-efficiency-tree-6-silver-thistle.png`.

## Feature importance

**Note:** `replaycap` and `replaywarm` were not drawn independently in this sweep (Spearman rho=1.00 across all 92 draws -- every draw's value of one determines the other). Their importances below describe one underlying axis counted twice, not two independent effects.

RF impurity importance (in-sample, from the full-data fit) vs. out-of-fold permutation importance (mean R2 drop when a feature is shuffled in a held-out fold, averaged over the 5 folds -- the more trustworthy of the two since it is evaluated out-of-sample):

| Feature | RF impurity importance | OOF permutation importance (mean +/- std) |
|---|---|---|
| modeltype_mlp | 0.572 | 1.298 +/- 0.319 |
| l2 | 0.185 | 0.261 +/- 0.108 |
| lr | 0.053 | 0.044 +/- 0.070 |
| sims | 0.050 | 0.032 +/- 0.011 |
| batch | 0.025 | 0.013 +/- 0.010 |
| cscale | 0.019 | 0.006 +/- 0.004 |
| open | 0.027 | 0.005 +/- 0.018 |
| replaycap | 0.011 | -0.002 +/- 0.007 |
| replaywarm | 0.011 | -0.002 +/- 0.006 |
| cvisit | 0.025 | -0.002 +/- 0.013 |
| m | 0.020 | -0.005 +/- 0.008 |

## Linear regression (standardized coefficients)

eff_elo_per_log2cpu change per +1 standard deviation of the feature, holding the others fixed (R2=0.654, see caution above about how much this linear fit actually explains):

| Feature | Std. coefficient (eff_elo_per_log2cpu / 1 sd) |
|---|---|
| replaycap | +38.5 |
| replaywarm | -37.7 |
| modeltype_mlp | +9.3 |
| l2 | -4.1 |
| lr | -2.7 |
| sims | -1.2 |
| batch | -0.9 |
| open | +0.6 |
| cscale | -0.6 |
| cvisit | +0.2 |
| m | +0.1 |

## Univariate bucket means (no model, raw data)

Every swept axis here is a small discrete grid, so the plain per-value mean peak `peak_eff_elo_per_log2cpu` is itself readable, with no model in the loop:

**sims**

| value | mean | median | std | n |
|---|---|---|---|---|
| 300 | 54 | 50 | 12 | 38 |
| 400 | 54 | 53 | 10 | 27 |
| 500 | 52 | 49 | 13 | 27 |

**lr**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.003 | 56 | 53 | 12 | 27 |
| 0.01 | 56 | 56 | 13 | 34 |
| 0.03 | 49 | 47 | 10 | 31 |

**l2**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 57 | 53 | 15 | 33 |
| 0.0003 | 53 | 50 | 11 | 28 |
| 0.001 | 51 | 50 | 8 | 31 |

**replaycap**

| value | mean | median | std | n |
|---|---|---|---|---|
| 500 | 49 | 46 | 10 | 28 |
| 2000 | 56 | 53 | 13 | 31 |
| 8000 | 56 | 53 | 12 | 33 |

**replaywarm**

| value | mean | median | std | n |
|---|---|---|---|---|
| 16 | 49 | 46 | 10 | 28 |
| 32 | 56 | 53 | 13 | 31 |
| 128 | 56 | 53 | 12 | 33 |

**batch**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 54 | 50 | 12 | 33 |
| 32 | 56 | 56 | 11 | 31 |
| 128 | 51 | 47 | 13 | 28 |

**open**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0 | 54 | 50 | 12 | 24 |
| 4 | 52 | 50 | 10 | 37 |
| 8 | 56 | 53 | 14 | 31 |

**cvisit**

| value | mean | median | std | n |
|---|---|---|---|---|
| 400 | 56 | 56 | 12 | 21 |
| 600 | 55 | 50 | 12 | 26 |
| 800 | 53 | 50 | 12 | 21 |
| 1000 | 51 | 48 | 12 | 24 |

**cscale**

| value | mean | median | std | n |
|---|---|---|---|---|
| 40 | 56 | 54 | 11 | 20 |
| 70 | 55 | 50 | 14 | 31 |
| 100 | 52 | 49 | 12 | 31 |
| 130 | 52 | 57 | 12 | 10 |

**m**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 55 | 51 | 13 | 25 |
| 12 | 53 | 52 | 12 | 24 |
| 16 | 53 | 50 | 11 | 24 |
| 20 | 54 | 50 | 12 | 19 |

**modeltype_mlp**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 45 | 44 | 5 | 46 |
| 1.0 | 63 | 62 | 10 | 46 |

**peak_rung distribution** (which rung the peak landed on)

| rung | count |
|---|---|
| 100 | 1 |
| 400 | 22 |
| 1500 | 24 |
| 4000 | 45 |

## Recommendation for the next round

Auto-generated from the tables above: for each axis, ranked by out-of-fold permutation importance, the univariate best-performing level and a read on whether the effect clears the noise floor. Coupled axes (see note above) are merged into one entry.

| Axis | Best level (highest mean peak_eff_elo_per_log2cpu) | OOF perm. importance | Read |
|---|---|---|---|
| modeltype_mlp | modeltype_mlp=1.0 (mean 63) | 1.298 | clear effect -- fix at the best level |
| l2 | l2=0.0 (mean 57) | 0.261 | clear effect -- fix at the best level |
| lr | lr=0.01 (mean 56) | 0.044 | clear effect -- fix at the best level |
| sims | sims=300 (mean 54) | 0.032 | clear effect -- fix at the best level |
| batch | batch=32 (mean 56) | 0.013 | weak/marginal effect |
| cscale | cscale=40 (mean 56) | 0.006 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| open | open=8 (mean 56) | 0.005 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| replaycap (= replaywarm) | replaycap=8000 (mean 56) | -0.002 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| cvisit | cvisit=400 (mean 56) | -0.002 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| m | m=8 (mean 55) | -0.005 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |

## Caveats

- **n=92, single seed per draw.** This target has an Elo component, which sits inside this project's documented 50-150 Elo seed-noise band, so the target variable is noisy on that account even though it is not Elo itself; the CV R2 above is the honest ceiling on how much of that noise these models can actually explain, not just the training-set fit.
- **Not a controlled factorial.** Most axes were drawn independently per draw, which keeps them roughly uncorrelated in expectation (unlike a hand-picked grid), but 92 draws over this many axes still leaves each importance estimate wide. Treat rankings as directional, not precise. Exception: `replaycap`/`replaywarm` are perfectly coupled in this sweep (see the Feature importance note), so they are not independent of each other.
- **Predicts peak eff_elo_per_log2cpu, not final-rung eff_elo_per_log2cpu.** The target is the max over each draw's 4-rung ladder, so `peak_rung` is an output of the search, not a controlled input -- do not read a feature's importance here as telling you which rung to train to.
- **Architecture/init scope.** `modeltype` is included as a feature (one-hot, mlp vs conv), but the architecture-specific width axis (`--mlp-hidden` / `--conv-channels`) is excluded -- its values are not comparable across the two model types, see the results doc's separate per-architecture width table instead.
- **Correlational.** As with every observation in the results doc this predictor is built from, nothing here isolates a single axis's causal effect. Use it to prioritize where a controlled follow-up sweep should look, not as a substitute for one.
