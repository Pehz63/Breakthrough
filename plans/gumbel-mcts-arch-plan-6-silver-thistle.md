# Gumbel-Zero: mlp/conv architecture sweep -- Plan

## Goal

The joint sweep (`gumbel-mcts-plan-5-violet-harbor.md`, Round 5) covered only
the linear architecture, from-scratch init -- `train.exe gumbelzero` had no
`--model-type`/`--mlp-hidden`/`--conv-channels` flag when that study launched.
Those flags now exist (added and unit-test smoke-tested in a prior session,
commit `586ad8f`), so this round covers the deferred architecture axis: mlp
(both value and policy heads) and conv (value head only, policy head stays
linear -- `src/ml_model.h`'s `ConvModel` doc comment). Linear itself is not
redrawn here; Round 5 already covers it exhaustively (404 checkpoints).

## Design

Random search (Bergstra & Bengio 2012, `Docs/works-cited.md`), not
one-axis-at-a-time. Every draw fixes 11 axes at once: model type (mlp/conv),
the matching architecture axis (`--mlp-hidden` for mlp, `--conv-channels` for
conv), the same 9 shared training/search-shape axes Round 5 used (for direct
comparability), sampled from Round 5's exact eligible sets:

**Training axes** (passed to `train.exe gumbelzero`):
| Axis | Eligible values |
|---|---|
| sims | 300, 400, 500 |
| lr | 0.003, 0.01, 0.03 |
| l2 | 0.0, 0.0003, 0.001 |
| replay capacity/warmup | (500,16), (2000,32), (8000,128) |
| batch size | 8, 32, 128 |
| open plies | 0, 4, 8 |

**Search-shape axes** (applied only at the certification head
`gaz(sims=N,cvisit=C,cscale=S,m=M)@1`, not passed to the trainer):
| Axis | Eligible values |
|---|---|
| cvisit | 400, 600, 800, 1000 |
| cscale (tenths) | 40, 70, 100, 130 |
| m | 8, 12, 16, 20 |

**Architecture axes** (new this round):
| Axis | Eligible values |
|---|---|
| mlp hidden layers | 16 / 32 / 64 / 64,32 |
| conv channels | 8,8 / 16,16 / 16,16,16 / 32,32 |

Conv's own FC head (`--mlp-hidden`, reused by the trainer for that purpose)
is fixed empty this round, not swept -- a second axis on top of
`--conv-channels` would widen an already-11-axis draw further; sweeping the
FC head jointly with channel depth is a natural follow-up, not this round's
scope. Conv's kernel is a fixed 3x3, no stride/pooling (`src/ml_model.h`).

`sims` is sampled once per draw and used for both self-play generation and
the certification head, matching Round 5's convention and
`Docs/model-training-playbook.md`'s generator/search-depth guidance.

**Stratified sampling.** Rather than a coin-flip per draw between mlp/conv
(expected but not exact 50/50), the sampler draws two separate blocks
(`M1..M45`, `C1..C45`) from one continuing, deterministically-seeded RNG
stream (`-SweepSeed 8917`), giving an exact 45/45 split. Plus one REF anchor
per architecture at that architecture's own built-in trainer default
(mlp-hidden=32; conv-channels=16,16), shared axes matching Round 5's REF
(sims=300, lr=0.01, l2=0.0, replay=(2000,32), batch=32, open=4, cvisit=600,
cscale=70, m=16). 92 draws total (2 REF + 90 random), 4 shared checkpoint
rungs (100/400/1500/4000 games, same as Round 5) = 368 checkpoints.
`tools/gumbelzero_arch_sample.ps1`'s `Get-GumbelZeroArchDraws` is the single
source of truth for the draw list.

**1 seed per draw** -- developer-confirmed breadth-over-reliability trade for
this round (same choice Round 5 made), via `AskUserQuestion` before launch.

## Rating

Pool-only gauntlet screening (`rank.exe gauntlet --keep`, sharded per-worker,
merged after all shards exit cleanly), a pinned fit against the frozen
`ranking/roster_screening_pool.txt` -- same mechanism as Round 5, reused
unchanged, for the same reason (cohort-vs-cohort play is quadratic in cohort
size and becomes prohibitively expensive at this cohort scale).

## Slot range

Slots 1402-1769 (368 checkpoints). Round 5's joint sweep owns 998-1401
(`src/CLAUDE.md`'s slot ledger); cross-checked against `.gitignore`'s slot
exception list (highest exception-listed number 707) and the locally-trained
slot files (highest 1401) before claiming this range, per the ledger's own
warning about a prior real collision between two claimed ranges.

## Pass-1: timing check and a bug it caught

A 2-draw (REF_MLP + REF_CONV, 8 checkpoints) timing check ran the full
train->roster->play->screen->export pipeline before committing to the full
92-draw sweep, per `Docs/model-training-playbook.md`. It caught a real ledger
bug: `--mlp-hidden`/`--conv-channels` values like `"16,16"` contain a comma,
and the study driver's ledger-row writer joined fields with plain unquoted
commas, so the embedded comma silently shifted every later column (rung,
slot, model path) on re-read via `Import-Csv`. This broke both `$done`
resumability and `export_cohort_results.ps1`'s join -- the timing check's
4 REF_CONV rows exported with every result column blank, flagged by the
export step's own "N of M ledger rows have no matching standings row"
warning. Would have corrupted every conv draw and any `"64,32"` mlp draw
(roughly half the full sweep) had it reached production un-caught. Fixed by
CSV-quoting the field at the write site (`tools/gumbelzero_arch_study.ps1`);
the already-written REF_CONV rows were repaired in place; re-running the
export phase confirmed all 8 rows populated cleanly with no warning.

**Timing projection and the 2-hour/1-day rule.** The check measured 8085s
(2.25hr) for 2 draws / 8 checkpoints at Workers=8. Scaled to the full
92-draw / 368-checkpoint sweep at Workers=12 ((368/12)/(8/8) = 30.7x), that
projected ~68.9hr (~2.9 days) -- over the 1-day threshold. Per the
2-hour/1-day rule, this was surfaced to the developer via `AskUserQuestion`
(run the full 92-draw sweep as designed, vs. reduce to N=15 to fit under a
day); developer chose to run the full sweep as designed.

## Worker count: leave 2 cores free

Mid-run, the developer asked to cap `-Workers` at 10 (not 12) on this
12-logical-core machine, so 2 cores stay free for other use, and asked that
this become the standing default rather than a flag to remember each time.
The running Workers=12 sweep was stopped cleanly (confirmed no orphaned
`train.exe`/`rank.exe` processes -- `Start-Process`-spawned children don't
die automatically when the parent script is killed) and relaunched at
Workers=10, resuming correctly past the already-trained REF_MLP/REF_CONV via
the (now-fixed) ledger. Every parallel study script's `-Workers` default was
then changed from a hardcoded number to `[Environment]::ProcessorCount - 2`
(`tools/CLAUDE.md`), except `run_rank.ps1`/`sweep_pst_v2.ps1`, whose
`Workers=1` default is for clean, unshared ms/move timing, unrelated to core
count.

See `gumbel-mcts-arch-results-6-silver-thistle.md` for outcomes.
