# Plan: Incrementalize the dist MLP model (NNUE-style first-layer accumulator)

## Context

`todo.md:206-214` promoted "Incrementalize an ML model" to the single top-priority
item (developer, 2026-07-21). The position-oracle campaign measured that a full-scan
MLP `dist` model's leaf costs **218x-573x more per move** than the linear model's
incremental accumulator path at depth 4, scaling with hidden width (`dist_mlp_wide`'s
256/128 heads cost the most). That cost is the concrete blocker on two things the
developer wants: rating the MLP `dist` variants at real search depth (`d6/nb200k`,
where a full-scan MLP leaf currently costs "seconds per move", so `tools/label_study.ps1`
rosters them at `d4` only) and using an MLP `dist` model as the GUI's live evaluator.

The substrate is already built for the *linear* case: sparse v2 piece-square features
(129 binary inputs, a move flips 2-3), a scalar accumulator `g_mlAcc` seeded by
`mlIncrementalBegin` and maintained by 2-3 weight adds per make/unmake, read at the
leaf by `mlLeafScore`. The stated NNUE step (`todo.md:170-176`, `todo.md:215-219`) is
exactly: **widen that scalar accumulator into a vector for the MLP's first hidden
layer, and run only the (small) remaining layers at each leaf.** Search only ever reads
the `DistModel` mean head (`mlIncrementalBegin` already unwraps `DistModel -> muHead`),
so only the mu head needs incrementalizing; the sigma head stays full-scan.

### Honest expected speedup (read before starting)

This is the standard NNUE first-layer-accumulator trick. It removes the first layer's
cost from every leaf but **cannot** touch the layers past the first ReLU (nonlinear, so
not incrementalizable for a fixed architecture). The win is therefore bounded by the
first layer's share of the forward pass:

- `dist_mlp_s1001`/`s2002` (129->128->64->1): layer0 = 16.5k MACs of ~24.8k total -> ~2-3x.
- `dist_mlp_wide` (129->256->128->1): layer0 = 33k of ~66k total -> ~2x.

So this makes `d6/nb200k` rating **feasible** (roughly halving per-move cost, turning
"seconds/move" into ~1s/move) but does **not** bring MLP cost near the linear
accumulator (which stays ~250x cheaper for the wide head, because its whole forward
*is* one first-layer-like dot product). Framing this honestly in the results doc is
required. The large-win version needs a differently-shaped head (wide first layer, tiny
rest) or the sparsity penalty in `todo.md:234-241` (dead-ReLU / zero-weight skipping in
the leaf tail) - both are follow-ups, not this change.

### What this does and does not optimize (design Q&A)

- **ReLU is honored, not exploited for skipping.** The accumulator holds the first-layer
  *pre-activations* (before ReLU) - that layer is linear, which is exactly why it is
  incrementalizable. ReLU is applied fresh at the leaf on the accumulated pre-activations.
  The plan does NOT use ReLU clamping to skip downstream work (a unit pinned at 0 across a
  move contributing an unchanged constant); that only pays off with a sparsity-trained model
  where most units are dead (`todo.md:220-241`), and is a separate optimization.
- **Undo is first-class.** The vector accumulator is maintained by BOTH `simulateMove*`
  (make) and `unsimulateMove*` (undo); undo applies the exact reverse column add/subtract,
  restoring the parent node's accumulator so alpha-beta can explore sibling moves. Identical
  to today's scalar `g_mlAcc`, and re-asserted after every unmake by the equivalence walk.
  Drift stays negligible because the accumulator is re-seeded from a full board scan at the
  start of each top-level search (`evalBeginSearch`), so `double` rounding only accrues
  within one move's tree, never across the game.
- **The second layer is NOT saved.** The first-layer accumulator makes the first-hidden
  pre-activations ~free, but the fully-connected second layer (H x H2) and beyond are fully
  recomputed at each leaf from the changed `act[1]`. This is precisely the ~2-3x ceiling: the
  first layer is only ~half the forward pass at these widths. Saving the second layer needs
  incrementally-maintained layer-2 pre-activations AND a model sparse enough that few
  post-ReLU units change per move - the dead-ReLU delta follow-up, not this change.

## Approach

Keep the proven scalar linear path **byte-identical**. Add a parallel vector path,
disambiguated by a new `g_mlAccDim` (0 = scalar/linear mode, >0 = MLP mode with that
many first-hidden units). Only the mu head is incrementalized.

