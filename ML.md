# Machine Learning System

A modular, composable system of interchangeable parts for learning and competition.
This document is the overview and the "how to add more" guide. The tables near the
bottom are **auto-generated** from the live code registries by `train.exe docs`, so
they never drift from what is actually implemented.

## The core idea: four pluggable axes + agents

An **Agent** is a complete player whose "brain" is one of two shapes:

```
            Agent (AgentSpec)            <- competes, gets an Elo, can be played against
              |
        Move Chooser (the brain)
         /                      \
   Search                       Policy (no lookahead)
   /     \                        |
Explorer  Evaluator        heuristic  OR  learned move-rater
(search)  (value)          (random..)     (policy model)
   |          |                                |
   +----------+------------- Models -----------+
        (linear / mlp / nnue / transformer; value head or policy head)
```

- **Board-state evaluator (value):** scores a board (white-centric). Registry
  `g_evaluators[]` in [src/ai_eval.cpp](src/ai_eval.cpp). Includes `LearnedValue`,
  which delegates to a value model.
- **Move-tree explorer (search):** decides *how* an evaluator is used. Registry
  `g_explorers[]` in [src/explorers.cpp](src/explorers.cpp): `Greedy` (1-ply) and
  `AlphaBeta` (wraps the existing minimax).
- **Move chooser / policy (no search):** picks a move directly. Registry
  `g_choosers[]` in [src/choosers.cpp](src/choosers.cpp): the random family plus
  `LearnedPolicy`, a learned move-rater that scores each legal move and plays the best.
- **Model:** the architecture behind a learned evaluator or policy. Registry
  `g_modelTypes[]` in [src/ml_model.cpp](src/ml_model.cpp): `linear` is implemented;
  `mlp` / `nnue` / `transformer` are registered for later.

An [`AgentSpec`](src/agents.h) ties these together (explorer + evaluator, or a
chooser) plus **strength dilution** (`randomMoveProb`, `depthCap`). One function,
`agentChooseMove`, drives tournaments, data generation, and (later) the GUI opponent.

## Hybrid C++/Python

- **C++** owns the engine, in-search inference, self-play data generation, and
  tournaments (fast, dependency-free). The Phase-1 linear models also train in C++.
- **Python** (optional) owns heavy model training (PyTorch for mlp/nnue/transformer)
  and analysis; trained weights export back into the same C++ model-file format. See
  [analysis/](analysis/) and [train_py/](train_py/).
- The contract is open data files: append-only JSONL under [data/](data/), queried in
  place by DuckDB. Canonical position keys (packed encoding + 64-bit hash, optional
  mirror fold) join evaluations and labels to positions.

## The trainer (`train.exe`)

Build: `.\tools\run_train.ps1 -Build` (or `build_train.bat`). Commands:

```
train.exe selfplay-supervised --out models/lin_value --games 250 --epochs 6
train.exe imitate             --out models/lin_policy.txt --games 150 --epochs 12
train.exe tournament          --games 10        # single-process, default depth ladder
train.exe docs                --ml ML.md
```

(The linear value model overfits past ~6-8 epochs on outcome labels, so keep `--epochs`
small.) Checkpoints land in `models/` and are recorded in `models/manifest.{json,md}` and
`data/*.jsonl`.

### Parallel depth-laddered tournament

The round-robin is the long pole, and the engine keeps its board/eval state in globals,
so games can't share a process. Instead games are **sharded across processes** (each its
own copy of the globals) and merged:

```
.\tools\run_tournament.ps1 -Workers 12 -Depths "2,4,6,8,10" -Games 10 -NodeBudget 500000
```

Under the hood: K `train.exe tournament-play --shard i --of K ...` workers each play their
slice (game index % K == shard) into `data/tourney.jsonl.<i>` with per-move timing, then
`train.exe tournament-rate` merges them, fits Elo, and prints
`Elo | ms/move | max ms | games | agent`. The roster is **all working agents**: the
random/heuristic family (incl. SmartRandom at several N), the learned policy, and Greedy +
AlphaBeta over a table of evaluator weight presets at each depth.

