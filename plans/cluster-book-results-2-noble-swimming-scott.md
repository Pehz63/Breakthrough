# Cluster-matched opening book ("cbook"): mining-scope comparison

Companion results doc for `plans/cluster-book-plan-1-noble-swimming-scott.md`,
extending Pass 1a (`plans/cluster-book-results-1-noble-swimming-scott.md`).
This round adds a third mining scope (`--core`) and directly measures the
win-rate/Elo effect of the `cbook` opener on two specific agents, rather than
just the mechanism-fires check Pass 1a ran. It does not run the plan's actual
Pass 1b (the universal/per-regime scope decision across the whole roster) or
Pass 2 (the cluster/keep/mirror/seed grid); it answers a narrower, differently
scoped developer question: for one core's own wins, does pooling in that
core's opener-wearing wins (`--core`) or its whole evaluator family
(`--regime`) change the picture versus mining that one exact id alone
(`--a`), and does any of the three actually help.

## Summary

Added a third `cbookdump` scope, `--core <id>`, that pools an agent's wins
under ANY opener (or none) rather than requiring an exact id match, answering
a developer question about whether the existing `--a` scope silently included
or excluded book/rand/cbook-wearing variants of the same core (it excludes
them; `--core` now includes them). Mined and fit 6 cluster books (2 cores --
the top-Elo bare `classic` chip counter and the top-Elo bare linear
`tdleaf_self` model on `ab(deep=6,tt,ord,nodes=200k)@1` -- x 3 scopes each)
with matched fit settings, then screened all 4 distinct resulting agents
(one of the 6 is a byte-identical duplicate, see below) via a pinned fit
against the standing screening pool. Result: **every cbook variant of both
cores rated below its bare baseline** at `ply=16`, `keep=6`, `clusters=16` --
this is a negative finding for the filter mechanism at this configuration, not
a confirmation of the plan's hypothesis. Mining scope (exact id vs core vs
regime) did not change that outcome for either core.

## Changes made

- `src/ranking.h` / `src/ranking.cpp` -- `rankClusterBookDump` gained a third
  scope parameter, `coreId`. A new static helper, `rankIdWithoutOpener(id)`,
  strips a trailing `.opener(...)@N` segment (the opener segment is always
  emitted last in the id-building code, so a substring search suffices,
  documented at the helper). Scope resolution is now: `--a` (exact id) >
  `--core` (id match after stripping any opener) > `--regime` (unchanged) >
  none (universal), and at most one of the three may be given.
- `tools/rank_main.cpp` -- wired `--core` into the `cbookdump` dispatch and
  usage text.
- `tools/CLAUDE.md`, `src/CLAUDE.md` -- reference updates: the `--core` flag
  in the subcommand list and the `rankClusterBookDump` prose entry, and the
  "Mined cluster books" ledger extended with `cbook2`-`cbook7`.
- `ranking/roster_cbook_scope_study.txt` / `ranking/cohort_cbook_scope_study.txt`
  (new, tracked) -- a copy of `ranking/roster_screening_pool.txt` plus the 4
  screened cbook-wearing agents, and the cohort id list used to play only
  their games.
- `models/cbook2.txt` .. `models/cbook7.txt` (new, tracked data artifacts) --
  see the mining table below. `models/cbook1.txt` (Pass 1a) is unchanged.
- Not committed (regenerable, gitignored): the 5 `data/cbook_dump_*.jsonl`
  dumps these were fit from.

## How to test

```powershell
.\tools\run_tests.ps1 -Build
```

