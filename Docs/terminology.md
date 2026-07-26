# Terminology

A glossary of project-specific and domain terms used across Breakthrough's
code and docs, each with a one-sentence definition and an example sentence.
Terms are grouped by area; within a group they're roughly in the order a
newcomer would meet them.

## Game fundamentals

**Piece** -- one of the 8x8 board's tokens, colored White or Black, each
occupying one square.
*"A piece captures diagonally onto an occupied enemy square."*

**Move** -- one legal action by the side to move: a forward step or a
diagonal step/capture.
*"White's move slid the piece on c2 forward to c3."*

**Ply** -- one move by one side; because Breakthrough alternates single moves
rather than paired White+Black "turns" the way chess notation does, ply and
move are used interchangeably in this project's code and reports (for
example the match-row field `plies`, or `report.md`'s "avg plies" column).
*"The game ended after 47 plies, roughly 24 for each side."*

**Turn** -- which color is currently to move; alternates White/Black after
each ply.
*"It's White's turn again after Black's capture."*

**Turn weight (tempo weight)** -- the evaluator parameter `p[0]` (the `t` in
an agent ID such as `classic(t1,c4,w0,l0)`), added to the leaf score as
`+t` when White is to move and `-t` when Black is. It prices the value of
having the move. `train.exe turn-swing` calibrates it by measuring the
1-ply white-centric eval swing between sides.
*"The champion's turn weight is 1, against a chip weight of 4."*
Read the turn weight with care, because it is usually inert: it depends only
on the side to move, never on the board, so in a fixed-depth search every leaf
is at the same ply parity and receives the *same constant*, and a constant
added to every leaf cannot change any min/max comparison. It becomes live
only when leaves sit at mixed ply parity, which in this engine means
quiescence (`qs`) is on, or `keepPartial` retains a cut iteration. It also
still shows up in displayed evaluations (`SHOW_EVAL`). Consequence: for a
bare fixed-depth agent, two IDs differing only in `t` describe agents that
play identically, and a ratio involving `t` (for example "chip/turn")
carries no meaning. The hill climber pins `t` rather than searching it, so
its `t` value is a script default, not a result.
*"Both agents are `ab(d6,ord,nb200k)` with no quiescence, so their differing
turn weights of 20 and 1 have no effect on the moves either one plays."*

**Wall** -- a structural-eval bonus for two same-color pieces that sit
orthogonally adjacent to each other, horizontally, so they can defend one
another.
*"Two White pieces side by side on row 4 form a wall, adding to Classic's
wall bonus."*

**Column** -- a structural-eval bonus for two same-color pieces vertically
adjacent in the same file.
*"A piece with a same-color piece directly behind it forms a column, scored
by the column weight."*

**Chip / chip diff** -- a piece; "chip diff" (`g_chipDiff`) is the material
term in evaluation, white piece count minus black piece count.
*"A 3-chip lead is usually decisive by the midgame."*

**Forward** -- the Experimental evaluator's extra weight rewarding pieces
that are further advanced toward the opponent's back row.
*"Increasing the forward weight makes Experimental push pieces more
aggressively than Classic would."*

**Opener** -- a scripted opening sequence (`OffensiveOpener`,
`DefensiveOpener`) a MiniMax agent plays before switching to normal search,
automatically disabled once the opponent advances into its half of the
board.
*"OffensiveOpener has edge pieces attack diagonally while center pieces push
forward."*

## Agents and move choosers

**Agent** -- anything that plays Breakthrough: given a board and a side to
move, it produces a move. An agent decomposes into either a **Search** (an
Explorer paired with an Evaluator) or a **Policy** (a direct Chooser).
*"The champion agent is `ab(d6,tt,ord,nb200k)@1.classic(...)@1`, a depth-6
alpha-beta search over the Classic evaluator."*

**Search** (agent brain) -- an agent that explores the move tree with an
Explorer and scores positions along the way with an Evaluator.
*"MiniMax at depth 6 is a Search agent pairing the AlphaBeta explorer with
the Classic evaluator."*

