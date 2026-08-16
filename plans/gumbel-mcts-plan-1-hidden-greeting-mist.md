# Gumbel AlphaZero bot -- Slice 1: joint model + Gumbel MCTS explorer

## Context

The developer asked for a new bot based on Gumbel AlphaZero (Danihelka et al.,
2022, "Policy improvement by planning with Gumbel") with a trained NN. This
project's `todo.md` already lists "MCTS / PUCT (pairs a policy head with a
value head)" as a `[Later]` item under Move-Tree Explorers, and has no MCTS of
any kind yet -- only `Greedy` and `AlphaBeta` (`src/explorers.cpp`). Gumbel
AlphaZero is a specific MCTS variant: at the root it replaces Dirichlet-noise
exploration with **Gumbel-top-k sampling + Sequential Halving** over a small
simulation budget, and at every node it replaces PUCT with a **deterministic
action-selection rule** whose visit distribution converges to
`softmax(logits + sigma(completedQ))`. That mechanism is what gives it a
provable policy-improvement guarantee at very low simulation counts (16-200),
unlike vanilla AlphaZero/PUCT which needs thousands. Building a shortcut
version of the search would not actually be "Gumbel AlphaZero," so this plan
implements the real algorithm.

Per this project's `Docs/model-training-playbook.md`, a new ML capability is
scoped and confirmed with the developer before code is written, and a full
AlphaZero-style build (new joint network + new search + a self-play training
loop + roster integration) is too large for one reviewable unit. The developer
chose to split it in two:

- **Slice 1 (this plan):** the joint value+policy model type and the Gumbel
  MCTS search engine itself, validated by tests and manual/`rank.exe` play
  with a hand-built or randomly-initialized model. No training regime yet.
