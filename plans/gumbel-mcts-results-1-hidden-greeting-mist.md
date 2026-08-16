# Gumbel AlphaZero bot -- Slice 1 results: joint model + Gumbel MCTS explorer

Companion to `plans/gumbel-mcts-plan-1-hidden-greeting-mist.md`. Read that
first for the design; this document records what actually shipped, how it was
verified, and what was found along the way.

## Summary

Shipped exactly the plan's scope: a `JointModel` (value + policy heads,
`src/ml_model.h`/`.cpp`), a `GumbelMCTS` explorer implementing Gumbel-top-k
root sampling + Sequential Halving + the deterministic non-root selection rule
(`src/ai_gumbel.h`/`.cpp`, new files), roster ID grammar wiring (`gaz(sims=N)@1`,
a `joint` mutype, a `rankAgentIsDeterministic` fix), and a test suite covering
the pure math, an immediate-win shortcut, a full-game legality run, and a
saved-model-loads-through-the-real-search-path check. No training regime --
that is slice 2, deliberately deferred per the plan.

**No Elo has been measured and none is claimed.** The only model that exists
is a hand-built, randomly-initialized `JointModel` used purely to exercise the
plumbing. Per `Docs/model-training-playbook.md`'s certification gate, this
agent must not be described as strong, promoted, or compared to anything in
the roster until slice 2 trains it and it clears a full-roster refit.

## How to test

```powershell
.\tools\run_tests.ps1 -Build      # full suite, includes tests/test_gumbel.cpp
.\tools\run_train.ps1 -Build docs # regenerates ML.md's AUTODOC region
```

Manual smoke test (what this session actually ran, since the console/GUI are
out of scope for this slice): build `rank.exe`, hand-build a random-weight
`JointModel` and save it to `models/sweep/slot634.txt`, then

```powershell
.\rank.exe check --roster <scratch-roster-with-one-anchor-and-one-gaz-line>
```

