# Gumbel AlphaZero bot -- Slice 2, Pass 1: self-play trainer (sanity only)

## Context

Slice 1 (shipped 2026-08-16, `plans/gumbel-mcts-plan-1-hidden-greeting-mist.md` /
`...-results-1-...md`) built the `JointModel` (value + policy heads,
`src/ml_model.h`/`.cpp`) and the `GumbelMCTS` search engine (Gumbel-top-k root
sampling + Sequential Halving, `src/ai_gumbel.h`/`.cpp`), registered as the
`gaz(sims=N)@1` head. No training regime exists yet -- the only model ever
loaded into it is a hand-built, randomly-initialized smoke-test file. `todo.md`
already records the deferred design (`## Move-Tree Explorers`, the "MCTS /
PUCT" entry's sub-bullet): train the value head against **a bootstrapped
search-value target** (the search's own improved value estimate, not game
outcome -- confirmed with the developer during Slice 1's planning) and the
policy head against **the Gumbel-improved policy target**
(`softmax(logits + sigma(completedQ))` over the root's final visit
counts/completedQ).

Per `Docs/model-training-playbook.md`, a new training regime goes through
three passes, and this plan is **Pass 1 only**: get one small self-play
configuration running correctly, prove every new mechanism does what it
claims, and confirm the resulting model is loadable through the real `gaz(...)`
search path. Pass 2 (a broad sweep, its configuration grid presented and
reviewed with the developer separately, per the playbook's "design the grid,
then stop and show it" rule) and Pass 3 (optimize) are explicitly follow-up
work, not part of this plan.

Confirmed with the developer this session (see the "Questions to clarify"
list in `Docs/model-training-playbook.md`):
- **Update schedule**: strictly online -- one SGD step per newly generated
  ply, not batched across whole games.
- **Self-play mechanism scope**: add a replay buffer now (mix recent plies
  across the run into each update, rather than training only on the ply just
  generated).
- **Model architecture**: linear for both heads (the project's established
  Pass-1 default; every current category champion is linear, and the
  developer's own reasoning -- faster convergence, proven not to saturate at
  this scale -- matches the playbook's own prior findings against
  higher-capacity heads, theory 37).
- **Initialization**: from scratch (decided during Slice 1's planning, applies
  here).

## Design

### Where the training signal already exists

`GumbelRootInfo` (`src/ai_gumbel.h`) is populated by every call to
`gumbelSearch(side, slot, simBudget, &info)` and already carries, for **every
legal root move** (not just the Gumbel-top-k survivors -- unvisited moves get
their `completedQ` filled in by the same parent-mean substitution the
non-root selection rule already uses):

```cpp
struct GumbelRootInfo {
    int    moveCount;
    double logits[ML_MAX_MOVES];
    double completedQ[ML_MAX_MOVES];      // mover-relative, in [-1,1]
    int    visitCounts[ML_MAX_MOVES];
    double rootValue;                     // raw network eval, white-centric
};
```

`rootValue` is the network's own leaf estimate computed *before* any
simulations (`gumbelSearch`, `src/ai_gumbel.cpp:259`) -- not the search's
improved estimate. Two small, additive extensions to `ai_gumbel.h`/`.cpp`
supply exactly what the two heads need, reusing the file's own existing
constants (`kGumbelCVisit`, `kGumbelCScale`) so nothing about the search's
own math has to be duplicated or re-derived elsewhere:

1. **`GumbelRootInfo::searchValue`** (new field): the root's mean backed-up
   value across every simulation, i.e. `root.meanValue()` at the point `info`
   is filled (both fill sites: the `n==1` forced-move branch and the main
   Sequential-Halving branch in `gumbelSearch`). This *is* white-centric
   already (every value threaded through `gumbelSimulate`'s backup is
   white-centric per its own docstring), so no conversion is needed at the
   fill site. This is "the MCTS's own improved value estimate" the developer
   asked for -- it differs from `rootValue` exactly when simulations moved the
   estimate away from the raw network read, which is also a natural Pass-1
   sanity check (run at `sims=1` vs `sims=200` and confirm they diverge; see
   Verification).

2. **`gumbelImprovedPolicy(const GumbelRootInfo& info, double* out)`** (new
   function): fills `out[0..info.moveCount)` with
   `softmax(logits[i] + sigma(completedQ[i], maxVisitCount, kGumbelCVisit, kGumbelCScale))`,
   the exact quantity `gumbelSelectAction`'s own visit-fraction-matching rule
   targets, evaluated over every legal root move using the search's *final*
   stats. Pure function of `info`'s own fields (recomputes `maxVisitCount`
   from `visitCounts[]`), so it needs no access to board state or the tree.

Neither change touches `gumbelSearch`'s control flow, the tree-walk, or the
registered `gumbelExplore` wrapper other explorers/agents call -- both are
outputs computed from data the search already has at the moment it returns.

### Value target: sigmoid-space, reusing the existing training convention

Every other value regime in this project (`trainSupervisedValue`, TD-Leaf)
treats a model's raw `forward()` output as a logistic-regression logit: the
model is trained by cross-entropy against a `[0,1]` target via
`Model::trainStep`/`gradStep` (`sgdLogisticStep` for `LinearModel`,
`src/ml_model.cpp:85`), and *separately*, `mlValueScore`'s search-time squash
runs the same raw output through `tanh` for display/search-magnitude purposes
(`src/ml_eval.cpp:48`). These are two different monotonic transforms of one
underlying scalar for two different purposes, already coexisting today.

`searchValue` lives in the tanh-squashed space (`gumbelLeafValue` divides
`mlValueScore`'s tanh-squashed int back down to `[-1,1]`), so training the
value head against it needs one linear rescale, not a new loss:

```
target = (searchValue + 1) / 2   // in [0,1]
```

then a plain `valueHead->trainStep(boardFeatures, MLV2_FEATURES, target, lr, l2, 0.0f)`
-- byte-identical machinery to every existing value regime, no new gradient
math, no new closed-form check beyond confirming the rescale (trivial).

### Policy target: softmax cross-entropy over one ply's move group

The policy head is a `LinearModel` scoring one move at a time
(`policyForward`, `src/ml_model.h`'s `JointModel`), so "the improved policy"
is a distribution over the *group* of a ply's legal moves, not a single
`trainStep` call. Given current policy logits `l_i = policyHead->forward(x_i)`
for the group and target distribution `p_i` from `gumbelImprovedPolicy`, the
softmax cross-entropy gradient is the standard closed form:

```
q = softmax(l)              // current model's own distribution over the group
gOut_i = q_i - p_i           // dL/dl_i
```

applied via `policyHead->gradStep(x_i, MLM_FEATURES, gOut_i, lr, l2)` per move
in the group. This is exposed as a small pure function in the new training
file (mirrors `tdLeafGradients`'s file-local, independently-unit-tested
pattern in `ml_tdleaf.h`): `gumbelPolicyGradients(logits, target, n, gOut)`.
Closed-form checks: gradient sums to ~0 across the group (softmax cross-entropy
invariant), and is ~0 everywhere when `target == softmax(logits)` already.

### Replay buffer

One ring buffer of **per-ply records**, each holding everything needed to
retrain that ply's targets against *whatever the model's weights are at
sample time* (not the weights when the ply was generated -- the policy side
needs a fresh softmax over the group under current weights, so a record
stores the raw features and targets, never a precomputed gradient):

```cpp
struct GumbelZeroRecord {
    float  boardFeatures[MLV2_FEATURES];
    double valueTarget;                 // (searchValue+1)/2, in [0,1]
    int    moveCount;
    float  moveFeatures[ML_MAX_MOVES][MLM_FEATURES];
    double policyTarget[ML_MAX_MOVES];  // gumbelImprovedPolicy output, sums to 1
};
```

Fixed-capacity circular buffer (oldest evicted first past capacity), pure
push/sample logic with no board dependency, so it is directly unit-testable
(capacity eviction, sample-without-replacement-within-a-draw). Self-play and
training interleave in one loop, one process, one thread (matching every
other regime here -- this project doesn't use worker threads for training,
`tools/CLAUDE.md`'s process-sharding pattern is how this project parallelizes
instead):

```
for each ply:
    run gumbelSearch(side, slot, simBudget, &info)   // also plays the move
    build a GumbelZeroRecord from the pre-move board + info
    push it into the replay buffer
    if buffer.size() >= warmup:
        sample one minibatch of records from the buffer
        for each sampled record:
            train the value head against valueTarget
            train the policy head against policyTarget (its own group softmax)
```

"Strictly online" is the *update cadence* (one step per new ply, no waiting
for a batch of whole games); the replay buffer is *what data feeds that
step* (a mix of recent plies, not only the one just generated) -- the two
developer answers compose cleanly rather than conflicting. In-memory only for
Pass 1 (no cross-process persistence/resume); flagged in Future Work as a
follow-up if a Pass-2/3 run turns out to need to resume a long training run,
mirroring TD-Leaf's `--ckpt-at` resumability model.

### Why this can't reuse `playGame`/`agentChooseMove`

`playGame` (`src/ml_train.cpp:140`) drives a game through the generic
`ExplorerDef::fn(side, evaluator, params, budget) -> victor` interface, which
returns only the victor code -- no root search diagnostics. `GumbelRootInfo`
is exactly the channel Slice 1 built for this (its header comment already
says "useful to a future self-play training regime... unused by the plain
registered explorer wrapper"), but it's only reachable by calling
`gumbelSearch` directly, not through `agentChooseMove`. So the new regime
gets its own small self-play loop (board reset via `reloadBoard`, direct
`gumbelSearch` calls, a max-half-move safety cap of 400 matching
`playGame`'s own convention) rather than reusing the generic runner --
consistent with how TD-Leaf also needed its own loop for the same reason (PV
access, not just a victor code).

### Config, entry point, files touched

New file pair `src/ml_gumbelzero.h` / `.cpp`, following the `ml_tdleaf.h`/
`.cpp` template exactly (a pure/testable core + a `GumbelZeroConfig` struct +
one `trainGumbelZero(cfg)` entry point):

```cpp
struct GumbelZeroConfig {
    string outPath;
    string boardFile;
    int    games;             // self-play games this run
    int    simBudget;         // gaz sims per move (both sides -- one model self-plays)
    unsigned seed;
    int    openPlies;         // uniform-random opening plies/side (diversity; no separate
                               // --explore knob needed -- Gumbel-top-k already resamples
                               // every move, unlike TD-Leaf's deterministic alpha-beta generator)
    double lr;
    double l2;
    int    replayCapacity;
    int    replayWarmup;      // don't start training until the buffer holds this many records
    int    batchSize;         // records sampled per training step
    int    ckptEvery;
    std::vector<int> ckptAt;  // rung ladder, same mechanism as TD-Leaf
    int    reportEvery;
};
GumbelZeroConfig gumbelZeroDefaults();
int trainGumbelZero(const GumbelZeroConfig& cfg);
```

From-scratch init builds both heads as zero-initialized `LinearModel`s exactly
like the Slice-1 smoke test (`tests/test_gumbel.cpp`: `LinearModel(HEAD_VALUE,
2, MLV2_FEATURES, 900.0f)` + `LinearModel(HEAD_POLICY,
mlMoveFeatureVersion(), MLM_FEATURES, 1.0f)`, wrapped in one `JointModel`) --
no random-symmetry-break needed (that's an `MLPModel`-only concern, not
applicable to linear heads).

Other files touched:

| File | Change |
|---|---|
| `src/ai_gumbel.h` / `.cpp` | `GumbelRootInfo::searchValue` field; `gumbelImprovedPolicy(info, out)` function |
| `src/ml_gumbelzero.h` (new) / `.cpp` (new) | The self-play trainer: config, replay buffer, loop, `gumbelPolicyGradients` |
| `src/ml_train.cpp` | `g_regimes[]` row for `"gumbelzero"` |
| `tools/train_main.cpp` | `gumbelzero` CLI subcommand (`--out`, `--games`, `--sims`, `--lr`, `--l2`, `--replay-capacity`, `--replay-warmup`, `--batch-size`, `--ckpt-at`, `--seed`, `--open-plies`, `--report-every`) |
| `src/ranking.cpp` | `R::of()` gains a `gumbelzero(` provenance prefix -> `"gumbel_self"` regime tag (mirrors `tdleaf(` -> `tdleaf_self`, `src/ranking.cpp:391`), so `learned()` ids for these models say how they were made |
| `build_train.bat`, `build_tests.bat` | add `src\ml_gumbelzero.cpp` (+ new test file) to the compile lists |
| `tests/test_gumbelzero.cpp` (new) | see Verification |
| `ML.md`, `src/CLAUDE.md`, `todo.md` | doc updates per the standard post-change workflow (below) |

Slot allocation: claims **650-659** for Pass-1 checkpoints (small on purpose
-- one config, a short `--ckpt-at` ladder, 2 seeds for the knob-validation
check in Verification; Pass 2 claims its own range later once its grid is
set, the same two-stage pattern TD-Leaf used: round 1 owned 128-165, Pass 2's
sweep claimed 166-633 separately).

## Verification (Pass 1 -- sanity, not a strength claim)

1. `tests/test_gumbelzero.cpp`:
   - `gumbelPolicyGradients` closed-form: sums to ~0 across a group; ~0
     everywhere when `target == softmax(logits)`.
   - `gumbelImprovedPolicy` closed-form: matches an independently computed
     softmax for a small hand-built `GumbelRootInfo`.
   - Replay buffer: push past capacity evicts oldest; sampling respects
     current size below capacity.
   - **Knob validation** (`CLAUDE.md`'s instrument-validation rule): call
     `gumbelSearch` at `sims=1` vs `sims=200` on the same position and confirm
     `searchValue` actually differs from `rootValue` and from each other --
     proving the search is doing something, not just echoing the network.
   - End-to-end: a tiny `trainGumbelZero` run (small `games`, `simBudget=50`
     matching Slice 1's own smoke-tested value, small replay capacity)
     completes without error, weights measurably move from their zero init,
     and the checkpoint is loadable through the *real* search path (`mlLoadSlot`
     + `agentChooseMove` on a `gaz(...)` agent playing real moves -- mirrors
     Slice 1's own "saved model loads through the real search path" test).
   - **Seed knob validation**: same config, two different seeds, confirm the
     resulting weights differ (proves the self-play + replay sampling
     randomness is actually live, not accidentally deterministic).
2. `.\tools\run_tests.ps1 -Build` passes in full (existing suite + the above).
3. Manual Pass-1 run: `train.exe gumbelzero --out models/sweep/slot650 --games
   <small> --sims 50 --ckpt-at "<small ladder>" --seed <s>`, publish the
   checkpoint(s), `rank.exe check` against a scratch roster line to confirm
   the id round-trips and the regime shows as `gumbel_self`, then a `rank.exe
   gauntlet --id "<id>" --games 4` plumbing smoke test (not a strength claim)
   against the live pool.
4. Report back to the developer per the playbook's Pass-1 interactivity
   requirement: what was verified, anything that looked off, and whether
   Pass 2 (the broad sweep -- sims budget, replay capacity/warmup, batch
   size, lr, seeds, all presented as a grid before running) should be scoped
   next.

## Post-change workflow (per root `CLAUDE.md`)

Update `README.md` (none of its sections describe training regimes directly,
but confirm), `ML.md` (extend the existing "Gumbel MCTS" section to note the
trainer exists, what Pass 1 verified, and that no Elo is certified yet),
`src/CLAUDE.md` (new file-table row, `ranking.cpp`'s regime list, the slot
ledger), `todo.md` (the deferred sub-bullet under "MCTS / PUCT" updated to
reflect Pass-1-shipped, still not `[done]` -- no certified Elo), archive this
plan + a companion results doc in `plans/`, run
`.\tools\run_tests.ps1 -Build` before committing, then commit.
