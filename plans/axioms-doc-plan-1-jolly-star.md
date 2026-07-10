# Plan: `Docs/axioms.md` -- Breakthrough truths document

## Context

`todo.md` (Agent Track, lines 86-103) asks for a new doc, `Docs/axioms.md`, that
records what is *known* about Breakthrough, separated by how it is known: things
taken straight from the rules, project-specific choices that a variant could swap,
things logically proven from those, and softer strategic claims that are only
observed. It sits alongside the existing `Docs/theories.md` (open/testable
theories) and `Docs/terminology.md` (glossary). The value is a stable reference a
future variant or a new session can consult to know exactly which "facts" are
inherent to Breakthrough, which are this project's parameter choices, which are
proven, and which are merely empirical and therefore revisable.

Developer decisions from this session (baked into the structure below):
- **No-draws proof:** use the rigorous version -- the most-advanced piece of the
  side to move always has a legal diagonal move, so no stalemate, so no draw. The
  engine's `explorers.cpp:16` "no move = loss" is a belt-and-suspenders guard for a
  case that cannot arise on a >= 2 column board.
- **Confidence scale:** `Certain / High / Medium / Low`. Two columns, one for the
  developer (default `N/A` until filled) and one for Claude.
- **Tier assignment:** capture-all-pieces = required (tier 1). Full starting rows,
  square board, 8x8 size, 2 starting rows = optional (tier 2). "White moves first"
  = a naming convention, not an axiom (its own short section). No-stacking = tier 1
  but flagged near-universal.
- **Organizing principle for hard vs soft:** an axiom's "hardness" is just how much
  of the game/its proofs you would have to redo if you changed it. Tier-2 entries
  each carry a short "what breaks if swapped" note; conventions are the degenerate
  case (nothing breaks).

## Deliverable

One new file, `Docs/axioms.md`, matching the voice and layout of
`Docs/theories.md` / `Docs/terminology.md` (factual, no semicolons or em dashes,
`->`/`>=` not Unicode, short intro + status/confidence legend + tables/sections).

### Structure

1. **Header + intro**: what the doc is, how it relates to `theories.md`
   (theories = not-yet-settled; axioms = settled or definitional) and
   `terminology.md` (definitions of the words used here).

2. **Confidence legend**: `Certain` (rules + proofs), `High`, `Medium`, `Low`
   (empirical only). Note the two-column dev/Claude split and the `N/A` default.

3. **A note on conventions (before the tiers)**: "White moves first" and the
   coordinate orientation (White on rows 6-7 moving toward row 7, Black on 0-1
   moving toward row 0). Explain these are symmetry-fixing definitions, not
   axioms: the color-swap relabeling is an automorphism of the game, so we *define*
   White as the first mover rather than *assert* it (WLOG). Changing a convention
   changes nothing substantive -- the softest end of the developer's
   "how-much-breaks-if-changed" scale.

4. **Tier 1 -- Direct axioms** (from the rules; `Certain`). Sourced from the
   code: forward step onto an empty square only, no forward capture
   (`moves.cpp` `tryMoveQuick*`, lines ~256-266); diagonal step onto empty or
   diagonal capture; at most one piece per square (no stacking, flagged
   near-universal); a captured piece is removed; players alternate single plies and
   must move (no pass); win by reaching the opponent's back row; win by capturing
   all opponent pieces (`board_analysis`/game-loop win checks). Each as a row with
   dev + Claude confidence (`Certain`) and a source (rule / file).

5. **Tier 2 -- Optional axioms** (this project's choices; `Certain` that the
   project made them, but swappable). Board is 8x8; two fully-filled starting rows
   per side (16 pieces each); board is square. Each carries a
   **"what breaks if swapped"** note (e.g. changing board width does not touch the
   no-draw proof but re-scales every learned PST and evaluator weight; a 1-column
   board would break the no-draw proof; non-full starting rows change opening
   theory but not the rules engine).

