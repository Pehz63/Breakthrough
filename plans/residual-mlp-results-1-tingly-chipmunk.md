# Residual / skip-connection value head + MLP inner arm -- Results

Companion to `residual-mlp-plan-1-tingly-chipmunk.md`. Session date 2026-07-13.

## What shipped

A frozen chip-count **skip connection** for learned value heads, plus a
hand-written **MLP** inner arm that rides on it, both plugged into the existing
model/eval/trainer/ranking machinery.

- `MLPModel` (`src/ml_model.cpp/.h`): fully-connected, 1-2 hidden layers, ReLU
  hidden + linear output logit. Hand-written `forward` and backprop (`trainStep`).
  Fan-in-scaled `initRandom` (zero-init would leave hidden units symmetric).
  Mutable act/pre scratch reused per call (no per-node allocation). NOT
  incrementally updatable -> uses the full-scan `mlValueScore` leaf.
- `ResidualModel` (`src/ml_model.cpp/.h`): wraps `float skipW` + an owned inner
  `Model*`. `forward = skipW*matDiffFromFeatures(x,featVer) + inner->forward(x)`.
  The skip is FROZEN (the wrapper owns no trainable weights); `trainStep` trains
  only the inner, passing the skip in as a GLM `offset` on the logit. Inner can be
  a `LinearModel` (stays fully incremental) or an `MLPModel` (full-scan).
- `Model::trainStep(x,n,target,lr,l2,offset)` virtual (base no-op) + an `offset`
  arg on `LinearModel::sgdLogisticStep`, computed inline so a subclass `forward`
  is never double-counted. `matDiffFromFeatures(x,featVer)` reads raw white-minus-
  black count from a feature vector (v2 = sum white sq - sum black sq; v1 =
  feature0*16), matching `g_chipDiff` units so the skip is consistent across the
  full-scan and incremental leaf paths.
- Serialization: `type=mlp` (`layers=` + indexed `l<k>w`/`l<k>b`) and
  `type=residual` (`skip_weight=` + `inner_type=` + the inner's weight block
  inline via a new `Model::writeWeights` hook). `loadModel` refactored into
  `buildLinearFromKV`/`buildMLPFromKV` helpers reused by the direct and residual
  cases. `g_modelTypes[]`: linear/mlp/residual implemented.
- Incremental path (`src/ml_eval.cpp`): `mlIncrementalBegin` unwraps a
  `ResidualModel` (latches `skipW` into a new `g_mlSkipW`, treats the inner as the
  model); a linear v2 inner stays incremental, an MLP inner fails the
  `dynamic_cast<LinearModel*>` and returns false so the leaf falls back to
  full-scan `mlValueScore` (correct, just not incremental). `mlLeafScore` adds
  `g_mlSkipW * g_chipDiff` (the chip diff is already maintained every make/unmake,
  so the skip is exact and nearly free).
- Trainer (`src/ml_train.cpp`): `trainSupervisedValue` now holds a `Model*` and
  trains generically via `trainStep`. New params `modelType` (linear|mlp),
  `mlpHidden`, `residualSkip` (0 off / >0 fixed / <0 auto-calibrate via
  `calibrateMaterialSkip`, a material-only logistic pre-fit). Prints a
  **stratified logistic loss by |matDiff| bucket {0,1,>=2}** for every model --
  the direct theory-24 equal-material calibration measure. CLI flags
  `--model-type`/`--mlp-hidden`/`--residual-skip` on `selfplay-supervised`.
- Sweep (`tools/sweep_pst_v2.ps1`) generalized in place (not duplicated): a
  candidate's `Args` is a raw train.exe arg array, so the new flags drop in with
  no scaffolding change. Added `-Groups` selection (default all; only selected
  groups consume slots), an optional per-candidate `Wrapper` (cheaper shell for
  full-scan MLP), groups **F** (linear residual baseline) and **G** (MLP capacity
  comparison), stratified-loss capture from train stdout into the report
  (`Loss0/1/2`), and `-NoRate` (train + report loss only; skip the expensive pool
  rating + permanent roster/matches append).

### How to test
- `.\tools\run_tests.ps1 -Build` -- 739 assertions / 77 cases pass, including the
  MLP finite-difference gradient check, residual fold-equivalence, and the
  residual+linear incremental walk.
