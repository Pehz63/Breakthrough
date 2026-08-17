# Results: back model slots with a sparse map instead of a fixed array

Companion to `model-slot-storage-plan-1-marbled-lynx.md`.

## Summary of changes

- `src/ml_eval.cpp`: `g_mlModels` changed from `Model* g_mlModels[ML_SLOTS] =
  { nullptr }` to `std::unordered_map<int, Model*> g_mlModels`. `mlSetModel`,
  `mlGetModel`, `mlClearSlots` changed from array indexing to
  find/insert/erase over the map. `mlGetModel`'s explicit `slot < 0 || slot >=
  ML_SLOTS` bounds check was dropped since a map lookup on an out-of-range key
  simply misses (returns `nullptr`) with no bounds risk; `mlSetModel` keeps
  its bounds check since it is the one write path and stores get silently
  dropped (`delete m; return;`) for a slot number outside the valid range,
  same behavior as before.
- `src/ml_eval.h`: updated the header comment block and the `ML_SLOTS`
  comment to describe storage as a sparse map (validation-only ceiling)
  rather than a fixed array, and fixed a stale `slotFile()` reference to the
  current `rankSlotFile()` name.
- `src/CLAUDE.md`: updated the `ml_eval.cpp`/`ml_eval.h` row to match.

No other files changed. `AgentSpec::modelSlot`, canonical ID format
(`model=N,<hash>`), `rankSlotFile()`, the reserved-scratch-range mechanism,
and `tests/helpers.h`'s `TestScratchSlot` registry are all untouched --
this was a pure internal-storage substitution behind the existing `int slot`
interface.

## How to test

Build via the documented `vcvars64.bat` + `cl` sequence (`run_tests.ps1
-Build` could not locate `vswhere.exe` in this environment, same known gotcha
as the prior slot-collision session; built `tests.exe` directly instead).
Full suite: 3570 assertions, 148 test cases, all passing -- same count as
before this change, confirming the map-backed storage is behaviorally
identical to the array for every existing test.

## Why this scope, not the full hash-identity redesign

The architecture discussion that motivated this (why an integer-indexed
array rather than a hash-keyed dictionary) concluded the better long-term
design keys model storage by content hash directly and drops the slot number
from canonical IDs and `AgentSpec` entirely -- eliminating the redundant
namespace that caused the slot6/slot7/slot9 collisions, not just papering
over it with a reserved range. That is a larger change (ID codec, roster.txt
format, every `modelSlot` call site) that needs its own session. This change
is the piece of it that was self-contained and low-risk: it removes the
"array sized for the largest sweep ever run" assumption without touching
anything identity- or format-related, so it could be verified end-to-end
(build + full test suite) in isolation.

## Correctness gotchas

None encountered -- the change is a mechanical substitution behind an
unchanged interface, and the full test suite (which already exercises
`mlSetModel`/`mlGetModel`/`mlClearSlots` via the ranking and ML test files)
passed at an identical assertion count to the pre-change baseline.

## Future Work

- The full hash-keyed identity redesign (drop `model=N` from canonical IDs,
  key `AgentSpec` by content hash, migrate `ranking/roster.txt`'s `learned()`
  segments) is still open. It would let `ML_SLOTS`/`ML_RESERVED_SLOTS` and
  the `TestScratchSlot` registry be removed entirely, since a test's
  throwaway model would get its own distinct hash and could never collide
  with a live agent's hash by construction, closing the slot9/slot6/slot7
  defect class structurally rather than via a reserved range.
- The pre-existing `ranking/roster.txt` line 155 hash-mismatch (slot6, logged
  in `todo.md`'s Elo/Tournaments section) still blocks every `rank.exe`
  subcommand that loads the roster. Unaffected by this change; still a
  roster-data decision for the developer.

## Ideas This Inspired

- If the full hash-keyed redesign happens, the resolved `Model*` for a search
  should be cached once per search setup (e.g. at `evalBeginSearch`) rather
  than doing a map lookup per leaf in `mlLeafScore`, so the hot path never
  pays a hash lookup per node visited. Worth checking during that redesign
  whether `mlLeafScore`'s current call pattern already resolves the slot once
  per search or genuinely per leaf.