### 1. New global state (`src/globals.h` / `src/globals.cpp`)

Alongside `g_mlAcc` / `g_mlIncremental` / `g_mlWeights`:

- `int g_mlAccDim` - first-hidden width `H` for the active MLP mu head; `0` = linear scalar mode.
- `double* g_mlAccVec` - the widened accumulator: `pre[1][j]` for `j in [0,H)` holding
  `B[0][j] + sum(first-layer weights of occupied piece-squares)` (bias + board part,
  side-to-move EXCLUDED, applied at read time - same convention as the scalar).
- `const float* g_mlL0ByInput` - the first layer stored **input-major** (`[129][H]`, i.e.
  `g_mlL0ByInput[idx*H + j]`), so the per-move update for a touched input `idx` is a
  contiguous length-`H` add/subtract (cache-friendly; this is NNUE's feature-transformer
  layout). Column `idx==128` is the side-to-move column, added only at leaf read.

Back these last two with `std::vector` buffers owned in `ml_eval.cpp`, resized once per
top-level search in `mlIncrementalBegin` (not on the hot path). Expose raw pointers +
dim as externs so `moves.cpp` can touch them without a function call.

### 2. Seed / teardown (`src/ml_eval.cpp`, `mlIncrementalBegin` / `mlIncrementalEnd`)

At the existing `dynamic_cast<LinearModel*>` gate (`ml_eval.cpp:126-129`), after the
`DistModel -> muHead` and `ResidualModel -> inner` unwraps: if the core casts to
`LinearModel`, keep today's scalar seed and set `g_mlAccDim = 0` (unchanged). Otherwise
try `dynamic_cast<MLPModel*>(core)` with `featureVersion()==2` and `sizes.size()>=2`:

- `H = sizes[1]`. Resize the input-major buffer to `129*H`; fill it as the transpose of
  `W[0]` (`g_mlL0ByInput[i*H+j] = W[0][j*129+i]`). O(129*H) once per search, cheap.
- Seed `g_mlAccVec[j] = B[0][j] + sum over occupied squares of that square's column`
  (exclude input 128). Set `g_mlAccDim = H`, latch the `MLPModel*` (for the leaf tail),
  `g_mlOutScale`, `g_mlSkipW` (0 for dist; nonzero only if a Residual wrapped the MLP).
- `g_mlIncremental = true`.

`mlIncrementalEnd` also clears `g_mlAccDim = 0` and the MLP pointer. This makes both the
`DistModel(mlp mu)` case and any future `Residual(mlp inner)` case incremental for free.

### 3. Make/unmake maintenance (`src/moves.cpp`, all four sim/unsim functions)

Inside each existing `if (g_mlIncremental) { ... }` block (`moves.cpp:301-304`, `333-336`,
`361-364`, `390-393`), branch on `g_mlAccDim`: `== 0` keeps today's scalar lines exactly;
`> 0` does the same source-off / dest-on / capture-off pattern but as length-`H` column
add/subtracts on `g_mlAccVec` via `g_mlL0ByInput`. Add small header-inline helpers
(e.g. in `ml_eval.h`) `mlAccAddInput(idx)` / `mlAccSubInput(idx)` that operate on the
column, so the four call sites stay readable and the delta logic lives in one place.
The touched-square indices (`mlSqW`/`mlSqB` of src/dst/capture) are already computed
there; the capture-color asymmetry (White captures Black, and vice-versa) is unchanged.

### 4. Leaf read (`src/ml_eval.cpp`, `mlLeafScore`)

Branch on `g_mlAccDim`. `== 0` is today's scalar read (unchanged). `> 0`:

- Form first-hidden pre-activations into a fixed stack buffer `float pre1[ML_ACC_MAX]`
  (`ML_ACC_MAX = 512`, a compile-time cap above the current 256): for each `j`,
  `pre1[j] = g_mlAccVec[j] + stmSign * g_mlL0ByInput[128*H + j]`.
- Call a new `MLPModel` method `forwardFromHidden(const float* pre1)` that applies ReLU to
  `pre1` into `act[1]` and runs layers `k=1..L-1` (reusing the model's existing mutable
  `act`/`pre` scratch and the exact `computeForward` math), returning the mu logit. This
  keeps all MLP forward math in `ml_model.cpp` as the single source of truth.
