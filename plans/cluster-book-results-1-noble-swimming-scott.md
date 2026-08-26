# Cluster-matched opening book ("cbook"): Pass-1a results

Companion results doc for `plans/cluster-book-plan-1-noble-swimming-scott.md`.
This covers implementation plus Pass 1a (plumbing sanity) only. Passes 1b
(mining-scope decision), 2 (broad sweep), and 3 (optimize) have not run and
are gated on developer review of this document, per the plan's own
interactivity checkpoint and `Docs/model-training-playbook.md`.

## Summary

Built and validated a new opener, `cbook`, that generalizes the project's
exact-position-hash opening book (`book`) to fuzzy nearest-cluster matching
(Steinmetz & Gini's SMARTSTART, IJCAI 2015, translated from Go/MCTS to
Breakthrough/alpha-beta). Where `book` needs an exact `positionKey` hash and
goes silent the instant the opponent deviates (theory 38), `cbook` matches the
live position to its nearest mined cluster for the current half-move and
narrows the search's ROOT move list to that cluster's historically-played
moves, letting the agent's own budgeted alpha-beta search choose among the
survivors. This was measured directly: against an opponent forced random for
its own first 8 moves, the exact-hash book's hit rate collapsed from 100% to
0% by half-move 3, while the cluster book's fuzzy match kept firing at
53-100% through half-move 15.

## Changes made

New files:

- `src/ml_cluster.h` / `src/ml_cluster.cpp` -- spherical k-means core, XOR
  difference-vector construction, left-right canonicalisation, book file
  format (`MlcBook`/`MlcBucket`/`MlcCluster`/`MlcMove`), save/load. Pure,
  dependency-free, no `rand()`.

Modified files:

- `src/globals.h` / `src/globals.cpp` -- new `g_useRootFilter` (bool),
  `g_rootMoveWhitelist[ROOT_FILTER_MAX][3]`, `g_rootMoveWhitelistCount`, and
  `rootMoveAllowed(sx,sy,dx)`. `ROOT_FILTER_MAX=64` matches `ML_MAX_MOVES`.
- `src/ai_minimax.cpp` -- `searchRootWhite`/`searchRootBlack`'s `tryMove`
  lambda checks `rootMoveAllowed` first and skips (without counting toward
  `rootTotal`) any candidate the filter excludes. Root-loop only, the
  recursive `maxAlphaBeta`/`minAlphaBeta` are untouched.
- `src/ai_random.cpp` / `src/ai_random.h` -- new `cbook` opener
  (`openerClusterBook`), lazy-loaded/cached `models/cbook<N>.txt` files the
  same way `book` caches, `g_openers[]` row.
- `src/ranking.cpp` / `src/ranking.h` -- `rankClusterBookDump` (the
  `cbookdump` subcommand: replay + record) and `rankClusterBookFit` (the
  `cbookfit` subcommand: cluster a dump into book files, no replay). Both
  `playOneGame` and pairgen's `playoutCapture` now reset
  `g_useRootFilter = false` unconditionally after each move, so a
  `cbook`-narrowed root list can never leak into a later ply.
- `tools/rank_main.cpp` -- CLI wiring for `cbookdump`/`cbookfit`, plus a
  small `parseIntList` helper for `--clusters`/`--keep`'s comma lists.
- `build_tests.bat` / `build_rank.bat` / `build_train.bat` -- link
  `src/ml_cluster.cpp`.
- `tests/test_ml.cpp` -- 10 new test cases covering `MlcVec` bit operations,
  the XOR-difference construction (verified sparse and lossless on a real
  board), canonicalisation, `mlcEffectiveK`'s density cap, spherical k-means
  (K=1 recovers the normalized mean, separates well-separated synthetic
  groups deterministically given a seed, handles all-identical and all-zero
  degenerate inputs without crashing), and book file save/load round-trip.
