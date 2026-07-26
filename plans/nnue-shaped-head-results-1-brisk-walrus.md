# NNUE-shaped dist head (129 -> 512 -> 8 -> 1): results

> **[ELO HYGIENE UNVERIFIED - flagged 2026-07-25]** The Elo comparisons in this
> document predate the ranking-claim hygiene rules and may be mistaken. Two
> defects were found in this project's Elo reporting: numbers were read from
> `ranking/ratings.tsv`, which mixes RETIRED agents (`active = gone`, superseded
> `@N` identities frozen at old game counts) in with live ones; and agents were
> sometimes compared across different SEARCH HEADS, which are different agents
> whose Elos are not interchangeable. Any finding here that was accepted or
> refuted on an Elo comparison, including any comparison against the champion,
> may therefore be wrong and needs re-evaluation against `ranking/standings.tsv`
> (active only, grouped by head) within a single fit. Full explanation:
> `Docs/benchmarking.md`, "Elo comparison hygiene". Tracked in `todo.md`; remove
> this banner once this document's numbers have been re-verified.

Companion results doc. This work was conversation-driven (no separate plan-mode
file to archive): after shipping the incremental MLP accumulator + sparse
leaf-tail forward
(`plans/nnue-incremental-mlp-results-1-crystalline-taco.md`), the developer asked
how to train a dist MLP to be "more efficient." The design discussion landed on
an NNUE-shaped head, a wide first layer with a tiny rest (`129 -> 512 -> 8 -> 1`),
on the theory that moving capacity into the accumulated (free) first layer would
keep prediction quality while collapsing the non-incrementalizable tail.

**Headline: the shape is prediction-neutral but the efficiency premise failed.**
The NNUE head predicts as well as the wide head, but it is ~2x SLOWER per node
than the standard `128,64` head, because once the accumulator makes the
first-layer UPDATE free, the leaf still must READ and ReLU all H first-hidden
pre-activations, an O(H) scalar cost that is now the dominant leaf cost, and the
NNUE shape has the LARGEST H. The ~17x-cheaper tail (in MACs) was never the
bottleneck. The fix is a vectorized (SIMD) leaf read, filed as `[Next]`.

## What was built

- **No engine code change.** The NNUE shape is a pure training-config change:
  `--mu-hidden "512,8"` on the exact `dist-value` recipe the existing dist MLPs
  used (`--epochs 40 --lr 0.01 --lr-sigma 0.002 --val-split 0.1 --early-stop
  --ckpt-every 5 --mu-type mlp --s-type mlp --s-hidden "64"`). H = 512 sits right
  at the incremental path's `ML_ACC_MAX` cap (`src/ml_eval.cpp`), so the
  accumulator + sparse leaf-tail forward already accept it (`H > ML_ACC_MAX`
  rejects only strictly-greater widths).
- **6 seed replicas** trained from the frozen label store
  (`data/labels/raw_train.jsonl`, `pool_train.jsonl`, `ratings_snapshot.tsv`):
  seeds 1001, 2002, 3003, 4004, 5005, 6006 ->
  `models/dist_mlp_nnue_s<seed>.txt`, copied to rating slots 110-115.

## Results

### Prediction quality (neutral: as good as wide)

Best held-out val NLL across the 6 NNUE seeds: **0.4353 - 0.4372** (mean ~0.4362),
a tight cluster indistinguishable from the existing MLP cohort. The value signal
saturates near 0.436 NLL for any MLP shape (a linear dist head gets only 0.4562),
so concentrating mass in a wide first layer costs nothing on prediction.

| head | shape | best val NLL |
|---|---|---|
| standard s1001 | 128,64 | 0.4356 |
| standard s2002 | 128,64 | 0.4368 |
| wide s3003 | 256,128 | 0.4375 |
| NNUE s1001..s6006 | 512,8 | 0.4353 - 0.4372 |
| linear | - | 0.4562 |

Held-out `dist-eval` on NNUE s1001 (calibrated, oracle-independent model metrics):
**MAE 143.5 Elo, RMSE 177.4, Spearman 0.736, outcome NLL 0.40795** -- matching (a
hair better MAE than) the wide head's recorded 146.2 / 0.4079. The 8-unit second
layer is a real capacity cut but does not hurt, because the task is low-capacity.

