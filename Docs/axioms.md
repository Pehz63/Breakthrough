# Breakthrough Truths

A list of what is known about Breakthrough, separated by how it is known.
Tier 1 holds the rules themselves. Tier 2 holds the rule choices this project
made that are not inherent to Breakthrough as a family of games, so a future
variant knows exactly what to swap. Tier 3 holds truths logically proven from
tiers 1-2. Tier 4 holds empirical truths: general strategic claims that are
not proven, only observed, each marked with a confidence level and the
evidence it rests on so it can be revisited if later evidence contradicts it.

Relationship to the other docs: [theories.md](theories.md) tracks specific
claims still under investigation (open, promising, or freshly tested).
A theory graduates into tier 4 here once it is settled enough to treat as
background knowledge, and a tier 4 entry moves back into `theories.md` if new
evidence reopens it. [terminology.md](terminology.md) defines the terms used
here (piece, ply, chip diff, wall, column, and so on).

## Confidence legend

| Level | Meaning |
|---|---|
| Certain | A rule (tiers 1-2) or a proof from the rules (tier 3). |
| High | Observed consistently across many independent measurements. |
| Medium | Observed, but in fewer settings or with known confounds. |
| Low | A weak or preliminary signal. |

Each tier 4 entry carries two confidence marks, one from the developer and
one from Claude, recorded separately. The developer's mark defaults to N/A
until filled in. Tiers 1-3 are Certain by construction, with one nuance
noted in tier 3: the developer supplied the no-stalemate argument directly,
while the other proofs were written by Claude and the developer's mark on
those stays N/A until reviewed.

## Conventions (not axioms)

Some choices carry no game content at all. They fix a symmetry or a labeling,
the way mathematics picks a coordinate system or argues "without loss of
generality" when a symmetry makes the choice free. Changing a convention
changes no substantive fact and no proof, only names. They sit below even the
softest axiom on the "how much breaks if you change it" scale, because the
answer is nothing.

