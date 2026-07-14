# Residual/MLP follow-up: Elo + validation + documentation -- Results

Companion to `residual-mlp-plan-2-tingly-chipmunk.md`. Session date 2026-07-14.
Follows `residual-mlp-results-1-tingly-chipmunk.md` (the initial 2-seed offline
result). This doc records the strength (Elo) measurement, a held-out re-check, and
the documentation the developer asked for.

## What this follow-up added

- **Validation split + early stopping** (`src/ml_train.cpp`, `tools/train_main.cpp`):
  `selfplay-supervised --val-split <f>` holds out that fraction of positions
  (deterministic under the run seed), trains on the remainder, prints per-epoch
  `val=` loss, and computes the final `Stratified loss by |matDiff|` on the HELD-OUT
  set (so the theory-24 equal-material measure is a generalization number, not fit).
  `--early-stop` keeps the lowest-validation-loss epoch as the saved model. Default
  off, so existing runs are byte-identical.
- **Sweep generalization** (`tools/sweep_pst_v2.ps1`): `-Seeds` list (default ~6
  training seeds) drives the number of seed replicas for the linear residual-skip
  group (F) and the MLP capacity group (G); `-RateOnly` skips training and rates the
  slot files an earlier `-NoRate` pass left on disk, so the cheap training and the
  expensive rating are separate phases.
- **Documentation** (developer-requested): a "worked example: the MLP value agent"
  in `ML.md` (assembling inputs, layers, output, training, search wrapper, Elo); the
  canonical agent description in `residual-mlp-plan-2`; a `Docs/benchmarking.md`
  subsection; and a CLAUDE.md standing instruction ("a new model or agent is not done
  until its Elo is measured and it is documented", with the full-run + ~6-training-
  seed rule).

### How to test
- `.\tools\run_tests.ps1 -Build` -- 742 assertions / 78 cases pass (adds a val-split
  end-to-end test).
- `.\train.exe selfplay-supervised --feature-version 2 --from-data <replay>
  --val-split 0.2 --early-stop` -- prints `val=` per epoch, a held-out stratified
  loss, and an "Early stopping: kept epoch N" line.
- `.\tools\sweep_pst_v2.ps1 -Groups "F,G" -NoRate` (train ~6 seeds, offline
  calibration), then `-RateOnly -Wrapper <head> -MlpWrapper <head> -Workers K` per
  head (d4 then d6) for the pooled Elo.

## The model + agent measured

(Canonical description; also in `ML.md` and `residual-mlp-plan-2`.) A value model
`MLPModel`: 129 v2 sparse piece-square inputs (64 White + 64 Black piece-presence +
side-to-move) -> fully-connected, one hidden layer of width 16 or 32 (the code
supports more), ReLU hidden, linear output -> `tanh*900` eval. Optionally wrapped by
a frozen chip-count skip (`ResidualModel`). The AGENT is alpha-beta search (depth 4
and depth 6, nb200k, tt + ordering) using the model as the leaf evaluator. Trained
by supervised value regression on outcomes from a `rank.exe extract` replay of the
rated pool's match history (8000-game sample, 6 epochs, lr 0.05), across ~6 training
seeds.

## Results

### 1. 6-seed calibration corrects the 2-seed result (theory 24 weakens)

Equal-material (`==0`) held-in stratified loss, 6 training seeds (1001..6006) per
recipe, 8000-game replay, 190666 positions/bucket.

| recipe | mean ==0 loss | seed spread (min-max) | skip delta vs plain |
|---|---|---|---|
| linear plain     | 0.698 | 0.629 - 0.832 | -- |
| linear residual  | 0.679 | 0.624 - 0.775 | -0.019 |
| mlp(16) plain    | 0.544 | 0.533 - 0.561 | -- |
| mlp(16) residual | 0.554 | 0.541 - 0.568 | +0.010 |
| mlp(32) plain    | 0.522 | 0.510 - 0.533 | -- |
| mlp(32) residual | 0.528 | 0.521 - 0.545 | +0.006 |

Two findings, both correcting or sharpening results-1:
1. **The residual skip's calibration effect is within seed noise at every capacity.**
   The linear skip delta (-0.019) sits inside a ~0.18-0.20 seed spread; the MLP skip
   deltas are tiny and POSITIVE (slightly worse). Results-1's headline -- the linear
   residual lowers `==0` loss ~0.10 (0.742 vs 0.644) and "confirms theory 24 at the
   linear level" -- was largely a 2-seed artifact: with 6 seeds the gap collapses to
   0.019, within noise. (At seed 6006 the plain and residual linear models converge
   to nearly identical `==0` loss, 0.7749 vs 0.7749, which makes sense: a linear v2
   model can already express material, so the skip only nudges the optimization
   path.) Theory 24's skip claim does not survive proper seeding.
2. **Capacity is the real calibration lever, and it is also more seed-stable.**
   linear ~0.69 -> mlp(16) ~0.55 -> mlp(32) ~0.52, and the MLP seed spreads
   (0.01-0.03) are ~10x tighter than linear (0.20). This is the theory-10 story
   (linear PST is the binding ceiling): the win is from breaking it with capacity,
   not from the material skip.