A per-move **node budget** (`g_nodeBudget`) with **iterative deepening** keeps deep
searches bounded and sound: an agent "at depth D" deepens 1,2,...,D and keeps the best move
from the deepest iteration that finished within budget. So Elo rises with depth, then
plateaus once the budget binds (without it, a plain depth-capped search wastes its budget on
the first line and plays *worse* at higher depth). The top agent is written to
`agents/champion.txt` + `agents/champion_params.txt` (a `minimax_params.txt` block).
Absolute ms/move is inflated by parallel CPU contention; relative order is informative.

### Restricting the roster (`-Only`) and the run archive

To focus a run on a subset (e.g. the strongest few agents plus the learned policy),
pass `-Only` an agent-name allowlist. The names must already exist in the roster, so
include their depths in `-Depths` (an `AB6-...` name needs `6` in the ladder):

```
.\tools\run_tournament.ps1 -Depths "4,6,8,10" -NodeBudget 1000000 `
  -Only "AB6-Classic-chip,AB8-Classic-chip,AB10-Classic-chip,LearnedPolicy" `
  -Note "why this run / what changed since last time"
```

A run with `-Only` does **not** overwrite `agents/library.txt` / `champion*.txt` (those
stay the full-roster snapshot). Names that do not resolve print a `WARNING` instead of
silently shrinking the field. `--only` is threaded identically through `tournament-play`
and `tournament-rate` so the two phases build the same roster.

**Every** run is archived, timestamped, under `runs/<run_id>/`:

| file | contents |
|------|----------|
| `config.json` | exact config: depths, node budget, games/pair, seed, workers, board, `only[]`, the pre-run note |
| `elo.tsv` | this run's ranked Elo table (elo, ms/move, ms_max, games, name, desc) |
| `results.jsonl` | immutable copy of this run's merged game/timing rows (gitignored, bulky) |
| `notes.md` | the pre-run note plus any later notes appended with `run-note` |

`runs/index.jsonl` keeps one summary line per run (champion + Elo + counts). The agent
**registry** (`agents/registry.jsonl` append-only, `agents/registry.md` regenerated) is
the union of every agent ever rated, so a subset run never erases knowledge of the rest.
Each row carries a `spec_hash` over the agent's structural fields **and**, for learned
agents, its model file content, so a retrain / param change / bugfix flags as `changed`.

Attach a realization to a past run without re-running it:

```
.\train.exe run-note --run 20260628T041236Z --note "CPU was throttled, ignore ms/move"
```

## Persistent Elo ranking (`rank.exe`)

The tournament above is a **one-off experiment runner**: it recomputes Elo from a
transient results file, and its roster lives in C++ code. The separate `rank.exe`
system (see [src/ranking.h](src/ranking.h) and the README's "Agent Elo ranking"
section) is the **permanent incremental ladder** and the ground-truth strength
label for training:

- Agents are canonical ID strings (`ab(d6,tt,ord,nb200k).classic(t2,c10,w3,l2).v1`)
  listed in the hand-edited `ranking/roster.txt` with `anchor|on|off` toggles.
- Every game is appended forever to `ranking/matches.jsonl`, keyed by those IDs, so
  the scheduler only plays missing pairings. Adding one agent costs O(N) games and
  nothing is recomputed.
- Ratings are a deterministic anchored Bradley-Terry refit (UniformRandom = Elo 0)
  with per-agent standard errors, written to `ranking/ratings.tsv` (machine-readable
  labels) and `ranking/report.md` (head-to-head matrix, per-agent match history).
- `rank.exe gauntlet --id <candidate>` rates one new agent against the frozen pool
  in O(N) games, the cheap evaluation step for a weight hill-climber over Elo vs
  compute time.
- `rank.exe extract` replays a deterministic sample of the stored match history into
  labeled value-training positions (`train.exe --from-data` consumes the file), which
  turns the whole rated pool into a training data source.