- **Slice 2 (a separate future plan):** the self-play training regime that
  actually trains the network, using a **bootstrapped search-value target**
  (the MCTS's own improved value estimate, not just game outcome) per the
  developer's stated preference -- the fuller AlphaZero-style signal.

Also confirmed with the developer: heads start from scratch (no bootstrapping
from an existing value model), and this bot only needs to be playable via
`rank.exe` / `train.exe` / automated tests for now -- the interactive console
and GUI are not in scope (the console's player selection is a fixed enum in
`src/main.cpp`, not the generic `AgentSpec`/explorer registry the rest of the
system uses, so wiring it in is separate, already-open scope in `todo.md`'s
GUI track).

## Design

### Key integration insight: zero changes to `ExplorerDef`/`AgentSpec`

`LearnedValue`'s existing convention already threads a model slot through
`evalParams[0]` (`src/ai_eval.cpp`), and `ExplorerDef::fn`'s signature
`(side, evaluator, params, budget)` already hands the explorer that evaluator
index + params array. The new joint model lives in **one slot** and supplies
both heads, so:

- `AgentSpec` is built exactly like any other search agent:
  `agentMakeSearch("GumbelBot", gumbelExplorerIdx, learnedValueIndex(), simBudget, slot)`.
- The explorer reads the value head via the existing `mlValueScore(turnColor, slot)`
  (`src/ml_eval.cpp`) unchanged -- it already does the near-win shortcut,
  feature-version dispatch, and tanh/clamp squash, and works automatically
  because `JointModel::forward()` delegates to its value head (the same
  pattern `DistModel::forward()` already uses to drop into `LearnedValue`).
- The explorer reads the policy head itself: `mlGetModel(slot)` (existing
  accessor, `src/ml_eval.h:38`), a `typeName()`-based check (the same
  unwrapping idiom `ml_eval.cpp`'s `mlIncrementalBegin` already uses for
  `ResidualModel`/`DistModel`), then a new non-virtual `policyForward(x, n)`
  method called per legal move on features from the existing
  `mlExtractMoveFeatures` (`src/ml_features.h`). This deliberately bypasses
  `mlRateMoves` (`src/ml_eval.cpp:269`), which requires `head()==HEAD_POLICY`
  and is single-head-only; `JointModel::head()` returns `HEAD_VALUE` (so it
  validates correctly everywhere a value model is expected), and the new
  explorer is the only caller that needs the policy side.

Net effect: no struct in `agents.h`/`explorers.h` changes at all. This is the
same reuse discipline `DistModel` and the Risk weight already established for
"a new capability rides the existing slot/evaluator plumbing."

### New model type: `JointModel` (`src/ml_model.h` / `.cpp`)

Mirrors `DistModel`'s "two owned heads" shape exactly, but the two heads are a
genuinely different kind of pair (value + policy, not mean + volatility):

```cpp
struct JointModel : public Model {
    Model* valueHead;   // board features v2 (129) -> scalar, HEAD_VALUE
    Model* policyHead;  // move  features v1 (9)   -> scalar, HEAD_POLICY

    const char* typeName() const override { return "joint"; }
    int head() const override { return HEAD_VALUE; }               // drop-in for LearnedValue
    int featureVersion() const override { return valueHead->featureVersion(); }
    float forward(const float* x, int n) const override { return valueHead->forward(x, n); }
    float policyForward(const float* x, int n) const { return policyHead->forward(x, n); } // new, non-virtual

    bool save(const string& path) const override;      // v_/p_ key-prefix split, like dist's mu_/s_
    void writeWeights(std::ostream& f) const override;
};
```

Both heads default to `LinearModel` per this project's playbook guidance
("default to linear architecture for a new regime's first pass") -- value over
feature v2, policy over move-feature v1, exactly the layouts `LearnedValue`
and `LearnedPolicy` already use, so no new feature engineering is needed.
`makeModel`/`loadModel` gets a `type=joint` factory case reusing
`buildLinearFromKV`, the same helper `dist`/`residual` already share. Add a
`g_modelTypes[]` row (`{"joint", "Two-headed value+policy model (Gumbel-style search substrate)", true}`).

Since slice 1 has no training regime, "from scratch" means a small helper
(test-only, not a CLI subcommand) that constructs a `JointModel` with
`LinearModel::initRandom()`-equivalent random weights, saves it to a slot file,
and is what the end-to-end tests and any manual `rank.exe gauntlet` smoke
check load. No `train.exe` surface is added in this slice.

### New search engine: `src/ai_gumbel.h` / `src/ai_gumbel.cpp` (new files)

Follows the file-separation precedent `ml_tdleaf.cpp` set (a genuinely new
tree-walk mechanism gets its own file rather than perturbing
`ai_minimax.cpp`'s hot recursion). Uses the same make/unmake tree-walk pattern
proven safe for recursive nested use by `ai_minimax.cpp`'s own
`maxAlphaBetaOrdered`/`minAlphaBetaOrdered` (`generateMoves` +
`simulateMoveWhite/Black` + recursive descent + `unsimulateMoveWhite/Black`,
no board copies) -- confirmed via Explore-agent research this session.

Core pieces, following Danihelka et al. 2022's Algorithm 1:

- **Node**: legal moves at this node, each with a prior logit (from
  `policyForward`), a visit count, a value sum, and lazily-created children
  (heap-allocated, one new node per simulation -- the standard MCTS growth
  pattern; no perf optimization in this slice, correctness first).
- **Root: Gumbel-top-k selection.** Draw one Gumbel variate per legal move
  (`g_a = -log(-log(uniform))`), rank by `g_a + logit_a`, keep the top
  `m = min(configured_m, numLegalMoves)` as the root's surviving candidates.
  This replaces Dirichlet-noise exploration.
- **Root: Sequential Halving.** Distribute the total simulation budget
  (`budget`, the existing `ExplorerDef::fn` parameter -- reused as "total sims"
  the same way `AlphaBeta` reuses it as "depth") across
  `ceil(log2(m))` rounds; each round runs simulations from every surviving
  candidate, then keeps the better half by `logit + sigma(completedQ)` until
  exactly one root action remains.
- **Every node (root candidates during halving, and every non-root node during
  each simulation's descent): deterministic action selection.** Pick the
  action maximizing `softmax(logits + sigma(completedQ))[a] - N(a)/(1+sum(N))`
  -- a greedy rule whose empirical visit fractions provably converge to that
  target softmax. `completedQ(a)` is the backed-up mean value for a visited
  child; for an unvisited child it substitutes the parent's own mixed value
  estimate (network value blended with visited children, simplified for this
  slice to the network's raw root value where the paper's fuller `v_mix`
  would apply -- flagged in code as a documented simplification, not hidden).
- **`sigma` transform**: `sigma(q) = (c_visit + max_b N(b)) * c_scale * q`,
  with the paper's defaults (`c_visit=50`, `c_scale=1.0`) as configurable
  constants, not hardcoded literals, matching this project's convention of
  exposing search knobs.
- **Value units**: internal search values are the tanh-squashed white-centric
  range (consistent with `mlValueScore`'s own output), so `sigma`/`completedQ`
  math operates on a fixed, bounded scale.
- **Terminal handling**: reuse `nearWinCheck`/`canWinWhite`/`canWinBlack`
  (`src/ai_eval.h`) exactly as `ai_minimax.cpp` and `ml_tdleaf.cpp`'s
  `walkPV` already do, so win detection can't diverge between search paths.
- Two entry points, `gumbelSearchWhite`/`gumbelSearchBlack(int simBudget, int
  slot, ...) -> victor code`, registered into `g_explorers[]` as
  `{"GumbelMCTS", "Gumbel-top-k root sampling + Sequential Halving MCTS
  (Danihelka et al. 2022); pairs a policy+value joint model.", gumbelExplore}`
  in `src/explorers.cpp`, mirroring `alphaBetaExplore`'s thin-wrapper shape.
- The internal search function also exposes (for slice 2's later use, so this
  slice's structure doesn't need reworking) the root's final visit counts and
  completed-Q values -- the exact ingredients of the Gumbel-improved policy
  target -- via an optional out-parameter, unused by the registered explorer
  wrapper itself in this slice.

### Roster / ID grammar wiring (`src/ranking.h` / `.cpp`)

Needed because the developer wants this playable via `rank.exe`. New head
grammar token, documented in `ranking.h`'s grammar comment alongside `ab(...)`:

```
gaz(sims=" N { "," "cvisit=" N | "cscale=" N | "m=" N } ")" "@" V
```

(`sims=` is the total simulation budget; `cvisit=`/`cscale=`/`m=` are omitted
at their paper defaults, same "subset, defaults omitted" convention every
other weight/flag list already follows.) `learned()` gains a new `mutype`
token `joint` with `value_shape=`/`policy_shape=` (dash-separated per-head
layer widths), mirroring `dist`'s `mu_shape=`/`sigma_shape=` exactly.

**Determinism correctness gotcha found while reasoning through this (worth
flagging on its own):** `rankAgentIsDeterministic` (`src/ranking.cpp`)
currently treats only dilution, the `rand` opener, and the random-chooser
family as consumers of `rand()`, which is what lets `pairGameTarget` pin a
deterministic-vs-deterministic pair at exactly 2 games. Gumbel-top-k sampling
draws from `rand()` on **every move** by construction -- any agent wearing the
new `gaz(...)` head is never deterministic. This must be added to
`rankAgentIsDeterministic`'s classification, or a `gaz` agent would be
mis-pinned at 2 games/pair like a deterministic pair, silently starving its
error bars exactly the way `Docs/benchmarking.md`'s defect 3 describes for
other pairs. This is a required part of the ranking wiring, not optional
polish.

Also: extend whatever ranking-codec completeness test currently enforces
`g_evaluators`/`g_rkEvals` coverage (`tests/test_ranking.cpp`) to also cover
the new explorer + `joint` mutype, so a future explorer addition can't silently
skip codec wiring.

### Slot allocation

Claims **634-649** (a small block; the free range starts at 634 per this
session's research into `src/CLAUDE.md`'s ledger) for this slice's hand-built/
random-init test models. Recorded in `src/CLAUDE.md`'s `slotFile()` allocation
note as required by `Docs/model-training-playbook.md`.

## Files touched

| File | Change |
|---|---|
| `src/ml_model.h` / `.cpp` | `JointModel` class, `type=joint` factory/loader case, `g_modelTypes[]` row |
| `src/ai_gumbel.h` (new) / `.cpp` (new) | The Gumbel MCTS search engine |
| `src/explorers.cpp` | Register `GumbelMCTS` in `g_explorers[]` |
| `src/ranking.h` | Grammar-comment update for `gaz(...)` head + `joint` mutype |
| `src/ranking.cpp` | Codec table row for the new head, `learned()` `joint` mutype support, `rankAgentIsDeterministic` fix |
| `tests/test_ml.cpp` | `JointModel` forward/save/load round-trip (mirrors existing `LinearModel` tests) |
| `tests/test_gumbel.cpp` (new) | Gumbel-top-k / Sequential Halving / sigma / non-root-selection closed-form checks; end-to-end legal-full-game test; "saved model loadable by the real search path" test (mirrors the TD-Leaf test tiers) |
| `tests/test_ranking.cpp` | Extend codec-completeness coverage to the new head/mutype |
| `ML.md` | New model-type + explorer rows (via `train.exe docs` autodoc) plus a hand-written prose section on the joint model + Gumbel search, citing Danihelka et al. 2022 |
| `Docs/works-cited.md` | Add the Gumbel AlphaZero paper citation |
| `src/CLAUDE.md` | New file-table rows for `ai_gumbel.*`; `ml_model.cpp`/`ranking.cpp` entries updated; slot ledger updated (634-649 claimed) |
| `todo.md` | Update the "MCTS / PUCT" line: explorer + model shipped this slice, self-play training regime (slice 2) still open -- not marked `[done]`, since there is no trained bot yet |
| `plans/gumbel-alphazero-plan-1-<suffix>.md` + `...-results-1-<suffix>.md` | This plan archived, paired with a results doc written at completion per the standard workflow |

## Verification

1. `.\tools\run_tests.ps1 -Build` passes, including the new
   `tests/test_gumbel.cpp` assertions and the extended `test_ml.cpp`/
   `test_ranking.cpp` coverage.
2. `.\tools\run_train.ps1 -Build docs` regenerates `ML.md`'s AUTODOC region;
   confirm the `joint` model type and `GumbelMCTS` explorer rows appear.
3. Hand-build a small random-weight `JointModel`, save it to slot 634, run
   `rank.exe check` against a roster line using the new
   `gaz(sims=50)@1.learned(model=634,<hash8>,unknown,joint,value_shape=129-1,policy_shape=9-1)@1`
   id to confirm it parses and round-trips.
4. `rank.exe gauntlet --id "<that id>" --games 4` as a plumbing smoke test --
   confirms the whole stack runs a handful of real games through the actual
   `rank.exe` runner without crashing or producing an illegal game. This is
   explicitly **not** a strength claim (the weights are random), matching this
   project's Pass-1 discipline: "it runs, here's what I checked," never a
   strength claim.
5. Report back to the developer what was verified and anything that looked
   off before calling this slice done, per the model-training-playbook's
   "After Pass 1" interactivity requirement -- and confirm whether slice 2
   (the self-play training regime) should be scoped next.
