# Results: heuristic evaluator overhaul (Advanced evaluator + benchmark + hill climb)

Companion to `heuristic-eval-overhaul-plan-1-buzzing-floyd.md`. Work done 2026-07-11.

## Summary of changes

- **New "Advanced" evaluator** (`evalAdvanced`, `src/ai_eval.cpp`), 16 params, ranking
  codec `adv` with letters `tcwlfdemhborxnsg`. Terms beyond Experimental's five:
  Support (diagonal same-color pairs, merging the todo's "defended pieces" and
  "diagonal phalanx" ideas, which are the same pair geometry), Center, Mobility,
  Hole (D10 outpost-admitting back-rank columns), Control, Open, Race
  (closest-piece distance differential), Overext, Noise (seeded random piece-square
  values, params Noise + NoiseSeed), and RaceWin (0/1, the exact D9/D14
  decided-race sentinel detector, side-to-move aware with a one-tempo margin for
  the non-mover). Registered after LearnedValue so persisted evaluator indices
  stay stable (`minimax_params.txt` stores a raw index).
- **Incremental paths** for every sum-local term via the generalized
  `evalPosFull`/`evalPosLocal` pair (per-square deltas; a mobility+overext
  bounding-box sum; hole/open affected-column recomputes). Race/RaceWin read row
  extremes from new `g_rowCountW/B` counters maintained in make/unmake under a
  `g_evalRowCounts` gate. `MAX_EVAL_PARAMS` 8 -> 20.
- **Zero-weight gating fix** (standing todo item from the chip-count study):
  `evalBeginSearch` leaves `g_evalIncremental` off when every positional weight is
  0, so the champion-weight configuration no longer pays accumulator maintenance.
- **Capacity helpers** `capacityWhite/Black()` (`src/board_analysis.cpp`, Lemma B)
  plus a code-verified identity (below) resolving theory 18's evaluator-feature
  half analytically.
- **Hill climber rewrite** (`tools/hill_climb.ps1`): climbs the Advanced 13-weight
  mix (turn, noise seed, RaceWin pinned), abs-sum-80 normalization, seeded mutation
  RNG, and a new `-AllowNegative` switch (sign-flip mutations + signed drastic
  resets) for the signed-weights experiment.
- **speedBench extensions**: Advanced rows in the ms-budget table, four Advanced
  eval-level-ladder presets (all-on + isolated mobility/hole/open).
- Tests: 661 assertions in 65 test cases, all passing (new: Advanced==Experimental
  /Classic at zero extras, per-feature crafted positions, RaceWin witness +
  near-misses, all-terms-on incremental walk asserting accumulator + row counters
  + `evalLeaf`==`evaluateBoard` at every node, capacity identity, adv codec round
  trip with a negative weight).

Commits: `c599e90` (engine + tests + tools + docs), plus a results/docs commit
after the studies.

## How to test

- `.\tools\run_tests.ps1 -Build` (full suite).
- `.\train.exe speed --positions 24 --ms 150 --seed 42 --maxdepth 5 --reps 5
  --warmup 1` (ms-budget table + eval-level ladder incl. the Advanced rows; the
  equivalence self-check must print PASS).
- `.\rank.exe gauntlet --id "ab(d4)@1.adv(t20,c80,w0,l0,f0,d0,e0,m0,h0,b0,o0,r0,x0,n0,s1,g1)@1" --games 4 --roster ranking/climb_roster.txt` (any adv ID round-trips).
- `.\tools\hill_climb.ps1 -Iters 2 -Games 2 -Depth 2 -AllowNegative` (climber smoke).
- Console: pick Advanced in `getSettings()`; GUI: the Eval dropdown gains
  "Advanced" (see Known limitations for the panel height).

## The capacity identity (theory 18, analytic half)

White capacity = sum over White pieces of `(7 - y)`, Black capacity = sum over
Black pieces of `y` (axioms.md Lemma B). Then

```
capacityBlack - capacityWhite == forwardSum - 7 * chipDiff
```