- `tests/test_ranking.cpp` -- ID round-trip coverage for `cbook` (with and
  without `ply=`) and for `book`'s own previously-unreachable `ply=` cutoff
  (see "Correctness gotchas" below), plus 4 new test cases exercising the
  `cbook` opener directly: matches and narrows the filter, an empty legality
  intersection leaves the filter off (the required safeguard), the `ply=`
  cutoff declines without touching the filter, and a canonicalised match
  un-mirrors correctly onto a live position that canonicalises to the other
  mirror image.
- `tests/test_ai_integration.cpp` -- 3 new test cases for the root filter
  itself: it reduces node count at the same depth, `rootMoveAllowed` is
  inert when the flag is off, and a real search plays a whitelisted quiet
  move instead of the capture it would otherwise prefer.
- `src/CLAUDE.md`, `tools/CLAUDE.md`, `README.md`, `todo.md`,
  `Docs/theories.md` -- reference updates (see each file's diff, since the
  `tools/CLAUDE.md` "Mined cluster books" ledger and `Docs/theories.md`
  theory 53 are the substantive new content, everything else is a pointer).

New data artifact (git-tracked, mirroring the `book<N>.txt` convention):

- `models/cbook1.txt` -- Pass-1a's mined book. Scope: one core's own wins
  (`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2`, the same core behind
  `book13`/`book14`). 4 clusters/bucket, `keep=6`, `mirror=canon`, seed 1.
  See "Mining results" below.

Not committed (regenerable, gitignored): `data/cbook_classic.jsonl` (the
dump), `models/cbook2.txt` (a second-seed fit used only for the stability
check below, deleted after measuring).

## How to test

```powershell
.\tools\run_tests.ps1 -Build
```

4293 assertions pass (up from 4243 before this session), including all new
`ml_cluster`/`cbook` coverage.

To reproduce the mining pipeline:

```powershell
.\rank.exe cbookdump --a "ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2" `
  --board boards/board1.txt --max-plies 32 --out data/cbook_classic.jsonl
.\rank.exe cbookfit --in data/cbook_classic.jsonl --clusters 4 --keep 6 `
  --mirror canon --seed 1 --out-slot 1
.\rank.exe check --roster <a roster file carrying .opener(cbook,cbook=1,ply=8)@1>
.\rank.exe play --roster <that roster> --games 4
```

## Results

### Mining (`cbookdump` + `cbookfit`)

