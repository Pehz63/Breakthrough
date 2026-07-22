# Results: Incrementalize the dist MLP model (NNUE-style first-layer accumulator)

Companion to `nnue-incremental-mlp-plan-1-crystalline-taco.md`. Shipped 2026-07-22.

## What shipped

An NNUE-style first-hidden-layer accumulator for an MLP value (mu) head over the v2
sparse piece-square features. Previously only a *linear* v2 head kept the incremental
scalar `g_mlAcc`; an MLP head (including a `DistModel`'s MLP mu head or a `ResidualModel`
MLP inner) fell back to a full forward pass at every search leaf. Now the MLP's first
layer is maintained as a vector accumulator across make/unmake, and only the (small)
remaining layers run per leaf. The linear scalar path is unchanged (byte-identical).

Disambiguated by a new `g_mlAccDim` global: `0` = the existing scalar/linear path,
`> 0` = the MLP vector path with that first-hidden width `H`.

### Files changed

- `src/globals.h` / `src/globals.cpp` -- new `g_mlAccDim` (int), `g_mlAccVec` (double*),
  `g_mlL0ByInput` (const float*, input-major layer-0 transpose).
- `src/ml_model.h` / `src/ml_model.cpp` -- `MLPModel::forwardFromHidden(pre1)`: applies
  ReLU to the externally-maintained first-hidden pre-activations and runs layers
  `1..L-1`, reusing `computeForward`'s exact math (single source of truth for the tail).
- `src/ml_eval.h` / `src/ml_eval.cpp` -- `mlIncrementalBegin` MLP branch (unwrap
  `DistModel -> muHead`, `ResidualModel -> inner`, then `dynamic_cast<MLPModel*>`; build
  the input-major `W[0]` transpose once per search; seed `g_mlAccVec[j] = B[0][j] +
  sum of occupied columns`, side-to-move excluded); `mlLeafScore` MLP branch (add the
  side-to-move column, then `forwardFromHidden`); `mlIncrementalEnd` clears the state;
  inline `mlAccAddColumn/mlAccSubColumn` header helpers (length-`H` AXPY on a contiguous
  column).
- `src/moves.cpp` -- the four `simulateMove*/unsimulateMove*` `g_mlIncremental` blocks
  gain a `g_mlAccDim > 0` branch: source-off / dest-on / capture-off as column
  add/subtracts. The scalar linear lines are preserved exactly under the `else`.
- `tests/test_ml.cpp` -- `mlAccVecFull` reference + `mlpWalk` harness; the two former
  "MLP stays off" tests (Residual+MLP, Dist+MLP) now assert incremental is ON and the
  vector leaf matches the full-scan `mlValueScore` over exhaustive make/unmake walks.
- `src/ml_train.{h,cpp}` + `tools/train_main.cpp` -- new `train.exe mlp-sparsity`
  measurement subcommand (dead-ReLU + activation churn).

### How to test

1. `.\tools\run_tests.ps1 -Build` (or the raw `build_tests.bat` command) -- the primary
   correctness gate. The MLP walk tests assert per-node accumulator drift `< 1e-4` and
   incremental-leaf-vs-full-scan agreement within +/-2, over both a crafted edge-capture
   position (depth 3) and the dense standard board (depth 2), re-checked after every
   unmake (the undo path). Result: **all tests pass, 2002 assertions in 98 test cases.**
2. `train.exe mlp-sparsity --model models/dist_mlp_wide.txt --pool data/labels/pool_eval.jsonl`
   -- the churn measurement.
3. Speed A/B: put a dist MLP in slot 2 (`models/pst_value.txt`), run `train.exe speed
   --maxdepth 4`, read the `learned-v2` us/move and nodes/move; compare against a
   full-scan build (revert the accumulator source files, rebuild).

## Results (concrete numbers)

### Speed A/B -- the mechanism payoff

`dist_mlp_wide` mu head (129 -> 256 -> 128 -> 1) in slot 2, `train.exe speed --board
boards/board1.txt --positions 6 --ms 200 --maxdepth 4 --seed 7`. Same model both builds;
per-node cost = us/move / nodes/move (fixed depth, so node counts match):

| depth | full-scan us/node | incremental us/node | speedup |
|---|---|---|---|
| d3 | 38.0 (117428/3092) | 21.3 (68538/3215) | ~1.78x |
| d4 | 36.1 (970896/26875) | 20.2 (544114/26875) | **1.78x** |

So the first-layer accumulator removes ~44% of per-node cost for the widest head. This
matches the honest pre-work estimate (~2x for a head where the first layer is ~half the
forward pass; the shortfall vs 2x is the per-make column-update overhead plus the STM +
tail costs).

### Sparse leaf-tail forward (second-layer optimization, shipped same session)

Prompted by the dead-ReLU finding below, `MLPModel::forwardFromHidden` was changed to sum
each remaining layer only over its NONZERO inputs. ReLU zeros ~90% of the first-hidden
units, and a zero activation contributes exactly nothing downstream, so this is
**bit-identical** to the dense loop (the equivalence tests pass unchanged) and just skips
~90% of the dominant second-layer matmul. Same benchmark, wide head:

| build | us/node (d4) | vs full-scan |
|---|---|---|
| full-scan | 36.1 | 1.0x |
| + first-layer accumulator (dense tail) | 20.2 | 1.78x |
| **+ sparse leaf-tail** | **2.84** | **12.7x** |

The sparse tail is a further **7.1x** on top of the first-layer accumulator, realizing
(and slightly exceeding) the ~8-9x churn ceiling below. At `nb200k` the wide MLP now costs
~0.57 s/move at d6 (200k x ~2.84 us), down from ~7.2 s full-scan -- d6 rating is now
genuinely affordable. Why the per-leaf sparse forward was chosen over the literal
cross-move delta-accumulator (the toy-example mechanism): for these ~90%-dead heads an
op-count shows the delta approach is both slower (it pays an update AND an undo every
make, and the side-to-move bit flips every ply, perturbing many units) and more complex
(H2-dim reversible accumulator state); recomputing the already-sparse tail per leaf wins.
The delta-accumulator would only pay off if the second layer were much wider than the
live-unit count. (Node counts differ slightly across the three runs because the timed
benchmark averages over whichever position subset fits its ms budget, not because the
eval changed -- the sparse tail is bit-identical.)

### Dead-ReLU / activation churn -- the measurement that flipped the prior

`train.exe mlp-sparsity`, 500 eval-pool positions, side-to-move held fixed:

| model | H | static dead ReLU | activation churn/move | 2nd-layer delta ceiling (H/churn) |
|---|---|---|---|---|
| dist_mlp_s1001 | 128 | 90.2% (115.4/128) | 12.3% (15.7/128) | 8.1x |
| dist_mlp_s2002 | 128 | 90.0% (115.1/128) | 12.4% (15.9/128) | 8.0x |
| dist_mlp_wide  | 256 | 91.7% (234.7/256) | 10.7% (27.3/256) | 9.4x |

The plan predicted dense outcome-trained models with ~half the units active, hence a
small (wash-to-modest) second-layer-delta win. **That prediction was wrong for these
models.** The L2-regularized probit-BCE `dist-value` training produces heads that are
~90% dead-ReLU per position, with only ~10-12% of first-hidden units changing activation
per move. So a second-accumulated-layer (dead-ReLU delta), or even just skipping zero
activations in the leaf tail's second-layer matmul, has an ~8-9x additional ceiling on
top of the first-layer accumulator -- a much larger and easier follow-up than assumed.
This is precisely the insight the developer asked the measurement to surface, and it is
worth acting on even without any added sparsity-training penalty (the models are already
sparse). Histogram: churn is tightly concentrated in the [0.0, 0.2) fraction band for
all three heads (>99% of moves), with essentially no move touching more than 30% of `H`.

### d6 rating -- already established, not re-run at scale

The plan's Phase 3 assumed d6 MLP rating was blocked by cost. It was not: it was
completed last session (full-scan, commit a86f08e) and is well-resolved at 720+ games per
agent. Current `ranking/ratings.tsv` (2026-07-21 fit; champion classic+book2 1131, oracle
1151, in the same fit -- do not compare across fits):

| agent | d6/nb200k Elo | games |
|---|---|---|
| dist_lin (s76)        | 1031 | 760 |
| dist_mlp_s1001 (s77)  | 974  | 720 |
| dist_mlp_s2002 (s78)  | 967  | 720 |
| dist_mlp_wide (s79)   | 931  | 720 |

`dist_lin` is the strongest dist agent at d6; all three MLPs rank below it and all are
well below the champion -- theory 27 (MLP wins on prediction, loses in play) holds at d6.
The incremental agent has the **same canonical ID** (content-hash based) and
eval-equivalent output (proven by the passing equivalence tests), so these numbers carry
over unchanged; the optimization is pure speed, not a strength change. On the developer's
call, instead of a redundant multi-hour campaign we ran a small consistency check.

**Consistency check** (`rank.exe pairgen`, incremental build, isolated from the ranking
store): `dist_mlp_wide@d6` vs `dist_lin@d6`, 12 games, 4 random opening plies. Their
stored Elo gap (~100, favoring dist_lin) predicts ~64% for dist_lin.

Result: **dist_lin won 8-4** (dist_mlp_wide 4-8, i.e. 33.3% for the wide MLP). That is
66.7% for dist_lin, essentially spot-on the ~64% its stored 100-Elo edge predicts. So the
incremental agent, playing fresh games with the new code, loses to dist_lin consistent
with the rating established from full-scan games -- the eval-equivalence proven by the unit
tests carries through to real play. (n=12, small; the rigorous proof is the equivalence
test, this is an end-to-end sanity confirmation.) Color split: dist_mlp_wide 1-5 as White,
3-3 as Black -- consistent with the pool's known White/Black asymmetry, not a code issue.

