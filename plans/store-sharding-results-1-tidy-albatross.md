# Match-store sharding and grouping - results

Session date: 2026-08-01. No prior plan document: this work started from a
reported symptom ("issues syncing with git") and the design was settled with the
developer mid-session, so this results doc stands alone.

## The problem

`git push` had been failing. It was not connectivity or credentials: `git fetch`
worked, the tree was clean, and `main` was 15 commits ahead of `origin/main` and
0 behind. The blocker was file size. GitHub rejects any single file over 100 MB,
and the 15 unpushed commits contained:

| File | Size at HEAD | Oversized blobs in the 15 commits |
|---|---|---|
| `ranking/matches.jsonl` | 270.6 MB | 5 (270, 114, 114, 93, 91 MB) |
| `ranking/games.tsv` | 179.2 MB | 1 |
| `ranking/report.md` | 10.5 MB | - |

`origin/main` was clean (67.9 MB and 44.2 MB there), so every oversized object
lived only in unpushed commits. No force-push over shared history was needed.

## What was measured before deciding anything

1. **Which files are actually irreplaceable.** `src/ranking.cpp:1890-1893`
   documents that `games.tsv`, `report.md`, `ratings.tsv` and `standings.tsv`
   are rating OUTPUTS derived from the store. Only `matches.jsonl` is the
   never-recomputed asset.
2. **How expensive re-deriving them is.** A full `rank.exe rate` over the
   649,434-game store took **19.4 s**. Re-running it left `ratings.tsv`,
   `standings.tsv` and `games.tsv` byte-identical, with `report.md` differing
   only in its generation-timestamp line, confirming the fit is deterministic.
   On that basis the four outputs were gitignored.

## What was built

Three pieces in `src/ranking.cpp` / `src/ranking.h`, exposed through
`tools/rank_main.cpp`:

- **A part index.** `rankStoreIndexPath` maps `ranking/matches.jsonl` to
  `ranking/matches.index.txt`; `rankStoreParts` resolves the load order from it,
  falling back to probing a contiguous `matches.NNNN.jsonl` chain when no index
  exists. The live tail is always loaded last, so writers are unchanged: `play`
  and the `run_rank.ps1` shard merge still append to `matches.jsonl`. A listed
  part whose file is missing is skipped rather than being an error, which is
  what lets untracked parts be absent on a fresh clone.
- **`rank.exe seal --max-mb N`.** Rolls an oversized tail into sealed
  `matches.NNNN.jsonl` shards. Sealed shards are immutable, so each is stored by
  git exactly once and only the small tail changes between commits.
- **`rank.exe split --group <substr> --max-mb N [--apply]`.** Regroups every row
  by WHO played it into `roster` (both agents in the roster), `retired_<substr>`
  and `retired_other` parts, capping each part and writing the index. Dry run by
  default.

## Results

Split of the real store (`--max-mb 90`, `--group tdleaf_self`):

| Bucket | Rows | MB | Parts | Committed? |
|---|---|---|---|---|
| `roster` | 132,769 | 54.8 | 1 | yes |
| `retired_tdleaf_self` | 457,611 | 193.2 | 3 | no (gitignored) |
| `retired_other` | 59,054 | 22.6 | 1 | yes |

`retired_other` is tracked at the developer's direction, so re-rostering one of
those agents later needs no out-of-band file transfer. Only the TD-Leaf Pass-2
screening cohort (25 candidates tried, 16 promoted) is left untracked.

696 agents present in the store are no longer in the 175-agent roster, and they
account for **80% of the bytes**. The roster-only games fit in one file, so no
further splitting is needed.

**Information preservation, verified three ways:**

- After the split, `rank.exe rate` produced `ratings.tsv` and `standings.tsv`
  **byte-identical** to the pre-split committed versions (649,434 games, 871
  rated agents both times).
- `games.tsv` changed, but a sorted multiset comparison of all 649,435 lines
  showed it is a **pure reordering** - no row gained or lost. The split changes
  row order because rows are grouped, and the Bradley-Terry fit is order
  independent.