where `forwardSum` is the Forward term at weight 1. Verified by a unit test on
crafted boards and the standard start (each side's start capacity = 104, D2's
constant). Consequence: a capacity-difference eval weight is linearly dependent
on chip + forward and adds nothing to a linear evaluator; a capacity weight k is
identical to forward += k, chip -= 7k. Reaching the pure-capacity direction
requires a NEGATIVE chip weight, which only the `-AllowNegative` climb can find.
The predictor-correlation half of theory 18 (does capacity difference correlate
with outcomes across `matches.jsonl`) remains open; `capacityWhite/Black()` are
the helpers for that future pass.

## Speed benchmark

Command: `.\train.exe speed --positions 24 --ms 150 --seed 42 --maxdepth 5 --reps 5 --warmup 1`
(fixed seeded workload, interleaved levels, warmup discarded, mean/median/min
recorded; full output kept this session in `build/speed_adv_s42.txt`, gitignored).
Eval-level ladder equivalence self-check: **PASS** (same end board + node count
across levels for every row).

### Zero-weight gating fix (the standing todo item)

`classic(t1,c4,w0,l0)` (champion weights) v2->v3 mean us/move deltas by depth:

| depth | before (chip-count study) | after |
|---|---|---|
| d1-d5 | +18 to +35% | -3.5%, -3.6%, -2.3%, -1.4%, -0.5% |

The incremental machinery no longer costs anything when it has nothing to
maintain. The v1->v2 chip-counter speedup itself re-measured at -42 to -57%,
consistent with the chip-count study's -45 to -62%.

### Advanced evaluator cost (ms-budget table, us/move)

| agent | d4 | d5 | nodes/move d5 |
|---|---|---|---|
| chip (classic t0,c4) | 188.5 | 2066.5 | 49212 |
| adv chip-only, g0 (t1,c4) | 201.6 (+7.0%) | 2199.0 (+6.4%) | 49280 |
| adv chip-only + RaceWin g1 | 217.1 | 2388.9 (+8.6% over g0) | 49331 |
| adv all-features-on | 9111.5 | 105431.6 | 187258 |

- The +6-7% at champion-equivalent weights is per-leaf zero-checks (and the t1 vs
  t0 turn weight slightly perturbs node counts, so this is approximate).
- The RaceWin detector costs ~8% us/move at champion weights (row-extreme reads
  at every leaf).
- All-features-on runs at ~0.56 us/node vs classic chip's ~0.04 us/node: ~13x
  per-node. Different evals search different trees, so only the per-node figure
  is meaningful.

### Per-feature incremental pricing (ladder v2->v3, full-scan leaf vs incremental)

| preset | v2->v3 range over d1-d5 | verdict |
|---|---|---|
| adv all-on | -41 to -52% | incremental wins big |
| adv chip+mobility only | +31 to +68% | incremental LOSES |
| adv chip+hole only | +17 to +30% | incremental LOSES |
| adv chip+open only | +24 to +63% | incremental LOSES |

