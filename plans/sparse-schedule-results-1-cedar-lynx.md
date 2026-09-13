# Results: rating agents on a sparse opponent graph (Round 4 subsample)

Companion to `plans/sparse-schedule-plan-1-cedar-lynx.md`. Run 2026-09-13 on
Round 4's rung-16 store, no new games. Raw tables:
`plans/sparse-schedule-results-1-cedar-lynx/`.

## Summary

- At equal games per agent, spreading an agent's games over many opponents
  rated it more precisely than concentrating them on a few. That held up to
  128 opponents, the most a disjoint pair of draws allows. With 128 random
  opponents at 8 games each (1,096 games per agent) the disjoint-draw error
  was 17.2 Elo. Every opponent at 2 games each (904 games per agent) differed
  from the full fit by 18.4 Elo RMS. This matches Shah et al. 2016 on real
  game data, with the caveats below.
- The first run could not show this, because `rank.exe`'s fit adds 0.5
  virtual games at 50% to every played pair. At 2 games per pair that is 20%
  of the pair's data, and it pulled the dil20 cells' mean up 60 to 70 Elo.
  `rank.exe rate --prior X` was added to hold the prior fixed, and the test
  was rerun at priors 0.1 and 0.01.
- No sparse design below the full data reproduced the 6 cell champions. The
  champions are separated by 2 to 14 Elo, less than any design's error. Title
  claims still need a dense contender block.
- Stratifying opponents by Elo band made no consistent difference: -7.5 to
  +11.2 Elo against random opponents, with both signs at every k.

## Setup

- **Data:** the 198 category agents of `ranking/q8/cohort_round4b.txt`, every
  row involving one of them (1,016,613 rows over 70,007 pairs), plus 792,396
  other rows kept whole in every fit. Each cohort agent has 449 to 568
  opponents in the store. The count exceeds the 425 other active agents
  because rungs 2 to 8 included the 4 cores benched after rung 1.
- **Designs:** opponents per agent k in {8, 16, 32, 64, 128, all} crossed with
  games per pair g in {2, 4, 8, 16}, opponents random or stratified by rung-8
  Elo quartile (`ranking/rungs/standings_r8.tsv`). Each cohort agent gets at
  least k opponents. Draw B shares no pair with draw A, which is possible up
  to k = 128. A pair keeps its first g rows in store order.
- **Fit:** `rank.exe rate --pin ranking/rungs/pin_source_standings_20260906.tsv`,
  the fit Round 4's convergence test uses, at priors 0.5 (the default), 0.1
  and 0.01. Each design is compared with the full rung-16 fit at the same
  prior.
- **Metrics:** RMS Elo difference from the full fit over the 198 agents, and
  the disjoint-draw RMS divided by sqrt(2). The second is each design's own
  error with no data shared. The first shares data with the reference, so it
  understates error except where the design is biased.
- **Instrument checks:** the all-opponents, 16-games design reproduces the
  reference exactly (RMS 0.0, rho 1). `rank.exe rate` at the default prior
  reproduces `ranking/rungs/cores_r16.tsv` and `standings_r16.tsv` byte for
  byte after the `--prior` change.

## Levels: disjoint-draw error per design (Elo), random opponents

Prior 0.01, the setting closest to the plain maximum-likelihood fit that
Shah et al. analyse:

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 124.9 | 88.0 | 65.2 | 56.2 |
| 16 | 83.1 | 61.5 | 46.3 | 40.9 |
| 32 | 56.5 | 46.4 | 35.8 | 30.9 |
| 64 | 38.9 | 28.3 | 21.5 | 17.3 |
| 128 | 27.3 | 21.5 | 17.2 | 14.1 |

Games per agent behind each cell:

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 18 | 36 | 71 | 137 |
| 16 | 37 | 73 | 144 | 280 |
| 32 | 74 | 144 | 284 | 554 |
| 64 | 146 | 286 | 565 | 1,098 |
| 128 | 285 | 555 | 1,096 | 2,128 |
| all | 904 | 1,756 | 3,458 | 6,673 |

