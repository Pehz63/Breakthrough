# Todo

Priority tags: `[Now]` = active focus, `[Next]` = queued up, `[Later]` = valuable but not soon, `[Dream]` = fun idea, not currently planning to do it. The two tracks below are prioritized independently, a `[Now]` in one track says nothing about the other.

- Keep optimizing {cpu: days, dev: high}

---

# 1v1 / GUI Track

- Display whose turn it is in the main board area `[Next]` {cpu: seconds, dev: high}
- Add a button or something that changes the white and black pieces to red and blue in the GUI `[Next]` {cpu: seconds, dev: high}
- Best moves list or recommendation arrow `[Later]` {cpu: seconds, dev: high}
- GUI: play against a named saved agent `[Next]` {cpu: seconds, dev: high}
- GUI: agent-vs-agent ladder / leaderboard view `[Later]` {cpu: seconds, dev: high}
## Pondering / 1v1 Performance
- Run the side-bar evaluator's minimax at iterative deepening with no node or depth limit
  while waiting for the human's turn. Only use this for the visual evaluation readout, not
  the move actually played `[Now]` {cpu: seconds, dev: medium}
- Pipeline eval: instantly show a precomputed eval, then precompute the response and
  resulting eval for every possible next move in the background while waiting for the
  opponent's turn (can be parallelized) `[Next]` {cpu: seconds, dev: medium}
- Parallelize more computations `[Later]` {cpu: seconds, dev: high}
- New "rush" mode: Player B is an agent that runs iterative deepening against all of Player
  A's possible moves while waiting for A to move. On B's turn it plays the best move it
  already found for A's actual move, so it responds instantly with hardly any computation.
  The longer A takes, the deeper B has precomputed `[Next]` {cpu: seconds, dev: medium}

---

# Agent Track (rank.exe / ML / Search)

A composable system of interchangeable parts. An **Agent** = a **Move Chooser** (its "brain"),
which is either a **Search** (a **Move-Tree Explorer** + a **Board-State Evaluator**) or a
**Policy** (a direct, no-lookahead move picker: a heuristic or a learned move-rater). Each axis
is a registry, so adding one is a single table entry + a function body, and everything
(UIs, tournaments, docs) picks it up automatically.

