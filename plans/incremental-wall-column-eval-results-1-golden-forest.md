# Results: incremental wall/column eval (implemented 2026-07-05)

Companion to the plan
[incremental-wall-column-eval-plan-1-golden-forest.md](incremental-wall-column-eval-plan-1-golden-forest.md).
This is the permanent record of what was actually done, what was found, and how it
was measured.

## Status: shipped and verified

Wall and column are now a true neighbor-local delta (`neighborStruct`), classic/exp
are versioned `@2`, the roster and ranking tests were updated, docs were updated,
and all **408 test assertions pass** including the make/unmake equivalence gate.
Eval values are byte-identical to before, so all game outcomes and Elo are
unchanged. Only cpu/node moved.

## What changed

- **Engine ([src/ai_eval.cpp](../src/ai_eval.cpp)).** `evalPosLocal`'s structure
  term no longer re-scans a ~3x3 `structOwner` bounding box before and after each
  make/unmake. A new `neighborStruct` helper sums only the orthogonal same-color
  pairs touching the two changed squares, under `structOwner`'s single-ownership
  convention. Forward was left as-is (it was already a 2-square delta). This is the
  only engine change.
- **Version bump ([src/ranking.cpp](../src/ranking.cpp)).** `classic` and `exp`
  bumped from `@1` to `@2` in `g_rkEvals` (both share the changed structure code).
- **Roster ([ranking/roster.txt](../ranking/roster.txt)).** All ~40 classic/exp
  segments rewritten to `@2` (heads `ab(...)@1` and `dil(...)@1` stay `@1`).
- **Tests ([tests/test_ranking.cpp](../tests/test_ranking.cpp)).** Canonical
  classic/exp ID strings updated to `@2` (defect-input tests that pin a stale or
  missing version were left intentionally).
- **Docs.** `README.md`, `CLAUDE.md`, and `todo.md` updated to describe the
  incremental eval as a true neighbor-local delta rather than a bounding-box
  rescan.

## Headline result: wall/column agents run meaningfully faster

Agents whose evaluator uses a non-zero **wall (`w`)** or **column (`l`)** weight
spend roughly **a third less CPU per node** in eval-heavy cases. Node counts were
identical old-vs-new (deterministic seeded games), so the delta is purely the
eval-code change:

| Agent (non-zero wall/column) | us/node OLD | us/node NEW | reduction |
|---|---|---|---|
| `ab(d4).classic(t2,c10,w3,l2)` | 0.1395 | 0.0847 | **-39.3%** |
| `ab(d4).exp(t20,c64,w0,l1,f15)` | 0.1513 | 0.0939 | **-37.9%** |
| `ab(d4).exp(t20,c65,w0,l1,f14)` | 0.1536 | 0.1022 | **-33.4%** |

Because the search is identical (same nodes/move), a cpu/node reduction is a
same-size reduction in total CPU per move for these agents. For `classic(w3,l2)`,
where the structure term dominates per-node cost, that is a ~39% overall speedup.
Agents where eval is a smaller share of per-node work see proportionally less.

## What is not affected (and why the "affected set" narrowed)

Only wall/column route through the changed code. **Forward was already a true
2-square delta**, so the forward-only `exp(...,w0,l0,fK)` "forward ladder" agents
skip the changed branch entirely (`w==0 && l==0`). They are effectively controls,
and their A/B scatter (-8% to +24%) sits inside the noise band. The genuine `w0,l0`
controls confirm the floor: the best-sampled one
(`ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0)`, ~3000 ms total CPU) came in at
**-4.2%** (about zero), exactly as expected for code it does not execute.

So the true "affected" set is **agents with non-zero wall or column**, not all
eight non-zero-weight agents named in the plan.

## Correctness gotcha the equivalence test caught

