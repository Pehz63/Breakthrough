# Cluster-matched opening book ("cbook"): applying SMARTSTART to Breakthrough

## Context

The developer asked to apply the ideas from Steinmetz & Gini's SMARTSTART paper
(clustering professional Go positions so a Monte Carlo search can match a
*similar*, not exact, position and still get move guidance) to this project.

Breakthrough's search is alpha-beta with a node/time budget, not MCTS, and it
already has an exact-hash opening book (`ai_random.cpp`'s `book` opener,
`models/book<N>.txt`, keyed by `positionKey(...).hash`). That book has a
documented failure mode: **Theory 38** found its Elo lift is a memorized-line
artifact that "collapses under opening diversification," because it falls out
of book the instant the opponent's move isn't the exact stored one. `todo.md`'s
"Books" section already has two open items reaching for something better
(pool win rate across the whole roster; mine a book that recovers from a
random opening) but both are still exact-hash keyed. `CHAMPION.md` separately
demoted the book divisions on 2026-08-24 pending "a self-maximizing mining
methodology... not yet scoped."

SMARTSTART's actual contribution -- fuzzy nearest-cluster matching instead of
exact match, so the guidance never fully disappears, just degrades gracefully
-- is the piece none of the above has. This plan builds it as a new opener,
`cbook`, and runs it through the model-training-playbook's three-pass process
before any strength claim is made.

The developer chose the full engine build (not a cheap offline test first) and
picked **filter mode** over **replace mode** for how the matched cluster's
moves are used: the opener restricts the search's root move list to the
cluster's candidates and lets the agent's own budgeted alpha-beta search pick
among them, rather than blindly playing the cluster's top move. This is what
actually reproduces the paper's mechanism (a compute-for-depth trade) and is
the version that could plausibly survive Theory 38's failure mode, since the
brain still evaluates every surviving candidate instead of trusting one stored
move outright.

## Measured grounding

Every number in this plan's design choices comes from the repo, measured
2026-08-26. Method: a field-extracting `awk` pass over the three loaded store
parts named in `ranking/matches.index.txt` (`matches.roster.0001.jsonl`,
`matches.retired_other.0001.jsonl`, `matches.jsonl`), joined against
`ranking/ratings.tsv` (2026-08-24 fit) for winner strength, plus a read of
`genWhite` in `src/ml_features.cpp` for the branching facts. The paper's own
scale numbers (64 to 256 clusters over roughly 8,000 positions per move
number, on a 361-point board) are not carried over, because none of the three
quantities they were sized from match this project.

| Quantity | Measured value |
|---|---|
| Store rows loaded | 268,169 |
| Distinct games, keyed `(white, black, seed, result)` | 108,939 (0.41 per row, consistent with the 0.438 median `CLAUDE.md` records) |
| Draws | none, every row is decisive |
| Start boards used | `boards/board1.txt` only, all 108,939 games |
| Distinct agent ids in the store | 673, over 81,406 ordered pairs (1.34 distinct games per pair) |
| Games with a stochastic opener or dilution | 65,046 (60%), the rest are deterministic pairs replaying one line per colour |
| Game length (20,000-row sample of the live tail) | mean 49.4 half-moves, median 51, p10 24, p90 71, max 106 |
| Legal moves at half-move 0 | exactly 22, and rising into the high twenties as the back rank clears |

Won games by regime tag (`rankAgentRegime` on the winner), distinct games:

| Regime | Won games | | Regime | Won games |
|---|---|---|---|---|
| `classic` | 29,163 | | `exp` | 3,562 |
| `learned?` | 30,817 | | `dist` | 3,546 |
| `value` | 15,006 | | `adv` | 2,697 |
| `pool_games` | 14,133 | | `weight_merge` | 1,656 |
| `position_elo` | 5,769 | | `tdleaf_self` | 1,555 |
| | | | `nonlearning` | 1,035 |
| | | | `gumbel_self` | **0** |

Two entries in that table drive real decisions. `learned?` is not a regime, it
is the legacy two-field `learned(slot,hash)` id form whose regime is
unrecoverable, so its 30,817 games can feed a universal book but can never get
a dedicated one. `gumbel_self` has zero games in the loaded store: its
five rostered agents played into the separate screening stores
(`ranking/matches_screen_gz*.jsonl`), which `matches.index.txt` does not load.
The newest two regimes, `gumbel_self` and `tdleaf_self`, are therefore exactly
the two that cannot support a dedicated book from this data.

