---
name: incremental-mlp-and-sparsity
description: Shipped NNUE-style incremental MLP mu head (1.78x); dist heads are ~90% dead-ReLU so sparse-tail forward is the top follow-up
metadata: 
  node_type: memory
  type: project
  originSessionId: 9b2197c9-c8a4-4a00-98c2-319283f178f1
  modified: 2026-07-22T09:12:33.313Z
---

2026-07-22: shipped the NNUE-style first-hidden accumulator for MLP `dist` mu heads
(`g_mlAccDim`/`g_mlAccVec`/`g_mlL0ByInput`, the vector generalization of `g_mlAcc`).
Measured **1.78x** per-node speedup for the wide head (256/128) at fixed depth. Bounded
by the first-layer share; layers past the first ReLU are irreducible for a fixed
architecture.

**Key finding (theory 36):** a new `train.exe mlp-sparsity` measurement showed the dist
MLP heads are **~90% dead-ReLU per position** with only **~10-12% activation churn per
move** -- NOT the dense ~50%-active heads assumed going in. So the highest-value follow-up
is a **sparse leaf-tail forward** (skip dead first-hidden units in `MLPModel::forwardFromHidden`'s
second-layer matmul): exact, a few lines, ~8-9x additional ceiling, no new accumulator
state. A full second-accumulated-layer delta is a later, heavier option. Sparsity-training
penalty is now low priority (heads already sparse).

**Decision/context:** the d6 MLP rating was NOT actually blocked -- it was already done
last session (720+ games/agent: dist_lin 1031 > MLPs 974/967/931, all below the champion;
theory 27 holds). The incremental agent has the same content-hash ID + eval-equivalent
output, so those numbers carry over; the change is pure speed. Do not re-run the big d6
campaign unless a new head (e.g. the sparse-tail one, or an NNUE-shaped 129->512->8->1)
rates near the top. See [[position-oracle-campaign]] and
`plans/nnue-incremental-mlp-results-1-crystalline-taco.md`.
