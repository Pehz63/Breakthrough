---
name: incremental-mlp-and-sparsity
description: "Incremental MLP mu head + sparse leaf-tail (12.7x wide head); NNUE-shaped-head follow-up FAILED on speed (O(H) leaf read is the floor)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 9b2197c9-c8a4-4a00-98c2-319283f178f1
  modified: 2026-07-25T00:00:00.000Z
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

**2026-07-25 follow-up -- NNUE-shaped head (`129 -> 512 -> 8 -> 1`, `--mu-hidden "512,8"`),
6 seeds: prediction-neutral but the efficiency premise FAILED (theory 37).** Idea was to
move capacity into the accumulated (free) first layer so the tail collapses. Prediction is
as good as the wide head (held-out MAE 143.5 / NLL 0.408) and Elo lands in the MLP band
(6 seeds 908-1037, mean ~973, top seeds reach dist_lin 1038). But it is ~2x SLOWER per node
than the standard 128,64 head: **2.47 us/node vs 1.24 (std) vs 2.86 (wide)**. Root cause:
the accumulator makes the first-layer UPDATE free, but the leaf still READs + ReLUs all H
first-hidden pre-activations (O(H) scalar, side-to-move column added at the leaf), which is
now the dominant leaf cost -- and the NNUE shape has the LARGEST H. The ~17x-cheaper tail
(MACs) was never the bottleneck. **Lesson: for a scalar leaf, the efficient shape is a
NARROW first layer, not a wide one; a wide first layer only pays with a vectorized (SIMD)
leaf read** (how real NNUE affords wide layers: int16 + SIMD accumulator reads). The
vectorized read is filed `[Next]`. No engine code change (config-only training). See
`plans/nnue-shaped-head-results-1-brisk-walrus.md`.
