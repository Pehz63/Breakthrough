# Todo

Priority tags: `[Now]` = active focus, `[Next]` = queued up, `[Later]` = valuable but not soon, `[Dream]` = fun idea, not currently planning to do it. The two tracks below are prioritized independently, a `[Now]` in one track says nothing about the other.

- ~~Make unit tests~~
- Keep optimizing
- ~~Improve board state evaluator with machine learning~~ (Phase 1 shipped: see the Agent Track below)
- ~~Make a GUI raylib + raygui~~
- ~~Display the board state evaluation for each AI in the main board area~~
  - ~~For tree search or other algorithms (like minimax), show both immediate evaluation and the AI's predicted downstream evaluation~~
- ~~Board state evaluator selector for heuristic, NN, or other BSEFs~~
- ~~Depth time budget for minimax (so I specify 10 seconds per move and it will stop calculating after going deep enough to do ~10 seconds)~~
  - ~~Per-move **node** budget (`g_nodeBudget`) with iterative deepening shipped (used by the tournament).~~
  - ~~Wall-clock **time** budget (`g_timeBudgetMs`) shipped: `--time-budget-ms` / per-agent `timeBudgetMs`, composes with the node budget.~~
  - ~~Budget is now decoupled from depth: per-agent `nodeBudget`/`timeBudgetMs` + an unbounded-depth budget ladder (`--budgets`), so a budget sweep varies strength. Per-move telemetry reports fractional effective depth (e.g. 5.7), which cap ended the search (node/time/depth), nodes/move, and branching-factor distribution.~~
- ~~Parameter study for classic board state evaluator (turn weight calibrated via `train.exe turn-swing`; wider chip/structure presets + `--ablate` feature comparison shipped)~~

---

# 1v1 / GUI Track

- Display whose turn it is in the main board area `[Next]`
- Best moves list or recommendation arrow `[Later]`
- GUI: play against a named saved agent `[Next]`
- GUI: agent-vs-agent ladder / leaderboard view `[Later]`

## Pondering / 1v1 Performance
- Run the side-bar evaluator's minimax at iterative deepening with no node or depth limit
  while waiting for the human's turn. Only use this for the visual evaluation readout, not
  the move actually played `[Now]`
- Pipeline eval: instantly show a precomputed eval, then precompute the response and
  resulting eval for every possible next move in the background while waiting for the
  opponent's turn (can be parallelized) `[Next]`
- Parallelize more computations `[Later]`
- New "rush" mode: Player B is an agent that runs iterative deepening against all of Player
  A's possible moves while waiting for A to move. On B's turn it plays the best move it
  already found for A's actual move, so it responds instantly with hardly any computation.
  The longer A takes, the deeper B has precomputed `[Next]`

---

# Agent Track (rank.exe / ML / Search)

A composable system of interchangeable parts. An **Agent** = a **Move Chooser** (its "brain"),
which is either a **Search** (a **Move-Tree Explorer** + a **Board-State Evaluator**) or a
**Policy** (a direct, no-lookahead move picker: a heuristic or a learned move-rater). Each axis
is a registry, so adding one is a single table entry + a function body, and everything
(UIs, tournaments, docs) picks it up automatically.

Legend: **(P1)** = built in the first pass (versatility proof). Everything else is future work
against the same seams.

- Add variety in openers and moves `[Now]`
  - Either an opening book, arbitrary rewards for certain opening positions, or a separate opener model for training
  - Random move chooser out of top candidates (especially ties), or add random noise to the board state evaluator that's usually dominated by the actual evaluation so the whole tree is sorted randomly (jitter the eval to weaken without full randomness)
- Interpret board analysis
  - Which piece is most impactful to the current evaluation? `[Later]`
  - What's the cheapest strategy to beat each given bot/parameters, even if overfitted? `[Dream]`
  - What strategies could a human devise to beat a bot? `[Dream]`
  - Is attacking the center or attacking the edge the best? `[Dream]`
  - Is advancing through the center or the edge the best? `[Dream]`
  - Is keeping the hind pieces in place the best? `[Dream]`