Prior 0.1:

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 106.7 | 83.8 | 63.6 | 55.5 |
| 16 | 73.6 | 57.6 | 44.7 | 40.1 |
| 32 | 51.3 | 43.9 | 34.7 | 30.4 |
| 64 | 35.6 | 27.0 | 20.9 | 17.0 |
| 128 | 24.9 | 20.4 | 16.7 | 13.9 |

Prior 0.5 (the default). Smaller at few games per pair because the prior
pulls both draws toward the same opponents' means, which shrinks their
disagreement without making either correct:

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 78.8 | 70.9 | 57.7 | 52.8 |
| 16 | 53.8 | 47.1 | 39.6 | 37.3 |
| 32 | 38.5 | 36.6 | 30.9 | 28.3 |
| 64 | 27.2 | 22.9 | 19.0 | 16.1 |
| 128 | 18.8 | 17.1 | 15.0 | 13.1 |

## Levels: RMS difference from the full fit (Elo), random opponents

| opponents \ games per pair | prior 0.5: 2 | 4 | 8 | 16 | prior 0.01: 2 | 4 | 8 | 16 |
|---|---|---|---|---|---|---|---|---|
| 8 | 95.7 | 79.1 | 63.9 | 54.1 | 137.2 | 93.5 | 71.6 | 58.1 |
| 16 | 72.0 | 55.4 | 44.7 | 41.0 | 85.3 | 62.7 | 50.2 | 44.4 |
| 32 | 65.3 | 45.2 | 31.1 | 25.7 | 56.1 | 43.8 | 33.4 | 28.0 |
| 64 | 58.2 | 36.1 | 21.8 | 16.1 | 40.2 | 29.4 | 21.3 | 17.2 |
| 128 | 54.6 | 33.0 | 18.0 | 11.3 | 29.8 | 21.3 | 15.5 | 12.1 |
| all | 52.8 | 28.8 | 11.7 | 0.0 | 18.4 | 11.1 | 5.8 | 0.0 |

Mean shift of the cohort from the full fit (design minus full):

| opponents \ games per pair | prior 0.5: 2 | 4 | 8 | 16 | prior 0.01: 2 | 4 | 8 | 16 |
|---|---|---|---|---|---|---|---|---|
| 8 | 17.9 | 16.0 | 14.1 | 3.4 | -9.7 | 6.9 | 10.8 | 2.3 |
| 16 | 19.9 | 17.1 | 9.8 | 7.9 | -1.9 | 8.1 | 5.4 | 6.6 |
| 32 | 19.3 | 11.9 | 6.4 | 0.9 | 0.2 | 2.5 | 2.3 | -0.1 |
| 64 | 20.1 | 12.2 | 5.9 | 1.0 | 0.7 | 2.7 | 1.5 | 0.2 |
| 128 | 22.6 | 14.3 | 6.8 | 1.0 | 3.5 | 5.2 | 3.0 | 0.6 |
| all | 23.3 | 12.8 | 5.3 | 0.0 | 2.7 | 2.6 | 1.4 | 0.0 |

## Equal budgets side by side (prior 0.01)

| games per agent | design (opponents x games per pair) | disjoint error | RMS vs full | Bradley-Terry SE, 173.7 / sqrt(0.164 G) |
|---|---|---|---|---|
| about 140 | 8 x 16 | 56.2 | 58.1 | 36.3 |
| | 16 x 8 | 46.3 | 50.2 | |
| | 32 x 4 | 46.4 | 43.8 | |
| | 64 x 2 | 38.9 | 40.2 | |
| about 285 | 16 x 16 | 40.9 | 44.4 | 25.6 |
| | 32 x 8 | 35.8 | 33.4 | |
| | 64 x 4 | 28.3 | 29.4 | |
| | 128 x 2 | 27.3 | 29.8 | |
| about 555 | 32 x 16 | 30.9 | 28.0 | 18.2 |
| | 64 x 8 | 21.5 | 21.3 | |
| | 128 x 4 | 21.5 | 21.3 | |
| about 1,100 | 64 x 16 | 17.3 | 17.2 | 12.9 |
| | 128 x 8 | 17.2 | 15.5 | |
| 904 | all x 2 | not available | 18.4 | 14.2 |
| 2,128 | 128 x 16 | 14.1 | 12.1 | 9.3 |