- `rank.exe pairgen` plays FRESH games between any two canonical IDs into the same
  training-data format, with a per-side dilution override (vary a deterministic
  agent's games without changing its identity), random opening plies (`--open-plies`,
  with `--open-side a|b|both` masking which agent plays them so an asymmetric opener
  handicaps only one side), a winner filter, and branch-from-win mining (rewind a
  kept win, substitute a different move, keep the tail if the winner wins again).
  Each dataset gets a `.meta.json` sidecar recording the full recipe and outcome
  tallies. The vs-champion study (`tools/train_vs_champion.ps1`) and the opener-bias
  study (`tools/opener_bias_study.ps1` + `tools/opener_bias_retrain.ps1`, Theory 6 in
  `Docs/theories.md`) are built on it.
- `rank.exe opener-bias` measures whether a symmetric random opener handicaps a
  deterministic champion: it replays the opener and, at each champion ply, scores the
  position after the forced-random move against the position after the champion's own
  move using a `--judge` agent's search, tabulating the delta split by color.

Learned agents embed a model-file content hash in their ID, so a retrain is a new
identity and old match rows stay truthful. Use the tournament for parameter sweeps
and ablation studies; use the ranker for the durable leaderboard and ML labels.

## Model file format (text, like `minimax_params.txt`)

```
# Breakthrough ML model
type=linear            # dispatched by the loader's factory
head=value             # value | policy
feature_version=1      # must match the engine's feature version
feature_count=30
out_scale=900          # maps the raw output into the integer eval range
bias=-0.5
w0=...
w1=...
```

`mlValueScore` applies the shared near-win shortcut, then maps the model output
through `tanh * out_scale` and clamps it strictly inside the `+/-WIN` sentinels.

## Position-strength labels and the dist model (the position oracle)

A pipeline that measures each board position's Elo advantage empirically and
trains a distributional model to predict it. The target: for a position, the
mean is the Elo handicap at which the game from there becomes a coin flip
(White-positive), and the SD is the volatility of that advantage (how
unreliably it converts). Everything runs on one shared piece of math,
`probitPoint` in `src/ml_model.h`: with latent advantage A ~ N(mu, sigma^2)
in logit units and a known players' Elo gap d, P(White wins) =
sigmoid(kappa*(mu+d)) with kappa = 1/sqrt(1 + (pi/8)*(sigma^2 + v)). One
logit is 400/ln(10) = 173.7 Elo (`ELO_PER_LOGIT`).

Pipeline stages (first three are `rank.exe`, rest are `train.exe`):

1. `posgen` builds position pools from stored games: deterministic replay
   sample, deduped by canonical enc, stratified by ply band and material,
   decided positions skipped, split into hash-disjoint train and eval tiers
   (hash % 17). Rows: `{"enc","h","ply","stm","md","seen"}`.
2. `label` plays a DESIGNED ladder of fresh games from every pool position:
   a hand-editable spec (`rung <i> <id>` + `pair <wi> <bi> <games> [mod k
   r]`) of rated stochastic agents spanning the Elo scale, several gap
   levels in both directions per position. Raw outcome rows
   `{"h","wi","bi","g","seed","y","p"}` append to a store whose `.meta.json`
   freezes the rung-id list, so gaps are recomputed from any future ratings
   snapshot without replaying a game. Rungs must consume rand() to vary
   (dilution or random choosers; eval noise/jitter is deterministic per seed
   and is rejected in deterministic-vs-deterministic pairings). Every game
   starts from a fresh TT (`ttClear`), so playouts are order-independent and
   shard splits reproduce exactly.
3. `labelfit` joins the store to a ratings snapshot and fits each position's
   (mu, sigma) by the 2-parameter probit MLE with Fisher standard errors,
   emitting labels `{"enc","h","mu_elo","sd_elo","se_mu","se_sd","n","nll",
   "flags"}` plus a per-pairing QC table. Rerunning it against a newer
   ratings file re-labels the same raw store for free.