- The full suite passes (2,985 assertions, 121 cases), including new tests for
  shard naming, sealed-shard load order, chain gaps, seal round-tripping, index
  behaviour, and split bucketing.

**Git history.** The 15 unpushed commits were rewritten with `git filter-branch
--index-filter` over `origin/main..main` only, dropping the five store/output
paths. All 15 commits and their messages survive. Largest blob in the unpushed
range afterwards: **0.19 MB** (`src/ranking.cpp`), down from 270.6 MB. A local
`backup-pre-split` branch holds the pre-rewrite tip.

## The cost of leaving retired parts untracked

This is the number that most qualifies the work. Two clone scenarios were rated
against the full store, both on the same 170 active agents:

| Measure | Full store | Roster only | Roster + `retired_other` (shipped) |
|---|---|---|---|
| Store size | 271 MB | 55 MB | 78 MB |
| Spearman rho of rank order | - | 0.9555 | **0.9526** |
| Agents whose rank moves | - | 156 of 170 | **153 of 170** |
| Largest rank shift | - | 53 places | **54 places** |
| Mean error bar (`pm`) | 7.2 | 11 | **10.2** |
| Games behind the d6 head's top agents | 6,648 | 2,208 | 2,728 |

**Tracking `retired_other` barely helps.** Adding its 59,054 rows moves rho from
0.9555 to 0.9526, which is no improvement at all at this precision. The TD-Leaf
games are 89% of the retired rows and carry nearly all the pairwise
connectivity, so the reproducibility gap is theirs almost entirely. Keeping
`retired_other` tracked is worth doing for the option value of re-rostering
those agents cheaply, not because it restores the ranking.

A note on why this surprises people, recorded because it came up directly: a
retired agent's games look discardable because the agent is absent from
`standings.tsv`. But Bradley-Terry fits every rating **jointly**. A game between
rostered X and retired R is evidence about X, so deleting it degrades X's
rating, not just R's. That is also why `ratings.tsv` keeps `gone` rows at all:
they are in the fit, merely filtered out of the standings view.

Two structural checks that rule out the easy dismissals:

- **The cohort is not a self-contained bubble.** Of the 457,611 TD-Leaf rows,
  **409,283 (89.4%) have exactly one rostered agent** and only 48,328 are
  cohort-internal. (Zero rows have both agents rostered, which independently
  confirms the split is correct.) So they are overwhelmingly direct evidence
  about roster members.
- **They are not duplicate replays.** 438,008 distinct trajectories out of
  457,611 rows, a ratio of **0.957**. The store-wide median is 0.438, so unlike
  most of this store these are genuinely independent games and no dedup would
  have recovered the space.

## Outcome: the games were dropped, and the ranking was re-certified

The developer's decision was to **drop them from the fit permanently** rather
than commit them, on the methodological ground that a permanent ladder should
not be dominated by games against transient screening candidates. The three
files' lines were removed from `ranking/matches.index.txt`, and on 2026-08-02
the files themselves were deleted to reclaim the 193 MB. They were never
committed, so **the excluded population no longer exists anywhere** and none of
the with-cohort measurements in this document can be re-derived. They stand as
a record of a fit that cannot be reproduced.

That made this a certification event. The re-certification is in
`ranking/CHAMPION.md`; the substance:

- **Four of five categories re-confirmed the same holder.** 4-book, 8-book,
  4-random and 8-random are unchanged.
- **The openless title changed hands**, from
  `learned(s76,ef183148,position_elo,...)` to
  `learned(s169,4975683c,tdleaf_self,...)`.
- The new leader initially sat at a **median 8.0 games/pair with all 169 pairs
  under 32** -- the exact fill level rule 2 says has inverted this table three
  times -- so `ranking/roster_top.txt` was rewritten to the openless top 9 plus
  the bare chip counter and played out (816 games). `rank.exe check --games 32`
  then reported **0 pending across all 55 contender pairs**, and s169 held:
  1050 +/- 12 at 8/pair became **1044 +/- 11 at 32/pair**, over s76's 1007 +/- 8.
