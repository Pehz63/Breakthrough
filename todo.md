# Todo

- ~~Make unit tests~~
- Keep optimizing
- ~~Improve board state evaluator with machine learning~~ (Phase 1 shipped: see the Modular ML System Backlog below)
- ~~Make a GUI raylib + raygui~~
- ~~Display the board state evaluation for each AI in the main board area~~
  - ~~For tree search or other algorithms (like minimax), show both immediate evaluation and the AI's predicted downstream evaluation~~
- Display whose turn it is in the main board area
- ~~Board state evaluator selector for heuristic, NN, or other BSEFs~~
- Depth time budget for minimax (so I specify 10 seconds per move and it will stop calculating after going deep enough to do ~10 seconds)
- Parameter study for classic board state evaluator (for ~3, ~10, ~30 second budgets per move)
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
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`)
- Transformer value model (squares as tokens) -- teacher / label generator only, not in-search

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
- Iterative deepening
- Time-budgeted search (ties to the "depth time budget for minimax" todo above)
- Quiescence search (extend on captures / near-wins)
- MCTS / PUCT (pairs a policy head with a value head)

## Training Regimes
- ~~Supervised on self-play outcomes (value) **(P1)**~~
- ~~Imitation / behavioral cloning from a stronger agent (policy) **(P1)**~~
- TD-Leaf(lambda) self-play bootstrap (value)
- Population / other-play tournaments as a data source
- Elo-tie labeling: label a position by the interpolated Elo E* at which expected score = 0.5
- Distillation from deep search or from a teacher model

## Strength Dilution (to spread an Elo ladder)
- ~~Random-move probability **(P1)**~~
- ~~Depth cap **(P1)**~~
- Evaluation noise (jitter the eval to weaken without full randomness)

## Elo / Tournaments
- ~~Round-robin + Elo rating **(P1)**~~
- ~~Checkpoints saved + manifest (JSON + Markdown) **(P1)**~~
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
