# Gumbel AlphaZero bot -- Slice 2 results: self-play trainer (Pass 1 sanity)

Companion to `plans/gumbel-mcts-plan-2-hidden-greeting-mist.md`. Read that
first for the design; this document records what actually shipped, how it
was verified, and what was found along the way.

## Summary

Shipped exactly the plan's scope: a self-play trainer for the `joint`
value+policy model (`src/ml_gumbelzero.h`/`.cpp`, new files), two small
additive extensions to `ai_gumbel.h`/`.cpp` (`GumbelRootInfo::searchValue`,
`gumbelImprovedPolicy`), roster wiring (a `gumbel_self` regime tag), a `train.exe
gumbelzero` CLI subcommand, and a full test suite. This is **Pass 1 only** --
one configuration, run small, to prove the plumbing. No Elo has been measured
and none is claimed. Pass 2 (a broad hyperparameter sweep) and Pass 3
(optimize) are unscoped follow-up work.

## How to test

```powershell
.\tools\run_tests.ps1 -Build      # full suite, includes tests/test_gumbelzero.cpp
.\tools\run_train.ps1 -Build docs # regenerates ML.md's AUTODOC region
.\train.exe gumbelzero --out models/sweep/slot650 --games 40 --sims 50 --seed 1001 --ckpt-at "20,40"
.\rank.exe check --roster <a roster containing the resulting gaz(...).learned(...) id>
```

## Results

- Build: `tests.exe`, `train.exe`, and `rank.exe` all compile clean against
  the new files with no warnings introduced.
- Tests: **3635 assertions in 157 test cases, all passing** (up from the
  3629/156 this session started with; this slice added
  `tests/test_gumbelzero.cpp` in full plus one targeted addition to
  `tests/test_ranking.cpp`).
- Manual Pass-1 run (`train.exe gumbelzero --out models/sweep/slot650 --games
  40 --sims 50 --seed 1001 --ckpt-at "20,40"`): completed in seconds, 40
  games (25 W / 15 B / 0 draw), 67,648 training steps applied, replay buffer
  filled to its 2000-record capacity, three checkpoints written
  (`slot650_g20.txt`, `slot650_g40.txt`, `slot650.txt`).
- Published `slot650.txt` (the 40-game checkpoint) and `slot650_g20.txt`
  (copied to `slot651.txt`, the 20-game rung) into the roster's model-slot
  convention. `rank.exe check --roster ranking/scratch_gz.txt` against a
  scratch roster containing
  `gaz(sims=50)@1.learned(model=650,06279612,gumbel_self,joint,value_shape=129-1,policy_shape=9-1)@1`
  and the slot-651 sibling printed `OK`: both ids parse, both model hashes
  validate against the files on disk, and the regime shows as `gumbel_self`
  (i.e. `archDescForSlot`'s `R::of()` correctly read the `gumbelzero(...)`
  `teacher=` line back off the trained model file).
- `train.exe docs` regenerated `ML.md`'s AUTODOC tables; the `gumbelzero`
  regime row appears with its registered description.

## Differences from the plan

None of substance. The plan's design (value target = `(searchValue+1)/2`
via the existing `Model::trainStep` sigmoid convention, policy target =
`gumbelImprovedPolicy` via a new `gumbelPolicyGradients` softmax-CE helper,
a ring-buffer replay buffer sampling distinct records per minibatch, a
bespoke self-play loop rather than reusing `playGame`/`agentChooseMove`)
shipped as designed. The only addition beyond the plan's own file list was a
second regime-token allowlist in `src/ranking.cpp` the plan's research had
not surfaced (see "Correctness gotchas" below) -- a few lines, not a design
change.

## Correctness gotchas discovered and how they were resolved