## Implementation notes and differences from the plan

- **The ReLU-kink caveat in the plan was overblown.** The plan worried that float-vs-double
  accumulation-order differences near the ReLU kink could cause discrete leaf jumps.
  ReLU is *continuous* (only its derivative is discontinuous), so a ~1e-5 pre-activation
  difference propagates continuously: at worst ~1e-5 x (downstream Lipschitz constant),
  i.e. ordinary float rounding, the same magnitude as the linear path. The tests confirm
  incremental-vs-full-scan agreement within +/-2 integer eval units (the linear path uses
  +/-1; the extra 1 is the two-layer float accumulation, not a kink flip). The incremental
  MLP leaf is *approximate*, not bit-exact, vs the float `mlValueScore`, but only at the
  rounding level.
- **Input-major layer-0 transpose.** `MLPModel::W[0]` is output-major (`W[0][j*in+idx]`).
  For a cache-friendly per-move column AXPY, `mlIncrementalBegin` builds an input-major
  copy `g_mlL0ByInput[idx*H + j]` once per top-level search (O(129*H), cheap). The
  side-to-move column (`idx == MLV2_STM`) lives in that same buffer and is applied at leaf
  read, exactly like the linear path's scalar `g_mlStmW`.
- **Only the mu head is incrementalized.** Search reads only the `DistModel` mean; the
  sigma head and `mlValueScoreDist` (GUI/analysis) are untouched and stay full-scan.
- **Guard:** the MLP branch requires `sizes.size() >= 3` (a genuine hidden layer) and
  `H <= ML_ACC_MAX (512)`; a degenerate no-hidden MLP or an over-wide head falls back to
  full-scan safely.
- **d6 rating already existed** (Phase 3 premise was false). Surfaced to the developer;
  they chose a small consistency check over a redundant campaign.

### Correctness gotchas

- The synthetic `dist-value` end-to-end test in `test_ml.cpp` calls `writeManifest`, which
  clobbers the real `models/manifest.{json,md}` with a throwaway model row. Pre-existing
  test behavior, unrelated to this change; restored with `git restore` so it does not ride
  along in the commit. (Filed as an Idea below: the test should write to a scratch
  manifest path.)
- Build note for this machine: `vswhere.exe` is not on PATH, so `build_*.bat` fail; builds
  used the explicit `vcvars64.bat` path (VS 18 Community). Also, Git Bash's `cmd /c '...'`
  with nested quotes runs but does not capture cl's stdout to a redirected log -- a local
  `.bat` file captures cleanly.

## Future Work

- **Sparse leaf-tail forward -- SHIPPED same session** (see the speed section above): the
  ~8-9x ceiling was realizable; measured 7.1x on top of the first-layer accumulator
  (2.84 us/node, 12.7x vs full-scan) for the wide head, bit-identical. d6 rating is now
  affordable (~0.57 s/move at nb200k), so a fuller multi-seed d6 campaign is no longer
  compute-gated if a future head warrants it.
- **Second-accumulated-layer (dead-ReLU delta).** Maintain the second-layer
  pre-activations as reversible accumulator state and propagate only the deltas of
  first-hidden units whose activation changed (~12% per move). Payoff `H / churn` ~= 8x.
  More make/unmake bookkeeping than the sparse-tail forward; the sparse-tail forward
  likely captures most of the win with far less complexity, so measure that first.
  Tethered to: the ~12% churn measurement.
- **Sparsity-training penalty** (`todo.md`): an L1 / dead-ReLU penalty in `dist-value`
  could push dead fraction even higher, but the measurement shows the models are already
  ~90% sparse without it, so this is now lower priority than the sparse-tail forward.
  Tethered to: the static-sparsity measurement (already high, so marginal).
- **Full multi-seed 32-game d6 campaign.** Only warranted if a new head rates near the top;
  the existing d6 MLP standings (all mid-pack, theory 27) do not justify the compute now.
  Tethered to: the existing 720-game d6 rating.

## Ideas This Inspired

- The natural NNUE shape for THIS engine is a wide first layer and a tiny rest (the
  opposite of the current 256->128). With the first layer accumulated and the tail sparse,
  a `129 -> 512 -> 8 -> 1` head could be both cheaper per node and higher-capacity. Worth a
  training + rating arm once the sparse-tail forward lands.
- The 90%-dead finding suggests the dist heads are effectively using a small active
  sub-network per region of board-space -- a soft mixture-of-experts. Probing which units
  fire for which board phases could connect to the phase-conditioned MoE idea in `todo.md`.
- The `dist-value` test clobbering the shared manifest is a general hazard: any test that
  calls `writeManifest`/`exportDocs` mutates committed files. A test-scoped output path (or
  a guard that redirects manifest writes under a temp dir in tests) would remove the
  footgun.
- `mlp-sparsity` is a generic MLP interpretability probe, not specific to this change --
  activation churn per move is a cheap proxy for "how local is this model's board
  representation," useful for any future net.
