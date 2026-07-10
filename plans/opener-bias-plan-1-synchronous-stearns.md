# Plan: Test Theory 6 -- do symmetric random openers inflate "beats the champion" results?

## Context

`Docs/theories.md` tracks the project's testable AI-development theories. Theory 6
was Open / untested:

> **Claim:** Evaluating with symmetric random openers (`--open-plies 6` applied to
> both sides) inflates every "beats the champion" result in the vs-champion-training
> study.

Every headline result in `plans/vs-champion-training-results-1-cozy-forest.md` was
measured with `--open-plies 6` on both sides, forcing the deterministic champion to
play 6 random moves it would never choose. If those random plies leave the champion
worse off than its own opening, then the champdil model's 62.5% d6 head-to-head
(basis for Theory 2 = REFUTED) and the oracle family's 785 screen / 1137 d6 tie (the
headline) could be measuring "beats a handicapped champion," not "beats the
champion's real play." The confound is one-directional: it can only inflate a "beats
the champion" result. Source: Future Work #1 and #5 of that results doc.

The results doc proposed two tests (Test A = asymmetric-opener retrain, Test B =
direct static-eval bias tabulation). The developer chose all three layers: those two
plus a cheap head-to-head sensitivity sweep. All three share one new primitive.

## Shared primitive: asymmetric openers in `pairgen`

`playoutCapture` in `src/ranking.cpp` played the opener as both-sides-random. Add an
`int openSide` color bitmask (1 = White random, 2 = Black random, 3 = both),
mirroring `RankDilOverride::apply`. During the opener window only masked side(s)
play random; the unmasked side consults its brain. Thread through
`playoutCapture` -> `playOneGameCapture` -> `rankPairGen`, mapping the a/b choice
onto per-game colors like the dilution override. Trailing param defaulted to 3
(both) so existing positional calls and behavior stay byte-identical. Record
`open_side` in the `.meta.json` sidecar. CLI: `--open-side a|b|both` in
`tools/rank_main.cpp`. Closes the standing `todo.md` "asymmetric opener flag" item.

## Layer 1 -- head-to-head sensitivity sweep (cheap, no retrain)

`pairgen` doubles as an evaluation tool (its `.meta.json` carries the win tally +
color split). For each promoted challenger (champdil s96, oracle s98) vs the
champion, run three opener configs at d6, ~80 games each:

- S: `--open-plies 6` (both) -- reproduce the symmetric baseline
- C: `--open-plies 6 --open-side a` -- challenger random, champion true policy
- P: `--open-plies 6 --open-side b` -- champion random, challenger true policy

If the challenger's win survives in C (it is the one handicapped, the champion plays
perfectly), the win is real; if it collapses S -> C, the symmetric win was a
shared-handicap artifact. The C-P spread brackets the opener's handicap magnitude.
Deliver as `tools/opener_bias_study.ps1` (sharded, reads meta tallies, prints table).

## Layer 2 -- direct mechanism tabulation

New read-only `rank.exe opener-bias` subcommand. For each seeded game, replay the
both-random opener (RNG-faithful), and at each champion ply score the position after
its forced-random move against the position after its own move, both with the
opponent to move so the eval's turn term cancels. delta = judge-value(own) -
judge-value(random), champion-relative; positive = the random opener hurt the
champion. A `--judge` agent (default the champion) does the scoring, because the
champion's own Classic eval (w0, l0) is positionally blind and cannot distinguish
opener positions -- a learned PST judge is needed for a discerning read. Tabulate
mean delta and % of plies/games hurt, split by the champion's color.

## Layer 3 -- asymmetric-opener retrain

Regenerate the oracle training set with `--open-side a` (only the oracle plays the
random opener; the champion plays its own opening throughout), retrain the 3-seed
oracle cell (`--from-data`), gauntlet-screen at the d4 wrapper, d6-confirm the best,
and compare to the symmetric baseline (785 screen mean / 1137 d6). A meaningful drop
confirms the headline tie was partly an artifact. Deliver as
`tools/opener_bias_retrain.ps1`, resumable via a CSV, archiving models to
`models/sweep/vsc_oracle-asym_<seed>.txt` (never overwriting the symmetric baseline
or touching the roster).

## Tests, docs, verification

- `tests/test_ranking.cpp`: asymmetric open-side divergence, default byte-identical
  back-compat, meta field, determinism, and an `opener-bias` smoke.
- Update `Docs/theories.md` (theory 6 verdict + cross-link theory 2), write the
  companion results doc, update `CLAUDE.md` / `ML.md` / `README.md` / `todo.md`.
- Verify: build `rank.exe` + tests pass, flag smoke test, run all three layers.
