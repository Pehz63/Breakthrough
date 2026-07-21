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
- Add a button or something that changes the white and black pieces to red and blue in the GUI `[Next]`
- Best moves list or recommendation arrow `[Later]`
- GUI: play against a named saved agent `[Next]`
- GUI: agent-vs-agent ladder / leaderboard view `[Later]`
- ~~GUI hangs while an AI is thinking. Move the AI computation off the render thread (separate
  worker/shard), or at minimum render the move just played before starting to compute the
  reply, so the board updates instead of freezing `[Next]`~~ Done: the native build now runs
  the AI search on a background `std::thread` and renders from a main-thread view snapshot, so
  the window stays responsive (measured 0 freezes over 12s of depth-12 AI-vs-AI vs a fully
  frozen synchronous control). See `plans/gui-ai-thread-results-1-quiet-heron.md`.

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
depth, then at lower compute (deeper-for-cheaper). The reigning champion is
declared in `ranking/CHAMPION.md` (single source of truth; the numbers below are
tagged to their fit dates). **The first tier was achieved
and certified 2026-07-17** (`plans/dethrone-champion-results-1-wiggly-mitten.md`):
after boosting the top pairs to 32 games each and refitting, a learned PST
(`ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1`, trained on oracle-vs-champion
data) dethroned the classic chip counter, 1064 +/- 14 vs 976 +/- 13 (direct
head-to-head 23-9). The chip counter's weakness class is now known: d6-head
learned piece-square models beat it far above their pooled Elo (theory 28).
Fit-scale caveat: Elo is not comparable across fits as the pool grows (see
`Docs/benchmarking.md`). Three follow-up phases then did NOT dethrone s98 and
each refuted a theory: quiescence (phase 1, theory 29 -- s98+qs ties pooled,
loses the pair 9-23); the oracle-mined refutation book (phase 2, theory 14's
naive form -- both book agents rated below their bookless selves); weight
symmetrization/ensembling (phase 3, theory 30 -- mirroring the champion's own
weights alone cost 135 Elo). **A FOURTH attempt, 2026-07-18, DID dethrone**:
mining a book from the WEAK agent's OWN wins (not a stronger agent's wins over
it -- theory 33, the repaired form of theory 14) turned the classic chip
counter itself back into the reigning champion,
`ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,2)@1` at 1145 +/- 13,
25-7 against s98 directly and 27-5 against the d8/nb2m oracle it was never
mined against. **This has an open scrutiny flag** (`ranking/CHAMPION.md`,
results-5's "open scrutiny question"): it is a 134-entry hard-coded book on the
original evaluator, not a generalized strength improvement, and whether it is
genuine transferable strength or a pool-specific/memorization effect is
unresolved. The learned-eval fixes remain untried regardless of that question --
eval-blended labels, Elo-filtered extraction, seed SELECTION by Elo (the best
of 6 champion-recipe seeds is +28 over s98) -- alongside the now-open follow-ups:
apply the same self-mining fix to s98's own book, test whether "stronger
opponent" is load-bearing or any own-win suffices, runner-threat quiescence,
and the reset-state prerequisite. See the results docs (1-5) for full detail.

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
  - ~~Offline refutation book against the champion: run deep budgeted searches (d8-d10, nb2m) on the champion's preferred opening lines (it is deterministic, so its lines are minable from games.tsv), store best replies keyed by `positionKey`. A book + d6 search agent then attempts the dethrone with LESS live computation by construction.~~ Shipped 2026-07-17 as `rank.exe bookgen` (mines A's winning positions/moves from stored games) + the `book` opener (`.opener(book,<N>)@1` plays `models/book<N>.txt`). First verdict (mining the STRONG agent's wins over the weak target): REFUTED in this naive form (theory 14, `plans/dethrone-champion-results-3-wiggly-mitten.md`) -- both book agents rated ~16 Elo below their bookless selves. **Reversed 2026-07-18** (theory 33, `plans/dethrone-champion-results-5-wiggly-mitten.md`): mining the WEAK/book-wearing agent's OWN wins instead (zero new code, just swapped which agent's wins get kept) fixes the brain-portability failure by construction and DETHRONED s98 outright (classic+selfbook 1145 +/- 13 vs s98 1074 +/- 12, 25-7 head-to-head, 27-5 vs the oracle it was never mined against). Open scrutiny flag: genuine strength vs pool-specific effect, unresolved (`ranking/CHAMPION.md`). Remaining open repairs: a `--reset-state` mode for reproducible det-vs-det play (still needed, drift rate was 12/32 for this pairing); testing whether "opponent must be stronger" is load-bearing or any own-win suffices; applying the same self-mining fix to s98's own book; and the stay-in-book-to-the-win / response-tree variant via `--branch-tries`-style mining, which folds into the single-line refutation book idea above `[Next]`
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
- ~~MLP value model (1-2 hidden layers, hand-written forward pass)~~ Shipped as `MLPModel`
  (`src/ml_model.cpp`): fully-connected, ReLU hidden + linear output logit, hand-written forward
  AND backprop, fan-in-scaled `initRandom`. Trained via `selfplay-supervised --model-type mlp
  --mlp-hidden "32"|"32,16"`. Full-scan leaf (not incremental -- NNUE below is the incremental
  step). See `plans/residual-mlp-results-1-tingly-chipmunk.md` `[done]`
- Convolutional NN value model (board as an 8x8xC grid; local spatial filters for walls/columns/forwardness) `[Later]`
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`).
  Concrete next step now that `MLPModel` (full-scan) ships and beats the linear PST ceiling on
  offline equal-material calibration (theory 24): make its FIRST layer an incremental accumulator
  -- widen the scalar `g_mlAcc` that the linear inner already maintains into a vector, and
  recompute only the hidden units touched by the 2-3 changed inputs per make/unmake -- so the MLP
  can compete at FIXED compute. Motivated directly by the residual-mlp results' full-scan Elo
  caveat (its per-node eval is strong but per-second it is handicapped). `[Next]`
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
  hand-crafted baseline stay open. See `plans/residual-mlp-results-1-tingly-chipmunk.md` `[done]`
- Residual skip design space (follow-up to the shipped HARD frozen chip skip; theory 24,
  `plans/residual-mlp-results-1-tingly-chipmunk.md`). The linear residual HELPED + stabilized
  equal-material calibration but the MLP residual was a WASH, so probe whether a softer/richer
  skip or more capacity changes that split: (a) SOFT / regularized skip -- a learnable material
  weight initialized at the material-only logistic fit and penalized for drifting, vs the hard
  frozen one (theory 24 Q1); (b) a BROADER hand-crafted baseline as the skip (e.g. the Advanced
  linear mix) instead of the literal chip differential (theory 24 Q2); (c) a wider / DEEPER MLP
  capacity sweep (2 hidden layers, more widths) to map where added capacity stops improving
  calibration. All measurable with the existing stratified-loss printout + the generalized
  `sweep_pst_v2.ps1` groups. `[Next]`
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
- Phase-conditioned mixture of experts: a lightweight router (start with a hand-fixed classifier
  on total material/piece count: high piece count = opener, low piece count = endgame, graded
  band in between) that dispatches evaluation to a phase-specialized model (separate opener/
  midgame/endgame value or policy models) instead of one model covering the whole game. Distinct
  from the Tapered/phase-split PST idea in Training Regimes below, which smoothly interpolates
  ONE linear model's weights by piece count: this is hard routing between genuinely separate
  models (potentially different architectures per phase), a phase-conditioned specialization of
  the Ensemble/blended evaluator idea above (routing instead of uniform blending) and akin to the
  "separate opener model" idea in the Agent Track above. Router could start as the fixed
  material-band classifier and later become learned/soft (e.g. blending adjacent-phase experts
  near the boundary instead of a hard cutoff). Motivated by the developer's hypothesis that
  Breakthrough has genuinely distinct phases with different best strategies, not just a smoothly
  varying one. See theory 25, `Docs/theories.md` `[Now]`

## Heuristic Evaluator Feature Ideas (Classic / Experimental)
New candidate terms for the hand-crafted evaluators, alongside the existing chip/wall/column/
forward mix. ~~Each is a single-place edit in `g_evaluators` (`src/ai_eval.cpp`) plus wiring the
term into the incremental `evalPosLocal` delta the same way wall/column/forward already are.~~
Batch 1 shipped 2026-07-11 as the **Advanced** evaluator (`adv`, 16 params, all terms below
plus the D14 RaceWin detector; see `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`).
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
  Theory 20 has the full split. See `plans/bounded-jitter-results-1-buzzing-floyd.md` `[done]`
- ~~Reward more advanced mid-column pieces relative to outer-column pieces at the same row
  (a center piece is harder to wall off since it has two escape diagonals instead of one)~~
  Shipped (Center, `e`)
- ~~Penalize holes in the back rank: an empty back-rank square the opponent can walk a piece into
  for the win, distinct from the existing wall/column terms which only look at same-color
  adjacency, not the vulnerability of the square itself~~ Shipped (Hole, `h`, the D10 form)
- ~~Reward defended pieces: a diagonal-adjacency analog of the existing wall (orthogonal) and
  column (same-file) structure terms, since a diagonal neighbor is what actually recaptures
  after a capture in this game~~ Shipped (Support, `d`), merged with the phalanx idea below --
  both describe the same diagonal-pair geometry; a saturating per-piece variant stays open
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
- ~~Race-distance differential: (opponent's closest piece to your back rank) minus (your closest
  piece to theirs), a cheap proxy for who wins a pure race, ignoring tactics.~~ Shipped (Race,
  `r`, from incrementally maintained per-row piece counts), plus a stronger sibling: RaceWin
  (`g`), the exact D9/D14 decided-race sentinel detector (proven sound, ~8% us/move, no
  measurable Elo at d6 -- theory 21). The capacity generalization is settled analytically:
  capacity advantage == forwardSum - 7*chipDiff exactly (code-verified identity), so it is
  linearly dependent on existing terms and was NOT added as a weight; theory 18's
  outcome-correlation half stays open, `capacityWhite/Black()` are the helpers for it
- ~~Overextension penalty: an advanced piece with no defender and no retreat option, distinct
  from the plain "forward" reward, which doesn't discriminate a supported advance from a bare
  one~~ Shipped (Overext, `x`)
- Per-term incremental routing: let each Advanced term declare whether it is maintained in
  `g_evalPos` or recomputed at the leaf based on which weights are enabled, so sparse mixes
  (e.g. chip+mobility) stop paying delta overhead (from the ladder pricing above) `[Next]`
- "Cluster": a Wall/Column variant restricted to the middle rows only (excluding the 2 rows
  nearest each side's home row and the 2 nearest the goal row, avoiding overlap with the
  existing Hole/Control/RaceWin terms that already own that territory). Motivated by theory
  31 (`Docs/theories.md`): quiescence may induce a "posturing" style (deferring an even trade
  until it lands exactly at the search horizon, since only pending-capture leaves get a
  deeper look), and a middle-only clustering term might reward the same pattern statically.
  Test by hill-climbing the Advanced weight mix twice, once with `qs` off and once on, and
  checking whether Cluster's (and Race/RaceWin's) climbed weight shifts between the two runs
  `[Next]`

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
- ~~Quiescence search (extend on captures / near-wins)~~ Shipped 2026-07-17 as the
  opt-in `qs` head flag (`quiesceMax/Min`, captures-only stand-pat extension at
  depth leaves; near-wins covered by the existing per-qnode `canWin*` sentinels).
  Strength verdict at the d6/nb200k head: no dethrone -- s98+qs ties plain s98
  pooled (1073 vs 1074) and loses the direct pair 9-23; classic+qs +19 within
  noise. Theory 29; `plans/dethrone-champion-results-2-wiggly-mitten.md`. The
  runner-threat extension variant remains open `[done]`
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
- ~~Weight symmetrization + seed-ensembling for linear models: after training, average each weight
  with its left-right mirror (exact symmetry projection, free variance cut), and average the
  weights of K seed-replicas (for a linear model the ensemble IS the average). Directly attacks
  the measured 50-150 Elo training-seed noise~~ Shipped 2026-07-17 as `train.exe ensemble`
  (`--models` list, `--mirror 0|1`). Verdict: REFUTED for playing strength (theory 30,
  `plans/dethrone-champion-results-4-wiggly-mitten.md`). Mirroring the champion's own weights
  cost 135 Elo (1079 -> 944), the 6-seed mirror ensemble 144 vs the seed mean -- likely because
  a symmetric model's extra evaluation ties get broken by the directional first-found rule
  (theory 23) and the learned asymmetry is doing useful anti-pool tie-breaking (theory 19), not
  just noise. Open follow-ups: a pure-average (mirror=0) control to isolate averaging from
  mirroring, the calibration-vs-strength check (theory 27 parallel), and seed SELECTION by Elo
  instead of averaging (the best of 6 seeds is +28 over the champion) `[done]`
  - Follow-up (theory 32, `Docs/theories.md`): is the harm from asymmetry PER SE being removed,
    or from removing THIS SPECIFIC learned asymmetry (fitted to how this pool's shared
    left-file tie-break bias, theory 23, actually plays)? Extend `train.exe ensemble`'s
    `--mirror` flag with 3 more modes beyond off/average -- flip (full reflection, no
    averaging), left-onto-both (copy each mirror pair's left-column value onto both squares),
    right-onto-both (same, mirrored) -- and rate all 5 variants (unflipped/flipped/averaged/
    left/right) against each other. If flipped ~ unflipped >> averaged, asymmetry itself is
    what matters, not its direction; if unflipped >> flipped, the specific learned direction is
    fitted to something real about the pool `[Next]`
- Extraction quality controls in rank.exe extract: --min-elo floor or Elo-confidence weighting
  (label quality), --exclude held-out agents (measure pool-style overfitting by comparing Elo vs
  held-in against held-out opponents; low risk for linear models, must exist before MLP/NNUE),
  and positionKey-based dedup / repeat capping (openings are massively overrepresented) `[Next]`
  (partly superseded: the position-oracle pipeline sidesteps found-data label quality entirely
  by playing designed fresh games per position; posgen ships the positionKey dedup for pools.
  extract's own controls still matter for the outcome-label training path)
  - Test the low-Elo-data-quality hypothesis (theory 26, `Docs/theories.md`) using the --min-elo /
    Elo-filter controls above: retrain the existing value-model recipes on replay data (a) EXCLUDING
    low-Elo agents' games, (b) EXCLUDING mixed high-vs-low games, (c) EXCLUDING high-Elo games (the
    control), and compare the trained models' Elo. Also try an Elo-weighted reward (stronger label
    signal from higher-Elo games). Use seed replicas so the deltas clear the training-seed noise
    band (theory 8) `[Next]`
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
- ~~Validation split + early stopping instead of the fixed 6-epoch folklore cap~~ Shipped:
  `selfplay-supervised --val-split <f>` holds out that fraction (deterministic by seed), prints
  per-epoch `val=` loss, and computes the final stratified loss on the held-out set (so the
  theory-24 equal-material measure becomes a generalization number). `--early-stop` keeps the
  lowest-validation-loss epoch as the saved model. See
  `plans/residual-mlp-results-2-tingly-chipmunk.md` `[done]`
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
- ~~Elo-tie labeling: label a position by the interpolated Elo E* at which expected score = 0.5~~
  Shipped as the position-oracle label pipeline (rank.exe posgen/label/labelfit + train.exe
  dist-value, `plans/position-oracle-plan-1-lazy-popping-simon.md`): each position's measured
  mu IS the Elo gap at which expected score = 0.5 (sign flipped), and the pipeline adds the
  volatility SD on top. Theories 34 and 35 track the oracle-prediction and sigma claims
- Position-oracle follow-ups (after the first campaign's results doc): activate --elo-se
  (rating-SE variance is plumbed, off by default); an adaptive second labeling pass (per
  position, add pairings whose gap centers on -mu_hat from the first labels, where sigma is
  best identified); relabel-free retrain after each future ratings refit (rerun labelfit +
  dist-value on the same raw stores, documented in ML.md); sigma as a search-time signal
  (e.g. prefer high-sigma lines when behind); rate the mlp dist variants at the d6/nb200k
  head once a faster mlp leaf exists (currently d4-only, full-scan cost) `[Next]`
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
  See `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md` `[done]`
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
