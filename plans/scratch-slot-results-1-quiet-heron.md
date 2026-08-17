# Results: fix the model-slot test-overwrite defect systemically

Companion to `scratch-slot-plan-1-quiet-heron.md`.

## Summary of changes

- `src/ml_eval.h`: new `#define ML_RESERVED_SLOTS 16` (slots
  `ML_SLOTS-16`..`ML_SLOTS-1`, i.e. 1008..1023, permanently reserved).
- `src/ranking.cpp`: `slotFile()` renamed to `rankSlotFile()`, made
  non-static, and given a new branch: a slot in the reserved range resolves
  to `models/scratch/slot<N>.txt` instead of `models/sweep/slot<N>.txt`. All
  seven internal call sites updated.
- `src/ranking.h`: declares `rankSlotFile()`.
- `tests/helpers.h`: `fileExists()` helper; the `TestScratchSlot` enum
  registry (six named constants, one per scratch-writing test case).
- `tests/test_ranking.cpp`: every test that writes a scratch model file now
  takes its slot number from the registry and builds its path via
  `rankSlotFile()` instead of hand-building a `"models/sweep/slot" + N`
  string. New tripwire test: `ranking roster - test-suite scratch slots never
  collide with a live roster agent`.
- `.gitignore`: `models/scratch/*` ignored (nothing there is ever
  roster-referenced, so unlike `models/sweep/` there are no exceptions to
  track).
- `todo.md`, `src/CLAUDE.md`, `tests/CLAUDE.md`: updated to match.

## How to test

`.\tools\run_tests.ps1 -Build` (or the manual `vcvars64.bat` + `cl` sequence
this session used, see below). Full suite: 3570 assertions, 148 test cases,
all passing. The new tripwire test alone: `.\tests.exe "ranking roster -
test-suite scratch slots never collide with a live roster agent"` (305
assertions, one per `model=N` token currently in `ranking/roster.txt`).

## Build environment gotcha (this session)

`run_tests.ps1 -Build` failed with "Could not locate Visual Studio via
vswhere" in this environment, and the root `CLAUDE.md`-documented workaround
(`cmd /c '"<vcvars64.bat>" && cl ...'` via the Bash tool) also failed
silently: the Bash tool's `cmd.exe` invocation opened an interactive shell
and ignored the `/c` argument entirely (confirmed with a bare `cmd.exe /c
"echo hello"` printing the banner instead of "hello", both with and without
`dangerouslyDisableSandbox`). The same command works correctly through the
PowerShell tool (`cmd /c $cmdString`), which is what actually built and ran
everything this session. Worth carrying forward as an amendment to the
existing workaround note if this recurs.

## What nearly went wrong (why the design escalated)