- Train a residual-MLP: `.\train.exe selfplay-supervised --feature-version 2
  --from-data <replay> --model-type mlp --mlp-hidden 32 --residual-skip -1
  --out models/sweep/x` -- watch the `Model: type=residual ... skip=... (auto)`
  line and the `Stratified loss by |matDiff|:` printout.
- Sweep the theory-24 comparison cheaply: `.\tools\sweep_pst_v2.ps1 -Groups "F,G"
  -NoRate` (training + stratified loss, no rating). Drop `-NoRate` (and add
  `-Workers K`) for the pooled Elo.

### Commit message
`Add residual chip-skip value head + hand-written MLP inner arm; generalize model sweep (theory 24)`

## Results

### Tests
739 assertions in 77 test cases pass. The load-bearing new ones: a central
finite-difference gradient check on a 3-4-1 MLP (validates the hand-written
backprop weight-by-weight), residual+linear fold-equivalence (a hard skip over a
linear inner equals a plain linear with the skip folded into the piece-square
weights), and the residual+linear incremental walk (incremental `mlLeafScore`
matches full `mlValueScore` within 1 over a make/unmake tree).

### Manual sanity (single seed 1001, 3000-game replay extract, 6 epochs)
Same data + seed across all four architectures; only the architecture differs.
Stratified logistic loss (lower = better calibrated); `==0` is the equal-material
bucket theory 24 targets.

| model | skip (auto) | loss ==0 | loss ==1 | loss >=2 | overall |
|---|---|---|---|---|---|
| linear plain    | -    | 0.783 | 0.552 | 0.366 | 0.593 |
| linear residual | 0.597 | **0.758** | 0.536 | 0.372 | 0.591 |
| mlp(32) plain    | -    | 0.471 | 0.266 | 0.120 | 0.383 |
| mlp(32) residual | 0.964 | 0.485 | 0.270 | 0.138 | 0.376 |

Preliminary and underpowered (one seed, light training), but the two arms already
diverge: the **linear** residual lowers equal-material loss (0.758 vs 0.783) as
theory 24 predicts, while the **MLP** residual RAISES it (0.485 vs 0.471) even as
its overall loss falls -- i.e. a high-capacity model handed a frozen material term
does not automatically calibrate equal-material positions better. This is the
contrast the seed-replicated sweep exists to confirm.

### Sweep (groups F + G, `-NoRate`, seeds 1001/2002, 8000-game replay)
Seed-replicated equal-material (`==0`) logistic loss, 190666 positions/bucket.
Auto-calibrated skip on this data = 1.438 (all residual cells).

| group | arch | skip | seed 1001 | seed 2002 | mean ==0 | seed spread |
|---|---|---|---|---|---|---|
| F | linear   | off  | 0.832 | 0.653 | 0.742 | 0.179 |
| F | linear   | auto | 0.639 | 0.649 | **0.644** | **0.010** |
| G | mlp(16)  | off  | 0.536 | 0.553 | 0.544 | 0.017 |
| G | mlp(16)  | auto | 0.562 | 0.541 | 0.552 | 0.021 |
| G | mlp(32)  | off  | 0.533 | 0.527 | 0.530 | 0.006 |
| G | mlp(32)  | auto | 0.523 | 0.530 | 0.526 | 0.008 |

Two clear results:
1. **Linear inner: the residual skip helps, and stabilizes.** It lowers mean
   equal-material loss ~0.10 (0.644 vs 0.742) AND cuts the seed-to-seed spread
   ~18x (0.010 vs 0.179) -- the plain linear had a fragile bad seed (0.832) the
   residual eliminates. It also improves the `==1` and `>=2` buckets. This is
   theory 24's prediction, confirmed at the linear level.
2. **MLP inner: the residual is a wash.** |plain - residual| on `==0` is <= 0.008
   at both widths, inside the seed spread and pointing both directions
   (mlp16 slightly worse, mlp32 slightly better). A high-capacity model already
   fits material, so freezing it frees no useful capacity. The single-seed manual
   run's "MLP residual hurt" (0.485 vs 0.471) was seed noise -- it does not
   survive replication.

The bigger calibration lever is capacity, not the skip: every MLP cell (~0.53 on
`==0`) beats every linear cell (0.64-0.74), skip or no skip -- consistent with
theory 10 (linear PST is the binding ceiling). Pooled Elo was intentionally not
measured (`-NoRate`); per theory 22 the tie-breaking Elo signal is expected small,
and the search-free stratified loss is the on-target measure for theory 24.

