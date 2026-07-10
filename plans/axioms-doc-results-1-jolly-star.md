# Results: Docs/axioms.md, the Breakthrough truths document

Companion to [axioms-doc-plan-1-jolly-star.md](axioms-doc-plan-1-jolly-star.md).
Committed as `5ec8383` (docs-only, test suite a no-op by the CLAUDE.md
exception).

## What shipped

One new document, `Docs/axioms.md`, plus housekeeping:

- **`Docs/axioms.md`**: four tiers of truths about Breakthrough.
  - Conventions section (before the tiers): "White = the side that moves
    first" and the coordinate orientation recorded as symmetry-fixing
    definitions (WLOG), not axioms, per the developer's question about how
    math treats such choices.
  - Tier 1: 13 direct axioms, each cited to the enforcing code
    (`src/moves.cpp` move rules, `src/main.cpp` alternation, capture-all
    detection at `moves.cpp:216,248`, and so on). Includes the less obvious
    ones: captures are optional, no promotion, perfect information, and
    "board + side to move is the entire state" (the axiom that makes
    `positionKey` and the transposition table sound).
  - Tier 2: 4 optional axioms (8x8, square, two full home rows, symmetric
    start), each with a "what breaks if swapped" note. The organizing
    principle, per the developer: an axiom's hardness is just how much you
    would have to redo if you changed it.
  - Tier 3: 3 lemmas (no stalemate via the developer's most-advanced-piece
    diagonal argument, strict progress via remaining advancement capacity,
    capture geometry) and 14 derived truths: termination, the 208-ply
    bound, no draws, no repeated positions (game graph is a DAG), color-swap
    automorphism, Zermelo determinacy, winner-always-moves-last plus the
    game-length parity corollary (odd plies = White won, a free integrity
    check for `matches.jsonl`), passed-runner unstoppability, exact race
    arithmetic, back-rank outposts (the rigorous basis for the
    back-rank-hole evaluator idea), irreversible material/phase (the basis
    for tapered PSTs), branching <= 3n with exactly 22 legal opening moves,
    earliest capture ply 5, earliest Black capture ply 6, minimum game
    length 11 plies (witnessed), and no capture-all before ply 35.
  - Tier 4: 7 empirical truths (White advantage, depth monotonicity,
    material dominance, tempo value, forward second-order, structure terms
    near-zero, random openers damage positions), each with dev + Claude
    confidence columns (dev defaults to N/A) and evidence pointers.
- **`Docs/theories.md`**: intro cross-link establishing the division of
  labor (theories.md = claims in motion, axioms.md = settled facts, with
  graduation in both directions).
- **`CLAUDE.md`**: `Docs/axioms.md` row in the Root file table, and a
  factual fix (see gotcha below).
- **`todo.md`**: the truths-document item struck through with a summary.

## How to test

Read `Docs/axioms.md` and check proofs against the rules. Concrete spot
checks: count legal moves in the standard start (should be 22), check any
stored game in `ranking/games.tsv` for the parity rule (odd plies exactly
when White won), and confirm `src/moves.cpp:256-269` matches A4-A6.

## Deviations from the plan

- The plan sketched 3 derived truths plus "candidate extras." Mid-session
  the developer asked for real iteration ("don't just do a first pass"),
  and the final doc has 3 lemmas + 14 derived truths. The additions beyond
  the plan: capture geometry (Lemma C), Zermelo determinacy,
  winner-moves-last + parity, passed runners, race arithmetic, back-rank
  outposts, irreversible phase, the 22-move and 48-move branching facts,
  and the ply-5/6/11/35 timing bounds.
- Tier 1 grew from the planned 7 rule axioms to 13 by mining the rules for
  the implicit ones (optional captures, no promotion, determinism, perfect
  information, statelessness, no other rules).
- Tier 2 gained O4 (the symmetric start), which the plan did not list. It
  earns its place because it is exactly what makes E1 readable as a pure
  first-mover effect.
- The no-draws proof was restructured per the developer's answer: the
  most-advanced-piece diagonal argument is rigorous on any board at least
  2 columns wide, and the engine's `explorers.cpp:16` no-move guard is
  defensive only.

## Gotchas discovered

- **CLAUDE.md had the coordinate convention swapped.** It said White starts
  on rows 6-7 and Black on rows 0-1. The code is the opposite:
  `board_io.cpp` reads a board file's first line into row 7, board files
  put Black on top, White moves `+y` (`moves.cpp:258`), and `g_whiteAtEnd`
  counts row `SIZE-1`. Fixed in the same commit. Lesson: the axioms doc's
  conventions section now pins this to the code so it cannot drift again.
- **Lemma A needed one more brick than the obvious argument.** "The
  forward-diagonal squares contain no friendly piece" is not enough, row
  r+1 must also exist. That follows from A8: a piece standing on its goal
  row would have already ended the game, so during play no piece occupies
  its goal row. First drafts of this kind of proof tend to miss it.
- **Insertion-only patches with `git apply --unidiff-zero` position by the
  new-side line number.** The session's working tree carried unrelated
  uncommitted work from earlier sessions in the same files, so the commit
  was staged with handcrafted per-hunk patches. A pure-insertion hunk
  copied out of a multi-hunk diff kept a new-side number offset by earlier
  hunks and landed one line off. Recomputing the new-side number for the
  standalone patch fixed it. Worth remembering for any future selective
  staging.

## Future Work

- **Developer review of Lemmas B-C and D1-D14.** The doc marks developer
  confidence N/A on every proof except Lemma A (their own argument). The
  claims are only as settled as that review, especially D13's witness line
  and D14's piece-count clause guarding the capture-all case.
- **Fill the tier 4 developer-confidence column.** The two-column design
  exists to record developer/Claude disagreement, and it is empty on the
  developer side until filled.
- **Tighten D2's 208-ply bound.** The remaining-capacity argument is not
  tight (the game ends at the first goal-row arrival, long before capacity
  exhausts). The actual maximum game length at 8x8 is an open, probably
  computable, question. Any improvement would also sharpen sanity bounds
  for stored games.
- **Check the parity rule (D8) against the stored match history.** One
  pass over `ranking/games.tsv` asserting odd-plies = White-won would
  either validate 46k+ games of stored data or catch a recording bug.
  Cheap and worth doing once.
- **E6's confound is now precisely stated.** Lemma C says the recapturing
  defender is diagonal, not orthogonal, so the tested wall/column terms
  measure something structurally different from defense. The
  defended-pieces (diagonal) evaluator term in todo.md is the direct test.

## Ideas This Inspired

- A `tests/` assertion for D12: generate moves in the standard start and
  assert exactly 22. A trivial regression test that would catch any future
  move-generation bug instantly.
- D8's parity rule as a permanent invariant check inside `rank.exe` play
  loops (assert at game end), not just an offline audit.
- D9/D14 (passed runners, race arithmetic) could become an exact endgame
  shortcut in search: when the position satisfies D14's hypotheses, return
  a forced-win score without searching. Same flavor as `nearWinCheck` but
  triggering many plies earlier.
- D4's no-repetition proof means a search transposition entry can only be
  revisited via a different move order at the SAME remaining-capacity
  level. Capacity could serve as a cheap TT aging/partition key.
- The "what breaks if swapped" column reads like a dependency map for a
  future variant mode (board size as a runtime parameter). O1/O2's note
  that only the models and constants break suggests variant support is
  mostly a `SIZE` refactor away.