1. **A second, separate regime-token allowlist needed the same addition as
   the emitter.** The plan identified that `archDescForSlot`'s `R::of()`
   (`src/ranking.cpp`) needed a `gumbelzero(` -> `gumbel_self` branch to
   *emit* the regime tag when deriving an id from a trained model file. What
   the plan's research missed is that `learned()`'s id *parser* separately
   validates the regime token against a hardcoded `kRegimes[]` allowlist
   (`src/ranking.cpp`, the `learned() regime '...' is not a known token`
   error), used to catch typos in hand-written roster lines. Adding the tag
   only on the emit side meant a freshly trained model's own id could be
   generated but never re-parsed: `rank.exe check` failed with `learned()
   regime 'gumbel_self' is not a known token` the first time this session
   actually ran a real self-play training run and tried to validate its
   output through the real roster pipeline -- caught immediately by
   following the plan's own Verification step 3 (the manual run + `rank.exe
   check`), not by a unit test, since no test in this slice happened to
   exercise the *parse* direction with the new tag before that point. Fixed
   by adding `"gumbel_self"` to `kRegimes[]` alongside `"tdleaf_self"`, and
   a new `tests/test_ranking.cpp` case ("ranking id - gumbel_self regime
   round trip") was added specifically to close this gap for future
   regimes: it builds a real model file with a `gumbelzero(...)` `teacher=`
   line and round-trips it through both the emit AND parse directions in one
   test, the way the analogous `joint` mutype test already did for Slice 1's
   `gaz(...)` head.
2. **`generateMoves`'s determinism is relied on twice per ply, and is the
   reason the training loop cannot special-case `GumbelRootInfo` naively.**
   The training loop calls `generateMoves` once itself (to build the ply's
   move-feature array) and `gumbelSearch` calls it again internally (to
   build the root); since the board is unchanged between the two calls,
   the two move lists are index-identical by construction, matching the
   plan's own stated invariant. The gotcha caught during implementation
   (not by a test failing, but by re-reading the code before it ran): the
   `n == 0` case (no legal moves) must be handled by the training loop
   itself *before* calling `gumbelSearch`, not by calling `gumbelSearch` and
   inspecting `info` afterward -- `gumbelSearch` returns early on `n == 0`
   without touching `info` at all, so reading `info.moveCount` in that
   branch would read indeterminate stack memory. Fixed by checking `n == 0`
   directly against the training loop's own `generateMoves` call and setting
   the correct loss victor immediately, never reaching the `gumbelSearch`
   call for that ply.

## Unrelated finding: the same slot6/slot7 blocker as Slice 1

**Not new, not caused by this session.** Slice 1's `rank.exe gauntlet`
plumbing smoke test was blocked by `models/sweep/slot6.txt`/`slot7.txt`
being live, active roster agents whose files were corrupted/deleted by a
pre-existing test defect; the developer's decision (2026-08-16) was to
accept the loss, mirroring the earlier `slot9` incident. That defect was
independently fixed later the same day in an unrelated commit
(`cbf1f98`, "Reserve a scratch model-slot range so tests can't overwrite live
agents", plus `4f37165`) -- but a defect being *fixed going forward* does not
repair the two model files that were already lost, so `ranking/roster.txt`
line 155 still names a `models/sweep/slot6.txt` whose current content does
not match the hash the roster expects. Running `rank.exe gauntlet --id
"<the slot650 id>" --games 4` this session hit that same, already-accepted
blocker (`ERROR: ranking/roster.txt: line 155: model hash mismatch for
models/sweep/slot6.txt`). As with Slice 1, this is why Pass 1's live-pool
gauntlet check did not run against the real roster this session; the
scratch-roster `rank.exe check` (which passed, `OK`) plus the full test
suite (which exercises the identical `gumbelSearch`/`agentChooseMove` code
path a gauntlet game would use) is the verification that actually ran.
Resolving `slot6`/`slot7` (retrain and re-point the roster line, or restore
the weights from an external backup) remains open and unrelated to this
slice; it will keep blocking any full-pool gauntlet smoke test, Gumbel or
otherwise, until it is.

## Future Work

- **Pass 2: the broad hyperparameter sweep.** Nothing here has been swept
  for strength: `sims`, `lr`, `l2`, replay capacity/warmup, batch size, and
  seeds are all still single untested values (see the new `gumbelzero`
  section of `Docs/hyperparameter-log.md`). Needs its own presented
  configuration grid (`Docs/model-training-playbook.md`'s "design the grid,
  then stop and show it" rule) before any training run larger than this
  session's sanity check.
- **No transposition/perf work in the search this trainer drives.**
  `ai_gumbel.cpp`'s tree remains heap-allocated per simulation with no
  sharing across simulations (a Slice-1-documented limitation, unchanged
  here). Self-play at `sims=50` for 40 games ran in seconds, so this is not
  yet a blocker, but a Pass-2 sweep at higher `sims` or many more games
  would be the first place to look if training throughput becomes the
  bottleneck rather than rating throughput (this project's usual ordering,
  per the playbook's "Generator/search depth" guidance).
- **Whether the value target should ever blend in game outcome is untested.**
  The developer's stated preference (search-value only, no outcome label at
  all) was implemented as specified. Nothing in this slice tests whether a
  blend (e.g. a small outcome-weighted term) would help or hurt; if Pass 2
  or Pass 3 ever wants to explore that, it is a new, explicit axis, not a
  silent default change.
- **Replay buffer persistence/resume does not exist.** In-memory only, as
  the plan specified for Pass 1. A long Pass-2/3 run that needs to resume
  after an interruption would need this added, mirroring TD-Leaf's
  `--ckpt-at` rung-file resumability model (which itself resumes by
  re-deriving from a checkpoint, not by literally reloading the buffer).

## Ideas This Inspired

- The regime-allowlist gotcha (two places needed the same addition, and only
  one was visible from reading `R::of()` alone) suggests `kRegimes[]` and
  `R::of()`'s prefix-match table could be unified into one data structure
  read by both the emitter and the parser, so a future regime addition
  cannot drift the same way again. Not done here (out of scope for a Pass-1
  slice, and `R::of()`'s prefix matching is richer than a flat allowlist
  membership check), but worth a dedicated look if a third regime addition
  ever hits the same gap a third time.
- `GumbelZeroRecord`'s "store raw features + targets, recompute the group
  softmax at sample time" design (needed so replay samples train against
  current weights, not stale ones) is a general pattern that would also let
  a future opponent-pool or checkpoint-mixing mechanism replay OLDER
  self-play data against a NEWER model cleanly, if Pass 2 or Pass 3 ever
  wants that -- the replay buffer's records are already self-describing
  enough to support it without a format change.