- **"White" is defined as the side that moves first.** The color-swap map
  (truth D5 below) is an automorphism of the rules, so every statement about
  White has an exact mirror for Black. The first mover needs a name and
  "White" is it (`src/main.cpp`'s game loop calls `moveWhite` first).
- **Coordinate orientation.** The code indexes the board as `board[x][y]`
  with columns a-h at x = 0-7 and rows y = 0-7. White starts on rows 0-1
  (the bottom of the printed board) and advances toward row 7. Black starts
  on rows 6-7 and advances toward row 0. Board files list the top row
  (y = 7) first (`src/board_io.cpp`).
- **Evaluations are white-centric.** A positive eval favors White. Pure
  sign convention.

Throughout this document, "ahead of" and "forward" are relative to the
moving piece's own direction of advance, "home row" means the row a side's
pieces are advancing away from, and "goal row" means the opponent's home
row. Statements are written for White where a color is needed. By D5 every
one of them holds verbatim for Black with the colors and directions swapped.

## Tier 1: Direct axioms (the rules)

All Certain. Sources are the rules as implemented, with the enforcing code
cited.

| # | Axiom | Source |
|---|---|---|
| A1 | The board is a rectangular grid of squares. Each square is empty or holds exactly one piece (no stacking). Near-universal across the Breakthrough family, but stated explicitly. | `globals.h` board representation |
| A2 | There are two players, each owning a set of identical pieces. A piece never changes type, color, or owner. There is no promotion. | board encoding: `W`, `B`, `.` only |
| A3 | Players alternate, exactly one move per turn. Passing is not allowed. | `src/main.cpp` game loop |
| A4 | A piece moves exactly one square, and always one row toward its owner's goal: straight forward or diagonally forward. Never sideways or backward. | `src/moves.cpp` `tryMoveQuickWhite/Black` (lines 256-269) |
| A5 | The straight-forward move requires the destination square to be empty. There is no forward capture, and an enemy piece directly ahead blocks the forward move. | `src/moves.cpp:259` |
| A6 | A diagonal move may land on an empty square or on an enemy-occupied square (a capture). It may never land on a friendly piece. | `src/moves.cpp:258-259` |
| A7 | A captured piece is removed from the game permanently. Pieces are never added. | `src/moves.cpp` `playMoveWhite/Black` |
| A8 | Moving a piece onto the opponent's home row wins the game immediately. The arrival counts however it happens, by a straight advance, a diagonal step, or a diagonal capture. | `g_whiteAtEnd`/`g_blackAtEnd`, `src/board_analysis.cpp` |
| A9 | Capturing the opponent's last piece wins the game immediately. | `src/moves.cpp:216,248` |
| A10 | Capturing is never compulsory. A player with an available capture is free to play any other legal move (unlike checkers). | absence of any forced-capture rule in move validation |
| A11 | The game is deterministic with perfect information. Both players see the whole board at all times, and no rule involves chance. | the engine has no hidden state or dice anywhere |
| A12 | The board plus the side to move is the entire game state. Legal moves and outcomes depend on nothing else, there is no history dependence (no analog of chess's en passant, castling rights, or repetition counts). | move validation reads only `board[][]` |
| A13 | There are no other rules. No draw offers, no repetition rule, no move limit. | absence anywhere in the engine |

A12 is what makes `positionKey` a sound join key across the project's data
streams and the transposition table sound within a search: two positions
with the same board and side to move are the same position, full stop.

## Tier 2: Optional axioms (this project's choices)

Choices that define this project's Breakthrough but could be swapped by a
variant. Each entry notes what breaks if it is swapped, which is the real
measure of how load-bearing it is.

| # | Choice | What breaks if swapped |
|---|---|---|
| O1 | The board is 8x8 (`SIZE = 8`). | No tier 1 rule and no tier 3 proof except the numeric constants (the proofs need only width >= 2). What does break: every learned model (the v2 feature layout is 129 fixed piece-square inputs), every saved PST and eval weight set, the constants in D2, D12, and D13, and all tier 4 evidence, which was measured at 8x8. |
| O2 | The board is square (one `SIZE` constant serves both width and height). | Nothing in the rules or proofs needs squareness. A rectangular variant is a code change (split `SIZE` into width and height) plus everything in O1. |
| O3 | Each side starts with its two home rows completely filled (16 pieces per side, nearest its own edge). | The rules engine itself is start-agnostic (it loads any board file, which is how the `boards/puzzle*.txt` positions work). What breaks: opening theory, openers and any future opening book, the constants in D2, D12, and D13, and every empirical truth tied to the standard start. |
| O4 | The standard start is color-swap symmetric: applying D5's mirror map to it gives back the same position. | With a symmetric start, the only asymmetry in the whole game is who moves first, so any measured White edge (E1) is a pure first-mover effect. A handicap or asymmetric variant loses that reading, and E1-style claims would need re-measuring per start. |

## Tier 3: Derived truths (proven from tiers 1-2)

Three lemmas do most of the work. Everything after them is a consequence.
Truths whose statement uses a specific number (D2, D12, D13) depend on the
tier 2 start and board size and are marked so. The rest need only tier 1
plus a board at least 2 columns wide.

**Lemma A (no stalemate).** The side to move always has a legal move, as
long as it has at least one piece and the board is at least 2 columns wide.
Proof: take that side's most-advanced piece, on the highest row r it
occupies (counting toward its goal). Row r+1 is on the board, because a
piece standing on its goal row would already have ended the game (A8), so
during play r is never the goal row. The piece's forward-diagonal squares
lie on row r+1, which by choice of r contains none of that side's own
pieces. On a board >= 2 columns wide, at least one of the two diagonal
squares is on the board, and it is empty or enemy-occupied. Either way A6 makes moving there
legal. This argument is the developer's (Certain from both). Note an
individual piece elsewhere on the board CAN be fully blocked (straight
ahead occupied, one diagonal off the board, the other holding a friendly
piece), which is why the proof picks the most-advanced piece rather than
claiming every piece can move. A special case of the same argument: a
side's lone remaining piece can never be blocked, since it has no friendly
pieces to block its diagonals. The engine still guards the no-move case
defensively (`src/explorers.cpp:16` scores it as a loss for the stuck
side), but on any legal >= 2 column board that code path cannot fire.

**Lemma B (strict progress).** Define a position's remaining capacity as
the sum over all pieces on the board of that piece's row distance to its
goal row. Every move advances the mover exactly one row (A4), so it lowers
remaining capacity by exactly 1, and a capture removes the victim's whole
remaining distance on top. Remaining capacity therefore strictly decreases
every ply and is never negative.

**Lemma C (capture geometry).** A piece can be captured only by an enemy
piece that, at the moment of capture, stands exactly one row ahead of it
(toward the victim's goal) in an adjacent column. Proof: the only capturing
move is a diagonal step (A5, A6), and the capturer moves one row toward its
own goal (A4), so to land on the victim's square it must start one row on
the victim's goal side of it, one column over. Corollary: a piece can never
be captured from behind, from its own row, or from its own column.

### D1. The game always terminates

*Needs: tier 1.* Remaining capacity strictly decreases each ply (Lemma B)
and is bounded below by 0, so play cannot continue forever. A win must occur
first: capacity 0 would mean every surviving piece stands on its goal row,
and the first such arrival already ended the game (A8).

### D2. From the standard start, a game lasts at most 208 plies

*Needs: O1, O3.* Initial remaining capacity is 8 pieces at distance 7 plus
8 at distance 6 per side, 104 each, 208 total. Each ply consumes at least 1
(Lemma B). Not a tight bound. The general formula for any start is the
initial remaining capacity.

### D3. No draws are possible

*Needs: tier 1.* The side to move always has a move (Lemma A), so no
stalemate. Every way a game ends names a winner (A8, A9). Play cannot go on
forever (D1). Nothing is left for a draw to be.

### D4. No position ever repeats within a game

*Needs: tier 1.* Remaining capacity is a function of the board alone and
strictly decreases every ply (Lemma B), so no board can occur twice.
Consequences: repetition rules would be vacuous (consistent with A13), the
game graph is a DAG, and with A12 a transposition-table entry can never be
invalidated by a cycle.

### D5. Color-swap symmetry

*Needs: tier 1.* The map (x, y) -> (x, SIZE-1-y) combined with swapping
piece colors and the side to move sends legal positions to legal positions
and legal moves to legal moves, because every rule in tier 1 is stated
symmetrically in the two colors and the flip reverses the forward
direction. Outcomes map to outcomes with colors swapped. This is what
licenses the "White = first mover" naming convention above.

### D6. A side reduced to zero pieces has already lost

*Needs: tier 1.* The only way to lose one's last piece is the opponent's
capture, which ended the game at that instant (A9). So no position with a
side to move and zero pieces of either color is ever reached in play.

### D7. Every position has a determined winner under optimal play

*Needs: tier 1.* Breakthrough is a finite (D1, D4: finitely many positions,
none repeating), two-player, zero-sum, perfect-information game with no
chance (A11) and no draws (D3). Zermelo's theorem then gives every position
a definite value, and with draws impossible that value is a forced win for
exactly one side. Which side wins the standard 8x8 start is unknown
(computationally open, believed White, see E1). Small Breakthrough boards
have been solved by search, 8x8 has not.

### D8. The winner always makes the last move

*Needs: tier 1.* Both win conditions trigger on the mover's own move:
reaching the goal row (A8) and capturing the last enemy piece (A9) are
things the mover did, and there is no other way to end (A13, Lemma A). So
it is impossible to lose on your own move. Corollary, from any
White-to-move start: a game of n plies was won by White exactly when n is
odd. A cheap integrity check for every stored game in
`ranking/matches.jsonl` and the `data/*.jsonl` streams.

### D9. A passed runner is uncapturable and unstoppable

*Needs: tier 1.* Call White's most-advanced piece P (row y) a passed runner
if no Black piece stands on any row above y. Black rows only ever decrease
and P's row only increases (A4), so no Black piece can ever again be
strictly ahead of P, and by Lemma C, P can never be captured. Moreover the
square straight ahead of P is always empty (no White piece is above P by
most-advancedness, no Black piece is above P by passedness), so P can
simply advance every White move and reach the goal row in exactly as many
further White moves as its remaining row distance. White wins then, or even
sooner, unless Black forces its own win during the intervening Black moves.
The game reduces to a pure race decided by tempo.

### D10. An empty back-rank neighborhood is a winning outpost

*Needs: tier 1.* A White piece on row 6 in column x can be captured only
from Black's back-rank squares (x-1, 7) and (x+1, 7) (Lemma C). If both are
empty or off the board, the piece is uncapturable, and on White's next move
a move onto row 7 always exists: the diagonals (x-1, 7)/(x+1, 7) are empty
(at least one on-board), and A6 allows stepping there. So the piece wins on
White's very next move unless Black wins on the single ply in between. This
is the rigorous basis for the "penalize holes in the back rank" evaluator
idea in [todo.md](../todo.md), and it is exactly the 1-step threat that
`canWinWhite/Black` (`src/board_analysis.cpp`) and `nearWinCheck` detect.

### D11. Material and game phase are irreversible

*Needs: tier 1.* Piece counts never increase (A7, A2: no promotion or
creation), so each side's count, the total, and any "phase" defined from
piece count move in one direction only over a game. A tapered or
phase-split evaluator indexed by piece count is therefore indexing a
monotone clock, it can never see the game move back into an earlier phase.

### D12. Branching is at most 3 per piece, and the standard start has exactly 22 legal first moves

*Needs: O1, O3 for the constants.* A piece has at most 3 destinations (A4),
so a side with n pieces has at most 3n legal moves, at most 48 with 16
pieces. In the standard start the back row has zero moves (all three
destinations hold friendly pieces), and the front row has 3 each except the
two edge pieces with 2, so 6*3 + 2*2 = 22.

### D13. No capture can occur before ply 5, and no game can end before ply 11

*Needs: O1, O3.* Let g = (lowest Black row) - (highest White row), which
starts at 6 - 1 = 5, and only decreases by at most 1 per ply (a side's move
advances its extreme row by at most 1, and captures can only raise g). A
capture needs attacker and victim on adjacent rows (Lemma C), so g <= 1,
which takes at least 4 plies, making ply 5 the earliest capture (a White
move), and ply 6 the earliest Black capture. For the game to end: a White
front-row piece needs 6 moves to reach row 7 (every move gains exactly one
row, A4), and White's 6th move is ply 11. A capture-all win needs 16
capturing moves by the winner, and captures start at ply 5 at the earliest,
so White's 16 captures are its moves 3 through 18 at plies 5 through 35: no
capture-all can end before ply 35. So 11 plies is the true minimum game
length, witnessed by the runner a1-a2, a2-a3, a3-a4, a4-a5, a5xb6, b6xa7
(six White moves, plies 1 through 11) while Black shuffles h-side pieces,
never capturing (captures are optional, A10).

### D14. Race arithmetic

*Needs: tier 1.* A passed runner needing d moves, with the move, wins
outright if no enemy piece is within d-1 rows of its own goal and the
runner's side has at least d pieces. Written for White: White advances the
runner every move (always possible and safe, D9) and wins on its d-th move,
ply 2d-1. Black can preempt that only by winning on or before ply 2d-2,
within d-1 Black moves. A Black breakthrough within d-1 moves needs a Black
piece within d-1 rows of row 0, excluded by hypothesis. A Black capture-all
needs one capturing move per White piece (A9, one capture per move by
A4/A6), at least d moves, also too slow. Note Black's proximity is measured
over ALL its pieces, not just passed ones, because a defender that turns to
chase the White runner cannot catch it (Lemma C, D9), so every Black piece
is effectively a free runner once White commits to the race. This is the
rigorous core of the "race-distance differential" evaluator idea in
[todo.md](../todo.md).

Developer confidence: Certain on Lemma A (their own argument), N/A on
Lemmas B-C and D1-D14 until reviewed. Claude confidence: Certain on all.

## Tier 4: Empirical truths (observed, not proven)

Strategic claims inferred from measurements, never proven from the rules.
Every entry is revisable. Confidence marks: Dev defaults to N/A until the
developer fills it in.

| # | Claim | Dev | Claude | Evidence |
|---|---|---|---|---|
| E1 | White (the first mover) has a real advantage. | N/A | High | The champion's historical record is White 96.5% vs Black 87.9% ([theories.md](theories.md) theory 15). The opener-swap study's color-effect bucket split 55% White-won-both vs 10% Black-won-both at n=20. Theory 5 (color-specific weights) exists because of this asymmetry. By O4 + D5 this can only be a tempo effect, and by D7 one side provably has a forced win from the start, so the open question is whether the observed edge reflects that side. |
| E2 | At a fixed evaluator, deeper search is stronger. | N/A | High | Every depth-laddered tournament and the ranking pool are monotone in depth. Current fit: champion d6 at Elo 1140, the d8/nb2m oracle at 1267 (`ranking/ratings.tsv`, 2026-07). |
| E3 | Material advantage is strongly predictive of winning. | N/A | High | Chip-weighted evaluators have dominated every pool. The reigning champion's weights are `classic(t1,c4,w0,l0)`: material and turn only. "A 3-chip lead is usually decisive by the midgame" ([terminology.md](terminology.md)). |
| E4 | The side to move holds measurable positive value (tempo). | N/A | High | `train.exe turn-swing` measures a consistent positive 1-ply eval swing, used to calibrate the turn weight. Magnitude is small relative to a chip (the champion carries t1 against c4). Consistent with D9's reduction of endgames to races decided by tempo. |
| E5 | Rewarding forward advancement adds little at search depths >= 4 (second-order at best). | N/A | Medium | The `--forward-study` family (Fwd0/1/2/4/8) and the training sweep found forward-weight effects inside training-seed noise ([training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md)). The champion has no forward term. Confound: search itself discovers advancement, so this may say more about d6 search than about the feature. |
| E6 | Wall/column structure terms, as implemented, add little or nothing. | N/A | Medium | The hill climber's best mixes and the reigning champion converged on `w0,l0` (zero wall, zero column). The diagonal-defense confound is now partially resolved: the Advanced evaluator's Support term (diagonal pairs, exactly Lemma C's recapture geometry) went through a 13-weight climb in both non-negative and signed modes and settled at 0 or slightly negative (-2, within fitness noise), alongside wall/column staying at 0 ([heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md)). Caveats: one climb pair at d4 with noisy fitness, not a targeted study. |
| E7 | Forced-random opening plies objectively damage a position. | N/A | High | A positionally-aware judge scored the champion worse off on ~64% of its forced-random opener plies (mean delta +54, 60/60 games hurt), and the champion drops ~217 Elo when rostered with `.opener(rand,6)@1` (1140 -> 923). Theory 6 in [theories.md](theories.md), tested in [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md). |

Claims still mid-investigation (for example theory 15, "champdil recovers
from bad positions better than the champion") stay in
[theories.md](theories.md) until settled, then graduate here.