The best design at each budget sits 1.07x to 1.5x above the Bradley-Terry
standard error, and the ratio grows with the budget. `CLAUDE.md` rule 7
measures printed pm as understated by about 1.5x across the store from
replayed games. That is consistent with the upper end but was not measured
per design here.

## Openless agents, RMS vs full (prior 0.01)

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 122.0 | 91.8 | 74.8 | 61.8 |
| 16 | 86.4 | 68.5 | 55.6 | 49.8 |
| 32 | 56.2 | 43.6 | 35.9 | 32.6 |
| 64 | 41.2 | 31.4 | 22.2 | 19.0 |
| 128 | 34.4 | 23.6 | 16.0 | 13.1 |
| all | 22.5 | 13.9 | 6.0 | 0.0 |

Within 0 to 4.6 Elo of the all-division RMS at every k of 32 and up. The
plan's concern that deterministic pairs would starve openless agents did not
show at these k, since most of an openless agent's opponents wear an opener
or dilution and give distinct games.

## Cell champions matching the full fit (of 6), mean of draws A and B, prior 0.01

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | 0.5 | 1.0 | 1.5 | 1.5 |
| 16 | 1.0 | 1.0 | 1.0 | 2.0 |
| 32 | 0.5 | 1.0 | 1.5 | 1.0 |
| 64 | 1.5 | 1.0 | 3.5 | 3.0 |
| 128 | 2.5 | 2.5 | 2.5 | 3.5 |
| all | 1.0 | 1.0 | 5.0 | 6.0 |

## Stratified minus random (prior 0.01): RMS vs full / disjoint error

| opponents \ games per pair | 2 | 4 | 8 | 16 |
|---|---|---|---|---|
| 8 | +8.8 / +11.2 | -1.3 / -2.4 | +0.5 / -1.3 | +3.2 / -0.2 |
| 16 | -7.3 / -5.7 | -5.1 / -3.3 | -4.1 / -1.0 | -4.4 / -0.7 |
| 32 | -0.2 / -3.0 | -4.1 / -7.5 | -1.5 / -5.1 | -1.3 / -3.6 |
| 64 | +0.4 / -0.7 | +0.6 / -0.1 | +2.9 / +2.0 | +2.9 / +3.2 |
| 128 | +0.7 / +2.5 | +1.3 / +3.4 | +1.3 / +2.6 | +1.3 / +2.4 |

One draw pair per cell, so these differences carry sampling noise of the
same order.

## The prior diagnostic

`analysis/sparse_prior_diag.py`, at the default prior. Each design uses every
cohort pair, compared with the rung-16 pinned fit, as mean shift / RMS (Elo):

| design | rows | openless node | openless time | opener8 node | opener8 time | dil20 node | dil20 time | all |
|---|---|---|---|---|---|---|---|---|
| pre-Round-4 rows only | 10,692 | -3.5 / 20.6 | -5.3 / 8.5 | - | - | - | - | -4.3 / 16.6 |
| rung 2 rows only | 135,934 | -15.6 / 42.3 | -11.2 / 52.7 | 11.3 / 36.3 | 25.7 / 47.0 | 59.7 / 67.1 | 63.4 / 70.5 | 22.2 / 54.1 |
| rung 4 rows only | 128,428 | -37.2 / 62.0 | -15.6 / 54.6 | 13.2 / 40.0 | 24.6 / 48.6 | 58.6 / 66.4 | 65.5 / 74.6 | 18.2 / 58.8 |
| rung 8 rows only | 256,856 | -20.6 / 35.6 | -7.8 / 32.4 | 7.2 / 20.2 | 14.9 / 25.5 | 28.6 / 32.7 | 34.2 / 40.7 | 9.4 / 31.9 |
| rung 16 rows only | 484,703 | -8.2 / 13.5 | -2.5 / 10.8 | 2.7 / 8.1 | 1.4 / 8.9 | 10.4 / 13.6 | 12.5 / 15.7 | 2.7 / 12.1 |
| first 2 per pair | 140,014 | -12.1 / 36.2 | -9.5 / 48.5 | 11.7 / 36.3 | 26.2 / 47.3 | 59.9 / 67.2 | 63.7 / 70.8 | 23.3 / 52.8 |
| last 2 per pair | 140,014 | -12.0 / 38.8 | -11.8 / 53.4 | 16.8 / 38.2 | 24.5 / 48.4 | 57.7 / 65.2 | 64.7 / 74.0 | 23.3 / 54.6 |
| random 2 per pair, seed 1 | 140,014 | -9.3 / 35.0 | -9.3 / 52.7 | 18.8 / 41.5 | 27.4 / 47.7 | 59.4 / 68.6 | 70.4 / 78.8 | 26.2 / 56.2 |
| random 2 per pair, seed 2 | 140,014 | -11.5 / 38.9 | -8.6 / 50.0 | 16.9 / 36.1 | 26.6 / 50.3 | 61.3 / 69.0 | 70.1 / 77.8 | 25.8 / 55.8 |
| all but pre-Round-4 rows | 1,005,921 | -0.5 / 3.5 | -0.2 / 2.7 | 0.0 / 0.0 | 0.1 / 0.3 | 0.0 / 0.2 | 0.1 / 0.2 | -0.1 / 1.8 |