This is a direct payoff of the developer's ~6-seed rule -- a 2-seed comparison in
this project's training-seed-noise band (50-150 Elo, theory 8) can invent an effect
that is not there.

**Held-out confirmation (Part C).** Retraining linear plain vs residual with
`--val-split 0.2 --early-stop` (255503 train / 63875 held out) gives held-out `==0`
loss 0.6267 (plain) vs 0.6268 (residual) -- identical to 4 decimals. The skip does
nothing for equal-material calibration on held-out data either. Side note: early
stopping kept epoch 1 for both, i.e. the linear model overfits after one epoch on
320k positions, so the fixed 6-epoch cap was training past the validation optimum
(the reason the "validation split + early stopping" todo item existed).

### 2. Elo (primary): full-roster run at depth 4

Pooled Elo from a full Bradley-Terry refit over the whole active roster (63 agents +
36 new IDs rated together), 2 games/pair, 6 training seeds per recipe, all at the
same head `ab(d4,tt,ord,nb200k)`. (At depth 4 the tree cap binds before the node
budget, so the full-scan MLP rates fast -- games were 0.0-0.3s.)

| recipe | mean Elo | seed range | mean ==0 loss (sec.1) |
|---|---|---|---|
| linear plain     | 759 | 723 - 789 | 0.698 |
| linear residual  | 753 | 687 - 804 | 0.679 |
| mlp(16) plain    | 655 | 611 - 734 | 0.544 |
| mlp(16) residual | 695 | 661 - 774 | 0.554 |
| mlp(32) plain    | 645 | 604 - 687 | 0.522 |
| mlp(32) residual | 659 | 631 - 691 | 0.528 |

Two results, one of them the headline of this whole follow-up:

1. **Better offline calibration did NOT become strength -- it inverted.** The MLP
   calibrates equal-material positions far better than the linear model (loss ~0.52
   vs ~0.69), yet it rates ~60-110 Elo LOWER in actual play (mlp(32) ~652, mlp(16)
   ~675, linear ~756), in the SAME depth-4 search shell. Lower outcome-prediction
   loss produced a WEAKER search evaluator, and more capacity (16 -> 32 hidden)
   lowered loss further while lowering Elo further. Likely causes: the MLP overfits
   the noisy outcome labels (early stopping showed even the linear model overfits
   after 1 epoch on 320k positions, and these MLPs trained 6 epochs), and a sharper
   loss-optimal evaluator is not the same as a better move-ranker for alpha-beta.
   This is exactly the divergence the developer's "always measure Elo" rule exists
   to catch: reported on calibration alone (results-1), the MLP looked like a big
   win; on Elo it is a regression.
2. **The residual skip still shows no reliable Elo effect.** Linear -6, mlp(16) +40,
   mlp(32) +14 -- all inside the ~100-160 seed spread (SE ~25). Consistent with the
   calibration wash and theory 22.

For scale, the reigning champion is ~1140. None of these value-model-in-search
agents approach it: the best is a plain linear seed at 789. The learned value head,
at this recipe and depth, does not beat the hand-crafted chip counter.

**Depth 6 (follow-up):** the developer convention rates at d4 AND d6. At d6 the node
budget binds for the full-scan MLP, so it is a multi-hour run left as a documented
follow-up; the d4 pattern (MLP weaker than linear, skip no help, all below the
champion) is unlikely to reverse. Command:
`.\tools\sweep_pst_v2.ps1 -Groups "F,G" -RateOnly -RateGames 2 -Workers 12
-Wrapper "ab(d6,tt,ord,nb200k)@1" -MlpWrapper "ab(d6,tt,ord,nb200k)@1"`.

## Implementation notes / caveats

- Early stopping reloads the best-validation checkpoint from disk at the end so the
  final report and datastore emit reflect the kept model (a minor float round-trip).
- The skip weight is auto-calibrated on ALL training positions (a 1-parameter
  material fit), not the train split only; the leakage into a held-out learned-head
  measure is negligible (one scalar) but noted.
- MLP Elo at a fixed node budget is a LOWER bound on an incremental (NNUE) version at
  equal wall-clock -- the full-scan cost is per-second, not per-node eval quality.

## Future Work
- Held-out generalization of the calibration finding across the full ~6 seeds (Part
  C), tethered to whether the linear residual edge survives seeding at all.
- NNUE-style incremental MLP so the MLP competes at fixed wall-clock (todo.md).
- Residual skip design space: soft/regularized skip, broader baseline, wider/deeper
  MLP (todo.md).
- Low-Elo-games-as-low-quality-data study (theory 26, todo.md).

## Ideas This Inspired
- The 2-seed-vs-6-seed correction suggests auditing earlier 2-seed conclusions in
  this project for the same over-statement.
- A per-recipe "seed stability" number (Elo spread over seeds) may itself be a useful
  agent property to track, not just a nuisance to average out.