The first `neighborStruct` (the naive 4-orthogonal-neighbor version in the plan)
was wrong: it counted top-row horizontal pairs and rightmost-column vertical pairs
that `evalPosFull` deliberately excludes via `structOwner`'s single-ownership
convention (each pair owned by its lower-left cell, owners restricted to
`[0, SIZE-2]`). The equivalence walk in `test_eval.cpp` failed immediately (1020
mismatch). The fix: guard each of the four directional pairs by its owner lying in
`[0, SIZE-2]` on both axes, so the incremental sum reproduces `evalPosFull`
exactly. Lesson: an incremental delta must replicate the full scan's counting
convention precisely, including its edge exclusions.

## Measurement methodology: why the planned store comparison was abandoned

The plan's Part 5 (compare new `@2` vs retired `@1` cpu/node in the match store)
turned out **not trustworthy**. The historical `@1` games were played in parallel
(par=8/10), so their per-node CPU is inflated by cache/memory contention and
reduced turbo, while the fresh `@2` run was serial. Even par=1-to-par=1
comparisons swung +/-20% from day-to-day machine variation on small old-game
samples. The raw store numbers showed a ~2x "drop" that was mostly machine effect.

It was replaced with a **controlled same-session A/B**: build `rank.exe` with the
old bounding-box eval, play a fixed scratch roster at a fixed seed; then rebuild
with the new neighbor-local eval and replay the identical seeds. Because the change
is value-preserving and games are deterministic, the two runs produce identical
games and identical node counts, so any cpu/node difference is exactly the eval
code. This is the gold-standard isolation and is what the headline table uses.

Precision caveat: Windows `GetProcessTimes` has ~15 ms quantization, which
dominates fast, low-CPU games. Only high-total-CPU (structure-heavy) agents give a
clean signal, so read the numbers as "roughly a third," not a precise 36%.

## Ranking state

The `@2` roster was re-ranked serially and
`ranking/{matches.jsonl,ratings.tsv,games.tsv,report.md}` regenerated. This was
done at `--games 4` rather than the planned `--games 12`: the controlled A/B
answered the speed question far more cleanly than the store ever could, and Elo is
unchanged by construction (identical eval -> identical games), so a larger fill
would only tighten confidence intervals on ratings that do not move. The store can
be topped up to a higher game count later (the scheduler tops up incrementally).

## Process note: concurrent commits during the session

Two commits landed mid-session (`30dc980`, `4e2d699`, "Add stochastic depth
dilution + eval-weight hill climber"; HEAD was `d499bf5` at session start). One of
them swept up the in-progress working tree and captured an **intermediate, buggy**
`ai_eval.cpp`: the first `neighborStruct` without ownership bounds, the version
that failed the equivalence test. The corrected `ai_eval.cpp` was still uncommitted
in the working tree. Net effect to watch for: a build from that HEAD alone would
fail the equivalence test until the corrected `ai_eval.cpp` is committed. The
`ranking.cpp` (@2), `roster.txt` (@2), and `test_ranking.cpp` (@2) edits were
already correctly in HEAD.

## How to test

- `.\tools\run_tests.ps1 -Build` -> 408 assertions pass. The `test_eval.cpp`
  equivalence walk is the correctness gate for the delta.
- `.\tools\run_rank.ps1 -Build check` -> roster canonical, no errors.
- Expected behavior: the AI plays identically (same moves, same Elo). Wall/column
  agents just spend about a third less CPU per node.

## Candidate commit messages

1. `Make wall/column eval a true incremental delta; re-rank @2 evaluators`
2. `Incrementalize wall/column structure eval (neighbor-local delta), ~1/3 less CPU/node for structure agents`
3. `Fix and complete neighbor-local structure eval; bump classic/exp to @2, re-rank`

**Top recommendation:** #1 (captures the functional change and the `@2` re-rank
together). If you want to flag that this commit carries the correctness fix over
the buggy intermediate now in HEAD, #3 is the more explicit choice.

## Uncommitted files at end of session

`src/ai_eval.cpp` (the ownership-bounds fix), `CLAUDE.md`, `README.md`, `todo.md`,
and the regenerated `ranking/{matches.jsonl,ratings.tsv,games.tsv,report.md}`.
The plan and results documents in `plans/` are new and untracked until committed.