`cbookdump` on the Pass-1a scope: 268,169 store rows -> 2,173 distinct
winning games for this one core -> 1,520 kept after replay (380 drifted from
their stored result, 273 unparseable/stale ids, 0 duplicate rows -- this
scope's games happened not to contain a duplicated deterministic pair) ->
22,408 recorded positions across 32 half-move buckets.

`cbookfit` at `clusters=4, keep=6, mirror=canon, seed=1`: 128 clusters total
(4 per bucket x 32 buckets), cluster sizes 0-737, mean intra-cluster cosine
0.680. The ply-0 bucket collapses to exactly one real cluster with the other
3 empty by construction: every game's ply-0 position IS the start board, so
its XOR difference is the zero vector for all 737 recorded points, and
k-means' farthest-point reseeding has no distinguishing content to reseed
from (documented in `src/ml_cluster.h`). Later buckets show genuine
structure: ply 8 has 4 clusters sized 146-216 with mean intra-cluster cosine
0.62-0.70 and visibly different move distributions (e.g. one cluster's top
move is `1,2,0` at count 45, another's is `7,2,6` at count 50).

### Fuzzy-match-fires-where-exact-wouldn't (the core hypothesis)

Method: a one-off harness (not part of the committed test suite, scratch
code) replayed 60 games between the Pass-1a core (agent A) and the same core
wearing `.opener(rand,moves=8)@1` (agent B, forced random for its own first 8
moves -- both colours sampled, 30 games each). At every one of A's plies
within half-move 0-15, it checked: is this position an exact hit in
`book14.txt` (the existing 8-ply exact-hash book mined from this same core's
wins)? Does `cbook1.txt`'s nearest-cluster match have a non-empty
intersection with A's actual legal moves?

| Half-move | n | Exact hit rate (`book14`) | Fuzzy fire rate (`cbook1`) | Mean intersection size |
|---|---|---|---|---|
| 0 | 30 | 1.00 | 1.00 | 6.0 |
| 1 | 30 | 0.13 | 1.00 | 6.0 |
| 2 | 30 | 0.10 | 0.53 | 2.9 |
| 3 | 30 | 0.00 | 1.00 | 3.4 |
| 4 | 30 | 0.00 | 0.57 | 1.7 |
| 5 | 30 | 0.00 | 1.00 | 4.4 |
| 6 | 30 | 0.00 | 1.00 | 2.6 |
| 8 | 30 | 0.00 | 0.97 | 1.9 |
| 12 | 27 | 0.00 | 0.56 | 1.0 |
| 15 | 29 | 0.00 | 1.00 | 2.7 |

(Full 16-row table, including half-moves 7/9-11/13-14, in the harness output,
omitted here for length, no different in character from the rows shown.)

The exact-hash book's hit rate is 100% at half-move 0 only because half-move
0 IS the shared start position for every game from `boards/board1.txt`, so
that row is a trivial floor, not evidence about the mechanism. From
half-move 1 (the first ply after the opponent's forced-random move) it falls
to 13%, then 0% from half-move 3 onward -- it never recovers for the rest of
the window. `cbook1`'s fuzzy match keeps firing throughout, 53-100% per
half-move, with a mean intersection size of 1-6 legal moves (against 22+
legal moves typically available), showing the root filter does real
narrowing rather than degenerating into "matches everything."

**This measures the mechanism, not playing strength.** A non-empty legality
intersection means the matched cluster has SOME legal moves in its
historical move list, which says nothing about whether those moves are any
good. That is exactly what Pass 2/3's actual Elo screening is for. This
result establishes that the mechanism has something to offer past the point
an exact-hash book has nothing at all, which is the necessary precondition
for a strength result to even be possible.

**Caveat on how aggressive this test is.** Forcing EVERY one of the
opponent's own first 8 moves to be random is a much larger perturbation than
theory 38's original test (which found the collapse "within a few plies" of
ordinary diversified play, not immediate). This test's near-instant collapse
(13% by half-move 1, 0% by half-move 3) should be read as "the exact-hash
mechanism has zero tolerance for this specific, aggressive diversification,"
not literally "collapses in 1-3 plies" as a property of theory 38 in general.
The comparison between the two mechanisms under the SAME aggressive
condition is what is being claimed, not an absolute collapse rate.

### K-means seed stability

Method: a second one-off harness fit the SAME dump's data at two different
k-means seeds (1 and 2, same `clusters=4`) for three representative buckets,
reporting the pairwise co-clustering agreement rate (Rand index: the
fraction of point PAIRS whose "same cluster / different cluster" status
agrees between the two fits, well-defined even when the two runs number
their clusters differently).

| Half-move | Points | K_eff (both seeds) | Rand index |
|---|---|---|---|
| 8 | 737 | 4 | 0.787 |
| 16 | 698 | 4 | 0.776 |
| 24 | 618 | 4 | 0.853 |

Moderate agreement (0.78-0.85), not near 1.0 (which would mean the seed
doesn't matter) and not at the chance floor either. This means the k-means
seed is a real, non-trivial source of variation in which cluster a given
position lands in. It validates keeping the playbook's minimum of >= 3 seeds
per Pass-2 configuration as load-bearing, not a formality: seed noise here is
a genuine variable, not a rubber stamp.

### Root filter mechanism (unit-tested, deterministic)

Three assertions the game-play behavior in the table above depends on, all
in `tests/test_ai_integration.cpp`:

- The filter reduces node count at the same search depth when a wide-open
  position's 9 legal root moves are narrowed to 1.
- `rootMoveAllowed` is inert (everything allowed) when `g_useRootFilter` is
  false, and correctly restrictive when true.
- A real depth-3 search that would otherwise capture a piece instead plays
  the one whitelisted quiet move when the filter excludes the capture --
  confirming the filter actually changes what the engine PLAYS, not just an
  internal node count.

### Live gameplay smoke test

34 games played (`rank.exe play --games 4` over a 5-agent roster: the bare
core, the core wearing `cbook`, the core wearing `book14`, the core wearing
`.opener(rand,moves=8)`, and `rand@1`) with no crash, across every pairing
including `cbook`-vs-`cbook`, `cbook`-vs-`book`, and `cbook`-vs-random-brain.
`rank.exe check` accepted `.opener(cbook,cbook=1,ply=8)@1` in a roster line.

## Implementation notes and differences from the plan

The plan's own "Measured grounding" and "Mining pipeline" sections already
reflect a from-scratch redesign done during planning (the dump/fit split,
the XOR-difference construction, canonicalisation over augmentation, the
`--keep` cutoff as the primary axis) -- those are not deviations from an
earlier version of the plan, they ARE the approved plan. What changed
DURING implementation, beyond that:

- **A pre-existing parser bug was found and fixed.** `ranking.cpp`'s
  `opener()` segment parser rejected any id with more than 2 arguments
  before checking whether that specific opener's `hasArg2` flag allowed a
  3rd. This meant `book`'s own documented `ply=` cutoff -- implemented since
  2026-08-03 and referenced in `tools/CLAUDE.md` as "verified 2026-08-03
  that a 4-ply mining of one pair produced entries identical to the first 13
  rows...", a verification that called the opener function directly with a
  hardcoded `arg2`, bypassing the ID parser entirely -- had NO roster
  spelling that actually parsed. `.opener(book,book=N,ply=M)@1` would have
  been rejected by `rank.exe check` before this session. Fixed by raising
  the parser's upper bound from 2 to 3 arguments and letting the per-opener
  `hasArg2` check (which already existed) govern the real limit.
  `tests/test_ranking.cpp` now round-trips this explicitly for both `book`
  and `cbook` so it can't regress silently again. This means any PRIOR claim
  in this project's history that assumed `book`'s `ply=` cap was usable via
  a roster line was never actually exercisable that way. The mechanism
  itself was correct (verified by direct function call), only the ID
  grammar's path to it was broken.
- **`models/cbook99.txt` scratch-file leak, caught and fixed.** The first
  draft of the `cbook` opener tests reused ONE scratch slot (99) across four
  test cases. `cbookForSlot` caches a loaded book by slot number for the
  process lifetime (the same convention `bookForSlot` already uses,
  documented as intentional), so the second, third, and fourth test cases
  were silently served the FIRST test's cached content regardless of what
  each test had just written to disk. Fixed by giving each test its own
  scratch slot (9901-9904). The underlying caching behavior is correct and
  unchanged, only the test's reuse of one slot across cases was wrong.
- **The un-mirror test needed a redesign mid-writing.** An early draft
  constructed the "before" position and the "tested move" from the SAME two
  squares, which conflated "the position the cluster was mined from" with
  "the move's own effect on that position" -- structurally impossible, since
  a mined cluster's centroid describes the position BEFORE its associated
  move, using whatever OTHER changes happened in earlier plies, not the
  move's own squares. Fixed by using two unrelated square pairs (columns 1/0
  for the "before" position's own prior move, columns 2/3 and 5/1/4 for the
  tested move and its mirror), which is also a more accurate model of what a
  real mined bucket actually looks like.

Nothing else diverged from the approved plan's design. Code placement
matches the plan's "Files touched" table exactly.

## Correctness gotchas

- **Deduplication found nothing to deduplicate for this specific scope**
  (`cbookdump` reported 0 duplicate rows collapsed for the classic-core
  scope), which is a real, expected result rather than a sign the dedup
  logic is dead code: this scope's 2,173 distinct games came from many
  different opponents rather than one deterministic pair replaying itself,
  so the `(w,b,seed,r,plies)` dedup key never collided. The universal and
  per-regime scopes Pass 1b will use are expected to hit this far more,
  since they pool across the whole store where deterministic pairs are
  common (measured store-wide: 0.41 distinct games per row).
- **273 of 2,173 candidate games (12.6%) carried unparseable/stale agent
  ids** even restricted to ONE agent's own wins, consistent with this
  project's long id-grammar history (label migrations, superseded
  spellings). `rankUpgradeId` is applied by `rankLoadMatches` before
  `cbookdump` ever sees a row, so this is the residual after that
  normalization, not evidence it's missing.
- **380 of 1,900 replay attempts (20%) drifted from their stored result**
  even for a single-core scope with no stochastic opener on this side,
  consistent with the project's own theory 19b (cross-game transposition-
  table state making replay imprecise) and with `models/book2.txt`'s
  existing header (12 of 32 replays drifted on a similarly deterministic
  pair). `cbookdump` follows `rankExtract`'s policy (drop a drifted replay)
  rather than `rankBookGen`'s (keep it if the recorded winner still won),
  since attributing positions from a game that didn't happen to the stored
  winner is exactly the instrument-validation problem this project's
  standing rules exist to catch.