- `out = muLogit + g_mlSkipW * g_chipDiff`; return `mlSquashToEval(out, g_mlOutScale)`
  (the shared squash tail, so incremental and full-scan can't diverge in scaling).

`mlValueScore` (full-scan) and `mlValueScoreDist` (mu+sigma for GUI/analysis) are
untouched; the sigma head is never read in search.

### Critical files

- `src/globals.h` / `src/globals.cpp` - new accumulator-vector globals.
- `src/ml_eval.cpp` / `src/ml_eval.h` - `mlIncrementalBegin`/`End` MLP branch, `mlLeafScore`
  MLP branch, buffer ownership, inline column helpers.
- `src/ml_model.h` / `src/ml_model.cpp` - `MLPModel::forwardFromHidden` (new; factors the
  tail out of `computeForward`).
- `src/moves.cpp` - vector branch in the four sim/unsim `g_mlIncremental` blocks.
- `tests/test_ml.cpp` - new equivalence walk (below); update the two existing
  "MLP stays off" tests to expect incremental ON.

## Correctness / numerical caveat

The vector accumulator sums first-layer weights in a path-dependent order in `double`;
`computeForward` sums them in input order in `float`. They agree to ~1e-12 relative
EXCEPT when a first-hidden pre-activation sits within that tolerance of 0, where the ReLU
kink can flip and produce a small discrete leaf difference. With real learned weights,
exact-zero pre-activations essentially never occur, so this is negligible in practice,
but it means the incremental MLP leaf is **approximate**, not bit-exact, vs the float
full-scan `mlValueScore` (unlike the linear path, which is exact within +/-1 rounding).
Document this as a measurement caveat. Consequence: the incremental agent could very
rarely pick a different move than its full-scan twin; expected to be immeasurably small.

## Verification (code correctness)

1. **Equivalence test (primary gate).** In `tests/test_ml.cpp`, mirror the existing
   `mlAccFull`/`mlWalk` harness (`test_ml.cpp:174-230`) for the vector case: add
   `mlAccVecFull(mu, out[])` (double reference recompute of `pre[1][]`, STM excluded) and
   a walk over a `DistModel` with an MLP mu head asserting, at every node, per-component
   drift `< 1e-4` and the incremental `evalLeaf` matching a double full-forward reference
   within +/-1. Run both a crafted edge-capture position (depth 3) and `board1.txt`
   (depth 2), same as the linear test. **Update** the two tests that currently assert
   `g_mlIncremental == false` for an MLP mu/inner head (`test_ml.cpp` ~:553-565 Residual+MLP,
   ~:960-979 Dist+MLP) to assert incremental is now ON and tracks the mu logit.
2. **Build + full suite:** `.\tools\run_tests.ps1 -Build` (the `/run-tests` skill) passes.
3. **Speed A/B (the mechanism payoff).** Load `dist_mlp_wide` into a sweep slot, play a few
   `ab(d6,tt,ord,nb200k).learned(sN)` moves, and compare ms/move against the pre-change
   full-scan (`git stash` baseline, or the checked-in `d4`-only numbers). Confirm the leaf
   equivalence held in real search (same or near-identical chosen moves vs the full-scan
   agent). Also spot-check `d4` to confirm no regression for the linear path.
4. Console sanity: `.\breakthrough.exe` still loads and a LearnedValue-vs-X game runs.

## Phase 2: dead-ReLU / activation-churn measurement (in scope this session)

The insight from the design discussion: the second-layer delta trick's payoff is exactly
`H / |changed|`, where `|changed|` is how many first-hidden units change ACTIVATION per
move. This is worth measuring on the CURRENT dense models even before any sparsity
training, because it quantifies how much a future second-layer delta could save. Add a
small measurement (a `train.exe` subcommand, e.g. `mlp-sparsity`, alongside `speed`/
`dist-eval`) that loads a dist MLP mu head and, over a sample of positions (from a label
pool or self-play):

- **Static sparsity:** mean fraction of first-hidden units with `relu(z)==0` (dead) per position.
- **Per-move churn:** over each position x its legal moves, mean number of first-hidden
  units whose activation changes (`Δa != 0`), and of the rest how many are
  clamped-both-sides (skippable). Report `|changed|/H` and a small histogram.

Report these for `dist_mlp_s1001/s2002` (H=128) and `dist_mlp_wide` (H=256). The results
feed the results doc and justify (or deflate) the second-layer follow-up. Reuse the
existing `MLPModel` forward scratch and `generateMoves`/`simulateMove*` to enumerate
children.

## Phase 3: full d6 rating campaign (in scope this session)

Only start this once the equivalence test is green and the speed A/B confirms `d6` is now
affordable. Goal: re-rate the MLP `dist` variants at the real `d6/nb200k` head that was
previously blocked, and see whether an incrementalized `d6` MLP changes the standings the
`d4`-only / `d6`-full-scan runs produced (`dist_lin@d6 = 1031` was the strongest dist agent;
all MLPs lost to it in play despite winning on prediction, theory 27). Follow the
`ranking/CHAMPION.md` claim-hygiene rules exactly:

- **Reuse the existing harness:** `tools/label_study.ps1` already copies the dist models to
  slots 76-79 and emits roster lines; extend/parameterize it so the MLP variants get the
  `ab(d6,tt,ord,nb200k)` head (not just `d4`), now that the leaf is affordable.
- **Multi-seed:** train ~6 seed replicas per MLP recipe (via `train.exe dist-value` with
  distinct `--seed`, the existing `label_study.ps1` recipe) so a strength comparison clears
  the 50-150 Elo training-seed-noise band (theory 8). Elo is the primary metric; prediction
  loss does not substitute.
- **Anchored refit at 32 games/pair for any top claim:** run `rank.exe run` / the sweep
  rating path over the full active roster, anchored Bradley-Terry refit. Any top-of-table
  conclusion requires every contender pair at >= 32 games (boost via `ranking/roster_top.txt`
  and refit); never conclude from 8-games/pair fills.
- **Compare within one fit only:** never compare absolute Elo across fits (the BT prior
  compresses the scale as the pool grows). Compare order and error bands inside the single
  refit that includes the new agents.
- **Re-certify the champion** if any incrementalized MLP rates near the top: update
  `ranking/CHAMPION.md` and the `todo.md` Agent Track goal paragraph in the same session.
- **Document as one complete picture:** each new agent's inputs (v2, 129 features),
  architecture (mu/sigma head widths), search wrapper (`ab(d6,tt,ord,nb200k)` + slot),
  training recipe, and pooled Elo, in its results doc plus `ML.md`'s shipped-models section
  and the `d2/d4/d6` table (`ML.md:280-285`).

## Deliverables / docs (per CLAUDE.md "after every functional change")

- Update `README.md` (AI/model description) and `ML.md` (the value-head roadmap NNUE entry,
  the shipped-models table) to reflect that MLP `dist` mu heads are now incrementally scored.
- Archive this plan into `plans/` and write the companion `...-results-...md`: the speed
  A/B numbers (ms/move and per-node cost before/after at d4 and d6, how measured), the d6
  rating table, the float-vs-double ReLU-kink caveat, a Future Work section (each item tied
  to a specific conclusion), and an Ideas-This-Inspired section.
- Update `Docs/theories.md` (theory 27 re-test outcome; any new NNUE-cost theory).
- Cross out the completed `todo.md` items (`todo.md:206-214` incrementalization,
  `todo.md:502-508` d6 MLP rating) with strikethrough.
- Commit after `run_tests.ps1 -Build` passes (never push without explicit instruction).

## Follow-ups (filed into `todo.md`, referencing this session's results doc)

- **Second-layer (2-accumulated-layer) delta:** maintain the second-layer pre-activation
  vector as reversible accumulator state and propagate only the deltas of first-hidden
  units whose activation changed (skip clamped-both-sides units). Exact by ReLU piecewise
  linearity. Payoff `= H / |changed|`; gated on the churn measurement above showing
  `|changed| << H`, which for dense models it will not, hence pairing with:
- **Sparsity penalty** (`todo.md:234-241`) in `dist-value` training (L1 on first-layer
  weights, or a penalty on the fraction of non-clamped ReLU units) to drive `|changed|`
  down so the second-layer delta pays off, and to let the leaf tail skip zero-weight paths.
- NNUE-shaped heads (wide first layer, tiny rest) trained to exploit the accumulator.
- GUI: set an incrementalized MLP `dist` model as the live on-screen evaluator
  (`todo.md:699-704`), now unblocked.
