# Incremental ML Eval: Results (companion to incremental-ml-eval-plan-1-luminous-snail.md)

Session date: 2026-07-06

## Summary of changes

The session shipped the objective-agnostic incremental ML plumbing: a sparse
piece-square value feature layout (version 2) and a scalar accumulator (`g_mlAcc`)
that keeps a linear v2 model's output up to date across make/unmake for 2-3 weight
additions per move, instead of a 30-feature full board scan (including two full
move generations) at every leaf. A linear model over the v2 layout is an
incremental piece-square table (PST) with zero approximation. The developer's
joint value + policy + next-value research idea (train a ReLU net to keep hidden
units clamped across the played move, so it co-learns strength and its own
recomputation cost) was documented in `todo.md` for a later session.

Files changed:

- `src/ml_features.h` / `.cpp`: `MLV2_FEATURES=129` (64 White piece-square inputs
  via `mlSqW`, 64 Black via `mlSqB`, `MLV2_STM=128` side to move),
  `mlExtractValueFeaturesV2`, `mlValueFeatureNameV2` (built-once names table
  `w_a1..b_h8`, `side_to_move`).
- `src/globals.h` / `.cpp`: `g_mlAcc` (double), `g_mlIncremental` (bool),
  `g_mlWeights` (const float*), mirroring the `g_evalPos` family.
- `src/ml_eval.h` / `.cpp`: `mlValueScore` now dispatches the extractor on the
  model's `featureVersion()` (v1 dense, v2 sparse) so the full-scan path serves
  the GUI/Greedy/reference cases for both layouts. New incremental trio:
  `mlIncrementalBegin(slot)` (latch weights, seed `g_mlAcc` = bias + occupied
  piece-square weights), `mlLeafScore(turnColor)` (near-win shortcut, then tanh of
  accumulator + side-to-move term), `mlIncrementalEnd()`. The squash/round/clamp
  tail is factored into one helper (`mlSquashToEval`) shared by both paths so they
  cannot diverge.
- `src/ai_eval.cpp`: `evalBeginSearch` enables the ML path when the evaluator is
  LearnedValue (recognized by fn pointer inside the registry's own file, because
  `breakthrough.exe` does not link `agents.cpp`), `evalLeaf` short-circuits to
  `mlLeafScore` while `g_mlIncremental` is set, `evalEndSearch` tears down.
  LearnedValue's registry `incremental` flag stays false (it drives only the
  heuristic `g_evalPos` path).
- `src/moves.cpp`: the four make/unmake functions apply the 2-3 weight
  add/subtract delta, guarded by `g_mlIncremental`, next to the existing
  `g_evalPos` update lines.
- `src/ml_train.cpp` + `src/ml_train.h` + `tools/train_main.cpp`:
  `selfplay-supervised` takes `--feature-version` (1 default, 2 sparse), threading
  it through position capture, model construction, SGD, and the manifest
  conditions. `speedBench` gained two AB+LearnedValue depth ladders
  (`learned-v1-scan` slot 0, `learned-v2-incr` slot 2 loading
  `models/pst_value.txt`) so one `train.exe speed` run prints the comparison.
  `exportDocs` emits a v2 value-features section into `ML.md`.
- `tests/test_ml.cpp`: 29 new assertions (see Tests below).
- Docs: `README.md` ML section (two feature layouts + the 9x number),
  `CLAUDE.md` (file tables, Key Global State, trainer note, 437 assertions),
  `ML.md` autodoc regenerated, `todo.md` (substrate crossed off under the
  incrementalize item + the new joint-objective entry).
- Trained artifact: `models/pst_value.txt` (linear, head=value,
  feature_version=2, 129 weights).

## Measured results

`train.exe speed --positions 24 --ms 400 --seed 42 --max-depth 6`, same 24
mid-game positions for every row. us/move divided by nodes/move gives cost per
node, which is the number the two learned paths differ on (their trees differ
because their evals differ, so per-node cost is the honest comparison):

| depth | v1 full-scan us/node | v2 incremental us/node | per-node speedup |
|---|---|---|---|
| 3 | 0.463 | 0.053 | 8.8x |
| 4 | 0.472 | 0.053 | 8.9x |
| 5 | 0.446 | 0.052 | 8.5x |
| 6 | 0.475 | 0.053 | 9.0x |

Headline: the incremental learned leaf costs about 9x less per node than the
full-scan learned leaf, flat across depths as expected (the saving is per-leaf,
not per-tree). At depth 3 the two trees happen to be nearly the same size (3446
vs 3536 nodes), and the whole-move time shows the same factor directly: 1596.6 ->
186.9 us/move (8.5x). The v2 incremental leaf now sits within ~2x of the cheapest
heuristic search (`chip` at 0.051 us/node at d3) instead of ~9x above it.

Raw ladder (us/move, nodes/move): v1-scan d3 1596.6/3446, d4 35338/74874, d5
85827/192432, d6 3926563/8268231; v2-incr d3 186.9/3536, d4 1436.4/26966, d5
13189/252229, d6 67788/1287487.

Training run (`selfplay-supervised --feature-version 2 --games 250 --epochs 6
--seed 20260706`): 10808 positions, loss 0.750 -> 0.735, checkpoint winrate vs
TieredRandom (Greedy 1-ply) 0.19-0.25. The v2 PST predicts outcomes worse than
the v1 aggregates (~0.68 loss), which is expected: it sees only piece placement,
no mobility/threat aggregates. Strength was not the goal of this milestone; the
NNUE step (vector accumulator + hidden layers on these same seams) is where v2
accuracy is supposed to come from.

## Implementation notes and differences from the plan

