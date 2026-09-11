# Replication study, Stage 1: results

Companion to `plans/replication-study-plan-1-brass-lectern.md`. This document
records Pass 0 (the build). Later passes append their own sections.

## Pass 0: what was built

Every item in the plan's Pass 0 table landed with a test, except the panel file
and study store of I8, which wait on dependency 1 (Round 4's fit).

| Item | Built | Where |
|---|---|---|
| I1 | `--backup td-leaf\|td-directed\|rootstrap\|treestrap`, `--tree-min-depth N` | `src/ml_tdleaf.cpp`, `tools/train_main.cpp` |
| I2 | read-only `ttPeek` and `ttGeneration`, the walk accepts only entries of the latest search | `src/transposition.h/.cpp` |
| I3 | `tdScoreToProb`, the inverse of the learned leaf tail `round(tanh(out)*900)` | `src/ml_tdleaf.cpp` |
| I4 | `--terminal winloss\|depth`, `tdTerminalTarget`, P = `TD_MAX_GAME_PLIES` = 177 | `src/ml_tdleaf.h/.cpp` |
| I5 | `--augment mirror`, `mlv2MirrorIndex` and `mlv2MirrorFeatures` made public | `src/ml_features.h/.cpp` |
| I6 | `--explore-dist eps\|ordinal`, `--ordinal-start/--ordinal-end/--ordinal-games`, root move ranking | `src/ml_tdleaf.cpp` |
| I7 | process CPU seconds (`GetProcessTimes`) in the budget and a `cpu=` provenance token | `src/train_budget.h/.cpp` |
| I8 | `rank.exe play/run --common-openings` (common random numbers across the cohort), store rows keep `seed`, `export` and `verify-store` in the analysis script. Panel file and study store: not yet | `src/ranking.cpp`, `tools/rank_main.cpp`, `analysis/replication_stage1.py` |
| I9 | `analysis/replication_stage1.py` (`analyze`, `export`, `verify-store`, `selftest`) | `analysis/` |

Model slots 1858..3857 are claimed for the study in `src/CLAUDE.md`'s slot
ledger. Nothing above 1857 was in use (checked against `models/sweep/`,
`.gitignore`'s exception list and the rosters).

### How the root is recovered

TreeStrap and ordinal need the root position after `agentChooseMove` has
already played. The trainer snapshots the board, recovers the played move by
diffing the snapshot, takes the move back (board, piece counts and
`g_chipDiff`), and checks that the root is restored exactly on every ply. A
mismatch exits with an error rather than training on a wrong position. TreeStrap
then walks the table, ordinal picks a move, and the move is replayed. Nothing
in `ai_minimax.cpp` changed.

## Verification

Full suite: **All tests passed (5870 assertions in 236 test cases)**, on the
committed build (all four binaries rebuilt after the last source change).

| Plan check | Test | Result |
|---|---|---|
| I1: the walk never changes the search | "trainTDLeaf switches - none of them changes what the search plays": at `lr = 0`, every switch against the baseline over 3 games | nodes, plies, searches and white wins equal for all 7 switches. Mirror updates exactly 2x. Ordinal at e = 1 plays identically with 0 non-best moves |
| I1: each switch acts | "each one changes what is learned" | weight L1 distance to the baseline > 1e-4 for each switch. Ordinal at e = 1 gives weights exactly equal to the baseline |
| I1: closed form | "treestrap - restricted to the root it IS rootstrap" (`treeMinDepth` 99) | weights equal within 1e-5. At d_min 1 the walk accepts more entries than there are searches |
| I2 | "ttPeek / ttGeneration" | a previous search's entry reads `gen = 1` while the current generation is 2, the current one matches, an absent key is not found. The walk's generation filter itself is code, not separately asserted |
| I3 | "tdScoreToProb - inverts the learned score tail on real positions" | 40 random positions, model output recovered within 1e-3 in probability |
| I4 | "tdTerminalTarget" | l held at P (a one-ply game) reproduces win/loss exactly, draws stay 0.5, quicker wins and slower losses score higher, a game past P clamps at l = 1 |
| I5 | "mlv2MirrorFeatures" | the mirrored board's features equal the mirrored features |
| I6 | "tdOrdinalProb / tdOrdinalPick" | 200,000 draws against the closed form within 0.005 per rank. e = 1 draws no random number, so it cannot shift later openings |
| I6 | "ordinal - e = 0 plays moves other than the search's" | non-best count > 0 |
| I7 | "CPU seconds charge busy work and not idle waiting" | a busy loop is charged, a sleeping wait of the same wall time is not |
| I8 | "common openings give every cohort agent the same couples", "rand opener - one seed plays one opening line" | seeds equal across the cohort for one panel agent, differ with the flag off. One seed replays one opening whatever the agents |
| I9 | `python analysis/replication_stage1.py selftest` | 0 failures (below) |

The plan's I7 check asked for two busy-loop loads giving equal CPU seconds and
different wall seconds. The test built is the stronger half of that: CPU
seconds track work done and ignore waiting. Wall seconds under load are not
asserted.

### Analysis self-test

Synthetic arms with known effects, 60 null repetitions for the family-wise
rate:

| check | result |
|---|---|
| NULL (true 0): Dunnett CI | [-20.8, 25.3], reported equivalent at +/-30 |
| CEIL (true +80 ceiling): Dunnett CI | [65.3, 111.5], positive, classified ceiling, ceiling gain 87.5, AULC gain 86.3 |
| SPEED (true 2 doublings, +31.1 at the last rung): Dunnett CI | [17.3, 63.4], classified speed, shift 1.95 doublings [1.80, 2.20], compute multiplier 0.213 (true 0.25) |
| NEG (true -60): Dunnett CI | [-61.6, -15.5], negative, multiplier censored |
| family-wise false positives under the null | 2 of 60 |
| verify-store | accepts paired common openings (12 couples, 0 broken), rejects a broken couple (2 broken couples, 4 agents with a differing seed set) |

## Cost smoke: every arm at the study head

Head `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3` (train flags
`--depth 12 --node-budget 100000 --rem 70 --retain`), scratch init, 20 games,
seed 1001, 4 opener plies. All 9 processes ran at once on one machine. One
seed and 20 games, so game lengths differ by arm partly by chance. The learning
rates are placeholders (0.01, TreeStrap 0.0005), not tuned values. This is the
Pass 0 cost check, not Pass 2's cost calibration.

| arm | switch | CPU s | searched moves | mean game length (plies) | nodes | CPU ms per searched move | nodes per searched move |
|---|---|---|---|---|---|---|---|
| B0 | TD-Leaf | 25.28 | 1112 | 64.6 | 100,920,751 | 22.7 | 90,756 |
| A1 | `--backup td-directed` | 27.00 | 1214 | 69.7 | 110,966,543 | 22.2 | 91,406 |
| A2 | `--backup rootstrap` | 24.08 | 1050 | 61.5 | 94,906,371 | 22.9 | 90,387 |
| A3 | `--backup treestrap` | 79.25 | 996 | 58.8 | 87,907,832 | 79.6 | 88,261 |
| A4 | `--lambda 1` | 22.30 | 1004 | 59.2 | 88,835,136 | 22.2 | 88,481 |
| A5 | `--terminal depth` | 24.64 | 1100 | 64.0 | 98,667,036 | 22.4 | 89,697 |
| A6 | `--augment mirror` | 26.81 | 1204 | 69.2 | 109,555,371 | 22.3 | 90,993 |
| A7 | `--explore 0.1` | 23.42 | 1009 | 64.7 | 93,008,879 | 23.2 | 92,179 |
| A8 | `--explore-dist ordinal`, e 0 -> 1 over 20 games | 16.80 | 738 | 45.9 | 64,036,927 | 22.8 | 86,771 |

Instrument readings from the same runs:

| arm | reading |
|---|---|
| B0 | mean PV depth 5.63 of 12, 1112 of 1112 truncated, 38 decided leaves skipped |
| A4 | mean PV depth 6.18 of 12 |
| A3 | 6,991,769 table entries accepted (7,019.85 per search), 3,235,486 updates with a nonzero gradient |
| A6 | 1179 of 2358 updates mirrored |
| A8 | 738 draws, 353 not the search's choice, alternatives ranked by table entry 16,727 times and by static eval 2,147 times |

What the table shows:

- Every arm except A3 costs 22.2 to 23.2 CPU ms per searched move, so their
  per-game cost is set by game length. A3 costs 3.5x more per move (79.6 ms
  against 22.7) from the table walk, at the same nodes per move. At equal CPU
  seconds A3 therefore plays roughly 3.5x fewer games, which is the matching the
  study is built to make. A3's model here was saturated by its learning rate
  (Pass 1, section 6), so this per-move cost is for a diverged model.
- A6's mirrored updates add nothing measurable per move (22.3 ms).
- Every B0 PV is truncated before depth 12 (mean 5.63). The table is
  always-replace and a depth-12 search at 100k nodes does not keep a full line.
  Theory 72's guard asks for mean PV depth well above 1, which holds. Truncation
  at this head is a property of the baseline to report, not a defect.
- A8's e starts at 0 in this smoke, so 48% of its moves are not the search's
  choice and its games are shorter (45.9 plies).

## Implementation decisions and deviations

These carry into the per-arm deviation lists in the final write-up.

1. **Ordinal ranking (I6) ranks by the serving search's own table, not a second
   search.** The plan left a choice between a training-only full-window root
   search and ranking by bound. Built: the search's chosen move first, then
   each other root move by its child's stored fail-soft entry from the same
   search (generation-matched), then by static eval where the entry was
   overwritten. A move that wins at once ranks above all stored entries, a move
   into a decided loss below. Within a tier moves sort by value, ties by move
   generation order. No
   second search, so ordinal costs the same per move as the baseline (22.8 ms
   against 22.7). The ranking of non-best moves is by bounds, not exact values,
   which is the recorded deviation. In the smoke 89% of alternatives were
   ranked by a table entry.
2. **Decided roots are not trained in any arm.** A root that `nearWinCheck`
   already decides is played out without a training entry, as in the baseline.
   A root whose search proves a result (a score inside the win sentinels) maps
   to target 1 or 0, as the plan says.
3. **P = 177, derived, not the 400-ply cap.** Every move advances a piece one
   row and reaching the far row wins, so a side makes at most 8x6 + 8x5 = 88
   non-winning moves and a game lasts at most 88 + 88 + 1 = 177 plies. The
   derivation is in `src/ml_tdleaf.h`.
4. **A searched move that ends the game gives no training entry**, as in the
   baseline, and for TreeStrap its tree terms are dropped too.
5. **TreeStrap accumulates densely and applies once per game** (or per batch),
   the same update timing as the other arms. `l2` is applied once per
   application. The study uses `l2 = 0`, where the dense per-game sum equals
   sequential SGD within the game's fixed weights.
6. **Ordinal and `--explore` are exclusive**: the trainer refuses both at once.
   Ordinal draws every searched move, so there is no separate epsilon.
7. **Mirror needs v2 features, TreeStrap needs a linear model head.** Both are
   refused otherwise.
8. **Summary printing.** A TreeStrap run prints the table-walk line instead of
   the trained-positions line, and only TD-Leaf prints mean PV depth. The
   first cost smoke printed zeros there, which would have read as a broken
   arm.

## How to test

```powershell
.\tools\run_tests.ps1 -Build          # or tests.exe after building
python analysis/replication_stage1.py selftest
.\train.exe tdleaf --out <scratch>\a3 --init "" --depth 12 --node-budget 100000 --rem 70 --retain --games 20 --seed 1001 --backup treestrap --lr 0.0005
```

Expect the suite to pass, the self-test to report 0 failures, and the TreeStrap
run to print a `treestrap (d_min 1): table entries accepted ...` line and a
provenance ending `backup=treestrap,dmin=1,seed=1001,games=20,secs=...,cpu=...,nodes=...`.

## Pass 1: sanity

Run by `tools/replication_pass1_sanity.ps1` (seed 2001, head
`ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3`, scratch init, 4 random
opener plies, placeholder learning rates as in the cost smoke). Every check
passed (0 failures). The panel-game independence check waits for the panel.

### 1. Rungs stop where they should and provenance is truthful

Each arm trained with `--ckpt-at 10,20`. Every rung's `games=` equals its rung,
`cpu=` and `nodes=` grow from rung 10 to rung 20, the arm's switch token is
present, the baseline carries no switch token, and every model says
`init:scratch`.

| arm | rung 10 CPU s | rung 20 CPU s | rung 10 nodes | rung 20 nodes |
|---|---|---|---|---|
| B0 | 13.61 | 27.42 | 53,958,562 | 108,260,354 |
| A1 | 14.92 | 29.69 | 58,810,275 | 116,783,343 |
| A2 | 14.63 | 28.78 | 56,969,476 | 111,495,238 |
| A3 | 57.34 | 101.23 | 48,965,481 | 90,105,192 |
| A4 | 12.94 | 25.11 | 51,641,817 | 100,720,047 |
| A5 | 13.77 | 27.55 | 54,050,367 | 108,839,841 |
| A6 | 13.48 | 27.78 | 52,813,715 | 108,223,767 |
| A7 | 10.73 | 22.88 | 40,901,976 | 87,542,430 |
| A8 | 7.13 | 15.03 | 27,937,892 | 58,281,324 |

### 2. Same seed twice gives the same model

Each arm was trained twice with the same command. Both rungs of all 9 arms
are identical in every byte except the measured `secs=` and `cpu=` values.

### 3. At lr 0 no switch changes what the search plays

3 games per arm at `--lr 0`, A8 with e held at 1. A7 is excluded because
exploring is meant to change play.

| arm | search nodes | searched moves | W-B-D |
|---|---|---|---|
| B0 | 13,178,700 | 149 | 1-2-0 |
| A1, A2, A3, A4, A5, A6, A8 | 13,178,700 | 149 | 1-2-0 (each) |

### 4. Every checkpoint loads through the real search path

Each rung-20 checkpoint was published to its slot (1858..1875, ledger
`models/sweep/replication_stage1_pass1.csv`) and played 2 games against B0's
rung 20 by `rank.exe pairgen` with 4 random opener plies. B0's own row is its
rung 10 against its rung 20. Two games per pair is a load test, not a
strength reading.

| arm | agent (a) | a wins | B0 rung 20 wins |
|---|---|---|---|
| B0 (rung 10) | `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3.learned(s1858,57d884ef)@1` | 1 | 1 |
| A1 | `...learned(s1861,29aa5040)@1` | 1 | 1 |
| A2 | `...learned(s1863,611eb82d)@1` | 1 | 1 |
| A3 | `...learned(s1865,f6bf0760)@1` | 2 | 0 |
| A4 | `...learned(s1867,845b883e)@1` | 0 | 2 |
| A5 | `...learned(s1869,d89f2e3a)@1` | 1 | 1 |
| A6 | `...learned(s1871,bf5941db)@1` | 1 | 1 |
| A7 | `...learned(s1873,4d8a036d)@1` | 0 | 2 |
| A8 | `...learned(s1875,a8907b4c)@1` | 1 | 1 |

### 5. The runs went to 500 games, which gave a longer reading

The first run passed `--ckpt-at 10,20` without `--games`, and the trainer ran
to `max(500, top rung)`: its default game count wins over a ladder shorter
than 500. The rung files were written on the way and are what sections 1 to 4
test. The script now passes `--games` equal to the top rung. The full logs
give a 500-game reading per arm (9 runs plus their 9 repeats, up to 10 at once):

| arm | CPU s | searched moves | mean game length | CPU ms per searched move | mean PV depth (of 12) |
|---|---|---|---|---|---|
| B0 | 678.8 | 28,820 | 66.6 | 23.6 | 5.72 |
| A1 | 641.4 | 27,603 | 64.2 | 23.2 | |
| A2 | 651.4 | 27,607 | 64.2 | 23.6 | |
| A3 | 2,102.7 | 26,013 | 61.0 | 80.8 | |
| A4 | 702.6 | 30,766 | 70.5 | 22.8 | 6.36 |
| A5 | 702.5 | 30,634 | 70.3 | 22.9 | 5.76 |
| A6 | 690.0 | 30,276 | 69.6 | 22.8 | 5.74 |
| A7 | 498.3 | 20,870 | 55.5 | 23.9 | 5.82 |
| A8 | 680.9 | 30,148 | 69.3 | 22.6 | 5.78 |

Every TD-Leaf arm's mean PV depth is above 5.7 of 12 with every PV truncated,
as at Pass 0. A6 mirrored 29,640 of 59,280 updates. A8 drew 30,148 moves, 326
not the search's choice, since e reached 1 at game 20.

### 6. TreeStrap diverged at the placeholder learning rate

A3 accepted 170,696,145 table entries (6,561.96 per search) but only 474,027
had a nonzero gradient, 0.28%, against 46% in the 20-game cost smoke. Its
weights show why:

| checkpoint | bias | max \|w\| | mean \|w\| |
|---|---|---|---|
| B0 rung 20 | 0.0150 | 0.0912 | 0.0277 |
| B0, 500 games | 0.0343 | 0.6380 | 0.1026 |
| A2, 500 games | 0.1667 | 0.3927 | 0.0882 |
| A3 rung 10 (lr 0.0005) | 18.42 | 19.67 | 3.256 |
| A3 rung 20 | 18.42 | 19.67 | 3.256 |
| A3, 500 games | 17.97 | 19.28 | 3.212 |

By game 10 the evaluation is saturated, every score sits at the sentinel edge,
and the one-sided gradient is zero almost everywhere. The dense per-game sum
covers about 6,500 to 9,500 entries per search times about 60 searches, so a
step size sized for one position per search is several hundred times too
large. The same seed at smaller rates, 20 games:

| lr | rung 10 bias | rung 10 max \|w\| | rung 20 max \|w\| | entries accepted per search | nonzero updates |
|---|---|---|---|---|---|
| 0.00005 | -0.163 | 0.2906 | 0.2719 | 9,238.64 | 2,748,500 of 10,799,967 |
| 0.000005 | -0.0028 | 0.0506 | 0.0520 | 9,442.03 | 2,053,032 of 9,800,824 |
| 0.0000005 | 0.00003 | 0.0502 | 0.0504 | 9,404.71 | 3,339,181 of 11,135,173 |
| 0.00000005 | 0.000006 | 0.0500 | 0.0500 | 9,468.54 | 3,704,849 of 10,595,295 |

The scratch init has max |w| 0.050. At 5e-5 the weights move to the size B0
reaches and stay there to game 20. At 5e-6 and below they barely move in 20
games. So A3's workable learning rate is 100 to 1,000 times below B0's, and
the plan's "same log range for every arm" and "each arm's effect at the
baseline's rate" both need restating for A3 before Pass 2.

This also qualifies the Pass 0 cost smoke: A3 there ran at lr 0.0005 and was
saturated too, accepting 7,020 entries per search against about 9,400 at a
working rate. A3's CPU per move at a working rate is Pass 2's cost
calibration to measure.

## Pass 2: the learning-rate probe

Developer decision (2026-09-11): every arm's learning rate is tuned over a
log range of one shared width, placed at each arm's own divergence point by
one probe. Run by `tools/replication_lr_probe.ps1`: all 9 arms x 17
half-decade rates from 1e-8 to 1, seed 3001, 50 games, head
`ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3`, scratch init, 10 at once.
Rows: `models/sweep/replication_lr_probe.csv`.

Max |w| at game 50 (the scratch init is 0.049). `*` marks a run whose max |w|
passed 5 at some 10-game checkpoint:

| lr | 1e-5 | 3.2e-5 | 1e-4 | 3.2e-4 | 1e-3 | 3.2e-3 | 0.01 | 0.032 | 0.1 | 0.32 | 1 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| B0 | 0.049 | 0.050 | 0.050 | 0.049 | 0.065 | 0.109 | 0.178 | 0.326 | 1.569 | 5.19* | 13.45* |
| A1 | 0.049 | 0.050 | 0.051 | 0.052 | 0.050 | 0.067 | 0.107 | 0.347 | 0.766 | 4.19 | 15.12* |
| A2 | 0.049 | 0.050 | 0.050 | 0.050 | 0.063 | 0.108 | 0.167 | 0.310 | 2.431 | 10.44* | 26.54* |
| A3 | 0.074 | 0.207 | 7.00* | 17.16* | 138.8* | 325.9* | 632.9* | 2,417* | 10,643* | 23,816* | 74,209* |
| A4 | 0.052 | 0.055 | 0.061 | 0.073 | 0.207 | 0.661 | 2.007 | 8.78* | 22.52* | 54.36* | 174.9* |
| A5 | 0.049 | 0.049 | 0.050 | 0.051 | 0.063 | 0.071 | 0.134 | 0.278 | 0.625 | 2.78 | 11.27* |
| A6 | 0.049 | 0.050 | 0.050 | 0.052 | 0.071 | 0.098 | 0.157 | 0.512 | 3.300 | 8.68* | 39.10* |
| A7 | 0.049 | 0.049 | 0.050 | 0.050 | 0.052 | 0.068 | 0.140 | 0.409 | 1.099 | 3.92 | 14.57* |
| A8 | 0.050 | 0.050 | 0.050 | 0.050 | 0.058 | 0.066 | 0.154 | 0.467 | 1.400 | 4.59 | 9.84* |

A3 below 1e-5: 0.058 at 1e-6, 0.063 at 3.2e-6. B0's 0.32 row peaked at 5.54
at game 40, A1's at 4.19, so their D values differ by half a decade on a
threshold both rows sit near.

D (divergence point), L (the lowest rate moving mean |w - init| >= 0.01 by
game 50) and the resulting tuning range [D / 10^2.5, D / 10^0.5]:

| arm | L | D | range low | range high |
|---|---|---|---|---|
| B0 | 0.00316 | 0.316 | 0.001 | 0.1 |
| A1 | 0.01 | 1 | 0.00316 | 0.316 |
| A2 | 0.01 | 0.316 | 0.001 | 0.1 |
| A3 | 3.16e-5 | 1e-4 | 3.16e-7 | 3.16e-5 |
| A4 | 0.001 | 0.0316 | 1e-4 | 0.01 |
| A5 | 0.01 | 1 | 0.00316 | 0.316 |
| A6 | 0.00316 | 0.316 | 0.001 | 0.1 |
| A7 | 0.01 | 1 | 0.00316 | 0.316 |
| A8 | 0.01 | 1 | 0.00316 | 0.316 |

Every range starts below its arm's L, by half a decade (B0, A6) to 2 decades
(A3). L is measured at 50 games, and a rate that barely moves the weights in
50 games can move them over a Pass 3 ladder, so the low end is not ruled out
by this reading. The same horizon limit applies at the top: a rate half a
decade below D can still diverge after game 50. B0's current rate, 0.01, sits
inside its range.

CPU seconds per 50 games: 63.5 to 80.5 for B0 at every rate, 437 to 458 for
A3 at rates that did not diverge and 251 to 259 at rates that did. At a
working rate A3 costs about 9.0 CPU s per game against B0's 1.33, 6.8x, not
the 3.5x the saturated cost smoke showed. At equal CPU seconds A3 plays about
a seventh as many games.

## Pass 2 driver

`tools/replication_pass2.ps1` runs Pass 2's three rated steps against a
panel file given as input (developer decision, 2026-09-11: build the driver
now, launch once Round 4 pins the panel).

| step | what it trains | what it reports |
|---|---|---|
| `curve` | every arm, seed 4001, ladder 10, 20, 40, ... 5120 games, at the geometric middle of its locked range | Elo per arm per rung, cumulative CPU seconds per rung, CPU seconds per game between rungs (the cost output) |
| `noise` | B0 and one contrasting arm, 5 seeds (4101..4105), to the game count at which each arm's curve run reached `-NoiseCpu` CPU seconds, every curve rung below it rated | Elo by seed per rung, the spread over seeds (sigma_seed), the mean pm (sigma_meas) |
| `tune` | the same random draws for every arm over its locked range, 1 seed per draw (5001 + draw), to the game count matching `-TuneCpu` | per arm, draws sorted by learning rate with Elo, the best draw, and an EDGE flag when the best rate sits within 0.2 decades of a range end |

Choices the plan left open, made in the driver and listed here so they are
visible when the grid goes to the developer:

1. **Curve and noise run at the middle of each arm's locked range**
   (D / 10^1.5): 0.01 for B0, A2, A6, 0.0316 for A1, A5, A7, A8, 0.001 for
   A4, 3.16e-6 for A3. Every arm sits at the same position relative to its
   own D. For A3 that is a decade below its L (3.16e-5), so its curve may
   rise late for a reason that is the rate, not the technique. The tuning
   step does not depend on this choice.
2. **Tuning budget.** `-Draws` (default 16) draws for the lr-only arms (A2,
   A4, A5, A6), twice that for arms with an arm-specific hyperparameter,
   drawn jointly with the rate. Every arm uses the same (u, v) pairs from one
   seeded generator (`-DrawSeed 7`, written to `<work>/tune_draws.csv`), so
   the draws sit at the same relative positions in every range.
3. **Arm-specific ranges.** lambda uniform on [0, 1] for B0 and A1, d_min
   from {1, 2, 4, 8} for A3, epsilon log-uniform on [0.01, 0.3] for A7, the
   ordinal start e uniform on [0, 1] for A8, rising linearly to 1 over the
   run. A4 keeps lambda = 1. RootStrap and TreeStrap do not read lambda.
4. **A5 to A8 train at B0's tuned lambda** (`-BaseLambda`, required before
   they tune), since each differs from B0 in one switch. So B0, A1, A2, A3
   and A4 tune first, then A5 to A8.
5. **Tuning and noise run at matched CPU seconds, not matched games.** Each
   arm's game count is where its curve run's cumulative CPU seconds reach
   the target, linear between rungs. At a working rate A3 costs about 6.8x
   B0 per game (probe), so matched games would give it 6.8x the compute.
6. **Every study agent wears `.opener(rand,moves=8)@1`**, the opener8
   division's opener. `rank.exe` plays a pair of two deterministic agents
   only 2 games (one per colour), so an openless study agent against an
   openless panel member would never reach `-GamesPerPair`.
7. **One process per study agent, one store part per agent.** `rank.exe
   play --cohort` schedules every pair touching a cohort agent, study agents
   against each other included, which the plan rules out. A roster of the
   panel plus one study agent schedules only that agent's panel games.
   Common openings are keyed on the panel opponent, so separate processes
   still draw identical couples. The parts are listed in
   `ranking/matches_rep1.index.txt`, which `rank.exe` reads as one store,
   and `analysis/replication_stage1.py verify-store` now reads the same
   index.

### Stand-in test

Run end to end on a stand-in panel from the 2026-09-06
`ranking/standings.tsv` (not the study panel): `rand@1` (0), `tiered@1`
(412), `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1.opener(rand,moves=8)@1`
(471), `greedy@1.classic(chip=100)@2` (553),
`ab(deep=6,tt,ord,nodes=200k)@3.classic(chip=100)@2.opener(rand,moves=8)@1`
(846), `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=96,990e39e7,pool_games,lin,shape=129-1)@1.opener(rand,moves=4)@1`
(1057). Arms B0, A3, A8 (A2 added to tune), curve ladder 4 and 8 games, 4
games per panel opponent, slots 3820..3847. The test's models, slots, store
and pinned outputs were deleted afterwards.

| check | result |
|---|---|
| curve: train, publish, play, rate, report, export | 6 agents rated, 24 games each |
| noise, `-NoiseCpu 30` | B0's curve run reached 30 CPU s at 28 games, A3's at 4, the ladder became 4, 8, 28 for B0 and 4 for A3 |
| tune, `-Draws 2` | 4 draws each for B0, A3, A8, 2 for A2. B0 and A2 drew the same rates (0.00584, 0.0210) and A3 and A8 the same relative positions. Every model's provenance carries the drawn lambda, d_min, or ordinal start. A8 was passed `-BaseLambda 0.7`, which equals the trainer's default, so this test does not show the value arriving |
| `verify-store`, every step | 0 couples without exactly one game per colour, 0 agents whose seeds differ from the others' against any panel opponent |
| distinct trajectories | 23 of 24 rows on every agent. The repeat is one game against `greedy@1.classic(chip=100)@2`, lost in 12 plies with 0 search nodes: the study agent's 6 moves were all opener moves, so two openings lost the same way. Two different seeds, not a replay |

Two PowerShell traps hit while building it, both fixed: a local `$draws`
overwrote the `-Draws` parameter (variable names are case-blind), and a
`switch` statement's automatic `$switch` shadowed a script variable of the
same name inside functions it called.

## Still open before Pass 2's rated steps

- The panel file and the study store (I8) wait for Round 4's fit and the
  `ranking/CHAMPION.md` rewrite, from which the panel ratings are pinned.
  Curve shape, seed noise and the tuning search are all rated against the
  panel. The driver is ready: `-Step curve -Panel <file>`.
- The per-arm ranges above were locked as computed (developer decision,
  2026-09-11), with the playbook's edge check as the safety net.
- C2, C6 and C7 have been read (plan's claims table). C2's chess comparison
  was against human opponents from standard material values, not from
  expert weights and not by self-play. C6's source reports a practice and no
  measured effect. C7's evidence is Sutton's random walk and an informal
  remark in Tesauro 1992.

## Future Work

- **The generation filter is not asserted end to end.** The `ttPeek` test shows
  a stale entry reads as stale, and the lr = 0 test shows the walk leaves the
  search unchanged. No test builds a table where a stale entry would be reached
  by the walk and checks it is not trained. That matters to A3's claim that it
  trains only on the current search's tree. A test that searches twice, then
  counts accepted entries with and without the generation check, would settle
  it.
- **The I7 load check.** CPU seconds under contention were not compared to an
  idle machine. The cost smoke ran 9 processes at once, so its CPU seconds may
  include cache contention. Pass 2's cost calibration should run one arm alone
  and one under the study's worker count and report both.
- **Ordinal's bound ranking against an exact ranking.** How often the bound
  order differs from a full-window root search's order is unmeasured. A small
  offline comparison on sampled roots would say whether deviation 1 changes
  which move rank e draws.
- **Games decided inside the opener carry no information about the model.**
  With 8 random opener moves, a study agent can lose to a weak panel member
  before its first search (the stand-in test's 12-ply loss with 0 nodes).
  Such games add the same noise to every arm. Counting games whose study
  side searched 0 nodes, per panel member, would say whether the weakest
  panel members should be dropped or the opener shortened.
- **A3's curve lr sits below its L.** If A3's curve rises late, a second A3
  curve at the top of its range would say whether T_max is being placed by
  the technique or by the rate.

## Ideas This Inspired

- The table walk's accepted-entry count (7,020 per search at this head) is a
  cheap measure of how much of a search's tree survives in an always-replace
  table. It could size the table for any head.
- The take-back and replay machinery makes any root-level intervention cheap:
  a trainer could also try policy-target distillation from the same table
  entries without touching the search.
- A5 and the search's win-decay both reward short wins. A version of A5 with the
  search's decay switched off would separate the two.

## Commit

Pass 0 commit message:

```
Build the replication study's Pass 0: backup, terminal, mirror and ordinal switches
```

Pass 1 commit message:

```
Run the replication study's Pass 1 sanity checks: all pass, TreeStrap's step size diverges
```

Learning-rate probe commit message:

```
Probe every replication arm's learning rate: divergence points span 1e-4 to 1
```

Pass 2 driver commit message:

```
Add the replication study's Pass 2 driver: curve, noise and tuning against a panel file
```
