# PST Training Hyperparameter Sweep: Results

Session date: 2026-07-06. Companion to the incremental ML eval work
(incremental-ml-eval-plan-1-luminous-snail.md); this stream had no separate
written plan, the grid design was agreed in-session. Raw per-candidate table:
models/sweep/report_v2.csv (gitignored, regenerable).

## What was run

78 sparse piece-square (feature v2) linear value models were trained and rated
in ONE unified Bradley-Terry fit alongside the existing 54-agent pool (132
agents, 4 games per pair, sharded across 12 workers, ~28.9k new games now in
ranking/matches.jsonl as permanent history). Every candidate used the same
screening wrapper ab(d4,tt,ord,nb200k) so only the model differed. Error bars
came out at +/-16 Elo. Five groups:

- A (36): teacher depth {2,4,6} x games {100,250} x constant dilution
  {0.1,0.2,0.3} x 2 training seeds
- B (12): dilution DECAY schedules (start {0.2,0.3,0.4} decaying to
  {0,0.05} over {15,30,50} plies) x 2 seeds, at depth 4 / 250 games
- C (6): two independent 3-generation self-play bootstrap chains (heuristic-
  taught gen0, then each generation taught by the previous model)
- D (12): replay-based training via the new rank.exe extract (sample
  {1000,3000,8000} games from the real match history) x feature v1/v2 x 2 seeds
- E (12): L2 weight decay {0.001,0.005,0.01} x teacher depth {4,6} x 2 seeds

## Findings, in order of importance

1. **Training-seed noise dominates every hyperparameter.** Identical configs
   differing only in training seed spread 50-150 Elo, worst case 196
   (d6/g250/dil0.2: 394 vs 590), an order of magnitude above the +/-16 rating
   error. Any single-seed conclusion from this pipeline is noise. Replication
   was the most valuable axis in the grid.
2. **More training data helps modestly; nothing else clearly does.** 100 -> 250
   games averaged +38 Elo (498 -> 536), the direction holding in all three
   teacher-depth strata. Group D echoed it (8k-game samples ~+20 over 1k/3k).
3. **Teacher depth does not matter (d2 ~ 516, d4 ~ 511, d6 ~ 525).** A depth-2
   teacher's outcome labels are as good as a depth-6 teacher's for this model
   class. Do not spend compute on deep teachers for linear PST training.
4. **Dilution decay looks promising but unproven.** Group B averaged ~+30 over
   matched constant-dilution cells and produced the sweep's best model overall
   (651 +/- 16: start 0.3, floor 0.05, 30 plies, seed 1001). Within seed noise,
   but the top-of-group concentration makes it the best current default.
5. **Replay extraction from the rank pool matches or beats bespoke self-play**
   (group D mean ~569 vs group A ~517) at zero generation cost, confirming the
   idea that motivated it: the existing rated, diverse agent pool is a better
   free data source than a single hand-picked teacher. v1 vs v2 features made
   no difference to strength (578 vs 559, within noise).
6. **Self-play bootstrapping neither compounds nor collapses at this scale**
   (chain 1: 430 -> 565 -> 517, chain 2: 471 -> 430 -> 483). Not worth pursuing
   until the seed-noise problem is addressed (bigger data or model class).
7. **L2 is indifferent** (means ~537 vs ~502 matched baseline, no dose-response
   across three strengths).
8. **The representation is the ceiling, not the recipe.** The best learned
   model (651) still sits far below its own search wrapper with the Classic
   evaluator (ab(d4) classic rated 934 in the same fit). Recipe tweaks move
   +/-50 Elo; the linear PST class itself is the binding constraint. The next
   real lever is model capacity (MLP/NNUE on the same incremental seams), not
   more hyperparameter search.

## d6 confirmation

The sweep winner (slot45) was re-gauntleted in the d6 champion wrapper
(ab(d6,tt,ord,nb200k)) against the restored 54-agent pool, the same protocol as
the two earlier PST gauntlets (595 and 665): result recorded below when the run
completed.

- Result: **Elo 855 +/- 27** at d6 vs the restored 54-agent pool, versus 595 for
  the original PST and 665 for the depth-6-teacher variant under the identical
  protocol. The d4-screening winner also wins at d6 (no rank flip at the top),
  and the learned agent moved from "d6 search plays like classic d2" to "d6
  search plays like classic d4". The winner was promoted to the committed
  models/pst_value.txt (hash d675341a, identity learned(s2,d675341a)) and a
  ready-to-enable off line was added to ranking/roster.txt.