6. **Tier 3 -- Derived truths** (proven from tiers 1-2; `Certain`). At least:
   - **No draws are possible.** Full proof: (a) the side to move always has a legal
     move -- take its most-advanced piece on the highest row `r` it occupies; the
     two forward-diagonal squares lie on row `r+1`, which contains none of that
     side's pieces by choice of `r`, so each such square is off-board, empty, or
     enemy, and on a >= 2 column board at least one is on-board -> a legal diagonal
     move or capture always exists; (b) every terminal condition (back row reached,
     opponent at zero pieces, and the vacuous "no legal move") names a winner; (c)
     play terminates because a strict progress measure increases every ply (net
     forward advancement, or a capture strictly reducing piece count), so infinite
     play is impossible. No terminal state and no infinite game leaves no room for a
     draw. Note this subsumes the engine's `explorers.cpp:16` guard.
   - **The game always terminates** (the (c) sub-lemma, stated on its own).
   - **A side reduced to zero pieces has already lost** (trivial from tier 1).
   - Candidate extras if clean: an upper bound on game length from the progress
     measure; the color-swap symmetry of the rules (justifies the WLOG in the
     conventions section).

7. **Tier 4 -- Empirical truths** (observed, not proven; graded confidence, each
   with an evidence/session pointer so it can be revisited). Starter set, sourced
   from existing artifacts:
   - White has an advantage over Black (High) -- champion historical record White
     96.5% vs Black 87.9%, `Docs/theories.md` theories 5 and 15.
   - Deeper search beats shallower at equal eval (High) -- the depth-laddered Elo
     table is monotone in depth; `ranking/ratings.tsv`, tournament reports.
   - A material (chip) lead is strongly good (High) -- Classic weights chip heavily
     and dominates; "a 3-chip lead is usually decisive," `terminology.md`.
   - Tempo / side-to-move has measurable value (Medium-High) -- calibrated by
     `train.exe turn-swing`.
   - Forward advancement helps but is second-order (Medium) -- Experimental's
     forward weight; the training sweep found it inside seed noise, `--forward-study`.
   - Structure (walls/columns/diagonal defense) matters only relative to the rest of
     the mix (Medium) -- hill-climber renormalization finding, `tools/hill_climb.ps1`.
   - A forced-random opener objectively worsens a position (~64% of opener plies)
     (High) -- `Docs/theories.md` theory 6, `opener-bias-results-1`.
   Developer-confidence column defaults to `N/A` on every row.

## Files

- **New:** `Docs/axioms.md` (the only substantive change).
- **Edit `todo.md`:** strike through the "Make a list of truths..." item
  (lines 86-103) per the strikethrough-on-completion convention.
- **Edit `CLAUDE.md`:** add an `Docs/axioms.md` row to the Root file table next to
  the existing `Docs/theories.md` / `Docs/terminology.md` rows.
- **Edit `Docs/theories.md`:** one line in the intro cross-linking axioms.md
  (settled/definitional facts live there; theories.md is for the unsettled).
- Optionally add an axioms.md mention to `README.md` if a docs list exists there
  (skip if it would be the only such reference).

## Verification

Docs-only change: the test suite is a no-op (`CLAUDE.md` exception), so no build.
- Proofread `Docs/axioms.md` renders correctly (tables, headings) and follows the
  house style (no semicolons/em dashes, `->`/`>=`).
- Re-check every code/record citation against the source: `moves.cpp` move rules,
  `explorers.cpp:16`, the theory numbers in `theories.md`, the 96.5/87.9 White/Black
  figures.
- Confirm each tier-4 row has an evidence pointer and both confidence columns, and
  every developer-confidence cell is `N/A`.
- Confirm the cross-links between axioms.md, theories.md, and terminology.md
  resolve.

Then commit (docs-only, allowed without waiting): message covering the new doc plus
the `todo.md`/`CLAUDE.md`/`theories.md` edits. Per the after-every-functional-change
workflow this is a doc addition, not code, so steps for README/results-doc/theory-log
are minimal (theories.md gets only the cross-link, no new theory).