Training cost: ~82 min/seed solo; the 5-seed parallel batch took 88 min wall (12
cores), so contention added only ~6 min. The wide first layer + narrow bottleneck
trains slower (best val at epoch 4-19, vs 5-8 for the standard/wide heads) but
reaches the same NLL.

### Static sparsity and churn (`train.exe mlp-sparsity`, eval pool, 300 positions)

| head | H | dead ReLU | active units | 2nd-layer width | tail MACs (active x width) |
|---|---|---|---|---|---|
| standard (128,64) | 128 | 90.5% | ~12 | 64 | ~800 |
| wide (256,128) | 256 | 92.2% | ~20 | 128 | ~2560 |
| NNUE (512,8) | 512 | 96.5% | ~18 | 8 | ~145 |

By the MAC count the NNUE tail is ~17x cheaper than wide. This is the metric that
misled the design: it measures the wrong thing (see below).

### Speed A/B (the decisive measurement)

`train.exe speed --positions 8 --ms 1000 --maxdepth 4 --seed 7`, each dist head
swapped into slot 2 (`models/pst_value.txt`) and timed on the current sparse-tail
binary. us/node = us/move / nodes/move (per-node leaf cost, tree-size-invariant).

| head | shape | us/node (d4) | ~ms/move at nb200k |
|---|---|---|---|
| standard | 128,64 | **1.24** | ~244 ms |
| NNUE | 512,8 | **2.47** | ~494 ms |
| wide | 256,128 | 2.86 | ~572 ms |

(Wide reproduces last session's 2.84 us/node sparse-tail figure exactly,
confirming the incremental path is engaged, not a full-scan fallback.)

The NNUE head is **~2x slower per node than the standard head** and only ~13%
faster than wide. Fitting `us/node ~= a*H + b*tail_MACs + c` to the three points
gives a ~= 4.3 ns per first-hidden unit (the read+ReLU) and b ~= 0.6 ns per tail
MAC. For NNUE the read term is 2.21 us of the 2.48 total; the tail is 0.09 us.
The leaf is entirely dominated by the O(H) = 512 first-hidden read.

### Elo (full-roster anchored refit)

6 NNUE seeds at `ab(d4)` and `ab(d6,tt,ord,nb200k)` added to the 103-agent roster
(slots 110-115), 8 games/pair, one anchored Bradley-Terry refit (~94 min, 12
shards, 9264 games). All numbers below are from THIS single fit -- never compared
across fits (the BT prior compresses the scale as the pool grows).

d6/nb200k dist class, this fit:

| dist head | slot | this-fit Elo (pm) |
|---|---|---|
| dist_lin | s76 | 1038 (15) |
| NNUE | s111 | 1037 (16) |
| NNUE | s113 | 1016 (15) |
| standard s1001 | s77 | 982 (15) |
| standard s2002 | s78 | 977 (15) |
| NNUE | s115 | 970 (15) |
| NNUE | s114 | 957 (15) |
| NNUE | s110 | 949 (15) |
| wide | s79 | 940 (15) |
| NNUE | s112 | 908 (14) |

The 6 NNUE d6 seeds span **908 - 1037, mean ~973, median ~963** (a 129-Elo spread,
within the 50-150 training-seed-noise band). That places the NNUE cohort squarely
in the existing MLP band (wide 940 -> standard 982), with its two best seeds
reaching dist_lin (1038), the strongest dist agent. At d4 the NNUE seeds span
598 - 834 (mean ~660). Champion 1114, d8/nb2m oracle 1158 in the same fit -- both
far above the whole dist class, so the throne is unaffected and no certification
refit was triggered (the new cohort landed mid-table).

**Strength verdict: a wash.** Across seeds the NNUE head plays in the same band as
the existing dist MLPs, with no regression and possibly a slight edge at the top,
but ordering the very top of the dist class (NNUE s111/s113 vs dist_lin vs the
standard MLPs, all within ~60 Elo at pm ~15) would need 32 games/pair; it is not
claimed from these 8/pair fills. Combined with the negative speed result, the
NNUE-shaped head is not worth adopting for this engine as-is: same strength, ~2x
slower per node than the standard 128,64 head.

## Root cause: the accumulator frees the UPDATE, not the leaf READ

