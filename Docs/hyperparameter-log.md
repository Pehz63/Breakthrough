# Hyperparameter log

A cross-study reference of hyperparameter values actually tried in this
project, by training regime. Purpose: before designing a new sweep, check here
for what's already been tried and what it found, instead of re-guessing a
range from nothing. Consult this when filling in `Docs/model-training-playbook.md`'s
Pass-2 grid design step.

This is a growing reference, not a force-read document. **Status: skeleton
only.** Backfilling it from the project's existing studies is tracked in
`todo.md`. Only the TD-Leaf section below is populated so far, since that
study's data was at hand while writing this.

Format per regime: one table, one row per hyperparameter, with every distinct
value actually run, what it found (one line, with a pointer to the doc that
has the real numbers), and a rough judgment of whether the value is settled or
still open.

---

## `tdleaf` (`train.exe tdleaf`, `src/ml_tdleaf.cpp`)

> **Every value in this table was measured on 2026-07-30, before the transposition
> table gained its searcher-context salt (`06e4738`, 2026-08-27). Those runs are
> unaffected: they predate the salt. Any tdleaf run made BETWEEN 2026-08-27 and
> 2026-09-09 is not, because the PV walk probed the bare position hash, matched
> nothing, and trained on the position one ply after the root instead of on a
> depth-d leaf (mean PV depth 1.0 of 12, 100% truncated). No such run exists in
> `models/sweep/`. Fixed 2026-09-09, guarded by `tests/test_ml.cpp`.**

Source: `plans/tdleaf-results-1-amber-pangolin.md` (screening fit, 2026-07-29).

| Hyperparameter | Values tried | Finding | Settled? |
|---|---|---|---|
| `--lambda` | 0.0, 0.7, 1.0 | 1.0 (== outcome-supervised on PV leaves, by the unit-tested closed form) scored BELOW the untrained init -- worse than not training. 0.7 beat 0.0 at 500 games, roughly tied at 2000. 0.7 is the best point found so far | No -- only 3 points on [0,1], gap between 0 and 0.7 unresolved |
| `--lr` | 0.003, 0.01, 0.03 | n=1 each, inconclusive (see results doc block D) | No |
| `--l2` | 0.0 only | untested axis | No -- never varied |
| `--explore` | 0.0 only | untested axis (no forced exploration outside the opening window was ever used) | No -- never varied |
| `--depth` / `--node-budget` (generator) | d4 (no budget), d6/nb200k | indistinguishable at n=1 (-13, +2 vs the d6 base) | No -- does not support or refute theory 44, just insufficiently sampled |
| `--open-plies` | 4 only | untested axis | No -- never varied |
| `--init` | scratch, `models/pst_value.txt` (champion-trained linear) | champion-init reached higher Elo faster; scratch was still climbing at the last rung tested (2000 games) and had not converged | Partially -- champ-init clearly ahead within the tested range, scratch's ceiling unknown |
| game count (`--ckpt-at` rungs) | 100, 250, 500, 1000, 2000 | **interior optimum**: peaks at 1000 (+260 Elo vs init), declines to 2000 (-36, paired across all 4 seeds) | No -- ladder never extended past 2000, mechanism for the decline untested |
| `--model-type` | linear only | -- | No -- MLP never run under this regime |
| `--feature-version` | v2 (129, sparse piece-square) only, and only reachable via `--init` (the scratch path hardcodes v2 in the current code) | -- | No -- v1 (30, dense) never run; needs a small code change to reach from scratch |
| `--batch` | 1 (strictly online) only | -- | No -- the batched path is implemented and untested |

## `selfplay-supervised` (`train.exe selfplay-supervised`, `src/ml_train.cpp`)

Source: `plans/training-sweep-results-1-luminous-snail.md` (78-candidate sweep),
`models/sweep/scaling.csv`, `tools/train_scaling.ps1`, `tools/sweep_pst_v2.ps1`.