## Future Work

Each entry is tied to the specific claim it would confirm or refute, per the
project's convention -- not a generic todo dump.

- **The fuzzy-match hit-rate curve was measured against ONE aggressive
  diversification (opponent forced random for its own first 8 moves).**
  Whether the same qualitative pattern (exact book collapses fast, cluster
  book keeps firing) holds under a milder, more realistic diversification
  (e.g. `pairgen --open-plies 2` on one side only) is untested. This matters
  because the current result's caveat ("this is a much larger perturbation
  than theory 38's original test") means the current numbers may overstate
  how fast a REAL opponent would knock the exact book out. A milder test
  would give the honest comparison at a diversification level closer to
  what theory 38 originally measured.
- **Pass 1b (mining-scope decision: universal vs. per-regime vs. hybrid) has
  not run.** This is the next planned step, explicitly gated on developer
  review of this document per the approved plan. The volume-bar analysis in
  the plan (7 qualifying regimes, `gumbel_self` and `tdleaf_self`
  structurally excluded) is grounded but not yet executed.
- **Whether the matched cluster's suggested moves are actually GOOD moves is
  completely untested.** The Pass-1a result shows the mechanism keeps firing
  where the exact-hash book cannot, but firing on a legal move is not the
  same as firing on a strong one. Only Pass 2's Elo screening answers this.
  A cheap intermediate check worth doing before a full Elo study: for the
  positions where `cbook1` fires, what fraction of the time is one of its
  suggested moves the SAME move the core's own unrestricted search would
  have picked anyway? A low agreement rate combined with a positive Elo
  result would be the more interesting outcome (the filter is doing more
  than mimicking the brain's own first choice).
- **The `ply=` cap's live effect was verified via unit tests (declines
  before its cutoff, fires after) but not via a live two-setting comparison
  with real telemetry** (e.g. root-filter engagement fraction over a game
  batch at `ply=4` vs `ply=32` on the same book file). The unit test is
  more rigorous for the mechanism itself, but a live number would double as
  a sanity check on the `cbook` opener's ply-cap wiring end to end, the way
  the fuzzy-match table above did for the matching logic.

## Ideas This Inspired

- **Mine the screening stores** (`ranking/matches_screen_gz*.jsonl`), not
  just the loaded ladder parts, to give the newest regimes (`gumbel_self`,
  currently 0 games in the loaded store) a cluster book without touching
  ranking hygiene, since mining is a data-mining step rather than a ranking
  claim.
- **Soft matching (bias, not filter) via move ordering.** Feed a cluster's
  move counts into the existing `ord` move-ordering path as a first-move
  hint rather than a hard root restriction. Strictly safer than filtering
  (nothing is ever excluded), and would separate "the cluster knows which
  move is good" from "restricting the root helps," which the current filter
  design conflates.
- **Per-bucket cluster-count selection** (e.g. the smallest K whose mean
  intra-cluster cosine clears a threshold) instead of one global K clamped
  by density, so early buckets stay coarse and late buckets go fine
  automatically as the data thins.
- **Agent-behavioral clustering** (cluster agents by whether they win from
  identical openings, then mine a book per agent-cluster): logged in the
  plan's own "Deferred" section, repeated here for visibility since it's a
  genuine two-level extension beyond SMARTSTART itself.