4. `dist-value` trains the `dist` model type (two heads over the v2
   piece-square features: mu, the evaluator-compatible output, and log-sigma,
   clamped [-4, 3] with projected gradients) on the RAW rows by probit BCE,
   with the gap entering as a fixed offset per row. Validation splits by
   position, never by row. It prints gap-only and intercept-only nulls the
   model must beat for the board to be adding anything, calibration by |d|
   bucket, and material-stratified sigma. A secondary mode regresses on the
   fitted labels instead (SE-weighted).
5. `score` ranks any set of positions (pool/labels files, bare enc lines, or
   board files with `--stm`) by predicted mean advantage, printed as
   mean +- SD in Elo with P(win | equal opponents).
6. `dist-eval` is the success criterion: the model against the d8/nb2m
   oracle's root search score, `models/pst_value.txt`, and the Classic
   static eval, each calibrated to the Elo scale by a 1-parameter fit on
   train-tier labels only, scored on the held-out eval tier (mu MAE/RMSE,
   Spearman, per-row outcome NLL, SD validity), ending in a VERDICT line.
   The oracle claim requires beating the calibrated d8 baseline on BOTH
   outcome NLL and mu MAE.

`tools/label_study.ps1` orchestrates the whole campaign (chunked, sharded,
CSV-ledger resumable; `-DryRun` runs a tiny end-to-end check in minutes).
Search integration: a dist model drops into any LearnedValue slot (its
forward() is the mu head; a linear mu head keeps the incremental accumulator
leaf), and `mlValueScoreDist` exposes mean +- SD of the current board for
analysis surfaces. Playing strength is a separate measurement from
prediction quality (theory 27): the dist models' roster Elo is recorded but
is not the oracle claim.

### Shipped models (2026-07-21 campaign)

Four dist models trained on the full labeled store (22,788 train positions,
950,884 raw rows; 700 held-out eval positions, 296,800 raw rows). Inputs:
feature v2, 129 binary piece-square features (as above). Training: `train.exe
dist-value`, primary raw-BCE mode, lr 0.01, lr-sigma 0.002, l2 1e-6,
val-split 0.1, early-stop (these hyperparameters were chosen from an
exploratory sweep documented in
[position-oracle-results-1](plans/position-oracle-results-1-lazy-popping-simon.md),
which found lr 0.01 beat the original 0.02 default).

| Model | mu / sigma heads | Parameters | Oracle-verdict MAE / NLL (beats d8?) |
|---|---|---|---|
| `models/dist_lin.txt` | linear / linear | 260 | 161.3 / 0.4240 (yes) |
| `models/dist_mlp_s1001.txt` | mlp(128,64) / mlp(32) | 29,154 | 147.9 / 0.4087 (yes) |
| `models/dist_mlp_s2002.txt` | mlp(128,64) / mlp(32) | 29,154 | 150.0 / 0.4110 (yes) |
| `models/dist_mlp_wide.txt` | mlp(256,128) / mlp(64) | 74,690 | 146.2 / 0.4079 (yes) |

All four beat the calibrated depth-8/2M-node oracle baseline (MAE 191.3,
NLL 0.4498) on both required metrics -- theory 34, confirmed. The sigma
(volatility) head is weaker: predicted sigma correlates with measured
position volatility at only 0.12-0.29 (Pearson) across configs -- theory 35,
weakly supported, not confirmed.

**Roster Elo by search head** (2026-07-21 fit; d8/nb2m oracle 1151+/-12,
reigning champion 1131+/-13, in the same fit -- never compare these numbers
against a different fit):

| Model | d2 | d4 | d6/nb200k |
|---|---|---|---|
| dist_lin | 594+/-16 | 694+/-15 | **1031+/-16** |
| dist_mlp_s1001 | 509+/-17 | 667+/-15 | 974+/-16 |
| dist_mlp_s2002 | 716+/-15 | 648+/-15 | 967+/-16 |
| dist_mlp_wide | 454+/-18 | 768+/-15 | 931+/-16 |