The incremental accumulator maintains the first-hidden pre-activations across
make/unmake with 2-3 column adds per move -- so the first-layer UPDATE is free.
But at every leaf, `MLPModel::forwardFromHidden` must still:

1. Form `pre1[j] = accumulator[j] + stm_sign * stm_column[j]` for all H units
   (the side-to-move bit flips every ply, so its dense column cannot be folded
   into the persistent accumulator -- it is applied at the leaf).
2. Apply ReLU to all H units.
3. Scan all H for nonzeros to drive the sparse tail.

Steps 1-3 are O(H) scalar work, unavoidable per leaf for this design. The NNUE
shape maximizes H (512), so it maximizes this floor. The sparse tail optimization
from last session removed the second-layer matmul cost, but that only exposed the
O(H) read as the true floor -- which a wide first layer makes worse, not better.

**Why real NNUE gets away with a wide first layer:** it reads the accumulator as
int16 with SIMD (an AVX-512 register holds 32 int16, clipped-ReLU is a vector
min/max), so the O(H) read is ~8-16x cheaper per unit. Our `forwardFromHidden`
reads scalar floats. Without a vectorized read, a wide first layer is a net loss.

## Follow-up: vectorized (SIMD) leaf read (implemented + measured)

The O(H) leaf read was the whole bottleneck, so we implemented it. AVX2 intrinsics
were added under `#if defined(__AVX2__)` (scalar path preserved unchanged under
`#else`): the pre1 accumulator read vectorized 8 units at a time reading the double
accumulator (`ml_eval.cpp mlLeafScore`), a vectorized ReLU, and the tail as a dense
AVX2-FMA matmul that skips the branchy nonzero-gather (`ml_model.cpp
forwardFromHidden`). Correctness: all 2002 test assertions pass under the AVX2
build within the leaf's existing tolerance (the dense tail + FMA contraction +
SIMD reduction order make it approximate, not bit-identical, like the rest of the
incremental leaf).

Two measured lessons:

1. **The `/arch:AVX2` build flag alone (no code) gave only ~10%.** The compiler
   cannot vectorize the data-dependent nonzero-gather or the mixed double/float
   pre1 read, which are the dominant terms. Explicit intrinsics were required.
2. **A uniform dense tail regressed the wide head** (2.86 -> 3.686 us/node): for a
   wide output layer (H2=128) doing all the ~90% zero MACs densely costs more than
   the sparse gather. Gating dense-vs-sparse on output width (dense for out <= 32,
   sparse otherwise) fixed it.

Final AVX2 speed A/B (us/node, d4, gated tail):

| head | scalar | AVX2 | speedup |
|---|---|---|---|
| standard (128,64) | 1.24 | 1.00 | 1.24x |
| NNUE (512,8) | 2.47 | 1.89 | 1.31x |
| wide (256,128) | 2.86 | 2.43 | 1.18x |

**Verdict: vectorization does NOT flip the shape ranking.** It is a real general
leaf speedup (~1.2-1.3x for every shape, and it helps the standard head we would
actually use, dropping it to 1.00 us/node), but it is a roughly constant factor
across shapes, so NNUE-wide (1.89) stays ~1.9x slower than the standard head
(1.00). Vectorization makes the wide first layer more affordable in absolute
terms but never cheaper than a vectorized narrow head, because the O(H) read
scales with first-layer width for everyone. Since width buys no strength here
(saturated prediction, wash Elo), the wide shape stays dominated. Real NNUE
affords wide first layers because in a strength-unsaturated domain the width buys
accuracy; that condition does not hold for this task.

The vectorized path is opt-in via `/arch:AVX2` (both paths are in-tree; the scalar
default is unchanged, so existing builds are unaffected). Realizing it in
production means adding `/arch:AVX2` to the native build scripts, which sets an
AVX2-CPU baseline (~2013+); left as a deliberate developer decision, not flipped
unilaterally.

## Theory-log impact

Theory 36 (dead-ReLU sparsity enables large speedup) is refined, not refuted: the
sparse tail is real and bit-identical, but it is not the whole leaf. The binding
constraint for a wide-first-layer head is the O(H) scalar accumulator read, not
the tail MACs. The efficient shape for the current scalar engine is a NARROW first
layer (the standard 128,64 head is already the fastest incremental MLP at 1.24
us/node); the wide shape's payoff is gated on a vectorized read.

## Implementation notes / differences from the design discussion

