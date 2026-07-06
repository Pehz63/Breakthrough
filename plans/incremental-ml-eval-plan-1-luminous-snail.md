# Incremental ML Eval: Sparse Piece-Square Plumbing + Joint-Objective TODO Entry

## Context

The developer wants to work on incrementalizing an ML model (the `[Later]` item in todo.md's value-model list). Exploration showed the existing linear value model cannot be incrementalized: its 30 features are dense board-wide aggregates (rank counts, phalanx/defended/threat counts, furthest-pawn maxima, and mobility via two full `generateMoves` calls), with no square-to-feature locality. `LearnedValue` is registered `incremental=false` and runs the full scan at every leaf.

The session decision: build the objective-agnostic incremental engine plumbing now, proven end to end with a linear value model over sparse piece-square inputs (an incremental PST, scalar accumulator, zero approximation). Separately, document the developer's joint-objective research idea in todo.md: train a ReLU net with three heads (value, policy, next-value) plus a penalty that maximizes the count of hidden units whose output stays ReLU-clamped at 0 across the played move, so the model co-learns playing strength and its own recomputation cost. The plumbing built here is the substrate that idea plugs into later (NNUE-style: widen the scalar accumulator to a vector, add hidden layers).

## Reference: the existing incremental seams (from exploration)

The heuristic accumulator `g_evalPos` already defines the pattern to mirror:

- Seed: `evalBeginSearch` (src/ai_eval.cpp:144-150), full scan once per search
- Per-move delta: guarded before/after pairs in the four make/unmake functions (src/moves.cpp:277/288, 295/307, 311/322, 326/338)
- Leaf read: `evalLeaf` (src/ai_eval.cpp:161-167), branch on `g_evalIncremental`
- Teardown: `evalEndSearch` (src/ai_eval.cpp:152-156)
- RAII: `EvalSearchScope` (src/ai_minimax.cpp:16-21), instantiated at both search entry points (253, 409)
- Equivalence guard: the walk test in tests/test_eval.cpp asserting accumulator == full recompute

Inside each make/unmake the changed squares are fully known: from `(moveX1, moveY)`, to `(moveX2, moveY+1)` for White or `(moveX2, moveY-1)` for Black, plus `isCapture` (the captured piece is the enemy color on the destination square).

## Step 1: Document the joint-objective idea in todo.md

Add under "## Models (value head: board -> scalar)", next to the existing incrementalize item, a new research entry (suggested tag `[Later]`, developer may adjust):

- Joint value + policy + next-value model trained to minimize its own recomputation: with ReLU units, if a hidden unit remains clamped at 0 before and after a move, its activation contributes no changed downstream value (equivalently, the derivative through the ReLU pre-activation is 0 except at the kink), so penalize (L0/L1 surrogate) the count of hidden units whose output changes across the move the policy head picks. The model must jointly learn the board value, the best move, and the successor value, because which units must recompute depends on which move is played. Caveats to record: the sparsity term must stay a light regularizer or the model prefers cheap moves over good ones, alpha-beta explores all moves so architectural locality (conv / locally connected first layer) bounds worst-case cost while learned clamping cheapens the chosen lines, and training needs the Python track (custom three-head loss is beyond the C++ SGD in ml_train.cpp).

Also update the incrementalize `[Later]` item to note the substrate shipped (after this session's work lands).

## Step 2: Sparse piece-square feature set (version 2)

In src/ml_features.h / .cpp:

- `MLV2_FEATURES = 129`: index 0-63 = White piece on square (x + 8*y), 64-127 = Black piece on square, 128 = side to move (+1 White, -1 Black). Values 0/1 (and +-1 for stm).
- `mlExtractValueFeaturesV2(int turnColor, float* f)`: single board scan filling the binary vector. No move generation, no aggregates.
- Names table `kValueNamesV2[]` (e.g. `w_a1..`, `b_h8..`, `side_to_move`) for the ML.md autodoc.
- Keep the v1 extractor untouched. Feature version is already per-model (`featureVersion()` on Model, `feature_version=` in the file format), so no global version bump.

## Step 3: Model support

src/ml_model.cpp needs no structural change: `LinearModel` already carries `featVer` and an arbitrary `n`, and `loadModel`/`save` round-trip `feature_version=2`, `feature_count=129`. Verify only.

In src/ml_eval.cpp, make `mlValueScore` dispatch the extractor on the loaded model's `featureVersion()` (1 -> v1 aggregates, 2 -> v2 sparse) so the full-scan path also works for v2 models outside search (GUI readout, greedy explorer, reference path for tests).

## Step 4: Incremental accumulator

New globals (globals.h extern + globals.cpp definitions), mirroring the g_evalPos family:

- `double g_mlAcc` (running dot product of sparse features with the model's weights, bias included at seed; double to keep add/subtract drift below test epsilon)
- `bool g_mlIncremental` (gates the make/unmake updates, exactly like `g_evalIncremental`)
- `const Model* g_mlActiveModel` (weights + outputScale for delta updates and leaf read)

Hook the four seams:

1. **Seed** in `evalBeginSearch`: if the evaluator is `LearnedValue` (`learnedValueIndex()`) and the slot's model is head=value with featureVersion 2, set `g_mlIncremental`, latch the model, and seed `g_mlAcc = bias + sum of weights of occupied piece-squares` (one board scan). The existing `EvalDef.incremental=false` for LearnedValue stays as is: that flag drives the g_evalPos path, the ML path uses its own flag.
2. **Per-move delta** in the four make/unmake functions in src/moves.cpp, adjacent to the existing `g_evalPos` update lines, guarded by `g_mlIncremental`. White make: `g_mlAcc -= w[wIdx(sx,sy)]; g_mlAcc += w[wIdx(dx,dy)]; if (isCapture) g_mlAcc -= w[bIdx(dx,dy)];`. Unmake reverses. Black symmetric. 2-3 weight ops per move, no rescans, same spirit as `evalPosLocal`.
3. **Leaf read** in `evalLeaf`: new branch when `g_mlIncremental`: `nearWinCheck` first (same shortcut `mlValueScore` uses), then `tanh(g_mlAcc + stmW * (turnColor==White ? 1 : -1)) * outputScale`, clamped to `ML_EVAL_CAP` exactly as `mlValueScore` does. The side-to-move weight (feature 128) is applied at read time, never stored in the accumulator.
4. **Teardown** in `evalEndSearch`: clear `g_mlIncremental` and the model pointer.

Expose the weight array to moves.cpp cheaply: either a `const float* g_mlWeights` latched at seed time or an inline accessor on LinearModel. Prefer the raw pointer, matching how `g_activeParams` works.

Correctness gotcha to handle: the incremental leaf and the full-scan `mlValueScore` must agree. Accumulate in double and compare pre-rounding values in the test with epsilon 1e-4, then assert the rounded ints match on the test walk.

## Step 5: Train a v2 PST model

In src/ml_train.cpp + tools/train_main.cpp: add `--feature-version 2` (default 1) to `selfplay-supervised`, routing position capture through the v2 extractor and constructing the LinearModel with featVer=2, n=129. Output e.g. `models/pst_value.txt`. The existing `sgdLogisticStep` works unchanged on any feature vector. Keep epochs small per the standing overfit note.

## Step 6: Tests (tests/test_ml.cpp, plus test_eval.cpp if cleaner for the walk)

- v2 extraction determinism and correctness on a hand-set board (exact indices on, all else 0).
- Accumulator walk: load a v2 model, seed via `evalBeginSearch`, run a randomized make/unmake tree (mirror the g_evalPos walk in test_eval.cpp), assert `g_mlAcc` equals a fresh full-scan dot product at every node (epsilon 1e-4).
- Leaf equivalence: `evalLeaf` under `g_mlIncremental` equals `mlValueScore` full scan for the same positions.
- v1 model still loads and scores (no regression on featureVersion dispatch).

## Step 7: Docs and workflow (per CLAUDE.md standing instructions)

- README.md: update the ML/AI description touching LearnedValue (now supports an incremental sparse piece-square model).
- CLAUDE.md: file-table rows for ml_features/ml_eval/ai_eval/moves reflecting the v2 features and the g_mlAcc family, and the Key Global State table.
- ML.md autodoc: run `train.exe docs` after the names table lands.
- todo.md: Step 1's new entry, and cross out nothing yet except updating the incrementalize item's note.
- Archive this plan to `plans/` under the repo naming style and write the companion results doc with before/after numbers.
- Suggest 2-3 candidate commit messages, no commits (developer commits manually).

## Verification

1. `.\tools\run_tests.ps1 -Build` passes (408 existing assertions + the new ones).
2. Train a quick v2 model: `.\tools\run_train.ps1 -Build selfplay-supervised --feature-version 2 --games 100 --epochs 4 --out models/pst_value.txt`.
3. Speed proof, the point of the session: run the same LearnedValue agent (v2 model, fixed depth, fixed seed) with the incremental path forced off vs on, compare nodes/sec or cpu/move (via `train.exe speed` if it fits, else a small tournament pair or `rank.exe gauntlet`). Record the numbers in the results doc. Expect a large leaf-cost drop since v1's per-leaf cost included two generateMoves calls and a 64-square scan.
4. Optional strength datapoint: `rank.exe gauntlet` the new PST agent (its model hash gives it a fresh identity, no codec change needed) against the frozen pool for a real Elo.
5. Console sanity: one quick game with a LearnedValue MiniMax side, confirm eval readout behaves.

## Explicitly out of scope this session

- The joint value/policy/next-value training objective itself (documented only).
- NNUE hidden layers / vector accumulator (next milestone on these same seams).
- Python training track, conv architectures, Zobrist TT hashing.