- **The 32 is 32 stored rows, not 32 distinct games.** The boost games came back
  at **0.706 distinct trajectories per row** (816 rows, 576 distinct), so the
  real fill is ~22.6 distinct games/pair and printed SEs are understated by
  ~1.19x. Corrected, the openless gap is 37 Elo against a combined SE of ~16.1:
  **2.3 SE, not 2.7**. Recorded because quoting the raw table would overstate it.
  These contenders carry no dilution and no opener, so the variation comes from
  the `tt` head's cross-game state differing across the 12 shard processes.

**What is NOT explained, and is flagged in `todo.md` as `[Now]`:** s169 took the
title while *losing* 36% of its games (it is a TD-Leaf agent, so the removed
cohort was its own family), and the bare chip counter fell from openless rank 2
to rank 35 under the same change. Bradley-Terry accounts for opponent strength,
so "it farmed weak candidates" is not an explanation and should not be repeated
as one. The boost run tested whether the result survived proper fill, and it
did, but surviving is not the same as being understood.

Concrete inversions: the agent ranked 4th on the full store
(`ab(d6,tt,ord,asp50,nb200k)@1.classic(t1,c4,w0,l0)@2`) falls to 43rd; 7th falls
to 51st; 9th falls to 59th. Meanwhile the TD-Leaf agent
`ab(d6,tt,ord,nb200k)@1.learned(s169,4975683c,tdleaf_self,lin,129-1,con100)@1`
rises from 11th to 4th.

The mechanism is not subtle: retired agents supplied most of the pairwise
connectivity, so removing them both widens error bars and reorders the table.
Per the project's own rule that Elo is never comparable across fits, the clone
table is not a degraded version of the full table, it is a different instrument.

**This means the certified ranking is not currently reproducible from the repo
alone.** That is in tension with `ranking/CHAMPION.md` rule 1 (certification is
the full-roster anchored refit). It is recorded as a `[Now]` item in `todo.md`.
The retired parts are all under 100 MB, so committing them is now possible and
the original blocker no longer argues against it.

## Implementation details and gotchas

- **Id canonicalization is load-bearing in the split.** Stored ids must go
  through `canonicalizeLearnedIds` before the roster is consulted, exactly as
  `rankLoadMatches` does. Without it every legacy-form `learned(sN,hash8)` row
  reads as non-rostered and live agents' games get filed under "retired". This
  is the same gap that broke scheduler dedup twice (commits 05b2fa0, 10c5f82).
- **Verify before destroying.** Both `seal` and `split` count the rows in the
  rewritten parts and compare against the original before deleting anything,
  aborting with the store untouched on a mismatch. The store is never
  regenerated, so a silent short write would be unrecoverable.
- **Unparseable rows are kept**, filed with the roster part, rather than being
  dropped as unclassifiable.
- **`seal` must be index-aware.** When an index exists it is the authority on
  what loads, so newly sealed shards are appended to it. Without that, sealing
  after a split would move rows into files nothing ever reads. This was caught
  in review, not by a test failure.
- **`filter-branch` needs a clean tree**, and its post-rewrite checkout deletes
  files no longer in HEAD. The working store tail vanished during the rewrite,
  which is harmless only because every row already lived in the parts.
- **Two test bugs, both mine:** the tests originally used `.tmp` store basenames,
  but the stem logic strips `.jsonl`, so part names and index paths did not line
  up. Fixed by using realistic `.jsonl` basenames, which also makes the tests
  exercise production naming.

## Future Work

- **Explain the two unexplained swings** (also `todo.md` `[Now]`): why removing
  the cohort games *promoted* a cohort member (s169) and *demoted* the bare chip
  counter by 33 places. Tests that would settle it: refit with the cohort games
  removed from classic@2's record only; measure classic@2's head-to-head record
  against the 9 dropped candidates to test a non-transitivity story; and
  separate connectivity from volume by topping the surviving pairs back up to
  their old game counts. Until one of these lands, any mechanism story about the
  openless title is a guess.
- **Confirm the rating gap is caused by lost connectivity, not lost volume.**
  The 0.9526 rho conflates two things: fewer games per pair, and fewer pairs.
  Re-rating after topping every surviving pair back up to its old game count
  would separate them. Matters because if the gap is mostly volume it closes on
  its own as games accumulate; if it is connectivity, it does not.
