# Heuristic evaluator overhaul: new "Advanced" evaluator + benchmark + hill climb

## Context

`todo.md`'s "Heuristic Evaluator Feature Ideas" section lists ~10 candidate terms for
the hand-crafted evaluators (noise tie-breaker `[Now]`, center advancement, back-rank
holes, defended pieces `[Next]`, mobility, phalanx, breakthrough-square control,
column-emptiness, race differential, overextension). The developer asked to overhaul
the heuristic board-state evaluator, implement a bunch of these, add a few new terms
informed by `Docs/axioms.md` and `Docs/theories.md`, benchmark it, and run the hill
climber. Related open items folded in: the zero-weight incremental gating fix
(todo.md, from the chip-count study), and the `[Now]` "test signed (negative)
wall/column weights with the hill climber" item.

Current state: `src/ai_eval.cpp` has Classic (turn, chip, wall, column) and
Experimental (+ forward), both incremental via the `g_evalPos` accumulator
(`evalPosFull` full scan, `evalPosLocal` neighbor-local delta called from
`simulateMove`/`unsimulateMove`). The champion is
`ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` at Elo 1140.

## Key design decisions

1. **New evaluator, not an extension of Experimental.** The ranking ID codec emits
   every weight letter, so adding params to Experimental would re-identify every
   existing `classic`/`exp` agent in `ranking/matches.jsonl` and the roster. A new
   registry entry (name **"Advanced"**, ID name **`adv`**) preserves all existing
   identities. Classic and Experimental stay frozen.
2. **Merge "defended pieces" and "diagonal phalanx" into one Support term.** Both
   todo ideas describe the same geometry: a diagonal same-color adjacency (the piece
   diagonally behind is exactly the recapturer, axioms.md Lemma C). One pair-count
   term, consistent with the wall/column pair style. Note the merge in the results
   doc, with the saturating per-piece variant as Future Work.
