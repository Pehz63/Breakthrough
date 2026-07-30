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
