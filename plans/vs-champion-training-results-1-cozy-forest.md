# Vs-Champion Training Study - Results (companion to vs-champion-training-plan-1-cozy-forest.md)

Session date: 2026-07-06/07. Everything below was measured on `boards/board1.txt`
with the pinned champion `ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`.

## Headline

The champion was NOT dethroned, but the gap collapsed. Before this study the best
learned model sat ~160 Elo below the champion (920 vs 1077 in the old fit). After
it, the best learned model (`learned(s98)`, trained on oracle-vs-champion games)
rates a statistical tie with the champion on the shared scale: 1137 vs 1140, with
standard errors of about +/- 22 each, at roughly equal per-move CPU (11.6 vs 9.9
cpu-ms/move) and fewer nodes (47k vs 71k). The new #1 overall is the d8 oracle
itself (1267), which is a reference point, not the goal agent.

The single biggest surprise: training data from a DILUTED CHAMPION playing its
clean self was the best-or-equal data source tested, beating the established
replay recipe, with strikingly low seed variance.

## What was built (infrastructure, reusable)

- `rank.exe pairgen`: fresh games between any two canonical IDs, emitted in the
  `train.exe --from-data` format with a `.meta.json` recipe/tally sidecar.
  Per-side dilution override (`--dil-apply a|b|both|none`, linear decay
  start->floor), `--open-plies` random openings, `--filter winner=a|b`,
  `--branch-tries` branch-from-win mining, `--shard/--of` process sharding.
  Deterministic: same IDs + seed reproduce byte-identical files.
- `tools/train_vs_champion.ps1`: the resumable 10-arm study harness (generation,
  training cells, d4 gauntlet screening into `models/sweep/vs_champ.csv`, gated
  bootstrap, promotion to reserved slots 94..99, d6 confirms, roster append,
  full re-rate, opponent-bucket analysis). `-AnalysisOnly` re-runs the bucket
  tables at any time, which is the standing longitudinal theory re-check.
- `teacher=` provenance now includes the self-play dilution recipe
  (`dil(0.3->0.05/30p)`), closing the recorded provenance gap.
- 64 new test assertions (dilution schedule, pairgen determinism, zero-override
  equivalence, filter tally honesty, open-plies divergence, branch determinism).
  Suite total: 501 assertions, all passing.

## Generation tallies (label skew per dataset)

| Dataset | Games | A record (W-L-D) | Positions |
|---|---|---|---|
| pstd2 vs champ (dil A) | 4000 | 14-3986-0 | 151,354 |
| classicd2 vs champ (dil A) | 4000 | 35-3965-0 | 133,320 |
| champdil vs champ (dil A) | 4000 | 983-3017-0 | 164,963 |
| oracle vs champ (open 6) | 2000 | 1437-563-0 | 98,999 |
| champloss (winner=a filter) | 4000 played | 950 kept | 43,932 |
| branch (filter + 4 tries) | 2000 played | 479 base + 879/1916 branches kept | 22,373 + 23,571 |
| bootstrap gen (s93 model d2 vs champ) | 4000 | 168-3832-0 | 177,382 |

Notes: the d8/nb2m oracle beats the champion 71.9% even from random 6-ply
openings. The diluted champion copy beats its clean self 24.6% of the time. The
branch re-win rate was 45.9% (879 of 1916 attempts), roughly doubling the
champion-defeat position pool. A d2 learner, even one whose weights later tie
the champion at d6, wins under 5% at d2 (168-3832), so bootstrap generation
produced heavily skewed labels.

## Screening results (d4 wrapper gauntlet, 4 games/opponent)

| Arm | Seed Elos | Mean |
|---|---|---|
| champdil-vs-champ | 825, 825, 809 | 820 |
| oracle-vs-champ | 792, 738, 825 | 785 |
| replay-4k (baseline) | 708, 821, 708 | 746 |
| mix-50-50 | 704, 746 | 725 |
| bootstrap (gated, ran) | 630 | 630 |
| selfplay-4k (control) | 516, 570 | 543 |
| pstd2-vs-champ | 541, 579, 475 | 532 |
| branch-wins | 485, 536 | 511 |
| champloss-only | 506, 496 | 501 |
| classicd2-vs-champ | 459, 469 | 464 |

Determinism artifact worth knowing: replay-4k seed 2002 reproduced
`models/pst_value.txt` byte for byte (hash 59815079), because it used the same
data file and seed as the scaling study. Training is fully deterministic.

## d6 confirms and final shared-scale fit

| Family (slot) | d6 gauntlet | Full-fit Elo | cpu ms/move | Source arm/seed |
|---|---|---|---|---|
| oracle (s98) | 1137 | 1137 | 11.6 | oracle-vs-champ / 3003 |
| dilution (s96) | 1153 | 1121 | 11.7 | champdil-vs-champ / 2002 |
| replay baseline (s99) | 934 | 1064 | 10.0 | replay-4k / 2002 (= old pst_value) |
| theory1 (s94) | 987 | 983 | 11.7 | mix-50-50 / 2002 |
| bootstrap (s95) | 889 | 941 | 10.8 | bootstrap / 1001 |
| branch (s97) | 683 | 805 | 9.5 | branch-wins / 2002 |