Playing-strength Elo diverges from the prediction-quality ranking (theory
27, reconfirmed here): `dist_lin` beats all three MLP configs in actual
play at d6 despite losing to them on prediction, and none of the four beat
the reigning champion or oracle -- the developer's own expectation going in
was that a d6 MLP would be the new champion; it was not, and that result is
reported as-is rather than softened. A further open, unexplained wrinkle:
`dist_mlp_wide` is the strongest MLP at d4 (768) but the WEAKEST at d6
(931), a reversal not seen in the other two MLP configs. Full numbers, the
exploratory sweeps that shaped this recipe, and caveats: the results doc
above.

## Worked example: the MLP value agent

A concrete learned agent end to end, so the pieces above (model type, features,
evaluator, explorer) connect into one picture.

- **Model = `MLPModel`, a VALUE model (board -> one scalar).** Inputs: feature
  version 2, 129 binary inputs (one per (color, square): 64 White piece-presence +
  64 Black + 1 side-to-move = +1/-1). Fully-connected. The code supports an
  arbitrary number of hidden layers (the `--mlp-hidden` widths list, designed for
  1-2); models trained so far use one hidden layer of width 16 or 32. ReLU on the
  hidden layer, linear output. The output scalar (a logit) becomes an eval via
  `tanh * out_scale (900)`, clamped inside the win sentinels: a white-centric board
  value. Optionally wrapped by `ResidualModel`, a frozen chip-count skip
  (`skipW * material_diff` added to the logit) plus the MLP learning the residual.
- **Agent = alpha-beta search + this value model.** e.g.
  `ab(d6,tt,ord,nb200k)@1.learned(<hash>)@1`: alpha-beta minimax at depth 6 with a
  200000-node budget, transposition table + move ordering, scoring leaf positions
  with the MLP. The model is a position evaluator, not a move-output policy; the
  agent searches and evaluates. The MLP is full-scan (not incrementally updatable
  like a linear v2 model), so a deeper search costs more per node -- the NNUE step
  in the value-head roadmap addresses that.
- **Training = supervised value regression on outcomes.** `selfplay-supervised
  --model-type mlp --residual-skip -1 --from-data <replay>`, where `<replay>` is a
  `rank.exe extract` sample of the rated pool's stored match history
  (`ranking/matches.jsonl`) -- real games the pool already played, each position
  labeled by its game result (logistic loss). Not self-play by this model, not
  imitation of a teacher's moves.
- **Strength:** measured as pooled Elo from a full-roster run at depth 4 and depth 6
  (see `Docs/benchmarking.md`); the residual/MLP results are in
  `plans/residual-mlp-results-2-tingly-chipmunk.md`.

## How to add more (extension workflow)

| To add a... | Edit | Result |
|---|---|---|
| Board-state evaluator | append an `EvalDef` to `g_evaluators[]` + write its `fn` ([src/ai_eval.cpp](src/ai_eval.cpp)) | appears in console, GUI, tournaments, docs |
| Move-tree explorer | append an `ExplorerDef` to `g_explorers[]` + write its `fn` ([src/explorers.cpp](src/explorers.cpp)) | selectable by any `AgentSpec` |
| Move chooser / policy | append a `ChooserDef` to `g_choosers[]` + write its `fn` ([src/choosers.cpp](src/choosers.cpp)) | a no-search brain; learned ones read a model slot |
| Model architecture | add a `Model` subclass + a `g_modelTypes[]` row + a `makeModel`/`loadModel` case ([src/ml_model.cpp](src/ml_model.cpp)) | loadable from a `type=` file; backs a value or policy head |
| Training regime | add a `RegimeDef` to `g_regimes[]` + a function ([src/ml_train.cpp](src/ml_train.cpp)) + a CLI case ([tools/train_main.cpp](tools/train_main.cpp)) | a `train.exe` subcommand + docs row |
| Feature | extend the lists in [src/ml_features.cpp](src/ml_features.cpp) and bump the version | new inputs for value/policy models |

