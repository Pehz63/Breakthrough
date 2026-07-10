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

**Roster** -- the hand-edited list of agents (`ranking/roster.txt`) active
in the persistent Elo pool, each marked anchor/on/off.
*"Adding an agent to the roster and running `rank.exe run` schedules only
its missing games against the rest of the pool."*

**Anchor** -- the single roster agent pinned at Elo 0, against which every
other rating is relative.
*"The anchor's rating never moves; every other agent's Elo is measured
relative to it."*

**Gauntlet** -- rating one candidate agent against the whole frozen pool in
O(N) games, without touching the rest of the pool's ratings.
*"A gauntlet run is how the hill climber cheaply scores each candidate
eval-weight mix."*

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
