# Done

Completed items moved out of todo.md, in the same order/sections they came from. Struck-through text is the original ask; unstruck text is the shipped outcome.

- ~~Make unit tests~~
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

## 1v1 / GUI Track
- ~~GUI hangs while an AI is thinking. Move the AI computation off the render thread (separate
  worker/shard), or at minimum render the move just played before starting to compute the
  reply, so the board updates instead of freezing~~ Done: the native build now runs
  the AI search on a background `std::thread` and renders from a main-thread view snapshot, so
  the window stays responsive (measured 0 freezes over 12s of depth-12 AI-vs-AI vs a fully
  frozen synchronous control). See `plans/gui-ai-thread-results-1-quiet-heron.md`.

## Agent Track
- ~~Asymmetric opener for `pairgen`: add `--open-plies-side a|b|both` so only ONE named agent plays random moves during the opener window while the other plays its own normal policy throughout.~~ Shipped as `pairgen --open-side a|b|both` (Theory 6 test, `plans/opener-bias-results-1-synchronous-stearns.md`). Finding: the symmetric opener DID inflate the champdil/dilution result (65% -> 40% once the champion plays its true policy) but NOT the oracle headline result (survives at ~66%). A third layer retrained the oracle on asymmetric-opener data and saw a large d6 drop (1137 -> 832), but that turned out to be confounded by training-label skew (win:loss ratio 2.55:1 -> 4.46:1), not a clean confirmation -- see the results doc. Also added the `rank.exe opener-bias` mechanism measure and the two study scripts (`tools/opener_bias_study.ps1`, `tools/opener_bias_retrain.ps1`)
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
  Developer confidence cells and Lemma B/C + D1-D14 proof review still open

## Models (value head: board -> scalar)
- ~~Linear value model **(P1)**~~
- ~~MLP value model (1-2 hidden layers, hand-written forward pass)~~ Shipped as `MLPModel`
  (`src/ml_model.cpp`): fully-connected, ReLU hidden + linear output logit, hand-written forward
  AND backprop, fan-in-scaled `initRandom`. Trained via `selfplay-supervised --model-type mlp
  --mlp-hidden "32"|"32,16"`. Full-scan leaf (not incremental -- NNUE below is the incremental
  step). See `plans/residual-mlp-results-1-tingly-chipmunk.md`
- ~~Residual/skip-connection value head: fix (or strongly regularize toward) a chip-count term as
  an additive skip connection into the head's output, i.e. output `chipCount + learned(board)`
  instead of `learned(board)` alone. Motivated directly by the Agent Track's current standing
  above ("the best agents so far are just chip counts"): if a learned head has to re-derive
  material counting from scratch, it may be spending capacity on something already known instead
  of on the harder residual, distinguishing and tie-breaking among positions with equal or
  near-equal material, which a pure material count cannot do at all. Natural target for the
  MLP/NNUE capacity jump above (the residual/nonlinear part could BE the MLP arm riding on a
  fixed linear chip skip), and kin to the Training Regimes "Tapered / phase-split PST" and
  "Weight symmetrization" ideas below, since all three inject known structure into the model
  rather than trusting a general learner to discover it. See theory 24, `Docs/theories.md`~~
  Shipped 2026-07-13 (HARD/frozen skip variant): `ResidualModel` wrapper = a frozen chip-count
  skip (`skipW*matDiff`, auto-calibrated from a material-only logistic pre-fit) + an inner
  `LinearModel` OR `MLPModel` that learns only the residual (the skip enters training as a frozen
  GLM offset). `selfplay-supervised --residual-skip -1` (0 off / >0 fixed / <0 auto). Linear inner
  stays fully incremental (`skipW*g_chipDiff` added at the leaf); MLP inner is full-scan. Measured
  by a stratified loss over `|matDiff|` buckets (theory 24). Soft/regularized skip and a broader
  hand-crafted baseline stay open. See `plans/residual-mlp-results-1-tingly-chipmunk.md`
- ~~Substrate shipped: sparse piece-square value features (v2, 129 binary inputs) plus the
  `g_mlAcc` scalar accumulator, updated by 2-3 weight adds per make/unmake and read at the
  leaf by `mlLeafScore`. A linear v2 model is an incremental PST with zero approximation
  (~9x lower cost per node than the v1 full-scan learned leaf). The NNUE step is widening
  the scalar to a vector and adding hidden layers on the same seams.~~ (sub-item of the
  incrementalize-an-ML-model entry below)