| Hyperparameter | Values tried | Finding | Settled? |
|---|---|---|---|
| `--lr` | 0.05 (the sweep/scaling default) | not itself varied as an axis in the 78-candidate sweep; used as a fixed constant | No -- never swept |
| `--l2` | 0.0 (default, per the sweep's Group E) | -- see the 78-candidate sweep's Group E for what it covered | Not backfilled yet -- see todo |
| `--model-type mlp --mlp-hidden` | 16, 32 (Group G, capacity comparison), 32 (residual-mlp study default) | capacity did not beat linear (theory 24 substrate); see `plans/residual-mlp-results-*` | Backfill full detail -- see todo |
| `--gen-depth` (teacher) | see the 78-candidate sweep | "teacher depth is irrelevant" (headline finding) | Backfill exact values -- see todo |
| self-play game count | 250, 500 (converge-stop artifact, see `Docs/corrections.md` `SELF-PLAY CONVERGENCE UNSUPPORTED`); 4000, 8000 (replay arm) | do not cite a ceiling from this arm | No -- corrupted stop rule, needs redoing |

*(Remaining hyperparameters of this regime -- `--gen-random`/`--gen-random-floor`/
`--gen-random-decay-plies`, `--residual-skip`, `--val-split` -- not yet
transcribed. See todo.)*

## `gumbelzero` (`train.exe gumbelzero`, `src/ml_gumbelzero.cpp`)

Source: `plans/gumbel-mcts-results-2-hidden-greeting-mist.md` (Pass 1 sanity,
2026-08-17), `plans/gumbel-mcts-results-3-amber-thicket.md` (Pass 2 broad
sweep, Round A + Round B + the compute-matched sims follow-up, 2026-08-17),
`plans/gumbel-mcts-results-5-violet-harbor.md` (joint training x
search-shape sweep, 2026-08-18: 101 draws, 1 seed, rungs 100/400/1500/4000,
`cvisit`/`cscale`/`m` drawn jointly with the training axes rather than swept
on one fixed checkpoint, linear architecture only), and
`plans/gumbel-mcts-arch-results-6-silver-thistle.md` (mlp/conv architecture
sweep, 2026-08-22: 92 draws -- 45 mlp + 45 conv + 2 REF -- same 9 shared
training/search-shape axes as results-5, plus `--mlp-hidden`/
`--conv-channels`, same rungs). Round A: 21 draws (random search, 1 seed,
rungs 100/400/1500). Round B: the top 8 Round-A draws seed-replicated x3 with
an extended ladder (100/400/1500/4000). All screening-level (pinned fit vs
`ranking/standings.tsv` for Rounds A/B, pool-only gauntlet vs
`ranking/roster_screening_pool.txt` for the joint and architecture sweeps --
see the results-5 doc for why; never certified against the unpinned pool).

| Hyperparameter | Values tried | Finding | Settled? |
|---|---|---|---|
| `--sims` (simulations/move) | 25, 50, 100, 200 (Round A draws); 25/50/100/200/400/800 compute-matched at one fixed config (theory 48) | **Interior optimum under fixed total training compute**: 585 (50) -> 646 (100) -> 673 (200) -> peak 683 (400) -> 617 (800), all +/- ~21; 25 sims separately ruled out (485 +/- 24). Round A's raw rung-matched table also shows sims rising with Elo, but that comparison is CONFOUNDED (higher sims costs more compute per game at a fixed game count) -- use the compute-matched result, not the rung-matched one | **Yes for the productive range: 100-400.** 25 too low, 800 too high, exact interior peak within [100,400] not pinned down |
| `--lr` | 0.003, 0.01, 0.03 (Round A) | Round A's raw group means look monotonic (386/421/572 rung-1500 pooled), but the lr=0.03 bucket also has 2.4x the mean `sims` of the lr=0.003 bucket (133 vs 56) in this single-seed random-search sample, so the apparent effect is substantially confounded with sims, not isolated | No -- confounded, needs a fixed-sims isolated sweep |
| `--l2` | 0.0, 0.0003, 0.001 (Round A); 0.0, 0.0003, 0.001 again, jointly with 8 other axes across 101 draws (results-5); same set again across 92 mlp/conv draws (results-6) | l2=0.0 wins outright in Round A (rung-1500 pooled means 595/422/362, monotonic) and survives a sims-confound check (bucket mean sims 94/69/83, not correlated with the l2 ordering). results-5's top-20 checkpoints (any rung, 404 total) are 19/20 l2=0.0 despite l2=0.0 being only 32/101 draws (~32%). results-6's top-20 (368 total, mlp/conv only) are 18/20 l2=0.0, 2/20 l2=0.0003, 0/20 l2=0.001 -- consistent direction a third time, still correlational (not an isolated sweep in any of the three) | Yes -- l2=0.0 is the best value found across three independent rounds and is the tested floor |
| `--replay-capacity` / `--replay-warmup` | (500,16), (2000,32), (8000,128) (Round A; same set, jointly-drawn in results-5) | No clean trend in Round A (rung-1500 pooled means 372/447/384 by capacity). Not separately broken out in results-5 (see that doc's caveat: 9 jointly-varied axes over 101 draws cannot isolate this one) | No -- flat within noise in Round A, not flagged for further sweeping |
| `--batch-size` | 8, 32, 64 (Round A); 8, 32, 128 (results-5, 64 dropped for 128) | No clean trend in Round A (rung-1500 pooled means 440/447/395). Not separately broken out in results-5 | No -- flat within noise in Round A, not flagged for further sweeping |
| `--open-plies` | 0, 4, 8 (Round A and results-5; grounded on `ranking/CHAMPION.md`'s openless/4-random/8-random categories, not arbitrary) | Weak monotonic trend in Round A, open=8 best (rung-1500 pooled means 388/446/464), but confounded with sims/lr in the same way as the lr finding above; capped at 8 by design since no serving category exists above it. results-5's top-20 skews toward open=8 (11/20) and open=0 (4/20) over open=4 (5/20), consistent direction but not independently isolated | No -- confounded, and not extendable past 8 without a new category |
| games / `--ckpt-at` ladder | Round A: 100/400/1500. Round B: 100/400/1500/4000 (top 8 draws, 3 seeds each). results-5: 100/400/1500/4000 (all 101 draws, 1 seed). results-6: same 4 rungs (92 mlp/conv draws, 1 seed) | Most Round-A draws were still rising at rung 1500 (12/21 monotonic rising). Round B's leader (R17) is still rising in 2 of 3 seeds even at rung 4000. results-5, at 4x Round A's draw count and the full 4-rung ladder on every draw, sharpens this: only 8/101 arms (7 up, 1 down) are monotonic in either direction, 93/101 (92%) are non-monotonic with an interior peak or dip, and the peak rung is roughly evenly spread across all 4 rungs (11/30/29/31 at 100/400/1500/4000) rather than concentrated late. The single best checkpoint found (results-5's R87, Elo 816) peaks at rung 1500 and drops to 604 by rung 4000. results-6 (mlp/conv) is non-monotonic MORE OFTEN THAN NOT but less extremely: 30/46 mlp and 34/46 conv arms non-monotonic (vs 92% for linear), peak rung skewed later (mlp 0/13/8/25, conv 1/6/17/22 at 100/400/1500/4000) but still 46-52% of arms peak before the final rung | No -- not a monotonic ceiling-approach at this scale in any architecture tested; screening only the final rung would have missed the best result found so far in both results-5 and results-6 |
| model architecture (both heads) | linear only (Pass 1 through results-5); mlp and conv added and swept (results-6, 2026-08-22, `--model-type`/`--mlp-hidden`/`--conv-channels`) | Pass-1 linear-only was a deliberate developer choice (faster convergence, proven not to saturate at this project's scale). results-6's 92-draw sweep (45 mlp + 45 conv, same shared axes as results-5) found mlp clearly ahead of both conv and results-5's best linear checkpoint: best-of-46 mlp 1023 +/- 20 Elo (rank #2 of 395 pool agents) vs best-of-46 conv 806 vs results-5's best linear 816. **Same-day seed-replication of the top 5 mlp blocks at 2 more seeds each found the winning single-seed number (1023) does NOT hold up** -- the original seed was highest of its own 3-seed group in 5/5 blocks (regression to the mean), true 3-seed mean for the top block is 971.7. The architecture-level gap survives seed correction (all 15 new seed x block points are >= 775, clearing conv's 806 in 14/15), only the specific extreme number does not. mlp hidden sizes 16/32/64/64,32 and conv channels 8,8/16,16/16,16,16/32,32 all tried; conv's own FC head (`--mlp-hidden`, reused for conv's post-flatten layer) was fixed empty, not swept | mlp vs conv: yes, mlp is clearly ahead at this scale (217-Elo gap between architecture maxima, well outside seed-noise band, population-level not single-seed, survives replication). mlp vs linear: suggestive (+207 Elo unreplicated, still positive after seed correction) but crosses fits, not confirmed. Exact best architecture depth/width, or any single recipe's exact Elo: no -- 1 seed per cell in the main sweep, confounded with the other 9 axes |
| `cvisit=` / `cscale=` / `m=` (root breadth / c_visit / c_scale, search-time not training-time) | exposed 2026-08-17 as per-agent `gaz(sims=N,cvisit=C,cscale=S,m=M)@1` roster knobs (default 50/1.0/16, the paper's own values). Swept same day (theory 49) on R17/rung4000 (slot746) at sims=200: `m` in {2,4,8,16,32} (free), then `cvisit` in {10,50,200,500,1000,2000} x `cscale` in {0.5,1.0,3.0,5.0,10.0,15.0} (m held at 16), then `m` re-isolated at the found corner, then the 4 leading candidates + REF rated together in one shared fit (Round 5). results-5 (2026-08-18) redrew cvisit in {400,600,800,1000}, cscale in {4.0,7.0,10.0,13.0}, m in {8,12,16,20} JOINTLY with training axes across 101 different checkpoints, rather than sweeping on one fixed checkpoint | `m<=2` costs 300+ Elo regardless of cvisit/cscale (not confounded, checked across a wide cvisit/cscale range at m=2); with m=16 fixed, cvisit and cscale both rise past the paper defaults; Round 5's shared fit found `cvisit=1000/cscale=10.0` (893), `cvisit=500/cscale=5.0` at m=16 (879) and m=8 (875) all statistically tied -- a broad plateau, not one point -- all beating REF (793) by 85-100 Elo. results-5's winner (cvisit=400,cscale=13.0,m=12) sits inside/near that plateau's range but on a DIFFERENT checkpoint than Round 5 tested, so it is weak evidence the plateau generalizes across checkpoints rather than a new, separately-confirmed point | Serving-time-only screening: yes, cvisit in [500,1000] / cscale in [5.0,10.0] / m in [8,16] beats the paper defaults on the ONE checkpoint Round 5 tested by ~85-100 Elo; recommended default `gaz(sims=200,cvisit=500,cscale=50)@1`. Not checked in isolation on a different checkpoint or sims value (results-5 varied it jointly with training, not as a controlled follow-up), so generality is still open |

## `dist-value` (`train.exe dist-value`, position-oracle pipeline)

Source: `plans/position-oracle-results-1-lazy-popping-simon.md`,
`plans/nnue-shaped-head-results-1-brisk-walrus.md`.

| Hyperparameter | Values tried | Finding | Settled? |
|---|---|---|---|
| `--lr` | 0.01 (nnue-shaped-head study) | -- | Backfill -- see todo |
| `--lr-sigma` | 0.002, 0.004 (default) | sigma head trains slower/noisier than mu by design | Backfill -- see todo |
| `--mu-hidden` / `--s-hidden` (MLP dist heads) | 128,64 (`dist_mlp_s*`); 512,8 (`dist_mlp_wide`, the NNUE-shaped-head study, theory 37) | wide/shallow (512,8) refuted on speed, neutral on strength vs 128,64 | Yes, for the wide-vs-balanced question (theory 37) |

*(Remaining position-oracle hyperparameters -- posgen/label/labelfit ladder
design, `--elo-se`, calibration sample size -- not yet transcribed. See todo.)*

## `hill_climb.ps1` (Advanced evaluator weight search, not a `train.exe` regime)

Source: `tools/hill_climb.ps1` header comment, `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`.

| Hyperparameter | Values tried | Finding | Settled? |
|---|---|---|---|
| step sizes (simplex mutation) | {1, 3, 5} units | -- | Backfill -- see todo |
| `-Drastic` (reset probability) | 0.3 (default) | -- | Backfill -- see todo |
| `-FlipProb` (sign-flip share, `-AllowNegative` mode) | 0.15 (default) | -- | Backfill -- see todo |
| non-negative vs signed search | both modes run | best mixes in both modes converged toward `w0,l0` (wall/column near zero) | Partially -- see `Docs/axioms.md` E6 |

---

## Notes for anyone filling this in (see the todo item)

- Pull real values from the cited results docs and scripts, not from memory --
  this doc's entire purpose is to be more reliable than remembering.
- A value that was only ever the DEFAULT (never deliberately varied) should
  still get a row, marked "untested axis" -- knowing what was never tried is as
  useful as knowing what was.
- Link every row to the doc it came from.