- The design discussion's size analysis ("bigger file, much cheaper leaf") was
  half right: the file is ~7% bigger than wide (70.7k vs 66.3k mu-head params) and
  the tail is much cheaper, but "cheaper leaf" did not follow, because the leaf
  cost is the O(H) read, not the tail. The size table underweighted the read.
- The mlp-sparsity tool's "2nd-layer delta ceiling = H/churn" line (21.3x for the
  NNUE head) describes a DIFFERENT optimization (the delta accumulator) and is not
  what the sparse leaf-tail forward realizes. The sparse tail's win is capped by
  static sparsity, and even fully realized it does not touch the O(H) read.

## Correctness / methodology caveats

- The speed A/B uses a fixed depth (d4) with no node budget, so node counts differ
  across heads (different eval -> different pruning). us/node normalizes this out;
  it is the per-node leaf cost, which is what the three heads actually differ on.
- `train.exe speed` at `--maxdepth 6` explodes in node count with no budget (it
  timed out last session); d4 is used for the per-node ratio, which is
  depth-independent for a fixed leaf.
- Swapping models into `models/pst_value.txt` for the A/B backs up and restores the
  original file; verify `git status` shows `models/pst_value.txt` unmodified before
  committing.
- Every Elo above is an `active = on` row at the stated search head, read from
  `ranking/standings.tsv` (added this session). Do NOT read standings from
  `ranking/ratings.tsv`: it is the full historical fit and also contains retired
  agents (`gone`, superseded `@N` code versions frozen at old game counts). Mixing
  those in produced a real error while writing this doc -- a retired `classic` row
  reads 1081 where its live identity is 990, which momentarily made the
  hill-climbed Advanced agent look worse than classic when at a matched head it is
  better (1036 vs 1009). Same trap for search heads: `ab(d6,tt,ord,nb200k)` and
  `ab(d6,ord,nb200k)` are different agents and their Elos are not interchangeable.
- The `ranking/ratings.tsv` `cpu_ms_move` column is NOT a valid cross-head speed
  comparison. Each agent's cpu is averaged over its whole game history: the existing
  dist MLP agents (s77/s78/s79) accumulated most of their games last session on
  older (pre-sparse-tail) binaries, so their ~730-2200 ms/move is not comparable to
  the NNUE agents' ~370 ms/move (all games this session on the current binary). The
  `train.exe speed` A/B on one binary is the authority for the per-node comparison.

## Future Work

- ~~**Vectorized (SIMD) leaf read.**~~ DONE this session (see the follow-up section
  above). It was implemented in AVX2 intrinsics and measured: a real ~1.2-1.3x leaf
  speedup for every shape, but a constant factor that does not change the ranking, so
  the NNUE-wide shape stays ~1.9x slower than the standard head. The negative shape
  verdict stands, now confirmed against a vectorized read rather than assumed. Remaining
  sub-item: decide whether to set `/arch:AVX2` as the native-build baseline to realize
  the general ~1.2x leaf speedup in production (requires an AVX2 CPU).
- **int16 quantization** of the accumulator for the full NNUE throughput recipe.
  Tethered to the vectorized-read conclusion: quantization multiplies SIMD lanes
  (32 int16 per AVX-512 register vs 16 floats) but breaks the bit-identical property
  and needs a retrain or post-training calibration.
- **A narrow-first-layer efficiency sweep.** The standard 128,64 head is the fastest
  incremental MLP measured. Is an even narrower first layer (e.g. 96 or 64 wide)
  cheaper still at equal prediction, given the O(H) read floor? Directly tests the
  "narrow is efficient for a scalar engine" claim this result implies.

## Ideas This Inspired

- The O(H)-read floor means per-node leaf cost of any accumulator MLP is ~linear in
  first-hidden width regardless of depth or tail sparsity. That gives a clean design
  rule: for a scalar leaf, spend width only where it buys prediction, because every
  unit costs a fixed read tax per leaf.
- A hybrid: keep the first layer narrow (cheap read) but make the SECOND layer the
  wide one, with the second layer's inputs already sparse. The read floor stays at
  the narrow first width, and the sparse tail handles the wide second layer. Untested.
- The mlp-sparsity tool could also report an ESTIMATED us/node from `a*H + b*tail`
  using calibrated constants, so a shape's speed is predictable before training.