The first pass at this fix picked `ML_SLOTS-5`/`ML_SLOTS-6` for the
`rankLoadAgentModels` test's two scratch slots, reasoning from a grep for
`ML_SLOTS\s*-\s*[12]` that only turned up `ML_SLOTS-1`, `ML_SLOTS-2`, and
`ML_SLOTS-4` already in use. The first full build+run immediately found two
NEW collisions the grep had missed: an existing "risk= weight" test already
used `ML_SLOTS-6`, and an existing "regime token" test's second slot already
used `ML_SLOTS-5` (via its own hand-built `models/sweep/slot`+N path, not
through the shared function). A regex covering only two of the six digits
that could follow `ML_SLOTS -` is exactly the kind of incomplete manual check
that caused the original incident. This is the concrete reason the final
design centralizes every scratch slot into one named-constant registry
(`tests/helpers.h`'s `TestScratchSlot`) instead of leaving numbers scattered
across test files: a single list is what a human or a grep can't fail to see
completely, where six separate `ML_SLOTS - N` literals spread across a
1600-line file demonstrably were missed even under direct, careful scrutiny.

## An unrelated, more severe finding surfaced along the way

Making the new tripwire test call `rankLoadRosterFile("ranking/roster.txt",
...)` (the full semantic parse) failed immediately -- not because of a
slot-range collision, but because `ranking/roster.txt` line 155's `model=6`
entry has a hash mismatch against the current (corrupted-since-2026-08-16)
`models/sweep/slot6.txt`, and `rankLoadRoster` aborts parsing the ENTIRE
roster on the first bad line (`src/ranking.cpp`, confirmed by reading the
loop). Verified directly against the live repo: `.\rank.exe check` exits 1
with exactly that error.

This means every `rank.exe` subcommand that loads the roster (`check`,
`play`, `rate`, `gauntlet`, ...) has been completely non-functional since the
2026-08-16 loss -- not merely "no new games for the two affected agents," as
`todo.md`'s existing entry characterized it at the time. That entry has been
corrected in place (see `todo.md`, Elo/Tournaments) with the confirmed
severity, re-tagged `[Next]`, and the repair (repoint `model=6`/`model=7` at
retrained replacements, or restore the original weights from outside the
repo) is left as a roster-data decision for the developer, not attempted
here -- editing `ranking/roster.txt` changes canonical ranking data and is out
of scope for a test-suite fix. The tripwire test itself was rewritten to do a
plain-text scan for `model=N` tokens instead of a full semantic load, so it
isn't blocked by this unrelated, pre-existing defect and still does its
actual job (catching a FUTURE slot-range collision).

## Correctness gotchas

- Catch2 v2's expression-decomposition macros reject a bare `a && b` inside
  `REQUIRE`/`REQUIRE_FALSE` (`chained comparisons are not supported inside
  assertions`, a `static_assert` in `catch.hpp`). Fixed by binding the
  compound condition to a named `bool` first.
- `tests/helpers.h` needed `#include "ml_eval.h"` to see `ML_SLOTS` for the
  `TestScratchSlot` enum; harmless (idempotent via `#pragma once`, and every
  consuming `.cpp` already included it directly).
- Leftover `models/sweep/slot1018.txt`, `slot1019.txt`, `slot1023.txt`,
  `slot5.txt` artifacts (from this session's own pre-fix test runs, plus one
  pre-existing from a prior session's "scheduler" test using slot 1023
  directly) were deleted at the end; all gitignored, none roster-referenced.
  Running the suite also regenerates `models/manifest.{json,md}` (a
  pre-existing, already-logged issue, `todo.md`) -- left unstaged rather than
  bundled into this commit.

## Future Work

- The roster-load failure this session confirmed (see above) blocks every
  `rank.exe` subcommand right now. Whoever picks up `todo.md`'s corrected
  entry needs to decide: retrain replacements for slot 6/7's role, or locate
  and restore the original weights from outside the repo. Until then, no
  ranking work of any kind can proceed on this repo.
- The tripwire test's general-range exception list (`kGeneralRangeScratchSlots
  = { 5 }`) is a second, smaller manual-tracking surface that mirrors the
  problem this whole fix closes for the reserved range. It's intentionally
  small (one entry, one test) and defended by the tripwire itself, but if a
  second test ever legitimately needs a general-range slot, this list is the
  one place that must be kept current -- worth a comment reminder at the
  "sweep slot convention" test site pointing back here (already added).

## Ideas This Inspired

- `rankLoadRoster` currently aborts the whole file on the first bad line.
  A `--tolerant` or `check --report-all` mode that collects every parse
  error across the full file before returning could have surfaced the
  slot6/slot7 hash mismatches (and any others) in one pass instead of one
  error at a time, which would have made the true severity of the
  2026-08-16 loss visible immediately instead of remaining hidden until this
  session's tripwire test happened to probe it.
- The `models/sweep/*.txt` git-tracking exception list in `.gitignore`
  (currently ~30 explicit `!models/sweep/slotN.txt` lines) is the same kind
  of manually-maintained enumeration this fix replaced elsewhere. A small
  `rank.exe` subcommand that cross-checks the roster's referenced model
  slots against that exception list (flagging any live agent whose model
  ISN'T git-tracked) would catch the next slot9/slot6/slot7-shaped loss
  before it happens, rather than after.
