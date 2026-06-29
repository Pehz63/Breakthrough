# Todo

- ~~Make unit tests~~
- Keep optimizing
- ~~Improve board state evaluator with machine learning~~ (Phase 1 shipped: see the Modular ML System Backlog below)
- ~~Make a GUI raylib + raygui~~
- ~~Display the board state evaluation for each AI in the main board area~~
  - ~~For tree search or other algorithms (like minimax), show both immediate evaluation and the AI's predicted downstream evaluation~~
- Display whose turn it is in the main board area
- ~~Board state evaluator selector for heuristic, NN, or other BSEFs~~
- ~~Depth time budget for minimax (so I specify 10 seconds per move and it will stop calculating after going deep enough to do ~10 seconds)~~
  - ~~Per-move **node** budget (`g_nodeBudget`) with iterative deepening shipped (used by the tournament).~~
  - ~~Wall-clock **time** budget (`g_timeBudgetMs`) shipped: `--time-budget-ms` / per-agent `timeBudgetMs`, composes with the node budget.~~
  - ~~Budget is now decoupled from depth: per-agent `nodeBudget`/`timeBudgetMs` + an unbounded-depth budget ladder (`--budgets`), so a budget sweep varies strength. Per-move telemetry reports fractional effective depth (e.g. 5.7), which cap ended the search (node/time/depth), nodes/move, and branching-factor distribution.~~
- ~~Parameter study for classic board state evaluator (turn weight calibrated via `train.exe turn-swing`; wider chip/structure presets + `--ablate` feature comparison shipped)~~
- Hyperparameter study for machine learning board state evaluator
- Best moves list or recommendation arrow
- Interpret board analysis
  - Which piece is most impactful to the current evaluation?
  - What's the cheapest strategy to beat each given bot/parameters, even if overfitted?
  - What strategies could a human devise to beat a bot?
  - Is attacking the center or attacking the edge the best?
  - Is advancing through the center or the edge the best?
  - Is keeping the hind pieces in place the best?


---

# Modular ML System Backlog

A composable system of interchangeable parts. An **Agent** = a **Move Chooser** (its "brain"),
which is either a **Search** (a **Move-Tree Explorer** + a **Board-State Evaluator**) or a
**Policy** (a direct, no-lookahead move picker: a heuristic or a learned move-rater). Each axis
is a registry, so adding one is a single table entry + a function body, and everything
(UIs, tournaments, docs) picks it up automatically.

Legend: **(P1)** = built in the first pass (versatility proof). Everything else is future work
against the same seams.

