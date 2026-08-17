# Plan: back model slots with a sparse map instead of a fixed array

## Problem

`g_mlModels` (`src/ml_eval.cpp`) was `Model* g_mlModels[ML_SLOTS]`, a fixed
1024-pointer array. This came up in an architecture discussion prompted by the
slot6/slot7 data-loss incident (`plans/scratch-slot-plan-1-quiet-heron.md`):
a model's real identity is its content hash, not its slot number, so the slot
number is a second, redundant, manually-tracked namespace, and the fixed
array size (`ML_SLOTS`) exists only because storage was preallocated for the
largest sweep ever run rather than sized to what a process actually loads.

The full architectural fix -- dropping the slot number from canonical IDs
entirely and keying everything by content hash, including the roster.txt
format and `AgentSpec` -- is a large, multi-file change (ID codec, roster
format migration, every call site that threads `modelSlot`). That is left for
a dedicated session. This plan scopes down to the storage layer alone: keep
the existing `int slot` handle that every call site already threads through
(`AgentSpec::modelSlot`, `mlValueScore(turnColor, slot)`, canonical IDs'
`model=N`, `rankSlotFile()`), but stop backing it with a preallocated array.

## Design

Replace `Model* g_mlModels[ML_SLOTS]` with `std::unordered_map<int, Model*>`.
`mlSetModel`/`mlGetModel`/`mlClearSlots` change from array indexing to map
lookup/insert/erase. `ML_SLOTS` stops being an allocation size and becomes
validation-only: a sanity ceiling the ID parser, roster tripwire test, and
`rankSlotFile()`'s reserved-range math check a slot number against, same as
before.

Everything downstream of the slot handle (canonical ID format, roster.txt
`model=N,` segments, `AgentSpec::modelSlot`, the reserved-scratch-range
mechanism from the previous slot-collision fix) is unchanged. This is a pure
internal-storage substitution, not an identity-model change.

## Scope boundary

Not attempted here: dropping `model=N` from canonical IDs, migrating
roster.txt off slot numbers, or changing `AgentSpec::modelSlot`'s type. Those
require touching the ID codec and stored data format and are a separate,
larger piece of work.