- ~~**Sparse leaf-tail forward.** The `mlp-sparsity` measurement found the dist MLP heads
  are ~90% dead-ReLU per position with only ~10-12% activation churn per move (theory
  36) -- NOT the dense ~50%-active heads the plan assumed.~~ Shipped same session:
  `MLPModel::forwardFromHidden` sums each remaining layer only over its nonzero inputs,
  bit-identical (adding `0*w` never changes a float sum), skipping ~90% of the dominant
  second-layer matmul. Measured 7.1x on top of the first-layer accumulator (2.84 us/node,
  **12.7x vs full-scan** for the wide head), meeting theory 36's predicted ~8-9x ceiling.
  d6/nb200k is now affordable for the wide head (~0.57 s/move, down from ~7.2 s).
- ~~**NNUE-shaped head** (`129 -> 512 -> 8 -> 1`: wide first layer, tiny rest): train + rate.~~
  Trained (6 seeds) and measured 2026-07-25. Prediction is neutral (held-out MAE 143.5 /
  NLL 0.408, matching the wide head) and Elo lands in the MLP band (6 seeds 908-1037, mean
  ~973, top seeds reach dist_lin 1038, 2026-07-25 refit), but the EFFICIENCY premise
  FAILED: at 2.47 us/node it is ~2x SLOWER per node than the 128/64 head (1.24) and only
  ~13% faster than wide (2.86). Root cause: once the accumulator makes the first-layer
  UPDATE free, the leaf still READs + ReLUs all H first-hidden pre-activations (O(H) scalar
  work), now the dominant leaf cost, and the NNUE shape has the LARGEST H. The ~17x-cheaper
  tail (MACs) was never the bottleneck (theory 37). The shape is not wrong, it is blocked on
  the vectorized read below. See `plans/nnue-shaped-head-results-1-brisk-walrus.md`.
- ~~**Vectorized (SIMD) leaf read for wide first layers.**~~ DONE 2026-07-25. AVX2 intrinsics
  added under `#if defined(__AVX2__)` (scalar path preserved under `#else`): vectorized double
  accumulator read + ReLU (`ml_eval.cpp` / `ml_model.cpp`) and a dense-FMA tail gated to
  small output width (out <= 32; sparse gather kept for wide H2, which a uniform dense tail
  regressed). Measured (us/node, d4): std 1.24 -> 1.00 (1.24x), NNUE 2.47 -> 1.89 (1.31x),
  wide 2.86 -> 2.43 (1.18x); all 2002 tests pass under the AVX2 build. KEY finding: a real
  ~1.2-1.3x leaf speedup for EVERY shape, but a constant factor that does NOT change the
  ranking, so NNUE-wide stays ~1.9x slower than the narrow standard head. The
  `/arch:AVX2`-flag-only build (no code) gave just ~10% (the branchy gather + mixed
  double/float read do not auto-vectorize). The vectorized path is opt-in via `/arch:AVX2`;
  both paths are in-tree.

## Models (policy head: board + move -> score / move-rater)
- ~~Linear move-rater **(P1)**~~

## Board-State Evaluators (BSEFs)
- ~~Classic (done)~~
- ~~Experimental (done)~~
- ~~LearnedValue: wraps a value model **(P1)**~~

## Heuristic Evaluator Feature Ideas (Classic / Experimental)
- ~~Tiny per-position random noise term, keyed by a seed rather than a hand-tuned weight (or
  optionally both: a seed for which positions get a nudge, plus a small weight to control how
  much). Dominated by the real evaluation so tactics still win, but breaks ties and re-sorts
  move ordering within near-equal branches, producing a distribution of "random-ish" board
  states distinct from picking uniformly among random legal moves~~ Shipped twice, selected by
  the Noise param's sign (NoiseSeed = seed, both deterministic). n > 0 = seeded random PST:
  refuted as a tie-breaker on the main-roster instrument (not dominated at material scale by
  the dominance walk; ~-240 Elo even at chip=80 where sibling material flips are impossible).
  n < 0 = the bounded per-position jitter with the tie-only-by-construction scaling
  (realEval*256 + jitter): provably never reverses a strict preference (dominance walk asserts
  0 reversals), works as a deterministic diversity knob (per-seed distinct play, byte-identical
  replays), and costs 0 to ~80 Elo at d6/nb200k, at least partly because breaking ties reduces
  alpha-beta cutoffs (+64% nodes/move) and node-budgeted heads convert that to lost depth.
  Theory 20 has the full split. See `plans/bounded-jitter-results-1-buzzing-floyd.md`
- ~~Reward more advanced mid-column pieces relative to outer-column pieces at the same row
  (a center piece is harder to wall off since it has two escape diagonals instead of one)~~
  Shipped (Center, `e`)