- As planned, with one addition: the plan's "leaf read applies tanh(acc + stm)"
  was implemented via a shared `mlSquashToEval` helper used by both mlValueScore
  and mlLeafScore, rather than duplicating the clamp in two places.
- The plan called for detecting LearnedValue in `evalBeginSearch` via
  `learnedValueIndex()`; that function lives in `agents.cpp`, which
  `breakthrough.exe` does not link. Solved with a fn-pointer comparison against
  `evalLearnedValue` inside `ai_eval.cpp` (a forward declaration above
  `evalBeginSearch`).
- `speedBench` previously had no AB+LearnedValue ladder at all (only Greedy
  1-ply), so the "incremental off vs on" comparison was added there as two
  ladders in one run instead of a separate toggle. There is deliberately no
  runtime off-switch for the incremental path; the v1 model IS the full-scan
  baseline.
- The v2 autodoc section prints the layout compactly (`w_a1..w_h8`, etc.) rather
  than enumerating 129 names.

## Correctness gotchas

- Float vs double summation: the full-scan path sums float in feature-index
  order, the accumulator sums double in move order. After tanh, x900 scaling, and
  rounding, the two can legitimately differ by 1 integer step. The tests assert
  the accumulator matches a from-scratch double recompute to 1e-4 and the two
  leaf paths agree within +-1. Within one search only the incremental path runs,
  so tree decisions never mix the two.
- The side-to-move weight (feature 128) must not live in the accumulator, or
  unmake would need to know whose turn it was; it is applied at read time in
  `mlLeafScore`.
- Capture bookkeeping mirrors the piece-placement lines exactly: a White capture
  also removes the Black weight at the destination square (and unmake re-adds
  it). The randomized-position walk with captures at depth 3 guards this.
- The heuristic-evaluator guard matters: a v2 model sitting in slot 0 must not
  enable the ML path when Classic/Experimental searches run. Covered by a test.

## Tests

`.\tools\run_tests.ps1 -Build`: all 437 assertions in 47 test cases pass (was
408). New in `tests/test_ml.cpp`:

- v2 extraction: exact indices set, nonzero count, stm sign, names.
- v2 save/load round trip preserves feature_version=2 / count 129.
- Accumulator walk (crafted capture-heavy position depth 3 both colors, plus
  board1 depth 2): `g_mlAcc` equals a full recompute after every make and every
  unmake, and incremental vs full-scan leaf evals agree within +-1 at every node.
- Off-path guards: v1 model leaves `g_mlIncremental` false with exact
  fallback equality; Classic search with a v2 model in the slot stays heuristic.

## How to test / reproduce

1. `.\tools\run_tests.ps1 -Build` (437 assertions).
2. Train: `.\tools\run_train.ps1 selfplay-supervised --out models/pst_value
   --feature-version 2 --games 250 --epochs 6`.
3. Speed: `.\tools\run_train.ps1 speed --positions 24 --ms 400 --seed 42
   --max-depth 6` and compare the `learned-v1-scan` vs `learned-v2-incr` rows.
4. Console: play MiniMax + LearnedValue (evaluator 2, model slot 0) vs
   TieredRandom; verified one full game completes with sane `now=`/`pred=`
   readouts (White won).
5. Elo datapoint (run, see below): `rank.exe gauntlet --id
   "ab(d6,tt,ord,nb200k)@1.learned(s2,d9426d29)@1" --games 4`.

## Gauntlet Elo (follow-up in the same session)

`slotFile` in `ranking.cpp` gained the slot-2 convention
(`models/pst_value.txt`) and `check` now prints its hash, so learned v2 agents
are rankable as `learned(s2,<hash8>)`.

Gauntlet: `ab(d6,tt,ord,nb200k)@1.learned(s2,d9426d29)@1`, 4 games vs each of
the 53 active roster agents (216 games):

- **Elo 595 +/- 27** (pool ratings held fixed). That lands between
  `ab(d2).classic` (611) and the tuned greedy (488): a depth-6 search steered by
  the untuned PST plays like a depth-2 classic search. The evaluator, not the
  search, is the bottleneck.
- **cpu 10.83 ms/move, 58.3k nodes/move, 0.186 us/node** over its 6017 moves.
  Two things inflate this vs the speed benchmark's 0.053 us/node: the gauntlet
  agent runs TT+ordering (positionKey's per-node string build is the known
  wall-clock tax) and the flat PST prunes worse than Classic (58k vs ~19k
  nodes/move for the classic d6 twin), an eval-quality effect, not an
  accumulator cost.
- **eff = 595 / log2(1 + cpu_us/move) = 44**, versus 80 for its classic twin
  `ab(d6,tt,ord,nb200k).classic@2` (Elo 961) and 115 for plain `ab(d4)` (858).
  So per unit of computation the untuned PST agent is currently poor: it spends
  depth-6 compute for depth-2 strength. The incremental machinery is doing its
  job (the leaf is cheap); the weights are the weak part, which is exactly what
  the NNUE/training milestones are for. Caveat: the model is data-limited (250
  d2-teacher games, loss flat at ~0.735 by epoch 6), not epoch-limited; more
  games and a stronger/deeper teacher are the first levers.

## Candidate commit messages

All uncommitted work from this session plus the pre-existing modified files is
covered by:

1. `Add incremental ML eval: sparse piece-square features (v2), g_mlAcc search
   accumulator, ~9x cheaper learned leaf` (top recommendation)
2. `Add feature-v2 piece-square value models with incremental search accumulator
   + speed benchmark ladders`
3. `Update ML eval with sparse v2 features and NNUE-style incremental
   accumulator; document joint-objective model idea`