Rebuild every linked binary after this change (`ranking.h`/`.cpp` are linked
into `tests.exe`, `train.exe`, `rank.exe`, and the GUI, not `breakthrough.exe`
per `CLAUDE.md`'s engine link set) -- confirmed this session by rebuilding
`rank.exe` and `tests.exe` directly and rerunning the full suite.

To reproduce the mining + screen:

```powershell
.\rank.exe cbookdump --a    "ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2" --board boards/board1.txt --max-plies 32 --out data/cbook_dump_classic_exact.jsonl
.\rank.exe cbookdump --core "ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2" --board boards/board1.txt --max-plies 32 --out data/cbook_dump_classic_core.jsonl
.\rank.exe cbookdump --regime classic                                            --board boards/board1.txt --max-plies 32 --out data/cbook_dump_classic_regime.jsonl
.\rank.exe cbookfit --in data/cbook_dump_classic_exact.jsonl  --clusters 16 --keep 6 --mirror canon --seed 1 --out-slot 2
.\rank.exe cbookfit --in data/cbook_dump_classic_core.jsonl   --clusters 16 --keep 6 --mirror canon --seed 1 --out-slot 3
.\rank.exe cbookfit --in data/cbook_dump_classic_regime.jsonl --clusters 16 --keep 6 --mirror canon --seed 1 --out-slot 4
# same pattern for the linear-169 core into cbook5 (--a/--core, same dump) and cbook7 (--regime tdleaf_self)

.\rank.exe check --roster ranking/roster_cbook_scope_study.txt
.\tools\run_rank.ps1 -Workers 6 -NoRate play --roster ranking/roster_cbook_scope_study.txt --cohort ranking/cohort_cbook_scope_study.txt --games 16
.\rank.exe rate --roster ranking/roster_cbook_scope_study.txt --pin ranking/standings.tsv
```

## Results

### Agent selection

Chosen from a fresh `ranking/standings.tsv` fit (`rank.exe rate`, 2026-08-26),
the top-Elo BARE (no opener/dilution) agent on `ab(deep=6,tt,ord,nodes=200k)@1`
in each of the two requested families:

| Core | Id | Elo (bare) | Games (bare, historical) |
|---|---|---|---|
| classic chip counter | `classic(chip=100)@2` | 923 | 3196 |
| linear `tdleaf_self` model | `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | 1031 | 1975 |

### Scope sizes (distinct winning games, measured via a `--sample 1` probe before committing to full replay)

| Scope | classic | linear-169 |
|---|---|---|
| `--a` (exact id) | 2,173 | 1,592 |
| `--core` (any opener) | 17,906 | 1,592 (identical set -- no opener-wearing wins exist for this model in the store) |
| `--regime` (`classic` / `tdleaf_self`) | 78,360 | 17,849 |

`--core` equalling `--a` for linear-169 is a real finding, not an omission:
this specific model has no roster lines pairing it with `book`/`rand`/`cbook`,
so its `--core` and `--a` scopes are provably the same game set. `cbook6`
(linear `--core`) is therefore byte-identical to `cbook5` (linear `--a`) and
was not separately screened -- screening it would have replayed the exact
same games and produced the exact same result.

### Mining and fitting

All 5 distinct dumps were run as parallel background processes (`--max-plies
32` throughout). Total wall time from launch to all 5 complete: **~12.5
hours**, dominated by the two regime-scope dumps (classic regime finished
last, at ~12.5 hours; `tdleaf_self` regime finished earlier, at ~8 hours).
This is real alpha-beta search replay, not a cheap scan: a 20-game isolated
sample measured 0.66s/game for the classic core and 2.39s/game for the linear
model before the parallel batch launched, projecting ~14.4 hours if the batch
ran uncontended; the observed 12.5 hours is close to that despite 5 processes
sharing the machine, because a meaningful fraction of "replayed" games in the
larger scopes are near-instant skips (stale/unparseable ids, see the kept-replay
counts below) rather than full searches.

| Dump | Games attempted | Kept | Drifted | Unparseable/stale id | Positions | `cbookfit` time (16 clusters, keep 6, canon) | Mean intra-cluster cosine |
|---|---|---|---|---|---|---|---|
| classic `--a` | 2,173 | 1,541 | 359 | 273 | 22,622 | 0.59s | 0.739 |
| classic `--core` | 17,906 | 14,435 | 2,440 | 1,031 | 214,819 | 10.76s | 0.718 |
| classic `--regime` | 78,360 | 52,432 | 9,381 | 16,547 | 764,840 | 52.74s | 0.701 |
| linear `--a`/`--core` | 1,592 | 1,380 | 146 | 66 | 21,765 | 0.55s | 0.706 |
| linear `--regime` (`tdleaf_self`) | 17,849 | 13,577 | 3,647 | 625 | 210,216 | 15.81s | 0.652 |

`cbookfit` (the clustering step, no replay) is cheap regardless of scope, well
under a minute even for the 764,840-position classic regime dump. Mean
intra-cluster cosine decreases as scope widens for both cores, consistent
with a broader population producing more diffuse clusters, though the
difference is modest (0.739 down to 0.701 for classic, 0.706 down to 0.652
for linear across roughly 8x and 10x more positions respectively).

### Win-rate / Elo effect

Screened via `rate --pin ranking/standings.tsv` against
`ranking/roster_screening_pool.txt` (27 opponents + `rand@1` anchor,
Elo-spread and opener-diverse), 16 games/pair, all 4 distinct cbook-wearing
agents at `ply=16`. Pinned Elo is the valid comparison here (same fit, same
frozen reference scale); each cbook agent's own SE (`pm`) is real since it
was solved freely from 242 games, while the bare baseline's SE is 0 by
construction (it is one of the pinned inputs).

| Agent | Elo | vs. bare | pm | Games | W-L (White) | W-L (Black) | Win rate |
|---|---|---|---|---|---|---|---|
| classic, bare | 923 | -- | 0 | 3196 (historical) | 1098-500 | 1078-520 | 68.1% |
| classic + `cbook2` (`--a`) | 899 | **-24** | 26 | 242 | 84-37 | 85-36 | 69.8% |
| classic + `cbook3` (`--core`) | 915 | **-8** | 26 | 242 | 90-31 | 83-38 | 71.5% |
| classic + `cbook4` (`--regime`) | 895 | **-28** | 26 | 242 | 91-30 | 77-44 | 69.4% |
| linear-169, bare | 1031 | -- | 0 | 1975 (historical) | 808-174 | 791-202 | 81.0% |
| linear-169 + `cbook5` (`--a`/`--core`) | 964 | **-67** | 27 | 242 | 99-22 | 86-35 | 76.4% |

**Every cbook variant of both cores rated below its bare baseline.** The raw
win rates above are NOT directly comparable to the bare baseline's historical
win rate (different, broader opponent population over the bare agent's full
history vs. this fixed screening pool for the cbook variants), which is
exactly why the pinned Elo column, not the win-rate column, is the load-bearing
comparison here: it accounts for opponent strength via the same Bradley-Terry
fit both numbers come from.

Two things about the size of these deltas: classic's spread across scopes
(-8 to -28) sits well inside this project's documented 50-150 Elo seed-noise
band and should not be read as "core scope is better than exact/regime scope"
without repetition at another k-means seed -- this is one fit, one seed,
consistent with the plan's own observation that seed is a genuine source of
variation (Pass 1a's Rand-index measurement, 0.78-0.85 agreement between
seeds). Linear's -67 is larger relative to its own pm (27) but is likewise a
single reading, not a certified result.

## What this does and does not show

This measures win-rate/Elo only, at one configuration (`ply=16`, `keep=6`,
`clusters=16`, `mirror=canon`, `seed=1`) and 16 games/pair against one fixed
pool. It does not re-run Pass 1a's hit-rate-vs-ply mechanism check for these
specific books (that mechanism -- the cluster match firing on a legal move
where the exact-hash book cannot -- was already established in Pass 1a and is
not scope-dependent in how it was measured there). What it adds is the
missing piece Pass 1a explicitly deferred: whether firing on a legal move
translates into a stronger agent. At this configuration, for both cores
tried, it does not -- filter mode at `ply=16`/`keep=6` cost both cores Elo
against this pool, regardless of mining scope.

## Correctness gotchas

- **The `--core` scope's own filter logic reuses the id-emitter's ordering
  invariant rather than parsing the opener segment.** `rankIdWithoutOpener`
  just truncates at the first `.opener(` substring, which is only correct
  because the id-building code always emits `.opener(...)@N` last (after any
  dilution segment) and appends nothing after it. This is documented at the
  helper's definition rather than re-derived from the grammar at each call
  site, since re-parsing would be strictly more work for the same answer
  given that invariant holds.
- **Scope-size probing via `--sample 1` is exact, not a sample.** The
  distinct-game count `cbookdump` prints (`N distinct winning games`) is
  computed by the scope filter + dedup pass BEFORE the sample cap is applied,
  so `--sample 1` reports the true population size for the cost of one
  replay. This was used deliberately to project total mining cost before
  committing to the full parallel run, and is worth reusing for any future
  scope sizing rather than reasoning from the regime win-count table in the
  original plan (which predates deduplication and undercounts distinct
  games relative to the raw per-row totals it was built from).
- **Throughput contention under 5 parallel `rank.exe` processes was real but
  smaller than expected.** An isolated single-process measurement (0.66s/game
  classic, 2.39s/game linear) projected ~14.4 hours for the slowest scope
  running alongside 4 others; the batch actually finished in ~12.5 hours. The
  likely explanation is that a large fraction of "replayed" games in the
  wider scopes are near-instant skips (16,547 of 78,360 attempts for the
  classic regime dump were unparseable/stale ids, resolved before any search
  runs), not that contention was absent.

## Future Work

- **This is not Pass 1b.** The plan's actual Pass 1b (universal vs. gated-
  universal vs. per-regime vs. a label-permuted control, across the volume-
  bar-qualifying regimes) has still not run. This round answers a narrower,
  differently framed question (does opener-inclusive/regime-inclusive mining
  change the outcome for two SPECIFIC already-chosen cores) and should not be
  read as having resolved or superseded the plan's own scope-decision design.
- **The negative Elo result was measured at exactly one configuration.**
  Whether a different `keep` (the plan's stated primary knob) or a different
  `ply` cutoff turns this positive is untested here -- Pass 2's grid
  (`--keep` in particular) is the design already built for that question and
  was not run this round.
- **Only one k-means seed was used for every book in this round.** Given
  Pass 1a's measured 0.78-0.85 seed agreement, part of the spread among
  classic's three scope variants (-8 to -28) could be seed noise rather than
  a scope effect. Re-fitting at 2-3 seeds per scope would separate the two,
  per the plan's own minimum-seed requirement for any conclusion.
- **The label-permuted control (the plan's Pass 1b arm 4) was not built or
  screened here.** Given the negative result above, a control that keeps
  the same whitelist sizes but destroys the position-to-move association
  would help distinguish "the cluster's specific moves are actively bad
  narrowing" from "any comparably-sized root restriction hurts these two
  cores at this depth/node budget," which the current data cannot separate.

## Ideas This Inspired

- **A "soft matching" (move-ordering bias instead of hard root filter) run on
  these same two cores' existing books** would directly test whether the
  negative result here is specific to filter mode's hard restriction (already
  logged as deferred work in the original plan) rather than to the cluster
  match itself.
- **The `--core` scope is a generally useful sizing tool beyond this study**:
  probing scope size via `--sample 1` before committing to a mining run (used
  here to catch the 32-hour serial / 14.4-hour parallel projection before
  spending it) is cheap enough to make a habit of before any future
  `cbookdump` invocation at unknown population size.