## Infrastructure shipped for this study (all tested, 455 assertions)

- ML_SLOTS 8 -> 128; ranking slotFile() slots 3+ map to models/sweep/slot<N>.txt
  so many candidates hold permanent identities in one process.
- rank.exe extract: deterministic replay of sampled matches.jsonl games into
  labeled training data (with a determinism-drift guard that discards any game
  whose replayed result mismatches the stored result).
- train.exe selfplay-supervised new flags: --l2, --gen-random-floor +
  --gen-random-decay-plies (linear dilution decay), --gen-model +
  --gen-model-explorer (self-play bootstrap), --from-data (fit on extracted
  replay data).
- tools/sweep_pst_v2.ps1: the 5-group orchestrator (train -> hash -> roster
  append with idempotency guard -> one unified rank run -> findings CSV).
  tools/sweep_pst.ps1 is the earlier, simpler gauntlet-based version.

## Lifecycle / repo hygiene decisions

- The 78 sweep roster lines were REMOVED after the study (git checkout of
  ranking/roster.txt): their model files live in gitignored models/sweep/, so
  keeping them would break roster validation on a fresh clone and schedule
  thousands of dead pending games in every future rank run. Their ~28.9k games
  remain permanently in ranking/matches.jsonl; the fit reports them as retired.
- models/sweep/ and the extract scratch files are gitignored; the durable
  artifacts of the study are this document, report_v2.csv's numbers quoted
  here, and the match history.

## Follow-up: data-scaling study (tools/train_scaling.ps1, same session)

A second study pinned everything the sweep said does not matter (d2 teacher,
decay 0.3 -> 0.05 over 30 plies, no L2) and scaled the data. Results in
models/sweep/scaling.csv, same d4 screening protocol:

| arm | data | Elo (per seed) |
|---|---|---|
| self-play | 250 games | 536, 442 |
| self-play | 500 games | 541, 469 |
| replay (extract) | 4000 games | 708, **813** |
| replay (extract) | 8000 games | 691, 695 |
| epoch probe (12 vs 6) | 500 games | 496, 501 |

Findings:

1. **Replay training crushed single-teacher self-play: ~+250 Elo at screening.**
   The best cell (replay 4000, seed 2002) confirmed at **d6 Elo 920 +/- 28**,
   beating the sweep winner's 855. Promoted to models/pst_value.txt (hash
   59815079). Notably the replay source had grown to ~46k stored games
   including the 78 sweep models' own games, a far more diverse teacher pool
   than the sweep-era store where group D scored only 524-607: the sweep
   improved the training data as a side effect of playing it.
2. **Two caveats on that number.** It is a selected max (winner's curse), and
   it was trained on the pool's own games then rated against that pool, so part
   of it may be style-fit. The held-out-agent probe (todo.md) is now the
   priority instrument, not optional hygiene.
3. **The self-play convergence stop triggered on noise, not on convergence.**
   With 2 seeds and 50-95 Elo seed spread, a 20-Elo threshold on a mean-of-2 is
   unresolvable: the loop stopped at 500 games on a "gain" of 16. Variance
   reduction (weight symmetrization, seed-averaging) is a prerequisite for
   measuring the scaling curve properly; do not trust "self-play plateaus at
   500" beyond d4-screening resolution.
4. **8000-game replay scored below 4000 (693 vs 760 mean).** Within noise, but
   consistent with dilution of data quality as extraction reaches deeper into
   the store's weaker/older games; Elo-weighted or min-Elo extraction (todo.md)
   is the designed follow-up.
5. **Twelve epochs gained nothing over six at 500 games** (496/501 vs 541/469),
   consistent with the standing overfit guidance even at moderately larger data.

## Methodology caveats

- 4 games per pair per candidate is thin for any single pairing; the +/-16
  bars are only valid within this one joint fit.
- All candidates were screened at d4. The earlier session showed eval quality
  and search depth interact, so d6 orderings could differ; only the winner was
  re-checked at d6.
- Group means above are simple averages over heterogeneous cells; treat them
  as directional, not precise effect sizes.
