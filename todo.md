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

**Goal:** a learned evaluator that beats the classic chip counter at EQUAL search
depth, then at lower compute (deeper-for-cheaper). Yardstick in the current fit
(post vs-champion study, 2026-07): champion classic d6 = Elo 1140, best learned
PST at d6 = 1137 (`learned(s98)`, oracle-vs-champ data), a statistical tie at
+/- 22 SE and roughly equal cpu/move. The d8/nb2m oracle tops the table at 1267
(reference, not the target). Next Elo probably comes from more games at the top
(the tie is under-sampled at 8 games/pair), the refutation opening book, or a
capacity jump (MLP/NNUE) on the proven champdil + oracle data recipe.

**Standing loop:** the recurring success criterion for any new agent is dethroning
the current #1 in `ranking/ratings.tsv`, either by outrating it outright or by
countering its specific build/weaknesses (see the adversarial counter-agent idea
below). rank.exe's pool + gauntlet already measures this on every run.

Legend: **(P1)** = built in the first pass (versatility proof). Everything else is future work
against the same seams.

- Add variety in openers and moves `[Now]`
  - Either an opening book, arbitrary rewards for certain opening positions, or a separate opener model for training
  - Random move chooser out of top candidates (especially ties); the board-state-evaluator noise variant of this idea moved to the Heuristic Evaluator Feature Ideas section below
  - Elo-rate the existing SCRIPTED openers (Offensive/Defensive) as ID modules: an optional `op(o|d)@1` ID segment (absent = Standard), roster the champion build under each opener, and let the BT fit price the openers directly. Generalize beyond the 3 built-in openers: rate arbitrary candidate opening move sequences the same way (Elo given/taken vs the pool), and learn to prefer strong ones and avoid weak ones. The general version is a bigger project (developer's own estimate: "too much work for now") `[Next]` (built-in openers) / `[Later]` (arbitrary sequences). ~~A sibling idea for RANDOM (not scripted) openers shipped~~: a pluggable opener registry `g_openers[]` (`src/ai_random.h`) + `AgentSpec::openerKind`/`openerArg` + a `.opener(<kind>[,<arg>])@1` ID segment lets ANY agent be rostered/gauntletted both with and without an opener, so the Elo gap is a general, per-agent opener-sensitivity score (currently one kind, `rand`: e.g. champion 1140 clean vs 923 with `.opener(rand,6)@1`, champdil 1153 vs 962). The registry is exactly the extension point for the SCRIPTED (`off`/`def`) and opening-book openers above -- adding one is a table row + fn, and the same ID slot names it. See `Docs/agents.md` and `Docs/terminology.md`'s "Opener (identity-level)" entry `[done]`
  - Mine `matches.jsonl` for an opening book: tabulate the first 8-10 plies of >= 900 Elo games by `positionKey` with win rates and visit counts, emit a book file, and add a book-follower opener that plays the book move while in book `[Next]`
  - ~~Color-swap recovery test: play the same random-opener snapshot to conclusion twice with colors swapped, to separate "the position favors a color" from "this agent recovers better."~~ Shipped as `rank.exe opener-swap`. Champdil vs the champion at n=20 (theory 15, `Docs/theories.md`): 65% of outcomes were a color effect (White won both 55%, Black won both 10% -- consistent with the champion's own White/Black split), but in the remaining 35% agent-effect bucket champdil won every time (7/7), the champion never (0/7) -- promising but small-sample signal that champdil recovers from bad positions better than the champion, independent of color. Follow-up (larger sample, try with oracle too) filed in `plans/opener-bias-results-1-synchronous-stearns.md`'s Future Work `[Next]`
  - ~~Random-first-K-plies opening diversity for data generation~~ (shipped as pairgen `--open-plies`); extend the same knob to tournament and self-play generation `[Next]`
  - Sweep `--open-plies` length for the oracle-vs-champion training regime (0/2/4/6/8/12 tried against the single untested value of 6 used in the first vs-champion study), gauntlet-screen each, to check whether 6 was actually a good choice or just a guess `[Next]`
  - ~~Asymmetric opener for `pairgen`: add `--open-plies-side a|b|both` so only ONE named agent plays random moves during the opener window while the other plays its own normal policy throughout.~~ Shipped as `pairgen --open-side a|b|both` (Theory 6 test, `plans/opener-bias-results-1-synchronous-stearns.md`). Finding: the symmetric opener DID inflate the champdil/dilution result (65% -> 40% once the champion plays its true policy) but NOT the oracle headline result (survives at ~66%). A third layer retrained the oracle on asymmetric-opener data and saw a large d6 drop (1137 -> 832), but that turned out to be confounded by training-label skew (win:loss ratio 2.55:1 -> 4.46:1), not a clean confirmation -- see the results doc. Also added the `rank.exe opener-bias` mechanism measure and the two study scripts (`tools/opener_bias_study.ps1`, `tools/opener_bias_retrain.ps1`) `[done]`
  - Learned opener: a policy head trained only on plies < 10 of high-Elo replay games, used as an opener module that hands off to the main brain once out of phase. Specific angle worth testing: train the opener on WINNING-line data (see the refutation-book idea below) and hand off to a cheaper depth-5 search after the opener phase, on the theory that a strong precomputed opening plus a shallower live search could beat the d6 champion for less total compute -- directly on-target for the session's "beat d6 without searching deeper" goal `[Later]`
  - Single-line refutation book (extended, more concrete version of the offline-refutation idea below): find ONE oracle-verified winning line against the champion for each color (2 lines total), play that fixed line against every other deterministic agent in the roster, and whenever an opponent deviates from the line (or the line stops applying), use the oracle to find a new winning continuation from that deviation point. Builds a small branching decision tree rooted at "beat the champion," tested for robustness against the whole pool, not just the champion. Consider separate White/Black book models plus a combined one `[Later]`
  - Offline refutation book against the champion: run deep budgeted searches (d8-d10, nb2m) on the champion's preferred opening lines (it is deterministic, so its lines are minable from games.tsv), store best replies keyed by `positionKey`. A book + d6 search agent then attempts the dethrone with LESS live computation by construction. The most promising follow-up for the standing dethrone goal `[Next]`
- Interpret board analysis
  - Which piece is most impactful to the current evaluation? `[Later]`
  - What's the cheapest strategy to beat each given bot/parameters, even if overfitted? `[Dream]`
  - What strategies could a human devise to beat a bot? `[Dream]`
  - Is attacking the center or attacking the edge the best? `[Dream]`
  - Is advancing through the center or the edge the best? `[Dream]`
  - Is keeping the hind pieces in place the best? `[Dream]`
- ~~Make a list of truths about Breakthrough (new doc, `Docs/axioms.md`, alongside the existing
  `Docs/theories.md` / `Docs/terminology.md`), in three tiers:~~
  1. ~~**Direct axioms** taken straight from the rules (e.g. a piece captures diagonally, moves
     straight forward onto an empty square).~~
  2. ~~**Optional axioms**: rule choices this project made that aren't inherent to "Breakthrough"
     as a family of games (e.g. board is 8x8, 2 starting rows per side) -- kept separate from
     tier 1 so a future variant (different board size, different starting depth) knows exactly
     what to swap.~~
  3. ~~**Derived truths**: logically proven from tiers 1-2 (e.g. no draws are possible).~~
  ~~Then a fourth, softer tier: **empirical truths**, general strategic claims that are NOT proven,
  only observed (e.g. "more material is good," inferred from evaluators that weight chip
  advantage heavily winning consistently). Mark each with a confidence level from the developer
  and from Claude separately (default the developer's confidence to N/A until filled in), plus
  the evidence/session it came from, so a claim can be revisited if later evidence contradicts
  it~~ Shipped as `Docs/axioms.md`: 13 direct axioms, 4 optional axioms each with a
  "what breaks if swapped" note, 14 derived truths proven from three lemmas (no-stalemate via
  the most-advanced-piece diagonal argument, strict progress via remaining advancement
  capacity, capture geometry -- giving termination, a 208-ply bound, no draws, no repeated
  positions, color-swap symmetry, Zermelo determinacy, winner-moves-last + game-length parity,
  passed-runner unstoppability, race arithmetic, back-rank outposts, irreversible
  material/phase, exactly 22 opening moves, earliest capture ply 5, 11-ply minimum game),
  7 empirical truths with dev (N/A) + Claude confidence columns and evidence pointers, plus a
  conventions section ("White = first mover" as a symmetry-fixing definition, not an axiom).
  Developer confidence cells and Lemma B/C + D1-D14 proof review still open `[done]`
- Build a search tool to bound/compute **distance-to-win**: the true, rules-respecting
  number of plies to a forced win from a position, as opposed to `Docs/axioms.md` Lemma
  B's naive "capacity" sum (which ignores blocking and whether a piece can actually reach
  its goal). First validation target: the standard start, since axioms.md D13 already
  hand-proves its minimum game length is exactly 11 plies (a witnessed tight bound) --
  a computational search should reproduce that number before trusting it on anything
  harder. The state space is too large to explore exhaustively, so the tool needs pruning:
  - Depth-first search so it returns a usable answer sooner rather than needing to finish
  - Explore non-capture moves before captures at each node (this cuts against the engine's
    normal capture-first move ordering, which is tuned for alpha-beta pruning under a
    different objective -- pin down and justify the reasoning for this tool specifically
    before relying on it)
  - A transposition table deduping by position so a position reached by multiple move
    sequences is only explored once. Pin this down carefully, it is easy to get backwards:
    depending on whether the goal is a short witness line or a proof of a lower bound, the
    table may need to keep the position's BEST (fewest plies) or WORST (most plies)
    arrival, not just whichever was seen last
  - Prove each pruning optimization sound (it cannot cause the search to miss a valid
    shorter game or invalidate a claimed bound) before relying on it, and prioritize
    optimizations with the biggest expected computation savings first
  - Embedded hypothesis to test once the tool exists: capturing an opponent piece that is
    one ply from winning is always an optimal reply, except when it is the last piece.
    (Ambiguous as stated -- clarify whether "it" is the threatened piece being the
    opponent's LAST piece, in which case the capture wins outright via A9/D6 rather than
    merely defusing a threat, or the capturing piece being the defender's OWN last piece
    needed elsewhere. Filed as theory 17 in `Docs/theories.md`.) `[Later]`

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
  - ~~Substrate shipped: sparse piece-square value features (v2, 129 binary inputs) plus the
    `g_mlAcc` scalar accumulator, updated by 2-3 weight adds per make/unmake and read at the
    leaf by `mlLeafScore`. A linear v2 model is an incremental PST with zero approximation
    (~9x lower cost per node than the v1 full-scan learned leaf). The NNUE step is widening
    the scalar to a vector and adding hidden layers on the same seams.~~
- Joint value + policy + next-value model trained to minimize its own recomputation `[Later]`
  - With ReLU units, a hidden unit that remains clamped at 0 before and after a move contributes
    no changed downstream value (equivalently, the derivative through the ReLU pre-activation is
    0 except at the kink). So add a penalty (L0, or an L1/sigmoid surrogate) on the count of
    hidden units whose output changes across the move the policy head picks. The model must
    jointly learn the board value, the best move, and the successor value, because which units
    must recompute depends on which move is played: it co-learns playing strength and its own
    recomputation cost, preferring representations where the lines it likes are also cheap.
  - Caveats: the sparsity term must stay a light regularizer or the model will prefer cheap moves
    over good ones. Alpha-beta explores all moves at a node, so architectural locality (conv or
    locally connected first layer) is what bounds worst-case per-move cost, while the learned
    clamping cheapens the chosen lines on top. Training needs the Python track (a custom
    three-head loss is beyond the C++ SGD in `ml_train.cpp`); inference plugs into the same
    `g_mlAcc`-style accumulator seams shipped above.

## Models (policy head: board + move -> score / move-rater)
- ~~Linear move-rater **(P1)**~~
- MLP policy `[Later]`
- Transformer policy `[Dream]`
- Softmax / temperature sampling over move scores (for exploration + diverse self-play) `[Now]`

## Models (difficulty head: board -> how hard is this position)
- Blunder labeling by branch replay: play a game between two strong deterministic
  agents, rewind to a move by the eventual winner, substitute a random different move,
  and replay from there. If the winner changes, label the substitute a blunder. Also a
  per-turn difficulty probe: turn difficulty = how few of the legal moves preserve the
  win. `[Later]`
- Position difficulty as a learned target: position difficulty = an aggregate of this
  turn's and the following turns' difficulty. A strong move lowers your future
  difficulty; a risky move raises it while still winning. Computing it exactly needs
  near-exhaustive tree exploration (intractable except trivial endgames), which makes
  it a natural ML target: generate labels by branch replay / sampling where it IS
  computable, train a model to predict it anywhere. `[Later]`
  - Uses: difficulty-aware move choice (prefer low-difficulty winning lines against a
    tricky opponent), rating the difficulty of puzzles/positions for humans,
    difficulty-aware time allocation in search.

## Adversarial / Opponent-Modeling Agents
- Counter-agent trained against one specific opponent: mine that opponent's alpha-beta
  search for moves it pruned early (cut off before full evaluation) and check whether
  continuing into that line actually wins. Train a policy that preferentially steers
  into an opponent's under-explored lines. Deliberately overfits to one opponent's
  build/pruning behavior, so it is fragile against everyone else by design. `[Later]`
  - Purpose is not a standalone strong agent: keep counter-agents in the rank.exe pool
    on purpose. They force every other agent, especially the reigning #1, to be robust
    not only to raw strength but to opponent-specific exploitation, the same way the
    dilution ladder forces robustness to weaker/noisier play.

## Board-State Evaluators (BSEFs)
- ~~Classic (done)~~
- ~~Experimental (done)~~
- ~~LearnedValue: wraps a value model **(P1)**~~
- Ensemble / blended evaluator (average or weighted mix of several evaluators/models) `[Later]`

## Heuristic Evaluator Feature Ideas (Classic / Experimental)
New candidate terms for the hand-crafted evaluators, alongside the existing chip/wall/column/
forward mix. Each is a single-place edit in `g_evaluators` (`src/ai_eval.cpp`) plus wiring the
term into the incremental `evalPosLocal` delta the same way wall/column/forward already are.
- Tiny per-position random noise term, keyed by a seed rather than a hand-tuned weight (or
  optionally both: a seed for which positions get a nudge, plus a small weight to control how
  much). Dominated by the real evaluation so tactics still win, but breaks ties and re-sorts
  move ordering within near-equal branches, producing a distribution of "random-ish" board
  states distinct from picking uniformly among random legal moves `[Now]`
- Reward more advanced mid-column pieces relative to outer-column pieces at the same row
  (a center piece is harder to wall off since it has two escape diagonals instead of one) `[Next]`
- Penalize holes in the back rank: an empty back-rank square the opponent can walk a piece into
  for the win, distinct from the existing wall/column terms which only look at same-color
  adjacency, not the vulnerability of the square itself `[Next]`
- Reward defended pieces: a diagonal-adjacency analog of the existing wall (orthogonal) and
  column (same-file) structure terms, since a diagonal neighbor is what actually recaptures
  after a capture in this game `[Next]`
- Mobility: count of legal (or unblocked) forward/diagonal moves available, as a tempo/
  flexibility proxy, incrementally maintainable since a move only changes mobility near the two
  touched squares `[Later]`
- Diagonal phalanx / triangle formation: reward two pieces diagonally adjacent on the same
  forward-facing diagonal, a mutual-support pattern distinct from the orthogonal wall and
  same-file column terms `[Later]`
- Breakthrough-square control: reward occupying or defending the squares 1-2 rows from the
  opponent's back rank, the actual contested win squares, rather than generic "forward" `[Later]`
- Column-emptiness asymmetry: penalize a file that's empty on your side but populated on the
  opponent's, since it's a clear lane for their advance `[Later]`
- Race-distance differential: (opponent's closest piece to your back rank) minus (your closest
  piece to theirs), a cheap proxy for who wins a pure race, ignoring tactics. Generalizes to the
  difference between the two sides' total remaining "capacity" (`Docs/axioms.md` Lemma B: the
  full-board sum of every piece's own row-distance to its own goal) as a candidate evaluator
  term or a key to sort/label/prioritize positions. Whether that difference is actually
  meaningful, rather than just re-tracking material, is theory 18 in `Docs/theories.md` `[Dream]`
- Overextension penalty: an advanced piece with no defender and no retreat option, distinct
  from the plain "forward" reward, which doesn't discriminate a supported advance from a bare
  one `[Dream]`

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
- Eval-blended labels: label each position with lambda*outcome + (1-lambda)*sigmoid(teacherEval/scale)
  instead of outcome alone. The teacher already computes a root search score every move and throws
  it away; blending turns one noisy bit per game into a real-valued signal per position (the NNUE
  training recipe) and should also improve move ordering, where the PST prunes 3x worse than
  Classic `[Now]`
- Weight symmetrization + seed-ensembling for linear models: after training, average each weight
  with its left-right mirror (exact symmetry projection, free variance cut), and average the
  weights of K seed-replicas (for a linear model the ensemble IS the average). Directly attacks
  the measured 50-150 Elo training-seed noise `[Now]`
- Extraction quality controls in rank.exe extract: --min-elo floor or Elo-confidence weighting
  (label quality), --exclude held-out agents (measure pool-style overfitting by comparing Elo vs
  held-in against held-out opponents; low risk for linear models, must exist before MLP/NNUE),
  and positionKey-based dedup / repeat capping (openings are massively overrepresented) `[Next]`
- ~~Pool-pair game generation: generate FRESH training games by pairing agents from the
  rank.exe pool (instead of one teacher's self-play), with the dilution-decay schedule
  overriding each agent's own dilution. Combines replay's diverse-teachers win (~+250
  Elo) with unlimited new data, decoupling training-set size from the stored match
  history~~ (shipped: `rank.exe pairgen` plays any two canonical IDs with a per-side
  dilution override, random opening plies, a winner filter, and branch-from-win mining.
  First use = the vs-champion training study, `tools/train_vs_champion.ps1`.)
- ~~Vs-champion training regime (first pairgen study): train value models on games
  involving the reigning champion, sourced every plausible way (learner vs champ,
  diluted champ vs champ, oracle vs champ, champion-loss cherry-picks, branch-mined
  winning lines), and compare against the replay/self-play baselines.~~ (done, see
  `plans/vs-champion-training-results-1-cozy-forest.md`. Headlines: diluted-champion
  vs clean-champion games are the best value-training data found so far (beats
  replay), oracle-vs-champ close behind, and the best model ties the champion at d6
  (1137 vs 1140). Cherry-picked one-sided datasets (champloss, branch-wins) fail from
  degenerate labels, and a d2 generator on one side drags data quality below the
  self-play control. Theory 1 (out-of-distribution fragility) refuted in the current
  pool, Theory 2 (dilution data can't approach the champ) refuted on strength but
  head-to-head unresolved at n=8.) Standing longitudinal check: after each future
  batch of diverse agents joins the pool, re-run `tools/train_vs_champion.ps1
  -AnalysisOnly` to re-test the out-of-distribution theory `[Now]`
- Tapered / phase-split PST: separate opening/endgame weight tables interpolated by piece count
  (piece count changes only on capture, so it stays fully incremental). The natural capacity step
  before MLP `[Next]`
- Validation split + early stopping instead of the fixed 6-epoch folklore cap `[Next]`
- PV/leaf position harvesting: train on positions from inside the teacher's search tree labeled
  by subtree value, matching the off-path distribution the eval actually sees in search `[Later]`
- Active / hard-example mining: oversample positions where the current model most
  disagrees with the teacher label or with a deeper search, instead of uniform
  sampling `[Later]`
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
- Test signed weights (negative forward, etc.) and structure x forward interaction explicitly.
  Specifically try negative wall/column weights with the hill climber: walls or columns might be
  slightly bad relative to diagonal defense structures (see the defended-pieces term in Heuristic
  Evaluator Feature Ideas above), if tying two pieces together side-by-side costs more in
  mobility than it returns in safety `[Now]`
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
- Standing project loop: on every new agent, check whether it lowers the current #1's
  Elo, by outrating it outright or by countering its specific build (see the
  adversarial counter-agent idea above). Treat "dethrone the champion" as the
  recurring success criterion, not just "raise some Elo" in isolation `[Now]`
- Roster curation policy (interim, until the classifier below exists): keep `on` the
  anchor, the dilution ladder, the reigning champion family, one oracle, the best agent
  per distinct data-source family (replay, self-play, vs-champion, oracle-mimic,
  branch-mined), and any agent with a distinctive opponent-bucket profile (e.g. a
  counter-agent that beats the champ but loses broadly). Retire (`off`) near-duplicates
  whose head-to-head profiles match an existing agent, since their games stay in
  `matches.jsonl` forever `[Now]`
- Agent behavioral classifier: characterize agents by how they PLAY, not just Elo, and
  use it to decide which agents are interesting enough to keep active. Features:
  position-distribution overlap between agents (shared `positionKey` histograms over
  their stored games, so two agents reaching the same positions 99% of the time rate as
  near-identical), a left-right symmetry measure, and responses against a fixed
  discriminator agent as a feature vector. Cluster k-means-style and keep the most
  interesting few per cluster. Big project, deliberately deferred `[Later]`

## Agent Composition + Play
- ~~AgentSpec (explorer + evaluator + chooser + model slots + dilution) **(P1)**~~
- ~~Saved agent library file **(P1)**~~
- Determinism classification: classify each agent component (explorer, evaluator, chooser,
  dilution) as deterministic or not, and classify each agent as the LEAST deterministic of
  its components (any random part makes the whole agent non-deterministic). For a
  deterministic-vs-deterministic pairing, one game per color is provably sufficient --
  repeats are guaranteed identical -- so the scheduler (`rankSchedule`) and `pairgen` could
  skip redundant games instead of replaying the same game `gamesPerPair` times. Motivated by
  a real bug found this session: an early `pairgen` head-to-head eval used no dilution/opening
  randomness and silently replayed only 2 distinct games ~120 times each, producing a
  misleadingly exact 50/50 split. Unit test: build a randomly-configured deterministic agent,
  play it against itself/an opponent twice, assert repeated games are identical (final
  position at minimum; the full move sequence if cheap, since compute is dominated by the
  search itself, not the check) `[Next]`

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
- ~~Provenance gap: the model file's teacher= string omits the dilution-decay parameters
  (--gen-random-floor / --gen-random-decay-plies) and the --from-data source, so two
  differently-trained models can carry identical provenance. Record the full recipe~~
  (self-play teacher= now appends `dil(start->floor/Np)`, --from-data was already
  recorded as `replay:<file>`, and pairgen datasets carry a full-recipe
  `<out>.meta.json` sidecar)
- ~~Hyperparameter study for the ML evaluators/policies~~ (done for the linear v2 PST:
  78 models across teacher depth x games x dilution(+decay) x bootstrap x replay-data x L2,
  rated in one Bradley-Terry fit; see `plans/training-sweep-results-1-luminous-snail.md`.
  Headlines: training-seed noise (50-150 Elo) dominates every axis, more data helps ~+38,
  teacher depth is irrelevant, dilution decay is the best default, replay extraction from
  the rank pool beats bespoke self-play for free, and the linear class itself is the
  ceiling. The follow-up scaling study (`tools/train_scaling.ps1`) then showed replay
  training on the grown 46k-game store beats single-teacher self-play by ~250 Elo:
  best model d6 Elo 920, promoted to `models/pst_value.txt`. Redo only after a capacity
  jump (MLP/NNUE).)
- Gate the incremental heuristic path on nonzero weights: `evalBeginSearch` should
  leave `g_evalIncremental` false when wall == column == forward == 0, because the
  chip-count speed study found the accumulator maintenance is pure overhead there
  (v3 ran +18 to +35% slower than the full-scan fallback at the champion weights
  w0,l0). Eval values are identical by construction; the ladder in `train.exe speed`
  verifies the fix (v2->v3 at w0,l0 should become ~0%). See
  `plans/chip-count-speedup-results-1-iterative-raven.md`.