Winner strength, joining winners to `ranking/ratings.tsv`: 43,708 of the
108,939 distinct games matched. The unmatched remainder carries
pre-`rankUpgradeId` id spellings that this offline join did not normalise, so
treat the matched count as a floor rather than a measurement, and have
`cbookgen` apply `rankUpgradeId` before joining. Within the matched portion,
977 games (2.2%) were won by an agent at >= 1000 Elo, 8,630 at 900 to 999,
5,845 at 800 to 899, 8,832 at 700 to 799, 4,960 at 600 to 699, and 14,464
(33%) below 600. **The store is dominated by weak play.** The paper mined
professional games. Pooling every winner here mines mostly amateur play, which
makes a strength gate a first-class design parameter rather than a refinement.

Two more facts that bound the design. The exact-hash books this has to beat
are tiny: `models/book15.txt` (4-ply, s98's own) holds 22 entries and
`models/book16.txt` (8-ply) holds 60, both from 25 kept winning replays of 32
stored rows. And replay is not reliably reproducible: `models/book2.txt`'s own
header records 12 of 32 replays drifting from the stored result on a pair with
no stochastic opener and no dilution (theory 19b, cross-game state).

## Position representation and clustering

- **Cluster the difference from the start, not the raw board.** Reuse
  `mlExtractValueFeaturesV2` (`src/ml_features.h`, 129-dim: 64 white-plane +
  64 black-plane + 1 side-to-move) to read the position, drop feature 128
  (constant within a ply bucket, since Breakthrough alternates strictly with
  no passing), then **XOR the remaining 128 entries against the start
  position's vector** and cluster that difference.

  Why the raw vector fails here, and why the paper did not have this problem:
  a Go position at move 20 has about 20 stones on 361 points, so two different
  move-20 positions overlap barely at all and cosine similarity is genuinely
  discriminative. A Breakthrough position at half-move 20 still has close to
  32 pieces on 64 squares, and at most 2 to 3 of the 128 vector entries change
  per half-move, so any two same-ply positions agree on the overwhelming
  majority of entries. Cosine similarity between them sits near 1 whatever
  they actually are, the shared baseline carries nearly all the vector's
  norm, and spherical k-means has almost no angular signal to separate on.
  The XOR removes exactly that constant baseline. It is lossless given the
  start board (the store is 100% `boards/board1.txt`, so the start vector is a
  single constant per mining run), and the result is sparse in the paper's own
  regime: roughly 2 entries changed per quiet move and 3 per capture, so about
  16 to 24 nonzero at half-move 8 to 12, against Go's ~20 stones at move 20.
  Mean-centering each bucket before normalising is the standard alternative
  and would also work, but the XOR is exact, needs no per-bucket statistics,
  and keeps the 0/1 sparse-cosine structure the algorithm assumes.

  Guard: at `h = 0` the difference is the zero vector, so that bucket holds
  exactly one position and K collapses to 1. Normalisation needs an
  explicit zero-norm case, and it must be unit-tested.
- **Bucketing**: one cluster set per half-move `h` (the shared game clock, same
  clock `bookgen --plies`/`book`'s `ply=` cap already use), mirroring the
  paper's per-move-number database split. `h`'s parity is fixed by which color
  the mined agent played in a given source game, exactly matching
  `rankBookGen`'s existing `record` scoping. **Consequence to hold onto when
  sizing K:** even buckets are fed only by White winners and odd buckets only
  by Black winners, so a bucket sees roughly half the won-game counts in the
  table above, not all of them.
- **Left-right symmetry: canonicalise, do not augment.** The rules and the
  standard start are left-right symmetric, which is why `trainEnsemble`
  already mirror-symmetrises weights via `mlv2MirrorIndex`. But that is
  augmentation for fitting a function approximator. A book is a lookup
  structure, and for a lookup the exact-halving version is better: map every
  position to the lexicographically smaller of itself and its mirror before
  clustering, and record the move under the same transform. At play time,
  canonicalise the live position, match, then un-mirror the returned moves
  (`sx -> 7-sx`, `dx -> 7-dx`) if the transform fired. This merges each
  symmetric pair into one cluster exactly instead of doubling the point count
  to approximate the same merge. Reuse `mlv2MirrorIndex` for the index
  mapping. Keep plain augmentation available as the comparison arm, since the
  plan already treats mirror handling as a swept axis.

  **Colour-swap symmetry does not apply and must not be added.** Flipping the
  board vertically and swapping colours maps a half-move-6 position (White to
  move, three moves each) to one with Black to move and three moves each,
  which is not reachable at any half-move: Black to move means `h` is odd,
  which forces unequal move counts. The left-right mirror is the only valid
  symmetry here.
- **Clustering algorithm**: spherical k-means (matches the paper's
  Cluto/cosine choice). Normalize every point to unit L2 norm, assign each
  point to its nearest centroid by dot product (cosine similarity on the unit
  sphere), recompute each centroid as the mean of its assigned points then
  re-normalize, iterate to a fixed-point or an iteration cap. Handle empty
  clusters by reseeding from the point farthest from any existing centroid (a
  standard k-means edge case, needs a unit test). Cap K per bucket at
  `K_eff = min(K, floor(N_bucket / 32))`, so a thin bucket degrades to fewer,
  better-supported clusters instead of producing near-singleton ones whose
  move counts are meaningless. Report `K_eff` per bucket in the file header.
  New, dependency-free, pure, unit-testable code: `src/ml_cluster.h`/`.cpp`
  (parallel to how `ml_tdleaf.h`/`.cpp` isolates a pure core function per the
  playbook's own extension-points template), tested in `tests/test_ml.cpp`:
  K=1 recovers the normalized mean, the fit is deterministic given a seed, it
  converges on a small synthetic separable set, the empty-cluster reseed does
  not crash, zero-norm input is handled, and `K_eff` clamping fires.

## Mining pipeline

**Split the expensive half from the cheap half.** This is the one structural
change to the plan's original shape, and it is an experimental-design point
rather than a code-placement one, because it decides what multiplies the
study's cost. Mining requires **replaying** games: the store keeps summary
rows only, with no move list, in both `ranking/matches.jsonl` and
`ranking/games.tsv`, which is why `rankBookGen` re-runs both agents' full
searches from the start board. Replay is by far the dominant cost and it is
the only part that depends on the game set. Clustering is cheap and depends
only on the recorded triples. So:

- **`rank.exe cbookdump`** (expensive, once per scope): select games, replay
  them, write a compact intermediate of `(h, canonical difference vector,
  move, winner id, winner Elo)` records to `data/cbook_<scope>.jsonl`.
- **`rank.exe cbookfit`** (cheap, no replay): read a dump and write
  `models/cbook<N>.txt` for any combination of cluster count, move cutoff,
  seed, and mirror handling.

This mirrors the `rank.exe extract` -> `train.exe --from-data` split the
project already runs, and it collapses what the original plan costed as "18
mining runs" into **one replay pass per scope**, with the entire
`K x keep x seed x mirror` grid falling out of post-processing. It also leaves
an inspectable artifact, so a bad sweep result can be diagnosed offline
instead of by re-running games.

`cbookdump` args: **exactly one of** `--a <id>` (mine one specific core's own
wins) **or** `--regime <tag>` (mine every agent's wins whose canonical id
resolves to that `rankAgentRegime` tag), **neither given** meaning universal
scope. Plus `--board <path>`, `--max-plies <N>` (the deepest half-move to
record, a ceiling rather than a fixed window, see "ply window is a rung"
below), `--min-elo <E>` with `--ratings <path>` (winner strength gate, see
below), `--sample <N>` and `--seed <N>` (game selection), `--out <path>`.

`cbookfit` args: `--in <dump>`, `--clusters <list>`, `--keep <list>`,
`--mirror canon|augment|off`, `--seed <N>`, `--out-slot <N>`. The list
arguments emit one book file per value from a single read, numbered
sequentially from `--out-slot`, and the summary reports which slot got which
combination.

**The move cutoff is the real knob, not the cluster count.** A whitelist
formed as "the matched cluster's moves intersected with the legal moves" only
does something if the cluster's move list is shorter than the legal move list.
There are 22 legal moves at half-move 0 and high twenties soon after. A
cluster holding hundreds or thousands of positions, each contributing one
move, will cover essentially every legal move, and the filter becomes a no-op
no matter how K is set. So `cbookfit` must truncate: `--keep M` writes only
the top M moves per cluster by count. M is the parameter that actually
controls the compute-for-depth trade the whole mechanism is about, and it
belongs at the front of the Pass-2 grid. `--keep 0` writes the full ranked
list, which is the no-op control.

**Gate on winner strength.** The measured winner-Elo distribution above shows
33% of rated wins coming from agents below 600 Elo and 2.2% from agents at
1000 or better. SMARTSTART's input was professional play. `--min-elo <E>`
against a ratings snapshot is the closest available analogue, and Pass 1b
below carries a gated arm and an ungated arm so the difference is measured
rather than assumed. Apply `rankUpgradeId` before joining ids to the ratings
file, since the raw store carries older id spellings.

**Deduplicate rows before replaying.** Measured: 268,169 rows collapse to
108,939 distinct `(white, black, seed, result)` games. `rankBookGen` replays
every matching row. For an exact-hash book that is merely wasteful, since a
duplicate re-sets the same entry. For a frequency-weighted cluster book it is
a correctness defect: a deterministic pair with 40 stored rows would weight
its single line 40 times over in both the centroid and the move counts.
`cbookdump` deduplicates on `(white, black, seed, result, plies)` first. This
is the project's own defect 3 (`Docs/benchmarking.md`) applied at the mining
stage rather than at the reporting stage.

**Sampling, and reuse of `rankExtract`'s shape.** `rankExtract`
(`src/ranking.cpp`) already solves "replay a subset of the whole store at
scale": a deterministic shuffle so a larger `--sample` is a prefix-superset of
a smaller one, per-game `loadModelSlots` followed by `mlClearSlots`, and a
progress line every 200 games. `cbookdump` should follow it directly.
`ML_SLOTS` is 4096 (`src/ml_eval.h`), well above the 673 distinct ids in the
store, so the slot bound is not a constraint, but per-game load and clear
still avoids holding hundreds of models resident at once.

**Decide the replay-drift policy explicitly and report it.** `rankExtract`
skips any replay whose result disagrees with the stored result, calling it a
determinism drift guard. `rankBookGen` keeps drifted replays as long as A
still won. `models/book2.txt`'s header shows this is not a rare edge case: 12
of 32 replays drifted, on a pair with no stochastic element at all.
`cbookdump` should follow `rankExtract` and skip drifted replays, because
mining a game that did not happen while attributing it to the stored winner is
exactly the instrument problem the project's "validate the instrument before
quoting the reading" rule exists to catch. Report both the kept and drifted
counts in the dump header, the way `bookgen` already does.

**Ply window is a rung, not a mining axis (developer correction, this
session):** clustering runs independently per move-number bucket, so ply 6's
clusters depend only on ply-6 positions recorded from the source games, never
on whether ply 20 was also recorded from those same games. A mine-to-24 run's
ply-6 bucket is therefore identical to a mine-to-8 run's ply-6 bucket -- mining
deeper is a strict superset, exactly like the existing `book` opener's own
`ply=` cap already is (`ai_random.cpp`'s `openerBook`: `arg2 > 0 && halfMove >=
arg2` stops it firing past a half-move cutoff on a book mined deeper; verified
2026-08-03 that a 4-ply mining of one pair produced entries identical to the
first 13 rows of the same pair's deeper 553-entry book). So: dump **once** per
scope, always to the deepest candidate `--max-plies` (32, see the ply rungs
below), and apply the candidate ply-window values as the `cbook` opener's own
runtime `ply=` cap (its `hasArg2`, see "Runtime integration" below) on the
resulting file, not as separate mining runs. Combined with the dump/fit split
above, this leaves the replay pass depending on nothing but the scope.

**Mining-scope decision, resolved by direct comparison, then locked in
(developer instruction, this session):** Theory 33 already characterizes the
"one book per specific agent, self-mined" case for exact-hash books (a core's
own book: +124 Elo; a foreign core's book: -8 Elo) -- that's the existing
`book` opener's scope, well understood, and kept here only as the Pass-1a
plumbing vehicle (the fastest single-slice mine to prove the mechanism works
at all). The genuinely open question is whether a *cluster* book behaves the
same way, since clustering already smooths away individual-agent tactical
specifics the way an exact memorized line doesn't -- so it may transfer more
like the paper's own design (pooled across many different professional
players, not one entity's self-play) rather than like a foreign exact-hash
book. Pass 1b (below) builds both a universal book and one book per regime
with sufficient game volume, and compares them head-to-head per regime.
**Whichever wins that comparison (or a stated hybrid rule) becomes the fixed
scope for the rest of the study** -- scope is not carried forward as a Pass-2
sweep axis alongside ply-window/cluster-count/mirror, to avoid combinatorial
blowup and because the developer asked to decide it once.

For the universal/per-regime modes, mining pools every qualifying agent's wins
against every opponent they beat (not one `--a`/`--b` pair the way `bookgen`
works), including retired agents' historical games. This is a data-mining step
rather than a ranking claim, so the active/retired restriction that governs
standings does not apply here.

Mining reuses `rankBookGen`'s existing move-recovery mechanism
(`diffMoveFromSnap`, replay-with-seed, "winner's own plies only") verbatim,
just feeding each recorded `(h, difference vector, move)` triple into the dump
instead of a hash map, tagged by which scope produced it.

**File format** (`models/cbook<N>.txt`, versioned distinctly from `book<N>.txt`
so the two are never confused): a header block recording the dump it came
from, the scope, the Elo gate, the mirror mode, the cutoff `M`, the seed, and
the counts (games kept, games drifted, positions recorded). Then per ply
bucket a header line (`ply <h> points <N> clusters <K_eff>`), and per cluster
a normalized 128-float centroid line, a cluster size, the mean intra-cluster
cosine, and up to `M` `move <sx> <sy> <dx> <count>` lines, already truncated
at fit time so the runtime needs no threshold logic. The per-bucket and
per-cluster diagnostics are not optional: without them a null Pass-2 result
cannot be told apart from a degenerate clustering, which is the most likely
way this fails quietly.

## Runtime integration

**New globals** (`globals.h`/`.cpp`, same opt-in-toggle pattern as
`g_useTT`/`g_useMoveOrder`): `g_useRootFilter` (bool) and a small fixed-size
`g_rootMoveWhitelist[]`/`g_rootMoveWhitelistCount` (capacity matching
`ML_MAX_MOVES`). `searchRootWhite`/`searchRootBlack` (`ai_minimax.cpp`) check
this in their existing root move enumeration loop only -- not the recursive
`maxAlphaBeta`/`minAlphaBeta` -- skipping any candidate not on the whitelist
when the flag is set. This is a root-only restriction, matching how every
existing opener already only ever decides the current ply, never touches
subtree search.

**Opener contract, no signature change needed**: `OpenerDef::fn` already
returns `false` to mean "don't play, hand off to the brain." The `cbook`
opener first applies its own optional `ply=` cap exactly like `book` does
(`if (arg2 > 0 && halfMove >= arg2) return false;` -- this is the play-time
mechanism "ply window is a rung" above relies on, letting one mined file serve
every candidate ply-window value), then computes the live position's vector,
XORs it against the start vector, canonicalises it under the left-right
mirror, finds the nearest cluster for the current `h`, un-mirrors that
cluster's move list if the canonicalisation flipped the position, intersects
the result with the position's actual legal moves (`generateMoves`), and:
- If the intersection is non-empty, sets `g_useRootFilter = true` +
  `g_rootMoveWhitelist` to that intersection, then returns `false` (defer to
  `agentChooseMove`, which will search only those root moves under its normal
  node/time budget).
- If the intersection is empty (the matched cluster's suggestions are all
  illegal here -- expected occasionally, since fuzzy match is only
  approximate), returns `false` with the flag left off, i.e. an ordinary
  unrestricted move this ply. **This safeguard is required**: without it a
  mismatched cluster could hand the search zero legal root moves.

`src/ranking.cpp`'s `playOneGame` and pairgen's `playoutCapture` (the only two
loops that consult openers at all) reset `g_useRootFilter = false`
unconditionally right after each `agentChooseMove` call, so a restriction never
leaks into a later ply where `cbook` doesn't fire (e.g., past its ply cap).
Console/GUI/`train.exe` tournament never consult openers at all already, so
they're unaffected.

`cbook` draws from no `rand()` (deterministic given its file + the search), so
it must NOT be added to `rankAgentIsDeterministic`'s stochastic-opener list --
same bucket as the existing `book` opener.

**New opener registration** (`ai_random.cpp`/`.h`): `{ "ClusterBook", "cbook",
"...", /*hasArg*/true, /*hasArg2*/true, "cbook", openerClusterBook }`. This
fits the existing generic `.opener(<idName>,<label>=<arg>[,ply=<arg2>])@N`
grammar with **no change** to `RK_OPENER_VERSION` or the ID parser/emitter in
`ranking.cpp` -- confirmed by reading that code: it's fully driven off
`g_openers[]`'s own metadata (`hasArg`/`hasArg2`/`argLabel`), the same reason
`rand` and `book` needed no codec changes either.

## Files touched

| File | Change |
|---|---|
| `src/ml_cluster.h`/`.cpp` (new) | Spherical k-means core, `K_eff` clamping, canonicalisation helper, pure + unit-testable |
| `src/ranking.cpp` | `rankClusterBookDump` (replay + record, mirrors `rankBookGen`'s move recovery and `rankExtract`'s sampling/slot handling), `rankClusterBookFit` (dump to book files, no replay), and a `g_useRootFilter` reset in `playOneGame`/`playoutCapture` |
| `tools/rank_main.cpp` | `cbookdump` and `cbookfit` CLI subcommand wiring |
| `src/ai_random.cpp`/`.h` | `cbook` opener: file loader/cache (mirrors `bookForSlot`'s lazy-load pattern), canonicalise + nearest-cluster match + un-mirror + legality intersection, `g_openers[]` row |
| `src/globals.h`/`.cpp` | `g_useRootFilter`, `g_rootMoveWhitelist[]`, `g_rootMoveWhitelistCount` |
| `src/ai_minimax.cpp` | Root-move-loop whitelist check in `searchRootWhite`/`searchRootBlack` |
| `tests/test_ml.cpp` | Spherical k-means unit tests |
| `tests/test_ranking.cpp` | `cbook` ID round-trip; mining-then-load smoke test |
| `tests/test_ai_integration.cpp` | Root filter actually restricts search (node count drops, chosen move constrained to the whitelist) at two settings |
| `src/CLAUDE.md`, `tools/CLAUDE.md`, `README.md`, `ML.md` (if it documents openers) | Reference-table updates per the standing "after every functional change" workflow |
| `todo.md`, `Docs/theories.md`, `Docs/Memories/`, `plans/` | Mark relevant "Books" items addressed; new theory entry (does fuzzy cluster matching resist Theory 38's collapse); archived plan + companion results doc |

## Three-pass execution (per `Docs/model-training-playbook.md`)

**Ply rungs, fixed for the whole study: 4, 8, 16, 32.** Mine once at
`--max-plies 32` and let the `ply=` cap select. The two short rungs are not
arbitrary: `CHAMPION.md`'s deferred 4-book and 8-book categories are exactly
4-ply and 8-ply self-mined exact books, so `cbook` at `ply=4` and `ply=8` is a
like-for-like reading against a measured baseline on the same head, rather
than a number with nothing to sit beside. Reference points, all from the
**2026-07-29 full-roster fit** on head `ab(deep=6,tt,ord,nodes=200k)@1`, not
comparable to any other fit's absolute scale: `classic(chip=100)@2` bare 921,
with `book13` (4-ply) 941, with `book14` (8-ply) 958. `learned(model=98,...)`
bare 973, with `book15` (4-ply) 972, with `book16` (8-ply) 997. A third core,
`learned(model=3,...)`, went the other way at both rungs, `-59` and `-49`. All
of those lifts sit inside or near this project's 50 to 150 Elo seed-noise
band, so they set the bar for what "works" has to clear, and they show the
sign is not even stable across cores.

The top rung is 32 rather than 24: median game length is 51 half-moves, so 32
covers about 65% of a median game, and under the superset property mining
deeper costs nothing extra because the replay runs the full game regardless.
The paper's own "past move 12" finding does not transfer proportionally, since
12 of a ~200-move Go game would scale to about 3 half-moves here. The
Breakthrough-specific version of the same question is where the difference
vector stops being sparse relative to 128, which puts the interesting range
around half-move 16 to 32.

**Pass 1a (plumbing sanity), concrete scope for this round:**
- One core, `--a` mode: `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2`.
  This is the core behind `book13`/`book14`, so the sanity run doubles as the
  first direct `cbook`-versus-`book` reading at matched rungs on a matched
  head.
- `cbookdump --max-plies 32`, then `cbookfit --clusters 4 --keep 6
  --mirror canon --seed 1`. **`clusters=4`, not 8**: this scope is one pair's
  own wins, on the order of tens of kept replays, so a ply bucket holds tens
  of points. At K=8 the per-cluster floor of 32 points would clamp `K_eff`
  down anyway, and K=4 states the intent honestly.
- Sanity-test at `ply=4` and `ply=8`, the two rungs with existing baselines.
- Build everything above, then verify each of:
  - the dump and fit complete and produce a valid `cbook1.txt` carrying its
    per-bucket diagnostics,
  - `rank.exe check` accepts a roster line with `.opener(cbook,cbook=1,ply=8)@1`,
  - a handful of real games play to completion with no crash,
  - the root-filter knob measurably changes search behavior at two settings
    (node count, chosen move), per the new `test_ai_integration.cpp` test,
  - the `ply=` cap measurably changes behavior at two settings, since that is
    the mechanism the whole ply ladder leans on.
- **Two measurements that are the actual point of Pass 1a**, both cheap and
  both needing no Elo:
  1. **Hit-rate curve versus ply.** Against a `.opener(rand,moves=8)`
     opponent, over the same set of games, record per half-move: the
     exact-hash book's hit rate (`book13`/`book14`), `cbook`'s match rate, and
     `cbook`'s mean legality-intersection size. Theory 38 says the exact
     book's curve falls off a cliff once the opponent deviates. The whole
     hypothesis is that `cbook`'s does not. This curve tests the mechanism
     directly and it is the cleanest evidence the build will produce, so it
     should exist before a single Elo game is played.
  2. **Offline k-means seed stability.** Fit the same bucket at two seeds and
     report the assignment agreement rate. This decides whether the seed axis
     is measuring anything at all, and it costs nothing. Seeds stay in the
     Pass-2 grid regardless, per the playbook's minimum, but knowing the
     answer changes how the resulting spread is read.
- Report back before Pass 1b, per the playbook's interactivity requirement.

**Pass 1b (mining-scope decision), before any cluster/cutoff/ply sweep.**
Four arms, not two, held at Pass 1a's `--keep 6 --mirror canon --seed 1` and
screened at a fixed `ply=16` so this is a scope-only comparison:

1. **Universal, ungated.** Every winner in the store.
2. **Universal, gated at `--min-elo 900`.** The professional-play analogue.
   Roughly 9,600 matched won games, against 43,700 ungated.
3. **Per-regime**, for regimes clearing the volume bar below.
4. **Label-permuted control.** Take the winning arm's book and randomly
   permute the move lists across clusters within each ply bucket. Same file
   format, same code path, same whitelist sizes, but the position-to-move
   association is destroyed. Without this the study cannot tell "SMARTSTART
   works here" apart from "searching 6 root moves at a 200k-node budget beats
   searching 22", which is a completely different and much less interesting
   result. This control needs no new engine code, only a small file shuffler.

**Volume bar for a dedicated per-regime book: 2,000 distinct won games.**
Derivation: a ply bucket is fed by one colour only, so it receives about half
the regime's won games. At K=16 with the 32-points-per-cluster floor a bucket
needs >= 512 points, so >= 1,024 won games of that parity, so >= ~2,000 in
total. Applying that to the measured table: **qualifying** are `classic`
(29,163), `value` (15,006), `pool_games` (14,133), `position_elo` (5,769),
`exp` (3,562), `dist` (3,546), and `adv` (2,697). **Not qualifying** are
`weight_merge` (1,656), `tdleaf_self` (1,555), `nonlearning` (1,035), and
`gumbel_self` (0 games in the loaded store). `learned?` (30,817) feeds the
universal arms but is not a regime and gets no dedicated book. So the
per-regime arm is seven books, and the two newest regimes are structurally
excluded from it, which is itself a result worth reporting rather than a
scheduling detail.

Screen a sample of agents from each qualifying regime wearing that regime's
own book versus the universal ones, via `rate --pin ranking/standings.tsv`
against `ranking/roster_screening_pool.txt` (36 opponents spanning the
standings). Then **lock in a scope rule** (uniformly universal, uniformly
per-regime, or an explicit hybrid such as "per-regime above the volume bar,
universal fallback below") before Pass 2. Report back first, same
interactivity requirement.

**Pass 2 (broad sweep), provisional grid, re-confirmed live with the developer
before it runs per the playbook's "design the grid, then stop and show it"
rule.** Scope is fixed from Pass 1b. Nothing in this grid triggers a replay:
the entire table is post-processing over Pass 1b's dumps.

| Axis | Candidate values | Why these values | Replay? |
|---|---|---|---|
| Move cutoff (`--keep M`) | 3, 6, 12, and 0 (full list, no-op control) | The primary axis. Against 22 legal moves at half-move 0 and high twenties after, these are roughly 1/7, 1/4, and 1/2 of the branching factor. M is what actually sets the compute-for-depth trade, and M=0 isolates "did the clustering help" from "did narrowing the root help". | No |
| Clusters per ply (`--clusters K`) | 16, 48, 128 | Sized from measured density, not from the paper. A universal ungated bucket holds roughly 49,000 points (about 54,000 won games per colour, ~90% of which reach half-move 24), so K=128 still leaves ~380 points per cluster. A gated-at-900 bucket holds roughly 4,300, so K=128 lands at ~34 per cluster, right on the floor. **The gate and K interact**: if Pass 1b picks the gated arm, cap the ladder at 48. The plan's original 4/8/16 was sized for a single agent's own wins and is two orders of magnitude too coarse for a pooled book. | No |
| Mirror handling (`--mirror`) | canon, augment, off | `canon` halves the space exactly, `augment` doubles the points to approximate the same merge, `off` is the control. Left-right is the only valid symmetry (colour-swap is unreachable, see above). | No |
| Seeds (k-means init) | 3, per the playbook minimum | Measures how much of a configuration's Elo is the configuration rather than the k-means starting point. Pass 1a's offline stability check says in advance whether this axis is live. | No |
| Ply cap (`ply=`) | 16 during the sweep, fixed | Held constant so the sweep is not confounded, and set to 16 rather than 8 because 16 is past the range the exact-hash book has already been shown to work in, which is where fuzzy matching is supposed to earn its keep. The full 4/8/16/32 ladder runs in Pass 3 on the winners. | No |

That is 4 cutoffs x 3 cluster counts x 3 mirror modes x 3 seeds = 108 books,
all produced by post-processing, plus the bare core and the label-permuted
control as reference agents. Screen each via `rate --pin` against
`roster_screening_pool.txt`. The screening side is the only real cost and it
is what the pre-launch grid conversation is about.

**Pass 3 (optimize):** run the full 4/8/16/32 ply ladder on the top two or
three Pass-2 configurations, and re-measure them under opening diversification
(`--paired-openings`) against the matched exact-hash book at the same rung.
That diversification comparison is the actual Theory-38 test and it belongs
here, on configurations already known to be worth the games, rather than
sprinkled across the sweep. A search harness like `hill_climb.ps1` is very
likely overkill for a small discrete grid, but confirm that call once Pass 2's
shape is known.

**Certification:** full unpinned refit with the chosen configuration(s) added
at >= 32 games/pair vs. contenders, per `CHAMPION.md`'s methodology. Note
explicitly: since the book divisions are currently *deferred*, a strong result
here doesn't by itself reinstate a book category title -- that's a separate
`CHAMPION.md`-restructuring conversation with the developer once real numbers
exist, not decided by this plan.

## Deferred (not this build)

**Clustering agents by opening-win behavior, then mining a position-cluster
book per agent-cluster.** A genuine extension beyond SMARTSTART itself (the
paper only clusters positions, over one undifferentiated pool of human
experts) -- a two-level system needing an agent-similarity metric and an
agent-level clustering pass before position clustering even starts. Logged
here so it survives into the eventual results doc's "Ideas This Inspired"
section rather than being lost; not in scope until the core position-cluster
mechanism above is validated.

Three more surfaced while grounding the numbers, same status, logged for the
results doc rather than built:

- **Mine the screening stores.** `gumbel_self` has zero games in the loaded
  match store because its games live in `ranking/matches_screen_gz*.jsonl`,
  which `matches.index.txt` deliberately does not load. That exclusion is a
  ranking-hygiene decision, and mining is not a ranking claim, so a later
  round could point `cbookdump` at those stores and give the newest regimes a
  book without touching the ladder.
- **Soft matching instead of hard filtering.** Filter mode is a hard
  restriction on the root move list. The paper's other mode biases rather than
  filters, and alpha-beta has a natural analogue that the plan currently has
  no use for: feed the cluster's move counts into the existing move-ordering
  path (`ord`) as a first-move hint instead of pruning. That is strictly safer
  than filtering, since nothing is ever excluded, and it would separate "the
  cluster knows which move is good" from "restricting the root helps".
- **Cluster-count selection per bucket rather than globally.** `K_eff`
  currently clamps a single global K by density. A bucket-by-bucket criterion
  (silhouette, or simply the smallest K whose mean intra-cluster cosine clears
  a threshold) would let early buckets stay coarse and late buckets go fine,
  which matches how the data actually thins out.

## Verification

1. `.\tools\run_tests.ps1 -Build` passes, including the three new/extended
   test files above.
2. `rank.exe cbookdump --a "ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2"
   --board boards/board1.txt --max-plies 32 --out data/cbook_classic.jsonl`
   runs to completion and reports games kept, games drifted, duplicate rows
   collapsed, and positions recorded.
3. `rank.exe cbookfit --in data/cbook_classic.jsonl --clusters 4 --keep 6
   --mirror canon --seed 1 --out-slot 1` writes `models/cbook1.txt` with
   per-bucket point counts, `K_eff`, cluster sizes, and mean intra-cluster
   cosine in the header.
4. `rank.exe check` validates a roster line using
   `...opener(cbook,cbook=1,ply=8)@1`.
5. A short `rank.exe play` batch (a handful of games) between that agent and a
   diversified-opening opponent completes with no crash, and its console
   output/telemetry shows the root filter engaging on at least some plies.
6. The two Pass-1a measurements are shown to the developer before Pass 1b: the
   hit-rate-versus-ply curve for `cbook` against `book13`/`book14` on the same
   games, and the offline k-means seed-agreement rate.
7. The two-settings instrument checks (root filter on/off, `ply=` cap low/high)
   are shown alongside them, per the standing "validate the instrument before
   quoting the reading" rule.
8. Duplicate-row collapse is reported as a ratio and sanity-checked against the
   measured store-wide figure of 0.41 distinct games per row, since a
   `cbookdump` that silently skipped deduplication would still run and would
   still produce a plausible-looking book.