Rungs 2 and 4 each hold 2 games per pair, rung 8 holds 4 and rung 16 holds
8. The shift is the same whichever 2 games are kept and roughly halves with
each doubling of games per pair, so it comes from the fit, not from when the
games were played. Low-rated cells (dil20) move up and the top cells
(openless) move down, which is shrinkage toward each agent's opponents.

What the prior does to Round 4's rung-16 fit itself (pinned, all data):

| cell | mean Elo at prior 0.5 / 0.1 / 0.01 | mean change 0.5 -> 0.01 | RMS change | largest change |
|---|---|---|---|---|
| openless x node | 928 / 935 / 937 | +8.3 | 16.9 | 39.0 |
| openless x time | 882 / 884 / 885 | +2.8 | 11.9 | 22.0 |
| opener8 x node | 780 / 777 / 777 | -3.2 | 8.5 | 21.0 |
| opener8 x time | 738 / 733 / 732 | -5.5 | 10.7 | 25.0 |
| dil20 x node | 593 / 582 / 579 | -13.3 | 15.2 | 36.0 |
| dil20 x time | 566 / 554 / 551 | -15.5 | 17.8 | 37.0 |

Top 3 cores per cell. All are `learned(model=N,<hash>,tdleaf_self,lin,shape=129-1)@1`
on node head `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3` and time head
`ab(deep=12,tt,ord,retain,time=25ms)@3`:

| cell | prior 0.5 | prior 0.1 | prior 0.01 |
|---|---|---|---|
| openless x node | m169 1173, m602 1166, m349 1153 | m169 1204, m602 1193, m349 1181 | m169 1212, m602 1199, m349 1188 |
| openless x time | m602 1182, m169 1179, m349 1154 | m602 1199, m169 1197, m349 1169 | m602 1203, m169 1201, m349 1173 |
| opener8 x node | m169 959, m171 957, m349 955 | m169 964, m171 962, m349 960 | m169 965, m171 963, m349 961 |
| opener8 x time | m169 966, m171 952, m349 948 | m169 972, m171 957, m349 953 | m169 973, m171 958, m349 954 |
| dil20 x node | m349 712, m171 706, m351 706 | m349 707, m351 701, m171 700 | m349 705, m351 700, m171 699 |
| dil20 x time | m349 711, m602 710, m169 706 | m349 706, m602 705, m169 701 | m349 705, m602 704, m169 700 |

Core order by mean Elo, prior 0.5 vs 0.01: rho 1.0000, no core moves.

## Round 4 rung 16

Rung 16 finished 2026-09-13 02:21 with 477,944 games. The store holds
1,809,009 rows (counted), sealed into `ranking/matches.0007.jsonl` and
`.0008` plus an 84 MB tail. `ranking/rungs/cores_r16.tsv` and
`standings_r16.tsv` are the pinned fit and the `_unpinned` files the
resume script's copies. `_prior0.1` and `_prior0.01` snapshots sit beside
them. `analysis/rung_convergence.py`, rung 8 vs rung 16: order PASS (rho
0.9990, worst move 2), median pm 7 -> 5 (ratio 1.40) PASS, champions FAIL on
4 of 6 cells, verdict DOUBLE AGAIN. The developer decided on 2026-09-12 to
stop at rung 16.

