# Gumbel-Zero: joint training x search-shape sweep -- Plan

## Goal

The prior search-shape study (`gumbel-mcts-results-4-copper-lantern.md`, Round
5) screened `cvisit`/`cscale`/`m` on ONE fixed, weak, undertrained network. This
study asks whether the same knobs matter differently depending on how the
network was trained, by drawing training hyperparameters and search-shape
knobs JOINTLY (one random draw fixes both), rather than training one network
and sweeping search shape on top of it alone.

## Design

Random search over 9 axes at once (Bergstra & Bengio 2012, per this project's
established practice for every hyperparameter sweep so far), not
one-axis-at-a-time:

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
`gaz(sims=N,cvisit=C,cscale=S,m=M)@1`, not passed to the trainer -- the
self-play generator does not read them):
| Axis | Eligible values |
|---|---|
| cvisit | 400, 600, 800, 1000 |
| cscale (tenths) | 40, 70, 100, 130 |
| m | 8, 12, 16, 20 |

`sims` is sampled once per draw and used for both self-play generation and
the certification head, per `Docs/model-training-playbook.md`'s
generator/search-depth-matching guidance.

101 draws (1 REF + 100 random, seed 8817), 1 seed per draw (traded for more
random draws per developer instruction), 4 shared checkpoint rungs
(100/400/1500/4000 games) so every arm is screened at the same points and an
interior optimum can't be missed by screening only the final rung -- 404
checkpoints total. `tools/gumbelzero_joint_sample.ps1`'s
`Get-GumbelZeroJointDraws` is the single source of truth for the draw list.

REF anchors both prior findings, snapped into this study's eligible sets:
sims=300, lr=0.01, l2=0.0, replay=(2000,32), batch=32, open=4 (Pass-1 sanity
recipe) and cvisit=600, cscale=70, m=16 (nearest in-range point to Round 5's
practical recommendation `cvisit=500,cscale=50`).

### Architecture/init: dropped before launch

The original design (confirmed via developer sign-off mid-session) called for
2 initializations x 2 architectures (linear + MLP-hidden-32) as parallel arms,
doubling the study to 1600 checkpoints -- the reason `ML_SLOTS` was raised
1024 -> 4096. Before the 1-arm timing check, inspection of
`src/ml_gumbelzero.cpp`/`tools/train_main.cpp` found `train.exe gumbelzero`
has no `--model-type`/`--mlp-hidden`/`--init` flag at all: initialization is
from-scratch-only, linear-only, by original Slice-1 design
(`src/ml_gumbelzero.h`'s own header comment). Adding that support is real
trainer code, not a config choice, so the developer chose to drop back to
101 draws x 1 architecture x 1 init x 4 rungs = 404 checkpoints now, with
MLP/init as an explicit follow-up round (same two-stage pattern as the
existing Pass-2 Round A -> Round B). `ML_SLOTS` stayed at 4096 rather than
being re-lowered, since the follow-up round is expected to need the headroom.

### Rating: pool-only gauntlet, not pinned-cohort

The 1-arm timing check (train + roster + play + screen on just REF) surfaced
a second scope problem: the project's usual Workflow A pattern (`play
--cohort` into a pinned fit, "pinning beats N gauntlets" because
cohort-vs-cohort games resolve the cohort's internal order) schedules
cohort-internal games, which is quadratic in cohort size. At 404 checkpoints
that is C(404,2) x 16 = 1.3M internal games on top of 404 x 27 x 16 = 174,528
pool games -- measured projection ~48 hours, crossing the 1-day compute
threshold. The "pinning beats gauntlets" guidance in
`Docs/ranking-workflow.md` was written for cohorts of ~20-50 (TD-Leaf scale)
and inverts at this size. Developer chose pool-only gauntlet screening instead
(each checkpoint vs only the 27-agent `ranking/roster_screening_pool.txt`,
~175K games, ~5.6hr projected), matching the "gauntlet" mechanism originally
asked for earlier in the session. Implemented as `rank.exe gauntlet --keep`
launched in parallel per-checkpoint via per-worker batch files (never
concurrent multi-process appends to one shared store file -- see the
gotchas below), not `play --cohort`.

## Screening pool

`ranking/roster_screening_pool.txt`: 26 agents + `rand@1` anchor, regime-
diverse and Elo-spread (1039 down to ~107) across all 5 opener categories,
adopted this session as the project-wide default gauntlet pool for rating any
new/candidate agent during training or screening (`Docs/ranking-workflow.md`,
"Default gauntlet pool").

## Process gotchas hit while building this

1. **`vswhere.exe`/bare-batch-script resolution fails from this shell** once
   `vcvars64.bat` has run in the same context (documented in root
   `CLAUDE.md`) -- compounded this session by MSYS/Git-Bash's path
   conversion mangling `/c` in `cmd /c`, which was the actual root cause of
   `cmd /c` invocations silently doing nothing. Fix: `MSYS_NO_PATHCONV=1`
   before any `cmd /c` from the Bash tool, and source `vcvars64.bat` by full
   path into a written `.bat` file invoked by its own absolute path (a bare
   relative name still fails to resolve).
2. **A `src/` header change is not "done" until every binary that links it is
   rebuilt.** `ML_SLOTS` 1024->4096 was verified via `tests.exe` alone; the
   roster phase then failed with "no hash" for every slot >= 1008 because
   `rank.exe`/`train.exe` were still linking the OLD `ml_eval.h` (never
   rebuilt this session before that point). Slots 1008-1023 additionally
   resolved to `models/scratch/` instead of `models/sweep/` under the stale
   binary's OLD reserved-range math. Fixed by rebuilding both.
3. **`Start-Process -PassThru`'s `.ExitCode` reads back empty/null** after
   `WaitForExit()` unless `.Handle` is touched immediately after the process
   starts -- a documented .NET/PowerShell quirk. Hit as 4/4 "failed" shards
   in the 1-arm timing check whose logs showed fully correct results.
4. **Roster lines need the FULL canonical `learned()` descriptor**
   (`model=N,hash,regime,type,shape=...`), not the short `learned(sN,hash)`
   form -- `rank.exe check` rejects the short form outright rather than
   silently upgrading it. `rank.exe canon --roster <file>` (an existing,
   previously-undocumented-in-`tools/CLAUDE.md` subcommand) rewrites a whole
   roster/id-list file to canonical spelling via the same `rankUpgradeId`
   the parser's own re-derive uses, so the study script generates short-form
   IDs and canonicalizes the whole file in one pass rather than hand-deriving
   the descriptor.

See `gumbel-mcts-results-5-violet-harbor.md` for outcomes.