Reference points on the same fit: oracle agent 1267 (rank 1), champion 1140
(rank 8, cpu 9.9 ms/move). The champion's own rating rose from 1077 to 1140
during the final re-rate because ~5,700 pending games (from the earlier @2
version bump) were finally played, so old-fit and new-fit numbers are not
directly comparable. Compare only within the new fit.

## Theory verdicts

**Theory 1 ("a champion-only model does fine overall now, but new diverse bots
will beat it"): REFUTED in the current pool, longitudinal half still open.**
The bucket residuals (actual minus Elo-expected score per game):

| Agent | champion (n=8) | classic-like (n=392) | diverse (n=80) |
|---|---|---|---|
| dilution s96 | -0.098 | +0.001 | +0.123 (76-4) |
| oracle s98 | -0.246 | +0.026 | +0.023 |
| theory1-mix s94 | -0.038 | +0.016 | +0.001 |
| replay baseline s99 | -0.142 | +0.031 | -0.031 |

The champion-only-trained agents (s96 trained purely on champ-vs-champ games,
s98 purely on oracle-vs-champ games) show NO weakness against the diverse
bucket. s96 actually overperforms there (+0.123, going 76-4). The predicted
out-of-distribution fragility did not appear. Caveats: the current diverse
bucket is small and mostly weak (the 78 retired sweep models play no new
games), and a linear PST may be too low-capacity to overfit a distribution the
way the theory assumes. Re-run `tools/train_vs_champion.ps1 -AnalysisOnly`
after the next batch of diverse agents joins the pool.

**Theory 2 ("you'll never beat the champion by playing against a random
dilution of the champion; oracle/branch data are the real counter-data"):
REFUTED, on a follow-up head-to-head re-measurement (see addendum below).**
The original n=8 in-pool sample (dilution agent 3-5) was too noisy to call,
and turned out to also be measuring mostly-repeated deterministic games (see
addendum). A dedicated 80-game head-to-head with randomized 6-ply openings
gives the dilution agent (s96) a real 50-30 (62.5%) record against the clean
champion at d6 -- dilution-sourced data DID produce a genuine champion-beater,
contrary to the theory.

## Addendum: champloss-only follow-up (why did it fail so badly?)

Prompted by "did champloss-only at least beat the champion head-to-head, and
was it a data-volume problem?" Two follow-up experiments, run directly against
the archived per-seed models (`models/sweep/vsc_<phase>_<seed>.txt`, which
`RunCell` archives before its slot cycles, so any cell from the study can be
re-examined after the fact).

**Methodology fix discovered along the way.** The first head-to-head attempt
(`rank.exe pairgen --a <model> --b <champion> --games N`, no `--open-plies`)
gave champdil-d4 an exact 120-120 split over 240 "games". That exactness was
the tell: with no dilution and no random opening, both agents are fully
deterministic, so a repeated pairing only ever produces 2 distinct games (one
per starting color), replayed N/2 times each. All head-to-head numbers below
use `--open-plies 6` so each game starts from an independent random position,
making the sample size real. This is a general lesson for anyone using
`pairgen` to evaluate (not train) a deterministic agent: always pass
`--open-plies` or a dilution override, or the "N games" figure is misleading.

**Real head-to-head results (randomized 6-ply openings, both colors):**

| Model | vs champion, d4 (200 games) | vs champion, d6 (80 games) |
|---|---|---|
| champloss-only (s85, seed 1001) | 0-200 | 0-80 |
| champdil-vs-champ (s96, the rostered dilution model) | 59-141 (29.5%) | 50-30 (62.5%) |

champloss-only did not win a single game out of 280, at either depth. This
directly answers "did it learn to beat the champion head-to-head": no, cleanly
no. The rostered dilution model, by contrast, has a real winning record
against the champion at d6 (though it loses more often than it wins at the
shallower d4 depth it was trained/screened at).

**Data-volume test.** champloss-only used only 43,932 positions (950 kept
games out of 4000 played, `--filter winner=a` keeping only games the diluted
side won), versus champdil's 164,963 positions from the same generator with no
filter -- 27% of the data. To isolate volume from the filter itself, ~92,000
more `--filter winner=a` games were generated (seed 505, 8-way sharded) and
concatenated with the original, yielding 180,793 positions (4.1x, matching
champdil's scale). Retrained with the same recipe (epochs 6, lr 0.05, seed
1001) and re-screened:

- d4 gauntlet Elo: 501/506 (original 950-game models) -> 547 (4x-data model).
  A real but small improvement (~40-45 Elo), nowhere near champdil's ~820 mean.
- Real head-to-head vs champion, d4, 200 games, randomized openings: still
  **0-200**. Quadrupling the data did not produce a single win.

**Conclusion: not a data-volume problem, not underfitting.** Training loss
decreased normally in both fits (the model fits the data it's given fine), so
this is not underfitting in the classical sense. The problem is what the
filtered dataset IS: every position comes from a trajectory where the
eventual winner played materially weaker (diluted, up to 30% random) moves
early and still won. The labels are truthful (real game outcomes, not a
labeling bug), but the SAMPLE is skewed toward positions that look tactically
compromised by the standard chip/structure/forward features yet are secretly
still winning. A linear model has no way to localize "this is one of those
rare recoveries" as a conditional pattern -- it can only fold the signal into
one global linear function, which drags down the evaluation of genuinely
strong positions everywhere. That plausibly explains both the near-random
head-to-head play and the ~500 Elo overall floor: the model isn't random, it's
systematically miscalibrated. More of the same biased sample does not fix a
systematic bias, which is exactly what the 4x-data result shows.

**On your specific follow-up ideas:**
- *More data*: tested directly above, ruled out as the primary fix.
- *More parameters (MLP/NNUE)*: plausible but untested. A higher-capacity,
  nonlinear model could in principle learn "recovery positions look like X
  AND Y" as a conditional exception instead of smearing the signal into one
  global slope, which the linear class structurally cannot do. Worth trying
  if the cherry-pick idea is revisited, but not with the current linear PST.
- *Current ply as a feature*: not recommended. Ply doesn't tell the model
  whether a given position sits on a "diluted-then-recovered" trajectory
  versus normal play, so it doesn't address the actual distribution-mismatch
  mechanism above. It would likely just let the model fit an unrelated
  ply-dependent prior.
- **Better default recommendation**: don't filter data to one-sided outcomes
  at all. If champion-specific counter-training is wanted, prefer the
  champdil recipe (full, unfiltered dilution data -- which already produced
  the study's best head-to-head result) or, if the win-only data is valuable,
  mix it into a balanced dataset (e.g. sample-weight the wins rather than
  excluding the losses) instead of an exclusive filter.

## Other lessons

- **Degenerate labels sink cherry-picked data.** champloss-only and branch-wins
  train on games where one fixed side always wins, and both arms landed at the
  bottom (~500 screen). The value model needs both outcomes in the data far
  more than it needs champion-specific content. If counter-data is revisited,
  mix it with balanced data instead of training on it alone.
- **Strong play on BOTH sides beats a strong teacher on one side.** The two top
  arms (champdil, oracle) are the two whose weaker side is still a d6+ search.
  The arms with a d2 generator on one side (pstd2, classicd2, bootstrap) all
  landed at or below the self-play control, despite the champion supplying
  half the moves. Generation quality seems to be bottlenecked by the WORST
  player in the game, not the best.
- **Bootstrap did not help** (630 vs its parent's 825): the d2-model generator
  lost 96% of its games, reintroducing the skewed-label problem.
- **Ratings drift when pending games fill in.** The champion gained 63 Elo in
  the final catch-up re-rate. Treat cross-fit Elo comparisons as invalid.
- The oracle agent costs ~12x the champion's CPU (116.5 vs 9.9 cpu-ms/move) for
  +127 Elo, consistent with the known budget-vs-Elo curve.

## Roster changes

7 agents appended `on`: the six promoted family models
(`ab(d6,tt,ord,nb200k)@1.learned(s94..s99,<hash>)@1`) and the oracle
(`ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2`, a reference, explicitly not the
target). Roster curation policy adopted this session (also recorded in
todo.md): keep the anchor, the ladder, the champion family, one oracle, the
best per data-source family, and distinctive-profile agents; retire
near-duplicates. Candidate retirements after the next study: s95 (bootstrap)
and s97 (branch) if they stay uninteresting in the buckets.

## How to reproduce / continue

- Full study: `.\tools\train_vs_champion.ps1` (resumable, skips finished cells
  in `models/sweep/vs_champ.csv`, reuses generated datasets in `data/`).
- Bucket tables only: `.\tools\train_vs_champion.ps1 -AnalysisOnly`.
- Datasets carry their recipes in `data/*.meta.json`.
- Tests: `.\tools\run_tests.ps1 -Build` (501 assertions).

## Follow-ups this study motivates

1. Close the last 3 Elo honestly: more games per champion pairing (the 8-game
   target leaves the top statistically tied), then a dedicated
   champion-vs-s98/s96 series.
2. The champion-refutation opening book (todo.md): the strongest remaining
   dethrone idea that also REDUCES live compute.
3. Oracle-labeled positions (eval-blended labels, already `[Now]` in todo.md):
   this study only used oracle GAMES with outcome labels; the oracle's root
   scores per move are still being thrown away.
4. Capacity jump (MLP/NNUE) now has a proven best data recipe: champdil +
   oracle pairgen data.

## Candidate commit messages

All uncommitted changes from this session belong together (pairgen + tests +
study script + docs + study outputs):

1. `Add rank.exe pairgen + vs-champion training study; oracle/champdil data ties the champion (1137 vs 1140)`
2. `Add pair-play data generation (pairgen), branch mining, and the 10-arm vs-champion study; best learned d6 now ties the champion`
3. `Add pairgen infrastructure + vs-champion study: champion-sourced data closes the learned-model gap from -160 to -3 Elo`

Top recommendation: message 1.