**Policy** (agent brain) -- an agent that picks a move directly via a
Chooser, without exploring a tree.
*"LearnedPolicy is a Policy agent: it rates each legal move with a trained
model and plays the argmax."*

**Explorer** -- the move-tree traversal strategy a Search agent uses; the
two registered explorers are Greedy (1-ply argmax) and AlphaBeta (full
minimax search).
*"Swapping the explorer from Greedy to AlphaBeta turns a 1-ply lookahead
agent into a full minimax searcher."*

**Evaluator** -- a function that scores a board position for a side to move;
the three registered evaluators are Classic, Experimental (Classic plus a
Forward weight), and LearnedValue (delegates to a trained model).
*"The Experimental evaluator is identical to Classic whenever its Forward
weight is 0."*

**Chooser** -- the direct move-selection strategy a Policy agent uses (the
random-move family, or LearnedPolicy).
*"SmartRandom is a Chooser that restricts candidate moves to the
furthest-advanced N pieces."*

**Dilution** -- mixing a fraction of random (or shallower-search) moves into
an otherwise-strong agent's play, to diversify training data or give an
opponent pool some non-determinism; a diluted move is either fully random or
a shallower depth-N search.
*"The roster's `dil(r0.3,d2)` ladder plays 30% of its moves as a depth-2
search instead of the full depth-6 search."*