- ~~Penalize holes in the back rank: an empty back-rank square the opponent can walk a piece into
  for the win, distinct from the existing wall/column terms which only look at same-color
  adjacency, not the vulnerability of the square itself~~ Shipped (Hole, `h`, the D10 form)
- ~~Mobility: count of legal (or unblocked) forward/diagonal moves available, as a tempo/
  flexibility proxy, incrementally maintainable since a move only changes mobility near the two
  touched squares~~ Shipped (Mobility, `m`). Caveat from the speed ladder: its local delta is
  SLOWER than a per-leaf full scan when it is the only enabled term (+31 to +68%), because the
  bounding-box delta is paid at every make/unmake; incrementality pays off in multi-term mixes
- ~~Diagonal phalanx / triangle formation: reward two pieces diagonally adjacent on the same
  forward-facing diagonal, a mutual-support pattern distinct from the orthogonal wall and
  same-file column terms~~ Merged into Support (`d`) above, same pair geometry
- ~~Breakthrough-square control: reward occupying or defending the squares 1-2 rows from the
  opponent's back rank, the actual contested win squares, rather than generic "forward"~~
  Shipped (Control, `b`, occupancy form; "defending those squares" is not implemented)
- ~~Column-emptiness asymmetry: penalize a file that's empty on your side but populated on the
  opponent's, since it's a clear lane for their advance~~ Shipped (Open, `o`)
- ~~Overextension penalty: an advanced piece with no defender and no retreat option, distinct
  from the plain "forward" reward, which doesn't discriminate a supported advance from a bare
  one~~ Shipped (Overext, `x`)

## Move Choosers / Policies (direct, no search)
- ~~Human, UniformRandom, TieredRandom, SmartRandom (done)~~
- ~~LearnedPolicy: argmax of the move-rater **(P1)**~~

## Move-Tree Explorers (search)
- ~~Greedy 1-ply **(P1)**~~
- ~~AlphaBeta minimax (done; wrapped as an explorer **(P1)**)~~
- ~~Iterative deepening (shipped: used by the node-budgeted search)~~
- ~~Time-budgeted search by wall-clock seconds (`g_timeBudgetMs`, shipped)~~
- ~~Transposition table + move ordering (killers/history) shipped as opt-in, ablatable features (`useTT`/`useMoveOrder`); aspiration windows too (`aspirationWindow`)~~
- ~~Quiescence search (extend on captures / near-wins)~~ Shipped 2026-07-17 as the
  opt-in `qs` head flag (`quiesceMax/Min`, captures-only stand-pat extension at
  depth leaves; near-wins covered by the existing per-qnode `canWin*` sentinels).
  Strength verdict at the d6/nb200k head: no dethrone -- s98+qs ties plain s98
  pooled (1073 vs 1074) and loses the direct pair 9-23; classic+qs +19 within
  noise. Theory 29; `plans/dethrone-champion-results-2-wiggly-mitten.md`. The
  runner-threat extension variant remains open

