# Plan: fix the model-slot test-overwrite defect systemically

## Problem

`tests/test_ranking.cpp`'s `rankLoadAgentModels` test picked slot numbers 6
and 7 as scratch space for its load-success/load-failure cases. Both were
live, active roster agents' actual model files
(`models/sweep/slot6.txt`/`slot7.txt`, referenced by
`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=6,eac8ab99,...)@1` and
`model=7,c7f7ce61` in `ranking/roster.txt`). The test overwrote slot 6 and
deleted slot 7. Neither file is git-tracked, so neither is recoverable; the
developer accepted the loss on 2026-08-16 (`todo.md`, Elo/Tournaments) rather
than fix the test at the time.

This is the SECOND occurrence of the identical defect class: `models/sweep/slot9.txt`
was lost the same way on 2026-07-30. The project's own `test_ranking.cpp`
already carries three comments documenting a manual mitigation ("use a slot
number near `ML_SLOTS`, as far as possible from any real study's range,
avoiding the ones the trainer/other tests already use") -- but that mitigation
is comment-only, applies nowhere except by a developer/agent remembering to
read and follow it, and the `rankLoadAgentModels` test that caused THIS
session's incident evidently never read it. The user's ask: fix this
systemically, not with more documentation.

## Design

1. **Structural separation, not a numbering convention.** Reserve the top
   `ML_RESERVED_SLOTS` slot numbers (a new constant, `src/ml_eval.h`) and make
   `ranking.cpp`'s slot-to-path function resolve them to a NEW directory,
   `models/scratch/`, instead of `models/sweep/`. Real roster-tracked agents
   only ever live under `models/sweep/`, so a test picking ANY number from the
   reserved range physically cannot collide with a live agent's file, no
   matter how large the sweep-owned ranges grow in the future. This replaces
   "remember not to use a taken number" with "it is impossible to take a
   number that matters."
2. **Export the slot-to-path function** (`slotFile` -> `rankSlotFile`, moved
   out of file-local static into `ranking.h`) so every caller -- rank.exe's
   own internals AND the test suite -- derives a scratch path the same way,
   rather than each test re-deriving (and risking divergence from) the naming
   convention by hand.
3. **A single named-constant registry** (`tests/helpers.h`'s
   `TestScratchSlot` enum) for every test that needs a throwaway slot, so
   collisions between DIFFERENT tests sharing the same reserved slot number
   are visible in one place at a glance, rather than requiring a full-file
   grep (which, notably, missed two collisions during THIS session's initial
   attempt at a manual fix -- see Results, "what nearly went wrong").
4. **A tripwire test** that fails loudly if a live roster agent is ever
   assigned a slot number inside the reserved range, or one of the specific
   general-range numbers the suite still uses (for the one test that must
   stay in the general range by design, since its whole point is exercising
   that convention). This is the safety net for the residual case that can't
   be made structurally impossible.

## Scope boundary

Repairing the ALREADY-LOST slot6/slot7 identities (retraining replacements or
restoring the original weights) is a separate, roster-data decision that
stays with the developer -- not attempted here. This plan only closes the
mechanism that let a test destroy live data in the first place.
