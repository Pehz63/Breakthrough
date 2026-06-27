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