After any registry change, run `train.exe docs` to regenerate the tables below.

<!-- AUTODOC:BEGIN -->

_Auto-generated by `train.exe docs` from the live registries. Do not edit by hand._

### Board-state evaluators

| name | params | incremental |
|------|--------|-------------|
| Classic | 4 | yes |
| Experimental | 5 | yes |
| LearnedValue | 1 | no |
| Advanced | 16 | yes |

### Move-tree explorers

| name | description |
|------|-------------|
| Greedy | 1-ply: play the move the evaluator scores best (no lookahead). |
| AlphaBeta | Alpha-beta minimax to a fixed depth (budget = depth). |

### Move choosers / policies

| name | description |
|------|-------------|
| UniformRandom | Every legal move equally likely. |
| TieredRandom | Prefer wins, then captures, then normal moves. |
| SmartRandom | TieredRandom restricted to the furthest-N pieces (param). |
| LearnedPolicy | Argmax of a learned move-rater (policy model); no search. |

### Model architectures

| name | implemented | description |
|------|-------------|-------------|
| linear | yes | Linear: bias + weighted sum of features. Fast; value or policy head. |
| mlp | yes | Multilayer perceptron (1-2 hidden layers), hand-written forward + backprop; ReLU hidden, linear output. |
| residual | yes | Frozen chip-count skip + an inner model (linear or mlp): output = skipW*matDiff + inner. Learns the residual. |
| dist | yes | Two-headed distributional value model: mu head (White advantage in logits, the evaluator output) + log-sigma head (volatility), probit-BCE trained on rated-gap playout outcomes. |
| nnue | no | Efficiently updatable NN; designed to plug into the g_evalPos accumulator. |
| transformer | no | Squares-as-tokens self-attention; teacher / offline label generator only. |

### Training regimes

| name | description |
|------|-------------|
| selfplay-supervised | Self-play games labeled by outcome; fit a linear value model. |
| ensemble | Average K trained linear v2 value models (optionally mirror-symmetrized) into one. |
| imitate | Behavioral cloning: a teacher's chosen move trains the policy move-rater. |
| dist-value | Distributional (mu, sigma) position-strength model fit on rated-gap playout outcomes from the label store (rank.exe posgen/label/labelfit). |
| score | Score positions with a saved model, ranked by mean White advantage (a dist model prints mean +- SD in Elo). |
| dist-eval | Evaluate a dist model against the calibrated d8 oracle, pst_value, and Classic baselines on the held-out eval tier. |
| tdleaf | TD-Leaf(lambda) self-play bootstrap of a value model. (future) |
| population | Other-play tournaments, Elo-tie labeling, multi-condition runs. (future) |
| tournament | Round-robin of composed agents; prints an Elo table. |
| docs | Regenerate the auto-doc tables from the live registries. |

### Value features (v1, 30)

mat_diff, white_total, black_total, white_rank0, white_rank1, white_rank2, white_rank3, white_rank4, white_rank5, white_rank6, white_rank7, black_rank0, black_rank1, black_rank2, black_rank3, black_rank4, black_rank5, black_rank6, black_rank7, white_forward, black_forward, white_phalanx, black_phalanx, white_defended, black_defended, white_threats, black_threats, white_mobility, black_mobility, side_to_move

### Value features (v2, 129, sparse piece-square)

One binary input per (color, square) plus side to move: w_a1..w_h8 (64), b_a1..b_h8 (64), side_to_move. A move changes 2-3 inputs, which is what the incremental g_mlAcc search accumulator exploits (train with selfplay-supervised --feature-version 2).

### Move features (v1, 9)

capture, fwd_to, fwd_from, is_diagonal, to_edge, reaches_goal, support_behind, enemy_forward, hanging

<!-- AUTODOC:END -->