## Implementation notes / differences from the plan

- Design changed from the plan's first draft (a `ResidualLinearModel` subclass) to
  a general `ResidualModel` wrapper once the MLP arm was in scope, so one wrapper
  serves both a linear and an MLP inner. The single `dynamic_cast<LinearModel*>`
  in the codebase (ml_eval) now unwraps the residual first.
- The skip lives in the LOGIT (a GLM offset), not post-tanh, because training is
  logistic regression on the same logit inference squashes with tanh -- adding it
  after the squash would break both the training target and the eval bound.
- Skip weight is auto-calibrated (material-only logistic pre-fit) rather than a
  magic constant; calibration is deterministic so it does not perturb the
  weight-init RNG, and the plain-linear path is byte-identical to before (existing
  linear models reproduce).
- MLP is deliberately full-scan (NNUE incrementality is a separate `[Later]`
  item). The full-scan leaf was already the fallback for v1/non-linear models, so
  no engine change was needed beyond routing.
- Added `-NoRate` to the sweep (not in the original plan): the pooled rating of 12
  new agents (8 full-scan MLP) against the 63-agent pool is expensive and appends
  permanently to `matches.jsonl`, while the primary theory-24 evidence
  (stratified loss) is search-free. `-NoRate` gets the calibration data cheaply
  and reversibly.

## Correctness gotchas
- Backprop must accumulate the previous layer's gradient using the PRE-update
  weight, then update the weight -- done in one pass by reading `gPrev[i] +=
  gj*wrow[i]` before `wrow[i] -= ...`. The finite-diff test is the guard.
- `sgdLogisticStep` computes the logit inline instead of calling the virtual
  `forward`; otherwise a `ResidualModel`/subclass override would add the skip
  twice during training.
- `trainStep` default args live only on the base `Model` signature; call it
  through a `Model*` (5 args, offset defaults) -- a concrete-type call needs all 6.

## Measurement caveats
- Stratified loss is on the TRAINING set (calibration/fit), not a held-out split;
  it measures how the model represents equal-material positions, not
  generalization. A validation split would strengthen it (see Future Work).
- MLP Elo is expensive to measure (full-scan search); the sweep screens MLP cells
  at a cheaper wrapper, and per theory 22 the Elo signal from tie-breaking is
  expected to be small regardless.

## Future Work
- **Elo of these models against the pool (tethered to the confirmed linear
  result).** The F/G sweep ran `-NoRate`, so the residual's calibration win at the
  linear level has no strength number attached. Re-run without `-NoRate` (or
  gauntlet the residual-linear cell) to see whether the ~0.10 equal-material loss
  gain and the seed-stability translate to Elo -- theory 22 predicts little, and
  that contrast (calibration up, Elo flat) would itself be the finding.
- **Held-out calibration (tethered to the stratified-loss measure).** The loss is
  in-sample; a train/validation split would test whether the residual improves
  equal-material GENERALIZATION, not just fit -- the theory-24 claim is really
  about the former.
- **Soft/regularized skip (theory 24 Q1 alternative).** This task did the HARD
  frozen skip. A learnable skip regularized toward the material fit would test
  whether letting the material weight drift slightly helps, at the cost of a
  regularization hyperparameter.
- **Elo of the residual-MLP at fixed compute (tethered to the standing dethrone
  goal).** Full-scan MLP is slow; whether a residual MLP can approach the champion
  at equal wall-clock is untested and needs the NNUE incremental path first.
- **Deeper/2-hidden-layer MLP + wider sweep.** Group G tries hidden {16,32}; the
  capacity at which the residual starts helping (if it does) is unmapped.

## Ideas This Inspired
- NNUE as the natural next step: the linear inner already rides the incremental
  `g_mlAcc` accumulator, and an MLP first layer is exactly an NNUE accumulator --
  the residual skip would sit on top unchanged.
- A "material-free" diagnostic: train an inner with the skip frozen at the
  material-optimal weight, then inspect what the residual learned (which squares /
  patterns move the eval once material is removed) as an interpretability probe.
- Per-phase skip: the optimal material weight may differ by game phase (cf. theory
  25 mixture-of-experts); a phase-conditioned skip is a cheap thing to try.
- Apply the stratified-loss-by-|matDiff| readout to the existing linear PST models
  as a retroactive calibration audit of the current champion-tying learned agents.