3. **Capacity (theory 18): implement the function, skip the redundant weight.**
   White capacity = sum over white of (7-y), Black capacity = sum of y. The
   white-centric capacity advantage (blackCap - whiteCap) equals
   `forwardSum - 7*chipDiff` exactly, so a dedicated capacity weight would be
   linearly dependent on two existing terms: any capacity weight k is identical to
   forward += k, chip -= 7k, and a duplicate dimension only wastes hill-climber
   iterations. Instead the plan ships the capacity CONCEPT three ways:
   - `capacityWhite()` / `capacityBlack()` helper functions in
     `board_analysis.cpp` (Lemma B's sum), also needed later by the
     distance-to-win tool and theory 18's outcome-correlation check.
   - A unit test asserting the identity `blackCap - whiteCap == forwardSum -
     7*chipDiff` on assorted boards, so the redundancy claim is code-verified.
   - The signed-weights climb below: the pure-capacity direction requires a
     NEGATIVE chip weight (forward : chip = 1 : -7), so it is unreachable under
     non-negative weights. Allowing negative weights is what puts capacity-driven
     play inside the climber's search space, and the two climb modes compare it
     directly. Record all of this in `Docs/theories.md` theory 18.
4. **Add two rules-derived terms of my own** beyond the todo list:
   - **Race-win detector (D14/D9):** an exact, proven decided-race check. From
     per-row piece counts: the most advanced White piece is a passed runner iff
     maxRow(Black) <= maxRow(White) (D9). With d = 7 - maxRow(White), White to move
     wins outright if no Black piece is within d-1 rows of row 0 and Nw >= d (D14).
     Returns the same WhiteWin/BlackWin sentinels as `nearWinCheck`, so it is a
     provably sound leaf shortcut that decides races many plies before search could.
     Side-to-move aware (the non-mover variant needs one extra tempo of margin).
     Gated by a 0/1 param so it is ablatable.
   - **Seeded Zobrist-style noise (the `[Now]` tie-breaker idea):** per-(color,
     square) hashed values in [-n, +n] derived from a seed param. This is a random
     PST, so it is incremental (2-3 term updates per move), deterministic per
     seed+position (agents stay classifiable as deterministic), and dominated by
     real terms so tactics still win. Two params: noise magnitude + seed.

## The Advanced evaluator (16 params, codec letters `tcwlfdemhborxnsg`)

`MAX_EVAL_PARAMS` bumps 8 -> 20 (headroom). All contributions white-centric
(positive favors White), each mirrored for Black per axioms D5.

| # | Letter | Name | Definition (White side shown) | Default | Incremental strategy |
|---|---|---|---|---|---|
| 0 | t | Turn | existing | 1 | leaf term |
| 1 | c | Chip | existing (`g_chipDiff`) | 4 | existing counter |
| 2 | w | Wall | existing orthogonal same-row pair | 0 | existing `g_evalPos` |
| 3 | l | Column | existing same-file pair | 0 | existing `g_evalPos` |
| 4 | f | Forward | existing per-square y | 0 | existing `g_evalPos` |
| 5 | d | Support | diagonal same-color adjacent pair count (merged defended+phalanx) | 0 | `g_evalPos`, diagonal-pair delta with a single-ownership convention mirroring `structOwner` (theory 13 lesson) |
| 6 | e | Center | per-square `y * min(x, 7-x)` (advanced center pieces have two escape diagonals) | 0 | `g_evalPos`, per-square delta |
| 7 | m | Mobility | legal-move count per piece (forward-to-empty + diagonals to empty/enemy) | 0 | `g_evalPos`, before/after sum over the 3x3 blocks around the two changed squares (the only cells whose mobility can change) |
| 8 | h | Hole | count of own back-rank columns x where every on-board guard square (x-1,0),(x+1,0) is empty (D10 outpost-admitting holes), applied as a penalty | 0 | `g_evalPos`, recompute affected hole columns when a changed square is on rows 0-1 (White) / 6-7 (Black) |
| 9 | b | Control | own pieces on the two rows nearest the opponent's home row (rows 5-6 for White) | 0 | `g_evalPos`, per-square delta |
| 10 | o | Open | penalty per file with zero own pieces on the own half (rows 0-3 for White) and at least one enemy piece anywhere in the file | 0 | `g_evalPos`, recompute the 1-2 affected files per move |
| 11 | r | Race | `r * (dB - dW)` where dW = 7 - maxRow(White), dB = minRow(Black) (soft D14 proxy) | 0 | per-row piece-count counters, computed at the leaf |
| 12 | x | Overext | penalty per own piece past the midline (y >= 4 for White) with zero friendly diagonal-behind defenders | 0 | `g_evalPos`, affected cells = changed squares + their diagonal-ahead dependents |
| 13 | n | Noise | seeded per-(color,square) hash noise in [-n, +n] | 0 | `g_evalPos`, per-square delta |
| 14 | s | NoiseSeed | the noise seed (not a strength weight) | 0 | n/a |
| 15 | g | RaceWin | 0/1: exact D14 decided-race sentinel detector | 1 | per-row counters, checked at the leaf like `nearWinCheck` |

Feature-gating principle (extends the chip-count study lesson, theory 16): every term
group computes its delta only when its weight is nonzero, and `evalBeginSearch` sets
`g_evalIncremental` only when at least one `g_evalPos` term is active. This also
delivers the standing todo fix for Classic/Experimental at w0,l0 (verified by the
speed ladder's v2->v3 row going to ~0%). The row counters get their own gate flag,
maintained in make/unmake only when r or g is active.

## Implementation steps

### 1. Engine (`src/`)

- `globals.h`: `MAX_EVAL_PARAMS` 8 -> 20. New globals: `g_rowCountW[SIZE]`,
  `g_rowCountB[SIZE]`, `g_evalRowCounts` (bool gate), noise seed/table state.
  Define in `globals.cpp`.
- `src/ai_eval.cpp`:
  - Shared per-term helper functions (one per feature) used by BOTH the full scan
    and the local delta, so the two paths cannot diverge (theory 13 lesson).
  - Generalize `evalPosFull` and `evalPosLocal` to compute all enabled `g_evalPos`
    terms (read weights from `g_activeParams` beyond p[4], gated per term).
  - New `evalAdvanced` fn + registry entry `{ "Advanced", 16, {...}, evalAdvanced,
    true }`. Full-scan path computes race/racewin by scanning for row extremes.
  - `evalBeginSearch`: seed row counters when r/g active, zero-weight gating for
    `g_evalIncremental` (all evaluators). `evalLeaf`: for evaluators with leaf
    extras (Advanced), add the race term and run the RaceWin sentinel check from
    the row counters. Keep the `g_evalLevel` ladder branches working for Advanced.
  - Noise: small deterministic hash `hash(seed, color, x, y) -> [-n, +n]`.
- `src/board_analysis.cpp` / `.h`: add `capacityWhite()` / `capacityBlack()`
  (Lemma B's remaining-advancement sums), the tested-but-not-weighted capacity
  helpers from decision 3.
- `src/moves.cpp`: in the 4 simulate/unsimulate fns, maintain `g_rowCountW/B` under
  the new gate (source row -1, dest row +1, victim row -1 on capture, mirrored on
  unmake).
- `src/ranking.cpp`: add `{ "Advanced", "adv", "tcwlfdemhborxnsg", 1 }` to
  `g_rkEvals` (codec-completeness test enforces coverage). Negative weights already
  parse (`lenientInt` allows them).

### 2. Tests (`tests/test_eval.cpp`, `tests/test_ranking.cpp`)

- Advanced with all new weights 0 scores identically to Experimental (and Classic
  when f=0) on crafted boards.
- Per-feature unit positions: support pairs (incl. edges), mobility counts, hole
  counts (edge columns where one guard square is off-board), control, open files,
  race differential, overextension, center, noise (deterministic per seed, inert at
  n=0, differs across seeds).
- RaceWin detector: a D14 witness position fires the sentinel, and near-miss
  variants (defender in range, too few pieces, runner not passed, wrong side to
  move) must NOT fire.
- Incremental walk (extend the existing `walkAssert` battery): Advanced with ALL
  weights nonzero over the crafted + board1 positions, asserting `g_evalPos` ==
  `evalPosFull`, row counters == a fresh recount, and `evalLeaf` == `evaluateBoard`
  at every node across make/unmake.
- Codec round trip for an `adv(...)` ID including a negative weight.
- Capacity identity: `blackCap - whiteCap == forwardSum - 7*chipDiff` (using the
  new `capacityWhite/Black` helpers) on assorted crafted boards and board1.

### 3. Benchmark (`src/ml_train.cpp` speedBench)

- Extend the hardcoded `p[5]` preset arrays to `MAX_EVAL_PARAMS`.
- ms-budget table: add Advanced rows at (a) champion-equivalent weights (t1,c4,g0),
  (b) champion + RaceWin (g1), (c) all features on.
- Eval-level ladder: add Advanced presets (all-on, and one-feature-at-a-time rows
  for the expensive terms: mobility, hole, open) so v2 (full scan per leaf) vs v3
  (incremental) prices each feature, with the equivalence self-check PASS required.
- Verify the zero-weight gating fix: the classic w0,l0 ladder row's v2->v3 delta
  should move from +18..+35% to ~0%.
- Record: exact command line with seed/reps, mean/median/min us/move, nodes/move.

### 4. Hill climber (`tools/hill_climb.ps1`)

- Retarget to Advanced: candidate ID `ab(dK)@1.adv(...)@1`, climb the 13 strength
  weights (c,w,l,f,d,e,m,h,b,o,r,x,n). Pin turn=20, s=1, g=1 (g is proven-sound,
  not a mix weight).
- **Signed weights behind a switch** (the `[Now]` todo item): new `-AllowNegative`
  flag, default off (current non-negative behavior preserved). When on: normalize
  by sum of ABSOLUTE values to 80 (largest-remainder rounding preserving signs),
  small steps may cross zero, drastic resets randomize the chip weight and flip
  random signs on other terms. This is also the capacity experiment (decision 3):
  only signed mode can reach the forward-positive / chip-negative capacity
  direction.
- Start = champion-equivalent `c80` (all else 0), so iteration 0 reproduces the
  champion mix as the baseline to beat.
- Extend the TSV log columns to all weights.

### 5. Runs (after tests pass)

1. `.\tools\run_tests.ps1 -Build` (must pass).
2. Rebuild `train.exe`, `rank.exe` (and `breakthrough.exe`, all link ai_eval).
3. `train.exe speed` with fixed seed/reps -> record the benchmark tables.
4. Sanity gauntlet: `adv` at champion-equivalent weights + g0 at the champion head
   should land within noise of the champion's Elo (same play by construction).
5. Ablation gauntlets at the same head: g0 vs g1 (does the D14 detector add Elo?),
   and n0 vs n1/n2 at a fixed seed (does noise cost Elo?).
6. Hill climb, TWO runs with identical budget, seed, and start (d4, ~9-opponent
   `climb_roster` pool, estimated 1-3 h each):
   - Run A (non-negative): `.\tools\hill_climb.ps1 -Iters 60 -Games 4`
   - Run B (signed): `.\tools\hill_climb.ps1 -Iters 60 -Games 4 -AllowNegative`
   Compare the two best mixes and Elos (does allowing negative weights find a
   different or stronger optimum, and does any weight actually settle negative,
   e.g. the capacity direction's negative chip or the E6-motivated negative
   wall/column?). Then `-Promote -PromoteTop 2` on the winners across both runs to
   rate them on the shared scale, and a d6-head gauntlet of the best mix for the
   standing dethrone check vs the champion (Elo 1140).

### 6. Docs + workflow (per CLAUDE.md standing instructions)

- README.md AI/evaluator section, `src/CLAUDE.md` (ai_eval + moves entries, global
  state table), `tools/CLAUDE.md` (hill_climb entry), regenerate ML.md autodoc
  (`train.exe docs`).
- `todo.md`: strike through the shipped feature ideas, the zero-weight gating item,
  and the signed-weights item, each with a short result note.
- `Docs/theories.md`: update theory 18 (analytic linear-dependence note), add
  entries for the D14-detector and noise-tiebreak claims with their ablation
  results, update E6's confound note in axioms.md if the diagonal Support result
  settles it.
- Archive this plan as `plans/heuristic-eval-overhaul-plan-1-buzzing-floyd.md` plus
  the companion `...-results-1-buzzing-floyd.md` with all measured numbers.
- Commit at natural checkpoints (tests green first, never push).

## Verification

- Full test suite green, including the new equivalence walks (the strongest guard:
  incremental == full recompute at every node).
- speedBench equivalence self-check PASS, benchmark numbers recorded.
- Sanity gauntlet: champion-equivalent Advanced ties the champion's rating.
- GUI smoke test (`.\tools\smoke_test_gui.ps1 -Build`): 16 stepper rows in the
  config panel may overflow vertically. If so, compact row spacing or two-column
  the param list for paramCount > 8 (small, contained GUI fix).
- Console: pick Advanced in `getSettings()`, confirm all 16 prompts appear and
  `minimax_params.txt` round-trips the new keys.

## Risks / notes

- Mobility/hole/open deltas have wider affected-cell sets (up to ~18 cells). If the
  incremental path loses to the full scan at some weight mixes, the per-feature
  gating keeps unused terms free, and the benchmark will show where the crossover
  is. Correctness is guarded by the walk test regardless.
- 13-dimensional climb with 60 iterations is a coarse search. Acceptable for a
  first pass, note in results, response-surface mapping stays a `[Next]` todo.
- The RaceWin sentinel interacts with TT/win-decay exactly like `nearWinCheck`
  sentinels (ttStore already skips them), but tests must cover the near-miss cases
  since a wrong WIN sentinel would be a strength bug, not a crash.