**Goal:** a learned evaluator that beats the classic chip counter at EQUAL search
depth, then at lower compute (deeper-for-cheaper). Since 2026-07-28 the throne is
split into 5 parallel category champions by opener loadout (openless / 4-book /
8-book / 4-random / 8-random), declared in `ranking/CHAMPION.md` (single source of
truth; the numbers below are tagged to their fit dates and predate the split, so
they describe the single-champion era's history, not a current category leader).
**The first tier was achieved
and certified 2026-07-17** (`plans/dethrone-champion-results-1-wiggly-mitten.md`):
after boosting the top pairs to 32 games each and refitting, a learned PST
(`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1`, trained on oracle-vs-champion
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
`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=2)@1` at 1145 +/- 13,
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

- Add variety in openers and moves `[Now]` {cpu: hours, dev: medium}
  - Either an opening book, arbitrary rewards for certain opening positions, or a separate opener model for training
  - Random move chooser out of top candidates (especially ties); the board-state-evaluator noise variant of this idea moved to the Heuristic Evaluator Feature Ideas section below
  - Elo-rate the existing SCRIPTED openers (Offensive/Defensive) as ID modules: an optional `op(o|d)@1` ID segment (absent = Standard), roster the champion build under each opener, and let the BT fit price the openers directly. Generalize beyond the 3 built-in openers: rate arbitrary candidate opening move sequences the same way (Elo given/taken vs the pool), and learn to prefer strong ones and avoid weak ones. The general version is a bigger project (developer's own estimate: "too much work for now") `[Next]` (built-in openers) / `[Later]` (arbitrary sequences) {cpu: minutes-hours, dev: medium}. ~~A sibling idea for RANDOM (not scripted) openers shipped~~: a pluggable opener registry `g_openers[]` (`src/ai_random.h`) + `AgentSpec::openerKind`/`openerArg` + a `.opener(<kind>[,<arg>])@1` ID segment lets ANY agent be rostered/gauntletted both with and without an opener, so the Elo gap is a general, per-agent opener-sensitivity score (currently one kind, `rand`: e.g. champion 1140 clean vs 923 with `.opener(rand,moves=6)@1`, champdil 1153 vs 962). The registry is exactly the extension point for the SCRIPTED (`off`/`def`) and opening-book openers above -- adding one is a table row + fn, and the same ID slot names it. See `Docs/agents.md` and `Docs/terminology.md`'s "Opener (identity-level)" entry `[done]`
  - Mine `matches.jsonl` for an opening book: tabulate the first 8-10 plies of >= 900 Elo games by `positionKey` with win rates and visit counts, emit a book file, and add a book-follower opener that plays the book move while in book `[Next]` {cpu: minutes, dev: low}
  - ~~Color-swap recovery test: play the same random-opener snapshot to conclusion twice with colors swapped, to separate "the position favors a color" from "this agent recovers better."~~ Shipped as `rank.exe opener-swap`. Champdil vs the champion at n=20 (theory 15, `Docs/theories.md`): 65% of outcomes were a color effect (White won both 55%, Black won both 10% -- consistent with the champion's own White/Black split), but in the remaining 35% agent-effect bucket champdil won every time (7/7), the champion never (0/7) -- promising but small-sample signal that champdil recovers from bad positions better than the champion, independent of color. Follow-up (larger sample, try with oracle too) filed in `plans/opener-bias-results-1-synchronous-stearns.md`'s Future Work `[Next]` {cpu: hours, dev: medium}
  - ~~Random-first-K-plies opening diversity for data generation~~ (shipped as pairgen `--open-plies`); extend the same knob to tournament and self-play generation `[Next]` {cpu: seconds, dev: low}
  - Sweep `--open-plies` length for the oracle-vs-champion training regime (0/2/4/6/8/12 tried against the single untested value of 6 used in the first vs-champion study), gauntlet-screen each, to check whether 6 was actually a good choice or just a guess `[Next]` {cpu: hours, dev: medium}
  - Learned opener: a policy head trained only on plies < 10 of high-Elo replay games, used as an opener module that hands off to the main brain once out of phase. Specific angle worth testing: train the opener on WINNING-line data (see the refutation-book idea below) and hand off to a cheaper depth-5 search after the opener phase, on the theory that a strong precomputed opening plus a shallower live search could beat the d6 champion for less total compute -- directly on-target for the session's "beat d6 without searching deeper" goal `[Later]` {cpu: hours, dev: high}
  - Single-line refutation book (extended, more concrete version of the offline-refutation idea below): find ONE oracle-verified winning line against the champion for each color (2 lines total), play that fixed line against every other deterministic agent in the roster, and whenever an opponent deviates from the line (or the line stops applying), use the oracle to find a new winning continuation from that deviation point. Builds a small branching decision tree rooted at "beat the champion," tested for robustness against the whole pool, not just the champion. Consider separate White/Black book models plus a combined one `[Later]` {cpu: hours, dev: medium}
  - ~~Offline refutation book against the champion: run deep budgeted searches (d8-d10, nb2m) on the champion's preferred opening lines (it is deterministic, so its lines are minable from games.tsv), store best replies keyed by `positionKey`. A book + d6 search agent then attempts the dethrone with LESS live computation by construction.~~ Shipped 2026-07-17 as `rank.exe bookgen` (mines A's winning positions/moves from stored games) + the `book` opener (`.opener(book,<N>)@1` plays `models/book<N>.txt`). First verdict (mining the STRONG agent's wins over the weak target): REFUTED in this naive form (theory 14, `plans/dethrone-champion-results-3-wiggly-mitten.md`) -- both book agents rated ~16 Elo below their bookless selves. **Reversed 2026-07-18** (theory 33, `plans/dethrone-champion-results-5-wiggly-mitten.md`): mining the WEAK/book-wearing agent's OWN wins instead (zero new code, just swapped which agent's wins get kept) fixes the brain-portability failure by construction and DETHRONED s98 outright (classic+selfbook 1145 +/- 13 vs s98 1074 +/- 12, 25-7 head-to-head, 27-5 vs the oracle it was never mined against). Open scrutiny flag: genuine strength vs pool-specific effect, unresolved (`ranking/CHAMPION.md`). Remaining open repairs: a `--reset-state` mode for reproducible det-vs-det play (still needed, drift rate was 12/32 for this pairing); testing whether "opponent must be stronger" is load-bearing or any own-win suffices; applying the same self-mining fix to s98's own book; and the stay-in-book-to-the-win / response-tree variant via `--branch-tries`-style mining, which folds into the single-line refutation book idea above `[Next]` {cpu: hours, dev: medium}
- Interpret board analysis
  - Which piece is most impactful to the current evaluation? `[Later]` {cpu: minutes, dev: high}
  - What's the cheapest strategy to beat each given bot/parameters, even if overfitted? `[Dream]` {cpu: minutes, dev: high}
  - What strategies could a human devise to beat a bot? `[Dream]` {cpu: minutes, dev: high}
  - Is attacking the center or attacking the edge the best? `[Dream]` {cpu: minutes, dev: high}
  - Is advancing through the center or the edge the best? `[Dream]` {cpu: minutes, dev: high}
  - Is keeping the hind pieces in place the best? `[Dream]` {cpu: minutes, dev: high}
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
    needed elsewhere. Filed as theory 17 in `Docs/theories.md`.) `[Later]` {cpu: minutes, dev: high}

## Models (value head: board -> scalar)
- Convolutional NN value model (board as an 8x8xC grid; local spatial filters for walls/columns/forwardness) `[Later]` {cpu: days, dev: high}
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`).
  Concrete next step now that `MLPModel` (full-scan) ships and beats the linear PST ceiling on
  offline equal-material calibration (theory 24): make its FIRST layer an incremental accumulator
  -- widen the scalar `g_mlAcc` that the linear inner already maintains into a vector, and
  recompute only the hidden units touched by the 2-3 changed inputs per make/unmake -- so the MLP
  can compete at FIXED compute. Motivated directly by the residual-mlp results' full-scan Elo
  caveat (its per-node eval is strong but per-second it is handicapped). `[Next]` {cpu: hours, dev: medium}
- Residual skip design space (follow-up to the shipped HARD frozen chip skip; theory 24,
  `plans/residual-mlp-results-1-tingly-chipmunk.md`). The linear residual HELPED + stabilized
  equal-material calibration but the MLP residual was a WASH, so probe whether a softer/richer
  skip or more capacity changes that split: (a) SOFT / regularized skip -- a learnable material
  weight initialized at the material-only logistic fit and penalized for drifting, vs the hard
  frozen one (theory 24 Q1); (b) a BROADER hand-crafted baseline as the skip (e.g. the Advanced
  linear mix) instead of the literal chip differential (theory 24 Q2); (c) a wider / DEEPER MLP
  capacity sweep (2 hidden layers, more widths) to map where added capacity stops improving
  calibration. All measurable with the existing stratified-loss printout + the generalized
  `sweep_pst_v2.ps1` groups. `[Next]` {cpu: hours, dev: medium}
- Transformer value model (squares as tokens) -- teacher / label generator only, not in-search `[Dream]` {cpu: days, dev: high}
- ~~Incrementalize an ML model (e.g. MLP/NNUE) so a move recomputes only the few inputs it changed
  instead of the whole forward pass.~~ Shipped 2026-07-22 as the NNUE-style first-hidden
  accumulator for an MLP mu head (`g_mlAccDim`/`g_mlAccVec`/`g_mlL0ByInput`; the vector
  generalization of `g_mlAcc`). The first layer is maintained across make/unmake; only the
  remaining layers run per leaf. Measured **1.78x** per-node speedup for the wide head
  (256/128) at fixed depth (36.1 -> 20.2 us/node at d4), bounded by the first-layer share as
  predicted; the layers past the first ReLU are irreducible for a fixed architecture. The d6
  MLP rating was NOT actually blocked (it was already done last session, 720+ games/agent:
  dist_lin 1031 > MLPs 974/967/931, all below the champion -- theory 27 holds); the
  incremental agent is the same identity + eval-equivalent, so those numbers carry over.
  See `plans/nnue-incremental-mlp-results-1-crystalline-taco.md`. `[done]`
  - **Second-accumulated-layer (dead-ReLU delta).** Superseded by the sparse leaf-tail
    forward above for these heads: an op-count showed the delta approach is both slower here
    (it pays an update AND an undo every make, and the side-to-move bit flips every ply,
    perturbing many units, so the "changed" set isn't much smaller than the live set) and
    more complex (H2-dim reversible accumulator state) than just recomputing the
    already-sparse tail per leaf. Would only be worth it if the second layer were much wider
    than the live-unit count, or for a head where per-move churn (not just static sparsity)
    is the bottleneck. `[Dream]` {cpu: hours, dev: medium}
  - **Sparsity-training penalty** (`dist-value` L1 / non-clamped-fraction loss): now lower
    priority still -- the sparse leaf-tail forward already captures the ~90%-sparse heads'
    speed win without changing training at all. Would only matter if a future architecture
    trains denser. `[Later]` {cpu: hours, dev: medium}
  - **Set `/arch:AVX2` as the native-build baseline?** Would realize the general ~1.2x leaf
    speedup in production (helps the standard head we would actually use). Requires an AVX2 CPU
    (~2013+) for the shipped native binaries; not the web/WASM build. A one-line flag add per
    native build script + a portability note. Developer decision. `[Next]` {cpu: seconds, dev: medium}
  - **int16 quantization of the accumulator.** The full NNUE throughput recipe (32 int16 per
    AVX-512 register vs 16 floats), but it multiplies SIMD lanes by a constant too, so it will
    not change the shape ranking either -- only worth it for the general leaf speedup, and it
    breaks bit-identicality and needs a retrain or post-training calibration. `[Later]` {cpu: hours, dev: medium}
  - **Full multi-seed 32-game d6 campaign for the dist MLPs**, now that d6 is affordable
    (~0.57 s/move for the wide head): re-confirm the existing 720-game standings (dist_lin
    1031 > MLPs 974/967/931) hold at tighter error bars, now that cost is no longer a
    constraint on games/pair. `[Next]` {cpu: hours, dev: low}
- Joint value + policy + next-value model trained to minimize its own recomputation `[Later]` {cpu: days, dev: high}
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
- Position-oracle MLP variant (developer, 2026-07-21): a lighter-weight version of the sparsity
  idea directly above, scoped to the dist model specifically rather than a new joint
  value+policy architecture -- add a sparsity penalty (L1 on weights, or a penalty on the
  fraction of ReLU units NOT clamped to 0) to `dist-value`'s loss, so the trained mu/sigma
  heads naturally have more zero weights and more dead ReLU units. Directly reduces the
  incrementalization work above once it exists (fewer nonzero paths to touch per move) and may
  also cut the current FULL-SCAN cost on its own (skip zero-weight multiplies even without an
  accumulator) `[Later]` {cpu: hours, dev: medium}
- Position-oracle MLP variant, more left-right symmetry (developer, 2026-07-21): either double
  each training batch with mirrored copies of each board, tie mirror-pair weights together, or
  add a symmetry-forcing loss term. Developer's own prediction going in: this will NOT do as
  well Elo-wise on this specific pool, and that prediction is well-grounded -- theories 23/30/32
  found the pool has a real, exploitable left-file tie-break bias, and mirror-symmetrizing a
  linear outcome-trained model already cost real Elo (theory 30) rather than being a free
  variance cut. Interesting BECAUSE it is expected to fail on playing strength: this is a
  chance to test whether the same left-bias-is-real-signal finding holds for a model trained to
  PREDICT position strength rather than to play well, or whether prediction quality and
  playing strength diverge again here too (see theory 27, freshly reconfirmed by this
  campaign). Concrete design already proposed and unbuilt for the general case: theory 32's
  5-way unflipped/flipped/averaged/left-onto-both/right-onto-both comparison, reusable for the
  dist model's mu head with `mlv2MirrorIndex` `[Later]` {cpu: hours, dev: medium}

## Models (policy head: board + move -> score / move-rater)
- MLP policy `[Later]` {cpu: hours, dev: medium}
- Transformer policy `[Dream]` {cpu: days, dev: high}
- Softmax / temperature sampling over move scores (for exploration + diverse self-play) `[Now]` {cpu: minutes, dev: low}
- Repurpose the position-oracle's labeled data for a policy signal (developer question,
  2026-07-21: "is that just the same thing?"). Answer: not quite, but three real, distinct
  paths exist, in order of how much new work each needs: (1) NOTHING new to build -- the dist
  model's mu head already scores positions, and running it through the existing Greedy explorer
  (1-ply argmax over an evaluator) or AlphaBeta already turns "which move leads to the
  highest-mu resulting position" into a policy for free, this is just using the model the way
  every other value model already gets used, not a new signal; (2) a genuinely NEW signal:
  instrument the labeler to record which move each ladder agent actually played from a labeled
  position (currently `rankLabel` plays to conclusion and keeps only the outcome, discarding
  the move), then train a move-rater on (position, move actually played by a HIGH-Elo ladder
  rung, weighted by the position's own measured mu/sigma) -- this is a real new dataset the
  raw store cannot currently produce without a labeler change, distinct from a value model's
  1-ply lookahead; (3) policy distillation -- train a fast direct move-scorer to mimic what
  "argmax over the dist model's mu across every legal move" would pick, without paying for the
  per-move mu evaluation at inference time. (2) is the interesting new-signal answer to the
  question as posed; (1) and (3) are real but are repackaging the existing model, not a new
  learning signal `[Later]` {cpu: hours, dev: medium}

## Models (difficulty head: board -> how hard is this position)
- Blunder labeling by branch replay: play a game between two strong deterministic
  agents, rewind to a move by the eventual winner, substitute a random different move,
  and replay from there. If the winner changes, label the substitute a blunder. Also a
  per-turn difficulty probe: turn difficulty = how few of the legal moves preserve the
  win. `[Later]` {cpu: hours, dev: medium}
- Position difficulty as a learned target: position difficulty = an aggregate of this
  turn's and the following turns' difficulty. A strong move lowers your future
  difficulty; a risky move raises it while still winning. Computing it exactly needs
  near-exhaustive tree exploration (intractable except trivial endgames), which makes
  it a natural ML target: generate labels by branch replay / sampling where it IS
  computable, train a model to predict it anywhere. `[Later]` {cpu: hours, dev: high}
  - Uses: difficulty-aware move choice (prefer low-difficulty winning lines against a
    tricky opponent), rating the difficulty of puzzles/positions for humans,
    difficulty-aware time allocation in search.

## Adversarial / Opponent-Modeling Agents
- Counter-agent trained against one specific opponent: mine that opponent's alpha-beta
  search for moves it pruned early (cut off before full evaluation) and check whether
  continuing into that line actually wins. Train a policy that preferentially steers
  into an opponent's under-explored lines. Deliberately overfits to one opponent's
  build/pruning behavior, so it is fragile against everyone else by design. `[Later]` {cpu: hours, dev: high}
  - Purpose is not a standalone strong agent: keep counter-agents in the rank.exe pool
    on purpose. They force every other agent, especially the reigning #1, to be robust
    not only to raw strength but to opponent-specific exploitation, the same way the
    dilution ladder forces robustness to weaker/noisier play.

## Board-State Evaluators (BSEFs)
- Ensemble / blended evaluator (average or weighted mix of several evaluators/models) `[Later]` {cpu: hours, dev: low}
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
  varying one. See theory 25, `Docs/theories.md` `[Now]` {cpu: days, dev: high}

## Heuristic Evaluator Feature Ideas (Classic / Experimental)
New candidate terms for the hand-crafted evaluators, alongside the existing chip/wall/column/
forward mix. ~~Each is a single-place edit in `g_evaluators` (`src/ai_eval.cpp`) plus wiring the
term into the incremental `evalPosLocal` delta the same way wall/column/forward already are.~~
Batch 1 shipped 2026-07-11 as the **Advanced** evaluator (`adv`, 16 params, all terms below
plus the D14 RaceWin detector; see `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`).
- ~~Reward defended pieces: a diagonal-adjacency analog of the existing wall (orthogonal) and
  column (same-file) structure terms, since a diagonal neighbor is what actually recaptures
  after a capture in this game~~ Shipped (Support, `d`), merged with the phalanx idea below --
  both describe the same diagonal-pair geometry; a saturating per-piece variant stays open
- ~~Race-distance differential: (opponent's closest piece to your back rank) minus (your closest
  piece to theirs), a cheap proxy for who wins a pure race, ignoring tactics.~~ Shipped (Race,
  `r`, from incrementally maintained per-row piece counts), plus a stronger sibling: RaceWin
  (`g`), the exact D9/D14 decided-race sentinel detector (proven sound, ~8% us/move, no
  measurable Elo at d6 -- theory 21). The capacity generalization is settled analytically:
  capacity advantage == forwardSum - 7*chipDiff exactly (code-verified identity), so it is
  linearly dependent on existing terms and was NOT added as a weight; theory 18's
  outcome-correlation half stays open, `capacityWhite/Black()` are the helpers for it
- Per-term incremental routing: let each Advanced term declare whether it is maintained in
  `g_evalPos` or recomputed at the leaf based on which weights are enabled, so sparse mixes
  (e.g. chip+mobility) stop paying delta overhead (from the ladder pricing above) `[Next]` {cpu: seconds, dev: low}
- "Cluster": a Wall/Column variant restricted to the middle rows only (excluding the 2 rows
  nearest each side's home row and the 2 nearest the goal row, avoiding overlap with the
  existing Hole/Control/RaceWin terms that already own that territory). Motivated by theory
  31 (`Docs/theories.md`): quiescence may induce a "posturing" style (deferring an even trade
  until it lands exactly at the search horizon, since only pending-capture leaves get a
  deeper look), and a middle-only clustering term might reward the same pattern statically.
  Test by hill-climbing the Advanced weight mix twice, once with `qs` off and once on, and
  checking whether Cluster's (and Race/RaceWin's) climbed weight shifts between the two runs
  `[Next]` {cpu: hours, dev: medium}

## Move Choosers / Policies (direct, no search)
- Greedy-by-eval (1-ply pick of the move maximizing a BSEF) **(P1, via the Greedy explorer)** {cpu: seconds, dev: low}
- Softmax/temperature sampling policy (probabilistic move choice) `[Now]` {cpu: minutes, dev: low}

## Move-Tree Explorers (search)
- MCTS / PUCT (pairs a policy head with a value head). Shipped 2026-08-16 as
  Gumbel MCTS (Danihelka et al. 2022; `Docs/works-cited.md`): the `GumbelMCTS`
  explorer (`src/ai_gumbel.cpp`, Gumbel-top-k root sampling + Sequential
  Halving) and the `joint` value+policy model type (`src/ml_model.h`),
  `gaz(sims=N)@1` in the roster ID grammar. Validated by unit tests and
  playable via `rank.exe`/`train.exe` with a hand-built or randomly-initialized
  model; no self-play training regime exists yet, so no Elo has been measured
  and it must not be described as strong (`Docs/model-training-playbook.md`'s
  certification gate). See `ML.md`'s "Gumbel MCTS" section for what exists vs.
  what is deferred `[Now]` {cpu: seconds, dev: high}
  - **Self-play training regime** (the deferred slice): generate games with
    `GumbelMCTS`, train the value head against a bootstrapped search-value
    target (the search's own improved value estimate, not just game outcome
    -- the developer's stated preference for this slice) and the policy head
    against the Gumbel-improved policy target
    (`softmax(logits + sigma(completedQ))` over the root's final visit
    counts/completedQ, already exposed via `GumbelRootInfo`). Needs its own
    plan per `Docs/model-training-playbook.md` (three-pass process,
    initialization/update-schedule questions, a presented configuration grid)
    `[Next]` {cpu: hours, dev: high}
- TT speedup is currently node-count-real but wall-clock-muddied by `positionKey`'s per-node string build; an incremental Zobrist hash would make the TT a wall-clock win too `[Next]` {cpu: seconds, dev: low}

## Training Regimes
- Eval-blended labels: label each position with lambda*outcome + (1-lambda)*sigmoid(teacherEval/scale)
  instead of outcome alone. The teacher already computes a root search score every move and throws
  it away; blending turns one noisy bit per game into a real-valued signal per position (the NNUE
  training recipe) and should also improve move ordering, where the PST prunes 3x worse than
  Classic `[Now]` {cpu: hours, dev: medium}
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
    fitted to something real about the pool `[Next]` {cpu: hours, dev: medium}
- Extraction quality controls in rank.exe extract: --min-elo floor or Elo-confidence weighting
  (label quality), --exclude held-out agents (measure pool-style overfitting by comparing Elo vs
  held-in against held-out opponents; low risk for linear models, must exist before MLP/NNUE),
  and positionKey-based dedup / repeat capping (openings are massively overrepresented) `[Next]` {cpu: hours, dev: medium}
  (partly superseded: the position-oracle pipeline sidesteps found-data label quality entirely
  by playing designed fresh games per position; posgen ships the positionKey dedup for pools.
  extract's own controls still matter for the outcome-label training path)
  - Test the low-Elo-data-quality hypothesis (theory 26, `Docs/theories.md`) using the --min-elo /
    Elo-filter controls above: retrain the existing value-model recipes on replay data (a) EXCLUDING
    low-Elo agents' games, (b) EXCLUDING mixed high-vs-low games, (c) EXCLUDING high-Elo games (the
    control), and compare the trained models' Elo. Also try an Elo-weighted reward (stronger label
    signal from higher-Elo games). Use seed replicas so the deltas clear the training-seed noise
    band (theory 8) `[Next]` {cpu: hours, dev: medium}
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
  -AnalysisOnly` to re-test the out-of-distribution theory `[Now]` {cpu: minutes, dev: medium}
- Tapered / phase-split PST: separate opening/endgame weight tables interpolated by piece count
  (piece count changes only on capture, so it stays fully incremental). The natural capacity step
  before MLP `[Next]` {cpu: hours, dev: medium}
- PV/leaf position harvesting: train on positions from inside the teacher's search tree labeled
  by subtree value, matching the off-path distribution the eval actually sees in search `[Later]` {cpu: hours, dev: medium}
- Active / hard-example mining: oversample positions where the current model most
  disagrees with the teacher label or with a deeper search, instead of uniform
  sampling `[Later]` {cpu: hours, dev: medium}
- ~~TD-Leaf(lambda) self-play bootstrap (value)~~ Shipped 2026-07-29 as `src/ml_tdleaf.cpp`
  + `train.exe tdleaf`. The project's first ONLINE, bootstrapped value regime: the target for a
  position is the model's own evaluation of a later position backed up through the search, applied
  at the principal-variation leaf, with weights moving during play. lambda=1 provably reduces to
  outcome-supervised training on PV leaves (unit-tested closed form). PV leaves come from TT probes
  along the played line, so `ai_minimax.cpp` is untouched and no rated agent's us/node changes.
  Strength MEASURED at screening level 2026-07-29 (38-agent cohort, `tools/tdleaf_study.ps1`,
  pinned fit at 8 games/pair, `plans/tdleaf-results-1-amber-pangolin.md`): peaks at **1051 vs its
  791 initialisation, +260 Elo**, at the same 130-param linear architecture and head. The
  lambda=1 control (provably == outcome-supervised on PV leaves) scored 713, BELOW the init, so
  the gain is specifically the BOOTSTRAP. Game count has an interior optimum ~1000 and declines
  past it (-36, all 4 seeds same sign). NOT certified: a pinned fit cannot dethrone and 8
  games/pair is half the standard `[Now]` {cpu: seconds, dev: low}
- ~~**Certify the TD-Leaf peak.** Append the top rungs to `ranking/roster.txt`, fill contenders to
  32 games/pair, run an unpinned refit (`Docs/ranking-workflow.md` Workflow B). Best screening
  agent is `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=131,18bfb7a0,tdleaf_self,lin,shape=129-1)@1` (A-base seed
  1001 at 1000 games) at 1056, vs the pinned openless champion's 1012~~ Done 2026-08-01, though
  via a different route than planned: the certification refit happened as a side effect of
  dropping the Pass-2 screening games from the fit. A TD-Leaf agent,
  `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`, **took the
  openless title** at 1044 +/- 11 over s76's 1007 +/- 8, boosted to 0 pending at 32 games/pair.
  See `ranking/CHAMPION.md`, which records two caveats: 32 rows/pair is ~22.6 DISTINCT games/pair
  (0.706 distinct/row measured), so the gap is 2.3 combined SE rather than 2.7; and s169 gained
  the title while LOSING 36% of its games, which is not understood `[Now]` {cpu: seconds, dev: low}
- **Still unexplained: why s169 GAINED the openless title while losing 36% of its games.**
  The chip counter's fall is now accounted for, its rise is not `[Now]` {cpu: hours, dev: high}
- **Decide whether `rate --regime-balanced` should become the canonical fit** `[Now]` {cpu: seconds, dev: high}.
  Implemented 2026-08-01 and writing `ranking/*_balanced.*`; not promoted, because promoting
  it re-certifies every champion. It moves the table a lot: at head `ab(deep=6,tt,ord,nodes=200k)@1`
  the openless order becomes s602 / s169 / s76 / s349 / classic@2 (1259 / 1249 / 1240 / 1239 /
  1213, all +/-8 except classic's +/-14), i.e. a 4-way statistical tie at the top and
  **`classic(chip=100)@2` back to rank 5 from rank 35**. Open questions before promoting:
  whether the anchor (`rand@1`, regime `nonlearning`, 5 agents) should be exempt from
  balancing; whether SEs remain interpretable after reweighting (they are effective-sample
  SEs now); and whether category champions should be declared on the balanced or pooled fit
- **Old (superseded) framing of the chip-counter question, kept so the reasoning is not
  re-derived:** `s169` rose to the openless title on 36% fewer games, while
  `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2` fell from openless rank 2 to rank 35 under the
  same change. Bradley-Terry accounts for opponent strength, so "it farmed weak candidates" is
  not an explanation and should not be repeated as one. Candidate tests: refit with only the
  cohort games REMOVED from classic@2's record but kept for everyone else; check whether the
  cohort was a non-transitive matchup for classic@2 specifically (head-to-head vs the 9 dropped
  candidates); check whether the effect is connectivity (pairs lost) or volume (games lost) by
  topping the surviving pairs back up to the old game counts `[Now]` {cpu: hours, dev: medium}
- Bracket the TD-Leaf game-count peak: the ladder jumps 500/1000/2000 so the optimum is located
  only within 2x, and the post-peak decline is unexplained. Add 700/1400 rungs + a decayed-lr arm `[Next]` {cpu: hours, dev: low}
- Extend the TD-Leaf from-scratch arm past 2000 games: it was still climbing (+134 from 500->2000)
  while champ-init had already peaked, and the gap had narrowed from -222 to -103 `[Next]` {cpu: hours, dev: low}
- Re-run the TD-Leaf lr and generator-depth arms at 4+ seeds: both are n=1, so the lr ordering and
  the d4 == d6 equivalence (theory 44) are suggestive only `[Next]` {cpu: hours, dev: low}
- Run the TD-Leaf batched-update path (`--batch`), implemented but never exercised; the
  online-vs-batched question is still open `[Next]` {cpu: hours, dev: low}
- Run a TD-Leaf MLP arm: the whole cohort was the 130-param linear model `[Next]` {cpu: hours, dev: medium}
- Backfill `Docs/hyperparameter-log.md`: only the `tdleaf` section is populated so far.
  Transcribe real values (not re-derived from memory) from `plans/training-sweep-results-1-luminous-snail.md`
  (the 78-candidate sweep's full axis list, `--gen-random`/`--gen-random-floor`/`--gen-random-decay-plies`,
  `--residual-skip`, `--val-split`), the position-oracle pipeline (posgen/label/labelfit ladder design,
  `--elo-se`, calibration sample size), and `tools/hill_climb.ps1`'s step-size/reset/flip-probability
  history in `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md` `[Next]` {cpu: seconds, dev: low}
- Population / other-play tournaments as a data source, including an evolutionary variant:
  each round, mutate the top-couple-Elo agents (unique random perturbations of their weights)
  into new agents, add them to the round-robin, drop the weakest, and iterate -- so the
  population crawls the weight surface by selection `[Now]` {cpu: days, dev: high}
- ~~Explore new agent types built from the mu/sigma distribution instead of just mu-as-eval:
  (a) SAMPLE a value from N(mu, sigma) at each leaf instead of using mu directly -- a
  genuinely new stochasticity source (the model's own learned uncertainty) distinct from the
  dilution/jitter mechanisms already in the ladder; (b) mean +/- k*sigma variants (an
  "optimistic" agent that scores mu+sigma, a "cautious" one at mu-sigma) -- ties directly to
  the sigma-as-search-time-signal idea two lines up (prefer high-sigma/high-upside lines when
  behind is exactly a mu+sigma-style leaf score). Needs a new accessor or explorer variant
  since `mlValueScore`/`mlLeafScore` currently read mu only (`mlValueScoreDist` already
  exposes both mu and sigma, so the plumbing exists, just not wired into move choice)~~ (b)
  shipped 2026-08-06 as LearnedValue's Risk weight (`mlValueScoreRisk`, ID `risk=<tenths>`,
  see `plans/risk-weight-plan-1-dusty-kestrel.md` / `risk-weight-results-1-dusty-kestrel.md`,
  theory 47). Screened on the s76 core only (developer scoping decision) at k in
  {-1.0,-0.5,-0.2,-0.1,0.1,0.2,0.5,1.0}: REFUTED cleanly, every nonzero k rated 66-228 Elo below
  the mu-only baseline (1007), monotonically worse with |k|, negative k less damaging than
  positive at matched magnitude. (a) sampling from N(mu,sigma) remains open, and so does
  screening (b) on the other 9 rostered position_elo cores and at k below 0.1 (see the results
  doc's Future Work) `[Next]` {cpu: hours, dev: medium}
- Activate --elo-se (rating-SE variance is plumbed, off by default); an adaptive second
  labeling pass (per position, add pairings whose gap centers on -mu_hat from the first
  labels, where sigma is best identified); relabel-free retrain after each future ratings
  refit (rerun labelfit + dist-value on the same raw stores, documented in ML.md) `[Next]` {cpu: hours, dev: medium}
- Position-oracle input-feature alternatives (developer question 2026-07-20, feature v2's
  129 raw piece-square bits are not the only reasonable encoding). Test candidates on the
  CHEAP linear model first via the same log-triplet position-count-sweep discipline used to
  find the ~425-1700 saturation point, since it isolates whether a feature helps independent
  of the architecture-capacity confound the MLP sweep just surfaced, before spending compute
  confirming a winner on the MLP:
  - Material/piece advantage as an explicit input (not a frozen skip). `matDiffFromFeatures`
    + `ResidualModel` already exist and were tested this way for the OUTCOME-trained linear/MLP
    value models -- refuted at 6 seeds (theory 24: the effect fell inside seed noise at every
    capacity). That result does NOT settle this case: theory 24 forced material in as a frozen
    additive skip bypassing the model, this idea is a plain feature the model can freely weight
    and combine with everything else, and it was tested where data was plentiful, not where a
    model is capacity-starved relative to its data (which the MLP position-count sweep just
    showed is true here for the mlp128-64/mlp32 config below ~6800 positions). Worth a fresh,
    honestly-scoped test, not assumed refuted by the old result `[Next]` {cpu: hours, dev: medium}
  - Ternary board encoding (-1/0/+1 per square, 64 features instead of 128) -- considered and
    NOT recommended, reasoning captured so it is not re-proposed blind: a single shared weight
    per square forces white-here and black-here to be exact negatives of each other, which is
    wrong for this game specifically (white advances row0->row7, black row7->row0, so e.g. row6
    is nearly-won for White and merely unadvanced for Black -- nowhere close to sign-flipped
    values on the same square). Would encode an incorrect symmetry and likely hurt, not help
  - Mover's-perspective (color-canonicalized) encoding: recolor every position to "my piece /
    empty / opponent piece" oriented to the mover's own forward direction, instead of literal
    White/Black. The rules genuinely have this symmetry (axioms.md color-swap symmetry) --
    DISTINCT from the LEFT-RIGHT mirror fold this pipeline deliberately does NOT apply
    (theories 30/32: that asymmetry is real pool-specific signal, not noise; do not conflate
    the two axes). Canonicalizing this way makes a White-played position and its exact
    color-swapped mirror the SAME input, roughly doubling usable data for free -- directly
    attacks the MLP's demonstrated data appetite. Untested anywhere in the project; the
    strongest candidate on this list `[Next]` {cpu: hours, dev: medium}
  - Forward-progress sum per side (Advanced evaluator's Forward term / the capacity axiom,
    `Docs/axioms.md` Lemma B) -- a race-tempo signal distinct from raw material
  - Per-row piece histogram -- the engine already maintains this incrementally (`g_rowCountW`/
    `g_rowCountB` in globals.h, used by the Race/RaceWin terms) so it costs nothing extra to
    also expose as a feature; gives race structure without inferring it from 128 raw bits
  - Support/structure count (Advanced's Support term, diagonal same-color adjacency, Lemma C)
    as a defensive/piece-safety proxy
  - Mobility (Advanced's Mobility term, legal-move count per side)
  - The decided-race sentinel (Advanced's Race/RaceWin, the exact D9/D14 detector) as an
    explicit input -- particularly relevant to the SIGMA head: a decided race should read as
    near-zero volatility, and handing that fact directly rather than making the model
    rediscover it could sharpen exactly the number this project cares about most
  - Ply / game-phase as an input -- already tracked in the position pool (`"ply"` field) but
    never fed to the model; volatility plausibly differs by phase (quiet openings, sharp
    midgames, near-decided endgames), so this may matter more for sigma than for mu
- Distillation from deep search or from a teacher model `[Later]` {cpu: hours, dev: medium}

## Weight optimization / geometry mapping
A single per-weight sweep is insufficient: each weight only matters RELATIVE to the others
(if chip=300, then forward 1 vs 2 vs 10 is indistinguishable), interactions are non-obvious
(forward may need to be HIGHER when structure is high, to offset the structure lost by
advancing; forward could even be NEGATIVE to keep pieces back and advance together), and the
optimum is a surface, not a point. Replace single sweeps with a search that maps the geometry:
- Report a response surface, not a single recommended value `[Next]` {cpu: hours, dev: medium}

## Strength Dilution (to spread an Elo ladder)
- Measure how often stochastic depth dilution (`dil(rP,dN)`) actually changes the move: for a
  sample of real positions, run both the full depth and the diluted shallower depth (e.g. depth
  6 vs depth 8, same node budget, tt+ord on, Classic eval) and compare chosen moves directly
  (diff the board before/after each search). Developer hypothesis to test alongside this: ODD
  dilution depths (5, 7) diverge from the full depth MORE than even depths (4, 6, 8), because an
  odd-depth search's leaf lands on the opponent's reply rather than after a complete move/response
  round trip, a horizon-style asymmetry -- compare odd vs even depth agreement rates to check.
  Motivated by a question about `dil(prob=15,deep=6)`/`dil(prob=30,deep=6)` (the position-oracle campaign's d8
  ladder rungs, `Docs/Memories/position-oracle-campaign.md`) but stands alone as a general
  dilution-quality question, deliberately deferred to its own session `[Later]` {cpu: minutes, dev: medium}

## Elo / Tournaments
- Recreate the `s9` linear v2 value model retired 2026-07-30 (`ranking/roster.txt`,
  `ranking/roster_top.txt`, `ranking/CHAMPION.md`): `models/sweep/slot9.txt` was
  accidentally overwritten by a test using an unverified slot number, and since
  `models/sweep/*.txt` is gitignored the original weights are gone. Its exact
  provenance (recipe/seed) lived in the file's own `teacher=` line and is lost with
  it, so this can only be a NEW agent trained to fill a similar role (linear v2,
  same head family), not a literal reproduction of the old weights/identity. Low
  priority: the roster is fine without it, and its historical Elo/match record
  already stands as-is regardless `[Later]` {cpu: hours, dev: low}
- **Same loss recurred for `models/sweep/slot6.txt` and `slot7.txt`, found
  2026-08-16.** Both are live, active (`on`) roster agents
  (`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=6,eac8ab99,...)@1` and
  `model=7,c7f7ce61`). `tests/test_ranking.cpp`'s `rankLoadAgentModels` test
  uses those exact slot numbers as scratch (overwrites slot 6, deletes slot 7)
  to exercise its load-success/load-failure cases, with no check that they
  were already live -- the identical unverified-slot-number defect class as
  the `s9` entry above. Neither file is git-tracked, so neither is
  recoverable. Developer decision 2026-08-16: accept the loss the same way as
  `s9` rather than fix the test now. Historical Elo/match record for both
  agents stands as-is; no NEW games can be played for either until the roster
  lines are repointed at retrained models or the weights are restored from
  outside this repo. Full detail: `plans/gumbel-mcts-results-1-hidden-greeting-mist.md`
  `[Later]` {cpu: hours, dev: low}
- ~~**Give the rating path real sample diversity.**~~ Done 2026-07-26, via a second
  pool rather than by changing the first. `ranking/roster_open.txt` holds 14 agents
  each wearing `.opener(rand,moves=4)@1`, played with `rank.exe ... --paired-openings` into
  `ranking/matches_open.jsonl`. Result: **median distinct-trajectory ratio 1.000, min
  1.000** across all 91 pairs (the fixed-start pool is 0.438 median, 0.062 min), and
  error bars scale as 1/sqrt(n) exactly as independent samples should (median pm 53 ->
  38 -> 28 -> 20 at 4 -> 8 -> 16 -> 32 games/pair). Defect 3 does not exist in this
  pool. Rank-order stability across fills: Spearman rho 0.974 (4->8), 0.969 (8->16),
  0.987 (16->32), with agents changing rank 6, 6, then 3 of 14. Converging but not
  converged at 32.
  Remaining sub-items: option 3 below (an effective-n column in `rank.exe rate`) is
  still worth doing for the FIXED-START pool, which keeps its defect. Option 2
  (`ttClear()` per game) is also still open and is what would make the fixed-start
  pool reproducible.
- **Can a book be mined to RECOVER from bad random openings? `[Next]`** {cpu: hours, dev: low}
  Developer question, 2026-07-26. In the diversified pool a book is inert, because it
  is keyed on exact position hashes that a random opening never reaches, so book
  agents were left out. But that assumes a book mined the way `bookgen` mines them:
  from the standard start, forward. The interesting variant is a book keyed on
  positions that arise AFTER a random opening, mined from games the owner won from a
  bad start. That is a different artifact and a different claim: not "replay my best
  line" but "here is the refutation once I am already worse". If it works it is a
  genuinely transferable skill rather than the memorization theory 38 found. Test:
  mine from `ranking/matches_open.jsonl` (which now has 2912 diversified games) with
  `--plies` covering the post-opening window, roster the result, and see whether it
  beats its own bare core in the diversified pool. Cheap, the games already exist.
- **Grow the diversified pool. `[Next]`** {cpu: hours, dev: medium}
  Currently 14 agents, chosen as one strongest representative per evaluator family
  plus the scale (see the header of `ranking/roster_open.txt` for the selection rule).
  Candidates to add: more seed replicas per recipe so the training-seed-noise band is
  visible inside this pool, the remaining `adv` hill-climb finds, and a second dist
  seed (only `learned(model=111,...)` is in). Also decide whether `.opener(rand,moves=4)` is the
  right depth: 4 own half-moves means 8 plies of random play, never swept.
- **Give the rating path real sample diversity (original entry, superseded above).** {cpu: hours, dev: low}
  Found 2026-07-26 (`plans/book-opener-audit-results-1-vivid-lantern.md`, defect 3 in
  `Docs/benchmarking.md`). `rankSchedule` seeds every game, but `rand()` is only consumed
  by dilution and random-move agents, so for a pair with no `dil(...)` and no `rand`
  opener the seed is inert and every game with the same colour assignment is
  byte-identical. `playOneGame` also never calls `ttClear()`, so for a `tt` head the sole
  source of variation is which games ran earlier in the same process. Measured: median
  distinct-trajectory ratio 0.438 across 190 pairs with >= 16 games, worst cases 32 games
  yielding 2 distinct games (all `ab(deep=6,ord,nodes=200k)`, the no-TT head). A null control of
  two identical deterministic agents went 32-0 as White and 1-31 as Black over 64 games.
  Consequence: `pm` in `ratings.tsv` / `standings.tsv` is understated by roughly 1.5x for
  a typical pair and much more for deterministic ones.
  Options, to be decided before the next certification refit:
  1. Add `--open-plies K` / `--open-side` to `rank.exe play` and `run` (the plumbing
     already exists in `playoutCapture`, only the CLI and `rankPlay` signature are
     missing). K random opening half-moves make `rand()` live, so each seed is a real
     game. This changes what the ladder measures, from "strength at the standard start"
     to "strength across openings", and it is the standard fix in engine testing. It also
     invalidates comparison against the existing store, so it needs a new board tag or a
     fresh store rather than mixing rows.
     **Option 1 alone is not sufficient.** Measured 2026-07-26: `--open-plies` makes the
     seed live but leaves cross-game TT state intact, and splitting the same 64
     diversified games into four 16-game processes moved a booked pair by 17pp. Options
     1 and 2 are one change, not alternatives.
  2. Add `ttClear()` per game in `playOneGame` for reproducibility, which is the
     `--reset-state` repair theory 14 and theory 19 both ask for. On its own it makes
     sample size WORSE (it removes the only current diversity source), so it must land
     together with option 1, which supplies real diversity to replace it.
  3. Report effective sample size instead of fixing it: have `rank.exe rate` emit a
     distinct-trajectory count per pair and an effective-n column, so a reader can see
     that a 32-game pair is 2 games. Cheapest, and worth doing regardless of 1 and 2.
- **Boost the category-champion pools to 32 games/pair. `[Next]`** {cpu: hours, dev: low}
  Round 1 (2026-07-28, 24 agents, 116->140 active) and round 2 (2026-07-29, 18
  more agents incl. `s3`/`adv` own-books at 4/8-ply, 140->158 active) both
  screened at only 8-11 games/pair, short of this project's own 32-games/pair
  top-of-table standard (`CLAUDE.md` rule 2). Consequence: only 4-book/8-book
  clear ~2 combined SE over their runner-up; openless, 4-random, and 8-random
  are all within about 1 SE of a tie, and growing the roster in round 2 made
  4-random and 8-random's gaps SMALLER, not larger (`ranking/CHAMPION.md`).
  None of the 5 declarations should be treated as settled until this is done.
  Remaining book-category growth candidates: `s4`/`s9`/`s10`/`s94`/`s95`/`s97`/
  `s99` don't have an established own-book pair yet (would need a fresh
  bookgen source, unlike `s3`/`adv` which reused book6/book3's existing
  target); the wide dist-mlp cores (`s77`/`s78`/`s79`/`s110`/`s112`/`s114`/
  `s115`, 350-1670 ms/move) were deliberately skipped from the random
  categories for cost, same call as `book5`'s exclusion.
- **Explain the 30-ply book depth rung. `[Next]`** {cpu: hours, dev: medium}
  Book depth was varied for the first time (6/16/30/60 ply, `models/book7..12`). On the
  `classic` core the lift rises with depth (+54, +45, +71, +110 over bare). On the `s98`
  core it does not (+70, +82, +9, +55), and the 30-ply rung sits 73 Elo below its 16-ply
  neighbour at +/- 10 each, far outside the bars. Untested hypothesis: handing off to
  the brain mid-middlegame is worse than handing off early or carrying to the endgame.
  Test by mining intermediate depths (20, 24, 36, 44) on the same pair and looking for a
  trough, and by instrumenting `openerBook` with a per-game in-book ply counter.
- **Re-evaluate every Elo claim under the new comparison hygiene, then clear its banner. `[Now]`** {cpu: hours, dev: high}
  On 2026-07-25 two defects were found in this project's Elo reporting: (a) numbers read
  from `ranking/ratings.tsv` mixed RETIRED agents (`active = gone`, superseded `@N`
  identities frozen at old game counts) in with live ones, and (b) agents were compared
  across different SEARCH HEADS, which are different agents. Measured instance: a retired
  `classic` row reads 1081 where its live identity reads 990, a 91-Elo phantom gap that
  inverted one conclusion. Fixes shipped the same day: `rank.exe rate` now writes
  `ranking/standings.tsv` (active only, grouped by head), `CLAUDE.md` gained ranking-claim
  hygiene rules (5) and (6), and `Docs/benchmarking.md` has the full explanation under
  "Elo comparison hygiene". A third defect was added 2026-07-26: nominal stored games are
  not distinct games (see the diversity item above), so every re-check below must also
  count distinct trajectories before trusting a record or an error bar.
  Progress: `Docs/theories.md` theories 14 and 33 corrected 2026-07-26, and
  `ranking/CHAMPION.md` flagged. Banners stay until the whole document is re-verified.
  **37 documents were flagged with an `[ELO HYGIENE UNVERIFIED]` banner** (all Elo-citing
  files in `plans/`, plus `ML.md`, `Docs/agents.md`, `Docs/axioms.md`, `Docs/theories.md`,
  `ranking/CHAMPION.md`). For each one: re-check its numbers against `standings.tsv` within
  a single fit at a fixed head, confirm or correct each finding it accepted or refuted,
  then delete that document's banner. A finding that flips must also be corrected in
  `Docs/theories.md` and, if it touches the throne, in `ranking/CHAMPION.md`.
  Suggested order (highest claim density / most load-bearing first):
  1. `ranking/CHAMPION.md` (the throne declaration itself)
  2. `Docs/theories.md` (the theory ledger other docs cite)
  3. `ML.md` (the shipped-models tables)
  4. `plans/dethrone-champion-results-*` (5 docs, all champion-relative claims)
  5. `plans/position-oracle-results-1`, `plans/heuristic-eval-overhaul-results-1`,
     `plans/bounded-jitter-results-1`, `plans/vs-champion-training-results-1`
  6. the remainder of `plans/`
- **Loadout-parity study: stop comparing bare cores against an equipped champion. `[Next]`** {cpu: days, dev: medium}
  The reigning champion is a `classic` core wearing one loadout item (`.opener(book,book=2)`),
  and that item is worth **+124 Elo** on that core (990 bare -> 1114 equipped, 2026-07-25
  fit, head `ab(deep=6,tt,ord,nodes=200k)`). Every learned/dist/hill-climbed agent it has been
  measured against is **bare**, so those comparisons have been reading the loadout, not
  the core. Report all three numbers instead of one: (1) **bare vs bare** (the honest core
  comparison), (2) the **lift** of each loadout item on each core (same core, with and
  without), and (3) **equipped vs equipped** (best achievable build of each core). See
  `Docs/terminology.md` for core / loadout / bare / equipped / loadout-matched / lift.
  **Critical implementation constraint, already established:** books are NOT portable
  across cores. Theory 14 was refuted in its naive foreign-book form and theory 33
  confirmed the self-mined form -- a book must be mined from the WEARER'S OWN wins.
  Current-fit evidence at the same head: classic + its own book2 = 1114 (**+124**), classic
  + the foreign book1 = 982 (**-8**), s98 + book1 = 1066 (+23 on the core the book was
  mined against). So do NOT hand the champion's `book2` to a learned agent and call it a
  fair build. Mine each core its own book via `rank.exe bookgen` from that agent's own
  wins, give it a fresh slot (book files are immutable and not hashed into the ID), and
  rate the equipped identity separately.
  Loadout items to sweep per core, each already an ID-level toggle: own-mined opening book,
  quiescence (`qs`), transposition table (`tt`), move ordering (`ord`), aspiration window
  (`asp`). Expected payoff beyond fairness: if lifts are roughly additive, the strongest
  agent is the best core wearing every positive-lift item, which no one has actually built
  yet -- the champion wears exactly one.
- Standing project loop: on every new agent, check whether it lowers the current #1's
  Elo, by outrating it outright or by countering its specific build (see the
  adversarial counter-agent idea above). Treat "dethrone the champion" as the
  recurring success criterion, not just "raise some Elo" in isolation `[Now]` {cpu: seconds, dev: high}
- Roster curation policy (interim, until the classifier below exists): keep `on` the
  anchor, the dilution ladder, the reigning champion family, one oracle, the best agent
  per distinct data-source family (replay, self-play, vs-champion, oracle-mimic,
  branch-mined), and any agent with a distinctive opponent-bucket profile (e.g. a
  counter-agent that beats the champ but loses broadly). Retire (`off`) near-duplicates
  whose head-to-head profiles match an existing agent, since their games stay in
  `matches.jsonl` forever `[Now]` {cpu: minutes, dev: high}
- Agent behavioral classifier: characterize agents by how they PLAY, not just Elo, and
  use it to decide which agents are interesting enough to keep active. Features:
  position-distribution overlap between agents (shared `positionKey` histograms over
  their stored games, so two agents reaching the same positions 99% of the time rate as
  near-identical), a left-right symmetry measure, and responses against a fixed
  discriminator agent as a feature vector. Cluster k-means-style and keep the most
  interesting few per cluster. Big project, deliberately deferred `[Later]` {cpu: days, dev: high}

## Books (openers and mid-game)

- **Opening book chosen by win rate over the roster's own game history.** Pick each
  position's move by how often it won across every game already in the match store,
  instead of mining one agent's wins against one target the way `rank.exe bookgen`
  does today. Iterations worth trying:
  - Restrict the source games to random-opener agents, so the book is built from
    diverse openings rather than the few lines deterministic pairs replay.
  - Weight each win by the Elo-EXPECTED win rate of that matchup rather than counting
    it flat, so beating a much stronger opponent counts for more. `rank.exe matchup`
    already computes actual minus Elo-expected, so the per-game expectation is on hand.
  - Compare against the existing per-pair mined books. Theory 38
    (`plans/book-opener-audit-results-1-vivid-lantern.md`) found a mined book's lift is
    a memorized-line artifact that collapses under opening diversification. A win-rate
    book drawn from many agents may or may not inherit that failure, and finding out
    is most of the value here. `[Next]` {cpu: hours, dev: medium}

- **Mid-game book: identify difficult positions and the best response.** Key the book on
  positions where an agent went on to lose, ranked by how many games the correction
  would have saved across the roster.
  - Only positions where it is the eventual loser's turn.
  - Only positions reached at least 16 times by DIFFERENT agent pairs, lowering that
    threshold iteratively once the candidates are exhausted.
  - Exclude the first 4 moves, which the opening book already covers.
  - For each agent pair that reached the position, replay with a different move
    substituted for the loser and keep it if the loser now wins. `pairgen
    --branch-tries` already rewinds to a snapshot and substitutes a move, so the
    replay machinery exists.
  - Sort entries by games saved, so the book can be truncated to whatever is worth
    carrying. `[Next]` {cpu: hours, dev: medium}

## Position and agent analysis

- **Do strong agents recognise the lines a random agent beat them on?** Collect games
  where a random agent beat an agent rated 500+ Elo above it, then check whether the
  strong agents' own evaluators score those lines as strong. If they do not, the losses
  are an evaluation or search blind spot rather than variance. `[Next]` {cpu: minutes, dev: high}

- **Position similarity score.** For a given position, find or generate another position
  with a similar win rate for EVERY agent pair. A position's signature is the whole
  vector of pairwise win rates, not one number: agent A may beat B from it 90% of the
  time but beat C only 60%.
  - Some positions likely have win rates that barely depend on who is playing. Weight
    the score by how common that distribution of pairwise win rates is, so an
    unremarkable signature counts for less.
  - Worth splitting: the likelihood an agent REACHES a position versus the likelihood
    it WINS from it. Different questions, possibly uncorrelated. `[Later]` {cpu: hours, dev: high}

- **Agent similarity score: the proportion of positions where two agents play the same
  move.** Scores agents on how they PLAY rather than on how they were trained, which is
  what actually determines a matchup. `rank.exe posgen` already builds deduped, ply and
  material stratified pools, so the benchmark set exists. Would settle empirically what
  provenance cannot, for example whether a TD-Leaf model initialised from another
  agent's weights belongs with its ancestor or with the other TD-Leaf models. `[Next]` {cpu: hours, dev: medium}

- **Model to predict which agent pair generated a board position.**
  - Baseline first, no model: search the history for that exact position, or the one
    the least piece-distance away, and return the agent pair that played it.
  - Then test whether a trained model beats that baseline. Open question is how much
    data diversity it needs. Unique 1-distance positions may suffice, or it may need
    1-distance and beyond to generalise to novel positions.
  - Hold out ENTIRE GAMES, not individual positions: positions from one game are not
    independent samples.
  - Related measurement this would need anyway: how different does a random opening
    actually make the games? `[Later]` {cpu: days, dev: high}

## Agent Composition + Play
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
  search itself, not the check) `[Next]` {cpu: seconds, dev: low}

## Data + Infrastructure
- **Make the Bradley-Terry SE honest, then schedule to a target SE** `[Now]` {cpu: hours, dev: medium}. The fit
  counts stored rows as independent samples, but a deterministic pair replays one game
  per colour, so store-wide only 0.438 distinct games per row -- every printed `pm` is
  understated by roughly 1.5x. Fix the fit to weight a pair by DISTINCT games, then
  replace `--games N` with a target ("every active agent at SE <= 8 Elo") and allocate
  greedily to whichever pair most reduces the worst SE. Fisher information per game is
  `p(1-p)`, so a lopsided pair contributes almost nothing and uniform games/pair spends
  most of its compute on settled matchups. Sparsity then falls out instead of being
  hand-tuned. Scheduling half is done (deterministic pairs pinned at 2); the fit half
  is not.
- **Regime-level pooling: ordering confirmed, magnitude not** `[Next]` {cpu: hours, dev: medium}. Measured
  2026-08-03 over 209 agents' matchup-residual profiles: same regime + same opener
  r=0.150, same regime different opener r=0.059, different regime same opener r=-0.010,
  different both r=-0.045. So the grouping is real and REGIME separates more strongly
  than opener (0.160 vs 0.091 drop), which is the opposite of the initial guess. But
  r=0.150 is far too weak to treat regime members as exchangeable, and the residuals
  are measured on 8-game samples whose noise attenuates correlation toward zero, so
  the true value is an unknown amount higher. Re-run after the SE fix before building
  a hierarchical fit or regime-sparse scheduling on it.
- **Running the test suite overwrites `models/manifest.{json,md}`** `[Next]` {cpu: seconds, dev: low}. The
  ML tests call `writeManifest` with their own scratch rows, so `tests.exe`
  replaces the real committed manifest with a single `build\dist_model_ckpt30.txt`
  entry. Noticed 2026-08-01 while reviewing a diff before committing, and the
  corruption is easy to commit by accident since it looks like an ordinary
  modification. Fix by pointing the tests at a scratch manifest path, or by
  having them restore what they overwrote.
- ~~Screening cohorts flood the permanent ladder~~ Fixed 2026-08-01:
  `tools/tdleaf_study.ps1 -ScreenStore` plays cohorts into
  `ranking/matches_screen.jsonl` instead. The pinned screening fit is unchanged
  (cohort-vs-roster games live in that store and the roster enters pinned). On
  promotion, `rank.exe split` over the screening store separates the kept
  agents' games for merging into the ladder. **Other study scripts still write
  to the permanent store** -- `sweep_pst_v2.ps1`, `train_vs_champion.ps1`, and
  `hill_climb.ps1`'s promote path should be audited for the same problem `[Next]` {cpu: minutes, dev: low}
- Python analysis layer (DuckDB queries: top Elo, fairest positions, avg eval per position) `[Later]` {cpu: minutes, dev: low}
- Python training (PyTorch) for MLP/NNUE/transformer, exporting C++-format weights `[Later]` {cpu: days, dev: high}
- Optional Weights & Biases tracking (local metrics by default, W&B opt-in) `[Dream]` {cpu: minutes, dev: low}
## GUI
- Add MLP models to the GUI (developer, 2026-07-21). `DrawPlayerConfig` generates its controls
  from the evaluator registry the same way the console's `getEvaluatorSettings` does (root
  `CLAUDE.md` Architecture Notes), so this is likely a model-slot-picker generalization rather
  than new UI machinery -- needs checking `gui/main_gui.cpp` for whatever currently limits
  LearnedValue selection to linear models specifically `[Now]` {cpu: seconds, dev: high}
- Set a dist MLP (`dist_mlp_wide` once incrementalized, or any dist model meanwhile) as the
  GUI's default on-screen board-state evaluator `[Now]` {cpu: seconds, dev: high}
- Give the on-screen board-state evaluator iterative deepening: currently (needs confirming
  against `gui/main_gui.cpp`) it looks like a static immediate leaf eval; make it a live,
  progressively-deepening background search the way a chess GUI's analysis panel updates as it
  thinks, rather than a single flat number `[Now]` {cpu: seconds, dev: high}
- Sharding to evaluate multiple lines at once in the GUI without freezing it (a multi-PV-style
  analysis view). Real architectural tension to resolve first: the engine's board/eval state is
  global (root `CLAUDE.md` Architecture Notes), which is exactly why every other parallel
  workload in this project (rank.exe, tournaments) shards across separate OS processes rather
  than threads. A GUI analysis panel showing several lines at once likely needs the same
  process-per-line approach with results streamed back to the GUI process, not an in-process
  thread pool over the current global-state engine `[Now]` {cpu: minutes, dev: high}
