---
name: incremental-mlp-and-sparsity
description: "Shipped NNUE-style incremental MLP mu head + bit-identical sparse leaf-tail forward, 12.7x total for the wide head"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9b2197c9-c8a4-4a00-98c2-319283f178f1
  modified: 2026-07-22T11:03:50.030Z
---

2026-07-22: shipped the NNUE-style first-hidden accumulator for MLP `dist` mu heads
(`g_mlAccDim`/`g_mlAccVec`/`g_mlL0ByInput`, the vector generalization of `g_mlAcc`), then
same session, a sparse leaf-tail forward on top. Combined per-node speedup for the wide
head (256/128) at fixed depth: full-scan 36.1 -> accumulator-only 20.2 (1.78x) ->
**+ sparse tail 2.84 us/node (12.7x total)**.

**Key finding (theory 36, confirmed):** a new `train.exe mlp-sparsity` measurement showed
the dist MLP heads are **~90% dead-ReLU per position** with only **~10-12% activation
churn per move** -- NOT the dense ~50%-active heads assumed going in. That motivated the
**sparse leaf-tail forward**: `MLPModel::forwardFromHidden` sums each remaining layer only
over its nonzero inputs, bit-identical (adding `0*w` never changes a float sum, tests pass
unchanged), realizing the predicted ~8-9x second-layer ceiling (measured 7.1x). d6/nb200k
is now affordable for the wide head (~0.57 s/move, down from ~7.2 s full-scan).

A literal cross-move delta-accumulator (maintain 2nd-layer pre-activations, undo on
unmake, propagate only changed-unit deltas) was considered and NOT built: an op-count
showed it would be both slower here (pays an update AND undo every make; side-to-move
flips every ply, perturbing many units, so "changed" isn't much smaller than "live") and
more complex than just recomputing the already-sparse tail per leaf. Only worth it if a
future head's second layer is much wider than its live-unit count.

**Decision/context:** the d6 MLP rating was NOT actually blocked by the first change --
it was already done the prior session (720+ games/agent: dist_lin 1031 > MLPs 974/967/931,
all below the champion; theory 27 holds). The incremental agent has the same content-hash
ID + eval-equivalent output, so those numbers carry over; both changes are pure speed. A
full multi-seed 32-game d6 re-confirmation is now affordable and filed as `[Next]` in
todo.md, not yet run. See [[position-oracle-campaign]] and
`plans/nnue-incremental-mlp-results-1-crystalline-taco.md`.
