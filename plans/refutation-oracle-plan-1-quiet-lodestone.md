# Refutation oracle: a book that beats every deterministic agent in the roster

**Goal.** Mine a single position-keyed book that, worn by any brain, wins every
game against every deterministic agent on the roster, both colours.

**Developer decisions, 2026-08-27.** Scope: all 119 deterministic agents,
including the 14 on the `time=150ms` head, rather than pre-excluding the wall-
clock budget. Miner: a purpose-built `rank.exe refute`, not a reuse of `pairgen
--branch-tries`. Sequencing: run Phase 0 before building anything else.

## Why this is tractable, and what makes it different from theory 14

Theory 14 (`Docs/theories.md`) proposed an offline refutation book and was
refuted in its naive form. Two premises failed:

1. The "deterministic" target was not reproducibly deterministic across runs.
   Cross-game transposition-table state made the target deviate from the mined
   lines within a few plies, and 3 of 32 source games did not reproduce their
   own stored result at mining time.
2. Mined moves are not brain-portable. The oracle's moves win because the
   oracle's deep search stands behind them at every later ply, and a weaker
   live brain cannot supply that once it falls out of book.

Theory 33 repaired premise 2 by mining the book-wearer's OWN wins. This project
repairs it a different way: the book carries the full continuation to the win,
so the live brain never plays at all and there is no handoff to mismatch.
Theory 14's own notes name this as one of the two open repaired variants.

Premise 1 is the Phase 0 gate. Since 2026-08-03, `playOneGame`
(`src/ranking.cpp`) calls `ttClear()` per game, and killer/history tables reset
once per top-level search, so the code-level cause theory 14 identified has been
addressed. Whether that is sufficient in practice has not been measured, and the
match store cannot answer it: no strictly deterministic ordered pair has been
played more than once since the fix.

## The structural insight

Both sides deterministic means each (book, opponent, colour) pair produces
exactly ONE game. Beating 119 agents is therefore winning 238 specific games,
not 238 distributions.

Two consequences:

- **It is a one-player search, not a minimax.** With the opponent a fixed
  function of the game path, finding a win is depth-first search over our own
  moves with backtracking, using the d8/nb2m oracle only for move ordering.
- **It is one book, not 119.** A position-keyed dictionary is a single partial
  deterministic strategy. Its trajectory against each opponent is one line, and
  the lines share their prefixes, so the tree is grown once across all opponents
  and split only where their replies diverge.

## Phases

**Phase 0, reproducibility gate.** Measure whether a deterministic agent
replayed against a fixed deterministic probe produces the same move sequence
every time. Built as `rank.exe determinism`: replicas interleaved agent-minor so
each replica is preceded by a different game sequence, per-ply position-hash
traces compared exactly, run twice in separate processes to cover cross-process
reproducibility. `--include-stochastic` is the positive control.

**Phase 1, shared-prefix frontier.** Grow one strategy tree across all
opponents at once rather than mining each line independently.

**Phase 2, one-player DFS.** Oracle-ordered candidate moves, opponent reply,
recurse. On a loss, backtrack to the deepest of our own moves with an untried
alternative.

**Phase 3, merge and conflict detection.** `openerBook` keys on
`positionKey(side).hash` with no ply and no path, so two lines needing different
moves from the same position cannot both be expressed. Detect at merge and
re-search one branch rather than silently dropping an entry.

**Phase 4, verification.** Replay the merged book against all 238 and require
238-0. Exhaustively checkable, so none of `Docs/benchmarking.md` defect 3's
distinct-game accounting applies.

## What the result would and would not establish

Memorization, not strength. The agent should be declared reference-class in
`ranking/CHAMPION.md` and not entered as a champion: a Bradley-Terry fit assigns
one strength parameter per agent, and this one is maximally intransitive, going
238-0 against deterministic opponents while rating ordinarily against the 52
dilution and 66 random-opener agents that never reproduce a line. Its pooled Elo
would be a fit artifact, in the same way `rank.exe matchup` was built to expose.

Its value is as a probe of the instrument: how much of this ladder is
memorizable, and the sharpest available test of theory 38 (exact-hash book
collapse) and theory 33 (self-mined book).

## Known risk that compute cannot remove

A black-side win against the strongest openless agents may not exist. 8x8
Breakthrough is unsolved and `boards/board1.txt` may simply be won for White
against a given opponent. The honest target is "beats every deterministic agent
that can be beaten," with the unwinnable set reported explicitly rather than
quietly dropped.