- **The other four categories were re-confirmed at unboosted fill** (1392 games,
  pairs well under 32), so "held" means only that the same agent still leads.
  4-random and 8-random remain statistically tied at the top. A boost run per
  category would be needed to say more.
- **Other study scripts still write to the permanent store.** Only
  `tdleaf_study.ps1` got `-ScreenStore`. `sweep_pst_v2.ps1`,
  `train_vs_champion.ps1` and `hill_climb.ps1 -Promote` should be audited for
  the same flooding problem before the next study runs.
- **The 19.4 s refit was measured once, on a warm cache, in one process.** It is
  quoted in `.gitignore` and `tools/CLAUDE.md` as the justification for not
  committing the rating outputs. It has not been measured cold or across
  several runs, so treat it as an order of magnitude, not a benchmark.
- **`split` has only been run once on real data.** Its resume/rerun behaviour
  (splitting an already-split store, then splitting again after new games land
  in the tail) is covered by unit tests but not by a real-store trial.
- **No test covers `seal` appending to an existing index.** The code path was
  added deliberately and reasoned through, but the test suite only covers seal
  without an index and split with one.

## Ideas This Inspired

- The index file makes "which games count" a first-class, hand-editable input.
  That is a rating *experiment* primitive, not just a storage detail: an index
  listing only pre-2026-07 parts would answer "what did the table look like
  before the TD-Leaf cohort landed" without touching any data.
- Grouping by agent class suggests grouping by other axes too - by board, by
  run id, by search head - which would let a study rate its own slice cheaply
  instead of loading 649k rows to use 2k of them.
- The store loads in ~20 s largely because every row is parsed to fit. A binary
  or column-oriented part format for sealed (immutable) parts would not change
  any result, since sealed parts never change.