## Models (value head: board -> scalar)
- ~~Linear value model **(P1)**~~
- MLP value model (1-2 hidden layers, hand-written forward pass) `[Next]`
- Convolutional NN value model (board as an 8x8xC grid; local spatial filters for walls/columns/forwardness) `[Later]`
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`) `[Later]`
- Transformer value model (squares as tokens) -- teacher / label generator only, not in-search `[Dream]`
- Incrementalize an ML model (e.g. MLP/NNUE) so a move recomputes only the few inputs it changed
  instead of the whole forward pass. Explore encouraging fewer recalculations per move by having
  hidden terms cancel/zero out (e.g. ReLU gating so unchanged regions contribute a constant), the
  way the heuristic `g_evalPos` already does a true neighbor-local delta. `[Later]`

## Models (policy head: board + move -> score / move-rater)
- ~~Linear move-rater **(P1)**~~
- MLP policy `[Later]`
- Transformer policy `[Dream]`
- Softmax / temperature sampling over move scores (for exploration + diverse self-play) `[Now]`

## Board-State Evaluators (BSEFs)
- ~~Classic (done)~~
- ~~Experimental (done)~~
- ~~LearnedValue: wraps a value model **(P1)**~~
- Ensemble / blended evaluator (average or weighted mix of several evaluators/models) `[Later]`

## Move Choosers / Policies (direct, no search)
- ~~Human, UniformRandom, TieredRandom, SmartRandom (done)~~
- ~~LearnedPolicy: argmax of the move-rater **(P1)**~~
- Greedy-by-eval (1-ply pick of the move maximizing a BSEF) **(P1, via the Greedy explorer)**
- Softmax/temperature sampling policy (probabilistic move choice) `[Now]`

## Move-Tree Explorers (search)
- ~~Greedy 1-ply **(P1)**~~
- ~~AlphaBeta minimax (done; wrapped as an explorer **(P1)**)~~
- ~~Iterative deepening (shipped: used by the node-budgeted search)~~
- ~~Time-budgeted search by wall-clock seconds (`g_timeBudgetMs`, shipped)~~
- ~~Transposition table + move ordering (killers/history) shipped as opt-in, ablatable features (`useTT`/`useMoveOrder`); aspiration windows too (`aspirationWindow`)~~
- Quiescence search (extend on captures / near-wins) `[Next]`
- MCTS / PUCT (pairs a policy head with a value head) `[Later]`
- TT speedup is currently node-count-real but wall-clock-muddied by `positionKey`'s per-node string build; an incremental Zobrist hash would make the TT a wall-clock win too `[Next]`

## Training Regimes
- ~~Supervised on self-play outcomes (value) **(P1)**~~
- ~~Imitation / behavioral cloning from a stronger agent (policy) **(P1)**~~
- TD-Leaf(lambda) self-play bootstrap (value) `[Later]`
- Population / other-play tournaments as a data source, including an evolutionary variant:
  each round, mutate the top-couple-Elo agents (unique random perturbations of their weights)
  into new agents, add them to the round-robin, drop the weakest, and iterate -- so the
  population crawls the weight surface by selection `[Now]`
- Elo-tie labeling: label a position by the interpolated Elo E* at which expected score = 0.5 `[Dream]`
- Distillation from deep search or from a teacher model `[Later]`

## Weight optimization / geometry mapping
A single per-weight sweep is insufficient: each weight only matters RELATIVE to the others
(if chip=300, then forward 1 vs 2 vs 10 is indistinguishable), interactions are non-obvious
(forward may need to be HIGHER when structure is high, to offset the structure lost by
advancing; forward could even be NEGATIVE to keep pieces back and advance together), and the
optimum is a surface, not a point. Replace single sweeps with a search that maps the geometry:
- ~~Coordinate-free local search: a hill climber that re-centers on its best result, with
  random drastic restarts to escape local optima~~ (shipped: `tools/hill_climb.ps1`, a greedy
  hill climber over the Experimental weight mix with `gauntlet` Elo as fitness; drastic
  chip-weight resets provide the restarts. Parallel multi-climber not yet done.)
- ~~Always normalize/anchor one weight (e.g. chip) so the others are measured relative to it.~~
  (shipped in `hill_climb.ps1`: turn pinned at 20, chip/wall/column/forward renormalized to
  sum 80, so the search moves on a fixed-magnitude simplex and scalar duplicates dedupe.)
- Test signed weights (negative forward, etc.) and structure x forward interaction explicitly `[Now]`
- Report a response surface, not a single recommended value `[Next]`

## Strength Dilution (to spread an Elo ladder)
- ~~Random-move probability **(P1)**~~
- ~~Depth cap **(P1)**~~
- ~~Stochastic depth dilution: play a shallower depth-N search P% of the time (`dil(rP,dN)`),
  a plausible-but-weaker move rather than a blunder~~

## Elo / Tournaments
- ~~Round-robin + Elo rating **(P1)**~~
- ~~Checkpoints saved + manifest (JSON + Markdown) **(P1)**~~
- ~~Parallel (process-sharded) depth-laddered round-robin with per-move timing + champion export~~
- ~~Roster subset via `-Only` agent-name allowlist (subset runs preserve the full-roster `library.txt`)~~
- ~~Per-run archive (`runs/<id>/` config + elo.tsv + notes.md + results, `runs/index.jsonl` log) + agent registry (`agents/registry.{jsonl,md}` union with a `spec_hash` that flags retrains/changes) + `run-note` for later annotations~~
- ~~Gauntlet vs fixed anchors~~ (rank.exe gauntlet: one candidate vs the frozen pool, O(N) games)
- ~~BayesElo-style rating with uncertainty~~ (rank.exe: anchored Bradley-Terry MLE + Fisher standard errors)
- ~~Persistent incremental Elo ranking (rank.exe: canonical agent IDs, editable roster with on/off toggles, append-only match store, anchored BT refit, per-agent head-to-head reports)~~

## Agent Composition + Play
- ~~AgentSpec (explorer + evaluator + chooser + model slots + dilution) **(P1)**~~
- ~~Saved agent library file **(P1)**~~

## Data + Infrastructure
- ~~Model file format (text, `type=`/`head=` header) **(P1)**~~
- ~~Model slots so White/Black can use different models in one process **(P1)**~~
- ~~Append-only JSONL datastore (runs, models, agents, games, positions, evaluations, labels) **(P1)**~~
- ~~Canonical position key (packed encoding + 64-bit hash, optional mirror fold) **(P1)**~~
- ~~`train.exe` CLI + `build_train.bat` + `tools/run_train.ps1` **(P1)**~~
- ~~`tests/test_ml.cpp` **(P1)**~~
- ~~Auto-exported docs (registries -> tables in ML.md + manifest) **(P1)**~~
- Python analysis layer (DuckDB queries: top Elo, fairest positions, avg eval per position) `[Later]`
- Python training (PyTorch) for MLP/NNUE/transformer, exporting C++-format weights `[Later]`
- Optional Weights & Biases tracking (local metrics by default, W&B opt-in) `[Dream]`
- Hyperparameter study for the ML evaluators/policies `[Next]`