**Opener (identity-level)** -- an `AgentSpec` property (openerKind + openerArg),
distinct from the scripted `OffensiveOpener`/`DefensiveOpener` above: an opener
from the pluggable registry `g_openers[]` overrides the agent's brain during the
opening phase, then hands off. Carried in the canonical ID as
`.opener(<idName>[,<arg>])@N`. One opener is registered so far, `rand`
(`.opener(rand,N)@1`: a uniform-random move for the agent's first N plies); the
registry is where an opening-book or scripted-opener kind would be added. Lets
the same agent be rated both with and without an opener as two roster entries,
so the Elo gap is a general opener-sensitivity measure for any agent.
*"`champion...classic(...)@2.opener(rand,6)@1` scores ~217 Elo below the clean
champion in the ranking pool, purely from 6 random opening plies."*

## Search mechanics

**Depth** -- how many plies ahead a Search explorer looks before calling the
evaluator.
*"MiniMax at depth 4 explores roughly 4 plies of the game tree before
evaluating."*

**Node** -- one board position visited during a search, whether or not it
turns out to be a leaf.
*"`nodesWhite` counts every position White's search touched this move."*

**Leaf** -- a node where the search stops recursing and calls the evaluator
instead, either because it hit the depth limit or a budget cutoff.
*"At the leaf, `evalLeaf` combines the incremental positional score with
chip diff and the turn term."*

**Alpha-beta (pruning)** -- a minimax search optimization that skips
branches that can't change the final decision, tracked with two running
bounds (alpha, beta).
*"Alpha-beta pruning lets depth-6 search finish in a fraction of full
minimax's node count."*

**Move ordering** -- trying the more promising moves first within a node so
alpha-beta prunes more branches; here that order is captures, then empty
diagonal advances, then the forward move.
*"Capture-first move ordering is why so few of a node's children ever get
explored at depth 8."*

**Transposition table** -- a cache of previously-searched positions, keyed
by a position hash, that lets a later search reuse or cut off work on a
position it has already evaluated.
*"With `useTT` on, a transposed sequence of moves that reaches an
already-seen position gets its stored score instead of a full re-search."*

**Node budget / time budget** -- a cap on how many nodes (or how much
wall-clock time) a single move's search may spend, enforced via iterative
deepening.
*"A 200,000-node budget keeps deep searches bounded in tournament play, at
the cost of an incomplete final iteration."*

**Effective depth** -- a fractional measure of how deep a budgeted search
actually reached: completed whole plies plus the fraction of the cut
iteration's root moves explored.
*"A search that finished depth 5 and got through half of depth 6's root
moves before hitting budget has effective depth 5.5."*

**Quiescence (`qs`)** -- a captures-only search extension past a true depth
leaf, so the search does not stop in the middle of an exchange and misread
the material. It makes leaves sit at mixed ply depths, which is what turns
the otherwise inert turn weight live.
*"With quiescence on, a depth-1 search resolves the exchange a plain leaf
would have scored as a free capture."*

**Aspiration window (`asp`)** -- seeding the root search with a narrow
alpha-beta window around the previous iteration's score, re-searching wider
only if the value falls outside it.
*"An aspiration window costs nothing when the score is stable and forces a
re-search when it is not."*

**us/node (per-node cost)** -- microseconds of search time divided by nodes
visited, the cost of one leaf evaluation. It is the speed metric that
survives comparison across evaluators, because a stronger evaluator changes
how many nodes get pruned and so changes us/move for reasons unrelated to
leaf cost.
*"The two heads differ by 2x in us/node even though their us/move looks
similar, because their node counts differ."*

**Win decay** -- reducing a forced-win score by 1 per ply deeper it is
found, so the search prefers the fastest forced win among several.
*"Win decay is why the search picks a 3-move forced win over an equally
certain 7-move one."*

**Simulate / unsimulate** -- applying and then reversing a move in place on
the shared global board, so search never has to copy the board.
*"`simulateMove`/`unsimulateMove` let alpha-beta explore into and back out
of a branch without allocating a new board each time."*

**Incremental evaluation** -- keeping a running eval score updated on each
simulate/unsimulate call (since a move only changes 2 squares) instead of
rescanning the whole board at every leaf.
*"Incremental evaluation is why `g_evalPos` only needs a 2-square delta per
move instead of a 64-square rescan."*

## Machine learning

**Feature version (v1 / v2)** -- which scheme a value model reads the board
through; v1 is dense white-centric aggregates, v2 is a sparse piece-square
layout (one binary input per color and square) that supports incremental
evaluation.
*"A feature-v2 model can use the incremental `g_mlAcc` accumulator; a v1
model falls back to a full scan at every leaf."*

**Model slot** -- one of 128 in-memory model registers (`g_mlModels`) that
an evaluator, side, or sweep candidate can point at, so many models can be
loaded and compared within one process.
*"Slots 3 and up hold sweep candidates like `models/sweep/slot7.txt`, while
slots 0-2 are the fixed value/policy/PST slot conventions."*

**Distributional (dist) model, mu head, sigma head** -- a model that predicts
a position's strength as a distribution rather than a point: a `mu` head for
the mean advantage (in logits, convertible to Elo) and a `sigma` head for its
volatility. Search reads only mu, so only mu needs to be fast.
*"The dist model's mu head says White is 140 Elo better, with a sigma saying
that estimate is not very stable."*

**Accumulator (NNUE-style)** -- search-resident state holding a model's
first-layer pre-activations, updated by the two or three inputs a move
changes instead of recomputed from scratch, and reversed on unmake. It makes
the first layer's UPDATE nearly free.
*"With the accumulator the first layer costs three column adds per move
instead of a full matrix multiply per leaf."*

**Leaf update vs leaf read** -- the two halves of an incremental
evaluation, and a distinction worth keeping separate because optimizing one
does not help the other. The *update* is the per-move maintenance the
accumulator makes cheap; the *read* is what every leaf must still do to turn
that state into a score, which for a hidden layer of width H is O(H) work
regardless of how cheap the update got.
*"Widening the first layer kept the update free but made the leaf read the
dominant cost."*

**Dead ReLU / activation churn** -- how sparse a hidden layer is. A unit is
dead at a position if its post-ReLU activation is zero (this project's dist
heads are roughly 90-96% dead); churn is the fraction of units whose
activation changes across one move.
*"At 96% dead only about 18 of 512 units feed the next layer."*

**Sparse leaf-tail forward** -- evaluating the layers past the first over
only the nonzero activations. Because a zero activation contributes nothing
to any downstream sum, this is bit-identical to the dense computation.
*"The sparse leaf tail skipped 90% of the second-layer multiply-accumulates
without changing a single score."*

**Bit-identical** -- an optimization whose output is exactly, bit-for-bit,
what the unoptimized code produced, so no test or rating can change. Weaker
guarantees (reordered floating-point sums, fused multiply-add, SIMD
reductions) are *approximate* and must say so.
*"Skipping zero terms is bit-identical; vectorizing the sum is not, though
the difference is far below one eval point."*

**Seed replica / seed-noise band** -- retraining one recipe under several
random seeds to measure how much of an Elo difference is just training
noise. In this project that band is roughly 50-150 Elo, so a single seed's
result is never a conclusion.
*"Six seed replicas of the same recipe spanned 129 Elo, which is why one
model beating another by 40 proves nothing."*

**Teacher** -- the agent (usually a search) whose evaluations or move
choices generate the labels for a training run; recorded as provenance in
the saved model file.
*"The model's `teacher=` line records that it was trained on labels from a
depth-2 Classic self-play teacher."*

**Self-play** -- generating training games by having an agent (often
diluted) play against itself or a copy of itself.
*"Self-play generation is how the trainer produces labeled positions without
any external game data."*

**Imitation (behavioral cloning)** -- training a policy model to reproduce
the move choices of an existing move-rater, rather than to predict a game
outcome.
*"`trainImitationPolicy` clones a move-rater's choices into a linear policy
model."*

**Replay extraction** -- re-deriving labeled training positions from games
already stored in the ranking match history, rather than generating fresh
self-play games.
*"Replay extraction beat a bespoke self-play teacher at zero extra
generation cost, per `training-sweep-results-1`."*

**Branch mining** -- rewinding a stored winning game to a random ply,
substituting a different legal move, and keeping the resulting line only if
the original winner still wins, to mine alternative winning training lines.
*"Branch-mined data supplements a dataset with winning lines that weren't in
the original stored games."*

## Ranking and tournaments

**Canonical ID** -- the exact string identifying an agent's full
configuration (chooser/explorer/evaluator, weights, model hash, dilution,
per-module code versions), used as the permanent key in the match store.
*"`ab(d6,tt,ord,nb200k)@1.classic(t2,c10,w3,l2)@1` is a canonical ID for a
depth-6 alpha-beta agent using the Classic evaluator."*

**Search head (head)** -- the leading segment of a canonical ID: the explorer
and its search settings, e.g. `ab(d6,tt,ord,nb200k)` (alpha-beta, depth 6,
transposition table, move ordering, 200k node budget). Because an agent is
core + loadout on top of a head, agents at different heads are different
players and their Elos are not interchangeable. Every strength comparison
fixes ONE head and says which.
*"`ab(d6,tt,ord,nb200k)` and `ab(d6,ord,nb200k)` differ only in the
transposition table, but they are still two different agents."*

**Roster** -- the hand-edited list of agents (`ranking/roster.txt`) active
in the persistent Elo pool, each marked anchor/on/off.
*"Adding an agent to the roster and running `rank.exe run` schedules only
its missing games against the rest of the pool."*

**Active / off / retired ("gone")** -- an agent's roster state in a ratings
table. `on` is on the live roster, `off` is temporarily disabled, `gone` is
retired: an identity no longer rostered, usually a superseded `@N` code
version, frozen at whatever games it had when it left. A retired row's Elo is
history, not current strength, so it is never quoted as an agent's rating.
*"That 1081 was a retired row; the live identity of the same agent rates
990."*

**Ratings vs standings** -- `ranking/ratings.tsv` is the full historical fit
(every agent ever rated, retired ones included, all heads mixed together);
`ranking/standings.tsv` is the same fit filtered to active agents and grouped
by head. Standings is what a current-strength claim reads.
*"Read standings.tsv for who is strongest today, ratings.tsv when you need
the whole history."*

**Effective evaluator (`eff_evaluator`)** -- the standings column showing an
evaluator with parameters that cannot affect play elided, currently the turn
weight when the head has no quiescence. Cores are compared on this; the
`evaluator` and `id` columns keep the exact canonical spelling.
*"As an effective evaluator the climbed agent is `adv(c77,...)`, so its
pinned `t20` never enters the comparison."*

**Refit / anchored refit** -- recomputing every agent's Elo from the whole
match store at once, with the anchor pinned at 0. Because the Bradley-Terry
prior compresses the scale as the pool grows, ratings are comparable only
within a single refit, never across two.
*"Both numbers come from the same anchored refit, so their gap is
meaningful."*

**Games per pair** -- how many games each pair of agents has actually played,
the resolution of a comparison. Roughly 8 per pair screens; a top-of-table
claim needs at least 32, since 8-per-pair reads have twice inverted at 32.
*"At 8 games per pair these four are indistinguishable; resolving their order
needs a boost to 32."*

**Provisional** -- an agent whose games do not connect to the anchor through
the pool, so its rating floats relative to its own component rather than the
anchored scale. Marked `~` in the report.
*"A brand-new agent that has only played other new agents comes out
provisional until it plays into the main pool."*

**Screening vs certification** -- a gauntlet screens (cheap, one candidate vs
a frozen pool, enough to rank candidates against each other); a full-roster
anchored refit certifies (the instrument any published claim must use).
*"The sweep screened 40 cells; only the best two were certified."*

**Target class vs reference class** -- target-class agents compete for the
throne; reference-class agents are deliberately excluded from it, currently
the d8/nb2m oracle because it runs at ten times the node budget. The
exclusions are named in `ranking/CHAMPION.md`.
*"The oracle outrates the champion, but it is reference class, so the throne
is unaffected."*

**Anchor** -- the single roster agent pinned at Elo 0, against which every
other rating is relative.
*"The anchor's rating never moves; every other agent's Elo is measured
relative to it."*

**Gauntlet** -- rating one candidate agent against the whole frozen pool in
O(N) games, without touching the rest of the pool's ratings.
*"A gauntlet run is how the hill climber cheaply scores each candidate
eval-weight mix."*

**Core** -- the part of an agent that decides moves on its own merits: the
evaluator (or policy model) plus its search. What is being studied when a
new model or weight mix is trained.
*"Both agents share the classic core; they differ only in loadout."*

**Loadout** -- the set of optional, core-independent components an agent
wears on top of its core: an opening book or other opener, quiescence,
transposition table, move ordering, aspiration windows, dilution. Each
appears as an optional segment in the canonical ID, so an agent's loadout is
readable off its ID.
*"The champion's loadout is a single item, `.opener(book,2)`."*

**Bare / equipped** -- an agent with an empty loadout is *bare*; one wearing
any loadout item is *equipped*. Preferred over "vanilla/modded" (a loadout
adds to a core rather than modifying it) and over "basic/featured" ("feature"
already means an ML input feature in this project, e.g. `MLV2_FEATURES`).
*"Comparing a bare learned evaluator against an equipped chip counter
measures the loadout, not the evaluator."*

**Loadout-matched** -- a comparison in which both agents wear the same
loadout, so the Elo difference is attributable to the core. The complement of
fixing the search head: head fixed + loadout matched isolates the evaluator.
*"Bare-vs-bare and equipped-vs-equipped are both loadout-matched; bare-vs-
equipped is not."*

**Lift** -- the Elo a single loadout item adds to a given core, measured as
the same core with and without it, at one head in one fit. Lift is
core-specific and need not transfer.
*"The self-mined book's lift on the classic core is +124 Elo (990 bare ->
1114 equipped), while the same core gains nothing from a foreign book."*

**Champion** -- the top-rated agent from the last full tournament rating,
written to `agents/champion.txt`; the reigning opponent the vs-champion
training study and dethrone efforts are measured against.
*"Beating the champion head-to-head, not just matching its screening Elo, is
the real bar for a new model."*

**Pairgen** -- playing fresh games between two specific named agents (rather
than a whole roster) to generate a labeled training dataset.
*"`rank.exe pairgen` between the champion and a diluted copy of itself
produced the champdil training set."*

**Elo / Bradley-Terry fit** -- the rating system used to convert win/loss
records into a single strength number per agent, fit here via an anchored
Bradley-Terry maximum-likelihood model.
*"A 1140 vs 1137 Elo gap at d6 is close enough that the two agents are
considered roughly tied."*
