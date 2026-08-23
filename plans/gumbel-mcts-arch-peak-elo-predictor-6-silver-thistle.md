# Peak-elo predictor: decision tree / feature importance

Source: `plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv`, 92 draws (one row per `block`, collapsing each draw's checkpoint ladder to its peak `elo`).
Purpose: identify, per swept config axis, which values tend to produce the highest elo, to bias the next round's sweep ranges rather than searching from scratch. This is descriptive/exploratory, not a certification -- see Caveats.

## Method

Three models fit on the same 11 swept axes -> peak elo, compared by 5-fold cross-validated R2 (`sklearn.model_selection.KFold`, shuffle, seed 0):

| Model | CV R2 (mean +/- std) | Why fit it |
|---|---|---|
| Decision tree (depth=3) | 0.571 +/- 0.146 | Human-readable rules; captures interactions and non-monotonic effects without assuming a functional form |
| Random forest (500 trees) | 0.697 +/- 0.163 | Averages many trees for a more stable importance ranking than any single tree's splits |
| Linear (standardized) | 0.518 +/- 0.094 | Baseline: if this is much worse than the tree/RF, the axes' effects are not additive/monotonic |

Tree depth chosen by CV R2 over depths 1=0.029, 2=0.450, 3=0.571, 4=0.554 (min_samples_leaf=8 throughout, to keep leaves large enough to average out per-draw seed noise).

## Decision tree rules

Fit on all 92 draws (the CV split above is for model comparison only; this tree is refit on the full dataset for interpretation):

```
|--- l2 <= 0.00015
|   |--- modeltype_mlp <= 0.5
|   |   |--- lr <= 0.02
|   |   |   |--- value: [725.1]
|   |   |--- lr >  0.02
|   |   |   |--- value: [692.5]
|   |--- modeltype_mlp >  0.5
|   |   |--- value: [917.2]
|--- l2 >  0.00015
|   |--- lr <= 0.02
|   |   |--- replaywarm <= 80
|   |   |   |--- value: [704.7]
|   |   |--- replaywarm >  80
|   |   |   |--- value: [761.6]
|   |--- lr >  0.02
|   |   |--- l2 <= 0.00065
|   |   |   |--- value: [699.9]
|   |   |--- l2 >  0.00065
|   |   |   |--- value: [627.7]
```

Rendered diagram: `plans/gumbel-mcts-arch-peak-elo-tree-6-silver-thistle.png`.

## Feature importance

**Note:** `replaycap` and `replaywarm` were not drawn independently in this sweep (Spearman rho=1.00 across all 92 draws -- every draw's value of one determines the other). Their importances below describe one underlying axis counted twice, not two independent effects.

RF impurity importance (in-sample, from the full-data fit) vs. out-of-fold permutation importance (mean R2 drop when a feature is shuffled in a held-out fold, averaged over the 5 folds -- the more trustworthy of the two since it is evaluated out-of-sample):

| Feature | RF impurity importance | OOF permutation importance (mean +/- std) |
|---|---|---|
| l2 | 0.323 | 0.927 +/- 0.175 |
| modeltype_mlp | 0.332 | 0.693 +/- 0.272 |
| lr | 0.090 | 0.096 +/- 0.045 |
| replaycap | 0.035 | 0.035 +/- 0.012 |
| replaywarm | 0.036 | 0.034 +/- 0.017 |
| batch | 0.037 | 0.033 +/- 0.022 |
| open | 0.031 | 0.018 +/- 0.010 |
| cscale | 0.026 | 0.017 +/- 0.022 |
| cvisit | 0.032 | 0.007 +/- 0.007 |
| sims | 0.019 | 0.003 +/- 0.002 |
| m | 0.038 | -0.001 +/- 0.009 |

## Linear regression (standardized coefficients)

elo change per +1 standard deviation of the feature, holding the others fixed (R2=0.518, see caution above about how much this linear fit actually explains):

| Feature | Std. coefficient (elo / 1 sd) |
|---|---|
| replaycap | +269.4 |
| replaywarm | -244.7 |
| l2 | -53.9 |
| lr | -37.0 |
| modeltype_mlp | +35.4 |
| batch | -15.5 |
| sims | +10.2 |
| open | +9.7 |
| cvisit | +3.8 |
| cscale | -2.1 |
| m | -1.1 |

## Univariate bucket means (no model, raw data)

Every swept axis here is a small discrete grid, so the plain per-value mean peak `peak_elo` is itself readable, with no model in the loop:

**sims**

| value | mean | median | std | n |
|---|---|---|---|---|
| 300 | 737 | 727 | 89 | 38 |
| 400 | 734 | 708 | 87 | 27 |
| 500 | 751 | 726 | 123 | 27 |

**lr**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.003 | 770 | 735 | 89 | 27 |
| 0.01 | 761 | 742 | 104 | 34 |
| 0.03 | 691 | 683 | 83 | 31 |

**l2**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 805 | 762 | 118 | 33 |
| 0.0003 | 730 | 719 | 64 | 28 |
| 0.001 | 681 | 676 | 53 | 31 |

**replaycap**

| value | mean | median | std | n |
|---|---|---|---|---|
| 500 | 697 | 680 | 71 | 28 |
| 2000 | 738 | 724 | 96 | 31 |
| 8000 | 779 | 752 | 108 | 33 |

**replaywarm**

| value | mean | median | std | n |
|---|---|---|---|---|
| 16 | 697 | 680 | 71 | 28 |
| 32 | 738 | 724 | 96 | 31 |
| 128 | 779 | 752 | 108 | 33 |

**batch**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 741 | 735 | 87 | 33 |
| 32 | 757 | 728 | 102 | 31 |
| 128 | 720 | 702 | 107 | 28 |

**open**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0 | 738 | 717 | 105 | 24 |
| 4 | 716 | 706 | 87 | 37 |
| 8 | 771 | 746 | 102 | 31 |

**cvisit**

| value | mean | median | std | n |
|---|---|---|---|---|
| 400 | 748 | 713 | 106 | 21 |
| 600 | 722 | 714 | 91 | 26 |
| 800 | 744 | 704 | 101 | 21 |
| 1000 | 750 | 734 | 103 | 24 |

**cscale**

| value | mean | median | std | n |
|---|---|---|---|---|
| 40 | 720 | 707 | 92 | 20 |
| 70 | 756 | 719 | 112 | 31 |
| 100 | 744 | 735 | 95 | 31 |
| 130 | 721 | 683 | 82 | 10 |

**m**

| value | mean | median | std | n |
|---|---|---|---|---|
| 8 | 737 | 683 | 117 | 25 |
| 12 | 742 | 712 | 103 | 24 |
| 16 | 737 | 728 | 88 | 24 |
| 20 | 747 | 735 | 87 | 19 |

**modeltype_mlp**

| value | mean | median | std | n |
|---|---|---|---|---|
| 0.0 | 705 | 707 | 45 | 46 |
| 1.0 | 776 | 744 | 123 | 46 |

**peak_rung distribution** (which rung the peak landed on)

| rung | count |
|---|---|
| 100 | 1 |
| 400 | 19 |
| 1500 | 25 |
| 4000 | 47 |

## Recommendation for the next round

Auto-generated from the tables above: for each axis, ranked by out-of-fold permutation importance, the univariate best-performing level and a read on whether the effect clears the noise floor. Coupled axes (see note above) are merged into one entry.

| Axis | Best level (highest mean peak_elo) | OOF perm. importance | Read |
|---|---|---|---|
| l2 | l2=0.0 (mean 805) | 0.927 | clear effect -- fix at the best level |
| modeltype_mlp | modeltype_mlp=1.0 (mean 776) | 0.693 | clear effect -- fix at the best level |
| lr | lr=0.003 (mean 770) | 0.096 | clear effect -- fix at the best level |
| replaycap (= replaywarm) | replaycap=8000 (mean 779) | 0.035 | clear effect -- fix at the best level |
| batch | batch=32 (mean 757) | 0.033 | clear effect -- fix at the best level |
| open | open=8 (mean 771) | 0.018 | weak/marginal effect |
| cscale | cscale=70 (mean 756) | 0.017 | weak/marginal effect |
| cvisit | cvisit=1000 (mean 750) | 0.007 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| sims | sims=500 (mean 751) | 0.003 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |
| m | m=20 (mean 747) | -0.001 | no detectable effect at this sample size -- fine to fix at any convenient/cheap value |

## Caveats

- **n=92, single seed per draw.** Per-draw Elo sits inside this project's documented 50-150 Elo seed-noise band, so the target variable itself is noisy; the CV R2 above is the honest ceiling on how much of that noise these models can actually explain, not just the training-set fit.
- **Not a controlled factorial.** Most axes were drawn independently per draw, which keeps them roughly uncorrelated in expectation (unlike a hand-picked grid), but 92 draws over this many axes still leaves each importance estimate wide. Treat rankings as directional, not precise. Exception: `replaycap`/`replaywarm` are perfectly coupled in this sweep (see the Feature importance note), so they are not independent of each other.
- **Predicts peak elo, not final-rung elo.** The target is the max over each draw's 4-rung ladder, so `peak_rung` is an output of the search, not a controlled input -- do not read a feature's importance here as telling you which rung to train to.
- **Architecture/init scope.** `modeltype` is included as a feature (one-hot, mlp vs conv), but the architecture-specific width axis (`--mlp-hidden` / `--conv-channels`) is excluded -- its values are not comparable across the two model types, see the results doc's separate per-architecture width table instead.
- **Correlational.** As with every observation in the results doc this predictor is built from, nothing here isolates a single axis's causal effect. Use it to prioritize where a controlled follow-up sweep should look, not as a substitute for one.