## Models (value head: board -> scalar)
- ~~Linear value model **(P1)**~~
- MLP value model (1-2 hidden layers, hand-written forward pass)
- Convolutional NN value model (board as an 8x8xC grid; local spatial filters for walls/columns/forwardness)
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`)
- Transformer value model (squares as tokens) -- teacher / label generator only, not in-search
- Incrementalize an ML model (e.g. MLP/NNUE) so a move recomputes only the few inputs it changed
  instead of the whole forward pass. Explore encouraging fewer recalculations per move by having
  hidden terms cancel/zero out (e.g. ReLU gating so unchanged regions contribute a constant), the
  way the heuristic `g_evalPos` already does an owner-bounding-box local delta.

## Models (policy head: board + move -> score / move-rater)
- ~~Linear move-rater **(P1)**~~
- MLP policy
- Transformer policy
- Softmax / temperature sampling over move scores (for exploration + diverse self-play)

## Board-State Evaluators (BSEFs)
- ~~Classic (done)~~
- ~~Experimental (done)~~
- ~~LearnedValue: wraps a value model **(P1)**~~
- Ensemble / blended evaluator (average or weighted mix of several evaluators/models)

## Move Choosers / Policies (direct, no search)
- ~~Human, UniformRandom, TieredRandom, SmartRandom (done)~~
- ~~LearnedPolicy: argmax of the move-rater **(P1)**~~
- Greedy-by-eval (1-ply pick of the move maximizing a BSEF) **(P1, via the Greedy explorer)**
- Softmax/temperature sampling policy (probabilistic move choice)

## Move-Tree Explorers (search)
- ~~Greedy 1-ply **(P1)**~~
- ~~AlphaBeta minimax (done; wrapped as an explorer **(P1)**)~~
- ~~Iterative deepening (shipped: used by the node-budgeted search)~~
- ~~Time-budgeted search by wall-clock seconds (`g_timeBudgetMs`, shipped)~~
- ~~Transposition table + move ordering (killers/history) shipped as opt-in, ablatable features (`useTT`/`useMoveOrder`); aspiration windows too (`aspirationWindow`)~~
- Quiescence search (extend on captures / near-wins)
- MCTS / PUCT (pairs a policy head with a value head)
- TT speedup is currently node-count-real but wall-clock-muddied by `positionKey`'s per-node string build; an incremental Zobrist hash would make the TT a wall-clock win too

## Training Regimes
- ~~Supervised on self-play outcomes (value) **(P1)**~~
- ~~Imitation / behavioral cloning from a stronger agent (policy) **(P1)**~~
- TD-Leaf(lambda) self-play bootstrap (value)
- Population / other-play tournaments as a data source
- Elo-tie labeling: label a position by the interpolated Elo E* at which expected score = 0.5
- Distillation from deep search or from a teacher model

## Weight optimization / geometry mapping
A single per-weight sweep is insufficient: each weight only matters RELATIVE to the others
(if chip=300, then forward 1 vs 2 vs 10 is indistinguishable), interactions are non-obvious
(forward may need to be HIGHER when structure is high, to offset the structure lost by
advancing; forward could even be NEGATIVE to keep pieces back and advance together), and the
optimum is a surface, not a point. Replace single sweeps with a search that maps the geometry:
- Coordinate-free local search: a few parallel hill-climbers, each re-centering its weights
  every few rounds on its best result (random restarts to escape local optima).
- Evolutionary tournament: each round, mutate the top-couple-Elo agents (unique random
  perturbations of their weights) into new agents, add them to the round-robin, drop the
  weakest, and iterate -- so the population crawls the weight surface by selection.
- Always normalize/anchor one weight (e.g. chip) so the others are measured relative to it.
- Test signed weights (negative forward, etc.) and structure x forward interaction explicitly.
- Report a response surface, not a single recommended value.

## Strength Dilution (to spread an Elo ladder)
- ~~Random-move probability **(P1)**~~
- ~~Depth cap **(P1)**~~
- Evaluation noise (jitter the eval to weaken without full randomness)

## Elo / Tournaments
- ~~Round-robin + Elo rating **(P1)**~~
- ~~Checkpoints saved + manifest (JSON + Markdown) **(P1)**~~
- ~~Parallel (process-sharded) depth-laddered round-robin with per-move timing + champion export~~
- ~~Roster subset via `-Only` agent-name allowlist (subset runs preserve the full-roster `library.txt`)~~
- ~~Per-run archive (`runs/<id>/` config + elo.tsv + notes.md + results, `runs/index.jsonl` log) + agent registry (`agents/registry.{jsonl,md}` union with a `spec_hash` that flags retrains/changes) + `run-note` for later annotations~~
- Gauntlet vs fixed anchors
- BayesElo-style rating with uncertainty

## Agent Composition + Play
- ~~AgentSpec (explorer + evaluator + chooser + model slots + dilution) **(P1)**~~
- ~~Saved agent library file **(P1)**~~
- GUI: play against a named saved agent
- GUI: agent-vs-agent ladder / leaderboard view

## Data + Infrastructure
- ~~Model file format (text, `type=`/`head=` header) **(P1)**~~
- ~~Model slots so White/Black can use different models in one process **(P1)**~~
- ~~Append-only JSONL datastore (runs, models, agents, games, positions, evaluations, labels) **(P1)**~~
- ~~Canonical position key (packed encoding + 64-bit hash, optional mirror fold) **(P1)**~~
- ~~`train.exe` CLI + `build_train.bat` + `tools/run_train.ps1` **(P1)**~~
- ~~`tests/test_ml.cpp` **(P1)**~~
- ~~Auto-exported docs (registries -> tables in ML.md + manifest) **(P1)**~~
- Python analysis layer (DuckDB queries: top Elo, fairest positions, avg eval per position)
- Python training (PyTorch) for MLP/NNUE/transformer, exporting C++-format weights
- Optional Weights & Biases tracking (local metrics by default, W&B opt-in)
- Hyperparameter study for the ML evaluators/policies