## Changes

- `rank.exe rate --prior X` (`g_rankPriorGames`, `RANK_PRIOR_GAMES_DEFAULT`
  0.5 in `src/ranking.h`). Used by `rankFitBT`, `rankFitBTPinned` and
  `rankFitSingle`. A non-default value writes `ranking/*_prior<X>*.*`.
  `--prior` must be > 0. New test: "BT fits - the per-pair prior sets how far
  a few-game pair is pulled toward even" checks the closed form
  p = (3 + q/2) / (4 + q) for a 3-1 pair at q = 0.5, 0.01 and 2, in both fits.
- `analysis/sparse_schedule_sim.py --prior`, passed to every fit.
- `analysis/sparse_prior_diag.py`, the diagnostic above.
- `analysis/rung_convergence.py` no longer reads `mean_elo` as a seventh cell.

**How to test:** `.\tools\run_tests.ps1 -Build` (238 test cases pass).
`.\rank.exe rate --pin ranking/rungs/pin_source_standings_20260906.tsv` must
reproduce `ranking/rungs/cores_r16.tsv` byte for byte. Adding `--prior 0.01`
writes `ranking/cores_prior0.01_pinned.tsv`, equal to
`ranking/rungs/cores_r16_prior0.01.tsv`. `rank.exe rate --prior 0` prints an
error. The GUI build also links `src/ranking.cpp` and was not rebuilt. Its
behaviour is unchanged at the default prior.

## Gotchas

- The prior's share of a pair is prior / (games + prior). Any comparison of
  schedules that differ in games per pair is confounded unless the prior is
  small or held in proportion. Deterministic pairs stop at 2 games, so the
  canonical fit carries the 20% share on every openless-vs-deterministic
  pair at every rung.
- Metrics against the full fit share data with it, so they understate error
  for designs close to the full data. The disjoint-draw error does not, but
  it cannot see a bias both draws share. Report both.
- `rank.exe rate --prior X | Select-Object -First 1` in PowerShell ends the
  pipeline after the first line and can kill `rank.exe` before it writes.
  Capture the output instead.
- `build_tests.bat` and `build_rank.bat` failed the `vswhere` lookup again.
  The direct `vcvars64.bat` + `cl` form in `CLAUDE.md` worked.

## Future Work

- **One draw pair per design.** Every error above is one A draw and one B
  draw. The differences between neighbouring cells (and all of the stratified
  vs random table) are inside that noise. Three to five draw seeds per design
  would give each cell an error bar and settle whether stratifying helps.
- **The complete graph's own error is not measured.** "All opponents" has no
  disjoint partner, so the table cannot say whether 128 opponents at 8 games
  matches all 449 at 2 games on the disjoint measure, only on RMS vs full. A
  split of every pair's 16 games into two halves of 8 (disjoint, all
  opponents) would give it.
- **A fixed panel is a different design.** Here every agent drew its own
  opponents. In the replication study's panel design every agent meets the
  same opponents, so opponent-mix error is shared across agents and partly
  cancels in contrasts between arms. A test that draws one shared panel of k
  and compares arm-to-arm differences would size the Pass 2 panel directly.
- **The ratio to Bradley-Terry SE grows with budget** (1.07x at 140 games per
  agent to 1.5x at 2,128). Counting distinct games per design would say how
  much of that is replayed games and how much is opponent-mix error.
- **Active sampling (claim R3)** needs new play and its own plan.

## Ideas This Inspired

- A per-agent prior (a fixed number of virtual games spread over the agent's
  opponents) would regularize the same way whatever the schedule, where the
  per-pair prior grows with the number of pairs.
- The scheduler could draw each agent's k opponents from a hash of the run
  seed and the pair's ids, so an agent joining later adds only its own k
  edges and existing edges never change.
- Round 4's 16 games per pair could be replaced for future cohorts by about
  128 opponents at 4 to 8 games, about 555 to 1,100 games per agent instead
  of 6,673, at 17 to 22 Elo of disjoint error per agent.