Honest scope finding (extends theory 16's zero-weight qualifier): a term whose
full-board scan is cheap (hole: 16 column checks; open: 64 reads) or whose local
delta has a wide affected set (mobility: two bounding boxes per make PLUS per
unmake, paid at every interior node) is faster recomputed at the leaf than
maintained incrementally, when it is the only enabled term. The incremental path
pays off when several terms share the per-move delta (all-on halves the cost).
Future refinement: per-term routing (each term chooses accumulator vs leaf-scan
by weight pattern).

## Behavioral sanity (Advanced == champion at equivalent weights)

- `rank.exe pairgen` head-to-head vs `learned(s97)`: the adv-equivalent and the
  classic champion produced **byte-identical games** (111 positions, agent-id
  fields masked, 0 differing rows). The policies are move-identical, as designed.
- Gauntlets are NOT the instrument for confirming this: same-policy agents with
  different ID strings score differently because per-game seeds are derived from
  the ID (stochastic opponents play different games) and search side-state (TT,
  killer/history tables) persists across games within a gauntlet process, so even
  deterministic opponents diverge between replays. Measured on the main pool at
  `--games 4`: classic champion ID 1162 +/- 32 (seed 7) / 1193 +/- 34 (seed 8);
  adv-equivalent 1064 +/- 28 / 1102 +/- 29; and the pre-existing exp(f0)
  champion-equivalent control 1078 +/- 29 -- the two re-labeled equivalents agree
  with each other and sit ~95 below the incumbent ID in both samples. Per-opponent
  records against the five deterministic learned agents differed between gauntlets
  (adv exactly 2-2 vs each; classic mixed 4-0/3-1/1-3), which is what implicated
  process-level state rather than policy. Filed as a new theory-log entry
  (gauntlet identity artifact) since it biases any gauntlet comparison of
  near-identical agents by up to ~100 Elo.

## RaceWin and Noise ablations (climb pool, 12 games/opponent, seed 7, d6,ord,nb200k head)

| config | Elo |
|---|---|
| adv t1,c4 baseline (g0) | 1362 +/- 85 |
| + RaceWin (g1) | 1292 +/- 72 |
| + noise n1,s1 (chip 4) | 840 +/- 40 |
| + noise n3,s1 (chip 4) | 562 +/- 43 |
| adv t20,c80 baseline (g0) | 1158 +/- 55 |
| + noise n1,s1 (chip 80) | 887 +/- 41 |
| + noise n3,s1 (chip 80) | 849 +/- 40 |

- **RaceWin adds no measurable Elo at d6** (-70 +/- 111 vs baseline, well within
  noise). The provably-sound detector fires too rarely at d6-decided games to
  move win rates on this pool, while costing ~8% us/move. Keep it available and
  ablatable; do not assume it is a free win. Theory 21. Untested regime worth a
  pass: shallow (d2-d4) and budget-cut searches, where added horizon matters.
- **Per-piece noise is refuted as a tie-breaker at d6, at both scales.** At the
  champion's chip=4, per-piece noise in [-1,+1] across ~32 pieces has a typical
  magnitude around one chip: n1 costs ~520 Elo and n3 ~800 (dose-dependent). At
  the climber's chip=80, where the typical noise sum is only ~0.06 chips, n1
  still costs ~270 and n3 ~310 vs the same-scale baseline -- re-sorting
  near-equal branches at d6 measurably loses games even when "dominated by the
  real evaluation". Theory 20. The surviving variant is a strictly bounded
  per-position nudge (a maintained raw-hash accumulator read modulo 2n+1 at the
  leaf), which decouples the bound from piece count; untested.
- The two same-policy baselines (t1,c4 = 1362 vs t20,c80 = 1158, identical play
  by scale invariance) differ by ~200 on this small pool -- the theory-19
  identity artifact again. All comparisons above are against the SAME-scale
  baseline for this reason.

## Hill climbs (run A non-negative vs run B -AllowNegative)

Setup: identical for both runs -- `-Iters 60 -Games 4 -Depth 4 -Seed 1`, start
`c80` (t20, s1, g1 pinned), climb pool = the 9-opponent `climb_roster.txt`,
fitness = gauntlet Elo (SE +/- 60-85 per evaluation). Runtime ~4 minutes per
run (both together under 9 -- far cheaper than estimated; the d4-vs-climb-pool
games are fast). Start baseline: 894 +/- 68 (identical cached ID both runs).
Each run accepted 3 of 60 mutations.

| run | best mix (t20 pinned, s1, g1) | climb Elo |
|---|---|---|
| A (non-negative) | c75, h3, r2 | 1075 +/- 80 |
| B (signed) | c77, d-2, b1 | 1114 +/- 85 |

Findings:

- **Both modes re-converge on chip dominance.** Every top-5 candidate in both
  runs is 75-80/80 chip. Wall, column, forward, support, center, mobility, open,
  overext, and noise are all zero in run A's top 5; run B's only nonzero
  newcomers are tiny (d-2, b1). Consistent with E3 and the champion's
  material-only mix, now across 11 more candidate terms.
- **Signed mode did settle one weight negative** (Support at -2, the
  anti-clustering direction E6's confound note anticipated), and its best (1114)
  nominally beats run A's (1075). But the difference (39) is far inside the
  combined fitness noise (+/- ~117 plus the theory-19 identity artifact), so
  "signed found a stronger optimum" is NOT established. The honest summary:
  allowing negative weights changed which near-noise garnish the climber kept,
  not the substance of the optimum.
- **The capacity direction is decisively refuted at d4** (theory 18's
  play-strength half). Signed drastic resets proposed negative-chip candidates
  15 times, including the pure anti-material `c-80` six times (cached at 577
  +/- 71) and near-pure mixes (`c-77,b1,r2` = 456, `c-58,...` = 358): every
  negative-chip candidate scored 300-750 below the chip-positive band and none
  was accepted. No sampled candidate sat exactly on the capacity ray
  (forward:chip = 1:-7), so this is strong directional evidence rather than a
  targeted test of the ray itself.
- Climber inefficiency observed: the drastic reset re-proposed already-cached
  ids repeatedly (6 duplicate `c-80` proposals), wasting iterations. A
  "re-mutate on cache hit" tweak would recover them.

### d6 dethrone check

Both winners re-headed to the champion's `ab(d6,ord,nb200k)` and gauntletted on
the main pool (seed 7, 4 games/opponent), read against the same-run controls
(champion ID 1162 +/- 32; same-policy re-labeled equivalents 1064-1102, the
theory-19 artifact band):

| candidate (d6 head) | gauntlet Elo |
|---|---|
| A-best adv(t20,c75,h3,r2,g1) | 1191 +/- 33 |
| B-best adv(t20,c77,d-2,b1,g1) | 1139 +/- 31 |

A-best sits above the champion ID's own same-seed gauntlet (1162) and well
clear of the artifact band; B-best lands at the champion's pool rating. Both
were promoted to `ranking/roster.txt` and rated on the shared scale by a full
`rank.exe run --games 8` refit (984 new games, the permanent measure; gauntlets
are the noisy screen).

### Final standings and dethrone verdict

Post-refit shared scale (`ranking/ratings.tsv`, 2026-07-12), active agents:

| agent | Elo | cpu ms/move |
|---|---|---|
| d8/nb2m oracle (reference, not the target) | 1254 +/- 26 | 117.0 |
| champion `ab(d6,ord,nb200k).classic(t1,c4,w0,l0)` | 1135 +/- 20 | 9.8 |
| **B-best `...adv(t20,c77,d-2,b1,g1)`** | 1124 +/- 22 | 30.3 |
| best learned `...learned(s98)` | 1124 +/- 22 | 11.3 |
| **A-best `...adv(t20,c75,h3,r2,g1)`** | 1114 +/- 21 | 17.0 |

**Verdict: no dethrone, but instant top tier.** Both climb winners land in a
statistical tie with the champion (differences of 11-21 at combined SE ~30)
and with the best learned PST, and each went exactly **4-4 head-to-head with
the champion**. B-best also split **4-4 with the d8/nb2m oracle** (whose
overall record is 457-39, 92%) and went 5-3 over learned s98; A-best went 3-5
vs the oracle and 6-2 over s98. The newcomers pay 2-3x the champion's
cpu/move for parity (eff 76-79 vs the champion's 86), so on the Elo-per-CPU
pareto they do not displace it.

Oddity worth a follow-up: B-best's color profile is inverted vs the pool norm
(E1's White advantage): 181-67 as White (73%) but 234-14 as Black (94%),
including at least one win as Black over the oracle. No other top agent shows
this shape; if real, it is exactly the "distinctive opponent/color profile"
the roster curation policy wants preserved.

## Correctness gotchas discovered

- The evaluator registry order is load-bearing: `minimax_params.txt` persists the
  evaluator as a raw index, so Advanced had to be appended AFTER LearnedValue.
- Diagonal-pair local deltas double-count the shared source-dest pair on a
  diagonal move; it is provably 0 in both passes (destination empty-or-enemy
  before, source empty after), so the double count cancels -- same argument shape
  as the orthogonal case, documented at `supportNeighbors`.
- Mobility/overext local deltas cannot be per-square sums (shared affected cells
  double-count); a coordinate-defined bounding-box sum sidesteps the dedup.
- A move's two squares are never on the same home row (adjacent rows), which is
  what makes the hole term's affected-column logic per-square safe.
- `ranking/climb_roster.txt` carried stale `classic(...)@1` IDs from before the
  classic codec bump; refreshed to `@2` (the climber was unusable until then).
- The GUI smoke script's `-Build` step trips on PowerShell 5.1 stderr wrapping
  (`vswhere` warning); building via `cmd /c .\build_gui.bat` directly works.
- **Console sentinel collision** (caught by driving the console per TESTING.md):
  `main.cpp` seeded unset evaluator params with -1 and `getEvaluatorSettings`
  prompts only for params outside their [lo, hi] range. Advanced's negative
  minimums (-99) made -1 a VALID value, so the console silently skipped every
  prompt from Chip through Overext. Fixed with an out-of-band `EVAL_PARAM_UNSET`
  (INT_MIN) sentinel in `globals.h`; verified by re-driving the console (all 16
  prompts fire, a negative weight entry lands in the right slot).

## Known limitations

- With Advanced selected, the GUI config panel renders 16 stepper rows and pushes
  the widgets below it far down; on a default-height window some fall off-screen
  (the window is resizable, so they remain reachable). A two-column or compact
  layout for paramCount > 8 is a contained follow-up.
- Gauntlet comparisons between near-identical agents carry the identity artifact
  above; use pairgen (byte-level) or full pool refits for such comparisons.

## Future Work

- **Per-term incremental routing** (from the ladder pricing): let each term
  declare whether it is maintained in `g_evalPos` or recomputed at the leaf,
  chosen by which terms are enabled, so sparse mixes (chip+mobility) stop paying
  the delta overhead. Would confirm/extend theory 16's scope qualifier.
- **Gauntlet identity artifact**: quantify the two mechanisms separately
  (ID-derived game seeds vs cross-game TT/killer/history persistence) by adding a
  `--reset-state` flag that clears search state between games and re-running the
  three equivalent IDs; settles how much of the ~95 Elo gap each contributes,
  and whether gauntlet fitness noise materially misleads the hill climber.
- **RaceWin at deeper margins**: the detector provably decides races, but at d6
  the search already sees most of them; test at shallow depths (d2-d4 heads,
  where the detector effectively adds plies) and in time-budgeted searches.
- **Bounded per-position noise**: implement the modulo-accumulator variant and
  re-test the tie-break hypothesis at champion scale, where PST noise failed.
- **Theory 18's open half**: correlate `capacityBlack-capacityWhite` (and the
  race differential) with stored game outcomes across `ranking/matches.jsonl`.
- **Are the climb winners' garnish weights real?** A-best's h3,r2 and B-best's
  d-2,b1 were each accepted on a single noisy fitness sample (SE +/- 80). A
  targeted pairgen head-to-head (garnished mix vs plain chip mix at equal head,
  `--open-plies` diversity, a few hundred games) would settle whether any of
  them carries signal, and doubles as the clean test of the E6 diagonal-support
  direction (d-2 negative).
- **Longer / multi-restart climbs**: 60 iterations with 3 accepts barely
  explores 13 dimensions, and each run took only ~4 minutes, so budget was not
  the constraint -- fitness noise was. Raising `-Games` (tighter SE per
  evaluation) is likely worth more than raising `-Iters`; a re-mutate-on-cache-
  hit tweak recovers the wasted duplicate proposals.
- **Climb at the champion head**: the climb optimized at d4 and transferred to
  d6 well here, but the identity artifact and depth transfer are confounded; a
  (slower) d6-head climb with `-Head "d6,ord,nb200k"` would remove the
  transfer question.
- **B-best's inverted color profile**: 73% as White vs 94% as Black over 496
  refit games contradicts the pool-wide E1 White advantage. Check whether the
  d-2/b1 terms interact with tempo (an opener-swap run of B-best vs the
  champion would separate position bias from a real Black-side skill), and
  whether the profile persists at larger n. If real, B-best is a natural
  counter-agent candidate for the pool.

## Ideas This Inspired

- A "term cost table" auto-derived by the ladder (one row per single-enabled
  term) could gate CLIMBER mutations: skip mixes whose predicted us/move exceeds
  a budget, optimizing Elo per compute rather than raw Elo.
- The RaceWin detector generalizes: any provably-decided-position class (e.g.
  D10 outposts with tempo accounting) can become a sentinel check; a library of
  such detectors is effectively a hand-written tablebase edge.
- Noise-seed ensembling: rostering the same mix under several noise seeds gives
  a cheap agent-diversity family for the pool (deterministic, distinct IDs,
  distinct play), useful for the roster-curation goal of behavioral variety.
- The per-game-seed-from-ID mechanism could take an explicit salt so identical
  policies can be A/B measured on IDENTICAL opponent game streams.
