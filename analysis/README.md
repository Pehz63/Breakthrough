# Analysis + Python layer

Optional Python tooling over the open datastore the C++ side writes. The engine and
`train.exe` need none of this; it is here for querying results and (later) training
heavier models. See [../ML.md](../ML.md) for the overall system.

## Setup

```
pip install -r ../requirements.txt
```

## Query the datastore (DuckDB)

The C++ trainer writes append-only JSONL under [../data/](../data/). Run SQL over it
in place:

```
python analyze.py top-agents        # highest-Elo agents (from a tournament)
python analyze.py models            # trained models + loss/winrate/Elo
python analyze.py avg-eval          # average evaluation per board position
python analyze.py fairest           # most fairly-matched positions (outcome ~ 0.5)
python analyze.py position <hash>   # evals + labels for one position
python analyze.py sql "SELECT ..."  # arbitrary query
```

These answer the headline questions directly: the highest-Elo agent, the most
fairly-matched positions, and the aggregate/average evaluation of any board state,
all by joining `positions` <- `labels` / `evaluations` on the canonical position
hash.

## Predict a cohort's peak Elo from its swept config axes

Given a long-format cohort export from `tools/export_cohort_results.ps1` (one
row per checkpoint, a rung ladder per training run), collapse each run to its
peak Elo and fit an interpretable model over the swept config axes -- a
shallow decision tree (depth chosen by cross-validation) plus a random forest
for a more stable feature-importance ranking, a linear baseline for contrast,
and plain per-value bucket means with no model at all. Emits a markdown
report and a rendered tree-diagram PNG:

```
python analysis/predict_peak_elo.py \
    --in ../plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv \
    --group-by block --target elo --rung-col rung \
    --out-report ../plans/gumbel-mcts-peak-elo-predictor-5-violet-harbor.md \
    --out-tree-image ../plans/gumbel-mcts-peak-elo-tree-5-violet-harbor.png
```

`--features` (default `sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m`)
and `--rung-col` (pass `""` to disable if the cohort has no checkpoint ladder)
make it reusable for a differently-shaped cohort study. Any non-numeric
feature column (e.g. `modeltype`) is one-hot encoded automatically. `--target`
accepts any per-checkpoint column, not just `elo` -- e.g. `cpu_ms_move` or
`eff_elo_per_log2cpu` (Elo per unit log-compute) to analyze speed/efficiency
instead of strength; pass `--minimize` for a cost metric where the best value
is the lowest (each draw is then collapsed to its cheapest checkpoint instead
of its strongest). The report auto-flags any pair of axes that turn out to be
perfectly coupled in the sweep (not actually independent draws) rather than
silently double-counting one underlying effect as two.

## Export a model for the C++ engine

Heavy models train in Python and export into the engine's text model format, which
`loadModel()` reads (the engine always does in-search inference):

```
python ../train_py/export_format.py --out ../models/py_linear.txt --features 30
```

`write_linear_model()` in `train_py/export_format.py` is the reusable writer; MLP /
NNUE / transformer exporters add their own `type=` plus a matching loader case in
`src/ml_model.cpp`.

## Experiment tracking

Training metrics (loss, win-rate, Elo per checkpoint) are always written to
`data/metrics.jsonl` and `models/manifest.{json,md}`. Weights & Biases is optional:
install `wandb` and mirror those metrics in your Python training loop. Nothing here
requires a W&B account or network access.
