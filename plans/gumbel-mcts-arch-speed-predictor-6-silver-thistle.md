# Min-cpu_ms_move predictor: decision tree / feature importance

Source: `plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv`, 92 draws (one row per `block`, collapsing each draw's checkpoint ladder to its min `cpu_ms_move`).
Purpose: identify, per swept config axis, which values tend to produce the lowest cpu_ms_move, to bias the next round's sweep ranges rather than searching from scratch. This is descriptive/exploratory, not a certification -- see Caveats.

## Method

Three models fit on the same 11 swept axes -> min cpu_ms_move, compared by 5-fold cross-validated R2 (`sklearn.model_selection.KFold`, shuffle, seed 0):

| Model | CV R2 (mean +/- std) | Why fit it |
|---|---|---|
| Decision tree (depth=3) | 0.535 +/- 0.102 | Human-readable rules; captures interactions and non-monotonic effects without assuming a functional form |
| Random forest (500 trees) | 0.472 +/- 0.109 | Averages many trees for a more stable importance ranking than any single tree's splits |
| Linear (standardized) | 0.372 +/- 0.111 | Baseline: if this is much worse than the tree/RF, the axes' effects are not additive/monotonic |

Tree depth chosen by CV R2 over depths 1=0.329, 2=0.523, 3=0.535, 4=0.529 (min_samples_leaf=8 throughout, to keep leaves large enough to average out per-draw seed noise).

## Decision tree rules

Fit on all 92 draws (the CV split above is for model comparison only; this tree is refit on the full dataset for interpretation):

```
|--- modeltype_mlp <= 0.5
|   |--- sims <= 450
|   |   |--- batch <= 80
|   |   |   |--- value: [49.1]
|   |   |--- batch >  80
|   |   |   |--- value: [91.1]
|   |--- sims >  450
|   |   |--- value: [164.3]
|--- modeltype_mlp >  0.5
|   |--- sims <= 350
|   |   |--- value: [3.5]
|   |--- sims >  350
|   |   |--- batch <= 80
|   |   |   |--- value: [10.4]
|   |   |--- batch >  80
|   |   |   |--- value: [5.1]
```

Rendered diagram: `plans/gumbel-mcts-arch-speed-tree-6-silver-thistle.png`.

## Feature importance

**Note:** `replaycap` and `replaywarm` were not drawn independently in this sweep (Spearman rho=1.00 across all 92 draws -- every draw's value of one determines the other). Their importances below describe one underlying axis counted twice, not two independent effects.

RF impurity importance (in-sample, from the full-data fit) vs. out-of-fold permutation importance (mean R2 drop when a feature is shuffled in a held-out fold, averaged over the 5 folds -- the more trustworthy of the two since it is evaluated out-of-sample):

| Feature | RF impurity importance | OOF permutation importance (mean +/- std) |
|---|---|---|
| modeltype_mlp | 0.392 | 0.938 +/- 0.345 |
| sims | 0.224 | 0.277 +/- 0.056 |
| cscale | 0.083 | 0.042 +/- 0.037 |
| batch | 0.049 | 0.025 +/- 0.048 |
| open | 0.042 | 0.018 +/- 0.055 |
| l2 | 0.050 | 0.001 +/- 0.016 |
| cvisit | 0.061 | -0.007 +/- 0.034 |
| replaycap | 0.022 | -0.009 +/- 0.010 |
| replaywarm | 0.020 | -0.010 +/- 0.012 |
| m | 0.035 | -0.029 +/- 0.016 |
| lr | 0.023 | -0.029 +/- 0.013 |

## Linear regression (standardized coefficients)

cpu_ms_move change per +1 standard deviation of the feature, holding the others fixed (R2=0.372, see caution above about how much this linear fit actually explains):

| Feature | Std. coefficient (cpu_ms_move / 1 sd) |
|---|---|
| replaycap | -151.3 |
| replaywarm | +148.1 |
| modeltype_mlp | -40.6 |
| sims | +17.3 |
| cscale | +9.8 |
| open | +9.7 |
| cvisit | +6.3 |
| m | -3.5 |
| lr | +2.7 |
| batch | -1.1 |
| l2 | +0.8 |

## Univariate bucket means (no model, raw data)

Every swept axis here is a small discrete grid, so the plain per-value mean min `min_cpu_ms_move` is itself readable, with no model in the loop:

**sims**

| value | mean | median | std | n |
|---|---|---|---|---|
| 300 | 40 | 21 | 46 | 38 |
| 400 | 32 | 15 | 55 | 27 |
| 500 | 78 | 21 | 94 | 27 |

**lr**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.003 | 47 | 16 | 64 | 27 |
| 0.01 | 40 | 15 | 62 | 34 |
| 0.03 | 60 | 16 | 78 | 31 |

**l2**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 60 | 21 | 75 | 33 |
| 0.0003 | 45 | 15 | 67 | 28 |
| 0.001 | 41 | 7 | 62 | 31 |

**replaycap**

| value | mean | median | std | n |
|---|---|---|---|---|
| 500 | 69 | 17 | 80 | 28 |
| 2000 | 37 | 11 | 56 | 31 |
| 8000 | 43 | 18 | 65 | 33 |

**replaywarm**

| value | mean | median | std | n |
|---|---|---|---|---|
| 16 | 69 | 17 | 80 | 28 |
| 32 | 37 | 11 | 56 | 31 |
| 128 | 43 | 18 | 65 | 33 |

**batch**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 51 | 16 | 66 | 33 |
| 32 | 38 | 16 | 68 | 31 |
| 128 | 59 | 29 | 70 | 28 |

**open**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0 | 37 | 16 | 56 | 24 |
| 4 | 46 | 17 | 63 | 37 |
| 8 | 61 | 11 | 81 | 31 |

**cvisit**

| value | mean | median | std | n |
|---|---|---|---|---|
| 400 | 34 | 9 | 50 | 21 |
| 600 | 30 | 11 | 45 | 26 |
| 800 | 49 | 16 | 70 | 21 |
| 1000 | 82 | 31 | 89 | 24 |

**cscale**

| value | mean | median | std | n |
|---|---|---|---|---|
| 40 | 24 | 7 | 40 | 20 |
| 70 | 43 | 16 | 58 | 31 |
| 100 | 61 | 29 | 75 | 31 |
| 130 | 80 | 6 | 103 | 10 |

**m**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 47 | 16 | 72 | 25 |
| 12 | 47 | 17 | 62 | 24 |
| 16 | 56 | 16 | 75 | 24 |
| 20 | 45 | 15 | 66 | 19 |

**modeltype_mlp**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 91 | 71 | 76 | 46 |
| 1.0 | 7 | 4 | 6 | 46 |

**min_rung distribution** (which rung the min landed on)

| rung | count |
|---|---|
| 100 | 46 |
| 400 | 30 |
| 1500 | 9 |
| 4000 | 7 |

## Recommendation for the next round

Auto-generated from the tables above: for each axis, ranked by out-of-fold permutation importance, the univariate best-performing level and a read on whether the effect clears the noise floor. Coupled axes (see note above) are merged into one entry.

| Axis | Best level (lowest mean min_cpu_ms_move) | OOF perm. importance | Read |
|---|---|---|---|
| modeltype_mlp | modeltype_mlp=1.0 (mean 7) | 0.938 | clear effect -- fix at the best level |
| sims | sims=400 (mean 32) | 0.277 | clear effect -- fix at the best level |
| cscale | cscale=40 (mean 24) | 0.042 | clear effect -- fix at the best level |
| batch | batch=32 (mean 38) | 0.025 | weak/marginal effect |
| open | open=0 (mean 37) | 0.018 | weak/marginal effect |
| l2 | l2=0.001 (mean 41) | 0.001 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| cvisit | cvisit=600 (mean 30) | -0.007 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| replaycap (= replaywarm) | replaycap=2000 (mean 37) | -0.009 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| m | m=20 (mean 45) | -0.029 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| lr | lr=0.01 (mean 40) | -0.029 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |

## Caveats

- **n=92, single seed per draw.** This target has no Elo component, but is still a single measurement per draw (one gauntlet's worth of games), so it carries its own per-draw sampling noise, unquantified here; the CV R2 above is the honest ceiling on how much of that noise these models can actually explain, not just the training-set fit.
- **Not a controlled factorial.** Most axes were drawn independently per draw, which keeps them roughly uncorrelated in expectation (unlike a hand-picked grid), but 92 draws over this many axes still leaves each importance estimate wide. Treat rankings as directional, not precise. Exception: `replaycap`/`replaywarm` are perfectly coupled in this sweep (see the Feature importance note), so they are not independent of each other.
- **Predicts min cpu_ms_move, not final-rung cpu_ms_move.** The target is the min over each draw's 4-rung ladder, so `min_rung` is an output of the search, not a controlled input -- do not read a feature's importance here as telling you which rung to train to.
- **Architecture/init scope.** `modeltype` is included as a feature (one-hot, mlp vs conv), but the architecture-specific width axis (`--mlp-hidden` / `--conv-channels`) is excluded -- its values are not comparable across the two model types, see the results doc's separate per-architecture width table instead.
- **Correlational.** As with every observation in the results doc this predictor is built from, nothing here isolates a single axis's causal effect. Use it to prioritize where a controlled follow-up sweep should look, not as a substitute for one.