## Training Regimes
- ~~Supervised on self-play outcomes (value) **(P1)**~~
- ~~Imitation / behavioral cloning from a stronger agent (policy) **(P1)**~~
- ~~Pool-pair game generation: generate FRESH training games by pairing agents from the
  rank.exe pool (instead of one teacher's self-play), with the dilution-decay schedule
  overriding each agent's own dilution. Combines replay's diverse-teachers win (~+250
  Elo) with unlimited new data, decoupling training-set size from the stored match
  history~~ (shipped: `rank.exe pairgen` plays any two canonical IDs with a per-side
  dilution override, random opening plies, a winner filter, and branch-from-win mining.
  First use = the vs-champion training study, `tools/train_vs_champion.ps1`.)
- ~~Validation split + early stopping instead of the fixed 6-epoch folklore cap~~ Shipped:
  `selfplay-supervised --val-split <f>` holds out that fraction (deterministic by seed), prints
  per-epoch `val=` loss, and computes the final stratified loss on the held-out set (so the
  theory-24 equal-material measure becomes a generalization number). `--early-stop` keeps the
  lowest-validation-loss epoch as the saved model. See
  `plans/residual-mlp-results-2-tingly-chipmunk.md`
- ~~**Explain why removing the Pass-2 cohort games hurt the chip counter.**~~ ANSWERED
  2026-08-01 (developer's hypothesis, then measured with the new `rank.exe matchup`). The
  TD-Leaf cohort was initialised from a learned champion and self-played, so it specialised
  against THAT style: it beat `learned-other` agents 70.5% but the chip counter only 63.6%.
  The chip counter's matchup was **comparatively** favourable, not absolutely -- it still lost.
  That bloc was ~30% of the store, so Bradley-Terry set the chip counter's single strength
  parameter to explain those 196,767 games and mispriced it elsewhere: `classic` vs
  `pool_games` ran a -6.0 residual with the cohort in, and -2.7 with it out. Dropping the
  cohort more than halved the worst large-sample miscalibration. Elo cannot represent this;
  it is a transitivity violation, not sampling noise.
- ~~Elo-tie labeling: label a position by the interpolated Elo E* at which expected score = 0.5~~
  Shipped as the position-oracle label pipeline (rank.exe posgen/label/labelfit + train.exe
  dist-value, `plans/position-oracle-plan-1-lazy-popping-simon.md`): each position's measured
  mu IS the Elo gap at which expected score = 0.5 (sign flipped), and the pipeline adds the
  volatility SD on top. Theories 34 and 35 track the oracle-prediction and sigma claims
- ~~Rate the 3 mlp dist variants at the d6/nb200k head, not just d4 -- developer expects a d6
  MLP to be the new champion~~ Done 2026-07-21, plus d2 added for all 4 models (full d2/d4/d6
  coverage). Result: the expectation did NOT hold. `dist_lin@d6` (1031) stays the strongest
  dist-model agent; all three MLPs at d6 (974/967/931) lose to it in play despite beating it on
  prediction (theory 27, reconfirmed a fourth time) -- none of the four beat the champion (1131)
  or oracle (1151) in this fit. Also surfaced an unexplained reversal: `dist_mlp_wide` is the
  best MLP at d4 (768) but the worst at d6 (931). Full table:
  `plans/position-oracle-results-1-lazy-popping-simon.md`, `ML.md`'s shipped-models section.

## Weight optimization / geometry mapping
- ~~Coordinate-free local search: a hill climber that re-centers on its best result, with
  random drastic restarts to escape local optima~~ (shipped: `tools/hill_climb.ps1`, a greedy
  hill climber over the Experimental weight mix with `gauntlet` Elo as fitness; drastic
  chip-weight resets provide the restarts. Parallel multi-climber not yet done.)
- ~~Always normalize/anchor one weight (e.g. chip) so the others are measured relative to it.~~
  (shipped in `hill_climb.ps1`: turn pinned at 20, chip/wall/column/forward renormalized to
  sum 80, so the search moves on a fixed-magnitude simplex and scalar duplicates dedupe.)
- ~~Test signed weights (negative forward, etc.) and structure x forward interaction explicitly.
  Specifically try negative wall/column weights with the hill climber: walls or columns might be
  slightly bad relative to diagonal defense structures (see the defended-pieces term in Heuristic
  Evaluator Feature Ideas above), if tying two pieces together side-by-side costs more in
  mobility than it returns in safety~~ Shipped as `hill_climb.ps1 -AllowNegative` (sign-flip
  mutations + signed drastic resets over all 13 Advanced weights). First A/B run (60 iters each,
  d4): the signed best kept a small negative Support (d-2) but within fitness noise; negative
  wall/column never survived acceptance; negative-chip (the capacity direction) was proposed 15
  times and decisively rejected (300-750 Elo below chip-positive mixes). The structure x forward
  interaction surface was not explicitly mapped (response-surface reporting still open below).
  See `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`

## Strength Dilution (to spread an Elo ladder)
- ~~Random-move probability **(P1)**~~
- ~~Depth cap **(P1)**~~
- ~~Stochastic depth dilution: play a shallower depth-N search P% of the time (`dil(rP,dN)`),
  a plausible-but-weaker move rather than a blunder~~

## Elo / Tournaments
- ~~**Re-certify the champion under the corrected instrument.**~~ Done 2026-07-26.
  13 book agents were added to `ranking/roster.txt`, played to 8 games/pair over the
  116-agent roster, boosted to 32 games/pair via a refreshed `ranking/roster_top.txt`,
  and refit on the full roster. **The throne changed:**
  `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1.opener(book,book=11)@1` at 1122 +/- 10
  dethroned `...classic(chip=100)@2.opener(book,book=2)@1`, now 1075 +/- 9, by 3.5
  combined SE. It is statistically tied with the 6-ply rung of its own ladder
  (`book=10`, 1110 +/- 10). `ranking/CHAMPION.md` updated and its UNVERIFIED banner
  cleared. The 8-games/pair fill had the OLD champion first at 1090 with book11 at
  1067, so the top inverted on boosting for the third time in this project.
- ~~**Decide whether book agents are champion-eligible.**~~ Resolved 2026-07-28:
  split rather than pick a side. The single throne is now 5 parallel category
  champions by opener loadout (openless / 4-book / 8-book / 4-random / 8-random),
  declared in `ranking/CHAMPION.md`. See that file for the category definitions,
  the eligibility rule, and each category's current champion.
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
- ~~Match store outgrew what a git host accepts (270 MB, over GitHub's 100 MB
  per-file limit, which blocked every push). Store is now PARTS + a live tail
  listed in `ranking/matches.index.txt`, grouped by who played each game via
  `rank.exe split`: rostered games committed (55 MB), retired-agent games kept
  on disk but gitignored (216 MB). Verified information-preserving: the refit
  returned byte-identical `ratings.tsv`/`standings.tsv`. See
  `plans/store-sharding-results-1-*.md`.~~
- ~~Agent IDs used bare integers whose meaning was ambiguous (`4-random` vs
  `4-book`: plies in one, a slot number in the other, and `d6`/`c4` two more
  quantities again).~~ Resolved 2026-08-05: **every number in an ID now carries a
  descriptive label joined by `=`** (`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2`).
  Legacy spellings still parse so the store keeps resolving; only the new form is
  emitted, and `rankUpgradeId()` rewrites stored ids on read. Weights became a
  SUBSET (omitted = registry default, a default-zero term hidden, a non-default
  zero kept, turn elided without `qs`/`part`, chip-only Classic written
  `chip=100`), and `rankDisplayId()` hides a CURRENT `@N` from console tables
  while every file keeps it. Migration verified per agent: 381 -> 381 distinct
  ids, 0 unmatched, 0 elo/games mismatches, Elo multiset identical against a
  rebuilt pre-migration binary. 269 ids across 12 docs migrated through
  `rank.exe canon`. See `plans/id-labelling-results-1-tidy-albatross.md`.
- ~~Decide what happens to the TD-Leaf screening games~~ Resolved 2026-08-01:
  **dropped from the fit permanently** (developer decision). Their three files
  were DELETED 2026-08-02 (never committed, so unrecoverable), their lines are
  out of `ranking/matches.index.txt`, and
  `matches.retired_other.*` remains tracked and loaded. The ranking is now
  reproducible from the repo alone, which was the point. Cost, measured: the
  change moved 153 of 170 rostered agents and re-certified the openless
  champion. The old population can no longer be restored: the files are gone, so
  the pre-drop fit is not reproducible and that comparison is now historical only.
- ~~Model file format (text, `type=`/`head=` header) **(P1)**~~
- ~~Model slots so White/Black can use different models in one process **(P1)**~~
- ~~Append-only JSONL datastore (runs, models, agents, games, positions, evaluations, labels) **(P1)**~~
- ~~Canonical position key (packed encoding + 64-bit hash, optional mirror fold) **(P1)**~~
- ~~`train.exe` CLI + `build_train.bat` + `tools/run_train.ps1` **(P1)**~~
- ~~`tests/test_ml.cpp` **(P1)**~~
- ~~Auto-exported docs (registries -> tables in ML.md + manifest) **(P1)**~~
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
  jump (MLP/NNUE).
  **[SELF-PLAY CONVERGENCE UNSUPPORTED - see `Docs/corrections.md`]** That scaling
  study's self-play arm did NOT converge and
  must not be cited as a game-count ceiling. It is 4 rows (250 and 500 games, 2 seeds
  each) and stopped at 500 on a +16 mean gain under a 20-Elo rule, against within-size
  seed spreads of 94 and 72 Elo; nothing above 500 was ever run. Theory 45, and
  `plans/training-sweep-results-1-luminous-snail.md` item 3, which said so at the time.
  It also says nothing about ONLINE regimes: it measures a fixed teacher making a fixed
  distribution, which is why it saturates, and is the assumption TD-Leaf breaks.)
- ~~Gate the incremental heuristic path on nonzero weights: `evalBeginSearch` should
  leave `g_evalIncremental` false when wall == column == forward == 0, because the
  chip-count speed study found the accumulator maintenance is pure overhead there
  (v3 ran +18 to +35% slower than the full-scan fallback at the champion weights
  w0,l0). Eval values are identical by construction; the ladder in `train.exe speed`
  verifies the fix (v2->v3 at w0,l0 should become ~0%). See
  `plans/chip-count-speedup-results-1-iterative-raven.md`.~~ Shipped with the Advanced
  evaluator overhaul (`posWeightsActive` in `evalBeginSearch`, covering all sum-local
  weights): ladder v2->v3 at w0,l0 measured -0.5 to -3.6% (was +18 to +35%). See
  `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`.

