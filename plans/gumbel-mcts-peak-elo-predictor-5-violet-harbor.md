# Peak-Elo predictor: decision tree / feature importance

Source: `plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv`, 101 draws (one row per `block`, collapsing each draw's checkpoint ladder to its peak `peak_elo`).
Purpose: identify, per swept config axis, which values tend to produce the highest peak Elo, to bias the next (MLP) round's sweep ranges rather than searching from scratch. This is descriptive/exploratory, not a certification -- see Caveats.

## Method

Three models fit on the same 10 swept axes -> peak Elo, compared by 5-fold cross-validated R2 (`sklearn.model_selection.KFold`, shuffle, seed 0):

| Model | CV R2 (mean +/- std) | Why fit it |
|---|---|---|
| Decision tree (depth=3) | 0.623 +/- 0.131 | Human-readable rules; captures interactions and non-monotonic effects without assuming a functional form |
| Random forest (500 trees) | 0.714 +/- 0.108 | Averages many trees for a more stable importance ranking than any single tree's splits |
| Linear (standardized) | 0.601 +/- 0.079 | Baseline: if this is much worse than the tree/RF, the axes' effects are not additive/monotonic |

Tree depth chosen by CV R2 over depths 1=0.578, 2=0.598, 3=0.623, 4=0.617 (min_samples_leaf=8 throughout, to keep leaves large enough to average out per-draw seed noise).

## Decision tree rules

Fit on all 101 draws (the CV split above is for model comparison only; this tree is refit on the full dataset for interpretation):

```
|--- l2 <= 0.00015
|   |--- replaycap <= 5000
|   |   |--- replaycap <= 1250
|   |   |   |--- value: [683.5]
|   |   |--- replaycap >  1250
|   |   |   |--- value: [717.0]
|   |--- replaycap >  5000
|   |   |--- value: [761.5]
|--- l2 >  0.00015
|   |--- l2 <= 0.00065
|   |   |--- replaycap <= 1250
|   |   |   |--- value: [591.0]
|   |   |--- replaycap >  1250
|   |   |   |--- value: [625.5]
|   |--- l2 >  0.00065
|   |   |--- cscale <= 115
|   |   |   |--- value: [583.7]
|   |   |--- cscale >  115
|   |   |   |--- value: [534.2]
```

Rendered diagram: `plans/gumbel-mcts-peak-elo-tree-5-violet-harbor.png`.

## Feature importance

**Note:** `replaycap` and `replaywarm` were not drawn independently in this sweep (Spearman rho=1.00 across all 101 draws -- every draw's value of one determines the other). Their importances below describe one underlying axis counted twice, not two independent effects.

RF impurity importance (in-sample, from the full-data fit) vs. out-of-fold permutation importance (mean R2 drop when a feature is shuffled in a held-out fold, averaged over the 5 folds -- the more trustworthy of the two since it is evaluated out-of-sample):

| Feature | RF impurity importance | OOF permutation importance (mean +/- std) |
|---|---|---|
| l2 | 0.660 | 1.346 +/- 0.281 |
| replaywarm | 0.056 | 0.050 +/- 0.017 |
| replaycap | 0.063 | 0.050 +/- 0.015 |
| cscale | 0.048 | 0.037 +/- 0.033 |
| batch | 0.030 | 0.014 +/- 0.012 |
| lr | 0.031 | 0.012 +/- 0.012 |
| open | 0.034 | 0.008 +/- 0.021 |
| m | 0.024 | -0.001 +/- 0.010 |
| cvisit | 0.032 | -0.002 +/- 0.008 |
| sims | 0.023 | -0.008 +/- 0.008 |

## Linear regression (standardized coefficients)

Elo change per +1 standard deviation of the feature, holding the others fixed (R2=0.601, see caution above about how much this linear fit actually explains):

| Feature | Std. coefficient (Elo / 1 sd) |
|---|---|
| l2 | -59.0 |
| replaywarm | +49.3 |
| replaycap | -22.2 |
| cscale | -15.7 |
| cvisit | -6.8 |
| sims | +4.5 |
| open | -3.4 |
| batch | +2.9 |
| lr | +1.7 |
| m | +0.0 |

## Univariate bucket means (no model, raw data)

Every swept axis here is a small discrete grid, so the plain per-value mean peak `peak_elo` is itself readable, with no model in the loop:

**sims**

| value | mean | median | std | n |
|---|---|---|---|---|
| 300 | 630 | 609 | 78 | 38 |
| 400 | 630 | 604 | 91 | 32 |
| 500 | 626 | 612 | 78 | 31 |

**lr**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.003 | 634 | 620 | 84 | 43 |
| 0.01 | 617 | 600 | 75 | 28 |
| 0.03 | 633 | 604 | 84 | 30 |

**l2**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 721 | 726 | 47 | 32 |
| 0.0003 | 614 | 604 | 44 | 27 |
| 0.001 | 568 | 570 | 52 | 42 |

**replaycap**

| value | mean | median | std | n |
|---|---|---|---|---|
| 500 | 608 | 601 | 61 | 37 |
| 2000 | 615 | 598 | 80 | 34 |
| 8000 | 671 | 704 | 91 | 30 |

**replaywarm**

| value | mean | median | std | n |
|---|---|---|---|---|
| 16 | 608 | 601 | 61 | 37 |
| 32 | 615 | 598 | 80 | 34 |
| 128 | 671 | 704 | 91 | 30 |

**batch**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 632 | 616 | 86 | 32 |
| 32 | 620 | 604 | 79 | 41 |
| 128 | 638 | 600 | 80 | 28 |

**open**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0 | 634 | 610 | 79 | 28 |
| 4 | 629 | 601 | 67 | 31 |
| 8 | 626 | 604 | 93 | 42 |

**cvisit**

| value | mean | median | std | n |
|---|---|---|---|---|
| 400 | 633 | 606 | 82 | 32 |
| 600 | 633 | 626 | 79 | 24 |
| 800 | 629 | 606 | 67 | 20 |
| 1000 | 620 | 601 | 96 | 25 |

**cscale**

| value | mean | median | std | n |
|---|---|---|---|---|
| 40 | 632 | 613 | 68 | 14 |
| 70 | 644 | 624 | 77 | 32 |
| 100 | 630 | 612 | 79 | 25 |
| 130 | 611 | 598 | 93 | 30 |

**m**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 626 | 606 | 80 | 22 |
| 12 | 630 | 622 | 88 | 28 |
| 16 | 618 | 594 | 77 | 22 |
| 20 | 638 | 612 | 82 | 29 |

**peak_rung distribution** (which rung the peak landed on)

| rung | count |
|---|---|
| 100 | 11 |
| 400 | 30 |
| 1500 | 30 |
| 4000 | 30 |

## Recommendation for the next round

Auto-generated from the tables above: for each axis, ranked by out-of-fold permutation importance, the univariate best-performing level and a read on whether the effect clears the noise floor. Coupled axes (see note above) are merged into one entry.

| Axis | Best level (highest mean peak Elo) | OOF perm. importance | Read |
|---|---|---|---|
| l2 | l2=0.0 (mean 721) | 1.346 | clear effect -- fix at the best level |
| replaywarm (= replaycap) | replaywarm=128 (mean 671) | 0.050 | clear effect -- fix at the best level |
| cscale | cscale=70 (mean 644) | 0.037 | clear effect -- fix at the best level |
| batch | batch=128 (mean 638) | 0.014 | weak/marginal effect |
| lr | lr=0.003 (mean 634) | 0.012 | weak/marginal effect |
| open | open=0 (mean 634) | 0.008 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| m | m=20 (mean 638) | -0.001 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| cvisit | cvisit=600 (mean 633) | -0.002 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| sims | sims=400 (mean 630) | -0.008 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |

## Caveats

- **n=101, single seed per draw.** Per-draw Elo sits inside this project's documented 50-150 Elo seed-noise band, so the target variable itself is noisy; the CV R2 above is the honest ceiling on how much of that noise these models can actually explain, not just the training-set fit.
- **Not a controlled factorial.** Most axes were drawn independently per draw, which keeps them roughly uncorrelated in expectation (unlike a hand-picked grid), but 101 draws over this many axes still leaves each importance estimate wide. Treat rankings as directional, not precise. Exception: `replaycap`/`replaywarm` are perfectly coupled in this sweep (see the Feature importance note), so they are not independent of each other.
- **Predicts peak Elo, not final-rung Elo.** The target is the max over each draw's 4-rung ladder, so `peak_rung` is an output of the search, not a controlled input -- do not read a feature's importance here as telling you which rung to train to.
- **Architecture/init scope.** Fit only on linear, from-scratch checkpoints (this cohort's only architecture). Extrapolating these axis preferences to an MLP or a warm-started init is untested.
- **Correlational.** As with every observation in the results doc this predictor is built from, nothing here isolates a single axis's causal effect. Use it to prioritize where a controlled follow-up sweep should look, not as a substitute for one.