- Retired agents cost 80% of the bytes for agents nobody rates. A retention
  policy expressed in the roster file itself ("keep the last N screening
  cohorts") would make this maintenance automatic rather than a manual decision.

## How to test

```powershell
.\tools\run_tests.ps1 -Build      # 2,985 assertions, 121 cases
.\rank.exe split                  # dry run: bucket sizes, no writes
.\rank.exe rate                   # ~20 s; ratings.tsv/standings.tsv are gitignored now
```

Expect `rank.exe rate` to reproduce the same table on this machine (all parts
present). On a fresh clone expect a different table, per the section above.

## Commit message used

```
Split the match store into indexed parts to fit a git host

The store had reached 270 MB, over GitHub's 100 MB per-file limit, which
blocked every push. It is now a set of parts plus a live tail, listed in
ranking/matches.index.txt, grouped by who played each game.

Games between rostered agents (55 MB) stay committed. Games touching
agents that were screened and never promoted (216 MB, 80% of the store)
are kept on disk but untracked. Rating outputs are untracked too: a
refit rebuilds them in 19.4 s and is deterministic.

Verified information-preserving: after the split the refit returned
byte-identical ratings.tsv and standings.tsv, and games.tsv is a
confirmed pure reordering of the same 649,435 rows.
```

---

# Addendum: regimes, matchups, and why pooled Elo depends on the pool

Added the same session, after the developer challenged the methodology rather
than the storage question. Their hypothesis, quoted because it turned out to be
the right frame: the TD-Leaf self-play agents "were often initialized by the
learned champion and then learned to beat itself from self play, which really
just means it learned to beat the previous champion. But that means that it
didn't learn how to beat the chip counter."

## The hypothesis was right in structure, wrong in direction

Measured over the dropped cohort's 457,611 rows, win rate against rostered
agents by regime:

| Rostered opponent | Games | Cohort win rate | Implied Elo gap |
|---|---|---|---|
| `learned-other` | 166,813 | **70.5%** | ~151 |
| `chip-counter` | 189,910 | **63.6%** | ~97 |
| `heuristic-other` | 47,040 | 58.4% | ~59 |

The chip counter did not have a *good* matchup: it lost 63.6%. It had a
**comparatively** good one, losing ~54 Elo less badly than the other learned
agents. The stated mechanism holds -- the cohort punishes agents resembling its
ancestor hardest -- but the raw direction needed correcting, and the corrected
version is what makes the effect possible at all.

## Where Bradley-Terry actually breaks

`rank.exe matchup` (built this session) reports, per regime pair with both
colours combined, the actual score rate, the Elo-expected score rate, and the
residual between them. The residual is the model misfit: BT assigns one strength
per agent, so it cannot express "A is strong but loses to B", and the residual
is exactly how much that costs.

| Population | `classic` vs `tdleaf_self` | `classic` vs `pool_games` |
|---|---|---|
| Cohort included (196,767 games in the left cell) | +0.2 | **-6.0** |
| Cohort dropped (6,857 games) | +0.8 | **-2.7** |

The cohort was ~30% of the store, so the fit spent the chip counter's single
parameter on explaining those games -- fitting them almost exactly (+0.2) --
and paid for it by over-predicting the chip counter against `pool_games` by 6
points. Dropping the cohort **more than halved the worst large-sample
miscalibration**. So the developer's methodology decision improved calibration,
which is a stronger justification than the storage argument it was made on.

## Roster composition is part of the measurement

Active roster by regime, 2026-08-01 (170 agents): `classic` 30.6%,
`pool_games` 30.0%, `position_elo` 17.6%, `tdleaf_self` 9.4%, `adv` 3.5%,
`exp` 3.5%, `nonlearning` 2.9%, `weight_merge` 2.4%.

61% of the pool is two regimes, so "strong" and "good against classic and
pool_games" are nearly the same measurement. `rate --regime-balanced` weights
each pair by `1/(agents in A's regime * agents in B's regime)`, rescaled to the
original total. Effect at head `ab(d6,tt,ord,nb200k)@1`, openless:

| Rank | Canonical fit | Regime-balanced fit |
|---|---|---|
| 1 | s169 tdleaf_self, 1044 +/- 11 | s602 tdleaf_self, 1259 +/- 8 |
| 2 | s76 position_elo, 1007 +/- 8 | s169 tdleaf_self, 1249 +/- 8 |
| 3 | s602 tdleaf_self, 996 +/- 10 | s76 position_elo, 1240 +/- 8 |
| 4 | s349 tdleaf_self, 990 +/- 10 | s349 tdleaf_self, 1239 +/- 8 |
| 5 | s6 pool_games, 987 +/- 8 | **classic(t1,c4,w0,l0)@2, 1213 +/- 14** |

(Absolute Elo is not comparable across the two fits; read the order.) The chip
counter returns from openless rank 35 to rank 5, and the top four collapse into
a 20-Elo, 4-way statistical tie. Not promoted to canonical: doing so re-certifies
every champion, so it writes `ranking/*_balanced.*` and the decision is logged
in `todo.md`.

## Future Work (addendum)

- **s169's rise is still unexplained.** The chip counter's fall now has a
  measured mechanism; the cohort member gaining the title while losing 36% of
  its games does not.
- **Should the anchor be exempt from balancing?** `rand@1` is regime
  `nonlearning` with 5 agents, so balancing up-weights games against random
  play, which is the least informative comparison in the pool.
- **SEs after reweighting are effective-sample SEs**, not game counts. They are
  rescaled to the original total so the magnitudes look familiar, which is
  convenient and slightly dishonest; a proper treatment would report effective
  sample size alongside.
- **The behavioural-clustering idea was deferred, not rejected.** Regimes are a
  provenance label, and provenance is ambiguous exactly where the developer
  noticed (a TD-Leaf agent initialised from another agent's weights). Clustering
  agents on move agreement over a fixed `posgen` position pool would group by how
  they PLAY, which is what determines a matchup, and would answer the
  categorisation question empirically instead of by convention.
- **Multidimensional ratings.** Nash averaging and multidimensional Elo (mElo)
  model cyclic structure explicitly rather than reporting it as residual. Worth
  reading before extending this further; verify the references before they go
  into `Docs/works-cited.md`.
