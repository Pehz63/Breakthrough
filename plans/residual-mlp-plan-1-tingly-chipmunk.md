# Residual / skip-connection value head + MLP inner arm

## Context

The strongest agents in the pool are plain chip counters, or learned evaluators
only statistically tied with one (Agent Track standing, `todo.md`). Theory 24
(`Docs/theories.md`): a learned value head forced to re-derive material counting
from scratch spends capacity on something already known. Architecturally fixing a
chip-count term as an additive skip connection into the head's output -- so the
model computes `chipCount_skip + learned(board)` instead of `learned(board)`
alone -- frees the learned part to specialize on the residual: distinguishing and
tie-breaking positions with equal or near-equal material, which a pure material
count cannot do.

The todo frames this as the substrate for the MLP/NNUE capacity jump ("the
residual/nonlinear part could BE the MLP arm riding on a fixed linear chip
skip"). Per developer decision this task builds BOTH: the skip substrate AND a
nonlinear MLP inner arm that rides on it. The skip is HARD/frozen (fixed weight,
never trained; the inner model learns only the residual).

At the linear level a chip skip is not new representational capacity (a v2
piece-square model already spans material as uniform +k/-k weights) -- there it is
an inductive-bias intervention. The genuine capacity payoff is the MLP inner arm.
Building both lets us run theory 24's core comparison: a residual-architecture
model vs an unconstrained model of the same total capacity, measured on
equal-material calibration, not just aggregate Elo (which theory 22 warns is
often flat for tie-breaking).

Facts from exploration:
- Value pipeline: `raw = inner logit`; inference maps `tanh(raw)*out_scale` and
  clamps (`mlSquashToEval`, [ml_eval.cpp:45](src/ml_eval.cpp#L45)); training is
  logistic `sigmoid(raw)` vs outcome ([ml_model.cpp:24](src/ml_model.cpp#L24)).
  The skip must live in the **logit** (a GLM offset), not post-tanh.
- The incremental leaf maintains `g_chipDiff` unconditionally, so a linear-inner
  chip skip at a leaf is `skipW * g_chipDiff` -- exact and nearly free.
- One `dynamic_cast<LinearModel*>` exists ([ml_eval.cpp:95](src/ml_eval.cpp#L95)).
  Ranking IDs learned models by file-content hash, no type assertion
  ([ranking.cpp:265](src/ranking.cpp#L265)), so new model types are rankable once
  `loadModel` handles them.
- MLP as a value model is `[Next]` and does NOT require incrementality (NNUE /
  "incrementalize an ML model" are separate `[Later]` items). The MLP plugs into
  the full-scan `mlValueScore` path (already dispatches on feature version and
  calls `m->forward`); `mlIncrementalBegin` returns false for a non-linear model,
  so search falls back to full-scan per leaf -- correct, just not incremental.

## Design

Three model pieces, composed via a wrapper (chosen over a LinearModel subclass
because the skip must wrap BOTH a linear and an MLP inner):

- `LinearModel` (exists) -- inner arm option 1.
- `MLPModel` (new) -- inner arm option 2: fully-connected, 1-2 hidden layers,
  ReLU hidden, linear output logit. Hand-written forward + backprop.
- `ResidualModel` (new) -- wraps `float skipW` + an inner `Model*` + `featVer`.
  `forward(x) = skipW * matDiffFromFeatures(x, featVer) + inner->forward(x)`.
  `outputScale()`/`featureVersion()`/`featureCount()` delegate to inner. The skip
  is frozen: the wrapper owns no trainable params.

`matDiffFromFeatures(x, featVer)`: v2 = `sum(x[0..63]) - sum(x[64..127])`; v1 =
`x[0]*16` (feature 0 is `(wTotal-bTotal)/16`). Returns the RAW white-minus-black
count so skipW is per-piece logit and matches `g_chipDiff` units in every path.

### 1. Model classes (src/ml_model.h / .cpp)
- Add virtual `float trainStep(const float* x, int n, float target, float lr,
  float l2, float offset=0)` to `Model` (default returns 0; overridden below).
  The GLM offset is how the frozen skip enters training.
- `LinearModel`: add optional `offset` to `sgdLogisticStep` (compute `z = offset +
  bias + w.x` INLINE, not via virtual `forward`, so a subclass override is never
  double-counted -- numerically identical for existing offset=0 callers).
  `trainStep` delegates to `sgdLogisticStep`.
- `MLPModel`: layer-sizes vector `[in, h1, (h2,) 1]`; per-layer weight/bias
  arrays; `forward` (ReLU hidden, linear out) storing pre-activations for
  backward; `trainStep` = forward (add `offset` to the output logit) + logistic
  backprop + SGD update with L2. Small scaled random init.
- `ResidualModel`: `trainStep(x,n,t,lr,l2,offset)` = `inner->trainStep(x,n,t,lr,
  l2, offset + skipW*matDiffFromFeatures(x,featVer))`.
- Serialization: `MLPModel::save` writes `type=mlp`, `layers=...`, indexed
  `l<k>w<idx>`/`l<k>b<idx>`. `ResidualModel::save` writes `type=residual`,
  `skip_weight=`, `inner_type=`, then the inner's weight block inline.
- `loadModel`: refactor per-type construction into `buildLinearFromKV` /
  `buildMLPFromKV` helpers reused by both the direct case and the residual case
  (residual reads `inner_type` and builds the inner from the same kv map).
- `g_modelTypes[]`: mark `mlp` implemented=true; add a `residual` row
  (implemented=true). `nnue`/`transformer` stay false.

### 2. Incremental path (src/ml_eval.cpp)
- Add static `g_mlSkipW` (reset in `mlIncrementalEnd`).
- `mlIncrementalBegin`: unwrap `ResidualModel` (latch `skipW`, set `core =
  inner`); then `dynamic_cast<LinearModel*>(core)`. Linear inner (v2) -> latch as
  today + `g_mlSkipW`; MLP inner -> dynamic_cast fails -> return false (full-scan
  fallback, correct).
- `mlLeafScore`: `out = g_mlAcc + (double)g_mlSkipW * g_chipDiff + g_mlStmW*turn`.
  Full-scan `mlValueScore` picks up the skip automatically via virtual `forward`.

### 3. Trainer (src/ml_train.cpp, .h, tools/train_main.cpp)
- Refactor `trainSupervisedValue` to hold a `Model*` and train generically via
  `model->trainStep(...)` (replacing the hardcoded `LinearModel` +
  `sgdLogisticStep`; the datastore/manifest blocks switch to virtual `forward` /
  `outputScale()` / `save`).
- New params: `modelType` (linear|mlp), `mlpHidden` (hidden sizes, e.g. "32" or
  "32,16"), `residualSkip` (0=off, >0 fixed, <0 auto-calibrate via a material-only
  logistic pre-fit `calibrateMaterialSkip(X,Y,featVer)`). Build plain vs residual,
  linear vs mlp inner from these. Defaults reproduce today's behavior exactly.
- Provenance: append e.g. `mlp(32) res(skip=0.40)` to `teacher=` and the manifest
  conditions so variants never share provenance.
- Post-training: print **stratified logistic loss** by `|matDiff|` bucket
  {0,1,>=2} for the trained model -- the direct theory-24 Q3 measurement. Printed
  for every model (plain and residual) so runs are comparable. Cheap, in-memory.
- CLI on `selfplay-supervised`: `--model-type`, `--mlp-hidden`, `--residual-skip`
  (compose with `--feature-version 2`, `--from-data`, dilution, bootstrap). Usage
  lines added.

### 4. Tests (tests/test_ml.cpp)
- MLP forward matches a hand-computed tiny net (2-2-1).
- **Finite-difference gradient check** on a small MLP: perturb each weight by eps,
  confirm the analytic backprop gradient matches numerically. The key safety net
  for hand-written backprop.
- MLP backprop reduces loss; save/load round trip (forward identical).
- `ResidualModel`: forward == skip + inner (linear and mlp inner); save/load.
- **Fold-equivalence** (linear inner): reslinear(skipW, w) == plain linear with
  `w_white += skipW`, `w_black -= skipW` over positions (validates skip inference
  vs existing machinery).
- **Incremental correctness** (residual + linear v2 inner): extend the make/unmake
  walk; incremental `mlLeafScore` matches full `mlValueScore` within 1.
- Residual + MLP inner: `mlIncrementalBegin` stays false; full-scan works.
- `mlValueScore` bounds for mlp and residual models; `trainStep` with nonzero
  offset reduces loss (linear and mlp).

### 5. Hyperparameter sweep -- generalize existing infra, do NOT duplicate
`tools/sweep_pst_v2.ps1` already trains a set of candidates and rates them as one
unified Bradley-Terry pool. Its candidate abstraction is model-agnostic: each is a
`Group`/`Slot`/`Meta`/`Args` object where `Args` is a raw train.exe arg array
trained into `models/sweep/slot<N>.txt`, so the new `--model-type`/`--mlp-hidden`/
`--residual-skip` flags drop into an `Args` array with no structural change.
Generalize it IN PLACE (keep the filename for back-compat; groups A-E keep
working, documented commands still run out of the box):
- Add `-Groups "F,G"` selection (default: all) so the new groups run in isolation
  without re-running A-E.
- Add an optional per-candidate `Wrapper` field (default the global `$Wrapper`),
  folded into the roster id, so full-scan MLP candidates screen at a cheaper
  budget than the linear default without affecting other groups.
- New candidate groups:
  - F (linear residual baseline): plain linear vs `--residual-skip -1`, same
    replay data + seed replicas. Cheap; isolates the skip at the linear level.
  - G (MLP capacity comparison): `--model-type mlp` x hidden {16,32,(64)} x skip
    {off, auto} x seed, same data recipe. Theory-24's residual-vs-unconstrained-
    same-capacity test.
- Capture train.exe's stratified-loss printout (parse stdout the way
  `sweep_pst.ps1` parses `loss=`) into each candidate's report row, so the sweep
  reports the theory-24 calibration measure next to pooled Elo.

Forwards-compat intent: candidates are defined by their train.exe arg array + an
optional wrapper, so future model types (nnue, ...) join by adding a group with
the right flags -- no scaffolding changes.

### 6. Docs (ride along with the commit)
`train.exe docs` regenerates ML.md's model-types table (mlp/residual). Update
`src/CLAUDE.md` (ml_model/ml_eval/ml_train entries), `ML.md` extension example,
`tools/CLAUDE.md` (the generalized sweep_pst_v2.ps1 description), `todo.md`
(strike the residual bullet + the "MLP value model" bullet), and theory 24 in
`Docs/theories.md` (status -> tested, link the results doc).

## Verification / experiment (sequence: build -> tests -> sanity -> sweep)

1. `.\tools\run_tests.ps1 -Build` -- all assertions pass (esp. the finite-diff
   gradient check, fold-equivalence, and residual incremental walk).
2. Quick manual sanity that it works before sweeping: train one residual-MLP cell
   (`--model-type mlp --mlp-hidden 32 --residual-skip -1 --feature-version 2
   --from-data <replay>`), confirm the stratified-loss printout, load it in the
   console `LearnedValue` slot, and confirm a game completes with sane evals.
3. Theory-24 core measurement via the generalized sweep (group G), holding total
   capacity fixed across plain vs residual MLP on the same data/seed. Primary
   evidence is the stratified loss on the `|matDiff|==0` bucket (search-free);
   pooled Elo is secondary (per theory 22, expect a small Elo delta even if the
   calibration prediction holds -- that contrast is itself the finding). Group F
   gives the cheaper linear baseline.
4. The sweep rates candidates against the frozen pool automatically; trim the
   temp roster lines back out afterward (games persist in `matches.jsonl`).

## Out of scope (future work)
- NNUE-style incremental MLP (first-layer accumulator) -- MLP here is full-scan.
- Soft/learnable regularized skip (theory 24 Q1 alternative; this task is hard).
- Broader hand-crafted baseline as the skip (theory 24 Q2 alternative).