should print `OK`. A full `rank.exe gauntlet --id "<id>" --games N` run against
the live pool was blocked this session by an unrelated pre-existing issue (see
"Unrelated finding" below); the scratch-roster check plus the full test suite
(which exercises the identical `agentChooseMove` code path `rank.exe`'s game
runner uses, via `tests/test_gumbel.cpp`'s full-game test) is the verification
that actually ran.

## Results

- Build: `tests.exe`, `train.exe`, and `rank.exe` all compile clean against
  the new files with no warnings introduced.
- Tests: **3265 assertions in 147 test cases, all passing** (up from the
  pre-existing suite; this slice added `tests/test_gumbel.cpp` in full plus
  targeted additions to `tests/test_ml.cpp` and `tests/test_ranking.cpp`).
- `rank.exe check` against a scratch roster containing
  `gaz(sims=50)@1.learned(model=634,6a22a9da,unknown,joint,value_shape=129-1,policy_shape=9-1)@1`
  printed `OK`: the id parses, the model hash validates against the file on
  disk, and `archDescForSlot` correctly derives the `joint,value_shape=...,
  policy_shape=...` descriptor from a real `type=joint` file.
- `train.exe docs` regenerated `ML.md`'s AUTODOC tables; `GumbelMCTS` and
  `joint` both appear with their registered descriptions.

## Differences from the plan

- **`sigma`'s completedQ substitution for an unvisited child was generalized
  beyond the root.** The plan described substituting "the network's raw root
  value" for an unvisited ROOT candidate specifically. The shipped code uses
  the same substitution at every node in the tree (an unvisited child's
  completedQ is the PARENT's own current mean backed-up value, whatever depth
  that parent is at), because by the time a non-root node is expanded it
  always already has at least one visit of its own (expansion and the first
  backup happen together), so its own mean value is a well-defined, always-
  available substitute. This is a more principled, uniformly-applied version
  of the same documented Pass-1 simplification, not a scope change.
- **Sequential Halving's within-round candidate choice does not use
  `gumbelSelectAction`.** On review while implementing, the plan's phrasing
  ("every node... deterministic action selection... used both within halving
  rounds at the root among candidates and at non-root nodes") was tightened:
  within one halving round every surviving root candidate gets an EQUAL,
  fixed number of additional simulations (no selection needed, matching
  Danihelka et al.'s actual Sequential Halving procedure), and the
  visit-fraction-matching rule (`gumbelSelectAction`) is used only (a) at the
  END of a round to rank survivors by `logit + sigma(completedQ)` for the
  halving cut, and (b) at every non-root node during a simulation's descent.
  Functionally equivalent to the plan's intent, just described more precisely
  in the code comments than the plan's own wording.
- **`gaz()`'s ID grammar shipped simpler than the plan's own draft.** The plan
  sketched `gaz(sims=N[,cvisit=N][,cscale=N][,m=N])`. Shipped: `gaz(sims=N)`
  only -- `c_visit`/`c_scale`/the root candidate count `m` are fixed internal
  constants in `src/ai_gumbel.cpp`, not per-agent ID fields, because nothing
  in slice 1 needs them to vary per-agent yet and `AgentSpec` has no spare
  generic fields for them (unlike `ab()`'s flags, which each ride a dedicated
  existing struct field). Exposing them as real per-agent knobs, if a future
  study wants to sweep them, is a small follow-up, not a redesign.
- **`JointModel`'s file-format prefix was corrected from `value_`/`policy_` to
  a uniform `v_`/`p_`** for both the meta keys (`v_type`, `v_feature_version`,
  ...) and the weight-block keys, after the mismatched-prefix version was
  caught by the loader failing to find its own meta keys during
  implementation (see "Correctness gotchas" below). This matches `DistModel`'s
  `mu_`/`s_` convention exactly, which the plan's own prose already said it
  would mirror -- the first draft just didn't apply that consistently.

## Correctness gotchas discovered and how they were resolved

1. **Own bug: mismatched save/load key prefixes.** The first `JointModel::save()`
   draft wrote long-form meta keys (`value_feature_version=`) but the shared
   `writePrefixedWeights` helper (reused from `DistModel`) prefixes weight
   lines with the short form (`v_`), so the loader's meta-key reads never
   matched anything it had written. Fixed by switching the meta keys to the
   same `v_`/`p_` prefix before any test ran against it -- caught by writing
   the save/load round-trip test itself, before the build even completed.
2. **Test-authoring bug: a slot-number collision with `archDescForSlot`'s
   process-lifetime cache.** The first `tests/test_ranking.cpp` addition used
   slot 20 for its scratch `joint` model. `models/sweep/slot20.txt` turned out
   to already exist on disk with different (non-`joint`) content, and
   `archDescForSlot`'s cache -- keyed only by slot number, populated the first
   time ANY earlier test in the same process emits an id referencing that slot
   -- returned the stale non-`joint` descriptor instead of reading the file my
   test had just written. Symptom: `canon.find(",joint,value_shape=")` came
   back `npos` even though the file on disk was correct. Root cause confirmed
   by checking `.gitignore`: `models/sweep/*` is ignored except an explicit
   allowlist of specific slots (94-99, 169, 602, ..., 700-707), and slot 20 is
   not on it, meaning it is an ordinary untracked scratch/production artifact
   some other machine state had already populated. Fixed by moving the test to
   slot 640, inside this slice's own claimed 634-649 block, which is
   guaranteed collision-free by construction. This is the exact "reusing a low
   slot number colliding with real state" failure mode this project has hit
   before (see the unrelated finding below) -- worth remembering whenever a
   test picks a slot number: check it's actually free, don't just pick a
   small round number.
3. **`rankAgentIsDeterministic` needed a real fix, not just documentation.**
   Caught while designing (not while debugging): Gumbel-top-k draws from
   `rand()` on every move, so without adding a `GumbelMCTS`-specific check,
   any `gaz(...)` agent paired with another deterministic agent would have
   been mis-pinned at exactly 2 games by `pairGameTarget`, silently starving
   its error bars the way `Docs/benchmarking.md`'s defect 3 already describes
   for a different cause. Fixed in the same commit as the ID grammar, not left
   as a follow-up, since an agent that can be rostered but is silently
   mismeasured the moment it is would be a worse state than not being
   rosterable at all.

## Unrelated finding: the test suite corrupts two live production model slots

**Not caused by this session's changes, but discovered by running the test
suite as this project's own commit workflow requires.** `tests/test_ranking.cpp`'s
pre-existing `rankLoadAgentModels` test (predates this session) writes scratch
content to `models/sweep/slot6.txt` and deletes `models/sweep/slot7.txt`
outright, to exercise its "loads successfully" / "missing file fails cleanly"
cases. Both slots are **live, active (`on`) roster entries**
(`ranking/roster.txt` lines 155-156 etc., `ab(deep=6,...)@1.learned(model=6,
eac8ab99,...)@1` and `model=7,c7f7ce61`), and `models/sweep/*.txt` is
gitignored except for a small explicit allowlist that does not include slots 6
or 7. Running `.\tools\run_tests.ps1 -Build` this session left
`models/sweep/slot6.txt` with the WRONG content (a hash mismatch against what
the roster expects) and deleted `models/sweep/slot7.txt` entirely. Neither is
recoverable from git history (confirmed: `git log --all --full-history` for
`models/sweep/slot6.txt` returns nothing -- it was never tracked).

This is the same defect class as the already-documented `models/sweep/slot9.txt`
loss in `todo.md`'s Elo/Tournaments section ("accidentally overwritten by a
test using an unverified slot number"). Effect: historical Elo/ratings for
these two agents are unaffected (the match store's stored game rows are
immutable text and do not depend on the live file), but **no new games can be
played for either agent** until the roster line is either pointed at a
retrained model with a new hash, or the original weights are restored from
some backup outside this repo. This is why the plan's step-4 live
`rank.exe gauntlet` verification did not run against the real pool this
session -- `rank.exe` correctly refuses to proceed once it hits either broken
slot while validating the full roster.

This was not touched or fixed as part of this slice (it is a pre-existing
defect in unrelated, already-existing test code, not something introduced by
the Gumbel work, and fixing it means either accepting the loss the way slot9's
was accepted or auditing every test's chosen scratch-slot numbers against the
live roster, which is separate scope). Flagged here so it is not silently
rediscovered later, and left for the developer to decide how to handle.

## Future Work

- **Slice 2: the self-play training regime.** Everything this document
  describes is untrained. The next plan needs its own three-pass design
  (`Docs/model-training-playbook.md`) covering: the self-play loop using
  `GumbelMCTS`, a bootstrapped search-value target for the value head (the
  search's own improved value estimate, per the developer's stated
  preference), and the Gumbel-improved policy target
  (`softmax(logits + sigma(completedQ))` over the root's final visit
  counts/completedQ -- already exposed via `GumbelRootInfo`, unused until this
  lands) for the policy head.
- **`c_visit`/`c_scale`/the root candidate count `m` are unswept constants.**
  Fixed at the paper's defaults (50, 1.0, 16) with no evidence yet that those
  are good choices for Breakthrough's branching factor/game length. Worth a
  Pass-2 axis once there is a trained model to measure Elo sensitivity against
  -- sweeping them on random weights would not mean anything.
- **The two live-slot corruptions (slot 6, slot 7) block any full-pool
  `rank.exe gauntlet`/`run`/`play` involving those two agents** until
  resolved. Not this slice's problem to fix, but it will keep blocking
  verification of ANY new agent's gauntlet smoke test (not just Gumbel's)
  until either the roster line is updated or the weights are restored.
- **No perf work done.** `ai_gumbel.cpp`'s tree is heap-allocated per node
  with no transposition sharing across simulations. Fine for Pass-1
  correctness; likely the first thing worth profiling once real training runs
  need many self-play games per checkpoint.

## Ideas This Inspired

- The `gaz()` ID grammar's `sims=` reuse of `AgentSpec::depth` (the same field
  `ab()` uses for search depth) worked cleanly enough that it's worth asking
  whether other "total effort" style search knobs across future explorers
  should default to that same field rather than inventing a new one each
  time, with a naming convention in the grammar (`deep=`/`sims=`/...) doing
  the disambiguation instead of a new struct field.
- `GumbelRootInfo`'s exposed completedQ/visitCounts is exactly the shape a
  "why did the search pick this move" GUI/console readout would want (a
  richer sibling of the existing `pred` downstream-eval display for AlphaBeta
  agents) -- unrelated to slice 2's training use, but a plausible GUI-track
  idea once/if this explorer is ever wired into the interactive console.
